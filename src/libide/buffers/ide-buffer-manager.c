/* ide-buffer-manager.c
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

#define G_LOG_DOMAIN "ide-buffer-manager"

#include <dazzle.h>
#include <gtksourceview/gtksource.h>
#include <glib/gi18n.h>

#include "ide-context.h"
#include "ide-debug.h"
#include "ide-global.h"

#include "buffers/ide-buffer-manager.h"
#include "buffers/ide-buffer.h"
#include "buffers/ide-buffer-private.h"
#include "buffers/ide-unsaved-files.h"
#include "diagnostics/ide-diagnostics-manager.h"
#include "diagnostics/ide-source-location.h"
#include "diagnostics/ide-source-range.h"
#include "files/ide-file-settings.h"
#include "files/ide-file.h"
#include "projects/ide-project-edit.h"
#include "projects/ide-project-edit-private.h"
#include "sourceview/ide-completion-words.h"
#include "util/ide-doc-seq.h"
#include "util/ide-progress.h"
#include "vcs/ide-vcs.h"

#define AUTO_SAVE_TIMEOUT_DEFAULT    60
#define MAX_FILE_SIZE_BYTES_DEFAULT  (1024UL * 1024UL * 10UL)

struct _IdeBufferManager
{
  IdeObject                 parent_instance;

  GPtrArray                *buffers;
  GHashTable               *timeouts;
  IdeBuffer                *focus_buffer;
  GtkSourceCompletionWords *word_completion;
  GSettings                *settings;
  GHashTable               *loading;

  gsize                     max_file_size;

  guint                     auto_save_timeout;
  guint                     auto_save : 1;
};

typedef struct
{
  IdeBufferManager *self;
  IdeBuffer        *buffer;
  guint             source_id;
} AutoSave;

typedef struct
{
  IdeBuffer            *buffer;
  IdeFile              *file;
  IdeProgress          *progress;
  GtkSourceFileLoader  *loader;
  guint                 is_new : 1;
  IdeWorkbenchOpenFlags flags;
  guint                 line;
  guint                 line_offset;
} LoadState;

typedef struct
{
  IdeBuffer   *buffer;
  IdeFile     *file;
  IdeProgress *progress;
} SaveState;

typedef struct
{
  GPtrArray  *edits;
  GHashTable *buffers;
  guint       count;
  guint       failed : 1;
} EditState;

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdeBufferManager, ide_buffer_manager, IDE_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

DZL_DEFINE_COUNTER (registered, "IdeBufferManager", "Registered Buffers",
                    "The number of buffers registered with the buffer manager.")

enum {
  PROP_0,
  PROP_AUTO_SAVE,
  PROP_AUTO_SAVE_TIMEOUT,
  PROP_FOCUS_BUFFER,
  PROP_MINIMUM_WORD_SIZE,
  LAST_PROP
};

enum {
  CREATE_BUFFER,

  SAVE_BUFFER,
  BUFFER_SAVED,

  LOAD_BUFFER,
  BUFFER_LOADED,
  BUFFER_UNLOADED,

  BUFFER_FOCUS_ENTER,
  BUFFER_FOCUS_LEAVE,

  LAST_SIGNAL
};

static void register_auto_save   (IdeBufferManager *self,
                                  IdeBuffer        *buffer);
static void unregister_auto_save (IdeBufferManager *self,
                                  IdeBuffer        *buffer);

static GParamSpec *properties [LAST_PROP];
static guint signals [LAST_SIGNAL];

static void
edit_state_free (gpointer data)
{
  EditState *state = data;

  if (state != NULL)
    {
      g_clear_pointer (&state->edits, g_ptr_array_unref);
      g_clear_pointer (&state->buffers, g_hash_table_unref);
      g_slice_free (EditState, state);
    }
}

static void
save_state_free (gpointer data)
{
  SaveState *state = data;

  if (state)
    {
      g_clear_object (&state->buffer);
      g_clear_object (&state->file);
      g_clear_object (&state->progress);
      g_slice_free (SaveState, state);
    }
}

static void
load_state_free (gpointer data)
{
  LoadState *state = data;

  if (state)
    {
      g_clear_object (&state->buffer);
      g_clear_object (&state->file);
      g_clear_object (&state->progress);
      g_clear_object (&state->loader);
      g_slice_free (LoadState, state);
    }
}

static void
unref_if_non_null (gpointer data)
{
  if (data != NULL)
    g_object_unref (data);
}

/**
 * ide_buffer_manager_get_auto_save_timeout:
 *
 * Gets the value of the #IdeBufferManager:auto-save-timeout property.
 *
 * Returns: The timeout in seconds if enabled, otherwise 0.
 */
guint
ide_buffer_manager_get_auto_save_timeout (IdeBufferManager *self)
{
  g_return_val_if_fail (IDE_IS_BUFFER_MANAGER (self), 0);

  if (self->auto_save)
    return self->auto_save_timeout;

  return 0;
}

/**
 * ide_buffer_manager_set_auto_save_timeout:
 * @auto_save_timeout: The auto save timeout in seconds.
 *
 * Sets the #IdeBufferManager:auto-save-timeout property.
 *
 * You can set this property to 0 to use the default timeout.
 *
 * This is the number of seconds to wait after a buffer has been changed before
 * automatically saving the buffer.
 */
void
ide_buffer_manager_set_auto_save_timeout (IdeBufferManager *self,
                                          guint             auto_save_timeout)
{
  g_return_if_fail (IDE_IS_BUFFER_MANAGER (self));

  if (!auto_save_timeout)
    auto_save_timeout = AUTO_SAVE_TIMEOUT_DEFAULT;

  if (self->auto_save_timeout != auto_save_timeout)
    {
      self->auto_save_timeout = auto_save_timeout;
      g_object_notify_by_pspec (G_OBJECT (self),
                                properties [PROP_AUTO_SAVE_TIMEOUT]);
    }
}

/**
 * ide_buffer_manager_get_auto_save:
 *
 * Gets the #IdeBufferManager:auto-save property.
 *
 * If auto-save is enabled, then buffers managed by @self will be automatically
 * persisted #IdeBufferManager:auto-save-timeout seconds after their last
 * change.
 *
 * Returns: %TRUE if auto save is enabled. otherwise %FALSE.
 */
gboolean
ide_buffer_manager_get_auto_save (IdeBufferManager *self)
{
  g_return_val_if_fail (IDE_IS_BUFFER_MANAGER (self), FALSE);

  return self->auto_save;
}

/**
 * ide_buffer_manager_set_auto_save:
 * @auto_save: %TRUE if auto-save should be enabled.
 *
 * Sets the #IdeBufferManager:auto-save property. If this is %TRUE, then a
 * buffer will automatically be saved after #IdeBufferManager:auto-save-timeout
 * seconds have elapsed since the buffer's last modification.
 */
void
ide_buffer_manager_set_auto_save (IdeBufferManager *self,
                                  gboolean          auto_save)
{
  g_return_if_fail (IDE_IS_BUFFER_MANAGER (self));

  auto_save = !!auto_save;

  if (self->auto_save != auto_save)
    {
      gsize i;

      self->auto_save = auto_save;

      for (i = 0; i < self->buffers->len; i++)
        {
          IdeBuffer *buffer;

          buffer = g_ptr_array_index (self->buffers, i);

          if (auto_save)
            register_auto_save (self, buffer);
          else
            unregister_auto_save (self, buffer);
        }

      g_object_notify_by_pspec (G_OBJECT (self),
                                properties [PROP_AUTO_SAVE]);
    }
}

/**
 * ide_buffer_manager_get_focus_buffer:
 *
 * Gets the #IdeBufferManager:focus-buffer property. This is the buffer behind
 * the current selected view.
 *
 * Returns: (transfer none): An #IdeBuffer or %NULL.
 */
IdeBuffer *
ide_buffer_manager_get_focus_buffer (IdeBufferManager *self)
{
  g_return_val_if_fail (IDE_IS_BUFFER_MANAGER (self), NULL);

  return self->focus_buffer;
}

void
ide_buffer_manager_set_focus_buffer (IdeBufferManager *self,
                                     IdeBuffer        *buffer)
{
  IdeBuffer *previous;

  g_return_if_fail (IDE_IS_BUFFER_MANAGER (self));
  g_return_if_fail (!buffer || IDE_IS_BUFFER (buffer));

  previous = self->focus_buffer;

  if (dzl_set_weak_pointer (&self->focus_buffer, buffer))
    {
      /* notify that we left the previous buffer */
      if (previous)
        g_signal_emit (self, signals [BUFFER_FOCUS_LEAVE], 0, previous);

      /* notify of the new buffer, but check for reentrancy */
      if (buffer && (buffer == self->focus_buffer))
        g_signal_emit (self, signals [BUFFER_FOCUS_ENTER], 0, buffer);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FOCUS_BUFFER]);
    }
}

static gboolean
ide_buffer_manager_auto_save_cb (gpointer data)
{
  AutoSave *state = data;
  gboolean saved = FALSE;

  g_assert (state);
  g_assert (IDE_IS_BUFFER_MANAGER (state->self));
  g_assert (IDE_IS_BUFFER (state->buffer));
  g_assert (state->source_id > 0);

  if (!ide_buffer_get_changed_on_volume (state->buffer))
    {
      IdeFile *file = ide_buffer_get_file (state->buffer);

      if (file != NULL)
        {
          ide_buffer_manager_save_file_async (state->self,
                                              state->buffer,
                                              file,
                                              NULL,
                                              NULL,
                                              NULL,
                                              NULL);
          saved = TRUE;
        }
    }

  if (!saved)
    unregister_auto_save (state->self, state->buffer);

  return G_SOURCE_REMOVE;
}


static void
ide_buffer_manager_buffer_changed (IdeBufferManager *self,
                                   IdeBuffer        *buffer)
{
  g_return_if_fail (IDE_IS_BUFFER_MANAGER (self));
  g_return_if_fail (IDE_IS_BUFFER (buffer));

  if (self->auto_save)
    {
      unregister_auto_save (self, buffer);
      register_auto_save (self, buffer);
    }
}

static void
ide_buffer_manager_track_buffer (IdeBufferManager *self,
                                 IdeBuffer        *buffer)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_BUFFER_MANAGER (self));
  g_return_if_fail (IDE_IS_BUFFER (buffer));

  if (self->auto_save)
    register_auto_save (self, buffer);

  gtk_source_completion_words_register (self->word_completion, GTK_TEXT_BUFFER (buffer));

  g_signal_connect_object (buffer,
                           "changed",
                           G_CALLBACK (ide_buffer_manager_buffer_changed),
                           self,
                           (G_CONNECT_SWAPPED | G_CONNECT_AFTER));

  g_list_model_items_changed (G_LIST_MODEL (self), self->buffers->len - 1, 0, 1);

  IDE_EXIT;
}

static void
ide_buffer_manager_save_cursor_position (IdeBufferManager *self,
                                         IdeBuffer        *buffer)
{
  g_autofree gchar *position = NULL;
  g_autoptr(GError) error = NULL;
  IdeFile *file;
  GFile *gfile;
  GtkTextIter iter;

  g_assert (IDE_IS_BUFFER_MANAGER (self));
  g_assert (IDE_IS_BUFFER (buffer));

  /* FIXME: We should have a metadata manager like gedit which is owned by the
   *        context. That would allow us to do this asynchronously and block
   *        on synchronization during unload.
   */

  file = ide_buffer_get_file (buffer);
  if (ide_file_get_is_temporary (file))
    return;

  gfile = ide_file_get_file (file);

  gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer), &iter,
                                    gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (buffer)));
  position = g_strdup_printf ("%u:%u",
                              gtk_text_iter_get_line (&iter),
                              gtk_text_iter_get_line_offset (&iter));

  if (!g_file_set_attribute_string (gfile, IDE_FILE_ATTRIBUTE_POSITION, position, 0, NULL, &error))
    g_warning ("Failed to persist cursor position: %s", error->message);
}

static void
ide_buffer_manager_remove_buffer (IdeBufferManager *self,
                                  IdeBuffer        *buffer)
{
  gint position = -1;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_BUFFER_MANAGER (self));
  g_return_if_fail (IDE_IS_BUFFER (buffer));

  for (guint i = 0; i < self->buffers->len; i++)
    {
      IdeBuffer *ele = g_ptr_array_index (self->buffers, i);

      if (ele == buffer)
        {
          position = i;
          break;
        }
    }

  if (position == -1)
    {
      IDE_TRACE_MSG ("failed to locate buffer %p within buffer manager", buffer);
      IDE_EXIT;
    }

  /* Stash the cursor position for next time we open the file */
  ide_buffer_manager_save_cursor_position (self, buffer);

  /* Stealing ownership from self->buffers */
  g_ptr_array_remove_index (self->buffers, position);

  gtk_source_completion_words_unregister (self->word_completion, GTK_TEXT_BUFFER (buffer));

  unregister_auto_save (self, buffer);

  g_signal_handlers_disconnect_by_func (buffer,
                                        G_CALLBACK (ide_buffer_manager_buffer_changed),
                                        self);

  /*
   * Notify anything that needs a pointer to the buffer to cleanup,
   * such as language server clients.
   */
  g_signal_emit (self, signals [BUFFER_UNLOADED], 0, buffer);

  /* Try hard to ensure the buffer releases any objects held.
   * That way, if for some reason the buffer does get leaked
   * (which we see sometimes with GSV and have yet to track down),
   * we at least drop all the related IDE objects.
   *
   * https://bugzilla.gnome.org/show_bug.cgi?id=766322
   */
  g_signal_emit_by_name (buffer, "destroy");
  g_object_run_dispose (G_OBJECT (buffer));

  /* We stole ownership from self->buffers */
  g_object_unref (buffer);

  DZL_COUNTER_DEC (registered);

  g_list_model_items_changed (G_LIST_MODEL (self), position, 1, 0);

  IDE_EXIT;
}

static IdeBuffer *
ide_buffer_manager_get_buffer (IdeBufferManager *self,
                               IdeFile          *file)
{
  g_return_val_if_fail (IDE_IS_BUFFER_MANAGER (self), NULL);
  g_return_val_if_fail (IDE_IS_FILE (file), NULL);

  for (guint i = 0; i < self->buffers->len; i++)
    {
      IdeBuffer *buffer = g_ptr_array_index (self->buffers, i);
      IdeFile *cur_file = ide_buffer_get_file (buffer);

      if (ide_file_equal (cur_file, file))
        return buffer;
    }

  return NULL;
}

static void
ide_buffer_manager_load_file__load_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  g_autoptr(GTask) task = user_data;
  g_autofree gchar *guess_contents = NULL;
  g_autofree gchar *content_type = NULL;
  GtkSourceFileLoader *loader = (GtkSourceFileLoader *)object;
  IdeBufferManager *self;
  const gchar *path;
  IdeContext *context;
  LoadState *state;
  GtkTextIter iter;
  GtkTextIter end;
  g_autoptr(GError) error = NULL;
  gboolean uncertain = TRUE;

  IDE_ENTRY;

  g_assert (G_IS_TASK (task));
  g_assert (GTK_SOURCE_IS_FILE_LOADER (loader));

  self = g_task_get_source_object (task);
  state = g_task_get_task_data (task);

  g_assert (IDE_IS_BUFFER_MANAGER (self));
  g_assert (IDE_IS_FILE (state->file));
  g_assert (IDE_IS_BUFFER (state->buffer));
  g_assert (IDE_IS_PROGRESS (state->progress));

  context = ide_object_get_context (IDE_OBJECT (self));

  /*
   * Always track the buffer so that we can clean things up
   * properly when teh buffer is disposed.
   */
  if (state->is_new)
    {
      g_ptr_array_add (self->buffers, g_object_ref (state->buffer));
      DZL_COUNTER_INC (registered);
    }

  if (!gtk_source_file_loader_load_finish (loader, result, &error))
    {
      /*
       * It's okay if we fail because the file does not exist yet.
       *
       * TODO: Add "Failed" state to buffers so we can display something
       *       other than the sourceview in the editor perspective.
       */
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        {
          _ide_buffer_set_failure (state->buffer, error);
          g_task_return_error (task, g_steal_pointer (&error));
          IDE_EXIT;
        }

      g_clear_error (&error);
    }

  gtk_text_buffer_set_modified (GTK_TEXT_BUFFER (state->buffer), FALSE);

  if (state->is_new)
    ide_buffer_manager_track_buffer (self, state->buffer);

  /* try to restore the insertion cursor */
  if (g_settings_get_boolean (self->settings, "restore-insert-mark"))
    gtk_text_buffer_get_iter_at_line_offset (GTK_TEXT_BUFFER (state->buffer), &iter,
                                             state->line, state->line_offset);
  else
    gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (state->buffer), &iter);

  if (_ide_buffer_can_restore_cursor (state->buffer))
    {
      IDE_TRACE_MSG ("Restoring insert mark to %u:%u",
                     gtk_text_iter_get_line (&iter) + 1,
                     gtk_text_iter_get_line_offset (&iter) + 1);
      gtk_text_buffer_select_range (GTK_TEXT_BUFFER (state->buffer), &iter, &iter);
    }

  /*
   * Try to discover the content type more accurately now that we have access to the
   * file contents inside of the IdeBuffer.
   */
  gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (state->buffer), &iter);
  end = iter;
  gtk_text_iter_forward_chars (&end, 1024);
  guess_contents = gtk_text_iter_get_slice (&iter, &end);
  path = ide_file_get_path (state->file);
  content_type = g_content_type_guess (path, (const guchar *)guess_contents,
                                       strlen (guess_contents), &uncertain);
  if (content_type && !uncertain)
    _ide_file_set_content_type (state->file, content_type);

  if (!_ide_context_is_restoring (context))
    ide_buffer_manager_set_focus_buffer (self, state->buffer);

  g_signal_emit (self, signals [BUFFER_LOADED], 0, state->buffer);

  g_task_return_pointer (task, g_object_ref (state->buffer), g_object_unref);

  IDE_EXIT;
}

static void
ide_buffer_manager__load_file_query_info_cb (GObject      *object,
                                             GAsyncResult *result,
                                             gpointer      user_data)
{
  IdeBufferManager *self;
  GFile *file = (GFile *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GFileInfo) file_info = NULL;
  g_autoptr(GError) error = NULL;
  LoadState *state;
  gsize size = 0;
  gboolean create_new_view = FALSE;

  IDE_ENTRY;

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_TASK (task));

  state = g_task_get_task_data (task);
  self = g_task_get_source_object (task);

  g_assert (state);
  g_assert (IDE_IS_BUFFER (state->buffer));
  g_assert (IDE_IS_BUFFER_MANAGER (self));

  file_info = g_file_query_info_finish (file, result, &error);

  if (file_info == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        {
          _ide_buffer_set_failure (state->buffer, error);
          g_task_return_error (task, g_steal_pointer (&error));
          IDE_EXIT;
        }
      g_clear_error (&error);
    }
  else
    {
      size = g_file_info_get_attribute_uint64 (file_info, G_FILE_ATTRIBUTE_STANDARD_SIZE);
    }

  if ((self->max_file_size > 0) && (size > self->max_file_size))
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_DATA,
                               _("File too large to be opened."));
      IDE_EXIT;
    }

  if (file_info && g_file_info_has_attribute (file_info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE))
    {
      gboolean read_only;

      read_only = !g_file_info_get_attribute_boolean (file_info,
                                                      G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE);
      _ide_buffer_set_read_only (state->buffer, read_only);
    }

  if (file_info && g_file_info_has_attribute (file_info, G_FILE_ATTRIBUTE_TIME_MODIFIED))
    {
      GTimeVal tv;

      g_file_info_get_modification_time (file_info, &tv);
      _ide_buffer_set_mtime (state->buffer, &tv);
    }

  if (file_info && g_file_info_has_attribute (file_info, IDE_FILE_ATTRIBUTE_POSITION))
    {
      const gchar *attr = g_file_info_get_attribute_string (file_info, IDE_FILE_ATTRIBUTE_POSITION);
      guint line = 0;
      guint line_offset = 0;

      if (attr != NULL && sscanf (attr, "%u:%u", &line, &line_offset) >= 1)
        {
          state->line = line;
          state->line_offset = line_offset;
        }
    }

  if (!(state->flags & IDE_WORKBENCH_OPEN_FLAGS_NO_VIEW))
    if (state->is_new || (state->flags & IDE_WORKBENCH_OPEN_FLAGS_BACKGROUND) == 0)
      create_new_view = TRUE;

  g_signal_emit (self, signals [LOAD_BUFFER], 0, state->buffer, create_new_view);

  gtk_source_file_loader_load_async (state->loader,
                                     G_PRIORITY_LOW,
                                     g_task_get_cancellable (task),
                                     ide_progress_file_progress_callback,
                                     g_object_ref (state->progress),
                                     g_object_unref,
                                     ide_buffer_manager_load_file__load_cb,
                                     g_object_ref (task));

  IDE_EXIT;
}

static void
ide_buffer_manager__load_file_read_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  GFile *file = (GFile *)object;
  g_autoptr(GFileInputStream) stream = NULL;
  g_autoptr(GTask) task = user_data;
  GtkSourceFile *source_file;
  LoadState *state;

  IDE_ENTRY;

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_TASK (task));

  state = g_task_get_task_data (task);

  g_assert (state);
  g_assert (IDE_IS_BUFFER (state->buffer));

  source_file = _ide_file_get_source_file (state->file);

  stream = g_file_read_finish (file, result, NULL);

  if (stream)
    state->loader = gtk_source_file_loader_new_from_stream (GTK_SOURCE_BUFFER (state->buffer),
                                                            source_file,
                                                            G_INPUT_STREAM (stream));
  else
    state->loader = gtk_source_file_loader_new (GTK_SOURCE_BUFFER (state->buffer), source_file);

  g_file_query_info_async (file,
                           G_FILE_ATTRIBUTE_STANDARD_SIZE","
                           G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE","
                           G_FILE_ATTRIBUTE_TIME_MODIFIED","
                           IDE_FILE_ATTRIBUTE_POSITION,
                           G_FILE_QUERY_INFO_NONE,
                           G_PRIORITY_DEFAULT,
                           g_task_get_cancellable (task),
                           ide_buffer_manager__load_file_query_info_cb,
                           g_object_ref (task));

  IDE_EXIT;
}

static void
ide_buffer_manager_load_task_completed (IdeBufferManager *self,
                                        GParamSpec       *pspec,
                                        GTask            *task)
{
  LoadState *state;

  g_assert (IDE_IS_BUFFER_MANAGER (self));
  g_assert (pspec != NULL);
  g_assert (g_strcmp0 ("completed", pspec->name) == 0);
  g_assert (G_IS_TASK (task));

  state = g_task_get_task_data (task);
  g_assert (state != NULL);
  g_assert (state->file != NULL);
  g_assert (IDE_IS_FILE (state->file));

  if (self->loading != NULL)
    g_hash_table_remove (self->loading, state->file);
}

/**
 * ide_buffer_manager_load_file_async:
 * @progress: (out) (nullable): A location for an #IdeProgress or %NULL.
 *
 * Asynchronously requests that the file represented by @file is loaded. If the file is already
 * loaded, the previously loaded version of the file will be returned, asynchronously.
 *
 * Before loading the file, #IdeBufferManager will check the file size to help protect itself
 * from the user accidentally loading very large files. You can change the maximum size of file
 * that will be loaded with the #IdeBufferManager:max-file-size property.
 *
 * See ide_buffer_manager_load_file_finish() for how to complete this asynchronous request.
 */
void
ide_buffer_manager_load_file_async (IdeBufferManager       *self,
                                    IdeFile                *file,
                                    gboolean                force_reload,
                                    IdeWorkbenchOpenFlags   flags,
                                    IdeProgress           **progress,
                                    GCancellable           *cancellable,
                                    GAsyncReadyCallback     callback,
                                    gpointer                user_data)
{
  g_autoptr(GTask) task = NULL;
  IdeContext *context;
  IdeBuffer *buffer;
  LoadState *state;
  GFile *gfile;

  IDE_ENTRY;

  if (progress)
    *progress = NULL;

  g_return_if_fail (IDE_IS_BUFFER_MANAGER (self));
  g_return_if_fail (IDE_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_priority (task, G_PRIORITY_LOW);
  g_task_set_source_tag (task, ide_buffer_manager_load_file_async);

  if (g_hash_table_contains (self->loading, file))
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_BUSY,
                               "The file is already loading");
      IDE_EXIT;
    }

  context = ide_object_get_context (IDE_OBJECT (self));
  ide_context_hold_for_object (context, task);

  buffer = ide_buffer_manager_get_buffer (self, file);

  /*
   * If the buffer is already loaded, then we can complete the request immediately.
   */
  if (buffer && !force_reload)
    {
      if (progress)
        *progress = g_object_new (IDE_TYPE_PROGRESS,
                                  "fraction", 1.0,
                                  NULL);
      g_task_return_pointer (task, g_object_ref (buffer), g_object_unref);
      if (!(flags & IDE_WORKBENCH_OPEN_FLAGS_BACKGROUND || flags & IDE_WORKBENCH_OPEN_FLAGS_NO_VIEW))
        ide_buffer_manager_set_focus_buffer (self, buffer);

      IDE_EXIT;
    }

  state = g_slice_new0 (LoadState);
  state->is_new = (buffer == NULL);
  state->file = g_object_ref (file);
  state->progress = ide_progress_new ();
  state->flags = flags;

  if (buffer)
    {
      state->buffer = g_object_ref (buffer);
    }
  else
    {
      /*
       * Allow application to specify the buffer instance which may be a
       * descendant of IdeBuffer.
       */
      g_signal_emit (self, signals [CREATE_BUFFER], 0, file, &state->buffer);

      if ((state->buffer != NULL) && !IDE_IS_BUFFER (state->buffer))
        {
          g_warning ("Invalid buffer type retrieved from create-buffer signal.");
          state->buffer = NULL;
        }

      if (state->buffer == NULL)
        state->buffer = g_object_new (IDE_TYPE_BUFFER,
                                      "context", context,
                                      "file", file,
                                      NULL);
    }

  _ide_buffer_set_mtime (state->buffer, NULL);
  _ide_buffer_set_changed_on_volume (state->buffer, FALSE);

  g_task_set_task_data (task, state, load_state_free);

  g_hash_table_insert (self->loading,
                       g_object_ref (file),
                       g_object_ref (state->buffer));

  g_signal_connect_object (task,
                           "notify::completed",
                           G_CALLBACK (ide_buffer_manager_load_task_completed),
                           self,
                           G_CONNECT_SWAPPED);

  if (progress)
    *progress = g_object_ref (state->progress);

  gfile = ide_file_get_file (file);

  g_file_read_async (gfile,
                     G_PRIORITY_DEFAULT,
                     cancellable,
                     ide_buffer_manager__load_file_read_cb,
                     g_steal_pointer (&task));

  IDE_EXIT;
}

/**
 * ide_buffer_manager_load_file_finish:
 *
 * Completes an asynchronous request to load a file via ide_buffer_manager_load_file_async().
 * If the buffer was already loaded, this function will return a reference to the previous buffer
 * with its reference count incremented by one.
 *
 * Returns: (transfer full): An #IdeBuffer if successful; otherwise %NULL and @error is set.
 */
IdeBuffer *
ide_buffer_manager_load_file_finish (IdeBufferManager  *self,
                                     GAsyncResult      *result,
                                     GError           **error)
{
  IdeBuffer *ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_BUFFER_MANAGER (self), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  ret = g_task_propagate_pointer (G_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_buffer_manager__buffer_reload_mtime_cb (GObject      *object,
                                            GAsyncResult *result,
                                            gpointer      user_data)
{
  GFile *file = (GFile *)object;
  g_autoptr(GFileInfo) file_info = NULL;
  g_autoptr(GTask) task = user_data;
  SaveState *state;

  g_assert (G_IS_TASK (task));

  state = g_task_get_task_data (task);
  g_assert (state != NULL);
  g_assert (IDE_IS_BUFFER (state->buffer));

  if ((file_info = g_file_query_info_finish (file, result, NULL)))
    {
      GTimeVal tv;

      g_file_info_get_modification_time (file_info, &tv);
      _ide_buffer_set_mtime (state->buffer, &tv);
    }

  _ide_buffer_set_changed_on_volume (state->buffer, FALSE);

  g_task_return_boolean (task, TRUE);
}

static void
ide_buffer_manager_save_file__save_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  GtkSourceFileSaver *saver = (GtkSourceFileSaver *)object;
  IdeBufferManager *self;
  IdeUnsavedFiles *unsaved_files;
  IdeContext *context;
  IdeDiagnosticsManager *diagnostics_manager;
  IdeFile *file;
  GFile *gfile;
  GFile *old_gfile;
  SaveState *state;

  g_assert (GTK_SOURCE_IS_FILE_SAVER (saver));
  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  state = g_task_get_task_data (task);

  g_assert (IDE_IS_BUFFER_MANAGER (self));
  g_assert (state);
  g_assert (IDE_IS_BUFFER (state->buffer));
  g_assert (IDE_IS_FILE (state->file));
  g_assert (IDE_IS_PROGRESS (state->progress));

  /* Complete the save request */
  if (!gtk_source_file_saver_save_finish (saver, result, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  /* Mark the buffer as not modified if we saved it to the backing file */
  file = ide_buffer_get_file (state->buffer);
  if (ide_file_equal (file, state->file))
    gtk_text_buffer_set_modified (GTK_TEXT_BUFFER (state->buffer), FALSE);

  /* Remove the unsaved files state */
  context = ide_object_get_context (IDE_OBJECT (self));
  unsaved_files = ide_context_get_unsaved_files (context);
  old_gfile = ide_file_get_file (file);
  ide_unsaved_files_remove (unsaved_files, old_gfile);

  /* Update the buffer and the diagnostics manager with the backing file
   * so that there are in sync before signals and properties dispatch.
   */
  gfile = ide_file_get_file (state->file);
  diagnostics_manager = ide_context_get_diagnostics_manager (context);
  ide_diagnostics_manager_update_group_by_file (diagnostics_manager, state->buffer, gfile);
  ide_buffer_set_file (state->buffer, state->file);

  /* Notify signal handlers that the file is saved */
  g_signal_emit (self, signals [BUFFER_SAVED], 0, state->buffer);
  g_signal_emit_by_name (state->buffer, "saved");

  /* Reload the mtime for the buffer */
  g_file_query_info_async (gfile,
                           G_FILE_ATTRIBUTE_TIME_MODIFIED,
                           G_FILE_QUERY_INFO_NONE,
                           G_PRIORITY_DEFAULT,
                           g_task_get_cancellable (task),
                           ide_buffer_manager__buffer_reload_mtime_cb,
                           g_object_ref (task));
}

static void
ide_buffer_manager_save_file__load_settings_cb (GObject      *object,
                                                GAsyncResult *result,
                                                gpointer      user_data)
{
  IdeFile *file = (IdeFile *)object;
  g_autoptr(IdeFileSettings) file_settings = NULL;
  g_autoptr(GTask) task = user_data;
  SaveState *state;
  GtkSourceFileSaver *saver;
  GtkSourceFile *source_file;
  GtkSourceNewlineType newline_type;
  const GtkSourceEncoding *encoding;
  const gchar *charset;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_FILE (file));
  g_assert (G_IS_TASK (task));

  source_file = _ide_file_get_source_file (file);
  state = g_task_get_task_data (task);

  file_settings = ide_file_load_settings_finish (file, result, &error);

  if (file_settings == NULL)
    {
      _ide_buffer_set_failure (state->buffer, error);
      g_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  g_assert (GTK_SOURCE_IS_FILE (source_file));
  g_assert (IDE_IS_BUFFER (state->buffer));
  g_assert (IDE_IS_FILE (state->file));
  g_assert (IDE_IS_PROGRESS (state->progress));

  if (!gtk_source_file_get_location (source_file))
    gtk_source_file_set_location (source_file, ide_file_get_file (file));

  saver = gtk_source_file_saver_new (GTK_SOURCE_BUFFER (state->buffer), source_file);

  /*
   * XXX: We need to think this through a bit more, but I think since the UI allows us to
   *      Reload or Ignore changes, it is safe to always set this here. However, there is the
   *      ever so slightest race condition between the mtime check and the save anyway.
   */
  gtk_source_file_saver_set_flags (saver, GTK_SOURCE_FILE_SAVER_FLAGS_IGNORE_MODIFICATION_TIME);

  /* Set file encoding and newline style defaults */
  newline_type = ide_file_settings_get_newline_type (file_settings);
  encoding = gtk_source_encoding_get_utf8 ();

  if ((charset = ide_file_settings_get_encoding (file_settings)))
    {
      encoding = gtk_source_encoding_get_from_charset (charset);
      if (!encoding)
        encoding = gtk_source_encoding_get_utf8 ();
    }

  /*
   * If we are performing a save-as operation, overwrite the defaults to match what was used
   * in the original source file.
   */
  if (!ide_file_equal (file, ide_buffer_get_file (state->buffer)))
    {
      IdeFile *orig_file = ide_buffer_get_file (state->buffer);

      if (orig_file)
        {
          source_file = _ide_file_get_source_file (orig_file);

          if (source_file)
            {
              encoding = gtk_source_file_get_encoding (source_file);
              newline_type = gtk_source_file_get_newline_type (source_file);
            }
        }
    }

  /*
   * If file-settings dictate that we should trim trailing whitespace, trim it from the modified
   * lines in the IdeBuffer. This is performed automatically based on line state within
   * ide_buffer_trim_trailing_whitespace().
   */
  if (ide_file_settings_get_trim_trailing_whitespace (file_settings))
    ide_buffer_trim_trailing_whitespace (state->buffer);

  gtk_source_file_saver_set_encoding (saver, encoding);
  gtk_source_file_saver_set_newline_type (saver, newline_type);

  _ide_buffer_set_mtime (state->buffer, NULL);

  gtk_source_file_saver_save_async (saver,
                                    G_PRIORITY_DEFAULT,
                                    g_task_get_cancellable (task),
                                    ide_progress_file_progress_callback,
                                    g_object_ref (state->progress),
                                    g_object_unref,
                                    ide_buffer_manager_save_file__save_cb,
                                    g_object_ref (task));

  g_clear_object (&saver);

  IDE_EXIT;
}

/**
 * ide_buffer_manager_save_file_async:
 *
 * This function asynchronously requests that a buffer be saved to the storage represented by
 * @file. @buffer should be a previously loaded buffer owned by @self, such as one loaded with
 * ide_buffer_manager_load_file_async().
 *
 * Call ide_buffer_manager_save_file_finish() to complete this asynchronous request.
 */
void
ide_buffer_manager_save_file_async (IdeBufferManager     *self,
                                    IdeBuffer            *buffer,
                                    IdeFile              *file,
                                    IdeProgress         **progress,
                                    GCancellable         *cancellable,
                                    GAsyncReadyCallback   callback,
                                    gpointer              user_data)
{
  g_autoptr(GTask) task = NULL;
  IdeContext *context;
  SaveState *state;

  if (progress)
    *progress = NULL;

  g_return_if_fail (IDE_IS_BUFFER_MANAGER (self));
  g_return_if_fail (IDE_IS_BUFFER (buffer));
  g_return_if_fail (IDE_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  context = ide_object_get_context (IDE_OBJECT (self));
  ide_context_hold_for_object (context, task);

  state = g_slice_new0 (SaveState);
  state->file = g_object_ref (file);
  state->buffer = g_object_ref (buffer);
  state->progress = ide_progress_new ();

  g_task_set_task_data (task, state, save_state_free);

  g_signal_emit (self, signals [SAVE_BUFFER], 0, buffer);

  if (progress)
    *progress = g_object_ref (state->progress);

  /*
   * First, we need to asynchronously load the file settings. The IdeFileSettings contains the
   * target encoding (utf-8, etc) as well as the newline style (\r\n vs \r vs \n). If the
   * file settings do not dictate an encoding, the encoding used to load the buffer will be used.
   */
  ide_file_load_settings_async (file,
                                cancellable,
                                ide_buffer_manager_save_file__load_settings_cb,
                                g_object_ref (task));

  unregister_auto_save (self, state->buffer);
}

/**
 * ide_buffer_manager_save_file_finish:
 *
 * This function completes an asynchronous request to save a buffer to storage using
 * ide_buffer_manager_save_file_async(). Upon failure, %FALSE is returned and @error is set.
 *
 * Returns: %TRUE if successful %FALSE upon failure and @error is set.
 */
gboolean
ide_buffer_manager_save_file_finish (IdeBufferManager  *self,
                                     GAsyncResult      *result,
                                     GError           **error)
{
  g_return_val_if_fail (IDE_IS_BUFFER_MANAGER (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
ide_buffer_manager_real_buffer_loaded (IdeBufferManager *self,
                                       IdeBuffer        *buffer)
{
  g_autofree gchar *uri = NULL;
  g_autofree gchar *app_exec = NULL;
  GtkRecentManager *recent_manager;
  IdeContext *context;
  GtkRecentData recent_data = { 0 };
  IdeFile *file;
  GFile *gfile;

  g_assert (IDE_IS_BUFFER_MANAGER (self));
  g_assert (IDE_IS_BUFFER (buffer));

  file = ide_buffer_get_file (buffer);
  if (ide_file_get_is_temporary (file))
    return;

  context = ide_object_get_context (IDE_OBJECT (self));
  recent_manager = ide_context_get_recent_manager (context);

  gfile = ide_file_get_file (file);
  uri = g_file_get_uri (gfile);
  app_exec = g_strdup_printf ("%s %%f", ide_get_program_name ());

  recent_data.display_name = NULL;
  recent_data.description = NULL;
  recent_data.mime_type = (gchar *)_ide_file_get_content_type (file);
  recent_data.app_name = (gchar *)ide_get_program_name ();
  recent_data.app_exec = app_exec;
  recent_data.groups = NULL;
  recent_data.is_private = FALSE;

  gtk_recent_manager_add_full (recent_manager, uri, &recent_data);

  _ide_buffer_set_loading (buffer, FALSE);
}

static IdeBuffer *
ide_buffer_manager_real_create_buffer (IdeBufferManager *self,
                                       IdeFile          *file)
{
  IdeContext *context;

  g_return_val_if_fail (IDE_IS_BUFFER_MANAGER (self), NULL);
  g_return_val_if_fail (IDE_IS_FILE (file), NULL);

  context = ide_object_get_context (IDE_OBJECT (self));

  return g_object_new (IDE_TYPE_BUFFER,
                       "context", context,
                       "file", file,
                       NULL);
}

static void
ide_buffer_manager_real_buffer_unloaded (IdeBufferManager *self,
                                         IdeBuffer        *buffer)
{
  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER_MANAGER (self));
  g_assert (IDE_IS_BUFFER (buffer));

  /*
   * If this buffer is not modified, then we want to release any
   * drafts associated with it as they are not necessary anymore.
   */

  if (!gtk_text_buffer_get_modified (GTK_TEXT_BUFFER (buffer)))
    {
      IdeUnsavedFiles *unsaved_files;
      IdeContext *context;
      IdeFile *file;
      GFile *gfile;

      context = ide_buffer_get_context (buffer);
      unsaved_files = ide_context_get_unsaved_files (context);

      if (NULL != (file = ide_buffer_get_file (buffer)) &&
          NULL != (gfile = ide_file_get_file (file)))
        ide_unsaved_files_remove (unsaved_files, gfile);
    }

  IDE_EXIT;
}

static GType
ide_buffer_manager_get_item_type (GListModel *self)
{
  return IDE_TYPE_BUFFER;
}

static guint
ide_buffer_manager_get_n_items (GListModel *model)
{
  IdeBufferManager *self = (IdeBufferManager *)model;

  g_return_val_if_fail (IDE_IS_BUFFER_MANAGER (self), 0);

  return self->buffers->len;
}

static gpointer
ide_buffer_manager_get_item (GListModel *model,
                             guint       position)
{
  IdeBufferManager *self = (IdeBufferManager *)model;

  g_return_val_if_fail (IDE_IS_BUFFER_MANAGER (self), NULL);
  g_return_val_if_fail (position < self->buffers->len, NULL);

  return g_object_ref (g_ptr_array_index (self->buffers, position));
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = ide_buffer_manager_get_item_type;
  iface->get_n_items = ide_buffer_manager_get_n_items;
  iface->get_item = ide_buffer_manager_get_item;
}

static void
ide_buffer_manager_dispose (GObject *object)
{
  IdeBufferManager *self = (IdeBufferManager *)object;

  dzl_clear_weak_pointer (&self->focus_buffer);

  while (self->buffers->len)
    {
      IdeBuffer *buffer;

      buffer = g_ptr_array_index (self->buffers, 0);
      ide_buffer_manager_remove_buffer (self, buffer);
    }

  g_clear_object (&self->word_completion);

  G_OBJECT_CLASS (ide_buffer_manager_parent_class)->dispose (object);
}

static void
ide_buffer_manager_finalize (GObject *object)
{
  IdeBufferManager *self = (IdeBufferManager *)object;

  if (g_hash_table_size (self->timeouts))
    g_warning ("Not all auto save timeouts have been removed.");

  if (self->buffers->len)
    g_warning ("Not all buffers have been destroyed.");

  g_clear_pointer (&self->buffers, g_ptr_array_unref);
  g_clear_pointer (&self->loading, g_hash_table_unref);
  g_clear_pointer (&self->timeouts, g_hash_table_unref);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (ide_buffer_manager_parent_class)->finalize (object);
}

static void
ide_buffer_manager_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  IdeBufferManager *self = IDE_BUFFER_MANAGER (object);

  switch (prop_id)
    {
    case PROP_AUTO_SAVE:
      g_value_set_boolean (value, ide_buffer_manager_get_auto_save (self));
      break;

    case PROP_AUTO_SAVE_TIMEOUT:
      g_value_set_uint (value, ide_buffer_manager_get_auto_save_timeout (self));
      break;

    case PROP_FOCUS_BUFFER:
      g_value_set_object (value, ide_buffer_manager_get_focus_buffer (self));
      break;

    case PROP_MINIMUM_WORD_SIZE:
      g_object_get_property (G_OBJECT (self->word_completion), "minimum-word-size", value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_buffer_manager_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  IdeBufferManager *self = IDE_BUFFER_MANAGER (object);

  switch (prop_id)
    {
    case PROP_AUTO_SAVE:
      ide_buffer_manager_set_auto_save (self, g_value_get_boolean (value));
      break;

    case PROP_AUTO_SAVE_TIMEOUT:
      ide_buffer_manager_set_auto_save_timeout (self, g_value_get_uint (value));
      break;

    case PROP_FOCUS_BUFFER:
      ide_buffer_manager_set_focus_buffer (self, g_value_get_object (value));
      break;

    case PROP_MINIMUM_WORD_SIZE:
      g_object_set_property (G_OBJECT (self->word_completion), "minimum-word-size", value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_buffer_manager_class_init (IdeBufferManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_buffer_manager_dispose;
  object_class->finalize = ide_buffer_manager_finalize;
  object_class->get_property = ide_buffer_manager_get_property;
  object_class->set_property = ide_buffer_manager_set_property;

  properties [PROP_AUTO_SAVE] =
    g_param_spec_boolean ("auto-save",
                          "Auto Save",
                          "If the documents should auto save after a configured timeout.",
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_AUTO_SAVE_TIMEOUT] =
    g_param_spec_uint ("auto-save-timeout",
                       "Auto Save Timeout",
                       "The number of seconds after modification before auto saving.",
                       0,
                       G_MAXUINT,
                       AUTO_SAVE_TIMEOUT_DEFAULT,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_FOCUS_BUFFER] =
    g_param_spec_object ("focus-buffer",
                         "Focused Buffer",
                         "The currently focused buffer.",
                         IDE_TYPE_BUFFER,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_MINIMUM_WORD_SIZE] =
    g_param_spec_uint ("minimum-word-size",
                       "Minimum Word Size",
                       "The minimum word size for word completion.",
                       0,
                       G_MAXUINT,
                       0,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  /**
   * IdeBufferManager::create-buffer:
   * @self: An #IdeBufferManager
   * @file: An #IdeFile
   *
   * This signal is emitted when there is a request to create a new buffer
   * object. This allows subclasses of #IdeBuffer to be instantiated by the
   * buffer manager.
   *
   * The first handler of this signal is responsible for returning an
   * #IdeBuffer or %NULL, for which one will be created.
   *
   * Returns: (transfer full) (nullable): An #IdeBuffer or %NULL.
   */
  signals [CREATE_BUFFER] = g_signal_new_class_handler ("create-buffer",
                                                        G_TYPE_FROM_CLASS (klass),
                                                        G_SIGNAL_RUN_LAST,
                                                        G_CALLBACK (ide_buffer_manager_real_create_buffer),
                                                        g_signal_accumulator_first_wins,
                                                        NULL, NULL,
                                                        IDE_TYPE_BUFFER,
                                                        1,
                                                        IDE_TYPE_FILE);

  /**
   * IdeBufferManager::save-buffer:
   * @self: An #IdeBufferManager.
   * @buffer: an #IdeBuffer.
   *
   * This signal is emitted when a request has been made to save a buffer. Connect to this signal
   * if you'd like to perform mutation of the buffer before it is persisted to storage.
   * Till the "buffer-saved" signal is emmited, the buffer:file property point to the current file.
   */
  signals [SAVE_BUFFER] = g_signal_new ("save-buffer",
                                         G_TYPE_FROM_CLASS (klass),
                                         G_SIGNAL_RUN_LAST,
                                         0,
                                         NULL, NULL, NULL,
                                         G_TYPE_NONE,
                                         1,
                                         IDE_TYPE_BUFFER);

  /**
   * IdeBufferManager::buffer-saved:
   * @self: An #IdeBufferManager.
   * @buffer: an #IdeBuffer.
   *
   * This signal is emitted when a buffer has finished saving to storage. You might connect to
   * this signal if you want to know when the modifications have successfully been written to
   * storage.
   * The buffer:file property point to the saved file.
   */
  signals [BUFFER_SAVED] = g_signal_new ("buffer-saved",
                                          G_TYPE_FROM_CLASS (klass),
                                          G_SIGNAL_RUN_LAST,
                                          0,
                                          NULL, NULL, NULL,
                                          G_TYPE_NONE,
                                          1,
                                          IDE_TYPE_BUFFER);

  /**
   * IdeBufferManager::load-buffer:
   * @self: An #IdeBufferManager.
   * @buffer: An #IdeBuffer.
   * @create_new_view: Whether to create a new #IdeEditorView for the buffer.
   *
   * This signal is emitted when a request has been made to load a buffer from storage. You might
   * connect to this signal to be notified when loading of a buffer has begun.
   *
   * If @create_new_view is %FALSE, then the buffer is probably being force-reloaded due to
   * changes from the host file-system.
   */
  signals [LOAD_BUFFER] = g_signal_new ("load-buffer",
                                        G_TYPE_FROM_CLASS (klass),
                                        G_SIGNAL_RUN_LAST,
                                        0,
                                        NULL, NULL, NULL,
                                        G_TYPE_NONE,
                                        2,
                                        IDE_TYPE_BUFFER,
                                        G_TYPE_BOOLEAN);

  /**
   * IdeBufferManager::buffer-loaded:
   * @self: An #IdeBufferManager.
   * @buffer: an #IdeBuffer.
   *
   * This signal is emitted when a buffer has been successfully loaded. You might connect to this
   * signal to be notified when a buffer has completed loading.
   */
  signals [BUFFER_LOADED] =
    g_signal_new_class_handler ("buffer-loaded",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (ide_buffer_manager_real_buffer_loaded),
                                NULL, NULL, NULL,
                                G_TYPE_NONE,
                                1,
                                IDE_TYPE_BUFFER);

  /**
   * IdeBufferManager::buffer-focus-enter:
   * @self: An #IdeBufferManager.
   * @buffer: an #IdeBuffer.
   *
   * This signal is emitted when a view for @buffer has received focus. You might connect to this
   * signal when you want to perform an operation while a buffer is in focus.
   */
  signals [BUFFER_FOCUS_ENTER] = g_signal_new ("buffer-focus-enter",
                                                G_TYPE_FROM_CLASS (klass),
                                                G_SIGNAL_RUN_LAST,
                                                0,
                                                NULL, NULL, NULL,
                                                G_TYPE_NONE,
                                                1,
                                                IDE_TYPE_BUFFER);

  /**
   * IdeBufferManager::buffer-focus-leave:
   * @self: An #IdeBufferManager.
   * @buffer: an #IdeBuffer.
   *
   * This signal is emitted when the focus has left the view containing @buffer. You might connect
   * to this signal to stop any work you were performing while the buffer was focused.
   */
  signals [BUFFER_FOCUS_LEAVE] = g_signal_new ("buffer-focus-leave",
                                                G_TYPE_FROM_CLASS (klass),
                                                G_SIGNAL_RUN_LAST,
                                                0,
                                                NULL, NULL, NULL,
                                                G_TYPE_NONE,
                                                1,
                                                IDE_TYPE_BUFFER);

  /**
   * IdeBufferManager::buffer-unloaded:
   * @self: An #IdeBufferManager
   * @buffer: An #IdeBuffer
   *
   * This signal is emitted when the buffer is unloaded. This allows consumers to access the
   * buffer before the items-changed signal is emitted; at which point it will be too late to
   * get a pointer to the buffer.
   */
  signals [BUFFER_UNLOADED] =
    g_signal_new_class_handler ("buffer-unloaded",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (ide_buffer_manager_real_buffer_unloaded),
                                NULL, NULL, NULL,
                                G_TYPE_NONE, 1, IDE_TYPE_BUFFER);

  g_type_ensure (GTK_SOURCE_TYPE_FILE_LOADER);
  g_type_ensure (GTK_SOURCE_TYPE_FILE_SAVER);
}

static void
ide_buffer_manager_init (IdeBufferManager *self)
{
  self->auto_save = TRUE;
  self->auto_save_timeout = AUTO_SAVE_TIMEOUT_DEFAULT;
  self->buffers = g_ptr_array_new ();
  self->max_file_size = MAX_FILE_SIZE_BYTES_DEFAULT;
  self->timeouts = g_hash_table_new (g_direct_hash, g_direct_equal);
  self->word_completion = g_object_new (IDE_TYPE_COMPLETION_WORDS, NULL);
  self->settings = g_settings_new ("org.gnome.builder.editor");
  self->loading = g_hash_table_new_full ((GHashFunc)ide_file_hash,
                                         (GEqualFunc)ide_file_equal,
                                         g_object_unref,
                                         g_object_unref);

  g_settings_bind (self->settings, "minimum-word-size", self->word_completion, "minimum-word-size", G_SETTINGS_BIND_GET);
  g_settings_bind (self->settings, "auto-save", self, "auto-save", G_SETTINGS_BIND_GET);
  g_settings_bind (self->settings, "auto-save-timeout", self, "auto-save-timeout", G_SETTINGS_BIND_GET);
}

static void
register_auto_save (IdeBufferManager *self,
                    IdeBuffer        *buffer)
{
  g_return_if_fail (IDE_IS_BUFFER_MANAGER (self));
  g_return_if_fail (IDE_IS_BUFFER (buffer));
  g_return_if_fail (!g_hash_table_lookup (self->timeouts, buffer));
  g_return_if_fail (self->auto_save_timeout > 0);

  if (gtk_text_buffer_get_modified (GTK_TEXT_BUFFER (buffer)))
    {
      AutoSave *state;

      state = g_slice_new0 (AutoSave);
      dzl_set_weak_pointer (&state->buffer, buffer);
      dzl_set_weak_pointer (&state->self, self);
      state->source_id = g_timeout_add_seconds (self->auto_save_timeout,
                                                ide_buffer_manager_auto_save_cb,
                                                state);
      g_hash_table_insert (self->timeouts, buffer, state);
    }
}

static void
unregister_auto_save (IdeBufferManager *self,
                      IdeBuffer        *buffer)
{
  AutoSave *state;

  g_assert (IDE_IS_BUFFER_MANAGER (self));
  g_assert (IDE_IS_BUFFER (buffer));

  state = g_hash_table_lookup (self->timeouts, buffer);

  if (state != NULL)
    {
      g_hash_table_remove (self->timeouts, buffer);
      dzl_clear_source (&state->source_id);
      dzl_clear_weak_pointer (&state->buffer);
      dzl_clear_weak_pointer (&state->self);
      g_slice_free (AutoSave, state);
    }
}

/**
 * ide_buffer_manager_get_buffers:
 *
 * Returns a newly allocated #GPtrArray of all the buffers managed by the #IdeBufferManager
 * instance.
 *
 * Buffers are generally not added to the buffer list until they have been loaded.
 *
 * Returns: (transfer container) (element-type Ide.Buffer): a #GPtrArray of buffers.
 */
GPtrArray *
ide_buffer_manager_get_buffers (IdeBufferManager *self)
{
  GPtrArray *ret;
  gsize i;

  g_return_val_if_fail (IDE_IS_BUFFER_MANAGER (self), NULL);

  ret = g_ptr_array_new_with_free_func (g_object_unref);

  for (i = 0; i < self->buffers->len; i++)
    {
      IdeBuffer *buffer;

      buffer = g_ptr_array_index (self->buffers, i);
      g_ptr_array_add (ret, g_object_ref (buffer));
    }

  return ret;
}

/**
 * ide_buffer_manager_get_word_completion:
 * @self: an #IdeBufferManager.
 *
 * Gets the #GtkSourceCompletionWords completion provider that will complete
 * words using the loaded documents.
 *
 * Returns: (transfer none): a #GtkSourceCompletionWords
 */
GtkSourceCompletionWords *
ide_buffer_manager_get_word_completion (IdeBufferManager *self)
{
  g_return_val_if_fail (IDE_IS_BUFFER_MANAGER (self), NULL);

  return self->word_completion;
}

/**
 * ide_buffer_manager_find_buffer:
 * @self: (in): An #IdeBufferManager.
 * @file: (in): a #GFile.
 *
 * Gets the buffer for a given file. If it has not yet been loaded, %NULL is
 * returned.
 *
 * Returns: (transfer none) (nullable): An #IdeBuffer or %NULL.
 */
IdeBuffer *
ide_buffer_manager_find_buffer (IdeBufferManager *self,
                                GFile            *file)
{
  GHashTableIter iter;
  gpointer key, value;

  g_return_val_if_fail (IDE_IS_BUFFER_MANAGER (self), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  for (guint i = 0; i < self->buffers->len; i++)
    {
      IdeBuffer *buffer;
      IdeFile *buffer_file;

      buffer = g_ptr_array_index (self->buffers, i);
      buffer_file = ide_buffer_get_file (buffer);

      if (g_file_equal (file, ide_file_get_file (buffer_file)))
        return buffer;
    }

  g_hash_table_iter_init (&iter, self->loading);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      IdeFile *buffer_file = key;
      IdeBuffer *buffer = value;

      if (g_file_equal (file, ide_file_get_file (buffer_file)))
        return buffer;
    }

  return NULL;
}

/**
 * ide_buffer_manager_has_file:
 * @self: (in): An #IdeBufferManager.
 * @file: (in): An #IdeFile.
 *
 * Checks to see if the buffer manager has the file loaded.
 *
 * Returns: %TRUE if @file is loaded.
 */
gboolean
ide_buffer_manager_has_file (IdeBufferManager *self,
                             GFile            *file)
{
  return !!ide_buffer_manager_find_buffer (self, file);
}

/**
 * ide_buffer_manager_get_max_file_size:
 * @self: An #IdeBufferManager.
 *
 * Gets the #IdeBufferManager:max-file-size property. This contains the maximum file size in bytes
 * that a file may be to be loaded by the #IdeBufferManager.
 *
 * If zero, no size limits will be enforced.
 *
 * Returns: a #gsize in bytes or zero.
 */
gsize
ide_buffer_manager_get_max_file_size (IdeBufferManager *self)
{
  g_return_val_if_fail (IDE_IS_BUFFER_MANAGER (self), 0);

  return self->max_file_size;
}

/**
 * ide_buffer_manager_set_max_file_size:
 * @self: An #IdeBufferManager.
 * @max_file_size: The maximum file size in bytes, or zero for no limit.
 *
 * Sets the maximum file size in bytes, that will be loaded by the #IdeBufferManager.
 */
void
ide_buffer_manager_set_max_file_size (IdeBufferManager *self,
                                      gsize             max_file_size)
{
  g_return_if_fail (IDE_IS_BUFFER_MANAGER (self));

  if (self->max_file_size != max_file_size)
    self->max_file_size = max_file_size;
}

/**
 * ide_buffer_manager_create_temporary_buffer:
 *
 * Creates a new #IdeBuffer that does not yet have a backing file attached to it. Interfaces
 * should perform a save-as operation to save the file to a real file.
 *
 * ide_file_get_file() will return %NULL to denote this type of buffer.
 *
 * Returns: (transfer full): A newly created #IdeBuffer
 */
IdeBuffer *
ide_buffer_manager_create_temporary_buffer (IdeBufferManager *self)
{
  IdeBuffer *buffer = NULL;
  g_autoptr(IdeFile) file = NULL;
  g_autoptr(GFile) gfile = NULL;
  g_autofree gchar *path = NULL;
  IdeContext *context;
  IdeVcs *vcs;
  GFile *workdir;
  guint doc_seq;

  g_return_val_if_fail (IDE_IS_BUFFER_MANAGER (self), NULL);

  context = ide_object_get_context (IDE_OBJECT (self));
  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);

  doc_seq = ide_doc_seq_acquire ();
  path = g_strdup_printf (_("unsaved document %u"), doc_seq);
  gfile = g_file_get_child (workdir, path);

  file = g_object_new (IDE_TYPE_FILE,
                       "context", context,
                       "path", path,
                       "file", gfile,
                       "temporary-id", doc_seq,
                       NULL);

  g_signal_emit (self, signals [CREATE_BUFFER], 0, file, &buffer);
  g_signal_emit (self, signals [LOAD_BUFFER], 0, buffer, TRUE);

  g_ptr_array_add (self->buffers, g_object_ref (buffer));
  DZL_COUNTER_INC (registered);
  ide_buffer_manager_track_buffer (self, buffer);

  g_signal_emit (self, signals [BUFFER_LOADED], 0, buffer);

  return buffer;
}

static void
ide_buffer_manager_reclaim__save_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  IdeBufferManager *self = (IdeBufferManager *)object;
  g_autoptr(IdeBuffer) buffer = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER_MANAGER (self));
  g_assert (IDE_IS_BUFFER (buffer));

  if (!ide_buffer_manager_save_file_finish (self, result, &error))
    {
      g_warning (_("Failed to save buffer, ignoring reclamation."));
      g_warning ("%s", error->message);
      IDE_EXIT;
    }

  ide_buffer_manager_remove_buffer (self, buffer);

  IDE_EXIT;
}

void
_ide_buffer_manager_reclaim (IdeBufferManager *self,
                             IdeBuffer        *buffer)
{
  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER_MANAGER (self));
  g_assert (IDE_IS_BUFFER (buffer));

  if (gtk_text_buffer_get_modified (GTK_TEXT_BUFFER (buffer)) &&
      !ide_buffer_get_changed_on_volume (buffer))
    {
      IdeFile *file;

      file = ide_buffer_get_file (buffer);
      ide_buffer_manager_save_file_async (self, buffer, file, NULL, NULL,
                                          ide_buffer_manager_reclaim__save_cb,
                                          g_object_ref (buffer));
    }
  else
    {
      ide_buffer_manager_remove_buffer (self, buffer);
    }

  IDE_EXIT;
}

guint
ide_buffer_manager_get_n_buffers (IdeBufferManager *self)
{
  g_return_val_if_fail (IDE_IS_BUFFER_MANAGER (self), 0);

  return self->buffers->len;
}

static void
ide_buffer_manager_save_all__save_file_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  IdeBufferManager *self = (IdeBufferManager *)object;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;
  guint *count;

  if (!ide_buffer_manager_save_file_finish (self, result, &error))
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
    }

  count = g_task_get_task_data (task);
  if (--(*count) == 0)
    g_task_return_boolean (task, TRUE);
}

void
ide_buffer_manager_save_all_async (IdeBufferManager    *self,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  gsize i;
  guint *count;

  g_return_if_fail (IDE_IS_BUFFER_MANAGER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  count = g_new0 (guint, 1);
  *count = self->buffers->len;
  g_task_set_task_data (task, count, g_free);

  for (i = 0; i < self->buffers->len; i++)
    {
      IdeBuffer *buffer;

      buffer = g_ptr_array_index (self->buffers, i);

      if (!gtk_text_buffer_get_modified (GTK_TEXT_BUFFER (buffer)))
        {
          (*count)--;
          continue;
        }

      ide_buffer_manager_save_file_async (self,
                                          buffer,
                                          ide_buffer_get_file (buffer),
                                          NULL,
                                          cancellable,
                                          ide_buffer_manager_save_all__save_file_cb,
                                          g_object_ref (task));

    }

  if (*count == 0)
    g_task_return_boolean (task, TRUE);
}

gboolean
ide_buffer_manager_save_all_finish (IdeBufferManager  *self,
                                    GAsyncResult      *result,
                                    GError           **error)
{
  g_return_val_if_fail (IDE_IS_BUFFER_MANAGER (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
ide_buffer_manager_do_apply_edits (IdeBufferManager *self,
                                   GHashTable       *buffers,
                                   GPtrArray        *edits)
{
  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER_MANAGER (self));
  g_assert (buffers != NULL);
  g_assert (edits != NULL);

  /* Allow each project edit to stage its GtkTextMarks */
  for (guint i = 0; i < edits->len; i++)
    {
      IdeProjectEdit *edit = g_ptr_array_index (edits, i);
      IdeSourceLocation *location;
      IdeSourceRange *range;
      IdeBuffer *buffer;
      IdeFile *file;

      if (NULL == (range = ide_project_edit_get_range (edit)) ||
          NULL == (location = ide_source_range_get_begin (range)) ||
          NULL == (file = ide_source_location_get_file (location)) ||
          NULL == (buffer = g_hash_table_lookup (buffers, file)))
        {
          g_warning ("Implausible failure to access buffer");
          continue;
        }

      gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (buffer));

      _ide_project_edit_prepare (edit, buffer);
    }

  /* Now actually perform the replacement between the text marks */
  for (guint i = 0; i < edits->len; i++)
    {
      IdeProjectEdit *edit = g_ptr_array_index (edits, i);
      IdeSourceLocation *location;
      IdeSourceRange *range;
      IdeBuffer *buffer;
      IdeFile *file;

      if (NULL == (range = ide_project_edit_get_range (edit)) ||
          NULL == (location = ide_source_range_get_begin (range)) ||
          NULL == (file = ide_source_location_get_file (location)) ||
          NULL == (buffer = g_hash_table_lookup (buffers, file)))
        {
          g_warning ("Implausible failure to access buffer");
          continue;
        }

      _ide_project_edit_apply (edit, buffer);
    }

  /* Complete all of our undo groups */
  for (guint i = 0; i < edits->len; i++)
    {
      IdeProjectEdit *edit = g_ptr_array_index (edits, i);
      IdeSourceLocation *location;
      IdeSourceRange *range;
      IdeBuffer *buffer;
      IdeFile *file;

      if (NULL == (range = ide_project_edit_get_range (edit)) ||
          NULL == (location = ide_source_range_get_begin (range)) ||
          NULL == (file = ide_source_location_get_file (location)) ||
          NULL == (buffer = g_hash_table_lookup (buffers, file)))
        {
          g_warning ("Implausible failure to access buffer");
          continue;
        }

      gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (buffer));
    }

  IDE_EXIT;
}

static void
ide_buffer_manager_apply_edits_save_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  IdeBufferManager *self = (IdeBufferManager *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER_MANAGER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!ide_buffer_manager_save_all_finish (self, result, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_buffer_manager_apply_edits_buffer_loaded (GObject      *object,
                                              GAsyncResult *result,
                                              gpointer      user_data)
{
  IdeBufferManager *self = (IdeBufferManager *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeBuffer) buffer = NULL;
  EditState *state;

  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER_MANAGER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  state = g_task_get_task_data (task);
  state->count--;

  /* Get our buffer, if we failed, we won't proceed with edits */
  if (NULL == (buffer = ide_buffer_manager_load_file_finish (self, result, &error)))
    {
      if (state->failed == FALSE)
        {
          state->failed = TRUE;
          g_task_return_error (task, g_steal_pointer (&error));
          return;
        }
      g_clear_error (&error);
    }

  /* Nothing to do if we already failed */
  if (state->failed)
    IDE_EXIT;

  /* If this is the last buffer to load, then we can go apply the edits. */
  if (state->count == 0)
    {
      GCancellable *cancellable = g_task_get_cancellable (task);

      ide_buffer_manager_do_apply_edits (self, state->buffers, state->edits);

      ide_buffer_manager_save_all_async (self,
                                         cancellable,
                                         ide_buffer_manager_apply_edits_save_cb,
                                         g_steal_pointer (&task));
    }

  IDE_EXIT;
}

/**
 * ide_buffer_manager_apply_edits_async:
 * @self: An #IdeBufferManager
 * @edits: (transfer container) (element-type Ide.ProjectEdit): An #GPtrArray of #IdeProjectEdit
 * @cancellable: (allow-none): a #GCancellable or %NULL
 * @callback: the callback to complete the request
 * @user_data: user data for @callback
 *
 * Asynchronously requests that all of @edits are applied to the buffers
 * in the project. If the buffer has not been loaded for a particular edit,
 * it will be loaded.
 */
void
ide_buffer_manager_apply_edits_async (IdeBufferManager    *self,
                                      GPtrArray           *edits,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  EditState *state;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_BUFFER_MANAGER (self));
  g_return_if_fail (edits != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_buffer_manager_apply_edits_async);

  state = g_slice_new0 (EditState);
  state->buffers = g_hash_table_new_full ((GHashFunc)ide_file_hash,
                                          (GEqualFunc)ide_file_equal,
                                          g_object_unref,
                                          unref_if_non_null);
  state->edits = g_steal_pointer (&edits);
  state->count = 1;

  g_task_set_task_data (task, state, edit_state_free);

  for (guint i = 0; i < state->edits->len; i++)
    {
      IdeProjectEdit *edit = g_ptr_array_index (state->edits, i);
      IdeSourceLocation *location;
      IdeSourceRange *range;
      IdeBuffer *buffer;
      IdeFile *file;

      if (NULL == (range = ide_project_edit_get_range (edit)) ||
          NULL == (location = ide_source_range_get_begin (range)) ||
          NULL == (file = ide_source_location_get_file (location)))
        continue;

      if (g_hash_table_contains (state->buffers, file))
        continue;

      buffer = ide_buffer_manager_find_buffer (self, ide_file_get_file (file));

      if (buffer != NULL)
        {
          g_hash_table_insert (state->buffers, g_object_ref (file), g_object_ref (buffer));
          continue;
        }

      g_hash_table_insert (state->buffers, g_object_ref (file), NULL);

      state->count++;

      ide_buffer_manager_load_file_async (self,
                                          file,
                                          FALSE,
                                          IDE_WORKBENCH_OPEN_FLAGS_BACKGROUND,
                                          NULL,
                                          cancellable,
                                          ide_buffer_manager_apply_edits_buffer_loaded,
                                          g_object_ref (task));
    }

  state->count--;

  IDE_TRACE_MSG ("Waiting for %d buffers to load", state->count);

  if (state->count == 0)
    {
      ide_buffer_manager_do_apply_edits (self, state->buffers, state->edits);
      ide_buffer_manager_save_all_async (self,
                                         cancellable,
                                         ide_buffer_manager_apply_edits_save_cb,
                                         g_steal_pointer (&task));
    }

  IDE_EXIT;
}

gboolean
ide_buffer_manager_apply_edits_finish (IdeBufferManager  *self,
                                       GAsyncResult      *result,
                                       GError           **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_BUFFER_MANAGER (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  ret = g_task_propagate_boolean (G_TASK (result), error);

  IDE_RETURN (ret);
}
