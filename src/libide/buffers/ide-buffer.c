/* ide-buffer.c
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "ide-buffer"

#include <dazzle.h>
#include <glib/gi18n.h>

#include "ide-context.h"
#include "ide-debug.h"

#include "buffers/ide-buffer-addin.h"
#include "buffers/ide-buffer-change-monitor.h"
#include "buffers/ide-buffer-manager.h"
#include "buffers/ide-buffer-private.h"
#include "buffers/ide-unsaved-files.h"
#include "diagnostics/ide-diagnostic.h"
#include "diagnostics/ide-diagnostics-manager.h"
#include "diagnostics/ide-diagnostics.h"
#include "diagnostics/ide-source-location.h"
#include "diagnostics/ide-source-range.h"
#include "files/ide-file-settings.h"
#include "files/ide-file.h"
#include "formatting/ide-formatter-options.h"
#include "formatting/ide-formatter.h"
#include "highlighting/ide-highlight-engine.h"
#include "highlighting/ide-highlighter.h"
#include "plugins/ide-extension-adapter.h"
#include "rename/ide-rename-provider.h"
#include "sourceview/ide-source-iter.h"
#include "sourceview/ide-source-style-scheme.h"
#include "symbols/ide-symbol-resolver.h"
#include "symbols/ide-symbol.h"
#include "util/ide-battery-monitor.h"
#include "util/ide-gtk.h"
#include "vcs/ide-vcs.h"

#define DEFAULT_DIAGNOSE_TIMEOUT_MSEC          333
#define SETTLING_DELAY_MSEC                    333
#define DEFAULT_DIAGNOSE_CONSERVE_TIMEOUT_MSEC 5000
#define RECLAIMATION_TIMEOUT_SECS              1
#define MODIFICATION_TIMEOUT_SECS              1

#define TAG_ERROR            "diagnostician::error"
#define TAG_WARNING          "diagnostician::warning"
#define TAG_DEPRECATED       "diagnostician::deprecated"
#define TAG_NOTE             "diagnostician::note"
#define TAG_SNIPPET_TAB_STOP "snippet::tab-stop"
#define TAG_DEFINITION       "action::hover-definition"
#define TAG_CURRENT_BKPT     "debugger::current-breakpoint"

#define DEPRECATED_COLOR "#babdb6"
#define ERROR_COLOR      "#ff0000"
#define NOTE_COLOR       "#708090"
#define WARNING_COLOR    "#fcaf3e"
#define CURRENT_BKPT_FG  "#fffffe"
#define CURRENT_BKPT_BG  "#fcaf3e"

typedef struct
{
  IdeContext             *context;
  IdeDiagnostics         *diagnostics;
  GHashTable             *diagnostics_line_cache;
  DzlSignalGroup         *diagnostics_manager_signals;
  IdeFile                *file;
  GBytes                 *content;
  IdeBufferChangeMonitor *change_monitor;
  IdeHighlightEngine     *highlight_engine;
  IdeExtensionAdapter    *formatter_adapter;
  IdeExtensionAdapter    *rename_provider_adapter;
  IdeExtensionSetAdapter *symbol_resolvers_adapter;
  PeasExtensionSet       *addins;
  gchar                  *title;
  GError                 *failure;

  DzlSignalGroup         *file_signals;

  GFileMonitor           *file_monitor;

  gulong                  change_monitor_changed_handler;

  guint                   check_modified_timeout;

  guint                   diagnostics_sequence;

  GTimeVal                mtime;

  gint                    hold_count;
  guint                   reclamation_handler;

  guint                   settling_handler;

  gsize                   change_count;

  guint                   cancel_cursor_restore : 1;
  guint                   changed_on_volume : 1;
  guint                   highlight_diagnostics : 1;
  guint                   loading : 1;
  guint                   mtime_set : 1;
  guint                   read_only : 1;
  guint                   failed : 1;
} IdeBufferPrivate;

typedef struct
{
  GPtrArray         *resolvers;

  IdeSourceLocation *location;

  IdeSymbol         *symbol;
} LookUpSymbolData;

G_DEFINE_TYPE_WITH_PRIVATE (IdeBuffer, ide_buffer, GTK_SOURCE_TYPE_BUFFER)

DZL_DEFINE_COUNTER (instances, "IdeBuffer", "Instances", "Number of IdeBuffer instances.")

enum {
  PROP_0,
  PROP_BUSY,
  PROP_CHANGED_ON_VOLUME,
  PROP_CONTEXT,
  PROP_FAILED,
  PROP_FILE,
  PROP_HAS_DIAGNOSTICS,
  PROP_HIGHLIGHT_DIAGNOSTICS,
  PROP_READ_ONLY,
  PROP_STYLE_SCHEME_NAME,
  PROP_TITLE,
  LAST_PROP
};

enum {
  CHANGE_SETTLED,
  CURSOR_MOVED,
  DESTROY,
  LINE_FLAGS_CHANGED,
  LOADED,
  SAVED,
  SYMBOL_RESOLVERS_LOADED,
  LAST_SIGNAL
};

static GParamSpec *properties [LAST_PROP];
static guint signals [LAST_SIGNAL];

static void
lookup_symbol_task_data_free (LookUpSymbolData *data)
{
  g_clear_pointer (&data->resolvers, g_ptr_array_unref);
  g_clear_pointer (&data->location, ide_source_location_unref);
  g_clear_pointer (&data->symbol, ide_symbol_unref);
  g_slice_free (LookUpSymbolData, data);
}

static void
lookup_symbol_get_extension (IdeExtensionSetAdapter *set,
                             PeasPluginInfo         *plugin_info,
                             PeasExtension          *extension,
                             gpointer                user_data)
{
  LookUpSymbolData *data = user_data;

  g_ptr_array_add (data->resolvers, IDE_SYMBOL_RESOLVER (extension));
}

static gboolean
ide_buffer_settled_cb (gpointer user_data)
{
  IdeBuffer *self = user_data;
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  g_assert (IDE_IS_BUFFER (self));

  priv->settling_handler = 0;
  g_signal_emit (self, signals [CHANGE_SETTLED], 0);

  return G_SOURCE_REMOVE;
}

static void
ide_buffer_delay_settling (IdeBuffer *self)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  g_assert (IDE_IS_BUFFER (self));

  dzl_clear_source (&priv->settling_handler);
  priv->settling_handler = gdk_threads_add_timeout (SETTLING_DELAY_MSEC,
                                                    ide_buffer_settled_cb,
                                                    self);
}

/**
 * ide_buffer_get_has_diagnostics:
 * @self: an #IdeBuffer.
 *
 * Gets the #IdeBuffer:has-diagnostics property.
 * Return whether the buffer contains diagnostic messages or not.
 *
 * Returns: %TRUE if the #IdeBuffer has diagnostics messages. Otherwise %FALSE.
 */
gboolean
ide_buffer_get_has_diagnostics (IdeBuffer *self)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUFFER (self), FALSE);

  return (priv->diagnostics != NULL) &&
         (ide_diagnostics_get_size (priv->diagnostics) > 0);
}

/**
 * ide_buffer_get_busy:
 * @self: an #IdeBuffer.
 *
 * Gets the #IdeBuffer:busy property.
 * Return whether the buffer is performing background work or not.
 *
 * Returns: %TRUE if the #IdeBuffer is performing background work. Otherwise %FALSE.
 */
gboolean
ide_buffer_get_busy (IdeBuffer *self)
{
  g_return_val_if_fail (IDE_IS_BUFFER (self), FALSE);

  /* TODO: This should be deprecated */

  return FALSE;
}

static void
ide_buffer_emit_cursor_moved (IdeBuffer *self)
{
  GtkTextMark *mark;
  GtkTextIter iter;

  g_assert (IDE_IS_BUFFER (self));

  mark = gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (self));
  gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (self), &iter, mark);
  g_signal_emit (self, signals [CURSOR_MOVED], 0, &iter);
}

static void
ide_buffer_get_iter_at_location (IdeBuffer         *self,
                                 GtkTextIter       *iter,
                                 IdeSourceLocation *location)
{
  guint line;
  guint line_offset;

  g_assert (IDE_IS_BUFFER (self));
  g_assert (iter);
  g_assert (location);

  line = ide_source_location_get_line (location);
  line_offset = ide_source_location_get_line_offset (location);

  gtk_text_buffer_get_iter_at_line_offset (GTK_TEXT_BUFFER (self), iter, line, line_offset);
}

static void
ide_buffer_release_context (gpointer  data,
                            GObject  *where_the_object_was)
{
  IdeBuffer *self = data;
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER (self));

  priv->context = NULL;

  /*
   * If the context was just lost, we handled reclamation in the buffer
   * manager while shutting down. We can safely drop our reclamation_handler
   * since it can no longer be run anyway.
   */
  if (priv->reclamation_handler != 0)
    {
      g_source_remove (priv->reclamation_handler);
      priv->reclamation_handler = 0;
    }

  IDE_EXIT;
}

static void
ide_buffer_set_context (IdeBuffer  *self,
                        IdeContext *context)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);
  IdeDiagnosticsManager *diagnostics_manager;

  g_return_if_fail (IDE_IS_BUFFER (self));
  g_return_if_fail (IDE_IS_CONTEXT (context));
  g_return_if_fail (priv->context == NULL);

  priv->context = context;

  g_object_weak_ref (G_OBJECT (context),
                     ide_buffer_release_context,
                     self);

  diagnostics_manager = ide_context_get_diagnostics_manager (context);

  dzl_signal_group_set_target (priv->diagnostics_manager_signals, diagnostics_manager);
}

void
ide_buffer_sync_to_unsaved_files (IdeBuffer *self)
{
  GBytes *content;

  g_assert (IDE_IS_BUFFER (self));

  if ((content = ide_buffer_get_content (self)))
    g_bytes_unref (content);
}

static void
ide_buffer_clear_diagnostics (IdeBuffer *self)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);
  GtkTextBuffer *buffer = (GtkTextBuffer *)self;
  GtkTextTagTable *table;
  GtkTextTag *tag;
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (IDE_IS_BUFFER (self));

  if (priv->diagnostics_line_cache != NULL)
    g_hash_table_remove_all (priv->diagnostics_line_cache);

  gtk_text_buffer_get_bounds (buffer, &begin, &end);

  table = gtk_text_buffer_get_tag_table (buffer);

  if (NULL != (tag = gtk_text_tag_table_lookup (table, TAG_NOTE)))
    dzl_gtk_text_buffer_remove_tag (buffer, tag, &begin, &end, TRUE);

  if (NULL != (tag = gtk_text_tag_table_lookup (table, TAG_WARNING)))
    dzl_gtk_text_buffer_remove_tag (buffer, tag, &begin, &end, TRUE);

  if (NULL != (tag = gtk_text_tag_table_lookup (table, TAG_DEPRECATED)))
    dzl_gtk_text_buffer_remove_tag (buffer, tag, &begin, &end, TRUE);

  if (NULL != (tag = gtk_text_tag_table_lookup (table, TAG_ERROR)))
    dzl_gtk_text_buffer_remove_tag (buffer, tag, &begin, &end, TRUE);
}

static void
ide_buffer_cache_diagnostic_line (IdeBuffer             *self,
                                  IdeSourceLocation     *begin,
                                  IdeSourceLocation     *end,
                                  IdeDiagnosticSeverity  severity)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);
  gpointer new_value = GINT_TO_POINTER (severity);
  gsize line_begin;
  gsize line_end;
  gsize i;

  g_assert (IDE_IS_BUFFER (self));
  g_assert (begin);
  g_assert (end);

  if (!priv->diagnostics_line_cache)
    return;

  line_begin = MIN (ide_source_location_get_line (begin),
                    ide_source_location_get_line (end));
  line_end = MAX (ide_source_location_get_line (begin),
                  ide_source_location_get_line (end));

  for (i = line_begin; i <= line_end; i++)
    {
      gpointer old_value;
      gpointer key = GINT_TO_POINTER (i);

      old_value = g_hash_table_lookup (priv->diagnostics_line_cache, key);

      if (new_value > old_value)
        g_hash_table_replace (priv->diagnostics_line_cache, key, new_value);
    }
}

static void
ide_buffer_update_diagnostic (IdeBuffer     *self,
                              IdeDiagnostic *diagnostic)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);
  IdeDiagnosticSeverity severity;
  const gchar *tag_name = NULL;
  IdeSourceLocation *location;
  gsize num_ranges;
  gsize i;

  g_assert (IDE_IS_BUFFER (self));
  g_assert (diagnostic);

  severity = ide_diagnostic_get_severity (diagnostic);

  switch (severity)
    {
    case IDE_DIAGNOSTIC_NOTE:
      tag_name = TAG_NOTE;
      break;

    case IDE_DIAGNOSTIC_DEPRECATED:
      tag_name = TAG_DEPRECATED;
      break;

    case IDE_DIAGNOSTIC_WARNING:
      tag_name = TAG_WARNING;
      break;

    case IDE_DIAGNOSTIC_ERROR:
    case IDE_DIAGNOSTIC_FATAL:
      tag_name = TAG_ERROR;
      break;

    case IDE_DIAGNOSTIC_IGNORED:
    default:
      return;
    }

  if (NULL != (location = ide_diagnostic_get_location (diagnostic)))
    {
      IdeFile *file;
      GtkTextIter iter1;
      GtkTextIter iter2;

      file = ide_source_location_get_file (location);

      if (file && priv->file && !ide_file_equal (file, priv->file))
        return;

      ide_buffer_cache_diagnostic_line (self, location, location, severity);

      ide_buffer_get_iter_at_location (self, &iter1, location);
      gtk_text_iter_assign (&iter2, &iter1);
      if (!gtk_text_iter_ends_line (&iter2))
        gtk_text_iter_forward_to_line_end (&iter2);
      else
        gtk_text_iter_backward_char (&iter1);

      gtk_text_buffer_apply_tag_by_name (GTK_TEXT_BUFFER (self), tag_name, &iter1, &iter2);
    }

  num_ranges = ide_diagnostic_get_num_ranges (diagnostic);

  for (i = 0; i < num_ranges; i++)
    {
      IdeSourceRange *range;
      IdeSourceLocation *begin;
      IdeSourceLocation *end;
      IdeFile *file;
      GtkTextIter iter1;
      GtkTextIter iter2;

      range = ide_diagnostic_get_range (diagnostic, i);
      begin = ide_source_range_get_begin (range);
      end = ide_source_range_get_end (range);

      file = ide_source_location_get_file (begin);

      if (file && priv->file && !ide_file_equal (file, priv->file))
        {
          /* Ignore */
        }

      ide_buffer_get_iter_at_location (self, &iter1, begin);
      ide_buffer_get_iter_at_location (self, &iter2, end);

      ide_buffer_cache_diagnostic_line (self, begin, end, severity);

      if (gtk_text_iter_equal (&iter1, &iter2))
        {
          if (!gtk_text_iter_ends_line (&iter2))
            gtk_text_iter_forward_char (&iter2);
          else
            gtk_text_iter_backward_char (&iter1);
        }

      gtk_text_buffer_apply_tag_by_name (GTK_TEXT_BUFFER (self), tag_name, &iter1, &iter2);
    }
}

static void
ide_buffer_update_diagnostics (IdeBuffer      *self,
                               IdeDiagnostics *diagnostics)
{
  gsize size;
  gsize i;

  g_assert (IDE_IS_BUFFER (self));
  g_assert (diagnostics);

  size = ide_diagnostics_get_size (diagnostics);

  for (i = 0; i < size; i++)
    {
      IdeDiagnostic *diagnostic;

      diagnostic = ide_diagnostics_index (diagnostics, i);
      if (diagnostic != NULL)
        ide_buffer_update_diagnostic (self, diagnostic);
    }
}

static void
ide_buffer_set_diagnostics (IdeBuffer      *self,
                            IdeDiagnostics *diagnostics)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER (self));
  g_assert (diagnostics != NULL);

  if (diagnostics != priv->diagnostics)
    {
      ide_buffer_clear_diagnostics (self);

      g_clear_pointer (&priv->diagnostics, ide_diagnostics_unref);

      if (diagnostics != NULL)
        {
          priv->diagnostics = ide_diagnostics_ref (diagnostics);
          ide_buffer_update_diagnostics (self, diagnostics);
        }

      g_signal_emit (self, signals [LINE_FLAGS_CHANGED], 0);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HAS_DIAGNOSTICS]);
    }

  IDE_EXIT;
}

static void
ide_buffer__diagnostics_manager__changed (IdeBuffer             *self,
                                          IdeDiagnosticsManager *diagnostics_manager)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);
  g_autoptr(IdeDiagnostics) diagnostics = NULL;
  GFile *file;
  guint sequence;

  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER (self));
  g_assert (IDE_IS_DIAGNOSTICS_MANAGER (diagnostics_manager));

  /*
   * To avoid updating diagnostics on every change event (which could happen a
   * lot) we check the sequence number with our last one to see if anything has
   * changed in this specific buffer.
   */

  file = ide_file_get_file (priv->file);
  sequence = ide_diagnostics_manager_get_sequence_for_file (diagnostics_manager, file);

  if (sequence != priv->diagnostics_sequence)
    {
      diagnostics = ide_diagnostics_manager_get_diagnostics_for_file (diagnostics_manager, file);
      ide_buffer_set_diagnostics (self, diagnostics);
      priv->diagnostics_sequence = sequence;
    }

  IDE_EXIT;
}

static void
ide_buffer__file_load_settings_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  g_autoptr(IdeBuffer) self = user_data;
  IdeFile *file = (IdeFile *)object;
  g_autoptr(IdeFileSettings) file_settings = NULL;

  g_assert (IDE_IS_BUFFER (self));
  g_assert (IDE_IS_FILE (file));

  file_settings = ide_file_load_settings_finish (file, result, NULL);

  if (file_settings)
    {
      gboolean insert_trailing_newline;

      insert_trailing_newline = ide_file_settings_get_insert_trailing_newline (file_settings);
      gtk_source_buffer_set_implicit_trailing_newline (GTK_SOURCE_BUFFER (self),
                                                       insert_trailing_newline);
    }
}

static void
ide_buffer__change_monitor_changed_cb (IdeBuffer              *self,
                                       IdeBufferChangeMonitor *monitor)
{
  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER (self));
  g_assert (IDE_IS_BUFFER_CHANGE_MONITOR (monitor));

  g_signal_emit (self, signals [LINE_FLAGS_CHANGED], 0);

  IDE_EXIT;
}

static void
ide_buffer_reload_change_monitor (IdeBuffer *self)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  g_assert (IDE_IS_BUFFER (self));

  if (priv->change_monitor)
    {
      dzl_clear_signal_handler (priv->change_monitor, &priv->change_monitor_changed_handler);
      g_clear_object (&priv->change_monitor);
    }

  if (!priv->loading && priv->context && priv->file)
    {
      IdeVcs *vcs;

      vcs = ide_context_get_vcs (priv->context);
      priv->change_monitor = ide_vcs_get_buffer_change_monitor (vcs, self);
      if (priv->change_monitor != NULL)
        {
          priv->change_monitor_changed_handler =
            g_signal_connect_object (priv->change_monitor,
                                     "changed",
                                     G_CALLBACK (ide_buffer__change_monitor_changed_cb),
                                     self,
                                     G_CONNECT_SWAPPED);
          ide_buffer_change_monitor_reload (priv->change_monitor);
        }
    }
}

static void
ide_buffer_do_modeline (IdeBuffer *self)
{
  g_autofree gchar *line = NULL;
  IdeFile *ifile;
  GtkTextIter begin;
  GtkTextIter end;
  GtkSourceLanguageManager *manager;
  GtkSourceLanguage *old_lang, *new_lang;
  const gchar *new_id, *old_id = NULL;
  g_autofree gchar *content_type = NULL;
  const gchar *file_path;
  gboolean uncertain;

  g_assert (IDE_IS_BUFFER (self));

  gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (self), &begin);
  end = begin;
  gtk_text_iter_forward_to_line_end (&end);
  line = gtk_text_iter_get_slice (&begin, &end);

  ifile = ide_buffer_get_file (self);
  file_path = ide_file_get_path (ifile);

  manager = gtk_source_language_manager_get_default ();
  content_type = g_content_type_guess (file_path, (guchar*)line, strlen(line), &uncertain);
  if (uncertain)
    return;
  new_lang = gtk_source_language_manager_guess_language (manager, file_path, content_type);
  if (new_lang == NULL)
    return;
  new_id = gtk_source_language_get_id (new_lang);

  old_lang = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (self));
  if (old_lang != NULL)
    old_id = gtk_source_language_get_id (old_lang);

  if (old_id == NULL || !dzl_str_equal0 (old_id, new_id))
    _ide_file_set_content_type (ifile, content_type);
}

static void
ide_buffer_changed (GtkTextBuffer *buffer)
{
  IdeBuffer *self = (IdeBuffer *)buffer;
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  GTK_TEXT_BUFFER_CLASS (ide_buffer_parent_class)->changed (buffer);

  priv->change_count++;

  g_clear_pointer (&priv->content, g_bytes_unref);

  ide_buffer_delay_settling (self);
}

static void
ide_buffer_delete_range (GtkTextBuffer *buffer,
                         GtkTextIter   *start,
                         GtkTextIter   *end)
{
  IDE_ENTRY;

#ifdef IDE_ENABLE_TRACE
  {
    gint begin_line, begin_offset;
    gint end_line, end_offset;

    begin_line = gtk_text_iter_get_line (start);
    begin_offset = gtk_text_iter_get_line_offset (start);
    end_line = gtk_text_iter_get_line (end);
    end_offset = gtk_text_iter_get_line_offset (end);

    IDE_TRACE_MSG ("delete-range (%d:%d, %d:%d)",
                   begin_line, begin_offset,
                   end_line, end_offset);
  }
#endif

  GTK_TEXT_BUFFER_CLASS (ide_buffer_parent_class)->delete_range (buffer, start, end);

  ide_buffer_emit_cursor_moved (IDE_BUFFER (buffer));

  IDE_EXIT;
}

static void
ide_buffer_insert_text (GtkTextBuffer *buffer,
                        GtkTextIter   *location,
                        const gchar   *text,
                        gint           len)
{
  gboolean check_modeline = FALSE;

  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (location);
  g_assert (text);

  /*
   * If we are inserting a \n at the end of the first line, then we might want to adjust the
   * GtkSourceBuffer:language property to reflect the format. This is similar to emacs "modelines",
   * which is apparently a bit of an overloaded term as is not to be confused with editor setting
   * modelines.
   */
  if ((gtk_text_iter_get_line (location) == 0) && gtk_text_iter_ends_line (location) &&
      ((text [0] == '\n') || ((len > 1) && (strchr (text, '\n') != NULL))))
    check_modeline = TRUE;

  GTK_TEXT_BUFFER_CLASS (ide_buffer_parent_class)->insert_text (buffer, location, text, len);

  ide_buffer_emit_cursor_moved (IDE_BUFFER (buffer));

  if (check_modeline)
    ide_buffer_do_modeline (IDE_BUFFER (buffer));
}

static void
ide_buffer_mark_set (GtkTextBuffer     *buffer,
                     const GtkTextIter *iter,
                     GtkTextMark       *mark)
{
  GTK_TEXT_BUFFER_CLASS (ide_buffer_parent_class)->mark_set (buffer, iter, mark);

  if (G_UNLIKELY (mark == gtk_text_buffer_get_insert (buffer)))
    ide_buffer_emit_cursor_moved (IDE_BUFFER (buffer));
}

static gboolean
do_check_modified (gpointer user_data)
{
  IdeBuffer *self = user_data;
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER (self));

  priv->check_modified_timeout = 0;

  ide_buffer_check_for_volume_change (self);

  IDE_RETURN (G_SOURCE_REMOVE);
}

static void
ide_buffer_queue_modify_check (IdeBuffer *self)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  g_assert (IDE_IS_BUFFER (self));

  if (priv->check_modified_timeout != 0)
    {
      g_source_remove (priv->check_modified_timeout);
      priv->check_modified_timeout = 0;
    }

  priv->check_modified_timeout = g_timeout_add_seconds (MODIFICATION_TIMEOUT_SECS,
                                                        do_check_modified,
                                                        self);
}

static void
ide_buffer__file_monitor_changed (IdeBuffer         *self,
                                  GFile             *file,
                                  GFile             *other_file,
                                  GFileMonitorEvent  event,
                                  GFileMonitor      *file_monitor)
{
  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER (self));
  g_assert (G_IS_FILE (file));
  g_assert (G_IS_FILE_MONITOR (file_monitor));

  switch (event)
    {
    case G_FILE_MONITOR_EVENT_CHANGED:
    case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
    case G_FILE_MONITOR_EVENT_MOVED:
    case G_FILE_MONITOR_EVENT_CREATED:
    case G_FILE_MONITOR_EVENT_DELETED:
    case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
    case G_FILE_MONITOR_EVENT_RENAMED:
      IDE_TRACE_MSG ("buffer change event = %d", (int)event);
      ide_buffer_queue_modify_check (self);
      break;

    case G_FILE_MONITOR_EVENT_PRE_UNMOUNT:
    case G_FILE_MONITOR_EVENT_UNMOUNTED:
    case G_FILE_MONITOR_EVENT_MOVED_IN:
    case G_FILE_MONITOR_EVENT_MOVED_OUT:
    default:
      break;
    }

  IDE_EXIT;
}

static void
ide_buffer__file_notify_file (IdeBuffer  *self,
                              GParamSpec *pspec,
                              IdeFile    *file)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);
  GFile *gfile;

  g_assert (IDE_IS_BUFFER (self));
  g_assert (IDE_IS_FILE (file));

  gfile = ide_file_get_file (file);

  if (priv->file_monitor)
    {
      g_file_monitor_cancel (priv->file_monitor);
      g_clear_object (&priv->file_monitor);
    }

  if (gfile != NULL)
    {
      GError *error = NULL;

      priv->file_monitor = g_file_monitor_file (gfile, G_FILE_MONITOR_NONE, NULL, &error);

      if (priv->file_monitor != NULL)
        {
          g_signal_connect_object (priv->file_monitor,
                                   "changed",
                                   G_CALLBACK (ide_buffer__file_monitor_changed),
                                   self,
                                   G_CONNECT_SWAPPED);
        }
      else if (error != NULL)
        {
          g_debug ("Failed to create GFileMonitor: %s", error->message);
          g_clear_error (&error);
        }
    }
}

static void
ide_buffer__file_notify_language (IdeBuffer  *self,
                                  GParamSpec *pspec,
                                  IdeFile    *file)
{
  GtkSourceLanguage *language;

  g_assert (IDE_IS_BUFFER (self));
  g_assert (IDE_IS_FILE (file));

  language = ide_file_get_language (file);
  gtk_source_buffer_set_language (GTK_SOURCE_BUFFER (self), language);

  ide_file_load_settings_async (file,
                                NULL,
                                ide_buffer__file_load_settings_cb,
                                g_object_ref (self));

  ide_buffer_reload_change_monitor (self);
}

static void
ide_buffer_notify_language (IdeBuffer  *self,
                            GParamSpec *pspec,
                            gpointer    unused)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);
  GtkSourceLanguage *language;
  const gchar *lang_id = NULL;

  g_assert (IDE_IS_BUFFER (self));
  g_assert (pspec != NULL);

  if ((language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (self))))
    lang_id = gtk_source_language_get_id (language);

  if (priv->rename_provider_adapter != NULL)
    ide_extension_adapter_set_value (priv->rename_provider_adapter, lang_id);

  if (priv->symbol_resolvers_adapter != NULL)
    ide_extension_set_adapter_set_value (priv->symbol_resolvers_adapter, lang_id);

  if (priv->formatter_adapter != NULL)
    ide_extension_adapter_set_value (priv->formatter_adapter, lang_id);
}

static void
apply_style (GtkTextTag  *tag,
             const gchar *first_property,
             ...)
{
  va_list args;

  g_assert (!tag || GTK_IS_TEXT_TAG (tag));
  g_assert (first_property != NULL);

  if (tag == NULL)
    return;

  va_start (args, first_property);
  g_object_set_valist (G_OBJECT (tag), first_property, args);
  va_end (args);
}

static void
ide_buffer_notify_style_scheme (IdeBuffer  *self,
                                GParamSpec *pspec,
                                gpointer    unused)
{
  GtkSourceStyleScheme *style_scheme;
  GtkTextTagTable *table;
  GdkRGBA deprecated_rgba;
  GdkRGBA error_rgba;
  GdkRGBA note_rgba;
  GdkRGBA warning_rgba;

  g_assert (IDE_IS_BUFFER (self));
  g_assert (pspec != NULL);

  style_scheme = gtk_source_buffer_get_style_scheme (GTK_SOURCE_BUFFER (self));
  table = gtk_text_buffer_get_tag_table (GTK_TEXT_BUFFER (self));

#define GET_TAG(name) (gtk_text_tag_table_lookup(table, name))

  if (style_scheme != NULL)
    {
      /*
       * These are used as a fall-back if our style scheme isn't installed.
       */
      gdk_rgba_parse (&deprecated_rgba, DEPRECATED_COLOR);
      gdk_rgba_parse (&error_rgba, ERROR_COLOR);
      gdk_rgba_parse (&note_rgba, NOTE_COLOR);
      gdk_rgba_parse (&warning_rgba, WARNING_COLOR);

      if (!ide_source_style_scheme_apply_style (style_scheme,
                                                TAG_DEPRECATED,
                                                GET_TAG (TAG_DEPRECATED)))
        apply_style (GET_TAG (TAG_DEPRECATED),
                     "underline", PANGO_UNDERLINE_ERROR,
                     "underline-rgba", &deprecated_rgba,
                     NULL);

      if (!ide_source_style_scheme_apply_style (style_scheme,
                                                TAG_ERROR,
                                                GET_TAG (TAG_ERROR)))
        apply_style (GET_TAG (TAG_ERROR),
                     "underline", PANGO_UNDERLINE_ERROR,
                     "underline-rgba", &error_rgba,
                     NULL);

      if (!ide_source_style_scheme_apply_style (style_scheme,
                                                TAG_NOTE,
                                                GET_TAG (TAG_NOTE)))
        apply_style (GET_TAG (TAG_NOTE),
                     "underline", PANGO_UNDERLINE_ERROR,
                     "underline-rgba", &note_rgba,
                     NULL);

      if (!ide_source_style_scheme_apply_style (style_scheme,
                                                TAG_WARNING,
                                                GET_TAG (TAG_WARNING)))
        apply_style (GET_TAG (TAG_WARNING),
                     "underline", PANGO_UNDERLINE_ERROR,
                     "underline-rgba", &warning_rgba,
                     NULL);

      if (!ide_source_style_scheme_apply_style (style_scheme,
                                                TAG_SNIPPET_TAB_STOP,
                                                GET_TAG (TAG_SNIPPET_TAB_STOP)))
        apply_style (GET_TAG (TAG_SNIPPET_TAB_STOP),
                     "underline", PANGO_UNDERLINE_SINGLE,
                     NULL);

      if (!ide_source_style_scheme_apply_style (style_scheme,
                                                TAG_DEFINITION,
                                                GET_TAG (TAG_DEFINITION)))
        apply_style (GET_TAG (TAG_DEFINITION),
                     "underline", PANGO_UNDERLINE_SINGLE,
                     NULL);

      if (!ide_source_style_scheme_apply_style (style_scheme,
                                                TAG_CURRENT_BKPT,
                                                GET_TAG (TAG_CURRENT_BKPT)))
        apply_style (GET_TAG (TAG_CURRENT_BKPT),
                     "paragraph-background", CURRENT_BKPT_BG,
                     "foreground", CURRENT_BKPT_FG,
                     NULL);
    }

#undef GET_TAG
}

static void
ide_buffer_on_tag_added (IdeBuffer       *self,
                         GtkTextTag      *tag,
                         GtkTextTagTable *table)
{
  GtkTextTag *chunk_tag;

  g_assert (IDE_IS_BUFFER (self));
  g_assert (GTK_IS_TEXT_TAG (tag));
  g_assert (GTK_IS_TEXT_TAG_TABLE (table));

  /*
   * Adjust priority of our tab-stop tag.
   */

  chunk_tag = gtk_text_tag_table_lookup (table, "snippet::tab-stop");
  if (chunk_tag != NULL)
    gtk_text_tag_set_priority (chunk_tag,
                               gtk_text_tag_table_get_size (table) - 1);
}

static void
ide_buffer_loaded (IdeBuffer *self)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);
  GtkSourceLanguage *language;
  GtkSourceLanguage *current;

  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER (self));

  /*
   * It is possible our source language has changed since the buffer loaded (as loading
   * contents provides us the opportunity to inspect file contents and get a more
   * accurate content-type).
   */
  language = ide_file_get_language (priv->file);
  current = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (self));
  if (current != language)
    gtk_source_buffer_set_language (GTK_SOURCE_BUFFER (self), language);

  /*
   * Force the views to reload language state.
   */
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FILE]);

  /* This is suspended until we've loaded */
  ide_highlight_engine_unpause (priv->highlight_engine);

  /* Unblock our previously blocked signals */
  dzl_signal_group_unblock (priv->diagnostics_manager_signals);

  /* Now we can reload the change monitor which was probably skipped
   * during the early initializtion.
   */
  ide_buffer_reload_change_monitor (self);

  IDE_EXIT;
}

static void
ide_buffer_load_formatter (IdeBuffer           *self,
                           GParamSpec          *pspec,
                           IdeExtensionAdapter *adapter)
{
  IdeFormatter *formatter;

  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER (self));
  g_assert (IDE_IS_EXTENSION_ADAPTER (adapter));

  formatter = ide_extension_adapter_get_extension (adapter);

  if (formatter != NULL)
    ide_formatter_load (formatter);

  IDE_EXIT;
}

static void
ide_buffer_load_rename_provider (IdeBuffer           *self,
                                 GParamSpec          *pspec,
                                 IdeExtensionAdapter *adapter)
{
  IdeRenameProvider *provider;

  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER (self));
  g_assert (IDE_IS_EXTENSION_ADAPTER (adapter));

  provider = ide_extension_adapter_get_extension (adapter);

  if (provider != NULL)
    {
      g_object_set (provider, "buffer", self, NULL);
      ide_rename_provider_load (provider);
    }

  IDE_EXIT;
}

static void
ide_buffer_load_symbol_resolver (IdeExtensionSetAdapter *adapter,
                                 PeasPluginInfo         *plugin_info,
                                 PeasExtension          *extension,
                                 gpointer                user_data)
{
  ide_symbol_resolver_load (IDE_SYMBOL_RESOLVER (extension));
}

static void
ide_buffer_load_symbol_resolvers (IdeBuffer              *self,
                                  IdeExtensionSetAdapter *adapter)
{
  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER (self));
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (adapter));

  if (!ide_extension_set_adapter_get_n_extensions (adapter))
    return;

  ide_extension_set_adapter_foreach (adapter,
                                     ide_buffer_load_symbol_resolver,
                                     NULL);

  g_signal_emit (self, signals [SYMBOL_RESOLVERS_LOADED], 0);

  IDE_EXIT;
}

static void
ide_buffer_addin_added (PeasExtensionSet *set,
                        PeasPluginInfo   *plugin_info,
                        PeasExtension    *exten,
                        gpointer          user_data)
{
  IdeBufferAddin *addin = (IdeBufferAddin *)exten;
  IdeBuffer *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_BUFFER_ADDIN (addin));
  g_assert (IDE_IS_BUFFER (self));

  g_debug ("loading IdeBufferAddin from %s",
           peas_plugin_info_get_module_name (plugin_info));

  ide_buffer_addin_load (addin, self);
}

static void
ide_buffer_addin_removed (PeasExtensionSet *set,
                          PeasPluginInfo   *plugin_info,
                          PeasExtension    *exten,
                          gpointer          user_data)
{
  IdeBufferAddin *addin = (IdeBufferAddin *)exten;
  IdeBuffer *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_BUFFER_ADDIN (addin));
  g_assert (IDE_IS_BUFFER (self));

  g_debug ("unloading IdeBufferAddin from %s",
           peas_plugin_info_get_module_name (plugin_info));

  ide_buffer_addin_unload (addin, self);
}

static void
ide_buffer_init_tags (IdeBuffer *self)
{
  GtkTextTagTable *tag_table;
  GtkSourceStyleScheme *style_scheme;
  g_autoptr(GtkTextTag) deprecated_tag = NULL;
  g_autoptr(GtkTextTag) error_tag = NULL;
  g_autoptr(GtkTextTag) note_tag = NULL;
  g_autoptr(GtkTextTag) warning_tag = NULL;
  GdkRGBA deprecated_rgba;
  GdkRGBA error_rgba;
  GdkRGBA note_rgba;
  GdkRGBA warning_rgba;

  g_assert (IDE_IS_BUFFER (self));

  tag_table = gtk_text_buffer_get_tag_table (GTK_TEXT_BUFFER (self));
  style_scheme = gtk_source_buffer_get_style_scheme (GTK_SOURCE_BUFFER (self));

  /*
   * These are used as a fall-back if our style scheme isn't installed.
   */
  gdk_rgba_parse (&deprecated_rgba, DEPRECATED_COLOR);
  gdk_rgba_parse (&error_rgba, ERROR_COLOR);
  gdk_rgba_parse (&note_rgba, NOTE_COLOR);
  gdk_rgba_parse (&warning_rgba, WARNING_COLOR);

  /*
   * NOTE:
   *
   * The tag table assigns priority upon insert. Each successive insert
   * is higher priority than the last.
   */

  deprecated_tag = gtk_text_tag_new (TAG_DEPRECATED);
  error_tag = gtk_text_tag_new (TAG_ERROR);
  note_tag = gtk_text_tag_new (TAG_NOTE);
  warning_tag = gtk_text_tag_new (TAG_WARNING);

  if (!ide_source_style_scheme_apply_style (style_scheme, TAG_DEPRECATED, deprecated_tag))
    apply_style (deprecated_tag,
                 "underline", PANGO_UNDERLINE_ERROR,
                 "underline-rgba", &deprecated_rgba,
                 NULL);

  if (!ide_source_style_scheme_apply_style (style_scheme, TAG_ERROR, error_tag))
    apply_style (error_tag,
                 "underline", PANGO_UNDERLINE_ERROR,
                 "underline-rgba", &error_rgba,
                 NULL);

  if (!ide_source_style_scheme_apply_style (style_scheme, TAG_NOTE, note_tag))
    apply_style (note_tag,
                 "underline", PANGO_UNDERLINE_ERROR,
                 "underline-rgba", &note_rgba,
                 NULL);

  if (!ide_source_style_scheme_apply_style (style_scheme, TAG_NOTE, warning_tag))
    apply_style (warning_tag,
                 "underline", PANGO_UNDERLINE_ERROR,
                 "underline-rgba", &warning_rgba,
                 NULL);

  gtk_text_tag_table_add (tag_table, deprecated_tag);
  gtk_text_tag_table_add (tag_table, error_tag);
  gtk_text_tag_table_add (tag_table, note_tag);
  gtk_text_tag_table_add (tag_table, warning_tag);

  gtk_text_buffer_create_tag (GTK_TEXT_BUFFER (self), TAG_SNIPPET_TAB_STOP,
                              NULL);
  gtk_text_buffer_create_tag (GTK_TEXT_BUFFER (self), TAG_DEFINITION,
                              "underline", PANGO_UNDERLINE_SINGLE,
                              NULL);
  gtk_text_buffer_create_tag (GTK_TEXT_BUFFER (self), TAG_CURRENT_BKPT,
                              "paragraph-background", CURRENT_BKPT_BG,
                              "foreground", CURRENT_BKPT_FG,
                              NULL);

  g_signal_connect_object (tag_table,
                           "tag-added",
                           G_CALLBACK (ide_buffer_on_tag_added),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
ide_buffer_constructed (GObject *object)
{
  IdeBuffer *self = (IdeBuffer *)object;
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  g_assert (IDE_IS_BUFFER (self));
  g_assert (IDE_IS_CONTEXT (priv->context));

  G_OBJECT_CLASS (ide_buffer_parent_class)->constructed (object);

  ide_buffer_init_tags (self);

  priv->highlight_engine = ide_highlight_engine_new (self);
  ide_highlight_engine_pause (priv->highlight_engine);

  priv->addins = peas_extension_set_new (peas_engine_get_default (),
                                         IDE_TYPE_BUFFER_ADDIN,
                                         NULL);

  g_signal_connect (priv->addins,
                    "extension-added",
                    G_CALLBACK (ide_buffer_addin_added),
                    self);

  g_signal_connect (priv->addins,
                    "extension-removed",
                    G_CALLBACK (ide_buffer_addin_removed),
                    self);

  peas_extension_set_foreach (priv->addins, ide_buffer_addin_added, self);

  priv->formatter_adapter = ide_extension_adapter_new (priv->context,
                                                       NULL,
                                                       IDE_TYPE_FORMATTER,
                                                       "Formatter-Languages",
                                                       NULL);

  g_signal_connect_object (priv->formatter_adapter,
                           "notify::extension",
                           G_CALLBACK (ide_buffer_load_formatter),
                           self,
                           G_CONNECT_SWAPPED);

  priv->rename_provider_adapter = ide_extension_adapter_new (priv->context,
                                                             NULL,
                                                             IDE_TYPE_RENAME_PROVIDER,
                                                             "Rename-Provider-Languages",
                                                             NULL);

  g_signal_connect_object (priv->rename_provider_adapter,
                           "notify::extension",
                           G_CALLBACK (ide_buffer_load_rename_provider),
                           self,
                           G_CONNECT_SWAPPED);

  priv->symbol_resolvers_adapter = ide_extension_set_adapter_new (priv->context,
                                                                  peas_engine_get_default (),
                                                                  IDE_TYPE_SYMBOL_RESOLVER,
                                                                  "Symbol-Resolver-Languages",
                                                                  NULL);

  g_signal_connect_object (priv->symbol_resolvers_adapter,
                           "extensions-loaded",
                           G_CALLBACK (ide_buffer_load_symbol_resolvers),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect (self,
                    "notify::language",
                    G_CALLBACK (ide_buffer_notify_language),
                    NULL);

  g_object_notify (G_OBJECT (self), "language");

  g_signal_connect (self,
                    "notify::style-scheme",
                    G_CALLBACK (ide_buffer_notify_style_scheme),
                    NULL);
}

static void
ide_buffer_dispose (GObject *object)
{
  IdeBuffer *self = (IdeBuffer *)object;
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  IDE_ENTRY;

  dzl_clear_source (&priv->settling_handler);
  dzl_clear_source (&priv->reclamation_handler);
  dzl_clear_source (&priv->check_modified_timeout);

  g_clear_pointer (&priv->failure, g_error_free);

  if (priv->context != NULL)
    {
      IdeBufferManager *buffer_manager = ide_context_get_buffer_manager (priv->context);

      _ide_buffer_manager_reclaim (buffer_manager, self);
    }

  if (priv->file_monitor != NULL)
    {
      g_file_monitor_cancel (priv->file_monitor);
      g_clear_object (&priv->file_monitor);
    }

  dzl_signal_group_set_target (priv->file_signals, NULL);

  if (priv->highlight_engine != NULL)
    g_object_run_dispose (G_OBJECT (priv->highlight_engine));

  if (priv->change_monitor != NULL)
    {
      dzl_clear_signal_handler (priv->change_monitor, &priv->change_monitor_changed_handler);
      g_clear_object (&priv->change_monitor);
    }

  dzl_signal_group_set_target (priv->diagnostics_manager_signals, NULL);

  g_clear_pointer (&priv->diagnostics_line_cache, g_hash_table_unref);
  g_clear_pointer (&priv->diagnostics, ide_diagnostics_unref);
  g_clear_object (&priv->addins);
  g_clear_object (&priv->highlight_engine);
  g_clear_object (&priv->rename_provider_adapter);
  g_clear_object (&priv->symbol_resolvers_adapter);

  G_OBJECT_CLASS (ide_buffer_parent_class)->dispose (object);

  IDE_EXIT;
}

static void
ide_buffer_finalize (GObject *object)
{
  IdeBuffer *self = (IdeBuffer *)object;
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  IDE_ENTRY;

  g_clear_object (&priv->file_signals);
  g_clear_object (&priv->file);
  g_clear_pointer (&priv->title, g_free);
  g_clear_pointer (&priv->content, g_bytes_unref);

  if (priv->context != NULL)
    {
      g_object_weak_unref (G_OBJECT (priv->context),
                           ide_buffer_release_context,
                           self);
      priv->context = NULL;
    }

  G_OBJECT_CLASS (ide_buffer_parent_class)->finalize (object);

  DZL_COUNTER_DEC (instances);

  IDE_EXIT;
}

static void
ide_buffer_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  IdeBuffer *self = IDE_BUFFER (object);

  switch (prop_id)
    {
    case PROP_BUSY:
      g_value_set_boolean (value, ide_buffer_get_busy (self));
      break;

    case PROP_CHANGED_ON_VOLUME:
      g_value_set_boolean (value, ide_buffer_get_changed_on_volume (self));
      break;

    case PROP_CONTEXT:
      g_value_set_object (value, ide_buffer_get_context (self));
      break;

    case PROP_FAILED:
      g_value_set_boolean (value, ide_buffer_get_failed (self));
      break;

    case PROP_FILE:
      g_value_set_object (value, ide_buffer_get_file (self));
      break;

    case PROP_HAS_DIAGNOSTICS:
      g_value_set_boolean (value, ide_buffer_get_has_diagnostics (self));
      break;

    case PROP_HIGHLIGHT_DIAGNOSTICS:
      g_value_set_boolean (value, ide_buffer_get_highlight_diagnostics (self));
      break;

    case PROP_READ_ONLY:
      g_value_set_boolean (value, ide_buffer_get_read_only (self));
      break;

    case PROP_TITLE:
      g_value_set_string (value, ide_buffer_get_title (self));
      break;

    case PROP_STYLE_SCHEME_NAME:
      g_value_set_string (value, ide_buffer_get_style_scheme_name (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_buffer_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  IdeBuffer *self = IDE_BUFFER (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      ide_buffer_set_context (self, g_value_get_object (value));
      break;

    case PROP_FILE:
      ide_buffer_set_file (self, g_value_get_object (value));
      break;

    case PROP_HIGHLIGHT_DIAGNOSTICS:
      ide_buffer_set_highlight_diagnostics (self, g_value_get_boolean (value));
      break;

    case PROP_STYLE_SCHEME_NAME:
      ide_buffer_set_style_scheme_name (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_buffer_class_init (IdeBufferClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkTextBufferClass *text_buffer_class = GTK_TEXT_BUFFER_CLASS (klass);

  object_class->constructed = ide_buffer_constructed;
  object_class->dispose = ide_buffer_dispose;
  object_class->finalize = ide_buffer_finalize;
  object_class->get_property = ide_buffer_get_property;
  object_class->set_property = ide_buffer_set_property;

  text_buffer_class->changed = ide_buffer_changed;
  text_buffer_class->delete_range = ide_buffer_delete_range;
  text_buffer_class->insert_text = ide_buffer_insert_text;
  text_buffer_class->mark_set = ide_buffer_mark_set;

  properties [PROP_BUSY] =
    g_param_spec_boolean ("busy",
                          "Busy",
                          "If the buffer is performing background work.",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_CHANGED_ON_VOLUME] =
    g_param_spec_boolean ("changed-on-volume",
                          "Changed on Volume",
                          "If the file has changed on disk and the buffer is not in sync.",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_CONTEXT] =
    g_param_spec_object ("context",
                         "Context",
                         "The IdeContext for the buffer.",
                         IDE_TYPE_CONTEXT,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * IdeBuffer:failed:
   *
   * The "failed" property is set to %TRUE if the buffer failed to load.
   *
   * Since: 3.28
   */
  properties [PROP_FAILED] =
    g_param_spec_boolean ("failed",
                          "Failed",
                          "If the buffer has failed in loading.",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_FILE] =
    g_param_spec_object ("file",
                         "File",
                         "The file represented by the buffer.",
                         IDE_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_HAS_DIAGNOSTICS] =
    g_param_spec_boolean ("has-diagnostics",
                          "Has Diagnostics",
                          "If the buffer contains diagnostic messages.",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_HIGHLIGHT_DIAGNOSTICS] =
    g_param_spec_boolean ("highlight-diagnostics",
                          "Highlight Diagnostics",
                          "If diagnostic warnings and errors should be highlighted.",
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_READ_ONLY] =
    g_param_spec_boolean ("read-only",
                          "Read Only",
                          "If the underlying file is read only.",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_STYLE_SCHEME_NAME] =
    g_param_spec_string ("style-scheme-name",
                         "Style Scheme Name",
                         "Style Scheme Name",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "The title of the buffer.",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  /**
   * IdeBuffer::change-settled:
   * @self: An #IdeBuffer
   *
   * This signal is emitted as short period of time after changes have
   * occurred. It provides plugins a convenient way to wait for the editor
   * to settle before performing expensive work.
   *
   * You should probably use this instead of implementing your own
   * settling management.
   *
   * Since: 3.26
   */
  signals [CHANGE_SETTLED] =
    g_signal_new ("change-settled",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  g_signal_set_va_marshaller (signals [CHANGE_SETTLED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__VOIDv);

  /**
   * IdeBuffer::cursor-moved:
   * @self: An #IdeBuffer.
   * @location: a #GtkTextIter.
   *
   * This signal is emitted when the insertion location has moved. You might
   * want to attach to this signal to update the location of the insert mark in
   * the display.
   */
  signals [CURSOR_MOVED] =
    g_signal_new ("cursor-moved",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeBufferClass, cursor_moved),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__BOXED,
                  G_TYPE_NONE,
                  1,
                  GTK_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (signals [CURSOR_MOVED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__BOXEDv);

  /**
   * IdeBuffer::line-flags-changed:
   *
   * This signal is emitted when the calculated line flags have changed. This occurs when
   * diagnostics and line changes have been recalculated.
   */
  signals [LINE_FLAGS_CHANGED] =
    g_signal_new ("line-flags-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);

  /**
   * IdeBuffer::loaded:
   *
   * This signal is emitted when the buffer manager has completed loading the file.
   */
  signals [LOADED] =
    g_signal_new_class_handler ("loaded",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (ide_buffer_loaded),
                                NULL, NULL, NULL,
                                G_TYPE_NONE,
                                0);

  /**
   * IdeBuffer::destroy:
   *
   * This signal is emitted when the buffer should be destroyed, as the
   * #IdeBufferManager has reclaimed the buffer.
   */
  signals [DESTROY] =
    g_signal_new ("destroy",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  /**
   * IdeBuffer::saved:
   *
   * This signal is emitted when the buffer manager has completed saving the file.
   */
  signals [SAVED] =
    g_signal_new ("saved",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);

  /**
   * IdeBuffer::symbol-resolver-loaded:
   *
   * This signal is emitted when the buffer has completed loading symbol resolvers.
   */
  signals [SYMBOL_RESOLVERS_LOADED] =
    g_signal_new_class_handler ("symbol-resolvers-loaded",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                0,
                                NULL, NULL, NULL,
                                G_TYPE_NONE,
                                0);
}

static void
ide_buffer_init (IdeBuffer *self)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  IDE_ENTRY;

  priv->loading = TRUE;

  priv->highlight_diagnostics = TRUE;

  priv->file_signals = dzl_signal_group_new (IDE_TYPE_FILE);
  dzl_signal_group_connect_object (priv->file_signals,
                                   "notify::language",
                                   G_CALLBACK (ide_buffer__file_notify_language),
                                   self,
                                   G_CONNECT_SWAPPED);
  dzl_signal_group_connect_object (priv->file_signals,
                                   "notify::file",
                                   G_CALLBACK (ide_buffer__file_notify_file),
                                   self,
                                   G_CONNECT_SWAPPED);

  priv->diagnostics_line_cache = g_hash_table_new (g_direct_hash, g_direct_equal);

  priv->diagnostics_manager_signals = dzl_signal_group_new (IDE_TYPE_DIAGNOSTICS_MANAGER);
  dzl_signal_group_connect_object (priv->diagnostics_manager_signals,
                                   "changed",
                                   G_CALLBACK (ide_buffer__diagnostics_manager__changed),
                                   self,
                                   G_CONNECT_SWAPPED);
  /* Block signals until we've been loaded */
  dzl_signal_group_block (priv->diagnostics_manager_signals);

  DZL_COUNTER_INC (instances);

  IDE_EXIT;
}

static void
ide_buffer_update_title (IdeBuffer *self)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);
  g_autofree gchar *title = NULL;

  g_return_if_fail (IDE_IS_BUFFER (self));

  if (priv->file)
    {
      GFile *workdir;
      GFile *gfile;
      IdeVcs *vcs;

      vcs = ide_context_get_vcs (priv->context);
      workdir = ide_vcs_get_working_directory (vcs);
      gfile = ide_file_get_file (priv->file);

      title = g_file_get_relative_path (workdir, gfile);
      if (!title)
        title = g_file_get_path (gfile);
    }

  g_clear_pointer (&priv->title, g_free);
  priv->title = g_strdup (title);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TITLE]);
}

/**
 * ide_buffer_get_file:
 * @self: an #IdeBuffer.
 *
 * Gets the underlying file behind the buffer.
 *
 * Returns: (transfer none): An #IdeFile.
 */
IdeFile *
ide_buffer_get_file (IdeBuffer *self)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);

  return priv->file;
}

/**
 * ide_buffer_set_file:
 * @self: an #IdeBuffer.
 * @file: An #IdeFile.
 *
 * Sets the underlying file to use when saving and loading @self to and from storage.
 */
void
ide_buffer_set_file (IdeBuffer *self,
                     IdeFile   *file)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  g_return_if_fail (IDE_IS_BUFFER (self));
  g_return_if_fail (IDE_IS_FILE (file));

  if (g_set_object (&priv->file, file))
    {
      dzl_signal_group_set_target (priv->file_signals, file);
      ide_file_load_settings_async (priv->file,
                                    NULL,
                                    ide_buffer__file_load_settings_cb,
                                    g_object_ref (self));
      ide_buffer_reload_change_monitor (self);
      /*
       * FIXME: More hack for 3.16.3. This all needs refactoring.
       *        In particular, IdeFile should probably subclass GtkSourceFile.
       */
      if (file != NULL)
        ide_buffer__file_notify_file (self, NULL, file);
      ide_buffer_update_title (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FILE]);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TITLE]);
    }
}

/**
 * ide_buffer_get_context:
 * @self: an #IdeBuffer.
 *
 * Gets the #IdeBuffer:context property. This is the #IdeContext that owns the buffer.
 *
 * Returns: (transfer none): An #IdeContext.
 */
IdeContext *
ide_buffer_get_context (IdeBuffer *self)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);

  return priv->context;
}

/**
 * ide_buffer_get_line_flags:
 * @self: an #IdeBuffer.
 * @line: a buffer line number.
 *
 * Return the flags set for the #IdeBuffer @line number.
 * (diagnostics and errors messages, line changed or added, notes)
 *
 * Returns: (transfer full): An #IdeBufferLineFlags struct.
 */
IdeBufferLineFlags
ide_buffer_get_line_flags (IdeBuffer *self,
                           guint      line)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);
  IdeBufferLineFlags flags = 0;
  IdeBufferLineChange change = 0;

  if (priv->diagnostics_line_cache)
    {
      gpointer key = GINT_TO_POINTER (line);
      gpointer value;

      value = g_hash_table_lookup (priv->diagnostics_line_cache, key);

      switch (GPOINTER_TO_INT (value))
        {
        case IDE_DIAGNOSTIC_FATAL:
        case IDE_DIAGNOSTIC_ERROR:
          flags |= IDE_BUFFER_LINE_FLAGS_ERROR;
          break;

        case IDE_DIAGNOSTIC_DEPRECATED:
        case IDE_DIAGNOSTIC_WARNING:
          flags |= IDE_BUFFER_LINE_FLAGS_WARNING;
          break;

        case IDE_DIAGNOSTIC_NOTE:
          flags |= IDE_BUFFER_LINE_FLAGS_NOTE;
          break;

        default:
          break;
        }
    }

  if (priv->change_monitor)
    {
      change = ide_buffer_change_monitor_get_change (priv->change_monitor, line);

      switch (change)
        {
        case IDE_BUFFER_LINE_CHANGE_ADDED:
          flags |= IDE_BUFFER_LINE_FLAGS_ADDED;
          break;

        case IDE_BUFFER_LINE_CHANGE_CHANGED:
          flags |= IDE_BUFFER_LINE_FLAGS_CHANGED;
          break;

        case IDE_BUFFER_LINE_CHANGE_DELETED:
          flags |= IDE_BUFFER_LINE_FLAGS_DELETED;
          break;

        case IDE_BUFFER_LINE_CHANGE_NONE:
        default:
          break;
        }
    }

  return flags;
}

/**
 * ide_buffer_get_highlight_diagnostics:
 * @self: an #IdeBuffer.
 *
 * Gets the #IdeBuffer:highlight-diagnostics property.
 * Return whether the diagnostic warnings and errors should be highlighted.
 *
 * Returns: %TRUE if diagnostics are highlighted. Otherwise %FALSE.
 */
gboolean
ide_buffer_get_highlight_diagnostics (IdeBuffer *self)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUFFER (self), FALSE);

  return priv->highlight_diagnostics;
}

/**
 * ide_buffer_set_highlight_diagnostics:
 * @self: an #IdeBuffer.
 * @highlight_diagnostics: Whether to highlight the diagnostics or not.
 *
 * Sets the #IdeBuffer:highlight-diagnostics property.
 * Sets whether the diagnostic warnings and errors should be highlighted.
 *
 */
void
ide_buffer_set_highlight_diagnostics (IdeBuffer *self,
                                      gboolean   highlight_diagnostics)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  g_return_if_fail (IDE_IS_BUFFER (self));

  highlight_diagnostics = !!highlight_diagnostics;

  if (highlight_diagnostics != priv->highlight_diagnostics)
    {
      priv->highlight_diagnostics = highlight_diagnostics;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HIGHLIGHT_DIAGNOSTICS]);
    }
}

/**
 * ide_buffer_get_diagnostic_at_iter:
 * @self: an #IdeBuffer.
 * @iter: a #GtkTextIter.
 *
 * Gets the first diagnostic that overlaps the position
 *
 * Returns: (transfer none) (nullable): An #IdeDiagnostic or %NULL.
 */
IdeDiagnostic *
ide_buffer_get_diagnostic_at_iter (IdeBuffer         *self,
                                   const GtkTextIter *iter)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);
  g_return_val_if_fail (iter, NULL);

  if (priv->diagnostics)
    {
      IdeDiagnostic *diagnostic = NULL;
      IdeBufferLineFlags flags;
      guint distance = G_MAXUINT;
      gsize size;
      gsize i;
      guint line;

      line = gtk_text_iter_get_line (iter);
      flags = ide_buffer_get_line_flags (self, line);

      if ((flags & IDE_BUFFER_LINE_FLAGS_DIAGNOSTICS_MASK) == 0)
        return NULL;

      size = ide_diagnostics_get_size (priv->diagnostics);

      for (i = 0; i < size; i++)
        {
          IdeDiagnostic *diag;
          IdeSourceLocation *location;
          GtkTextIter pos;

          diag = ide_diagnostics_index (priv->diagnostics, i);
          location = ide_diagnostic_get_location (diag);
          if (!location)
            continue;

          /* TODO: This should look at the range for the diagnostic */

          ide_buffer_get_iter_at_location (self, &pos, location);

          if (line == (guint)gtk_text_iter_get_line (&pos))
            {
              guint offset;

              offset = ABS (gtk_text_iter_get_offset (iter) - gtk_text_iter_get_offset (&pos));

              if (offset < distance)
                {
                  distance = offset;
                  diagnostic = diag;
                }
            }
        }

      return diagnostic;
    }

  return NULL;
}

static gboolean
ide_buffer_can_do_newline_hack (IdeBuffer *self,
                                guint      len)
{
  guint next_pow2;

  g_assert (IDE_IS_BUFFER (self));

  /*
   * If adding two bytes to our length (one for \n and one for \0) is still under the next
   * power of two, then we can avoid making a copy of the buffer when saving the buffer
   * to our drafts.
   *
   * HACK: This relies on the fact that GtkTextBuffer returns a GString allocated string
   *       which grows the string in powers of two.
   */

  if ((len == 0) || (len & (len - 1)) == 0)
    return FALSE;

  next_pow2 = len;
  next_pow2 |= next_pow2 >> 1;
  next_pow2 |= next_pow2 >> 2;
  next_pow2 |= next_pow2 >> 4;
  next_pow2 |= next_pow2 >> 8;
  next_pow2 |= next_pow2 >> 16;
  next_pow2++;

  return ((len + 2) < next_pow2);
}

/**
 * ide_buffer_get_content:
 * @self: an #IdeBuffer.
 *
 * Gets the contents of the buffer as GBytes.
 *
 * By using this function to get the bytes, you allow #IdeBuffer to avoid calculating the buffer
 * text unnecessarily, potentially saving on allocations.
 *
 * Additionally, this allows the buffer to update the state in #IdeUnsavedFiles if the content
 * is out of sync.
 *
 * Returns: (transfer full): a #GBytes containing the buffer content.
 */
GBytes *
ide_buffer_get_content (IdeBuffer *self)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);

  if (!priv->content)
    {
      IdeUnsavedFiles *unsaved_files;
      gchar *text;
      GtkTextIter begin;
      GtkTextIter end;
      GFile *gfile = NULL;
      gsize len;

      gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (self), &begin, &end);
      text = gtk_text_buffer_get_text (GTK_TEXT_BUFFER (self), &begin, &end, TRUE);

      /*
       * If implicit newline is set, add a \n in place of the \0 and avoid duplicating the buffer.
       * Make sure to track length beforehand, since we would overwrite afterwards. Since
       * conversion to \r\n is dealth with during save operations, this should be fine for both.
       * The unsaved files will restore to a buffer, for which \n is acceptable.
       */
      len = strlen (text);
      if (gtk_source_buffer_get_implicit_trailing_newline (GTK_SOURCE_BUFFER (self)))
        {
          if (!ide_buffer_can_do_newline_hack (self, len))
            {
              gchar *copy;

              copy = g_malloc (len + 2);
              memcpy (copy, text, len);
              g_free (text);
              text = copy;
            }

          text [len] = '\n';
          text [++len] = '\0';
        }

      /*
       * We pass a buffer that is longer than the length we tell GBytes about.
       * This way, compilers that don't want to see the trailing \0 can ignore
       * that data, but compilers that rely on valid C strings can also rely
       * on the buffer to be valid.
       */
      priv->content = g_bytes_new_take (text, len);

      if ((priv->context != NULL) &&
          (priv->file != NULL) &&
          (gfile = ide_file_get_file (priv->file)))
        {
          unsaved_files = ide_context_get_unsaved_files (priv->context);
          ide_unsaved_files_update (unsaved_files, gfile, priv->content);
        }
    }

  return g_bytes_ref (priv->content);
}

/**
 * ide_buffer_trim_trailing_whitespace:
 * @self: an #IdeBuffer.
 *
 * Trim trailing whitespaces from the buffer.
 *
 */
void
ide_buffer_trim_trailing_whitespace (IdeBuffer *self)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  gint line;

  g_return_if_fail (IDE_IS_BUFFER (self));

  buffer = GTK_TEXT_BUFFER (self);

  gtk_text_buffer_get_end_iter (buffer, &iter);

  for (line = gtk_text_iter_get_line (&iter); line >= 0; line--)
    {
      IdeBufferLineChange change = IDE_BUFFER_LINE_CHANGE_CHANGED;

      if (priv->change_monitor)
        change = ide_buffer_change_monitor_get_change (priv->change_monitor, line);

      if (change != IDE_BUFFER_LINE_CHANGE_NONE)
        {
          gtk_text_buffer_get_iter_at_line (buffer, &iter, line);

/*
 * Preserve all whitespace that isn't space or tab.
 * This could include line feed, form feed, etc.
 */
#define TEXT_ITER_IS_SPACE(ptr) \
  ({  \
    gunichar ch = gtk_text_iter_get_char (ptr); \
    (ch == ' ' || ch == '\t'); \
  })

          /*
           * Move to the first character at the end of the line (skipping the newline)
           * and progress to trip if it is white space.
           */
          if (gtk_text_iter_forward_to_line_end (&iter) &&
              !gtk_text_iter_starts_line (&iter) &&
              gtk_text_iter_backward_char (&iter) &&
              TEXT_ITER_IS_SPACE (&iter))
            {
              GtkTextIter begin = iter;

              gtk_text_iter_forward_to_line_end (&iter);

              while (TEXT_ITER_IS_SPACE (&begin))
                {
                  if (gtk_text_iter_starts_line (&begin))
                    break;

                  if (!gtk_text_iter_backward_char (&begin))
                    break;
                }

              if (!TEXT_ITER_IS_SPACE (&begin) && !gtk_text_iter_ends_line (&begin))
                gtk_text_iter_forward_char (&begin);

              if (!gtk_text_iter_equal (&begin, &iter))
                gtk_text_buffer_delete (buffer, &begin, &iter);
            }

#undef TEXT_ITER_IS_SPACE
        }
    }
}

/**
 * ide_buffer_get_title:
 * @self: an #IdeBuffer.
 *
 * Gets the #IdeBuffer:title property. This property contains a title for the buffer suitable
 * for display.
 *
 * Returns: A string containing the buffer title.
 */
const gchar *
ide_buffer_get_title (IdeBuffer *self)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);

  return priv->title;
}

/**
 * ide_buffer_get_style_scheme_name:
 * @self: an #IdeBuffer.
 *
 * Gets the #IdeBuffer:style-scheme-name property.
 * This property contains the current style scheme used by the buffer.
 *
 * Returns: (transfer none): A string containing the name of the currently used style scheme.
 */
const gchar *
ide_buffer_get_style_scheme_name (IdeBuffer *self)
{
  GtkSourceStyleScheme *scheme;

  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);

  scheme = gtk_source_buffer_get_style_scheme (GTK_SOURCE_BUFFER (self));
  if (scheme)
    return gtk_source_style_scheme_get_id (scheme);

  return NULL;
}

/**
 * ide_buffer_set_style_scheme_name:
 * @self: an #IdeBuffer.
 * @style_scheme_name: A string containing the name of the style scheme to use.
 *
 * Sets the #IdeBuffer:style-scheme-name property.
 * Sets the style scheme to be used by this buffer.
 */
void
ide_buffer_set_style_scheme_name (IdeBuffer   *self,
                                  const gchar *style_scheme_name)
{
  GtkSourceStyleSchemeManager *mgr;
  GtkSourceStyleScheme *scheme;

  g_return_if_fail (IDE_IS_BUFFER (self));

  mgr = gtk_source_style_scheme_manager_get_default ();

  scheme = gtk_source_style_scheme_manager_get_scheme (mgr, style_scheme_name);
  if (scheme)
    gtk_source_buffer_set_style_scheme (GTK_SOURCE_BUFFER (self), scheme);
}

gboolean
_ide_buffer_get_loading (IdeBuffer *self)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUFFER (self), FALSE);

  return priv->loading;
}

void
_ide_buffer_set_loading (IdeBuffer *self,
                         gboolean   loading)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_BUFFER (self));

  loading = !!loading;

  if (priv->loading != loading)
    {
      priv->loading = loading;

      if (!priv->loading)
        g_signal_emit (self, signals [LOADED], 0);
    }

  IDE_EXIT;
}

/**
 * ide_buffer_get_read_only:
 * @self: an #IdeBuffer.
 *
 * Gets the #IdeBuffer:read-only property. This property indicate if the underlying file is read only or not.
 *
 * Returns: %TRUE if the #IdeBuffer is read only. Otherwise %FALSE.
 */
gboolean
ide_buffer_get_read_only (IdeBuffer *self)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUFFER (self), FALSE);

  return priv->read_only;
}

void
_ide_buffer_set_read_only (IdeBuffer *self,
                           gboolean   read_only)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  g_return_if_fail (IDE_IS_BUFFER (self));

  read_only = !!read_only;

  if (read_only != priv->read_only)
    {
      priv->read_only = read_only;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_READ_ONLY]);
    }
}

/**
 * ide_buffer_get_changed_on_volume:
 * @self: an #IdeBuffer.
 *
 * Gets if the file backing the buffer has changed on the underlying storage.
 *
 * Use ide_buffer_manager_load_file_async() to reload the buffer.
 *
 * Returns: %TRUE if the file has changed.
 */
gboolean
ide_buffer_get_changed_on_volume (IdeBuffer *self)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUFFER (self), FALSE);

  return priv->changed_on_volume;
}

void
_ide_buffer_set_changed_on_volume (IdeBuffer *self,
                                   gboolean   changed_on_volume)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_BUFFER (self));

  changed_on_volume = !!changed_on_volume;

  if (changed_on_volume != priv->changed_on_volume)
    {
      priv->changed_on_volume = changed_on_volume;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CHANGED_ON_VOLUME]);
    }

  IDE_EXIT;
}

static void
ide_buffer__check_for_volume_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  g_autoptr(IdeBuffer) self = user_data;
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);
  g_autoptr(GFileInfo) file_info = NULL;
  GFile *file = (GFile *)object;

  g_assert (IDE_IS_BUFFER (self));
  g_assert (G_IS_FILE (file));

  file_info = g_file_query_info_finish (file, result, NULL);

  if (file_info != NULL)
    {
      if (g_file_info_has_attribute (file_info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE))
        {
          gboolean read_only;

          read_only = !g_file_info_get_attribute_boolean (file_info,
                                                          G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE);
          _ide_buffer_set_read_only (self, read_only);
        }

      if (g_file_info_has_attribute (file_info, G_FILE_ATTRIBUTE_TIME_MODIFIED) && priv->mtime_set)
        {
          GTimeVal tv;

          g_file_info_get_modification_time (file_info, &tv);

          if (memcmp (&tv, &priv->mtime, sizeof tv) != 0)
            _ide_buffer_set_changed_on_volume (self, TRUE);
        }
    }
}

/**
 * ide_buffer_check_for_volume_change:
 * @self: an #IdeBuffer.
 *
 * Update the #IdeBuffer:read-only property and the corresponding
 * modification time (mtime).
 *
 */
void
ide_buffer_check_for_volume_change (IdeBuffer *self)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);
  GFile *location;

  g_return_if_fail (IDE_IS_BUFFER (self));

  if (priv->changed_on_volume)
    return;

  location = ide_file_get_file (priv->file);
  if (location == NULL)
    return;

  g_file_query_info_async (location,
                           G_FILE_ATTRIBUTE_TIME_MODIFIED ","
                           G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE,
                           G_FILE_QUERY_INFO_NONE,
                           G_PRIORITY_DEFAULT,
                           NULL, /* Plumb to shutdown cancellable? */
                           ide_buffer__check_for_volume_cb,
                           g_object_ref (self));
}

void
_ide_buffer_set_mtime (IdeBuffer      *self,
                       const GTimeVal *mtime)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_BUFFER (self));

  if (mtime == NULL)
    {
      priv->mtime_set = FALSE;
      priv->mtime.tv_sec = 0;
      priv->mtime.tv_usec = 0;
    }
  else
    {
      priv->mtime = *mtime;
      priv->mtime_set = TRUE;
    }

  IDE_EXIT;
}

/**
 * ide_buffer_get_iter_at_source_location:
 * @self: an #IdeBuffer.
 * @iter: (out): a #GtkTextIter.
 * @location: a #IdeSourceLocation.
 *
 * Fill @iter with the position designated by @location.
 *
 */
void
ide_buffer_get_iter_at_source_location (IdeBuffer         *self,
                                        GtkTextIter       *iter,
                                        IdeSourceLocation *location)
{
  guint line;
  guint line_offset;

  g_return_if_fail (IDE_IS_BUFFER (self));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (location != NULL);

  line = ide_source_location_get_line (location);
  line_offset = ide_source_location_get_line_offset (location);

  gtk_text_buffer_get_iter_at_line_offset (GTK_TEXT_BUFFER (self), iter, line, line_offset);
}

/**
 * ide_buffer_rehighlight:
 * @self: an #IdeBuffer.
 *
 * Force the #IdeBuffer to rebuild the highlight.
 *
 */
void
ide_buffer_rehighlight (IdeBuffer *self)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_BUFFER (self));

  /* In case we are disposing */
  if (priv->highlight_engine == NULL || priv->loading)
    IDE_EXIT;

  if (gtk_source_buffer_get_highlight_syntax (GTK_SOURCE_BUFFER (self)))
    {
      ide_highlight_engine_rebuild (priv->highlight_engine);
      IDE_EXIT;
    }

  ide_highlight_engine_clear (priv->highlight_engine);

  IDE_EXIT;
}

static void
ide_buffer_get_symbol_at_location_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  IdeSymbolResolver *symbol_resolver = (IdeSymbolResolver *)object;
  g_autoptr(IdeSymbol) symbol = NULL;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  LookUpSymbolData *data;

  g_assert (IDE_IS_SYMBOL_RESOLVER (symbol_resolver));
  g_assert (G_IS_TASK (task));

  symbol = ide_symbol_resolver_lookup_symbol_finish (symbol_resolver, result, &error);

  data = g_task_get_task_data (task);

  if (symbol != NULL)
    {
      /*
       * Store symbol which has definition location. If no symbol has definition location
       * then store symbol which has declaration location.
       */
      if ((data->symbol == NULL) ||
          (ide_symbol_get_definition_location (symbol) != NULL) ||
          (ide_symbol_get_definition_location (data->symbol) == NULL &&
           ide_symbol_get_declaration_location (symbol)))
        {
          g_clear_pointer (&data->symbol, ide_symbol_unref);

          data->symbol = g_steal_pointer (&symbol);
        }
    }

  g_ptr_array_remove_index (data->resolvers, data->resolvers->len - 1);

  if (data->resolvers->len)
    {
      IdeSymbolResolver *resolver;
      GCancellable *cancellable;

      resolver = g_ptr_array_index (data->resolvers, data->resolvers->len - 1);
      cancellable = g_task_get_cancellable (task);

      ide_symbol_resolver_lookup_symbol_async (resolver,
                                               data->location,
                                               cancellable,
                                               ide_buffer_get_symbol_at_location_cb,
                                               g_steal_pointer (&task));
    }
  else if (data->symbol == NULL)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "Symbol not found");
    }
  else
    {
      g_task_return_pointer (task,
                             g_steal_pointer (&data->symbol),
                             (GDestroyNotify)ide_symbol_unref);
    }
}

/**
 * ide_buffer_get_symbol_at_location_async:
 * @self: an #IdeBuffer.
 * @location: a #GtkTextIter indicating a position to search for a symbol.
 * @cancellable: a #GCancellable.
 * @callback: a #GAsyncReadyCallback.
 * @user_data: a #gpointer to hold user data.
 *
 * Asynchronously get a possible symbol at @location.
 *
 */
void
ide_buffer_get_symbol_at_location_async (IdeBuffer           *self,
                                         const GtkTextIter   *location,
                                         GCancellable        *cancellable,
                                         GAsyncReadyCallback  callback,
                                         gpointer             user_data)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);
  g_autoptr(IdeSourceLocation) srcloc = NULL;
  g_autoptr(GPtrArray) extensions = NULL;
  g_autoptr(GTask) task = NULL;
  IdeExtensionSetAdapter *adapter;
  IdeSymbolResolver *resolver;
  LookUpSymbolData *data;
  guint line;
  guint line_offset;
  guint offset;
  guint n_extensions;

  g_return_if_fail (IDE_IS_BUFFER (self));
  g_return_if_fail (location != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  adapter = ide_buffer_get_symbol_resolvers (self);
  n_extensions = ide_extension_set_adapter_get_n_extensions (adapter);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_priority (task, G_PRIORITY_LOW);
  g_task_set_source_tag (task, ide_buffer_get_symbol_at_location_async);

  if (n_extensions == 0)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               _("The current language lacks a symbol resolver."));
      return;
    }

  line = gtk_text_iter_get_line (location);
  line_offset = gtk_text_iter_get_line_offset (location);
  offset = gtk_text_iter_get_offset (location);
  srcloc = ide_source_location_new (priv->file, line, line_offset, offset);

  data = g_slice_new0 (LookUpSymbolData);
  data->resolvers = g_ptr_array_new_full (n_extensions, NULL);
  data->location = ide_source_location_ref (srcloc);

  ide_extension_set_adapter_foreach (adapter, lookup_symbol_get_extension, data);

  g_task_set_task_data (task, data, (GDestroyNotify)lookup_symbol_task_data_free);

  resolver = g_ptr_array_index (data->resolvers, data->resolvers->len - 1);

  /* Try lookup_symbol on each symbol resolver one by by one. */
  ide_symbol_resolver_lookup_symbol_async (resolver,
                                           data->location,
                                           cancellable,
                                           ide_buffer_get_symbol_at_location_cb,
                                           g_steal_pointer (&task));
}

/**
 * ide_buffer_get_symbol_at_location_finish:
 * @self: an #IdeBuffer.
 * @result: a #GAsyncResult.
 * @error: (out): a #GError.
 *
 * Completes an asynchronous request to locate a symbol at a location.
 *
 * Returns: (transfer full): An #IdeSymbol or %NULL.
 */
IdeSymbol *
ide_buffer_get_symbol_at_location_finish (IdeBuffer     *self,
                                          GAsyncResult  *result,
                                          GError       **error)
{
  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

/**
 * ide_buffer_get_symbols_finish:
 * @self: an #IdeBuffer.
 *
 * Returns: (transfer container) (element-type IdeSymbol*): a #GPtrArray if successful;
 *   otherwise %NULL.
 */
GPtrArray *
ide_buffer_get_symbols_finish (IdeBuffer     *self,
                               GAsyncResult  *result,
                               GError       **error)
{
  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

static gboolean
ide_buffer_reclaim_timeout (gpointer data)
{
  IdeBuffer *self = data;
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);
  IdeBufferManager *buffer_manager;

  g_assert (IDE_IS_BUFFER (self));

  priv->reclamation_handler = 0;

  g_clear_object (&priv->rename_provider_adapter);
  g_clear_object (&priv->symbol_resolvers_adapter);

  buffer_manager = ide_context_get_buffer_manager (priv->context);

  _ide_buffer_manager_reclaim (buffer_manager, self);

  return G_SOURCE_REMOVE;
}

void
ide_buffer_hold (IdeBuffer *self)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  g_return_if_fail (IDE_IS_BUFFER (self));
  g_return_if_fail (priv->hold_count >= 0);

  priv->hold_count++;

  if (priv->context == NULL)
    return;

  if (priv->reclamation_handler != 0)
    {
      g_source_remove (priv->reclamation_handler);
      priv->reclamation_handler = 0;
    }
}

void
ide_buffer_release (IdeBuffer *self)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_BUFFER (self));
  g_return_if_fail (priv->hold_count >= 0);

  priv->hold_count--;

  if (priv->context == NULL)
    IDE_EXIT;

  /*
   * If our hold count has reached zero, then queue the buffer for
   * reclamation by the buffer manager after a grace period has elapsed.
   * This helps us proactively drop buffers after there are no more views
   * watching them, but deal with the case where we are transitioning to
   * a new split after dropping the current split.
   */

  if ((priv->hold_count == 0) && (priv->reclamation_handler == 0))
    {
      priv->reclamation_handler = g_timeout_add_seconds (RECLAIMATION_TIMEOUT_SECS,
                                                         ide_buffer_reclaim_timeout,
                                                         self);
    }

  IDE_EXIT;
}

/**
 * ide_buffer_get_selection_bounds:
 * @self: an #IdeBuffer.
 * @insert: (out): a #GtkTextIter to get the insert position.
 * @selection: (out): a #GtkTextIter to get the selection position.
 *
 * This function acts like gtk_text_buffer_get_selection_bounds() except that it always
 * places the location of the insert mark at @insert and the location of the selection
 * mark at @selection.
 *
 * Calling gtk_text_iter_order() with the results of this function would be equivalent
 * to calling gtk_text_buffer_get_selection_bounds().
 */
void
ide_buffer_get_selection_bounds (IdeBuffer   *self,
                                 GtkTextIter *insert,
                                 GtkTextIter *selection)
{
  GtkTextMark *mark;

  g_return_if_fail (IDE_IS_BUFFER (self));

  if (insert != NULL)
    {
      mark = gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (self));
      gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (self), insert, mark);
    }

  if (selection != NULL)
    {
      mark = gtk_text_buffer_get_selection_bound (GTK_TEXT_BUFFER (self));
      gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (self), selection, mark);
    }
}

/**
 * ide_buffer_get_rename_provider:
 *
 * Gets the #IdeRenameProvider for this buffer, or %NULL.
 *
 * Returns: (nullable) (transfer none): An #IdeRenameProvider or %NULL if there
 *   is no #IdeRenameProvider that can statisfy the buffer.
 */
IdeRenameProvider *
ide_buffer_get_rename_provider (IdeBuffer *self)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);

  if (priv->rename_provider_adapter != NULL)
    return ide_extension_adapter_get_extension (priv->rename_provider_adapter);

  return NULL;
}

/**
 * ide_buffer_get_symbol_resolvers:
 * @self: an #IdeBuffer.
 *
 * Gets the symbol resolvers for the buffer based on the current language.
 *
 * Returns: (nullable) (transfer none): An #IdeExtensionSetAdapter or %NULL.
 */
IdeExtensionSetAdapter *
ide_buffer_get_symbol_resolvers (IdeBuffer *self)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);

  return priv->symbol_resolvers_adapter;
}

/**
 * ide_buffer_get_word_at_iter:
 * @self: an #IdeBuffer.
 * @iter: a #GtkTextIter.
 *
 * Gets the word found under the position denoted by @iter.
 *
 * Returns: (transfer full): A newly allocated string.
 */
gchar *
ide_buffer_get_word_at_iter (IdeBuffer         *self,
                             const GtkTextIter *iter)
{
  GtkTextIter begin;
  GtkTextIter end;

  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);
  g_return_val_if_fail (iter != NULL, NULL);

  end = begin = *iter;

  if (!_ide_source_iter_starts_word (&begin))
    _ide_source_iter_backward_extra_natural_word_start (&begin);

  if (!_ide_source_iter_ends_word (&end))
    _ide_source_iter_forward_extra_natural_word_end (&end);

  return gtk_text_iter_get_slice (&begin, &end);
}

gsize
ide_buffer_get_change_count (IdeBuffer *self)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUFFER (self), 0);

  return priv->change_count;
}

gchar *
ide_buffer_get_uri (IdeBuffer *self)
{
  IdeFile *file;
  GFile *gfile;

  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);

  file = ide_buffer_get_file (self);
  gfile = ide_file_get_file (file);

  return g_file_get_uri (gfile);
}

/**
 * ide_buffer_get_iter_location:
 *
 * Gets the location of the iter as an #IdeSourceLocation.
 *
 * Returns: (transfer full): An #IdeSourceLocation
 */
IdeSourceLocation *
ide_buffer_get_iter_location (IdeBuffer         *self,
                              const GtkTextIter *iter)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);
  g_return_val_if_fail (iter != NULL, NULL);
  g_return_val_if_fail (gtk_text_iter_get_buffer (iter) == GTK_TEXT_BUFFER (self), NULL);

  return ide_source_location_new (priv->file,
                                  gtk_text_iter_get_line (iter),
                                  gtk_text_iter_get_line_offset (iter),
                                  gtk_text_iter_get_offset (iter));
}

/**
 * ide_buffer_get_insert_location:
 *
 * Gets the location of the insert mark as an #IdeSourceLocation.
 *
 * Returns: (transfer full): An #IdeSourceLocation
 */
IdeSourceLocation *
ide_buffer_get_insert_location (IdeBuffer *self)
{
  GtkTextMark *mark;
  GtkTextIter iter;

  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);

  mark = gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (self));
  gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (self), &iter, mark);

  return ide_buffer_get_iter_location (self, &iter);
}

static void
ide_buffer_format_selection_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  g_autoptr(GError) error = NULL;

  if (!ide_formatter_format_finish (IDE_FORMATTER (object), result, &error))
    g_task_return_error (user_data, g_steal_pointer (&error));
  else
    g_task_return_boolean (user_data, TRUE);
}

static void
ide_buffer_format_selection_range_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  g_autoptr(GError) error = NULL;

  if (!ide_formatter_format_range_finish (IDE_FORMATTER (object), result, &error))
    g_task_return_error (user_data, g_steal_pointer (&error));
  else
    g_task_return_boolean (user_data, TRUE);
}

void
ide_buffer_format_selection_async (IdeBuffer           *self,
                                   IdeFormatterOptions *options,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);
  g_autoptr(GTask) task = NULL;
  IdeFormatter *formatter;
  GtkTextIter begin;
  GtkTextIter end;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_BUFFER (self));
  g_return_if_fail (IDE_IS_FORMATTER_OPTIONS (options));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_buffer_format_selection_async);

  formatter = ide_extension_adapter_get_extension (priv->formatter_adapter);

  if (formatter == NULL)
    {
      GtkSourceLanguage *language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (self));
      const gchar *language_id = "none";

      if (language != NULL)
        language_id = gtk_source_language_get_id (language);

      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               "No formatter registered for language %s",
                               language_id);

      IDE_EXIT;
    }

  if (!gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (self), &begin, &end))
    {
      ide_formatter_format_async (formatter,
                                  self,
                                  options,
                                  cancellable,
                                  ide_buffer_format_selection_cb,
                                  g_steal_pointer (&task));
      IDE_EXIT;
    }

  gtk_text_iter_order (&begin, &end);

  ide_formatter_format_range_async (formatter,
                                    self,
                                    options,
                                    &begin,
                                    &end,
                                    cancellable,
                                    ide_buffer_format_selection_range_cb,
                                    g_steal_pointer (&task));

  IDE_EXIT;
}

gboolean
ide_buffer_format_selection_finish (IdeBuffer     *self,
                                    GAsyncResult  *result,
                                    GError       **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_BUFFER (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  ret = g_task_propagate_boolean (G_TASK (result), error);

  IDE_RETURN (ret);
}

void
_ide_buffer_cancel_cursor_restore (IdeBuffer *self)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);
  g_return_if_fail (IDE_IS_BUFFER (self));
  priv->cancel_cursor_restore = TRUE;
}

gboolean
_ide_buffer_can_restore_cursor (IdeBuffer *self)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);
  g_return_val_if_fail (IDE_IS_BUFFER (self), FALSE);
  return !priv->cancel_cursor_restore;
}

PeasExtensionSet *
_ide_buffer_get_addins (IdeBuffer *self)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);

  return priv->addins;
}

gboolean
ide_buffer_get_failed (IdeBuffer *self)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUFFER (self), FALSE);

  return priv->failed;
}

void
_ide_buffer_set_failure (IdeBuffer    *self,
                         const GError *error)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  g_return_if_fail (IDE_IS_BUFFER (self));

  priv->failed = !!error;
  g_clear_pointer (&priv->failure, g_error_free);
  priv->failure = g_error_copy (error);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FAILED]);
}

const GError *
ide_buffer_get_failure (IdeBuffer *self)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);

  return priv->failure;
}
