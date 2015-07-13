/* ide-buffer.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include <glib/gi18n.h>

#include "egg-counter.h"
#include "egg-signal-group.h"

#include "ide-battery-monitor.h"
#include "ide-buffer.h"
#include "ide-buffer-change-monitor.h"
#include "ide-context.h"
#include "ide-debug.h"
#include "ide-diagnostic.h"
#include "ide-diagnostician.h"
#include "ide-diagnostics.h"
#include "ide-extension-adapter.h"
#include "ide-file.h"
#include "ide-file-settings.h"
#include "ide-highlighter.h"
#include "ide-highlight-engine.h"
#include "ide-internal.h"
#include "ide-source-location.h"
#include "ide-source-range.h"
#include "ide-symbol.h"
#include "ide-symbol-resolver.h"
#include "ide-unsaved-files.h"
#include "ide-vcs.h"

#define DEFAULT_DIAGNOSE_TIMEOUT_MSEC          333
#define DEFAULT_DIAGNOSE_CONSERVE_TIMEOUT_MSEC 5000
#define RECLAIMATION_TIMEOUT_SECS              1
#define MODIFICATION_TIMEOUT_SECS              1

#define TAG_ERROR      "diagnostician::error"
#define TAG_WARNING    "diagnostician::warning"
#define TAG_DEPRECATED "diagnostician::deprecated"
#define TAG_NOTE       "diagnostician::note"

#define TEXT_ITER_IS_SPACE(ptr) g_unichar_isspace(gtk_text_iter_get_char(ptr))

typedef struct
{
  IdeContext             *context;
  IdeDiagnostics         *diagnostics;
  GHashTable             *diagnostics_line_cache;
  IdeFile                *file;
  GBytes                 *content;
  IdeBufferChangeMonitor *change_monitor;
  IdeDiagnostician       *diagnostician;
  IdeHighlightEngine     *highlight_engine;
  IdeExtensionAdapter    *symbol_resolver_adapter;
  gchar                  *title;

  EggSignalGroup         *file_signals;

  GFileMonitor           *file_monitor;

  gulong                  change_monitor_changed_handler;

  guint                   diagnose_timeout;
  guint                   check_modified_timeout;

  GTimeVal                mtime;

  gint                    hold_count;
  guint                   reclamation_handler;

  guint                   changed_on_volume : 1;
  guint                   diagnostics_dirty : 1;
  guint                   highlight_diagnostics : 1;
  guint                   in_diagnose : 1;
  guint                   loading : 1;
  guint                   mtime_set : 1;
  guint                   read_only : 1;
  guint                   has_done_diagnostics_once : 1;
} IdeBufferPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeBuffer, ide_buffer, GTK_SOURCE_TYPE_BUFFER)

EGG_DEFINE_COUNTER (instances, "IdeBuffer", "Instances", "Number of IdeBuffer instances.")

enum {
  PROP_0,
  PROP_BUSY,
  PROP_CHANGED_ON_VOLUME,
  PROP_CONTEXT,
  PROP_FILE,
  PROP_HIGHLIGHT_DIAGNOSTICS,
  PROP_READ_ONLY,
  PROP_STYLE_SCHEME_NAME,
  PROP_TITLE,
  LAST_PROP
};

enum {
  CURSOR_MOVED,
  LINE_FLAGS_CHANGED,
  LOADED,
  SAVED,
  LAST_SIGNAL
};

static void ide_buffer_queue_diagnose (IdeBuffer *self);

static GParamSpec *gParamSpecs [LAST_PROP];
static guint gSignals [LAST_SIGNAL];

static gboolean
ide_buffer_get_busy (IdeBuffer *self)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUFFER (self), FALSE);

  return priv->in_diagnose;
}

static void
ide_buffer_emit_cursor_moved (IdeBuffer *self)
{
  GtkTextMark *mark;
  GtkTextIter iter;

  g_assert (IDE_IS_BUFFER (self));

  mark = gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (self));
  gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (self), &iter, mark);
  g_signal_emit (self, gSignals [CURSOR_MOVED], 0, &iter);
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

  gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (self), iter, line);

  while (line_offset && !gtk_text_iter_ends_line (iter))
    {
      gtk_text_iter_forward_char (iter);
      line_offset--;
    }
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

  g_return_if_fail (IDE_IS_BUFFER (self));
  g_return_if_fail (IDE_IS_CONTEXT (context));
  g_return_if_fail (priv->context == NULL);

  priv->context = context;

  g_object_weak_ref (G_OBJECT (context),
                     ide_buffer_release_context,
                     self);
}

static void
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
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (IDE_IS_BUFFER (self));

  if (priv->diagnostics_line_cache)
    g_hash_table_remove_all (priv->diagnostics_line_cache);

  gtk_text_buffer_get_bounds (buffer, &begin, &end);

  gtk_text_buffer_remove_tag_by_name (buffer, TAG_NOTE, &begin, &end);
  gtk_text_buffer_remove_tag_by_name (buffer, TAG_WARNING, &begin, &end);
  gtk_text_buffer_remove_tag_by_name (buffer, TAG_DEPRECATED, &begin, &end);
  gtk_text_buffer_remove_tag_by_name (buffer, TAG_ERROR, &begin, &end);
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

  if ((location = ide_diagnostic_get_location (diagnostic)))
    {
      IdeFile *file;
      GtkTextIter iter1;
      GtkTextIter iter2;

      file = ide_source_location_get_file (location);

      if (file && priv->file && !ide_file_equal (file, priv->file))
        {
          /* Ignore? */
        }

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
      ide_buffer_update_diagnostic (self, diagnostic);
    }
}

static void
ide_buffer_set_diagnostics (IdeBuffer      *self,
                            IdeDiagnostics *diagnostics)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  g_assert (IDE_IS_BUFFER (self));

  if (diagnostics != priv->diagnostics)
    {
      g_clear_pointer (&priv->diagnostics, ide_diagnostics_unref);
      priv->diagnostics = diagnostics ? ide_diagnostics_ref (diagnostics) : NULL;

      ide_buffer_clear_diagnostics (self);

      if (diagnostics)
        ide_buffer_update_diagnostics (self, diagnostics);

      g_signal_emit (self, gSignals [LINE_FLAGS_CHANGED], 0);
    }
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
ide_buffer__diagnostician_diagnose_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  IdeDiagnostician *diagnostician = (IdeDiagnostician *)object;
  g_autoptr(IdeBuffer) self = user_data;
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);
  g_autoptr(IdeDiagnostics) diagnostics = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_DIAGNOSTICIAN (diagnostician));
  g_assert (IDE_IS_BUFFER (self));

  priv->in_diagnose = FALSE;
  g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_BUSY]);

  diagnostics = ide_diagnostician_diagnose_finish (diagnostician, result, &error);

  if (error)
    g_message ("%s", error->message);

  ide_buffer_set_diagnostics (self, diagnostics);

  if (priv->diagnostics_dirty)
    ide_buffer_queue_diagnose (self);

  if (!priv->has_done_diagnostics_once)
    {
      priv->has_done_diagnostics_once = TRUE;
      ide_highlight_engine_rebuild (priv->highlight_engine);
    }
}

static gboolean
ide_buffer__diagnose_timeout_cb (gpointer user_data)
{
  IdeBuffer *self = user_data;
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  g_assert (IDE_IS_BUFFER (self));

  priv->diagnose_timeout = 0;

  if (priv->file != NULL)
    {
      priv->diagnostics_dirty = FALSE;
      priv->in_diagnose = TRUE;
      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_BUSY]);

      ide_buffer_sync_to_unsaved_files (self);
      ide_diagnostician_diagnose_async (priv->diagnostician,
                                        priv->file,
                                        NULL,
                                        ide_buffer__diagnostician_diagnose_cb,
                                        g_object_ref (self));
    }

  return G_SOURCE_REMOVE;
}

static guint
ide_buffer_get_diagnose_timeout_msec (void)
{
  guint timeout_msec = DEFAULT_DIAGNOSE_TIMEOUT_MSEC;

  if (ide_battery_monitor_get_should_conserve ())
    timeout_msec = DEFAULT_DIAGNOSE_CONSERVE_TIMEOUT_MSEC;

  return timeout_msec;
}

static void
ide_buffer_queue_diagnose (IdeBuffer *self)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);
  guint timeout_msec;

  g_assert (IDE_IS_BUFFER (self));

  priv->diagnostics_dirty = TRUE;

  if (priv->diagnose_timeout != 0)
    {
      g_source_remove (priv->diagnose_timeout);
      priv->diagnose_timeout = 0;
    }

  /*
   * Try to real in how often we parse when on battery.
   */
  timeout_msec = ide_buffer_get_diagnose_timeout_msec ();

  priv->diagnose_timeout = g_timeout_add (timeout_msec, ide_buffer__diagnose_timeout_cb, self);
}

static void
ide_buffer__change_monitor_changed_cb (IdeBuffer              *self,
                                       IdeBufferChangeMonitor *monitor)
{
  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER (self));
  g_assert (IDE_IS_BUFFER_CHANGE_MONITOR (monitor));

  g_signal_emit (self, gSignals [LINE_FLAGS_CHANGED], 0);

  IDE_EXIT;
}

static void
ide_buffer_reload_change_monitor (IdeBuffer *self)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  g_assert (IDE_IS_BUFFER (self));

  if (priv->change_monitor)
    {
      ide_clear_signal_handler (priv->change_monitor, &priv->change_monitor_changed_handler);
      g_clear_object (&priv->change_monitor);
    }

  if (priv->context && priv->file)
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

  if (old_id == NULL || !ide_str_equal0 (old_id, new_id))
    _ide_file_set_content_type (ifile, content_type);
}

static void
ide_buffer_changed (GtkTextBuffer *buffer)
{
  IdeBuffer *self = (IdeBuffer *)buffer;
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  GTK_TEXT_BUFFER_CLASS (ide_buffer_parent_class)->changed (buffer);

  priv->diagnostics_dirty = TRUE;

  g_clear_pointer (&priv->content, g_bytes_unref);

  if (priv->highlight_diagnostics && !priv->in_diagnose)
    ide_buffer_queue_diagnose (self);
}

static void
ide_buffer_delete_range (GtkTextBuffer *buffer,
                         GtkTextIter   *start,
                         GtkTextIter   *end)
{
  GTK_TEXT_BUFFER_CLASS (ide_buffer_parent_class)->delete_range (buffer, start, end);
  ide_buffer_emit_cursor_moved (IDE_BUFFER (buffer));
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
#if GLIB_CHECK_VERSION(2,45,0)
    case G_FILE_MONITOR_EVENT_RENAMED:
#endif
      IDE_TRACE_MSG ("buffer change event = %d", (int)event);
      ide_buffer_queue_modify_check (self);
      break;

    case G_FILE_MONITOR_EVENT_PRE_UNMOUNT:
    case G_FILE_MONITOR_EVENT_UNMOUNTED:
#if GLIB_CHECK_VERSION(2,45,0)
    case G_FILE_MONITOR_EVENT_MOVED_IN:
    case G_FILE_MONITOR_EVENT_MOVED_OUT:
#endif
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

      priv->file_monitor = g_file_monitor_file (gfile, G_FILE_MONITOR_NONE, NULL, NULL);

      if (priv->file_monitor != NULL)
        {
          g_signal_connect_object (priv->file_monitor,
                                   "changed",
                                   G_CALLBACK (ide_buffer__file_monitor_changed),
                                   self,
                                   G_CONNECT_SWAPPED);
        }
      else
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

  /*
   * FIXME: Workaround for 3.16.3
   *        This should be refactored as part of the move to libpeas.
   */

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

  ide_extension_adapter_set_value (priv->symbol_resolver_adapter, lang_id);

  ide_diagnostician_set_language (priv->diagnostician, language);
}

static void
ide_buffer_constructed (GObject *object)
{
  IdeBuffer *self = (IdeBuffer *)object;
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);
  GdkRGBA deprecated_rgba;
  GdkRGBA error_rgba;
  GdkRGBA note_rgba;
  GdkRGBA warning_rgba;

  G_OBJECT_CLASS (ide_buffer_parent_class)->constructed (object);

  /*
   * TODO: Once we bump to GtkSourceView 3.17, these should be extracted
   *       from the style scheme (or use the style scheme directly).
   */
  gdk_rgba_parse (&deprecated_rgba, "#babdb6");
  gdk_rgba_parse (&error_rgba, "#ff0000");
  gdk_rgba_parse (&note_rgba, "#708090");
  gdk_rgba_parse (&warning_rgba, "#fcaf3e");

  /*
   * NOTE:
   *
   * The tag table assigns priority upon insert. Each successive insert
   * is higher priority than the last.
   */
  gtk_text_buffer_create_tag (GTK_TEXT_BUFFER (self), TAG_NOTE,
                              "underline", PANGO_UNDERLINE_ERROR,
                              "underline-rgba", &note_rgba,
                              NULL);
  gtk_text_buffer_create_tag (GTK_TEXT_BUFFER (self), TAG_DEPRECATED,
                              "underline", PANGO_UNDERLINE_ERROR,
                              "underline-rgba", &deprecated_rgba,
                              NULL);
  gtk_text_buffer_create_tag (GTK_TEXT_BUFFER (self), TAG_WARNING,
                              "underline", PANGO_UNDERLINE_ERROR,
                              "underline-rgba", &warning_rgba,
                              NULL);
  gtk_text_buffer_create_tag (GTK_TEXT_BUFFER (self), TAG_ERROR,
                              "underline", PANGO_UNDERLINE_ERROR,
                              "underline-rgba", &error_rgba,
                              NULL);

  priv->highlight_engine = ide_highlight_engine_new (self);

  priv->symbol_resolver_adapter = ide_extension_adapter_new (priv->context,
                                                             NULL,
                                                             IDE_TYPE_SYMBOL_RESOLVER,
                                                             "Symbol-Resolver-Languages",
                                                             NULL);

  priv->diagnostician = g_object_new (IDE_TYPE_DIAGNOSTICIAN,
                                      "context", priv->context,
                                      NULL);

  g_signal_connect (self,
                    "notify::language",
                    G_CALLBACK (ide_buffer_notify_language),
                    NULL);

  g_object_notify (G_OBJECT (self), "language");
}

static void
ide_buffer_dispose (GObject *object)
{
  IdeBuffer *self = (IdeBuffer *)object;
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  IDE_ENTRY;

  if (priv->check_modified_timeout != 0)
    {
      g_source_remove (priv->check_modified_timeout);
      priv->check_modified_timeout = 0;
    }

  if (priv->file_monitor)
    {
      g_file_monitor_cancel (priv->file_monitor);
      g_clear_object (&priv->file_monitor);
    }

  g_clear_object (&priv->file_signals);

  if (priv->highlight_engine != NULL)
    g_object_run_dispose (G_OBJECT (priv->highlight_engine));

  if (priv->diagnose_timeout)
    {
      g_source_remove (priv->diagnose_timeout);
      priv->diagnose_timeout = 0;
    }

  if (priv->change_monitor)
    {
      ide_clear_signal_handler (priv->change_monitor, &priv->change_monitor_changed_handler);
      g_clear_object (&priv->change_monitor);
    }

  g_clear_pointer (&priv->diagnostics_line_cache, g_hash_table_unref);
  g_clear_pointer (&priv->diagnostics, ide_diagnostics_unref);
  g_clear_pointer (&priv->content, g_bytes_unref);
  g_clear_pointer (&priv->title, g_free);
  g_clear_object (&priv->diagnostician);
  g_clear_object (&priv->file);
  g_clear_object (&priv->highlight_engine);
  g_clear_object (&priv->symbol_resolver_adapter);

  if (priv->context != NULL)
    {
      g_object_weak_unref (G_OBJECT (priv->context),
                           ide_buffer_release_context,
                           self);
      priv->context = NULL;
    }

  G_OBJECT_CLASS (ide_buffer_parent_class)->dispose (object);

  IDE_EXIT;
}

static void
ide_buffer_finalize (GObject *object)
{
  IdeBuffer *self = (IdeBuffer *)object;
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  IDE_ENTRY;

  if (priv->reclamation_handler != 0)
    {
      g_source_remove (priv->reclamation_handler);
      priv->reclamation_handler = 0;
    }

  ide_clear_weak_pointer (&priv->context);

  G_OBJECT_CLASS (ide_buffer_parent_class)->finalize (object);

  EGG_COUNTER_DEC (instances);

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

    case PROP_FILE:
      g_value_set_object (value, ide_buffer_get_file (self));
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

  gParamSpecs [PROP_BUSY] =
    g_param_spec_boolean ("busy",
                         _("Busy"),
                         _("If the buffer is performing background work."),
                         FALSE,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  gParamSpecs [PROP_CHANGED_ON_VOLUME] =
    g_param_spec_boolean ("changed-on-volume",
                         _("Changed on Volume"),
                         _("If the file has changed on disk and the buffer is not in sync."),
                         FALSE,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  gParamSpecs [PROP_CONTEXT] =
    g_param_spec_object ("context",
                         _("Context"),
                         _("The IdeContext for the buffer."),
                         IDE_TYPE_CONTEXT,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  gParamSpecs [PROP_FILE] =
    g_param_spec_object ("file",
                         _("File"),
                         _("The file represented by the buffer."),
                         IDE_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gParamSpecs [PROP_HIGHLIGHT_DIAGNOSTICS] =
    g_param_spec_boolean ("highlight-diagnostics",
                          _("Highlight Diagnostics"),
                          _("If diagnostic warnings and errors should be highlighted."),
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gParamSpecs [PROP_READ_ONLY] =
    g_param_spec_boolean ("read-only",
                          _("Read Only"),
                          _("If the underlying file is read only."),
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  gParamSpecs [PROP_STYLE_SCHEME_NAME] =
    g_param_spec_string ("style-scheme-name",
                         _("Style Scheme Name"),
                         _("Style Scheme Name"),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gParamSpecs [PROP_TITLE] =
    g_param_spec_string ("title",
                         _("Title"),
                         _("The title of the buffer."),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);

  /**
   * IdeBuffer::cursor-moved:
   * @self: An #IdeBuffer.
   * @location: A #GtkTextIter.
   *
   * This signal is emitted when the insertion location has moved. You might
   * want to attach to this signal to update the location of the insert mark in
   * the display.
   */
  gSignals [CURSOR_MOVED] =
    g_signal_new ("cursor-moved",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeBufferClass, cursor_moved),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  GTK_TYPE_TEXT_ITER);

  /**
   * IdeBuffer::line-flags-changed:
   *
   * This signal is emitted when the calculated line flags have changed. This occurs when
   * diagnostics and line changes have been recalculated.
   */
  gSignals [LINE_FLAGS_CHANGED] =
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
  gSignals [LOADED] =
    g_signal_new ("loaded",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);

  /**
   * IdeBuffer::saved:
   *
   * This signal is emitted when the buffer manager has completed saving the file.
   */
  gSignals [SAVED] =
    g_signal_new ("saved",
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

  priv->file_signals = egg_signal_group_new (IDE_TYPE_FILE);
  egg_signal_group_connect_object (priv->file_signals,
                                   "notify::language",
                                   G_CALLBACK (ide_buffer__file_notify_language),
                                   self,
                                   G_CONNECT_SWAPPED);
  egg_signal_group_connect_object (priv->file_signals,
                                   "notify::file",
                                   G_CALLBACK (ide_buffer__file_notify_file),
                                   self,
                                   G_CONNECT_SWAPPED);

  priv->diagnostics_line_cache = g_hash_table_new (g_direct_hash, g_direct_equal);

  EGG_COUNTER_INC (instances);

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
  g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_TITLE]);
}

/**
 * ide_buffer_get_file:
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
 *
 * Sets the underlying file to use when saving and loading @self to and and from storage.
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
      egg_signal_group_set_target (priv->file_signals, file);
      ide_file_load_settings_async (priv->file,
                                    NULL,
                                    ide_buffer__file_load_settings_cb,
                                    g_object_ref (self));
      ide_buffer_reload_change_monitor (self);
      /*
       * FIXME: More hack for 3.16.3. This all needs refactorying.
       *        In particular, IdeFile should probably subclass GtkSourceFile.
       */
      if (file != NULL)
        ide_buffer__file_notify_file (self, NULL, file);
      ide_buffer_update_title (self);
      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_FILE]);
      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_TITLE]);
    }
}

/**
 * ide_buffer_get_context:
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
      GtkTextIter iter;

      gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (self), &iter, line);
      change = ide_buffer_change_monitor_get_change (priv->change_monitor, &iter);

      switch (change)
        {
        case IDE_BUFFER_LINE_CHANGE_ADDED:
          flags |= IDE_BUFFER_LINE_FLAGS_ADDED;
          break;

        case IDE_BUFFER_LINE_CHANGE_CHANGED:
          flags |= IDE_BUFFER_LINE_FLAGS_CHANGED;
          break;

        case IDE_BUFFER_LINE_CHANGE_DELETED:
        case IDE_BUFFER_LINE_CHANGE_NONE:
        default:
          break;
        }
    }

  return flags;
}

gboolean
ide_buffer_get_highlight_diagnostics (IdeBuffer *self)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUFFER (self), FALSE);

  return priv->highlight_diagnostics;
}

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
      if (!highlight_diagnostics)
        ide_buffer_clear_diagnostics (self);
      else
        ide_buffer_queue_diagnose (self);
      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_HIGHLIGHT_DIAGNOSTICS]);
    }
}

/**
 * ide_buffer_get_diagnostic_at_iter:
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

          ide_buffer_get_iter_at_location (self, &pos, location);

          if (line == gtk_text_iter_get_line (&pos))
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

/**
 * ide_buffer_get_content:
 *
 * Gets the contents of the buffer as GBytes.
 *
 * By using this function to get the bytes, you allow #IdeBuffer to avoid calculating the buffer
 * text unnecessarily, potentially saving on allocations.
 *
 * Additionally, this allows the buffer to update the state in #IdeUnsavedFiles if the content
 * is out of sync.
 *
 * Returns: (transfer full): A #GBytes containing the buffer content.
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
        text [len++] = '\n';

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

void
ide_buffer_trim_trailing_whitespace  (IdeBuffer *self)
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
        {
          GtkTextIter tmp;

          gtk_text_buffer_get_iter_at_line (buffer, &tmp, line);
          change = ide_buffer_change_monitor_get_change (priv->change_monitor, &tmp);
        }

      if (change != IDE_BUFFER_LINE_CHANGE_NONE)
        {
          gtk_text_buffer_get_iter_at_line (buffer, &iter, line);

          if (gtk_text_iter_forward_to_line_end (&iter) && TEXT_ITER_IS_SPACE (&iter))
            {
              GtkTextIter begin = iter;

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
        }
    }
}

/**
 * ide_buffer_get_title:
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

  g_return_if_fail (IDE_IS_BUFFER (self));

  loading = !!loading;

  if (priv->loading != loading)
    {
      priv->loading = loading;

      /*
       * TODO: We probably want some sort of state rather than this boolean value.
       *       But that can come later after we get plumbing hooked up.
       */

      if (!priv->loading)
        {
          GtkSourceLanguage *language;
          GtkSourceLanguage *current;

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
          g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_FILE]);

          g_signal_emit (self, gSignals [LOADED], 0);
        }
    }
}

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
      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_READ_ONLY]);
    }
}

/**
 * ide_buffer_get_changed_on_volume:
 * @self: A #IdeBuffer.
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
      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_CHANGED_ON_VOLUME]);
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

  gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (self), iter, line);
  for (; line_offset; line_offset--)
    if (gtk_text_iter_ends_line (iter) || !gtk_text_iter_forward_char (iter))
      break;
}

void
ide_buffer_rehighlight (IdeBuffer *self)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_BUFFER (self));

  if (priv->highlight_engine != NULL)
    ide_highlight_engine_rebuild (priv->highlight_engine);

  IDE_EXIT;
}

static void
ide_buffer__symbol_provider_lookup_symbol_cb (GObject      *object,
                                              GAsyncResult *result,
                                              gpointer      user_data)
{
  IdeSymbolResolver *symbol_resolver = (IdeSymbolResolver *)object;
  g_autoptr(IdeSymbol) symbol = NULL;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;

  g_assert (G_IS_TASK (task));

  symbol = ide_symbol_resolver_lookup_symbol_finish (symbol_resolver, result, &error);

  if (symbol == NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  g_task_return_pointer (task, ide_symbol_ref (symbol), (GDestroyNotify)ide_symbol_unref);
}

void
ide_buffer_get_symbol_at_location_async (IdeBuffer           *self,
                                         const GtkTextIter   *location,
                                         GCancellable        *cancellable,
                                         GAsyncReadyCallback  callback,
                                         gpointer             user_data)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);
  IdeSymbolResolver *symbol_resolver;
  g_autoptr(GTask) task = NULL;
  g_autoptr(IdeSourceLocation) srcloc = NULL;
  guint line;
  guint line_offset;
  guint offset;

  g_return_if_fail (IDE_IS_BUFFER (self));
  g_return_if_fail (location != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  symbol_resolver = ide_buffer_get_symbol_resolver (self);

  if (symbol_resolver == NULL)
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

  ide_symbol_resolver_lookup_symbol_async (symbol_resolver,
                                           srcloc,
                                           cancellable,
                                           ide_buffer__symbol_provider_lookup_symbol_cb,
                                           g_object_ref (task));
}

/**
 * ide_buffer_get_symbol_at_location_finish:
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
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);
  g_return_val_if_fail (G_IS_TASK (task), NULL);

  return g_task_propagate_pointer (task, error);
}

static void
ide_buffer__symbol_provider_get_symbols_cb (GObject      *object,
                                            GAsyncResult *result,
                                            gpointer      user_data)
{
  IdeSymbolResolver *symbol_resolver = (IdeSymbolResolver *)object;
  g_autoptr(GPtrArray) symbols = NULL;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;

  g_assert (G_IS_TASK (task));

  symbols = ide_symbol_resolver_get_symbols_finish (symbol_resolver, result, &error);

  if (symbols == NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  g_task_return_pointer (task, g_ptr_array_ref (symbols), (GDestroyNotify)g_ptr_array_unref);
}

void
ide_buffer_get_symbols_async (IdeBuffer           *self,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);
  g_autoptr(GTask) task = NULL;
  IdeSymbolResolver *symbol_resolver;

  g_return_if_fail (IDE_IS_BUFFER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  symbol_resolver = ide_buffer_get_symbol_resolver (self);

  if (symbol_resolver == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               _("Symbol resolver is not supported."));
      return;
    }

  ide_symbol_resolver_get_symbols_async (symbol_resolver,
                                         priv->file,
                                         cancellable,
                                         ide_buffer__symbol_provider_get_symbols_cb,
                                         g_object_ref (task));
}

/**
 * ide_buffer_get_symbols_finish:
 *
 * Returns: (transfer container) (element-type IdeSymbol*): A #GPtrArray if successful;
 *   otherwise %NULL.
 */
GPtrArray *
ide_buffer_get_symbols_finish (IdeBuffer     *self,
                               GAsyncResult  *result,
                               GError       **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);
  g_return_val_if_fail (G_IS_TASK (task), NULL);

  return g_task_propagate_pointer (task, error);
}

static gboolean
ide_buffer_reclaim_timeout (gpointer data)
{
  IdeBuffer *self = data;
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);
  IdeBufferManager *buffer_manager;

  g_assert (IDE_IS_BUFFER (self));

  priv->reclamation_handler = 0;

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

  g_return_if_fail (IDE_IS_BUFFER (self));
  g_return_if_fail (priv->hold_count >= 0);

  priv->hold_count--;

  if (priv->context == NULL)
    return;

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
}

/**
 * ide_buffer_get_selection_bounds:
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
 * ide_buffer_get_symbol_resolver:
 *
 * Gets the symbol resolver for the buffer based on the current language.
 *
 * Returns: (nullable) (transfer none): An #IdeSymbolResolver or %NULL.
 */
IdeSymbolResolver *
ide_buffer_get_symbol_resolver (IdeBuffer *self)
{
  IdeBufferPrivate *priv = ide_buffer_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);

  if (priv->symbol_resolver_adapter != NULL)
    return ide_extension_adapter_get_extension (priv->symbol_resolver_adapter);

  return NULL;
}
