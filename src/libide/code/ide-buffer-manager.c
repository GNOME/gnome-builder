/* ide-buffer-manager.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-buffer-manager"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-core.h>
#include <libide-threading.h>

#include "ide-marshal.h"

#include "ide-buffer.h"
#include "ide-buffer-private.h"
#include "ide-buffer-manager.h"
#include "ide-doc-seq-private.h"
#include "ide-text-edit.h"
#include "ide-text-edit-private.h"
#include "ide-location.h"
#include "ide-range.h"

struct _IdeBufferManager
{
  IdeObject   parent_instance;
  GHashTable *loading_tasks;
  gssize      max_file_size;
};

typedef struct
{
  GPtrArray *buffers;
  guint      n_active;
  guint      had_failure : 1;
} SaveAll;

typedef struct
{
  GPtrArray  *edits;
  GHashTable *buffers;
  GHashTable *to_close;
  guint       n_active;
  guint       failed : 1;
} EditState;

typedef struct
{
  GFile *file;
  IdeBuffer *buffer;
} FindBuffer;

typedef struct
{
  IdeBufferForeachFunc func;
  gpointer user_data;
} Foreach;

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeBufferManager, ide_buffer_manager, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

enum {
  PROP_0,
  PROP_MAX_FILE_SIZE,
  N_PROPS
};

enum {
  BUFFER_LOADED,
  BUFFER_SAVED,
  BUFFER_UNLOADED,
  LOAD_BUFFER,
  N_SIGNALS
};

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static void
edit_state_free (EditState *state)
{
  g_assert (IDE_IS_MAIN_THREAD ());

  if (state != NULL)
    {
      g_clear_pointer (&state->edits, g_ptr_array_unref);
      g_clear_pointer (&state->buffers, g_hash_table_unref);
      g_clear_pointer (&state->to_close, g_hash_table_unref);
      g_slice_free (EditState, state);
    }
}

static void
save_all_free (SaveAll *state)
{
  g_assert (state->n_active == 0);
  g_clear_pointer (&state->buffers, g_ptr_array_unref);
  g_slice_free (SaveAll, state);
}

static IdeBuffer *
ide_buffer_manager_create_buffer (IdeBufferManager *self,
                                  GFile            *file,
                                  gboolean          enable_addins,
                                  gboolean          is_temporary)
{
  g_autoptr(IdeBuffer) buffer = NULL;
  g_autoptr(IdeObjectBox) box = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER_MANAGER (self));

  buffer = _ide_buffer_new (self, file, enable_addins, is_temporary);
  box = ide_object_box_new (G_OBJECT (buffer));

  ide_object_append (IDE_OBJECT (self), IDE_OBJECT (box));
  _ide_buffer_attach (buffer, IDE_OBJECT (box));

  IDE_RETURN (g_steal_pointer (&buffer));
}

static void
ide_buffer_manager_add (IdeObject         *object,
                        IdeObject         *sibling,
                        IdeObject         *child,
                        IdeObjectLocation  location)
{
  IdeBufferManager *self = (IdeBufferManager *)object;
  g_autoptr(IdeBuffer) buffer = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER_MANAGER (self));
  g_assert (IDE_IS_OBJECT (child));

  if (!IDE_IS_OBJECT_BOX (child) ||
      !IDE_IS_BUFFER ((buffer = ide_object_box_ref_object (IDE_OBJECT_BOX (child)))))
    {
      g_critical ("You may only add an IdeObjectBox of IdeBuffer to an IdeBufferManager");
      return;
    }

  IDE_OBJECT_CLASS (ide_buffer_manager_parent_class)->add (object, sibling, child, location);
  g_list_model_items_changed (G_LIST_MODEL (self), ide_object_get_position (child), 0, 1);
}

static void
ide_buffer_manager_remove (IdeObject *object,
                           IdeObject *child)
{
  IdeBufferManager *self = (IdeBufferManager *)object;
  g_autoptr(IdeBuffer) buffer = NULL;
  guint position;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER_MANAGER (self));
  g_assert (IDE_IS_OBJECT_BOX (child));

  IDE_TRACE_MSG ("Request to remove buffer from manager");

  buffer = ide_object_box_ref_object (IDE_OBJECT_BOX (child));
  g_signal_emit (self, signals [BUFFER_UNLOADED], 0, buffer);

  position = ide_object_get_position (child);
  IDE_OBJECT_CLASS (ide_buffer_manager_parent_class)->remove (object, child);
  g_list_model_items_changed (G_LIST_MODEL (self), position, 1, 0);

  IDE_EXIT;
}

static void
ide_buffer_manager_destroy (IdeObject *object)
{
  IdeBufferManager *self = (IdeBufferManager *)object;

  IDE_ENTRY;

  g_clear_pointer (&self->loading_tasks, g_hash_table_unref);

  IDE_OBJECT_CLASS (ide_buffer_manager_parent_class)->destroy (object);

  IDE_EXIT;
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
    case PROP_MAX_FILE_SIZE:
      g_value_set_int64 (value, ide_buffer_manager_get_max_file_size (self));
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
    case PROP_MAX_FILE_SIZE:
      ide_buffer_manager_set_max_file_size (self, g_value_get_int64 (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_buffer_manager_class_init (IdeBufferManagerClass *klass)
{
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = ide_buffer_manager_get_property;
  object_class->set_property = ide_buffer_manager_set_property;

  i_object_class->add = ide_buffer_manager_add;
  i_object_class->remove = ide_buffer_manager_remove;
  i_object_class->destroy = ide_buffer_manager_destroy;

  /**
   * IdeBufferManager:max-file-size:
   *
   * The "max-file-size" property is the largest file size in bytes that
   * Builder will attempt to load. Larger files will fail to load to help
   * ensure that Builder's buffer manager does not attempt to load files that
   * will slow the buffer management beyond usefulness.
   */
  properties [PROP_MAX_FILE_SIZE] =
    g_param_spec_int64 ("max-file-size",
                        "Max File Size",
                        "The max file size to load",
                        -1,
                        G_MAXINT64,
                        10L * 1024L * 1024L,
                        (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * IdeBufferManager:load-buffer:
   * @self: an #IdeBufferManager
   * @buffer: an #IdeBuffer
   *
   * The "load-buffer" signal is emitted before a buffer is (re)loaded.
   */
  signals [LOAD_BUFFER] =
    g_signal_new ("load-buffer",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  ide_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1,
                  IDE_TYPE_BUFFER | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (signals [LOAD_BUFFER],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__OBJECTv);

  /**
   * IdeBufferManager::buffer-loaded:
   * @self: an #IdeBufferManager
   * @buffer: an #IdeBuffer
   *
   * The "buffer-loaded" signal is emitted when an #IdeBuffer has loaded
   * a file from storage.
   */
  signals [BUFFER_LOADED] =
    g_signal_new ("buffer-loaded",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  ide_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1,
                  IDE_TYPE_BUFFER | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (signals [BUFFER_LOADED],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__OBJECTv);

  /**
   * IdeBufferManager::buffer-saved:
   * @self: an #IdeBufferManager
   * @buffer: an #IdeBuffer
   *
   * The "buffer-saved" signal is emitted when an #IdeBuffer has been saved
   * to storage.
   */
  signals [BUFFER_SAVED] =
    g_signal_new ("buffer-saved",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  ide_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1,
                  IDE_TYPE_BUFFER | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (signals [BUFFER_SAVED],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__OBJECTv);

  /**
   * IdeBufferManager::buffer-unloaded:
   * @self: an #IdeBufferManager
   * @buffer: an #IdeBuffer
   *
   * The "buffer-unloaded" signal is emitted when an #IdeBuffer has been
   * unloaded from the buffer manager.
   */
  signals [BUFFER_UNLOADED] =
    g_signal_new ("buffer-unloaded",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  ide_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1,
                  IDE_TYPE_BUFFER | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (signals [BUFFER_UNLOADED],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__OBJECTv);
}

static void
ide_buffer_manager_init (IdeBufferManager *self)
{
  IDE_ENTRY;

  self->loading_tasks = g_hash_table_new_full (g_file_hash,
                                               (GEqualFunc)g_file_equal,
                                               g_object_unref,
                                               g_object_unref);

  IDE_EXIT;
}

static GFile *
ide_buffer_manager_next_temp_file (IdeBufferManager *self)
{
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(GFile) workdir = NULL;
  g_autoptr(GFile) ret = NULL;
  g_autofree gchar *name = NULL;
  guint doc_id;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_BUFFER_MANAGER (self), NULL);

  context = IDE_CONTEXT (ide_object_ref_root (IDE_OBJECT (self)));
  workdir = ide_context_ref_workdir (context);
  doc_id = ide_doc_seq_acquire ();

  /* translators: %u is replaced with an incrementing number */
  name = g_strdup_printf (_("unsaved file %u"), doc_id);

  ret = g_file_get_child (workdir, name);

  IDE_RETURN (g_steal_pointer (&ret));
}

/**
 * ide_buffer_manager_from_context:
 *
 * Gets the #IdeBufferManager for the #IdeContext.
 *
 * Returns: (transfer none): an #IdeBufferManager
 */
IdeBufferManager *
ide_buffer_manager_from_context (IdeContext *context)
{
  IdeBufferManager *self;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  self = ide_context_peek_child_typed (context, IDE_TYPE_BUFFER_MANAGER);
  g_return_val_if_fail (IDE_IS_BUFFER_MANAGER (self), NULL);
  return self;
}

/**
 * ide_buffer_manager_has_file:
 * @self: an #IdeBufferManager
 * @file: a #GFile
 *
 * Checks to see if a buffer has been loaded which contains the contents
 * of @file.
 *
 * Returns: %TRUE if a buffer exists for @file
 */
gboolean
ide_buffer_manager_has_file (IdeBufferManager *self,
                             GFile            *file)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (IDE_IS_BUFFER_MANAGER (self), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  return ide_buffer_manager_find_buffer (self, file) != NULL;
}

static void
ide_buffer_manager_find_buffer_cb (IdeObject  *object,
                                   FindBuffer *find)
{
  g_autoptr(IdeBuffer) buffer = NULL;

  g_assert (IDE_IS_OBJECT_BOX (object));
  g_assert (find != NULL);
  g_assert (G_IS_FILE (find->file));

  if (find->buffer != NULL)
    return;

  buffer = ide_object_box_ref_object (IDE_OBJECT_BOX (object));

  /* We pass back a borrowed reference */
  if (g_file_equal (find->file, ide_buffer_get_file (buffer)))
    find->buffer = buffer;
}

/**
 * ide_buffer_manager_find_buffer:
 * @self: an #IdeBufferManager
 * @file: a #GFile
 *
 * Locates the #IdeBuffer that matches #GFile, if any.
 *
 * Returns: (transfer full): an #IdeBuffer or %NULL
 */
IdeBuffer *
ide_buffer_manager_find_buffer (IdeBufferManager *self,
                                GFile            *file)
{
  FindBuffer find = { file, NULL };

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_BUFFER_MANAGER (self), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  ide_object_foreach (IDE_OBJECT (self),
                      (GFunc)ide_buffer_manager_find_buffer_cb,
                      &find);

  IDE_RETURN (find.buffer);
}

/**
 * ide_buffer_manager_get_max_file_size:
 * @self: an #IdeBufferManager
 *
 * Gets the max file size that will be allowed to be loaded from disk.
 * This is useful to protect Builder from files that would overload the
 * various subsystems.
 *
 * Returns: the max file size in bytes or -1 for unlimited
 */
gssize
ide_buffer_manager_get_max_file_size (IdeBufferManager *self)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), 0);
  g_return_val_if_fail (IDE_IS_BUFFER_MANAGER (self), 0);

  return self->max_file_size;
}

/**
 * ide_buffer_manager_set_max_size:
 * @self: an #IdeBufferManager
 * @max_file_size: the max file size in bytes or -1 for unlimited
 *
 * Sets the max file size that will be allowed to be loaded from disk.
 * This is useful to protect Builder from files that would overload the
 * various subsystems.
 */
void
ide_buffer_manager_set_max_file_size (IdeBufferManager *self,
                                      gssize            max_file_size)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_BUFFER_MANAGER (self));
  g_return_if_fail (max_file_size >= -1);

  if (self->max_file_size != max_file_size)
    {
      self->max_file_size = max_file_size;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MAX_FILE_SIZE]);
    }
}

static void
ide_buffer_manager_load_file_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  IdeBuffer *buffer = (IdeBuffer *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeBufferManager *self;
  GFile *file;

  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  file = ide_task_get_task_data (task);

  g_assert (IDE_IS_BUFFER_MANAGER (self));
  g_assert (G_IS_FILE (file));

  g_hash_table_remove (self->loading_tasks, file);

  if (!_ide_buffer_load_file_finish (buffer, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_object (task, g_object_ref (buffer));

  IDE_EXIT;
}

/**
 * ide_buffer_manager_load_file_async:
 * @self: an #IdeBufferManager
 * @file: (nullable): a #GFile
 * @flags: optional flags for loading the buffer
 * @notif: (nullable): a location for an #IdeNotification, or %NULL
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: a callback to execute upon completion of the operation
 * @user_data: closure data for @callback
 *
 * Requests that @file be loaded by the buffer manager. Depending on @flags,
 * this may result in a new view being displayed in a Builder workspace.
 *
 * If @file is %NULL, then a new temporary file is created with an
 * incrementing number to denote the document, such as "unsaved file 1".
 *
 * After completion, @callback will be executed and you can receive the buffer
 * that was loaded with ide_buffer_manager_load_file_finish().
 *
 * If a buffer has already been loaded from @file, the operation will complete
 * using that existing buffer.
 *
 * If a buffer is currently loading for @file, the operation will complete
 * using that existing buffer after it has completed loading.
 *
 * If @notif is non-NULL, it will be updated with status information while
 * loading the document.
 */
void
ide_buffer_manager_load_file_async (IdeBufferManager     *self,
                                    GFile                *file,
                                    IdeBufferOpenFlags    flags,
                                    IdeNotification      *notif,
                                    GCancellable         *cancellable,
                                    GAsyncReadyCallback   callback,
                                    gpointer              user_data)
{
  g_autoptr(IdeBuffer) buffer = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GFile) temp_file = NULL;
  IdeBuffer *existing;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_BUFFER_MANAGER (self));
  g_return_if_fail (!file || G_IS_FILE (file));
  g_return_if_fail (!notif || IDE_IS_NOTIFICATION (notif));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (file == NULL)
    file = temp_file = ide_buffer_manager_next_temp_file (self);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_buffer_manager_load_file_async);
  ide_task_set_task_data (task, g_file_dup (file), g_object_unref);

  /* If the file requested has already been opened, then we will return
   * that (unless a forced reload was requested).
   */
  if ((existing = ide_buffer_manager_find_buffer (self, file)))
    {
      IdeTask *existing_task;

      /* If the buffer does not need to be reloaded, just return the
       * buffer to the user now.
       */
      if (!(flags & IDE_BUFFER_OPEN_FLAGS_FORCE_RELOAD))
        {
          ide_task_return_object (task, g_object_ref (existing));
          IDE_EXIT;
        }

      /* If the buffer is still loading, we can just chain onto that
       * loading operation and complete this task when that task finishes.
       */
      if ((existing_task = g_hash_table_lookup (self->loading_tasks, file)))
        {
          ide_task_chain (existing_task, task);
          IDE_EXIT;
        }

      buffer = g_object_ref (existing);
    }
  else
    {
      /* Create the buffer and track it so we can find it later */
      buffer = ide_buffer_manager_create_buffer (self, file,
                                                 (flags & IDE_BUFFER_OPEN_FLAGS_DISABLE_ADDINS) == 0,
                                                 temp_file != NULL);
    }

  /* Save this task for later in case we get in a second request to open
   * the file while we are already opening it.
   */
  g_hash_table_insert (self->loading_tasks, g_file_dup (file), g_object_ref (task));

  /* Notify any listeners of new buffers */
  g_assert (buffer != NULL);
  g_assert (IDE_IS_BUFFER (buffer));
  g_signal_emit (self, signals [LOAD_BUFFER], 0, buffer);

  /* Now we can load the buffer asynchronously */
  _ide_buffer_load_file_async (buffer,
                               notif,
                               cancellable,
                               ide_buffer_manager_load_file_cb,
                               g_steal_pointer (&task));

  IDE_EXIT;
}

/**
 * ide_buffer_manager_load_file_finish:
 * @self: an #IdeBufferManager
 * @result: a #GAsyncResult provided to callback
 * @error: a location for a #GError, or %NULL
 *
 * Completes an asynchronous request to ide_buffer_manager_laod_file_async().
 *
 * Returns: (transfer full): an #IdeBuffer
 */
IdeBuffer *
ide_buffer_manager_load_file_finish (IdeBufferManager  *self,
                                     GAsyncResult      *result,
                                     GError           **error)
{
  IdeBuffer *ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_BUFFER_MANAGER (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  ret = ide_task_propagate_object (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_buffer_manager_save_all_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  IdeBuffer *buffer = (IdeBuffer *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  SaveAll *state;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  state = ide_task_get_task_data (task);

  g_assert (state != NULL);
  g_assert (state->buffers != NULL);
  g_assert (state->n_active > 0);

  if (!ide_buffer_save_file_finish (buffer, result, &error))
    {
      g_warning ("Failed to save buffer “%s”: %s",
                 ide_buffer_dup_title (buffer),
                 error->message);
      state->had_failure = TRUE;
    }

  state->n_active--;

  if (state->n_active == 0)
    {
      if (state->had_failure)
        ide_task_return_new_error (task,
                                   G_IO_ERROR,
                                   G_IO_ERROR_FAILED,
                                   "One or more buffers failed to save");
      else
        ide_task_return_boolean (task, TRUE);
    }

  IDE_EXIT;
}

static void
ide_buffer_manager_save_all_foreach_cb (IdeObject *object,
                                        IdeTask   *task)
{
  g_autoptr(IdeBuffer) buffer = NULL;
  SaveAll *state;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_OBJECT_BOX (object));
  g_assert (IDE_IS_TASK (task));

  buffer = ide_object_box_ref_object (IDE_OBJECT_BOX (object));
  state = ide_task_get_task_data (task);

  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (state != NULL);
  g_assert (state->buffers != NULL);

  /* Skip buffers that are loading or saving, as they are already in
   * the correct form on disk (or will be soon). We somewhat risk beating
   * an existing save, but that is probably okay to the user since they've
   * already submitted the save request.
   */
  if (ide_buffer_get_state (buffer) != IDE_BUFFER_STATE_READY)
    return;

  /* If the file is externally modified on disk, don't save it either
   * so we don't risk overwriting changed files. The user needs to
   * explicitly overwrite those to avoid loosing work saved outside
   * of Builder.
   */
  if (ide_buffer_get_changed_on_volume (buffer))
    return;

  g_ptr_array_add (state->buffers, g_object_ref (buffer));

  state->n_active++;

  ide_buffer_save_file_async (buffer,
                              NULL,
                              ide_task_get_cancellable (task),
                              NULL,
                              ide_buffer_manager_save_all_cb,
                              g_object_ref (task));
}

/**
 * ide_buffer_manager_save_all_async:
 * @self: an #IdeBufferManager
 * @cancellable: (nullable): a #GCancellable
 * @callback: a callback to execute upon completion
 * @user_data: closure data for @callback
 *
 * Asynchronously requests that the #IdeBufferManager save all of the loaded
 * buffers to disk.
 *
 * @callback will be executed after all the buffers have been saved.
 */
void
ide_buffer_manager_save_all_async (IdeBufferManager    *self,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  SaveAll *state;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_BUFFER_MANAGER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_buffer_manager_save_all_async);

  state = g_slice_new0 (SaveAll);
  state->buffers = g_ptr_array_new_full (0, g_object_unref);
  ide_task_set_task_data (task, state, save_all_free);

  ide_object_foreach (IDE_OBJECT (self),
                      (GFunc)ide_buffer_manager_save_all_foreach_cb,
                      task);

  if (state->n_active == 0)
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

/**
 * ide_buffer_manager_save_all_finish:
 * @self: an #IdeBufferManager
 * @result: a #GAsyncResult
 * @error: a location for a #GError, or %NUL
 *
 * Completes an asynchronous request to save all buffers.
 *
 * Returns: %TRUE if all the buffers were saved successfully
 */
gboolean
ide_buffer_manager_save_all_finish (IdeBufferManager  *self,
                                    GAsyncResult      *result,
                                    GError           **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (IDE_IS_BUFFER_MANAGER (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_buffer_manager_apply_edits_save_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  IdeBufferManager *self = (IdeBufferManager *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER_MANAGER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_buffer_manager_save_all_finish (self, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_buffer_manager_do_apply_edits (IdeBufferManager *self,
                                   GHashTable       *buffers,
                                   GPtrArray        *edits)
{
  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER_MANAGER (self));
  g_assert (buffers != NULL);
  g_assert (edits != NULL);

  g_debug ("Applying %u edits", edits->len);

  /* Allow each project edit to stage its GtkTextMarks */
  for (guint i = 0; i < edits->len; i++)
    {
      IdeTextEdit *edit = g_ptr_array_index (edits, i);
      IdeLocation *location;
      IdeRange *range;
      IdeBuffer *buffer;
      GFile *file;

      if (NULL == (range = ide_text_edit_get_range (edit)) ||
          NULL == (location = ide_range_get_begin (range)) ||
          NULL == (file = ide_location_get_file (location)) ||
          NULL == (buffer = g_hash_table_lookup (buffers, file)))
        {
          g_warning ("Implausible failure to access buffer");
          continue;
        }

      gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (buffer));

      _ide_text_edit_prepare (edit, buffer);
    }

  /* Now actually perform the replacement between the text marks */
  for (guint i = 0; i < edits->len; i++)
    {
      IdeTextEdit *edit = g_ptr_array_index (edits, i);
      IdeLocation *location;
      IdeRange *range;
      IdeBuffer *buffer;
      GFile *file;

      if (NULL == (range = ide_text_edit_get_range (edit)) ||
          NULL == (location = ide_range_get_begin (range)) ||
          NULL == (file = ide_location_get_file (location)) ||
          NULL == (buffer = g_hash_table_lookup (buffers, file)))
        {
          g_warning ("Implausible failure to access buffer");
          continue;
        }

      _ide_text_edit_apply (edit, buffer);
    }

  /* Complete all of our undo groups */
  for (guint i = 0; i < edits->len; i++)
    {
      IdeTextEdit *edit = g_ptr_array_index (edits, i);
      IdeLocation *location;
      IdeRange *range;
      IdeBuffer *buffer;
      GFile *file;

      if (NULL == (range = ide_text_edit_get_range (edit)) ||
          NULL == (location = ide_range_get_begin (range)) ||
          NULL == (file = ide_location_get_file (location)) ||
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
ide_buffer_manager_apply_edits_buffer_loaded_cb (GObject      *object,
                                                 GAsyncResult *result,
                                                 gpointer      user_data)
{
  IdeBufferManager *self = (IdeBufferManager *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeBuffer) buffer = NULL;
  GCancellable *cancellable;
  EditState *state;
  GFile *file;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER_MANAGER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  cancellable = ide_task_get_cancellable (task);
  state = ide_task_get_task_data (task);

  g_assert (state != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  state->n_active--;

  /* Get our buffer, if we failed, we won't proceed with edits */
  if (!(buffer = ide_buffer_manager_load_file_finish (self, result, &error)))
    {
      if (state->failed == FALSE)
        {
          state->failed = TRUE;
          ide_task_return_error (task, g_steal_pointer (&error));
        }
    }

  /* Nothing to do if we already failed */
  if (state->failed)
    IDE_EXIT;

  /* Save the buffer for future use when applying edits */
  file = ide_buffer_get_file (buffer);
  g_hash_table_insert (state->buffers, g_object_ref (file), g_object_ref (buffer));
  g_hash_table_insert (state->to_close, g_object_ref (file), g_object_ref (buffer));

  /* If this is the last buffer to load, then we can go apply the edits. */
  if (state->n_active == 0)
    {
      ide_buffer_manager_do_apply_edits (self,
                                         state->buffers,
                                         state->edits);
      ide_buffer_manager_save_all_async (self,
                                         cancellable,
                                         ide_buffer_manager_apply_edits_save_cb,
                                         g_steal_pointer (&task));
    }

  IDE_EXIT;
}

static void
ide_buffer_manager_apply_edits_completed_cb (IdeBufferManager *self,
                                             GParamSpec       *pspec,
                                             IdeTask          *task)
{
  GHashTableIter iter;
  IdeBuffer *buffer;
  EditState *state;

  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER_MANAGER (self));
  g_assert (pspec != NULL);
  g_assert (IDE_IS_TASK (task));

  state = ide_task_get_task_data (task);
  g_assert (state != NULL);
  g_assert (state->to_close != NULL);
  g_assert (state->buffers != NULL);

  g_hash_table_iter_init (&iter, state->to_close);

  while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&buffer))
    {
      if (buffer != NULL)
        {
          IdeObjectBox *box = ide_object_box_from_object (G_OBJECT (buffer));

          g_assert (!box || IDE_IS_OBJECT_BOX (box));

          if (box != NULL)
            ide_object_destroy (IDE_OBJECT (box));
        }
    }

  IDE_EXIT;
}

static int
compare_edits (gconstpointer a,
               gconstpointer b)
{
  IdeTextEdit *edit_a = *(IdeTextEdit * const *)a;
  IdeTextEdit *edit_b = *(IdeTextEdit * const *)b;
  IdeRange *range_a = ide_text_edit_get_range (edit_a);
  IdeRange *range_b = ide_text_edit_get_range (edit_b);
  IdeLocation *loc_a = ide_range_get_begin (range_a);
  IdeLocation *loc_b = ide_range_get_begin (range_b);
  int cmpval = ide_location_compare (loc_a, loc_b);

  /* Reverse sort */
  if (cmpval < 0)
    return 1;
  else if (cmpval > 0)
    return -1;
  else
    return 0;
}

/**
 * ide_buffer_manager_apply_edits_async:
 * @self: An #IdeBufferManager
 * @edits: (transfer full) (element-type IdeTextEdit):
 *   An #GPtrArray of #IdeTextEdit.
 * @cancellable: (allow-none): a #GCancellable or %NULL
 * @callback: the callback to complete the request
 * @user_data: user data for @callback
 *
 * Asynchronously requests that all of @edits are applied to the buffers
 * in the project. If the buffer has not been loaded for a particular edit,
 * it will be loaded.
 *
 * @callback should call ide_buffer_manager_apply_edits_finish() to get the
 * result of this operation.
 */
void
ide_buffer_manager_apply_edits_async (IdeBufferManager    *self,
                                      GPtrArray           *edits,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  EditState *state;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_BUFFER_MANAGER (self));
  g_return_if_fail (edits != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_PTR_ARRAY_SET_FREE_FUNC (edits, g_object_unref);

  g_ptr_array_sort (edits, compare_edits);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_buffer_manager_apply_edits_async);

  state = g_slice_new0 (EditState);
  state->buffers = g_hash_table_new_full (g_file_hash,
                                          (GEqualFunc)g_file_equal,
                                          g_object_unref,
                                          _g_object_unref0);
  state->to_close = g_hash_table_new_full (g_file_hash,
                                           (GEqualFunc)g_file_equal,
                                           g_object_unref,
                                           _g_object_unref0);
  state->edits = g_steal_pointer (&edits);
  ide_task_set_task_data (task, state, edit_state_free);

  g_signal_connect_object (task,
                           "notify::completed",
                           G_CALLBACK (ide_buffer_manager_apply_edits_completed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  for (guint i = 0; i < state->edits->len; i++)
    {
      IdeTextEdit *edit = g_ptr_array_index (state->edits, i);
      IdeLocation *location;
      IdeBuffer *buffer;
      IdeRange *range;
      GFile *file;

      if (NULL == (range = ide_text_edit_get_range (edit)) ||
          NULL == (location = ide_range_get_begin (range)) ||
          NULL == (file = ide_location_get_file (location)))
        continue;

      if (g_hash_table_contains (state->buffers, file))
        continue;

      if ((buffer = ide_buffer_manager_find_buffer (self, file)))
        {
          g_hash_table_insert (state->buffers, g_object_ref (file), g_object_ref (buffer));
          continue;
        }

      g_hash_table_insert (state->buffers, g_object_ref (file), NULL);

      state->n_active++;

      /* Load buffers, but don't create views for them since we don't want to
       * create lots of views if there are lots of files to edit.
       */
      ide_buffer_manager_load_file_async (self,
                                          file,
                                          IDE_BUFFER_OPEN_FLAGS_DISABLE_ADDINS,
                                          NULL,
                                          cancellable,
                                          ide_buffer_manager_apply_edits_buffer_loaded_cb,
                                          g_object_ref (task));
    }

  IDE_TRACE_MSG ("Waiting for %d buffers to load", state->n_active);

  if (state->n_active == 0)
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

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (IDE_IS_BUFFER_MANAGER (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

void
_ide_buffer_manager_buffer_loaded (IdeBufferManager *self,
                                   IdeBuffer        *buffer)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_BUFFER_MANAGER (self));
  g_return_if_fail (IDE_IS_BUFFER (buffer));

  g_signal_emit (self, signals [BUFFER_LOADED], 0, buffer);
}

void
_ide_buffer_manager_buffer_saved (IdeBufferManager *self,
                                  IdeBuffer        *buffer)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_BUFFER_MANAGER (self));
  g_return_if_fail (IDE_IS_BUFFER (buffer));

  g_signal_emit (self, signals [BUFFER_SAVED], 0, buffer);
}

static GType
ide_buffer_manager_get_item_type (GListModel *model)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER_MANAGER (model));

  return IDE_TYPE_BUFFER;
}

static gpointer
ide_buffer_manager_get_item (GListModel *model,
                             guint       position)
{
  IdeBufferManager *self = (IdeBufferManager *)model;
  g_autoptr(IdeObject) box = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER_MANAGER (self));

  if ((box = ide_object_get_nth_child (IDE_OBJECT (self), position)))
    return ide_object_box_ref_object (IDE_OBJECT_BOX (box));

  return NULL;
}

static guint
ide_buffer_manager_get_n_items (GListModel *model)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER_MANAGER (model));

  return ide_object_get_n_children (IDE_OBJECT (model));
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = ide_buffer_manager_get_item_type;
  iface->get_item = ide_buffer_manager_get_item;
  iface->get_n_items = ide_buffer_manager_get_n_items;
}

static void
ide_buffer_manager_foreach_cb (IdeObject *object,
                               gpointer   user_data)
{
  const Foreach *state = user_data;

  g_assert (IDE_IS_OBJECT (object));

  if (IDE_IS_OBJECT_BOX (object))
    {
      g_autoptr(IdeObject) wrapped = NULL;

      wrapped = ide_object_box_ref_object (IDE_OBJECT_BOX (object));

      if (IDE_IS_BUFFER (wrapped))
        state->func (IDE_BUFFER (wrapped), state->user_data);
    }
}

/**
 * ide_buffer_manager_foreach:
 * @self: a #IdeBufferManager
 * @foreach_func: (scope call): an #IdeBufferForeachFunc
 * @user_data: closure data for @foreach_func
 *
 * Calls @foreach_func for every buffer registered.
 */
void
ide_buffer_manager_foreach (IdeBufferManager     *self,
                            IdeBufferForeachFunc  foreach_func,
                            gpointer              user_data)
{
  Foreach state = { foreach_func, user_data };

  g_return_if_fail (IDE_IS_BUFFER_MANAGER (self));
  g_return_if_fail (foreach_func != NULL);

  ide_object_foreach (IDE_OBJECT (self),
                      (GFunc)ide_buffer_manager_foreach_cb,
                      &state);
}

static void
ide_buffer_manager_reload_all_load_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  IdeBufferManager *self = (IdeBufferManager *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  guint *n_active;

  g_assert (IDE_IS_BUFFER_MANAGER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  n_active = ide_task_get_task_data (task);

  if (!ide_buffer_manager_load_file_finish (self, result, &error))
    g_warning ("Failed to reload buffer: %s", error->message);

  (*n_active)--;

  if (*n_active == 0)
    ide_task_return_boolean (task, TRUE);
}

static void
ide_buffer_manager_reload_all_foreach_cb (IdeBuffer *buffer,
                                          IdeTask   *task)
{
  IdeBufferManager *self;
  guint *n_active;

  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  n_active = ide_task_get_task_data (task);

  if (ide_buffer_get_changed_on_volume (buffer))
    {
      (*n_active)++;

      ide_buffer_manager_load_file_async (self,
                                          ide_buffer_get_file (buffer),
                                          IDE_BUFFER_OPEN_FLAGS_FORCE_RELOAD,
                                          NULL,
                                          ide_task_get_cancellable (task),
                                          ide_buffer_manager_reload_all_load_cb,
                                          g_object_ref (task));
    }
}

void
ide_buffer_manager_reload_all_async (IdeBufferManager    *self,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  guint *n_active;

  g_return_if_fail (IDE_IS_BUFFER_MANAGER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  n_active = g_new0 (guint, 1);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_buffer_manager_reload_all_async);
  ide_task_set_task_data (task, n_active, g_free);

  ide_buffer_manager_foreach (self,
                              (IdeBufferForeachFunc)ide_buffer_manager_reload_all_foreach_cb,
                              task);

  if (*n_active == 0)
    ide_task_return_boolean (task, TRUE);
}

gboolean
ide_buffer_manager_reload_all_finish (IdeBufferManager  *self,
                                      GAsyncResult      *result,
                                      GError           **error)
{
  g_return_val_if_fail (IDE_IS_BUFFER_MANAGER (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}
