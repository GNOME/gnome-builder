/* ide-buffer-manager.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "ide-buffer-manager"

#include <gtksourceview/gtksource.h>
#include <glib/gi18n.h>

#include "ide-buffer.h"
#include "ide-buffer-manager.h"
#include "ide-file.h"
#include "ide-file-settings.h"
#include "ide-internal.h"
#include "ide-progress.h"

#define AUTO_SAVE_TIMEOUT_DEFAULT 60

struct _IdeBufferManager
{
  IdeObject                 parent_instance;

  GPtrArray                *buffers;
  GHashTable               *timeouts;

  IdeBuffer                *focus_buffer;

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
  IdeBuffer   *buffer;
  IdeFile     *file;
  IdeProgress *progress;
  guint        is_new;
} LoadState;

typedef struct
{
  IdeBuffer   *buffer;
  IdeFile     *file;
  IdeProgress *progress;
} SaveState;

G_DEFINE_TYPE (IdeBufferManager, ide_buffer_manager, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_AUTO_SAVE,
  PROP_AUTO_SAVE_TIMEOUT,
  PROP_FOCUS_BUFFER,
  LAST_PROP
};

enum {
  SAVE_BUFFER,
  BUFFER_SAVED,

  LOAD_BUFFER,
  BUFFER_LOADED,

  BUFFER_FOCUS_ENTER,
  BUFFER_FOCUS_LEAVE,

  LAST_SIGNAL
};

static void register_auto_save   (IdeBufferManager *self,
                                  IdeBuffer        *buffer);
static void unregister_auto_save (IdeBufferManager *self,
                                  IdeBuffer        *buffer);

static GParamSpec *gParamSpecs [LAST_PROP];
static guint gSignals [LAST_SIGNAL];

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
      g_slice_free (LoadState, state);
    }
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
                                gParamSpecs [PROP_AUTO_SAVE_TIMEOUT]);
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
 * seconds have elapsed since the buffers last modification.
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
                                gParamSpecs [PROP_AUTO_SAVE]);
    }
}

/**
 * ide_buffer_manager_get_focus_buffer:
 *
 * Gets the #IdeBufferManager:focus-buffer property. This the buffer behind
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

  if (ide_set_weak_pointer (&self->focus_buffer, buffer))
    {
      /* notify that we left the previous buffer */
      if (previous)
        g_signal_emit (self, gSignals [BUFFER_FOCUS_LEAVE], 0, previous);

      /* notify of the new buffer, but check for reentrancy */
      if (buffer && (buffer == self->focus_buffer))
        g_signal_emit (self, gSignals [BUFFER_FOCUS_ENTER], 0, buffer);
    }
}

static gboolean
ide_buffer_manager_auto_save_cb (gpointer data)
{
  AutoSave *state = data;
  IdeFile *file;

  g_assert (state);
  g_assert (IDE_IS_BUFFER_MANAGER (state->self));
  g_assert (IDE_IS_BUFFER (state->buffer));
  g_assert (state->source_id > 0);

  file = ide_buffer_get_file (state->buffer);
  if (file)
    ide_buffer_manager_save_file_async (state->self, state->buffer, file, NULL, NULL, NULL, NULL);

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
ide_buffer_manager_add_buffer (IdeBufferManager *self,
                               IdeBuffer        *buffer)
{
  g_return_if_fail (IDE_IS_BUFFER_MANAGER (self));
  g_return_if_fail (IDE_IS_BUFFER (buffer));

  g_ptr_array_add (self->buffers, g_object_ref (buffer));

  if (self->auto_save)
    register_auto_save (self, buffer);

  g_signal_connect_object (buffer,
                           "changed",
                           G_CALLBACK (ide_buffer_manager_buffer_changed),
                           self,
                           (G_CONNECT_SWAPPED | G_CONNECT_AFTER));
}

static void
ide_buffer_manager_remove_buffer (IdeBufferManager *self,
                                  IdeBuffer        *buffer)
{
  g_return_if_fail (IDE_IS_BUFFER_MANAGER (self));
  g_return_if_fail (IDE_IS_BUFFER (buffer));

  if (g_ptr_array_remove_fast (self->buffers, buffer))
    {
      unregister_auto_save (self, buffer);
      g_signal_handlers_disconnect_by_func (buffer,
                                            G_CALLBACK (ide_buffer_manager_buffer_changed),
                                            self);
      g_object_unref (buffer);
    }
}

static IdeBuffer *
ide_buffer_manager_get_buffer (IdeBufferManager *self,
                               IdeFile          *file)
{
  gsize i;

  g_return_val_if_fail (IDE_IS_BUFFER_MANAGER (self), NULL);

  for (i = 0; i < self->buffers->len; i++)
    {
      IdeBuffer *buffer;
      IdeFile *cur_file;

      buffer = g_ptr_array_index (self->buffers, i);
      cur_file = ide_buffer_get_file (buffer);

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
  GtkSourceFileLoader *loader = (GtkSourceFileLoader *)object;
  IdeBufferManager *self;
  LoadState *state;
  GError *error = NULL;
  gsize i;

  g_assert (G_IS_TASK (task));
  g_assert (GTK_SOURCE_IS_FILE_LOADER (loader));

  self = g_task_get_source_object (task);
  state = g_task_get_task_data (task);

  g_assert (IDE_IS_BUFFER_MANAGER (self));
  g_assert (IDE_IS_FILE (state->file));
  g_assert (IDE_IS_BUFFER (state->buffer));
  g_assert (IDE_IS_PROGRESS (state->progress));

  if (!gtk_source_file_loader_load_finish (loader, result, &error))
    {
      /*
       * It's okay if we fail because the file does not exist yet.
       */
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        {
          g_task_return_error (task, error);
          return;
        }

      g_clear_error (&error);
    }

  for (i = 0; i < self->buffers->len; i++)
    {
      IdeBuffer *cur_buffer;

      cur_buffer = g_ptr_array_index (self->buffers, i);

      if (cur_buffer == state->buffer)
        goto emit_signal;
    }

  if (state->is_new)
    ide_buffer_manager_add_buffer (self, state->buffer);

emit_signal:
  g_signal_emit (self, gSignals [BUFFER_LOADED], 0, state->buffer);

  g_task_return_pointer (task, g_object_ref (state->buffer), g_object_unref);
}

/**
 * ide_buffer_manager_load_file_async:
 * @progress: (out) (nullable): A location for an #IdeProgress or %NULL.
 *
 * Asynchronously requests that the file represented by @file is loaded. If the file is already
 * loaded, the previously loaded version of the file will be returned, asynchronously.
 *
 * See ide_buffer_manager_load_file_finish() for how to complete this asynchronous request.
 */
void
ide_buffer_manager_load_file_async  (IdeBufferManager     *self,
                                     IdeFile              *file,
                                     gboolean              force_reload,
                                     IdeProgress         **progress,
                                     GCancellable         *cancellable,
                                     GAsyncReadyCallback   callback,
                                     gpointer              user_data)
{
  g_autoptr(GTask) task = NULL;
  IdeContext *context;
  IdeBuffer *buffer;
  LoadState *state;
  GtkSourceFileLoader *loader;
  GtkSourceFile *source_file;

  if (progress)
    *progress = NULL;

  g_return_if_fail (IDE_IS_BUFFER_MANAGER (self));
  g_return_if_fail (IDE_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  context = ide_object_get_context (IDE_OBJECT (self));
  buffer = ide_buffer_manager_get_buffer (self, file);

  /*
   * If the buffer is already loaded, then we can complete the request immediately.
   */
  if (buffer && !force_reload)
    {
      if (progress)
        *progress = g_object_new (IDE_TYPE_PROGRESS,
                                  "context", context,
                                  "fraction", 1.0,
                                  NULL);
      g_task_return_pointer (task, g_object_ref (buffer), g_object_unref);
      return;
    }

  state = g_slice_new0 (LoadState);
  state->is_new = (buffer == NULL);
  state->file = g_object_ref (file);
  state->progress = g_object_new (IDE_TYPE_PROGRESS,
                                  "context", context,
                                  NULL);
  if (buffer)
    state->buffer = g_object_ref (buffer);
  else
    state->buffer = g_object_new (IDE_TYPE_BUFFER,
                                  "context", context,
                                  "file", file,
                                  NULL);

  g_task_set_task_data (task, state, load_state_free);

  if (progress)
    *progress = g_object_ref (state->progress);

  source_file = _ide_file_get_source_file (file);
  loader = gtk_source_file_loader_new (GTK_SOURCE_BUFFER (state->buffer), source_file);

  g_signal_emit (self, gSignals [LOAD_BUFFER], 0, state->buffer);

  gtk_source_file_loader_load_async (loader,
                                     G_PRIORITY_DEFAULT,
                                     cancellable,
                                     ide_progress_file_progress_callback,
                                     g_object_ref (state->progress),
                                     g_object_unref,
                                     ide_buffer_manager_load_file__load_cb,
                                     g_object_ref (task));

  g_clear_object (&loader);
}

/**
 * ide_buffer_manager_load_file_finish:
 *
 * Completes an asynchronous request to load a file via ide_buffer_manager_load_file_async().
 * If the buffer was already loaded, this function will return a reference to the previous buffer
 * with it's reference count incremented by one.
 *
 * Returns: (transfer full): An #IdeBuffer if successf; otherwise %NULL and @error is set.
 */
IdeBuffer *
ide_buffer_manager_load_file_finish (IdeBufferManager  *self,
                                     GAsyncResult      *result,
                                     GError           **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_BUFFER_MANAGER (self), NULL);
  g_return_val_if_fail (G_IS_TASK (task), NULL);

  return g_task_propagate_pointer (task, error);
}

static void
ide_buffer_manager_save_file__save_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  g_autoptr(GTask) task = user_data;
  GtkSourceFileSaver *saver = (GtkSourceFileSaver *)object;
  IdeBufferManager *self;
  SaveState *state;
  GError *error = NULL;

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
      g_task_return_error (task, error);
      return;
    }

  /* Notify signal handlers that the file is saved */
  g_signal_emit (self, gSignals [BUFFER_SAVED], 0, state->buffer);

  g_task_return_boolean (task, TRUE);
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
  GError *error = NULL;

  g_assert (IDE_IS_FILE (file));
  g_assert (G_IS_TASK (task));

  file_settings = ide_file_load_settings_finish (file, result, &error);

  if (!file_settings)
    {
      g_task_return_error (task, error);
      return;
    }

  source_file = _ide_file_get_source_file (file);

  state = g_task_get_task_data (task);

  g_assert (GTK_SOURCE_IS_FILE (source_file));
  g_assert (IDE_IS_BUFFER (state->buffer));
  g_assert (IDE_IS_FILE (state->file));
  g_assert (IDE_IS_PROGRESS (state->progress));

  saver = gtk_source_file_saver_new (GTK_SOURCE_BUFFER (state->buffer), source_file);

  /* set file encoding and newline style defaults */
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

  gtk_source_file_saver_save_async (saver,
                                    G_PRIORITY_DEFAULT,
                                    g_task_get_cancellable (task),
                                    ide_progress_file_progress_callback,
                                    g_object_ref (state->progress),
                                    g_object_unref,
                                    ide_buffer_manager_save_file__save_cb,
                                    g_object_ref (task));

  g_clear_object (&saver);
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
ide_buffer_manager_save_file_async  (IdeBufferManager     *self,
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

  state = g_slice_new0 (SaveState);
  state->file = g_object_ref (file);
  state->buffer = g_object_ref (buffer);
  state->progress = g_object_new (IDE_TYPE_PROGRESS,
                                  "context", context,
                                  NULL);

  g_task_set_task_data (task, state, save_state_free);

  g_signal_emit (self, gSignals [SAVE_BUFFER], 0, buffer);

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
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_BUFFER_MANAGER (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (task), FALSE);

  return g_task_propagate_boolean (task, error);
}

static void
ide_buffer_manager_dispose (GObject *object)
{
  IdeBufferManager *self = (IdeBufferManager *)object;

  while (self->buffers->len)
    {
      IdeBuffer *buffer;

      buffer = g_ptr_array_index (self->buffers, 0);
      ide_buffer_manager_remove_buffer (self, buffer);
    }

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
  g_clear_pointer (&self->timeouts, g_hash_table_unref);

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

  gParamSpecs [PROP_AUTO_SAVE] =
    g_param_spec_boolean ("auto-save",
                          _("Auto Save"),
                          _("If the documents should auto save after a configured timeout."),
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_AUTO_SAVE,
                                   gParamSpecs [PROP_AUTO_SAVE]);

  gParamSpecs [PROP_AUTO_SAVE_TIMEOUT] =
    g_param_spec_uint ("auto-save-timeout",
                       _("Auto Save Timeout"),
                       _("The number of seconds after modification before auto saving."),
                       0,
                       G_MAXUINT,
                       AUTO_SAVE_TIMEOUT_DEFAULT,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_AUTO_SAVE_TIMEOUT,
                                   gParamSpecs [PROP_AUTO_SAVE_TIMEOUT]);

  /**
   * IdeBufferManager::save-buffer:
   * @self: An #IdeBufferManager.
   * @buffer: an #IdeBuffer.
   *
   * This signal is emitted when a request has been made to save a buffer. Connect to this signal
   * if you'd like to perform mutation of the buffer before it is persisted to storage.
   */
  gSignals [SAVE_BUFFER] = g_signal_new ("save-buffer",
                                         G_TYPE_FROM_CLASS (object_class),
                                         G_SIGNAL_RUN_LAST,
                                         0,
                                         NULL, NULL,
                                         g_cclosure_marshal_generic,
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
   */
  gSignals [BUFFER_SAVED] = g_signal_new ("buffer-saved",
                                          G_TYPE_FROM_CLASS (object_class),
                                          G_SIGNAL_RUN_LAST,
                                          0,
                                          NULL, NULL,
                                          g_cclosure_marshal_generic,
                                          G_TYPE_NONE,
                                          1,
                                          IDE_TYPE_BUFFER);

  /**
   * IdeBufferManager::load-buffer:
   * @self: An #IdeBufferManager.
   * @buffer: an #IdeBuffer.
   *
   * This signal is emitted when a request has been made to load a buffer from storage. You might
   * connect to this signal to be notified when loading of a buffer has begun.
   */
  gSignals [LOAD_BUFFER] = g_signal_new ("load-buffer",
                                         G_TYPE_FROM_CLASS (object_class),
                                         G_SIGNAL_RUN_LAST,
                                         0,
                                         NULL, NULL,
                                         g_cclosure_marshal_generic,
                                         G_TYPE_NONE,
                                         1,
                                         IDE_TYPE_BUFFER);

  /**
   * IdeBufferManager::buffer-loaded:
   * @self: An #IdeBufferManager.
   * @buffer: an #IdeBuffer.
   *
   * This signal is emitted when a buffer has been successfully loaded. You might connect to this
   * signal to be notified when a buffer has completed loading.
   */
  gSignals [BUFFER_LOADED] = g_signal_new ("buffer-loaded",
                                           G_TYPE_FROM_CLASS (object_class),
                                           G_SIGNAL_RUN_LAST,
                                           0,
                                           NULL, NULL,
                                           g_cclosure_marshal_generic,
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
  gSignals [BUFFER_FOCUS_ENTER] = g_signal_new ("buffer-focus-enter",
                                                G_TYPE_FROM_CLASS (object_class),
                                                G_SIGNAL_RUN_LAST,
                                                0,
                                                NULL, NULL,
                                                g_cclosure_marshal_generic,
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
  gSignals [BUFFER_FOCUS_LEAVE] = g_signal_new ("buffer-focus-leave",
                                                G_TYPE_FROM_CLASS (object_class),
                                                G_SIGNAL_RUN_LAST,
                                                0,
                                                NULL, NULL,
                                                g_cclosure_marshal_generic,
                                                G_TYPE_NONE,
                                                1,
                                                IDE_TYPE_BUFFER);
}

static void
ide_buffer_manager_init (IdeBufferManager *self)
{
  self->auto_save = TRUE;
  self->auto_save_timeout = AUTO_SAVE_TIMEOUT_DEFAULT;
  self->buffers = g_ptr_array_new ();
  self->timeouts = g_hash_table_new (g_direct_hash, g_direct_equal);
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
      ide_set_weak_pointer (&state->buffer, buffer);
      ide_set_weak_pointer (&state->self, self);
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
      if (state->source_id > 0)
        g_source_remove (state->source_id);
      ide_clear_weak_pointer (&state->buffer);
      ide_clear_weak_pointer (&state->self);
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
 * Returns: (transfer container) (element-type IdeBuffer*): A #GPtrArray of buffers.
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
