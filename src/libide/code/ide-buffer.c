/* ide-buffer.c
 *
 * Copyright 2018-2022 Christian Hergert <chergert@redhat.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "ide-buffer"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-io.h>
#include <libide-plugins.h>
#include <libide-threading.h>

#include "ide-marshal.h"

#include "ide-buffer.h"
#include "ide-buffer-addin.h"
#include "ide-buffer-addin-private.h"
#include "ide-buffer-manager.h"
#include "ide-buffer-private.h"
#include "ide-code-action-provider.h"
#include "ide-code-enums.h"
#include "ide-diagnostic.h"
#include "ide-diagnostics.h"
#include "ide-file-settings.h"
#include "ide-formatter.h"
#include "ide-formatter-options.h"
#include "ide-gfile-private.h"
#include "ide-highlight-engine.h"
#include "ide-location.h"
#include "ide-range.h"
#include "ide-source-iter.h"
#include "ide-source-style-scheme.h"
#include "ide-symbol-resolver.h"
#include "ide-unsaved-files.h"

#define SETTLING_DELAY_MSEC  333

#define TAG_ERROR            "-Builder:error"
#define TAG_WARNING          "-Builder:warning"
#define TAG_DEPRECATED       "-Builder:deprecated"
#define TAG_UNUSED           "-Builder:unused"
#define TAG_NOTE             "-Builder:note"
#define TAG_SNIPPET_TAB_STOP "-Builder:tab-stop"
#define TAG_DEFINITION       "-Builder:hover-definition"
#define TAG_CURRENT_BKPT     "-Builder:current-breakpoint"

#define DEPRECATED_COLOR     "#babdb6"
#define UNUSED_COLOR         "#c17d11"
#define ERROR_COLOR          "#ff0000"
#define NOTE_COLOR           "#708090"
#define WARNING_COLOR        "#fcaf3e"
#define CURRENT_BKPT_FG      "#fffffe"
#define CURRENT_BKPT_BG      "#fcaf3e"

struct _IdeBuffer
{
  GtkSourceBuffer         parent_instance;

  const GtkSourceEncoding *encoding;

  /* Owned references */
  IdeExtensionSetAdapter *addins;
  IdeExtensionSetAdapter *symbol_resolvers;
  IdeExtensionAdapter    *rename_provider;
  IdeExtensionAdapter    *formatter;
  IdeExtensionAdapter    *code_action_provider;
  IdeBufferManager       *buffer_manager;
  IdeBufferChangeMonitor *change_monitor;
  GBytes                 *content;
  IdeDiagnostics         *diagnostics;
  GError                 *failure;
  IdeFileSettings        *file_settings;
  IdeHighlightEngine     *highlight_engine;
  GtkSourceFile          *source_file;
  GFile                  *readlink_file;

  GSignalGroup           *file_settings_signals;

  IdeTask                *in_flight_symbol_at_location;
  int                     in_flight_symbol_at_location_pos;

  guint                   change_count;
  guint                   settling_source;
  int                     hold;
  guint                   release_in_idle;
  guint                   diagnostics_stamp;

  GArray                 *commit_funcs;
  guint                   next_commit_handler;

  /* Bit-fields */
  IdeBufferState          state : 3;
  guint                   can_restore_cursor : 1;
  guint                   is_temporary : 1;
  guint                   enable_addins : 1;
  guint                   changed_on_volume : 1;
  guint                   read_only : 1;
  guint                   has_encoding_error : 1;
  guint                   highlight_diagnostics : 1;
  GtkSourceNewlineType    newline_type : 2;
};

typedef struct
{
  IdeNotification *notif;
  GFile           *file;
  guint            highlight_syntax : 1;
} LoadState;

typedef struct
{
  GFile           *file;
  IdeNotification *notif;
  GtkSourceFile   *source_file;
} SaveState;

typedef struct
{
  GPtrArray   *resolvers;
  IdeLocation *location;
  IdeSymbol   *symbol;
} LookUpSymbolData;

G_DEFINE_FINAL_TYPE (IdeBuffer, ide_buffer, GTK_SOURCE_TYPE_BUFFER)

enum {
  PROP_0,
  PROP_BUFFER_MANAGER,
  PROP_CHANGE_MONITOR,
  PROP_CHANGED_ON_VOLUME,
  PROP_CHARSET,
  PROP_ENABLE_ADDINS,
  PROP_DIAGNOSTICS,
  PROP_FAILED,
  PROP_FILE,
  PROP_FILE_SETTINGS,
  PROP_FORMATTER,
  PROP_HAS_DIAGNOSTICS,
  PROP_HAS_ENCODING_ERROR,
  PROP_HAS_SYMBOL_RESOLVERS,
  PROP_HIGHLIGHT_DIAGNOSTICS,
  PROP_IS_TEMPORARY,
  PROP_LANGUAGE_ID,
  PROP_NEWLINE_TYPE,
  PROP_READ_ONLY,
  PROP_STATE,
  PROP_SYMBOL_RESOLVERS,
  PROP_STYLE_SCHEME_NAME,
  PROP_TITLE,
  N_PROPS
};

enum {
  CHANGE_SETTLED,
  CURSOR_MOVED,
  LINE_FLAGS_CHANGED,
  LOADED,
  REQUEST_SCROLL_TO_INSERT,
  N_SIGNALS
};

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static void     lookup_symbol_data_free            (LookUpSymbolData       *data);
static void     apply_style                        (GtkTextTag             *tag,
                                                    const gchar            *first_property,
                                                    ...);
static void     load_state_free                    (LoadState              *state);
static void     save_state_free                    (SaveState              *state);
static void     ide_buffer_save_file_cb            (GObject                *object,
                                                    GAsyncResult           *result,
                                                    gpointer                user_data);
static void     ide_buffer_load_file_cb            (GObject                *object,
                                                    GAsyncResult           *result,
                                                    gpointer                user_data);
static void     ide_buffer_progress_cb             (goffset                 current_num_bytes,
                                                    goffset                 total_num_bytes,
                                                    gpointer                user_data);
static void     ide_buffer_get_property            (GObject                *object,
                                                    guint                   prop_id,
                                                    GValue                 *value,
                                                    GParamSpec             *pspec);
static void     ide_buffer_set_property            (GObject                *object,
                                                    guint                   prop_id,
                                                    const GValue           *value,
                                                    GParamSpec             *pspec);
static void     ide_buffer_constructed             (GObject                *object);
static void     ide_buffer_dispose                 (GObject                *object);
static void     ide_buffer_notify_language         (IdeBuffer              *self,
                                                    GParamSpec             *pspec,
                                                    gpointer                user_data);
static void     ide_buffer_notify_style_scheme     (IdeBuffer              *self,
                                                    GParamSpec             *pspec,
                                                    gpointer                unused);
static void     ide_buffer_reload_file_settings    (IdeBuffer              *self);
static void     ide_buffer_set_file_settings       (IdeBuffer              *self,
                                                    IdeFileSettings        *file_settings);
static void     ide_buffer_changed                 (GtkTextBuffer          *buffer);
static void     ide_buffer_delete_range            (GtkTextBuffer          *buffer,
                                                    GtkTextIter            *start,
                                                    GtkTextIter            *end);
static void     ide_buffer_insert_text             (GtkTextBuffer          *buffer,
                                                    GtkTextIter            *location,
                                                    const gchar            *text,
                                                    gint                    len);
static void     ide_buffer_delay_settling          (IdeBuffer              *self);
static gboolean ide_buffer_settled_cb              (gpointer                user_data);
static void     ide_buffer_apply_diagnostics       (IdeBuffer              *self);
static void     ide_buffer_clear_diagnostics       (IdeBuffer              *self);
static void     ide_buffer_apply_diagnostic        (IdeBuffer              *self,
                                                    IdeDiagnostic          *diagnostics);
static void     ide_buffer_init_tags               (IdeBuffer              *self);
static void     ide_buffer_on_tag_added            (IdeBuffer              *self,
                                                    GtkTextTag             *tag,
                                                    GtkTextTagTable        *table);
static void     ide_buffer_get_symbol_resolvers_cb (IdeExtensionSetAdapter *set,
                                                    PeasPluginInfo         *plugin_info,
                                                    GObject          *extension,
                                                    gpointer                user_data);
static void     ide_buffer_symbol_resolver_removed (IdeExtensionSetAdapter *adapter,
                                                    PeasPluginInfo         *plugin_info,
                                                    GObject          *extension,
                                                    gpointer                user_data);
static void     ide_buffer_symbol_resolver_added   (IdeExtensionSetAdapter *adapter,
                                                    PeasPluginInfo         *plugin_info,
                                                    GObject          *extension,
                                                    gpointer                user_data);
static gboolean ide_buffer_can_do_newline_hack     (IdeBuffer              *self,
                                                    guint                   len);
static void     ide_buffer_guess_language          (IdeBuffer              *self);
static void     ide_buffer_real_loaded             (IdeBuffer              *self);
static void     _ide_buffer_set_has_encoding_error (IdeBuffer              *self,
                                                    gboolean                has_encoding_error);
static void     settle_async                       (IdeBuffer              *self,
                                                    GCancellable           *cancellable,
                                                    GAsyncReadyCallback     callback,
                                                    gpointer                user_data);
static gboolean settle_finish                      (IdeBuffer              *self,
                                                    GAsyncResult           *result,
                                                    GError                **error);

static void
load_state_free (LoadState *state)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (state != NULL);

  g_clear_object (&state->notif);
  g_clear_object (&state->file);
  g_slice_free (LoadState, state);
}

static void
save_state_free (SaveState *state)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (state != NULL);

  g_clear_object (&state->notif);
  g_clear_object (&state->file);
  g_clear_object (&state->source_file);
  g_slice_free (SaveState, state);
}

static void
lookup_symbol_data_free (LookUpSymbolData *data)
{
  g_assert (IDE_IS_MAIN_THREAD ());

  g_clear_pointer (&data->resolvers, g_ptr_array_unref);
  g_clear_object (&data->location);
  g_clear_object (&data->symbol);
  g_slice_free (LookUpSymbolData, data);
}

IdeBuffer *
_ide_buffer_new (IdeBufferManager *buffer_manager,
                 GFile            *file,
                 gboolean          enable_addins,
                 gboolean          is_temporary)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_BUFFER_MANAGER (buffer_manager), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  return g_object_new (IDE_TYPE_BUFFER,
                       "buffer-manager", buffer_manager,
                       "file", file,
                       "enable-addins", enable_addins,
                       "is-temporary", is_temporary,
                       "implicit-trailing-newline", FALSE,
                       "style-scheme-name", "Adwaita",
                       NULL);
}

void
_ide_buffer_set_file (IdeBuffer *self,
                      GFile     *file)
{
  GFile *location;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_BUFFER (self));
  g_return_if_fail (G_IS_FILE (file));

  location = gtk_source_file_get_location (self->source_file);

  if (location == NULL || !g_file_equal (file, location))
    {
      g_autoptr(GFile) copy = g_file_dup (file);

      gtk_source_file_set_location (self->source_file, copy);

      g_clear_object (&self->readlink_file);
      self->readlink_file = _ide_g_file_readlink (copy);

      ide_buffer_reload_file_settings (self);

      if (self->addins != NULL && self->enable_addins)
        {
          IdeBufferFileLoad closure = { self, copy };
          ide_extension_set_adapter_foreach (self->addins,
                                             _ide_buffer_addin_file_loaded_cb,
                                             &closure);
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FILE]);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TITLE]);
    }
}

static void
ide_buffer_set_state (IdeBuffer      *self,
                      IdeBufferState  state)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_BUFFER (self));
  g_return_if_fail (state == IDE_BUFFER_STATE_READY ||
                    state == IDE_BUFFER_STATE_LOADING ||
                    state == IDE_BUFFER_STATE_SAVING ||
                    state == IDE_BUFFER_STATE_FAILED);

  if (self->state != state)
    {
      self->state = state;
      if (self->state != IDE_BUFFER_STATE_FAILED)
        g_clear_pointer (&self->failure, g_error_free);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_STATE]);
    }
}

static void
ide_buffer_update_implicit_newline (IdeBuffer *self)
{
  IdeFileSettings *file_settings;
  gboolean was_implicit;
  gboolean now_implicit;

  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER (self));
  g_assert (!ide_buffer_get_loading (self));

  if (!(file_settings = ide_buffer_get_file_settings (self)))
    IDE_EXIT;

  /* If the new file-settings object does not match our
   * current setting for trailing newlines, then we need
   * to adjust that immediately to ensure our buffer contains
   * the contents as expected.
   *
   * Otherwise, buffer change monitors will not line up with
   * the the expectation on storage.
   */
  was_implicit = gtk_source_buffer_get_implicit_trailing_newline (GTK_SOURCE_BUFFER (self));
  now_implicit = ide_file_settings_get_insert_trailing_newline (file_settings);

  if (was_implicit != now_implicit)
    {
      GtkTextIter iter;

      gtk_source_buffer_set_implicit_trailing_newline (GTK_SOURCE_BUFFER (self), now_implicit);

      if (now_implicit)
        {
          gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (self), &iter);

          if (gtk_text_iter_starts_line (&iter))
            {
              GtkTextIter begin = iter;

              gtk_text_iter_backward_char (&begin);

              if (!gtk_text_iter_equal (&begin, &iter))
                {
                  gboolean modified = gtk_text_buffer_get_modified (GTK_TEXT_BUFFER (self));

                  gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (self));
                  gtk_text_buffer_delete (GTK_TEXT_BUFFER (self), &begin, &iter);
                  gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (self));

                  if (!modified)
                    gtk_text_buffer_set_modified (GTK_TEXT_BUFFER (self), modified);
                }
            }
        }
      else
        {
          gboolean modified = gtk_text_buffer_get_modified (GTK_TEXT_BUFFER (self));

          gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (self), &iter);

          gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (self));
          gtk_text_buffer_insert (GTK_TEXT_BUFFER (self), &iter, "\n", 1);
          gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (self));

          if (!modified)
            gtk_text_buffer_set_modified (GTK_TEXT_BUFFER (self), modified);
        }
    }

  IDE_EXIT;
}

static void
ide_buffer_real_loaded (IdeBuffer *self)
{
  g_assert (IDE_IS_BUFFER (self));

  ide_buffer_update_implicit_newline (self);

  if (self->buffer_manager != NULL)
    _ide_buffer_manager_buffer_loaded (self->buffer_manager, self);
}

static void
ide_buffer_notify_language (IdeBuffer  *self,
                            GParamSpec *pspec,
                            gpointer    user_data)
{
  const gchar *lang_id;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER (self));

  ide_buffer_reload_file_settings (self);

  lang_id = ide_buffer_get_language_id (self);

  if (self->addins != NULL && self->enable_addins)
    {
      IdeBufferLanguageSet state = { self, lang_id };

      ide_extension_set_adapter_set_value (self->addins, state.language_id);
      ide_extension_set_adapter_foreach (self->addins,
                                         _ide_buffer_addin_language_set_cb,
                                         &state);
    }

  if (self->symbol_resolvers)
    ide_extension_set_adapter_set_value (self->symbol_resolvers, lang_id);

  if (self->rename_provider)
    ide_extension_adapter_set_value (self->rename_provider, lang_id);

  if (self->formatter)
    ide_extension_adapter_set_value (self->formatter, lang_id);

  if (self->code_action_provider)
    ide_extension_adapter_set_value (self->code_action_provider, lang_id);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LANGUAGE_ID]);
}

static void
on_settings_changed_newline_type_cb (IdeBuffer       *self,
                                     GParamSpec      *pspec,
                                     IdeFileSettings *file_settings)
{
  GtkSourceNewlineType newline_type;

  g_assert (IDE_IS_BUFFER (self));
  g_assert (IDE_IS_FILE_SETTINGS (file_settings));

  newline_type = ide_file_settings_get_newline_type (file_settings);

  ide_buffer_set_newline_type (self, newline_type);
}

static void
ide_buffer_constructed (GObject *object)
{
  IdeBuffer *self = (IdeBuffer *)object;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER (self));

  G_OBJECT_CLASS (ide_buffer_parent_class)->constructed (object);

  ide_buffer_init_tags (self);
}

static void
ide_buffer_dispose (GObject *object)
{
  IdeBuffer *self = (IdeBuffer *)object;
  IdeObjectBox *box;

  g_assert (IDE_IS_MAIN_THREAD ());

  if (self->source_file != NULL)
    {
      GFile *file = gtk_source_file_get_location (self->source_file);

      if (file != NULL)
        {
          g_autofree char *uri = g_file_get_uri (file);
          g_debug ("Releasing IdeBuffer for URI \"%s\"", uri);
        }
    }

  g_clear_handle_id (&self->settling_source, g_source_remove);
  g_clear_handle_id (&self->release_in_idle, g_source_remove);

  ide_clear_and_destroy_object (&self->addins);

  ide_clear_and_destroy_object (&self->rename_provider);
  ide_clear_and_destroy_object (&self->symbol_resolvers);
  ide_clear_and_destroy_object (&self->formatter);
  ide_clear_and_destroy_object (&self->code_action_provider);
  ide_clear_and_destroy_object (&self->highlight_engine);
  ide_clear_and_destroy_object (&self->change_monitor);
  ide_clear_and_destroy_object (&self->file_settings);

  g_signal_group_set_target (self->file_settings_signals, NULL);

  g_clear_object (&self->diagnostics);
  g_clear_object (&self->buffer_manager);

  /* Remove ourselves from the object-tree if necessary */
  if ((box = ide_object_box_from_object (object)) &&
      !ide_object_in_destruction (IDE_OBJECT (box)))
    ide_object_destroy (IDE_OBJECT (box));

  g_clear_pointer (&self->content, g_bytes_unref);

  G_OBJECT_CLASS (ide_buffer_parent_class)->dispose (object);
}

static void
ide_buffer_finalize (GObject *object)
{
  IdeBuffer *self = (IdeBuffer *)object;

  g_clear_object (&self->file_settings_signals);
  g_clear_object (&self->source_file);
  g_clear_object (&self->readlink_file);
  g_clear_pointer (&self->failure, g_error_free);

  G_OBJECT_CLASS (ide_buffer_parent_class)->finalize (object);
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
    case PROP_CHANGE_MONITOR:
      g_value_set_object (value, ide_buffer_get_change_monitor (self));
      break;

    case PROP_CHANGED_ON_VOLUME:
      g_value_set_boolean (value, ide_buffer_get_changed_on_volume (self));
      break;

    case PROP_CHARSET:
      g_value_set_string (value, ide_buffer_get_charset (self));
      break;

    case PROP_ENABLE_ADDINS:
      g_value_set_boolean (value, self->enable_addins);
      break;

    case PROP_DIAGNOSTICS:
      g_value_set_object (value, ide_buffer_get_diagnostics (self));
      break;

    case PROP_FAILED:
      g_value_set_boolean (value, ide_buffer_get_failed (self));
      break;

    case PROP_FILE:
      g_value_set_object (value, ide_buffer_get_file (self));
      break;

    case PROP_FILE_SETTINGS:
      g_value_set_object (value, ide_buffer_get_file_settings (self));
      break;

    case PROP_FORMATTER:
      g_value_set_object (value, ide_buffer_get_formatter (self));
      break;

    case PROP_HAS_DIAGNOSTICS:
      g_value_set_boolean (value, ide_buffer_has_diagnostics (self));
      break;

    case PROP_HAS_ENCODING_ERROR:
      g_value_set_boolean (value, ide_buffer_has_encoding_error (self));
      break;

    case PROP_HAS_SYMBOL_RESOLVERS:
      g_value_set_boolean (value, ide_buffer_has_symbol_resolvers (self));
      break;

    case PROP_HIGHLIGHT_DIAGNOSTICS:
      g_value_set_boolean (value, ide_buffer_get_highlight_diagnostics (self));
      break;

    case PROP_LANGUAGE_ID:
      g_value_set_string (value, ide_buffer_get_language_id (self));
      break;

    case PROP_NEWLINE_TYPE:
      g_value_set_enum (value, ide_buffer_get_newline_type (self));
      break;

    case PROP_IS_TEMPORARY:
      g_value_set_boolean (value, ide_buffer_get_is_temporary (self));
      break;

    case PROP_READ_ONLY:
      g_value_set_boolean (value, ide_buffer_get_read_only (self));
      break;

    case PROP_STATE:
      g_value_set_enum (value, ide_buffer_get_state (self));
      break;

    case PROP_SYMBOL_RESOLVERS:
      g_value_take_object (value, ide_buffer_list_symbol_resolvers (self));
      break;

    case PROP_STYLE_SCHEME_NAME:
      g_value_set_string (value, ide_buffer_get_style_scheme_name (self));
      break;

    case PROP_TITLE:
      g_value_take_string (value, ide_buffer_dup_title (self));
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
    case PROP_BUFFER_MANAGER:
      self->buffer_manager = g_value_dup_object (value);
      break;

    case PROP_CHANGE_MONITOR:
      ide_buffer_set_change_monitor (self, g_value_get_object (value));
      break;

    case PROP_CHARSET:
      ide_buffer_set_charset (self, g_value_get_string (value));
      break;

    case PROP_ENABLE_ADDINS:
      self->enable_addins = g_value_get_boolean (value);
      break;

    case PROP_DIAGNOSTICS:
      ide_buffer_set_diagnostics (self, g_value_get_object (value));
      break;

    case PROP_FILE:
      _ide_buffer_set_file (self, g_value_get_object (value));
      break;

    case PROP_HIGHLIGHT_DIAGNOSTICS:
      ide_buffer_set_highlight_diagnostics (self, g_value_get_boolean (value));
      break;

    case PROP_LANGUAGE_ID:
      ide_buffer_set_language_id (self, g_value_get_string (value));
      break;

    case PROP_NEWLINE_TYPE:
      ide_buffer_set_newline_type (self, g_value_get_enum (value));
      break;

    case PROP_IS_TEMPORARY:
      self->is_temporary = g_value_get_boolean (value);
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
  GtkTextBufferClass *buffer_class = GTK_TEXT_BUFFER_CLASS (klass);

  object_class->constructed = ide_buffer_constructed;
  object_class->dispose = ide_buffer_dispose;
  object_class->finalize = ide_buffer_finalize;
  object_class->get_property = ide_buffer_get_property;
  object_class->set_property = ide_buffer_set_property;

  buffer_class->changed = ide_buffer_changed;
  buffer_class->delete_range = ide_buffer_delete_range;
  buffer_class->insert_text = ide_buffer_insert_text;

  /**
   * IdeBuffer:buffer-manager:
   *
   * Sets the "buffer-manager" property, which is used by the buffer to
   * clean-up state when the buffer is no longer in use.
   */
  properties [PROP_BUFFER_MANAGER] =
    g_param_spec_object ("buffer-manager",
                         "Buffer Manager",
                         "The buffer manager for the context.",
                         IDE_TYPE_BUFFER_MANAGER,
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeBuffer:change-monitor:
   *
   * The "change-monitor" property is an #IdeBufferChangeMonitor that will be
   * used to track changes in the #IdeBuffer. This can be used to show line
   * changes in the editor gutter.
   */
  properties [PROP_CHANGE_MONITOR] =
    g_param_spec_object ("change-monitor",
                         "Change Monitor",
                         "Change Monitor",
                         IDE_TYPE_BUFFER_CHANGE_MONITOR,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeBuffer:charset:
   *
   * Sets the encoding to use for the buffer based on the "charset"
   * specified. This is useful to ensure that characters may not be
   * lost from the original encoding.
   */
  properties [PROP_CHARSET] =
    g_param_spec_string ("charset",
                         "Character Set",
                         "The encoding character set",
                         "UTF-8",
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeBuffer:changed-on-volume:
   *
   * The "changed-on-volume" property is set to %TRUE when it has been
   * discovered that the file represented by the #IdeBuffer has changed
   * externally to Builder.
   */
  properties [PROP_CHANGED_ON_VOLUME] =
    g_param_spec_boolean ("changed-on-volume",
                          "Changed On Volume",
                          "If the buffer has been modified externally",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * IdeBuffer:enable-addins:
   *
   * The "enable-addins" property determines whether addins will be aware of
   * this buffer. When set to %FALSE no ide_buffer_addin_*() functions will be
   * called on this buffer.
   */
  properties [PROP_ENABLE_ADDINS] =
    g_param_spec_boolean ("enable-addins",
                          "Enable Addins",
                          "Whether to enable addins for this buffer",
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY));

  /**
   * IdeBuffer:diagnostics:
   *
   * The "diagnostics" property contains an #IdeDiagnostics that represent
   * the diagnostics found in the buffer.
   */
  properties [PROP_DIAGNOSTICS] =
    g_param_spec_object ("diagnostics",
                         "Diagnostics",
                         "The diagnostics for the buffer",
                         IDE_TYPE_DIAGNOSTICS,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeBuffer:failed:
   *
   * The "failed" property is %TRUE when the buffer has entered a failed
   * state such as when loading or saving the buffer to disk.
   */
  properties [PROP_FAILED] =
    g_param_spec_boolean ("failed",
                          "Failed",
                          "If the buffer has entered a failed state",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * IdeBuffer:file:
   *
   * The "file" property is the underlying file represented by the buffer.
   */
  properties [PROP_FILE] =
    g_param_spec_object ("file",
                         "File",
                         "The file the buffer represents",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * IdeBuffer:file-settings:
   *
   * The "file-settings" property are the settings to be used by the buffer
   * and source-view for the underlying file.
   *
   * These are automatically discovered and kept up to date based on the
   * #IdeFileSettings extension points.
   */
  properties [PROP_FILE_SETTINGS] =
    g_param_spec_object ("file-settings",
                         "File Settings",
                         "The file settings for the buffer",
                         IDE_TYPE_FILE_SETTINGS,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_FORMATTER] =
    g_param_spec_object ("formatter", NULL, NULL,
                         IDE_TYPE_FORMATTER,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * IdeBuffer:has-diagnostics:
   *
   * The "has-diagnostics" property denotes that there are a non-zero number
   * of diangostics registered for the buffer.
   */
  properties [PROP_HAS_DIAGNOSTICS] =
    g_param_spec_boolean ("has-diagnostics",
                          "Has Diagnostics",
                          "The diagnostics for the buffer",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_HAS_ENCODING_ERROR] =
    g_param_spec_boolean ("has-encoding-error", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * IdeBuffer:has-symbol-resolvers:
   *
   * The "has-symbol-resolvers" property is %TRUE if there are any symbol
   * resolvers loaded.
   */
  properties [PROP_HAS_SYMBOL_RESOLVERS] =
    g_param_spec_boolean ("has-symbol-resolvers",
                          "Has symbol resolvers",
                          "If there is at least one symbol resolver available",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * IdeBuffer:highlight-diagnostics:
   *
   * The "highlight-diagnostics" property indicates that diagnostics which
   * are discovered should be styled.
   */
  properties [PROP_HIGHLIGHT_DIAGNOSTICS] =
    g_param_spec_boolean ("highlight-diagnostics",
                          "Highlight Diagnostics",
                          "If diagnostics should be highlighted",
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeBuffer:is-temporary:
   *
   * The "is-temporary" property denotes the #IdeBuffer:file property points
   * to a temporary file. When saving the the buffer, various UI components
   * know to check this property and provide a file chooser to allow the user
   * to select the destination file.
   *
   * Upon saving the file, the property will change to %FALSE.
   */
  properties [PROP_IS_TEMPORARY] =
    g_param_spec_boolean ("is-temporary",
                          "Is Temporary",
                          "If the file property is a temporary file",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeBuffer:language-id:
   *
   * The "language-id" property is a convenience property to set the
   * #GtkSourceBuffer:language property using a string name.
   */
  properties [PROP_LANGUAGE_ID] =
    g_param_spec_string ("language-id",
                         "Language Id",
                         "The language identifier as a string",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeBuffer:newline-type:
   *
   * Sets the style of newline to append to each line.
   *
   * This sets the style of newlines to use within the file. Generally,
   * only LF should be used (\n) as it is common on Unix, Linux, and most
   * editors at this point.
   *
   * Older Windows editors might prefer CR+LF (\r\n) which is similar to what
   * is displayed in a raw terminal without a \n line discipline.
   *
   * Really old Mac Classic systems use just a \r.
   */
  properties [PROP_NEWLINE_TYPE] =
    g_param_spec_enum ("newline-type",
                       "Newline Type",
                       "The style of newlines to append at the end of each line",
                       GTK_SOURCE_TYPE_NEWLINE_TYPE,
                       GTK_SOURCE_NEWLINE_TYPE_DEFAULT,
                       (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeBuffer:read-only:
   *
   * The "read-only" property is set to %TRUE when it has been
   * discovered that the file represented by the #IdeBuffer is read-only
   * on the underlying storage.
   */
  properties [PROP_READ_ONLY] =
    g_param_spec_boolean ("read-only",
                          "Read Only",
                          "If the buffer's file is read-only",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * IdeBuffer:state:
   *
   * The "state" property can be used to determine if the buffer is
   * currently performing any specific background work, such as loading
   * from or saving a buffer to storage.
   */
  properties [PROP_STATE] =
    g_param_spec_enum ("state",
                       "State",
                       "The state for the buffer",
                       IDE_TYPE_BUFFER_STATE,
                       IDE_BUFFER_STATE_READY,
                       (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SYMBOL_RESOLVERS] =
    g_param_spec_object ("symbol-resolvers", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * IdeBuffer:style-scheme-name:
   *
   * The "style-scheme-name" is the name of the style scheme that is used.
   * It is a convenience property so that you do not need to use the
   * #GtkSourceStyleSchemeManager to lookup style schemes.
   */
  properties [PROP_STYLE_SCHEME_NAME] =
    g_param_spec_string ("style-scheme-name",
                         "Style Scheme Name",
                         "The name of the GtkSourceStyleScheme to use",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeBuffer:title:
   *
   * The "title" for the buffer which includes some variant of the path
   * to the underlying file.
   */
  properties [PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "The title for the buffer",
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * IdeBuffer::change-settled:
   * @self: an #IdeBuffer
   *
   * The "change-settled" signal is emitted when the buffer has stopped
   * being edited for a short period of time. This is useful to connect
   * to when you want to perform work as the user is editing, but you
   * don't want to get in the way of their editing.
   */
  signals [CHANGE_SETTLED] =
    g_signal_new ("change-settled",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  ide_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  g_signal_set_va_marshaller (signals [CHANGE_SETTLED],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__VOIDv);

  /**
   * IdeBuffer::line-flags-changed:
   * @self: an #IdeBuffer
   *
   * The "line-flags-changed" signal is emitted when the buffer has detected
   * ancillary information has changed for lines in the buffer. Such information
   * might include diagnostics or version control information.
   */
  signals [LINE_FLAGS_CHANGED] =
    g_signal_new_class_handler ("line-flags-changed",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                NULL,
                                NULL, NULL,
                                ide_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);
  g_signal_set_va_marshaller (signals [LINE_FLAGS_CHANGED],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__VOIDv);

  /**
   * IdeBuffer::loaded:
   * @self: an #IdeBuffer
   *
   * The "loaded" signal is emitted after the buffer is loaded.
   *
   * This is useful to watch if you want to perform a given action but do
   * not want to interfere with buffer loading.
   */
  signals [LOADED] =
    g_signal_new_class_handler ("loaded",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (ide_buffer_real_loaded),
                                NULL, NULL,
                                ide_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);
  g_signal_set_va_marshaller (signals [LOADED],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__VOIDv);

  /**
   * IdeBuffer::request-scroll-to-insert:
   *
   * Requests that attached views scroll to insert location.
   *
   * This is generally only used when loading a buffer.
   */
  signals [REQUEST_SCROLL_TO_INSERT] =
    g_signal_new_class_handler ("request-scroll-to-insert",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                NULL,
                                NULL, NULL,
                                ide_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);
  g_signal_set_va_marshaller (signals [REQUEST_SCROLL_TO_INSERT],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__VOIDv);
}

static void
ide_buffer_init (IdeBuffer *self)
{
  g_assert (IDE_IS_MAIN_THREAD ());

  self->in_flight_symbol_at_location_pos = -1;
  self->source_file = gtk_source_file_new ();
  self->can_restore_cursor = TRUE;
  self->highlight_diagnostics = TRUE;
  self->enable_addins = TRUE;
  self->newline_type = GTK_SOURCE_NEWLINE_TYPE_DEFAULT;

  self->file_settings_signals = g_signal_group_new (IDE_TYPE_FILE_SETTINGS);

  g_signal_group_connect_object (self->file_settings_signals,
                                 "notify::newline-type",
                                 G_CALLBACK (on_settings_changed_newline_type_cb),
                                 self,
                                 G_CONNECT_SWAPPED);

  g_signal_connect (self,
                    "notify::language",
                    G_CALLBACK (ide_buffer_notify_language),
                    NULL);

  g_signal_connect (self,
                    "notify::style-scheme",
                    G_CALLBACK (ide_buffer_notify_style_scheme),
                    NULL);
}

static void
ide_buffer_rename_provider_notify_extension (IdeBuffer           *self,
                                             GParamSpec          *pspec,
                                             IdeExtensionAdapter *adapter)
{
  IdeRenameProvider *provider;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER (self));
  g_assert (IDE_IS_EXTENSION_ADAPTER (adapter));

  if ((provider = ide_extension_adapter_get_extension (adapter)))
    {
      g_object_set (provider, "buffer", self, NULL);
      ide_rename_provider_load (provider);
    }
}

static void
ide_buffer_formatter_notify_extension (IdeBuffer           *self,
                                       GParamSpec          *pspec,
                                       IdeExtensionAdapter *adapter)
{
  IdeFormatter *formatter;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER (self));
  g_assert (IDE_IS_EXTENSION_ADAPTER (adapter));

  if ((formatter = ide_extension_adapter_get_extension (adapter)))
    ide_formatter_load (formatter);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FORMATTER]);
}

static void
ide_buffer_code_action_provider_notify_extension (IdeBuffer           *self,
                                                  GParamSpec          *pspec,
                                                  IdeExtensionAdapter *adapter)
{
  IdeCodeActionProvider *code_action_provider;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER (self));
  g_assert (IDE_IS_EXTENSION_ADAPTER (adapter));

  if ((code_action_provider = ide_extension_adapter_get_extension (adapter)))
    ide_code_action_provider_load (code_action_provider);
}

static void
ide_buffer_symbol_resolver_added (IdeExtensionSetAdapter *adapter,
                                  PeasPluginInfo         *plugin_info,
                                  GObject          *extension,
                                  gpointer                user_data)
{
  IdeSymbolResolver *resolver = (IdeSymbolResolver *)extension;
  IdeBuffer *self = user_data;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (adapter));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_SYMBOL_RESOLVER (resolver));
  g_assert (IDE_IS_BUFFER (self));

  IDE_TRACE_MSG ("Loading symbol resolver %s", G_OBJECT_TYPE_NAME (resolver));

  ide_symbol_resolver_load (resolver);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HAS_SYMBOL_RESOLVERS]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SYMBOL_RESOLVERS]);

  IDE_EXIT;
}

static void
ide_buffer_symbol_resolver_removed (IdeExtensionSetAdapter *adapter,
                                    PeasPluginInfo         *plugin_info,
                                    GObject          *extension,
                                    gpointer                user_data)
{
  IdeSymbolResolver *resolver = (IdeSymbolResolver *)extension;
  IdeBuffer *self = user_data;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (adapter));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_SYMBOL_RESOLVER (resolver));
  g_assert (IDE_IS_BUFFER (self));

  IDE_TRACE_MSG ("Unloading symbol resolver %s", G_OBJECT_TYPE_NAME (resolver));

  ide_symbol_resolver_unload (resolver);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HAS_SYMBOL_RESOLVERS]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SYMBOL_RESOLVERS]);

  IDE_EXIT;
}

void
_ide_buffer_attach (IdeBuffer *self,
                    IdeObject *parent)
{
  const char *language_id;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_OBJECT_BOX (parent));
  g_return_if_fail (ide_object_box_contains (IDE_OBJECT_BOX (parent), self));
  g_return_if_fail (IDE_IS_BUFFER (self));
  g_return_if_fail (self->addins == NULL);
  g_return_if_fail (self->highlight_engine == NULL);
  g_return_if_fail (self->formatter == NULL);
  g_return_if_fail (self->rename_provider == NULL);

  language_id = ide_buffer_get_language_id (self);

  /* Setup the semantic highlight engine */
  self->highlight_engine = ide_highlight_engine_new (self);

  /* Load buffer addins */
  self->addins = ide_extension_set_adapter_new (parent,
                                                peas_engine_get_default (),
                                                IDE_TYPE_BUFFER_ADDIN,
                                                "Buffer-Addin-Languages",
                                                language_id);
  g_signal_connect (self->addins,
                    "extension-added",
                    G_CALLBACK (_ide_buffer_addin_load_cb),
                    self);
  g_signal_connect (self->addins,
                    "extension-removed",
                    G_CALLBACK (_ide_buffer_addin_unload_cb),
                    self);
  ide_extension_set_adapter_foreach (self->addins,
                                     _ide_buffer_addin_load_cb,
                                     self);

  /* Setup our rename provider, if any */
  self->rename_provider = ide_extension_adapter_new (parent,
                                                     peas_engine_get_default (),
                                                     IDE_TYPE_RENAME_PROVIDER,
                                                     "Rename-Provider-Languages",
                                                     language_id);
  g_signal_connect_object (self->rename_provider,
                           "notify::extension",
                           G_CALLBACK (ide_buffer_rename_provider_notify_extension),
                           self,
                           G_CONNECT_SWAPPED);
  ide_buffer_rename_provider_notify_extension (self, NULL, self->rename_provider);

  /* Setup our formatter, if any */
  self->formatter = ide_extension_adapter_new (parent,
                                               peas_engine_get_default (),
                                               IDE_TYPE_FORMATTER,
                                               "Formatter-Languages",
                                               language_id);
  g_signal_connect_object (self->formatter,
                           "notify::extension",
                           G_CALLBACK (ide_buffer_formatter_notify_extension),
                           self,
                           G_CONNECT_SWAPPED);
  ide_buffer_formatter_notify_extension (self, NULL, self->formatter);

  /* Setup our code action provider, if any */
  self->code_action_provider = ide_extension_adapter_new (parent,
                                                          peas_engine_get_default (),
                                                          IDE_TYPE_CODE_ACTION_PROVIDER,
                                                          "Code-Action-Languages",
                                                          language_id);

  g_signal_connect_object (self->code_action_provider,
                           "notify::extension",
                           G_CALLBACK (ide_buffer_code_action_provider_notify_extension),
                           self,
                           G_CONNECT_SWAPPED);
  ide_buffer_code_action_provider_notify_extension (self, NULL, self->code_action_provider);

  /* Setup symbol resolvers */
  self->symbol_resolvers = ide_extension_set_adapter_new (parent,
                                                          peas_engine_get_default (),
                                                          IDE_TYPE_SYMBOL_RESOLVER,
                                                          "Symbol-Resolver-Languages",
                                                          language_id);
  g_signal_connect_object (self->symbol_resolvers,
                           "extension-added",
                           G_CALLBACK (ide_buffer_symbol_resolver_added),
                           self,
                           0);
  g_signal_connect_object (self->symbol_resolvers,
                           "extension-removed",
                           G_CALLBACK (ide_buffer_symbol_resolver_removed),
                           self,
                           0);
  ide_extension_set_adapter_foreach (self->symbol_resolvers,
                                     ide_buffer_symbol_resolver_added,
                                     self);
}

/**
 * ide_buffer_get_file:
 * @self: an #IdeBuffer
 *
 * Gets the #IdeBuffer:file property.
 *
 * Returns: (transfer none): a #GFile
 */
GFile *
ide_buffer_get_file (IdeBuffer *self)
{
  GFile *ret;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);

  ret = gtk_source_file_get_location (self->source_file);

  g_return_val_if_fail (G_IS_FILE (ret), NULL);

  return ret;
}

/**
 * ide_buffer_dup_uri:
 * @self: a #IdeBuffer
 *
 * Gets the URI for the underlying file and returns a copy of it.
 *
 * Returns: (transfer full): a new string
 */
gchar *
ide_buffer_dup_uri (IdeBuffer *self)
{
  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);

  return g_file_get_uri (ide_buffer_get_file (self));
}

/**
 * ide_buffer_get_is_temporary:
 *
 * Checks if the buffer represents a temporary file.
 *
 * This is useful to check by views that want to provide a save-as dialog
 * when the user requests to save the buffer.
 *
 * Returns: %TRUE if the buffer is for a temporary file
 */
gboolean
ide_buffer_get_is_temporary (IdeBuffer *self)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (IDE_IS_BUFFER (self), FALSE);

  return self->is_temporary;
}

/**
 * ide_buffer_get_state:
 * @self: an #IdeBuffer
 *
 * Gets the #IdeBuffer:state property.
 *
 * This will changed while files are loaded or saved to disk.
 *
 * Returns: an #IdeBufferState
 */
IdeBufferState
ide_buffer_get_state (IdeBuffer *self)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), 0);
  g_return_val_if_fail (IDE_IS_BUFFER (self), 0);

  return self->state;
}

static void
ide_buffer_progress_cb (goffset  current_num_bytes,
                        goffset  total_num_bytes,
                        gpointer user_data)
{
  IdeNotification *notif = user_data;
  gdouble progress = 0.0;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_NOTIFICATION (notif));

  if (total_num_bytes)
    progress = (gdouble)current_num_bytes / (gdouble)total_num_bytes;

  ide_notification_set_progress (notif, progress);
}

static gboolean
should_ignore_load_error (IdeBuffer    *self,
                          const GError *error)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER (self));
  g_assert (error != NULL);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
    return TRUE;

  if (g_error_matches (error, GTK_SOURCE_FILE_LOADER_ERROR, GTK_SOURCE_FILE_LOADER_ERROR_CONVERSION_FALLBACK) ||
      g_error_matches (error, GTK_SOURCE_FILE_LOADER_ERROR, GTK_SOURCE_FILE_LOADER_ERROR_ENCODING_AUTO_DETECTION_FAILED))
    {
      _ide_buffer_set_has_encoding_error (self, TRUE);
      return TRUE;
    }

  return FALSE;
}

static void
ide_buffer_load_file_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  GtkSourceFileLoader *loader = (GtkSourceFileLoader *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  GtkTextIter iter;
  LoadState *state;
  IdeBuffer *self;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GTK_SOURCE_IS_FILE_LOADER (loader));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  state = ide_task_get_task_data (task);

  g_assert (IDE_IS_BUFFER (self));
  g_assert (state != NULL);
  g_assert (G_IS_FILE (state->file));
  g_assert (IDE_IS_NOTIFICATION (state->notif));

  if (!gtk_source_file_loader_load_finish (loader, result, &error))
    {
      if (!should_ignore_load_error (self, error))
        {
          g_debug ("Failure loading file: %s", error->message);
          ide_buffer_set_state (self, IDE_BUFFER_STATE_FAILED);
          ide_notification_set_progress (state->notif, 0.0);
          ide_task_return_error (task, g_steal_pointer (&error));
          IDE_EXIT;
        }

      g_clear_error (&error);
    }

  /* First move the insert cursor back to 0:0, plugins might move it
   * but we certainly don't want to leave it at the end.
   */
  gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (self), &iter);
  gtk_text_buffer_select_range (GTK_TEXT_BUFFER (self), &iter, &iter);

  /* Assume we are at newest state at end of file-load operation */
  gtk_text_buffer_set_modified (GTK_TEXT_BUFFER (self), FALSE);
  _ide_buffer_set_changed_on_volume (self, FALSE);

  ide_highlight_engine_unpause (self->highlight_engine);
  ide_buffer_set_state (self, IDE_BUFFER_STATE_READY);
  ide_notification_set_progress (state->notif, 1.0);
  ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

void
_ide_buffer_load_file_async (IdeBuffer            *self,
                             IdeNotification      *notif,
                             GCancellable         *cancellable,
                             GAsyncReadyCallback   callback,
                             gpointer              user_data)
{
  g_autoptr(GtkSourceFileLoader) loader = NULL;
  g_autoptr(IdeTask) task = NULL;
  LoadState *state;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_BUFFER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (ide_buffer_get_file (self) != NULL);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, _ide_buffer_load_file_async);

  if (self->state != IDE_BUFFER_STATE_READY &&
      self->state != IDE_BUFFER_STATE_FAILED)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_BUSY,
                                 "Cannot load file while buffer is busy");
      IDE_EXIT;
    }

  state = g_slice_new0 (LoadState);
  state->file = g_object_ref (ide_buffer_get_file (self));
  state->notif = notif ? g_object_ref (notif) : ide_notification_new ();
  state->highlight_syntax = gtk_source_buffer_get_highlight_syntax (GTK_SOURCE_BUFFER (self));
  ide_task_set_task_data (task, state, load_state_free);

  ide_buffer_set_state (self, IDE_BUFFER_STATE_LOADING);
  _ide_buffer_set_has_encoding_error (self, FALSE);

  /* Disable some features while we reload */
  gtk_source_buffer_set_highlight_syntax (GTK_SOURCE_BUFFER (self), FALSE);
  ide_highlight_engine_pause (self->highlight_engine);

  /* Create our loader, inheriting some values like encoding from the
   * source_file if previously set.
   */
  loader = gtk_source_file_loader_new (GTK_SOURCE_BUFFER (self), self->source_file);

  /* If the user specified an encoding, make sure that we apply
   * that when reloading the file.
   */
  if (self->encoding != NULL)
    {
      g_autofree char *uri = g_file_get_uri (state->file);
      GSList list = { (gpointer)self->encoding, NULL };

      gtk_source_file_loader_set_candidate_encodings (loader, &list);
      g_debug ("Using user-selected encoding of %s while loading '%s'",
               gtk_source_encoding_get_charset (self->encoding), uri);
    }

  gtk_source_file_loader_load_async (loader,
                                     G_PRIORITY_DEFAULT,
                                     cancellable,
                                     ide_buffer_progress_cb,
                                     g_object_ref (state->notif),
                                     g_object_unref,
                                     ide_buffer_load_file_cb,
                                     g_steal_pointer (&task));

  /* Load file settings immediately so that we can increase the chance
   * they are settled by the the load operation is finished. The modelines
   * file settings will auto-monitor for IdeBufferManager::buffer-loaded
   * and settle the file settings when we complete.
   */
  ide_buffer_reload_file_settings (self);

  IDE_EXIT;
}

/**
 * _ide_buffer_load_file_finish:
 * @self: an #IdeBuffer
 * @result: a #GAsyncResult
 * @error: a location for a #GError, or %NULL
 *
 * This should be called by the buffer manager to complete loading the initial
 * state of a buffer. It can also be used to reload a buffer after it was
 * modified on disk.
 *
 * You MUST call this function after using _ide_buffer_load_file_async() so
 * that the completion of signals and addins may be notified.
 *
 * Returns: %TRUE if the file was successfully loaded
 */
gboolean
_ide_buffer_load_file_finish (IdeBuffer     *self,
                              GAsyncResult  *result,
                              GError       **error)
{
  LoadState *state;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (IDE_IS_BUFFER (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  if (!ide_task_propagate_boolean (IDE_TASK (result), error))
    return FALSE;

  /* Restore various buffer features we disabled while loading */
  state = ide_task_get_task_data (IDE_TASK (result));
  if (state->highlight_syntax)
    gtk_source_buffer_set_highlight_syntax (GTK_SOURCE_BUFFER (self), TRUE);

  /* Guess the syntax language now if necessary */
  if (!gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (self)))
    ide_buffer_guess_language (self);

  /* Let consumers know they can access the buffer now */
  g_signal_emit (self, signals [LOADED], 0);

  /* Notify buffer addins that a file has been loaded */
  if (self->addins != NULL && self->enable_addins)
    {
      IdeBufferFileLoad closure = { self, state->file };
      ide_extension_set_adapter_foreach (self->addins,
                                         _ide_buffer_addin_file_loaded_cb,
                                         &closure);
    }

  return TRUE;
}

static void
ide_buffer_save_file_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  GtkSourceFileSaver *saver = (GtkSourceFileSaver *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeBuffer *self;
  SaveState *state;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GTK_SOURCE_IS_FILE_SAVER (saver));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  state = ide_task_get_task_data (task);

  g_assert (IDE_IS_BUFFER (self));
  g_assert (state != NULL);
  g_assert (G_IS_FILE (state->file));
  g_assert (IDE_IS_NOTIFICATION (state->notif));

  if (!gtk_source_file_saver_save_finish (saver, result, &error))
    {
      ide_notification_set_progress (state->notif, 0.0);
      ide_buffer_set_state (self, IDE_BUFFER_STATE_FAILED);
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  ide_notification_set_progress (state->notif, 1.0);
  ide_buffer_set_state (self, IDE_BUFFER_STATE_READY);

  /* Treat our save as freshest. It's possible we race, as we'd need an etag to
   * detect that, probably fine in all but the most slowest of races.
   */
  _ide_buffer_set_changed_on_volume (self, FALSE);

  /* Notify addins that a save has completed */
  if (self->addins != NULL && self->enable_addins)
    {
      IdeBufferFileSave closure = { self, state->file };
      ide_extension_set_adapter_foreach (self->addins,
                                         _ide_buffer_addin_file_saved_cb,
                                         &closure);
    }

  if (self->buffer_manager != NULL)
    _ide_buffer_manager_buffer_saved (self->buffer_manager, self);
  else
    g_critical ("Attempt to save buffer without access to buffer-manager");

  ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_buffer_save_file_settle_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  IdeBuffer *self = (IdeBuffer *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GtkSourceFileSaver) saver = NULL;
  SaveState *state;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  settle_finish (self, result, NULL);

  state = ide_task_get_task_data (task);

  g_assert (state != NULL);
  g_assert (G_IS_FILE (state->file));
  g_assert (IDE_IS_NOTIFICATION (state->notif));
  g_assert (GTK_SOURCE_IS_FILE (state->source_file));

  if (self->addins != NULL && self->enable_addins)
    {
      IdeBufferFileSave closure = { self, state->file };
      ide_extension_set_adapter_foreach (self->addins,
                                         _ide_buffer_addin_save_file_cb,
                                         &closure);
    }

  saver = gtk_source_file_saver_new (GTK_SOURCE_BUFFER (self), state->source_file);

  /* At this point, we've notified the user of changes to the underlying file using
   * the infobar, so just save the file knowing that we are overwriting things.
   */
  gtk_source_file_saver_set_flags (saver,
                                   (GTK_SOURCE_FILE_SAVER_FLAGS_IGNORE_INVALID_CHARS |
                                    GTK_SOURCE_FILE_SAVER_FLAGS_IGNORE_MODIFICATION_TIME));

  /* Propagate the requested encoding, if necessary */
  if (self->encoding != NULL)
    gtk_source_file_saver_set_encoding (saver, self->encoding);

  /* Same for newlines */
  gtk_source_file_saver_set_newline_type (saver, self->newline_type);

  gtk_source_file_saver_save_async (saver,
                                    G_PRIORITY_DEFAULT,
                                    ide_task_get_cancellable (task),
                                    ide_buffer_progress_cb,
                                    g_object_ref (state->notif),
                                    g_object_unref,
                                    ide_buffer_save_file_cb,
                                    g_object_ref (task));

}

/**
 * ide_buffer_save_file_async:
 * @self: an #IdeBuffer
 * @file: (nullable): a #GFile or %NULL
 * @cancellable: (nullable): a #GCancellable
 * @notif: (out) (optional) (transfer full): a location for an #IdeNotification or %NULL
 * @callback: a #GAsyncReadyCallback to execute upon completion
 * @user_data: closure data for @callback
 *
 * Asynchronously saves the buffer contents to @file.
 *
 * If @file is %NULL, then the #IdeBuffer:file property is used.
 *
 * The buffer is marked as busy during the operation, and must not have
 * further editing until the operation is complete.
 *
 * @callback is executed upon completion and should call
 * ide_buffer_save_file_finish() to get the result of the operation.
 */
void
ide_buffer_save_file_async (IdeBuffer            *self,
                            GFile                *file,
                            GCancellable         *cancellable,
                            IdeNotification     **notif,
                            GAsyncReadyCallback   callback,
                            gpointer              user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GtkSourceFile) alternate = NULL;
  g_autoptr(IdeNotification) local_notif = NULL;
  GtkSourceFile *source_file;
  SaveState *state;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_BUFFER (self));
  g_return_if_fail (!file || G_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  ide_clear_param (notif, NULL);

  /* If the user is requesting to save a file and our current file
   * is a temporary file, then we want to transition to become that
   * file instead of our temporary one.
   */
  if (file != NULL && self->is_temporary)
    {
      _ide_buffer_set_file (self, file);
      self->is_temporary = FALSE;

      /* The buffer might be empty, so mark it as modified so we really save */
      gtk_text_buffer_set_modified (GTK_TEXT_BUFFER (self), TRUE);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_IS_TEMPORARY]);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TITLE]);
    }

  if (file == NULL)
    file = ide_buffer_get_file (self);

  local_notif = ide_notification_new ();
  ide_notification_set_has_progress (local_notif, TRUE);

  state = g_slice_new0 (SaveState);
  state->file = g_object_ref (file);
  state->notif = g_object_ref (local_notif);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_buffer_save_file_async);
  ide_task_set_task_data (task, state, save_state_free);

  /* Keep buffer alive during save operation */
  ide_buffer_hold (self);
  g_signal_connect_object (task,
                           "notify::completed",
                           G_CALLBACK (ide_buffer_release),
                           self,
                           G_CONNECT_SWAPPED);

  if (self->state == IDE_BUFFER_STATE_SAVING)
    {
      /* TODO: We could save in-flight tasks and chain to them */
      ide_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  if (self->state != IDE_BUFFER_STATE_READY)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_BUSY,
                                 "Failed to save buffer as it is busy");
      IDE_EXIT;
    }

  source_file = self->source_file;

  if (file && !g_file_equal (file, ide_buffer_get_file (self)))
    {
      alternate = gtk_source_file_new ();
      gtk_source_file_set_location (alternate, file);
      source_file = alternate;
    }

  state->source_file = g_object_ref (source_file);

  /* Possibly avoid any writing if we can detect a no-change state */
  if (file == NULL || g_file_equal (file, ide_buffer_get_file (self)))
    {
      if (!self->changed_on_volume &&
          !gtk_text_buffer_get_modified (GTK_TEXT_BUFFER (self)))
        {
          ide_notification_set_progress (local_notif, 1.0);
          ide_task_return_boolean (task, TRUE);
          IDE_GOTO (set_out_param);
        }
    }

  ide_buffer_set_state (self, IDE_BUFFER_STATE_SAVING);

  /* We're attempting to write our contents now, clear error */
  _ide_buffer_set_has_encoding_error (self, FALSE);

  settle_async (self,
                cancellable,
                ide_buffer_save_file_settle_cb,
                g_steal_pointer (&task));

set_out_param:
  if (notif != NULL)
    *notif = g_steal_pointer (&local_notif);

  IDE_EXIT;
}

/**
 * ide_buffer_save_file_finish:
 * @self: an #IdeBuffer
 * @result: a #GAsyncResult provided to callback
 * @error: a location for a #GError, or %NULL
 *
 * Completes an asynchronous request to save the buffer via
 * ide_buffer_save_file_async().
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 */
gboolean
ide_buffer_save_file_finish (IdeBuffer     *self,
                             GAsyncResult  *result,
                             GError       **error)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (IDE_IS_BUFFER (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

/**
 * ide_buffer_get_language_id:
 * @self: an #IdeBuffer
 *
 * A helper to get the language identifier of the buffers current language.
 *
 * Returns: (nullable): a string containing the language id, or %NULL
 */
const gchar *
ide_buffer_get_language_id (IdeBuffer *self)
{
  GtkSourceLanguage *lang;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);

  if ((lang = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (self))))
    return gtk_source_language_get_id (lang);

  return "plain";
}

void
ide_buffer_set_language_id (IdeBuffer   *self,
                            const gchar *language_id)
{
  GtkSourceLanguage *language = NULL;

  g_return_if_fail (IDE_IS_BUFFER (self));

  if (language_id != NULL)
    {
      GtkSourceLanguageManager *manager;

      manager = gtk_source_language_manager_get_default ();
      language = gtk_source_language_manager_get_language (manager, language_id);
    }

  gtk_source_buffer_set_language (GTK_SOURCE_BUFFER (self), language);
}

IdeHighlightEngine *
_ide_buffer_get_highlight_engine (IdeBuffer *self)
{
  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);

  return self->highlight_engine;
}

void
_ide_buffer_set_failure (IdeBuffer    *self,
                         const GError *error)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_BUFFER (self));

  if (error == self->failure)
    return;

  if (error != NULL)
    self->state = IDE_BUFFER_STATE_FAILED;

  g_clear_pointer (&self->failure, g_error_free);
  self->failure = g_error_copy (error);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FAILED]);
}

/**
 * ide_buffer_get_failure:
 *
 * Gets a #GError representing a failure that has occurred for the
 * buffer.
 *
 * Returns: (transfer none): a #GError, or %NULL
 */
const GError *
ide_buffer_get_failure (IdeBuffer *self)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);

  return self->failure;
}

/**
 * ide_buffer_get_failed:
 * @self: an #IdeBuffer
 *
 * Gets the #IdeBuffer:failed property, denoting if the buffer has failed
 * in some aspect such as loading or saving.
 *
 * Returns: %TRUE if the buffer is in a failed state
 */
gboolean
ide_buffer_get_failed (IdeBuffer *self)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (IDE_IS_BUFFER (self), FALSE);

  return self->state == IDE_BUFFER_STATE_FAILED;
}

static void
ide_buffer_set_file_settings (IdeBuffer       *self,
                              IdeFileSettings *file_settings)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER (self));
  g_assert (IDE_IS_FILE_SETTINGS (file_settings));

  if (self->file_settings == file_settings)
    return;

  ide_clear_and_destroy_object (&self->file_settings);
  self->file_settings = g_object_ref (file_settings);

  g_signal_group_set_target (self->file_settings_signals, file_settings);
  on_settings_changed_newline_type_cb (self, NULL, self->file_settings);

  if (!ide_buffer_get_loading (self))
    ide_buffer_update_implicit_newline (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FILE_SETTINGS]);
}

static void
ide_buffer_reload_file_settings (IdeBuffer *self)
{
  IdeObjectBox *box;
  const gchar *lang_id;
  GFile *file;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER (self));

  file = ide_buffer_get_file (self);
  lang_id = ide_buffer_get_language_id (self);

  /* Bail if we'll just create the same settings as before */
  if (self->file_settings != NULL &&
      (g_file_equal (file, ide_file_settings_get_file (self->file_settings)) &&
       ide_str_equal0 (lang_id, ide_file_settings_get_language (self->file_settings))))
    return;

  /* Now apply the settings (and they'll settle in the background) */
  if ((box = ide_object_box_from_object (G_OBJECT (self))))
    {
      g_autoptr(IdeFileSettings) file_settings = NULL;

      file_settings = ide_file_settings_new (IDE_OBJECT (box), file, lang_id);
      ide_buffer_set_file_settings (self, file_settings);
    }
}

/**
 * ide_buffer_get_loading:
 * @self: an #IdeBuffer
 *
 * This checks to see if the buffer is currently loading. This is equivalent
 * to calling ide_buffer_get_state() and checking for %IDE_BUFFER_STATE_LOADING.
 *
 * Returns: %TRUE if the buffer is loading; otherwise %FALSE.
 */
gboolean
ide_buffer_get_loading (IdeBuffer *self)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (IDE_IS_BUFFER (self), FALSE);

  return ide_buffer_get_state (self) == IDE_BUFFER_STATE_LOADING;
}

static void
ide_buffer_changed (GtkTextBuffer *buffer)
{
  IdeBuffer *self = (IdeBuffer *)buffer;

  g_assert (IDE_IS_BUFFER (self));

  GTK_TEXT_BUFFER_CLASS (ide_buffer_parent_class)->changed (buffer);

  g_clear_object (&self->in_flight_symbol_at_location);
  self->in_flight_symbol_at_location_pos = -1;

  self->change_count++;
  g_clear_pointer (&self->content, g_bytes_unref);
  ide_buffer_delay_settling (self);
}

static void
ide_buffer_delete_range (GtkTextBuffer *buffer,
                         GtkTextIter   *begin,
                         GtkTextIter   *end)
{
  IdeBuffer *self = (IdeBuffer *)buffer;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER (self));
  g_assert (begin != NULL);
  g_assert (end != NULL);

  GTK_TEXT_BUFFER_CLASS (ide_buffer_parent_class)->delete_range (buffer, begin, end);

  IDE_EXIT;
}

static void
ide_buffer_insert_text (GtkTextBuffer *buffer,
                        GtkTextIter   *location,
                        const gchar   *text,
                        gint           len)
{
  IdeBuffer *self = (IdeBuffer *)buffer;
  gboolean recheck_language = FALSE;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER (self));
  g_assert (location != NULL);
  g_assert (text != NULL);

  /*
   * If we are inserting a \n at the end of the first line, then we might want
   * to adjust the GtkSourceBuffer:language property to reflect the format.
   * This is similar to emacs "modelines", which is apparently a bit of an
   * overloaded term as is not to be confused with editor setting modelines.
   */
  if ((gtk_text_iter_get_line (location) == 0) && gtk_text_iter_ends_line (location) &&
      ((text [0] == '\n') || ((len > 1) && (strchr (text, '\n') != NULL))))
    recheck_language = TRUE;

  GTK_TEXT_BUFFER_CLASS (ide_buffer_parent_class)->insert_text (buffer, location, text, len);

  if G_UNLIKELY (recheck_language)
    ide_buffer_guess_language (IDE_BUFFER (buffer));

  IDE_EXIT;
}

/**
 * ide_buffer_get_changed_on_volume:
 * @self: an #IdeBuffer
 *
 * Returns %TRUE if the #IdeBuffer is known to have been modified on storage
 * externally from this #IdeBuffer.
 *
 * Returns: %TRUE if @self is known to be modified on storage
 */
gboolean
ide_buffer_get_changed_on_volume (IdeBuffer *self)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (IDE_IS_BUFFER (self), FALSE);

  return self->changed_on_volume;
}

/**
 * _ide_buffer_set_changed_on_volume:
 * @self: an #IdeBuffer
 * @changed_on_volume: if the buffer was changed externally
 *
 * Sets the #IdeBuffer:changed-on-volume property.
 *
 * Set this to %TRUE if the buffer has been discovered to have changed
 * outside of this buffer.
 */
void
_ide_buffer_set_changed_on_volume (IdeBuffer *self,
                                   gboolean   changed_on_volume)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_BUFFER (self));

  changed_on_volume = !!changed_on_volume;

  if (changed_on_volume != self->changed_on_volume)
    {
      self->changed_on_volume = changed_on_volume;
      if (changed_on_volume)
        gtk_text_buffer_set_modified (GTK_TEXT_BUFFER (self), TRUE);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CHANGED_ON_VOLUME]);
    }
}

/**
 * ide_buffer_get_read_only:
 *
 * This function returns %TRUE if the underlying file has been discovered to
 * be read-only. This may be used by the interface to display information to
 * the user about saving the file.
 *
 * Returns: %TRUE if the underlying file is read-only
 */
gboolean
ide_buffer_get_read_only (IdeBuffer *self)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (IDE_IS_BUFFER (self), FALSE);

  return self->read_only;
}

/**
 * _ide_buffer_set_read_only:
 * @self: an #IdeBuffer
 * @read_only: if the buffer is read-only
 *
 * Sets the #IdeBuffer:read-only property, which should be set when the buffer
 * has been discovered to be read-only on disk.
 */
void
_ide_buffer_set_read_only (IdeBuffer *self,
                           gboolean   read_only)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_BUFFER (self));

  read_only = !!read_only;

  if (read_only != self->read_only)
    {
      self->read_only = read_only;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_READ_ONLY]);
    }
}

/**
 * ide_buffer_get_style_scheme_name:
 * @self: an #IdeBuffer
 *
 * Gets the name of the #GtkSourceStyleScheme from the #IdeBuffer:style-scheme
 * property.
 *
 * Returns: (nullable): a string containing the style scheme or %NULL
 */
const gchar *
ide_buffer_get_style_scheme_name (IdeBuffer *self)
{
  GtkSourceStyleScheme *scheme;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);

  if ((scheme = gtk_source_buffer_get_style_scheme (GTK_SOURCE_BUFFER (self))))
    return gtk_source_style_scheme_get_id (scheme);

  return NULL;
}

/**
 * ide_buffer_set_style_scheme_name:
 * @self: an #IdeBuffer
 * @style_scheme_name: (nullable): string containing the style scheme's name
 *
 * Sets the #IdeBuffer:style-scheme property by locating the style scheme
 * matching @style_scheme_name.
 */
void
ide_buffer_set_style_scheme_name (IdeBuffer   *self,
                                  const gchar *style_scheme_name)
{
  GtkSourceStyleSchemeManager *manager;
  GtkSourceStyleScheme *scheme;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_BUFFER (self));

  if ((manager = gtk_source_style_scheme_manager_get_default ()) &&
      (scheme = gtk_source_style_scheme_manager_get_scheme (manager, style_scheme_name)))
    gtk_source_buffer_set_style_scheme (GTK_SOURCE_BUFFER (self), scheme);
  else
    gtk_source_buffer_set_style_scheme (GTK_SOURCE_BUFFER (self), NULL);
}

/**
 * ide_buffer_dup_title:
 * @self: an #IdeBuffer
 *
 * Gets a string to represent the title of the buffer. An attempt is made to
 * make this relative to the project workdir if possible.
 *
 * Returns: (transfer full): a string containing a title
 */
gchar *
ide_buffer_dup_title (IdeBuffer *self)
{
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(GFile) workdir = NULL;
  g_autoptr(GFile) home = NULL;
  GFile *file;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);

  file = ide_buffer_get_file (self);

  if (self->is_temporary)
    return g_file_get_basename (file);

  /* Unlikely, but better to be safe */
  if (!(context = ide_buffer_ref_context (self)))
    return g_file_get_basename (file);

  workdir = ide_context_ref_workdir (context);

  if (g_file_has_prefix (file, workdir))
    return g_file_get_relative_path (workdir, file);

  home = g_file_new_for_path (g_get_home_dir ());

  if (g_file_has_prefix (file, home))
    {
      g_autofree gchar *relative = g_file_get_relative_path (home, file);
      return g_strdup_printf ("~/%s", relative);
    }

  if (!g_file_is_native (file))
    return g_file_get_uri (file);
  else
    return g_file_get_path (file);
}

/**
 * ide_buffer_get_highlight_diagnostics:
 * @self: an #IdeBuffer
 *
 * Checks if diagnostics should be highlighted.
 *
 * Returns: %TRUE if diagnostics should be highlighted
 */
gboolean
ide_buffer_get_highlight_diagnostics (IdeBuffer *self)
{
  g_return_val_if_fail (IDE_IS_BUFFER (self), FALSE);

  return self->highlight_diagnostics;
}

/**
 * ide_buffer_set_highlight_diagnostics:
 * @self: an #IdeBuffer
 * @highlight_diagnostics: if diagnostics should be highlighted
 *
 * Sets the #IdeBuffer:highlight-diagnostics property.
 *
 * If set to %TRUE, diagnostics will be styled in the buffer.
 */
void
ide_buffer_set_highlight_diagnostics (IdeBuffer *self,
                                      gboolean   highlight_diagnostics)
{
  g_return_if_fail (IDE_IS_BUFFER (self));

  highlight_diagnostics = !!highlight_diagnostics;

  if (self->highlight_diagnostics != highlight_diagnostics)
    {
      ide_buffer_clear_diagnostics (self);
      self->highlight_diagnostics = highlight_diagnostics;
      ide_buffer_apply_diagnostics (self);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HIGHLIGHT_DIAGNOSTICS]);
    }
}

/**
 * ide_buffer_get_iter_location:
 * @self: an #IdeBuffer
 * @iter: a #GtkTextIter
 *
 * Gets an #IdeLocation for the position represented by @iter.
 *
 * Returns: (transfer full): an #IdeLocation
 */
IdeLocation *
ide_buffer_get_iter_location (IdeBuffer         *self,
                              const GtkTextIter *iter)
{
  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);
  g_return_val_if_fail (iter != NULL, NULL);

  return ide_location_new_with_offset (ide_buffer_get_file (self),
                                       gtk_text_iter_get_line (iter),
                                       gtk_text_iter_get_line_offset (iter),
                                       gtk_text_iter_get_offset (iter));
}

/**
 * ide_buffer_get_selection_range:
 * @self: an #IdeBuffer
 *
 * Gets an #IdeRange to represent the current buffer selection.
 *
 * Returns: (transfer full): an #IdeRange
 */
IdeRange *
ide_buffer_get_selection_range (IdeBuffer *self)
{
  g_autoptr(IdeLocation) begin = NULL;
  g_autoptr(IdeLocation) end = NULL;
  GtkTextIter begin_iter;
  GtkTextIter end_iter;

  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);

  gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (self), &begin_iter, &end_iter);
  gtk_text_iter_order (&begin_iter, &end_iter);

  begin = ide_buffer_get_iter_location (self, &begin_iter);
  end = ide_buffer_get_iter_location (self, &end_iter);

  return ide_range_new (begin, end);
}

/**
 * ide_buffer_get_change_count:
 * @self: an #IdeBuffer
 *
 * Gets the monotonic change count for the buffer.
 *
 * Returns: the change count for the buffer
 */
guint
ide_buffer_get_change_count (IdeBuffer *self)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), 0);
  g_return_val_if_fail (IDE_IS_BUFFER (self), 0);

  return self->change_count;
}

static gboolean
ide_buffer_settled_cb (gpointer user_data)
{
  IdeBuffer *self = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER (self));

  self->settling_source = 0;
  g_signal_emit (self, signals [CHANGE_SETTLED], 0);

  if (self->addins != NULL && self->enable_addins)
    ide_extension_set_adapter_foreach (self->addins,
                                       _ide_buffer_addin_change_settled_cb,
                                       self);

  return G_SOURCE_REMOVE;
}

static void
ide_buffer_delay_settling (IdeBuffer *self)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER (self));

  g_clear_handle_id (&self->settling_source, g_source_remove);
  self->settling_source = g_timeout_add (SETTLING_DELAY_MSEC,
                                         ide_buffer_settled_cb,
                                         self);
}

/**
 * ide_buffer_set_diagnostics:
 * @self: an #IdeBuffer
 * @diagnostics: (nullable): an #IdeDiagnostics
 *
 * Sets the #IdeDiagnostics for the buffer. These will be used to highlight
 * the buffer for errors and warnings if #IdeBuffer:highlight-diagnostics
 * is %TRUE.
 */
void
ide_buffer_set_diagnostics (IdeBuffer      *self,
                            IdeDiagnostics *diagnostics)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_BUFFER (self));
  g_return_if_fail (!diagnostics || IDE_IS_DIAGNOSTICS (diagnostics));

  if (diagnostics == self->diagnostics)
    return;

  if (self->diagnostics)
    {
      ide_buffer_clear_diagnostics (self);
      g_clear_object (&self->diagnostics);
    }

  if (diagnostics)
    {
      IdeCodeActionProvider *code_action_provider;
      self->diagnostics = g_object_ref (diagnostics);
      code_action_provider = ide_extension_adapter_get_extension (self->code_action_provider);
      if (code_action_provider)
        ide_code_action_provider_set_diagnostics (IDE_CODE_ACTION_PROVIDER (code_action_provider), self->diagnostics);
      ide_buffer_apply_diagnostics (self);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DIAGNOSTICS]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HAS_DIAGNOSTICS]);

  _ide_buffer_line_flags_changed (self);
}

/**
 * ide_buffer_get_diagnostics:
 * @self: an #IdeBuffer
 *
 * Gets the #IdeDiagnostics for the buffer if any have been registered.
 *
 * Returns: (transfer none) (nullable): an #IdeDiagnostics or %NULL
 */
IdeDiagnostics *
ide_buffer_get_diagnostics (IdeBuffer *self)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);

  return self->diagnostics;
}

/**
 * ide_buffer_has_diagnostics:
 * @self: a #IdeBuffer
 *
 * Returns %TRUE if any diagnostics have been registered for the buffer.
 *
 * Returns: %TRUE if there are a non-zero number of diagnostics.
 */
gboolean
ide_buffer_has_diagnostics (IdeBuffer *self)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (IDE_IS_BUFFER (self), FALSE);

  if (self->diagnostics)
    return g_list_model_get_n_items (G_LIST_MODEL (self->diagnostics)) > 0;

  return FALSE;
}

static void
ide_buffer_clear_diagnostics (IdeBuffer *self)
{
  GtkTextTagTable *table;
  GtkTextTag *tag;
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER (self));

  if (!self->highlight_diagnostics)
    return;

  gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (self), &begin, &end);

  table = gtk_text_buffer_get_tag_table (GTK_TEXT_BUFFER (self));

  if (NULL != (tag = gtk_text_tag_table_lookup (table, TAG_NOTE)))
    gtk_text_buffer_remove_tag (GTK_TEXT_BUFFER (self), tag, &begin, &end);

  if (NULL != (tag = gtk_text_tag_table_lookup (table, TAG_WARNING)))
    gtk_text_buffer_remove_tag (GTK_TEXT_BUFFER (self), tag, &begin, &end);

  if (NULL != (tag = gtk_text_tag_table_lookup (table, TAG_DEPRECATED)))
    gtk_text_buffer_remove_tag (GTK_TEXT_BUFFER (self), tag, &begin, &end);

  if (NULL != (tag = gtk_text_tag_table_lookup (table, TAG_UNUSED)))
    gtk_text_buffer_remove_tag (GTK_TEXT_BUFFER (self), tag, &begin, &end);

  if (NULL != (tag = gtk_text_tag_table_lookup (table, TAG_ERROR)))
    gtk_text_buffer_remove_tag (GTK_TEXT_BUFFER (self), tag, &begin, &end);
}

static void
ide_buffer_apply_diagnostic (IdeBuffer     *self,
                             IdeDiagnostic *diagnostic)
{
  IdeDiagnosticSeverity severity;
  const gchar *tag_name = NULL;
  IdeLocation *location;
  guint n_ranges;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER (self));
  g_assert (IDE_IS_DIAGNOSTIC (diagnostic));

  severity = ide_diagnostic_get_severity (diagnostic);

  switch (severity)
    {
    case IDE_DIAGNOSTIC_NOTE:
      tag_name = TAG_NOTE;
      break;

    case IDE_DIAGNOSTIC_UNUSED:
      tag_name = TAG_UNUSED;
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

  n_ranges = ide_diagnostic_get_n_ranges (diagnostic);
  if (n_ranges == 0)
    {
      if ((location = ide_diagnostic_get_location (diagnostic)))
        {
          GtkTextIter begin_iter;
          GtkTextIter end_iter;

          ide_buffer_get_iter_at_location (self, &begin_iter, location);
          end_iter = begin_iter;

          if (gtk_text_iter_ends_line (&end_iter))
            {
              gtk_text_iter_backward_char (&begin_iter);
            }
          else
            {
              /* Only highlight to next word */
              if (_ide_source_iter_inside_word (&end_iter) ||
                  _ide_source_iter_starts_word (&end_iter))
                _ide_source_iter_forward_visible_word_end (&end_iter);
              else
                gtk_text_iter_forward_to_line_end (&end_iter);
            }

          gtk_text_buffer_apply_tag_by_name (GTK_TEXT_BUFFER (self), tag_name, &begin_iter, &end_iter);
        }
    }

  for (guint i = 0; i < n_ranges; i++)
    {
      GtkTextIter begin_iter;
      GtkTextIter end_iter;
      IdeLocation *begin;
      IdeLocation *end;
      IdeRange *range;
      GFile *file;

      range = ide_diagnostic_get_range (diagnostic, i);
      begin = ide_range_get_begin (range);
      end = ide_range_get_end (range);
      file = ide_location_get_file (begin);

      if (file != NULL)
        {
          if (!g_file_equal (file, ide_buffer_get_file (self)))
            continue;
        }

      ide_buffer_get_iter_at_location (self, &begin_iter, begin);
      ide_buffer_get_iter_at_location (self, &end_iter, end);

      if (gtk_text_iter_equal (&begin_iter, &end_iter))
        {
          if (!gtk_text_iter_ends_line (&end_iter))
            gtk_text_iter_forward_char (&end_iter);
          else
            gtk_text_iter_backward_char (&begin_iter);
        }

      gtk_text_buffer_apply_tag_by_name (GTK_TEXT_BUFFER (self), tag_name, &begin_iter, &end_iter);
    }
}

typedef struct _ApplyDiagnostics
{
  IdeBuffer  *buffer;
  GListModel *diagnostics;
  guint       stamp;
} ApplyDiagnostics;

static void
apply_diagnostics_free (ApplyDiagnostics *state)
{
  g_clear_object (&state->buffer);
  g_clear_object (&state->diagnostics);
  g_free (state);
}

static gint64
next_deadline (void)
{
  return g_get_monotonic_time () + (G_USEC_PER_SEC / 1000L);
}

static DexFuture *
ide_buffer_apply_diagnostics_fiber (gpointer data)
{
  ApplyDiagnostics *state = data;
  gint64 deadline;
  guint n_items;

  g_assert (state != NULL);
  g_assert (G_IS_LIST_MODEL (state->diagnostics));
  g_assert (IDE_IS_BUFFER (state->buffer));

  n_items = g_list_model_get_n_items (G_LIST_MODEL (state->diagnostics));
  deadline = next_deadline ();

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(IdeDiagnostic) diagnostic = NULL;

      if (g_get_monotonic_time () > deadline)
        {
          deadline = next_deadline ();
          dex_await (dex_timeout_new_deadline (deadline), NULL);
        }

      if (state->stamp != state->buffer->diagnostics_stamp)
        break;

      if ((diagnostic = g_list_model_get_item (G_LIST_MODEL (state->diagnostics), i)))
        ide_buffer_apply_diagnostic (state->buffer, diagnostic);
    }

  return dex_future_new_true ();
}

static void
ide_buffer_apply_diagnostics (IdeBuffer *self)
{
  ApplyDiagnostics *state;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER (self));

  if (!self->highlight_diagnostics)
    return;

  if (self->diagnostics == NULL)
    return;

  state = g_new0 (ApplyDiagnostics, 1);
  state->stamp = ++self->diagnostics_stamp;
  state->diagnostics = g_object_ref (G_LIST_MODEL (self->diagnostics));
  state->buffer = g_object_ref (self);

  dex_future_disown (dex_scheduler_spawn (NULL, 0,
                                          ide_buffer_apply_diagnostics_fiber,
                                          state,
                                          (GDestroyNotify)apply_diagnostics_free));
}

/**
 * ide_buffer_get_iter_at_location:
 * @self: an #IdeBuffer
 * @iter: (out): a #GtkTextIter
 * @location: a #IdeLocation
 *
 * Set @iter to the position designated by @location.
 */
void
ide_buffer_get_iter_at_location (IdeBuffer   *self,
                                 GtkTextIter *iter,
                                 IdeLocation *location)
{
  gint line;
  gint line_offset;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_BUFFER (self));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (location != NULL);

  line = ide_location_get_line (location);
  line_offset = ide_location_get_line_offset (location);

  gtk_text_buffer_get_iter_at_line_offset (GTK_TEXT_BUFFER (self),
                                           iter,
                                           MAX (0, line),
                                           MAX (0, line_offset));

  /* Advance to first non-space if offset < 0 */
  if (line_offset < 0)
    {
      while (!gtk_text_iter_ends_line (iter))
        {
          if (!g_unichar_isspace (gtk_text_iter_get_char (iter)))
            break;
          gtk_text_iter_forward_char (iter);
        }
    }
}

/**
 * ide_buffer_get_change_monitor:
 * @self: an #IdeBuffer
 *
 * Gets the #IdeBuffer:change-monitor for the buffer.
 *
 * Returns: (transfer none) (nullable): an #IdeBufferChangeMonitor or %NULL
 */
IdeBufferChangeMonitor *
ide_buffer_get_change_monitor (IdeBuffer *self)
{
  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);

  return self->change_monitor;
}

/**
 * ide_buffer_set_change_monitor:
 * @self: an #IdeBuffer
 * @change_monitor: (nullable): an #IdeBufferChangeMonitor or %NULL
 *
 * Sets an #IdeBufferChangeMonitor to use for the buffer.
 */
void
ide_buffer_set_change_monitor (IdeBuffer              *self,
                               IdeBufferChangeMonitor *change_monitor)
{
  g_return_if_fail (IDE_IS_BUFFER (self));
  g_return_if_fail (!change_monitor || IDE_IS_BUFFER_CHANGE_MONITOR (change_monitor));

  if (g_set_object (&self->change_monitor, change_monitor))
    {
      /* Destroy change monitor with us if we can */
      if (change_monitor && ide_object_is_root (IDE_OBJECT (change_monitor)))
        {
          IdeObjectBox *box = ide_object_box_from_object (G_OBJECT (self));
          ide_object_append (IDE_OBJECT (box), IDE_OBJECT (change_monitor));
        }

      if (change_monitor != NULL)
        ide_buffer_change_monitor_reload (change_monitor);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CHANGE_MONITOR]);
    }
}

static gboolean
ide_buffer_can_do_newline_hack (IdeBuffer *self,
                                guint      len)
{
  guint next_pow2;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER (self));

  /*
   * If adding two bytes to our length (one for \n and one for \0) is still
   * under the next power of two, then we can avoid making a copy of the buffer
   * when saving the buffer to our drafts.
   *
   * HACK: This relies on the fact that GtkTextBuffer returns a GString
   *       allocated string which grows the string in powers of two.
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
 * ide_buffer_dup_content:
 * @self: an #IdeBuffer.
 *
 * Gets the contents of the buffer as GBytes.
 *
 * By using this function to get the bytes, you allow #IdeBuffer to avoid
 * calculating the buffer text unnecessarily, potentially saving on
 * allocations.
 *
 * Additionally, this allows the buffer to update the state in #IdeUnsavedFiles
 * if the content is out of sync.
 *
 * Returns: (transfer full): a #GBytes containing the buffer content.
 */
GBytes *
ide_buffer_dup_content (IdeBuffer *self)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);

  if (self->content == NULL)
    {
      g_autoptr(IdeContext) context = NULL;
      IdeUnsavedFiles *unsaved_files;
      GtkTextIter begin;
      GtkTextIter end;
      GFile *file;
      gchar *text;
      gsize len;

      gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (self), &begin, &end);
      text = gtk_text_buffer_get_text (GTK_TEXT_BUFFER (self), &begin, &end, TRUE);

      /*
       * If implicit newline is set, add a \n in place of the \0 and avoid
       * duplicating the buffer. Make sure to track length beforehand, since we
       * would overwrite afterwards. Since conversion to \r\n is dealth with
       * during save operations, this should be fine for both. The unsaved
       * files will restore to a buffer, for which \n is acceptable.
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
      self->content = g_bytes_new_take (g_steal_pointer (&text), len);

      /* Only persist if we have access to the object tree */
      if (self->buffer_manager != NULL &&
          !ide_object_in_destruction (IDE_OBJECT (self->buffer_manager)))
        {
          file = ide_buffer_get_file (self);
          context = ide_buffer_ref_context (IDE_BUFFER (self));
          unsaved_files = ide_unsaved_files_from_context (context);
          ide_unsaved_files_update (unsaved_files, file, self->content);
        }
    }

  return g_bytes_ref (self->content);
}

static void
ide_buffer_format_selection_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  IdeFormatter *formatter = (IdeFormatter *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = user_data;

  IDE_ENTRY;

  g_assert (IDE_IS_FORMATTER (object));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_formatter_format_finish (formatter, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_buffer_format_selection_range_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  IdeFormatter *formatter = (IdeFormatter *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = user_data;

  IDE_ENTRY;

  g_assert (IDE_IS_FORMATTER (object));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_formatter_format_range_finish (formatter, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

/**
 * ide_buffer_format_selection_async:
 * @self: an #IdeBuffer
 * @options: options for the formatting
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @callback: the callback upon completion
 * @user_data: user data for @callback
 *
 * Formats the selection using an available #IdeFormatter for the buffer.
 */
void
ide_buffer_format_selection_async (IdeBuffer           *self,
                                   IdeFormatterOptions *options,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  IdeFormatter *formatter;
  GtkTextIter begin;
  GtkTextIter end;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_BUFFER (self));
  g_return_if_fail (IDE_IS_FORMATTER_OPTIONS (options));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_buffer_format_selection_async);

  if (!(formatter = ide_extension_adapter_get_extension (self->formatter)))
    {
      const gchar *language_id = ide_buffer_get_language_id (self);

      if (language_id == NULL)
        language_id = "none";

      ide_task_return_new_error (task,
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

/**
 * ide_buffer_format_selection_finish:
 * @self: an #IdeBuffer
 * @result: a #GAsyncResult
 * @error: a location for a #GError, or %NULL
 *
 * Completes an asynchronous request to ide_buffer_format_selection_async().
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 */
gboolean
ide_buffer_format_selection_finish (IdeBuffer     *self,
                                    GAsyncResult  *result,
                                    GError       **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (IDE_IS_BUFFER (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_buffer_query_code_action_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  IdeCodeActionProvider *code_action_provider = (IdeCodeActionProvider *)object;
  g_autoptr(GPtrArray) code_actions = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CODE_ACTION_PROVIDER (object));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  code_actions = ide_code_action_provider_query_finish (code_action_provider, result, &error);

  if (!code_actions)
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task, g_steal_pointer (&code_actions), g_ptr_array_unref);

  IDE_EXIT;
}

/**
 * ide_buffer_get_code_action_provider:
 * @self: a #IdeBuffer
 *
 * Gets the code action provider, if any.
 *
 * Returns: (transfer none) (nullable): an #IdeCodeActionProvider or %NULL
 *
 * Since: 44
 */
IdeCodeActionProvider *
ide_buffer_get_code_action_provider (IdeBuffer *self)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);

  return IDE_CODE_ACTION_PROVIDER (ide_extension_adapter_get_extension (self->code_action_provider));
}

/**
 * ide_buffer_code_action_query_async:
 * @self: an #IdeBuffer
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @callback: the callback upon completion
 * @user_data: user data for @callback
 *
 * Queries for code actions in the current buffer.
 */
void
ide_buffer_code_action_query_async (IdeBuffer           *self,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  IdeCodeActionProvider *code_action_provider;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_BUFFER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_buffer_code_action_query_async);

  if (!(code_action_provider = ide_extension_adapter_get_extension (self->code_action_provider)))
    {
      const gchar *language_id = ide_buffer_get_language_id (self);

      if (language_id == NULL)
        language_id = "none";

      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "No code action provider registered for language %s",
                                 language_id);

      IDE_EXIT;
    }

  ide_code_action_provider_query_async (code_action_provider,
                                        self,
                                        cancellable,
                                        ide_buffer_query_code_action_cb,
                                        g_steal_pointer (&task));

  IDE_EXIT;
}

/**
 * ide_buffer_code_action_query_finish:
 * @self: an #IdeBuffer
 * @result: a #GAsyncResult
 * @error: a location for a #GError, or %NULL
 *
 * Completes an asynchronous request to ide_buffer_query_code_action_async().
 *
 * Returns: (transfer full) (element-type IdeCodeAction): a #GPtrArray of #IdeCodeAction.
 */
GPtrArray*
ide_buffer_code_action_query_finish (IdeBuffer     *self,
                                     GAsyncResult  *result,
                                     GError       **error)
{
  GPtrArray* ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

/**
 * ide_buffer_get_insert_location:
 *
 * Gets the location of the insert mark as an #IdeLocation.
 *
 * Returns: (transfer full): An #IdeLocation
 */
IdeLocation *
ide_buffer_get_insert_location (IdeBuffer *self)
{
  GtkTextMark *mark;
  GtkTextIter iter;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);

  mark = gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (self));
  gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (self), &iter, mark);

  return ide_buffer_get_iter_location (self, &iter);
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

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);
  g_return_val_if_fail (iter != NULL, NULL);

  end = begin = *iter;

  if (!_ide_source_iter_starts_word (&begin))
    _ide_source_iter_backward_extra_natural_word_start (&begin);

  if (!_ide_source_iter_ends_word (&end))
    _ide_source_iter_forward_extra_natural_word_end (&end);

  return gtk_text_iter_get_slice (&begin, &end);
}

/**
 * ide_buffer_get_rename_provider:
 * @self: an #IdeBuffer
 *
 * Gets the #IdeRenameProvider for this buffer, or %NULL.
 *
 * Returns: (nullable) (transfer none): An #IdeRenameProvider or %NULL if
 *   there is no #IdeRenameProvider that can satisfy the buffer.
 */
IdeRenameProvider *
ide_buffer_get_rename_provider (IdeBuffer *self)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);

  if (self->rename_provider != NULL)
    return ide_extension_adapter_get_extension (self->rename_provider);

  return NULL;
}

/**
 * ide_buffer_get_file_settings:
 * @self: an #IdeBuffer
 *
 * Gets the #IdeBuffer:file-settings property.
 *
 * The #IdeFileSettings are updated when changes to the file or language
 * syntax are chnaged.
 *
 * Returns: (transfer none) (nullable): an #IdeFileSettings or %NULL
 */
IdeFileSettings *
ide_buffer_get_file_settings (IdeBuffer *self)
{
  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);

  return self->file_settings;
}

/**
 * ide_buffer_ref_context:
 * @self: an #IdeBuffer
 *
 * Locates the #IdeContext for the buffer and returns it.
 *
 * Returns: (transfer full): an #IdeContext
 */
IdeContext *
ide_buffer_ref_context (IdeBuffer *self)
{
  g_autoptr(IdeObject) root = NULL;

  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);

  if (self->buffer_manager != NULL)
    root = ide_object_ref_root (IDE_OBJECT (self->buffer_manager));

  g_return_val_if_fail (root != NULL, NULL);
  g_return_val_if_fail (IDE_IS_CONTEXT (root), NULL);

  return IDE_CONTEXT (g_steal_pointer (&root));
}

static void
apply_style (GtkTextTag  *tag,
             const gchar *first_property,
             ...)
{
  va_list args;

  g_assert (IDE_IS_MAIN_THREAD ());
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
  GdkRGBA unused_rgba;
  GdkRGBA error_rgba;
  GdkRGBA note_rgba;
  GdkRGBA warning_rgba;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER (self));
  g_assert (pspec != NULL);

  style_scheme = gtk_source_buffer_get_style_scheme (GTK_SOURCE_BUFFER (self));
  table = gtk_text_buffer_get_tag_table (GTK_TEXT_BUFFER (self));

#define GET_TAG(name) (gtk_text_tag_table_lookup(table, name))

  if (style_scheme != NULL)
    {
      /* These are a fall-back if our style scheme isn't installed. */
      gdk_rgba_parse (&deprecated_rgba, DEPRECATED_COLOR);
      gdk_rgba_parse (&unused_rgba, UNUSED_COLOR);
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
                                                TAG_UNUSED,
                                                GET_TAG (TAG_UNUSED)))
        apply_style (GET_TAG (TAG_UNUSED),
                     "underline", PANGO_UNDERLINE_ERROR,
                     "underline-rgba", &unused_rgba,
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

  if (self->addins != NULL && self->enable_addins)
    ide_extension_set_adapter_foreach (self->addins,
                                       _ide_buffer_addin_style_scheme_changed_cb,
                                       self);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_STYLE_SCHEME_NAME]);

}

static void
ide_buffer_on_tag_added (IdeBuffer       *self,
                         GtkTextTag      *tag,
                         GtkTextTagTable *table)
{
  GtkTextTag *chunk_tag;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER (self));
  g_assert (GTK_IS_TEXT_TAG (tag));
  g_assert (GTK_IS_TEXT_TAG_TABLE (table));

  /* Adjust priority of our tab-stop tag. */
  chunk_tag = gtk_text_tag_table_lookup (table, TAG_SNIPPET_TAB_STOP);
  if (chunk_tag != NULL)
    gtk_text_tag_set_priority (chunk_tag,
                               gtk_text_tag_table_get_size (table) - 1);
}

static void
ide_buffer_init_tags (IdeBuffer *self)
{
  GtkTextTagTable *tag_table;
  GtkSourceStyleScheme *style_scheme;
  g_autoptr(GtkTextTag) deprecated_tag = NULL;
  g_autoptr(GtkTextTag) unused_tag = NULL;
  g_autoptr(GtkTextTag) error_tag = NULL;
  g_autoptr(GtkTextTag) note_tag = NULL;
  g_autoptr(GtkTextTag) warning_tag = NULL;
  GdkRGBA deprecated_rgba;
  GdkRGBA unused_rgba;
  GdkRGBA error_rgba;
  GdkRGBA note_rgba;
  GdkRGBA warning_rgba;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER (self));

  tag_table = gtk_text_buffer_get_tag_table (GTK_TEXT_BUFFER (self));
  style_scheme = gtk_source_buffer_get_style_scheme (GTK_SOURCE_BUFFER (self));

  /* These are fall-back if our style scheme isn't installed. */
  gdk_rgba_parse (&deprecated_rgba, DEPRECATED_COLOR);
  gdk_rgba_parse (&unused_rgba, UNUSED_COLOR);
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
  unused_tag = gtk_text_tag_new (TAG_UNUSED);
  error_tag = gtk_text_tag_new (TAG_ERROR);
  note_tag = gtk_text_tag_new (TAG_NOTE);
  warning_tag = gtk_text_tag_new (TAG_WARNING);

  if (!ide_source_style_scheme_apply_style (style_scheme, TAG_DEPRECATED, deprecated_tag))
    apply_style (deprecated_tag,
                 "underline", PANGO_UNDERLINE_ERROR,
                 "underline-rgba", &deprecated_rgba,
                 NULL);

  if (!ide_source_style_scheme_apply_style (style_scheme, TAG_UNUSED, unused_tag))
    apply_style (unused_tag,
                 "underline", PANGO_UNDERLINE_ERROR,
                 "underline-rgba", &unused_rgba,
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
  gtk_text_tag_table_add (tag_table, unused_tag);
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

/**
 * ide_buffer_get_formatter:
 * @self: an #IdeBuffer
 *
 * Gets an #IdeFormatter for the buffer, if any.
 *
 * Returns: (transfer none) (nullable): an #IdeFormatter or %NULL
 */
IdeFormatter *
ide_buffer_get_formatter (IdeBuffer *self)
{
  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);

  if (self->formatter == NULL)
    return NULL;

  return ide_extension_adapter_get_extension (self->formatter);
}

void
_ide_buffer_sync_to_unsaved_files (IdeBuffer *self)
{
  GBytes *content;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER (self));

  if ((content = ide_buffer_dup_content (self)))
    g_bytes_unref (content);

  IDE_EXIT;
}

/**
 * ide_buffer_rehighlight:
 * @self: an #IdeBuffer
 *
 * Force @self to rebuild the highlighted words.
 */
void
ide_buffer_rehighlight (IdeBuffer *self)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_BUFFER (self));

  /* In case we are disposing */
  if (self->highlight_engine == NULL || ide_buffer_get_loading (self))
    IDE_EXIT;

  if (gtk_source_buffer_get_highlight_syntax (GTK_SOURCE_BUFFER (self)))
    ide_highlight_engine_rebuild (self->highlight_engine);
  else
    ide_highlight_engine_clear (self->highlight_engine);

  IDE_EXIT;
}

static void
ide_buffer_get_symbol_at_location_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  IdeSymbolResolver *symbol_resolver = (IdeSymbolResolver *)object;
  g_autoptr(IdeSymbol) symbol = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  LookUpSymbolData *data;
  IdeBuffer *self;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SYMBOL_RESOLVER (symbol_resolver));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  data = ide_task_get_task_data (task);
  self = ide_task_get_source_object (task);
  g_assert (data->resolvers != NULL);
  g_assert (data->resolvers->len > 0);

  g_clear_object (&self->in_flight_symbol_at_location);
  self->in_flight_symbol_at_location_pos = -1;

  if ((symbol = ide_symbol_resolver_lookup_symbol_finish (symbol_resolver, result, &error)))
    {
      /*
       * Store symbol which has definition location. If no symbol has
       * definition location then store symbol which has declaration location.
       */
      if ((data->symbol == NULL) ||
          (ide_symbol_get_location (symbol) != NULL) ||
          (ide_symbol_get_location (data->symbol) == NULL &&
           ide_symbol_get_header_location (symbol)))
        {
          g_clear_object (&data->symbol);
          data->symbol = g_steal_pointer (&symbol);
        }
    }

  g_ptr_array_remove_index (data->resolvers, data->resolvers->len - 1);

  IDE_TRACE_MSG ("%u resolvers left to query", data->resolvers->len);

  if (data->resolvers->len > 0)
    {
      IdeSymbolResolver *resolver;
      GCancellable *cancellable;

      resolver = g_ptr_array_index (data->resolvers, data->resolvers->len - 1);
      cancellable = ide_task_get_cancellable (task);

      ide_symbol_resolver_lookup_symbol_async (resolver,
                                               data->location,
                                               cancellable,
                                               ide_buffer_get_symbol_at_location_cb,
                                               g_steal_pointer (&task));
    }
  else if (data->symbol == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_FOUND,
                                 "Symbol not found");
    }
  else
    {
      ide_task_return_object (task, g_steal_pointer (&data->symbol));
    }

  IDE_EXIT;
}

/**
 * ide_buffer_get_symbol_at_location_async:
 * @self: an #IdeBuffer
 * @location: a #GtkTextIter indicating a position to search for a symbol
 * @cancellable: a #GCancellable
 * @callback: a #GAsyncReadyCallback
 * @user_data: a #gpointer to hold user data
 *
 * Asynchronously get a possible symbol at @location.
 */
void
ide_buffer_get_symbol_at_location_async (IdeBuffer           *self,
                                         const GtkTextIter   *location,
                                         GCancellable        *cancellable,
                                         GAsyncReadyCallback  callback,
                                         gpointer             user_data)
{
  g_autoptr(IdeLocation) srcloc = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GPtrArray) resolvers = NULL;
  IdeSymbolResolver *resolver;
  LookUpSymbolData *data;
  guint line;
  guint line_offset;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_BUFFER (self));
  g_return_if_fail (location != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  resolvers = ide_buffer_get_symbol_resolvers (self);
  IDE_PTR_ARRAY_SET_FREE_FUNC (resolvers, g_object_unref);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_buffer_get_symbol_at_location_async);

  if (resolvers->len == 0)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 _("The current language lacks a symbol resolver."));
      IDE_EXIT;
    }

  /* If this query is the same as one in-flight, then try to chain
   * to that query instead of competing and duplicating work.
   */
  if (self->in_flight_symbol_at_location_pos == (int)gtk_text_iter_get_offset (location) &&
      self->in_flight_symbol_at_location != NULL)
    {
      ide_task_chain (self->in_flight_symbol_at_location, task);
      IDE_EXIT;
    }
  else
    {
      g_set_object (&self->in_flight_symbol_at_location, task);
      self->in_flight_symbol_at_location_pos = gtk_text_iter_get_offset (location);
    }

  _ide_buffer_sync_to_unsaved_files (self);

  line = gtk_text_iter_get_line (location);
  line_offset = gtk_text_iter_get_line_offset (location);
  srcloc = ide_location_new (ide_buffer_get_file (self), line, line_offset);

  data = g_slice_new0 (LookUpSymbolData);
  data->resolvers = g_steal_pointer (&resolvers);
  data->location = g_steal_pointer (&srcloc);
  ide_task_set_task_data (task, data, lookup_symbol_data_free);

  /* Try lookup_symbol on each symbol resolver one by by one. */
  resolver = g_ptr_array_index (data->resolvers, data->resolvers->len - 1);
  ide_symbol_resolver_lookup_symbol_async (resolver,
                                           data->location,
                                           cancellable,
                                           ide_buffer_get_symbol_at_location_cb,
                                           g_steal_pointer (&task));

  IDE_EXIT;
}

/**
 * ide_buffer_get_symbol_at_location_finish:
 * @self: an #IdeBuffer
 * @result: a #GAsyncResult
 * @error: a location for a #GError
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
  IdeSymbol *symbol;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  symbol = ide_task_propagate_object (IDE_TASK (result), error);

  IDE_RETURN (symbol);
}

/**
 * ide_buffer_get_selection_bounds:
 * @self: an #IdeBuffer
 * @insert: (out) (nullable): a #GtkTextIter to get the insert position
 * @selection: (out) (nullable): a #GtkTextIter to get the selection position
 *
 * This function acts like gtk_text_buffer_get_selection_bounds() except that
 * it always places the location of the insert mark at @insert and the location
 * of the selection mark at @selection.
 *
 * Calling gtk_text_iter_order() with the results of this function would be
 * equivalent to calling gtk_text_buffer_get_selection_bounds().
 */
void
ide_buffer_get_selection_bounds (IdeBuffer   *self,
                                 GtkTextIter *insert,
                                 GtkTextIter *selection)
{
  GtkTextMark *mark;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
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

static void
ide_buffer_get_symbol_resolvers_cb (IdeExtensionSetAdapter *set,
                                    PeasPluginInfo         *plugin_info,
                                    GObject          *extension,
                                    gpointer                user_data)
{
  IdeSymbolResolver *resolver = (IdeSymbolResolver *)extension;
  GPtrArray *ar = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_SYMBOL_RESOLVER (resolver));
  g_assert (ar != NULL);

  g_ptr_array_add (ar, g_object_ref (resolver));
}

/**
 * ide_buffer_get_symbol_resolvers:
 * @self: an #IdeBuffer
 *
 * Gets the symbol resolvers for the buffer based on the current language. The
 * resolvers in the resulting array are sorted by priority.
 *
 * Returns: (transfer full) (element-type IdeSymbolResolver): a #GPtrArray
 *   of #IdeSymbolResolver.
 */
GPtrArray *
ide_buffer_get_symbol_resolvers (IdeBuffer *self)
{
  GPtrArray *ar;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);

  ar = g_ptr_array_new_with_free_func (g_object_unref);

  if (self->symbol_resolvers != NULL)
    ide_extension_set_adapter_foreach_by_priority (self->symbol_resolvers,
                                                   ide_buffer_get_symbol_resolvers_cb,
                                                   ar);

  return IDE_PTR_ARRAY_STEAL_FULL (&ar);
}

static void
ide_buffer_list_symbol_resolvers_cb (IdeExtensionSetAdapter *set,
                                     PeasPluginInfo         *plugin_info,
                                     GObject                *extension,
                                     gpointer                user_data)
{
  IdeSymbolResolver *resolver = (IdeSymbolResolver *)extension;
  GListStore *store = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_SYMBOL_RESOLVER (resolver));
  g_assert (G_IS_LIST_STORE (store));

  g_list_store_append (store, resolver);
}

/**
 * ide_buffer_list_symbol_resolvers:
 * @self: a #IdeBuffer
 *
 * Gets the symbol resolvers for the buffer.
 *
 * Returns: (transfer full): a #GListModel of #IdeSymbolResolver
 *
 * Since: 45
 */
GListModel *
ide_buffer_list_symbol_resolvers (IdeBuffer *self)
{
  g_autoptr(GListStore) store = NULL;

  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);

  store = g_list_store_new (IDE_TYPE_SYMBOL_RESOLVER);

  if (self->symbol_resolvers != NULL)
    ide_extension_set_adapter_foreach_by_priority (self->symbol_resolvers,
                                                   ide_buffer_list_symbol_resolvers_cb,
                                                   store);

  return G_LIST_MODEL (g_steal_pointer (&store));
}

/**
 * ide_buffer_get_line_text:
 * @self: a #IdeBuffer
 * @line: a line number starting from 0
 *
 * Gets the contents of a single line within the buffer.
 *
 * Returns: (transfer full) (nullable): a string containing the line's text
 *   or %NULL if the line does not exist.
 */
gchar *
ide_buffer_get_line_text (IdeBuffer *self,
                          guint      line)
{
  GtkTextIter begin;

  g_assert (IDE_IS_BUFFER (self));

  gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (self), &begin, line);

  if (gtk_text_iter_get_line (&begin) == line)
    {
      GtkTextIter end = begin;

      if (gtk_text_iter_ends_line (&end) ||
          gtk_text_iter_forward_to_line_end (&end))
        return gtk_text_iter_get_slice (&begin, &end);
    }

  return g_strdup ("");
}

static void
ide_buffer_guess_language (IdeBuffer *self)
{
  GtkSourceLanguageManager *manager;
  GtkSourceLanguage *lang;
  g_autofree gchar *basename = NULL;
  g_autofree gchar *content_type = NULL;
  g_autofree gchar *line = NULL;
  const gchar *lang_id;
  const gchar *path;
  GFile *file;
  gboolean uncertain = FALSE;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER (self));

  line = ide_buffer_get_line_text (self, 0);
  file = ide_buffer_get_file (self);

  basename = g_file_get_basename (file);

  if (!g_file_is_native (file))
    path = basename;
  else
    path = g_file_peek_path (file);

  manager = gtk_source_language_manager_get_default ();
  lang = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (self));

  content_type = g_content_type_guess (path, (const guchar *)line, strlen (line), &uncertain);
  if (uncertain && lang != NULL)
    return;

  /* First try with full path, then with shortname */
  if (!(lang = gtk_source_language_manager_guess_language (manager, path, content_type)) &&
      !(lang = gtk_source_language_manager_guess_language (manager, basename, content_type)))
    return;

  lang_id = gtk_source_language_get_id (lang);

  /* Override to python3 by default for now, until shared-mime-info
   * gets a better way to detect the difference between the two.
   */
  if (ide_str_equal0 (lang_id, "python"))
    {
      lang_id = "python3";
      lang = gtk_source_language_manager_get_language (manager, lang_id);
    }

  if (!ide_str_equal0 (lang_id, ide_buffer_get_language_id (self)))
    gtk_source_buffer_set_language (GTK_SOURCE_BUFFER (self), lang);
}

gboolean
_ide_buffer_can_restore_cursor (IdeBuffer *self)
{
  g_return_val_if_fail (IDE_IS_BUFFER (self), FALSE);

  return self->can_restore_cursor;
}

void
_ide_buffer_cancel_cursor_restore (IdeBuffer *self)
{
  g_return_if_fail (IDE_IS_BUFFER (self));

  self->can_restore_cursor = FALSE;
}

/**
 * ide_buffer_hold:
 * @self: a #IdeBuffer
 *
 * Increases the "hold count" of the #IdeBuffer by one.
 *
 * The hold count is similar to a reference count, as it allows the buffer
 * manager to know when a buffer may be destroyed cleanly.
 *
 * Doing so ensures that the buffer will not be unloaded or have reference
 * cycles broken.
 *
 * Release the hold with ide_buffer_release().
 *
 * When the hold count reaches zero, the buffer will be destroyed.
 *
 * Returns: (transfer full): @self
 */
IdeBuffer *
ide_buffer_hold (IdeBuffer *self)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);

  self->hold++;

  g_clear_handle_id (&self->release_in_idle, g_source_remove);

  return g_object_ref (self);
}

static gboolean
ide_buffer_release_in_idle (gpointer data)
{
  IdeBuffer *self = data;
  IdeObjectBox *box;

  g_assert (IDE_IS_BUFFER (self));

  self->release_in_idle = 0;

  if ((box = ide_object_box_from_object (G_OBJECT (self))))
    ide_object_destroy (IDE_OBJECT (box));

  return G_SOURCE_REMOVE;
}

/**
 * ide_buffer_release:
 * @self: a #IdeBuffer
 *
 * Releases the "hold count" on a buffer.
 *
 * The buffer will be destroyed and unloaded when the hold count
 * reaches zero.
 */
void
ide_buffer_release (IdeBuffer *self)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_BUFFER (self));
  g_return_if_fail (self->hold > 0);

  self->hold--;

  if (self->hold == 0)
    {
      g_assert (self->release_in_idle == 0);
      self->release_in_idle =
        g_idle_add_full (G_MAXINT,
                         ide_buffer_release_in_idle,
                         self,
                         g_object_unref);
      return;
    }

  g_object_unref (self);
}

IdeExtensionSetAdapter *
_ide_buffer_get_addins (IdeBuffer *self)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);

  return self->addins;
}

void
_ide_buffer_line_flags_changed (IdeBuffer *self)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_BUFFER (self));

  g_signal_emit (self, signals [LINE_FLAGS_CHANGED], 0);
}

/**
 * ide_buffer_has_symbol_resolvers:
 * @self: a #IdeBuffer
 *
 * Checks if any symbol resolvers are available.
 *
 * Returns: %TRUE if at least one symbol resolvers is available
 */
gboolean
ide_buffer_has_symbol_resolvers (IdeBuffer *self)
{
  g_return_val_if_fail (IDE_IS_BUFFER (self), FALSE);

  return self->symbol_resolvers != NULL &&
         ide_extension_set_adapter_get_n_extensions (self->symbol_resolvers) > 0;
}

static void
settle_cb (GObject      *object,
           GAsyncResult *result,
           gpointer      user_data)
{
  IdeBufferAddin *addin = (IdeBufferAddin *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  gint *n_active;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER_ADDIN (addin));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  n_active = ide_task_get_task_data (task);

  if (!ide_buffer_addin_settle_finish (addin, result, &error))
    g_warning ("Buffer addin \"%s\" failed to settle: %s",
               G_OBJECT_TYPE_NAME (addin),
               error->message);

  (*n_active)--;

  if (*n_active == 0)
    ide_task_return_boolean (task, TRUE);
}

static void
settle_foreach_cb (IdeExtensionSetAdapter *set,
                   PeasPluginInfo         *plugin_info,
                   GObject          *extension,
                   gpointer                user_data)
{
  IdeBufferAddin *addin = (IdeBufferAddin *)extension;
  IdeTask *task = user_data;
  gint *n_active;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_BUFFER_ADDIN (addin));
  g_assert (IDE_IS_TASK (task));

  n_active = ide_task_get_task_data (task);

  (*n_active)++;

  ide_buffer_addin_settle_async (addin,
                                 ide_task_get_cancellable (task),
                                 settle_cb,
                                 g_object_ref (task));
}

static void
settle_async (IdeBuffer           *self,
              GCancellable        *cancellable,
              GAsyncReadyCallback  callback,
              gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  gint *n_active;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  n_active = g_new0 (gint, 1);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, settle_async);
  ide_task_set_task_data (task, n_active, g_free);

  if (self->addins != NULL && self->enable_addins)
    ide_extension_set_adapter_foreach (self->addins,
                                       settle_foreach_cb,
                                       task);

  if (*n_active == 0)
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static gboolean
settle_finish (IdeBuffer     *self,
               GAsyncResult  *result,
               GError       **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER (self));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

void
_ide_buffer_request_scroll_to_cursor (IdeBuffer *self)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_BUFFER (self));

  g_signal_emit (self, signals [REQUEST_SCROLL_TO_INSERT], 0);
}

gboolean
_ide_buffer_is_file (IdeBuffer *self,
                     GFile     *nolink_file)
{
  g_return_val_if_fail (IDE_IS_BUFFER (self), FALSE);
  g_return_val_if_fail (G_IS_FILE (nolink_file), FALSE);

  return g_file_equal (nolink_file, ide_buffer_get_file (self)) ||
         g_file_equal (nolink_file, self->readlink_file);
}

const char *
ide_buffer_get_charset (IdeBuffer *self)
{
  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);

  return self->encoding
         ? gtk_source_encoding_get_charset (self->encoding)
         : "UTF-8";
}

void
ide_buffer_set_charset (IdeBuffer  *self,
                        const char *charset)
{
  const GtkSourceEncoding *encoding = NULL;
  GSList *all;

  g_return_if_fail (IDE_IS_BUFFER (self));

  if (charset == NULL || charset[0] == 0)
    charset = "UTF-8";

  all = gtk_source_encoding_get_all ();

  for (const GSList *iter = all; iter; iter = iter->next)
    {
      const GtkSourceEncoding *candidate_encoding = iter->data;

      if (g_strcmp0 (charset, gtk_source_encoding_get_charset (candidate_encoding)) == 0)
        {
          encoding = candidate_encoding;
          break;
        }
    }

  if (self->encoding != encoding)
    {
      self->encoding = encoding;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CHARSET]);

      /* We changed encoding and the buffer is not modified. We should just
       * reload the file using the new encoding so that the user can see the
       * document as they expected it to.
       */
      if (!gtk_text_buffer_get_modified (GTK_TEXT_BUFFER (self)))
        _ide_buffer_load_file_async (self, NULL, NULL, NULL, NULL);
    }

  g_slist_free (all);
}

GtkSourceNewlineType
ide_buffer_get_newline_type (IdeBuffer *self)
{
  g_return_val_if_fail (IDE_IS_BUFFER (self), GTK_SOURCE_NEWLINE_TYPE_DEFAULT);

  return self->newline_type;
}

void
ide_buffer_set_newline_type (IdeBuffer            *self,
                             GtkSourceNewlineType  newline_type)
{
  g_return_if_fail (IDE_IS_BUFFER (self));
  g_return_if_fail (newline_type == GTK_SOURCE_NEWLINE_TYPE_LF ||
                    newline_type == GTK_SOURCE_NEWLINE_TYPE_CR ||
                    newline_type == GTK_SOURCE_NEWLINE_TYPE_CR_LF ||
                    newline_type == GTK_SOURCE_NEWLINE_TYPE_DEFAULT);

  if (newline_type == self->newline_type)
    return;

  self->newline_type = newline_type;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NEWLINE_TYPE]);
}

static void
_ide_buffer_set_has_encoding_error (IdeBuffer *self,
                                    gboolean   has_encoding_error)
{
  g_assert (IDE_IS_BUFFER (self));

  has_encoding_error = !!has_encoding_error;

  if (has_encoding_error != self->has_encoding_error)
    {
      self->has_encoding_error = has_encoding_error;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HAS_ENCODING_ERROR]);
    }
}

/**
 * ide_buffer_has_encoding_error:
 * @self: a #IdeBuffer
 *
 * Gets the "has-encoding-error" property.
 *
 * This returns %TRUE if there was an error loading a file due to a failure
 * to discover the encoding or if character conversions occurred.
 *
 * Returns: %FALSE if there were no encoding errors
 *
 * Since: 44
 */
gboolean
ide_buffer_has_encoding_error (IdeBuffer *self)
{
  g_return_val_if_fail (IDE_IS_BUFFER (self), FALSE);

  return self->has_encoding_error;
}
