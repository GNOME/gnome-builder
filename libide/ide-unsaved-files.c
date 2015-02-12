/* ide-unsaved-files.c
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

#include <string.h>

#include "ide-context.h"
#include "ide-global.h"
#include "ide-internal.h"
#include "ide-project.h"
#include "ide-unsaved-file.h"
#include "ide-unsaved-files.h"

typedef struct
{
  gint64  sequence;
  GFile  *file;
  GBytes *content;
} UnsavedFile;

typedef struct
{
  GPtrArray *unsaved_files;
  gint64     sequence;
} IdeUnsavedFilesPrivate;

typedef struct
{
  GPtrArray *unsaved_files;
  gchar     *drafts_directory;
} AsyncState;

G_DEFINE_TYPE_WITH_PRIVATE (IdeUnsavedFiles, ide_unsaved_files, IDE_TYPE_OBJECT)

static void
async_state_free (gpointer data)
{
  AsyncState *state = data;

  if (state)
    {
      g_free (state->drafts_directory);
      g_ptr_array_unref (state->unsaved_files);
      g_slice_free (AsyncState, state);
    }
}

static void
unsaved_file_free (gpointer data)
{
  UnsavedFile *uf = data;

  if (uf)
    {
      g_object_unref (uf->file);
      g_bytes_unref (uf->content);
      g_slice_free (UnsavedFile, uf);
    }
}

static UnsavedFile *
unsaved_file_copy (const UnsavedFile *uf)
{
  UnsavedFile *copy;

  copy = g_slice_new0 (UnsavedFile);
  copy->file = g_object_ref (uf->file);
  copy->content = g_bytes_ref (uf->content);

  return copy;
}

static gboolean
unsaved_file_save (UnsavedFile  *uf,
                   const gchar  *path,
                   GError      **error)
{
  gboolean ret;

  ret = g_file_set_contents (path,
                             g_bytes_get_data (uf->content, NULL),
                             g_bytes_get_size (uf->content),
                             error);
  return ret;
}

static gchar *
hash_uri (const gchar *uri)
{
  GChecksum *checksum;
  gchar *ret;

  checksum = g_checksum_new (G_CHECKSUM_SHA1);
  g_checksum_update (checksum, (guchar *)uri, strlen (uri));
  ret = g_strdup (g_checksum_get_string (checksum));
  g_checksum_free (checksum);

  return ret;
}

static void
ide_unsaved_files_save_worker (GTask        *task,
                               gpointer      source_object,
                               gpointer      task_data,
                               GCancellable *cancellable)
{
  GString *manifest;
  AsyncState *state = task_data;
  g_autoptr(gchar) manifest_path = NULL;
  GError *error = NULL;
  gsize i;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_UNSAVED_FILES (source_object));
  g_assert (state);

  manifest = g_string_new (NULL);
  manifest_path = g_build_filename (state->drafts_directory,
                                    "manifest",
                                    NULL);

  for (i = 0; i < state->unsaved_files->len; i++)
    {
      g_autoptr(gchar) path = NULL;
      g_autoptr(gchar) uri = NULL;
      g_autoptr(gchar) hash = NULL;
      UnsavedFile *uf;

      uf = g_ptr_array_index (state->unsaved_files, i);

      uri = g_file_get_uri (uf->file);

      g_string_append_printf (manifest, "%s\n", uri);

      hash = hash_uri (uri);
      path = g_build_filename (state->drafts_directory, hash, NULL);

      if (!unsaved_file_save (uf, path, &error))
        {
          g_task_return_error (task, error);
          goto cleanup;
        }
    }

  if (!g_file_set_contents (manifest_path,
                            manifest->str, manifest->len,
                            &error))
    {
      g_task_return_error (task, error);
      goto cleanup;
    }

  g_task_return_boolean (task, TRUE);

cleanup:
  g_string_free (manifest, TRUE);
}

static AsyncState *
async_state_new (IdeUnsavedFiles *files)
{
  IdeContext *context;
  IdeProject *project;
  AsyncState *state;
  const gchar *project_name;

  g_assert (IDE_IS_UNSAVED_FILES (files));

  context = ide_object_get_context (IDE_OBJECT (files));
  project = ide_context_get_project (context);
  project_name = ide_project_get_name (project);

  state = g_slice_new (AsyncState);
  state->unsaved_files = g_ptr_array_new_with_free_func (unsaved_file_free);
  state->drafts_directory = g_build_filename (g_get_user_data_dir (),
                                              ide_get_program_name (),
                                              "drafts",
                                              project_name,
                                              NULL);

  return state;
}

void
ide_unsaved_files_save_async (IdeUnsavedFiles     *files,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  IdeUnsavedFilesPrivate *priv;
  g_autoptr(GTask) task;
  AsyncState *state;
  gsize i;

  g_return_if_fail (IDE_IS_UNSAVED_FILES (files));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  priv = ide_unsaved_files_get_instance_private (files);

  state = async_state_new (files);

  for (i = 0; i < priv->unsaved_files->len; i++)
    {
      UnsavedFile *uf;
      UnsavedFile *uf_copy;

      uf = g_ptr_array_index (priv->unsaved_files, i);
      uf_copy = unsaved_file_copy (uf);
      g_ptr_array_add (state->unsaved_files, uf_copy);
    }

  task = g_task_new (files, cancellable, callback, user_data);
  g_task_set_task_data (task, state, async_state_free);
  g_task_run_in_thread (task, ide_unsaved_files_save_worker);
}

gboolean
ide_unsaved_files_save_finish (IdeUnsavedFiles  *files,
                               GAsyncResult     *result,
                               GError          **error)
{
  g_return_val_if_fail (IDE_IS_UNSAVED_FILES (files), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
ide_unsaved_files_restore_worker (GTask        *task,
                                  gpointer      source_object,
                                  gpointer      task_data,
                                  GCancellable *cancellable)
{
  AsyncState *state = task_data;
  g_autoptr(gchar) contents = NULL;
  g_autoptr(gchar) manifest_path = NULL;
  gchar **lines;
  GError *error = NULL;
  gsize len;
  gsize i;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_UNSAVED_FILES (source_object));
  g_assert (state);

  manifest_path = g_build_filename (state->drafts_directory,
                                    "manifest",
                                    NULL);

  if (!g_file_test (manifest_path, G_FILE_TEST_IS_REGULAR))
    {
      g_task_return_boolean (task, TRUE);
      return;
    }

  if (!g_file_get_contents (manifest_path, &contents, &len, &error))
    {
      g_task_return_error (task, error);
      return;
    }

  lines = g_strsplit (contents, "\n", 0);

  for (i = 0; lines [i]; i++)
    {
      g_autoptr(GFile) file = NULL;
      g_autoptr(gchar) contents = NULL;
      g_autoptr(gchar) hash = NULL;
      g_autoptr(gchar) path = NULL;
      UnsavedFile *unsaved;
      gsize len;

      if (!*lines [i])
        continue;

      file = g_file_new_for_uri (lines [i]);
      if (!file)
        continue;

      hash = hash_uri (lines [i]);
      path = g_build_filename (state->drafts_directory, hash, NULL);

      if (!g_file_get_contents (path, &contents, &len, &error))
        {
          g_task_return_error (task, error);
          break;
        }

      unsaved = g_slice_new0 (UnsavedFile);
      unsaved->file = g_object_ref (file);
      unsaved->content = g_bytes_new_take (contents, len);

      g_ptr_array_add (state->unsaved_files, unsaved);
    }

  g_strfreev (lines);
}

void
ide_unsaved_files_restore_async (IdeUnsavedFiles     *files,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  g_autoptr(GTask) task;
  AsyncState *state;

  g_return_if_fail (IDE_IS_UNSAVED_FILES (files));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback);

  state = async_state_new (files);

  task = g_task_new (files, cancellable, callback, user_data);
  g_task_set_task_data (task, state, async_state_free);
  g_task_run_in_thread (task, ide_unsaved_files_restore_worker);
}

gboolean
ide_unsaved_files_restore_finish (IdeUnsavedFiles  *files,
                                  GAsyncResult     *result,
                                  GError          **error)
{
  AsyncState *state;
  gsize i;

  g_return_val_if_fail (IDE_IS_UNSAVED_FILES (files), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  state = g_task_get_task_data (G_TASK (result));

  for (i = 0; i < state->unsaved_files->len; i++)
    {
      UnsavedFile *uf;

      uf = g_ptr_array_index (state->unsaved_files, i);
      ide_unsaved_files_update (files, uf->file, uf->content);
    }

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
ide_unsaved_files_move_to_front (IdeUnsavedFiles *self,
                                 guint            index)
{
  IdeUnsavedFilesPrivate *priv = ide_unsaved_files_get_instance_private (self);
  UnsavedFile *new_front;
  UnsavedFile *old_front;

  g_return_if_fail (IDE_IS_UNSAVED_FILES (self));

  new_front = g_ptr_array_index (priv->unsaved_files, index);
  old_front = g_ptr_array_index (priv->unsaved_files, 0);

  /*
   * TODO: We could shift all these items down, but it probably isnt' worth
   *       the effort. We will just move-to-front after a miss and ping
   *       pong the old item back to the front.
   */
  priv->unsaved_files->pdata[0] = new_front;
  priv->unsaved_files->pdata[index] = old_front;
}

void
ide_unsaved_files_remove (IdeUnsavedFiles *self,
                          GFile           *file)
{
  IdeUnsavedFilesPrivate *priv = ide_unsaved_files_get_instance_private (self);
  guint i;

  g_return_if_fail (IDE_IS_UNSAVED_FILES (self));
  g_return_if_fail (G_IS_FILE (file));

  for (i = 0; i < priv->unsaved_files->len; i++)
    {
      UnsavedFile *unsaved;

      unsaved = g_ptr_array_index (priv->unsaved_files, i);

      if (g_file_equal (file, unsaved->file))
        {
          g_ptr_array_remove_index_fast (priv->unsaved_files, i);
          break;
        }
    }
}

void
ide_unsaved_files_update (IdeUnsavedFiles *self,
                          GFile           *file,
                          GBytes          *content)
{
  IdeUnsavedFilesPrivate *priv = ide_unsaved_files_get_instance_private (self);
  UnsavedFile *unsaved;
  guint i;

  g_return_if_fail (IDE_IS_UNSAVED_FILES (self));
  g_return_if_fail (G_IS_FILE (file));

  priv->sequence++;

  if (!content)
    {
      ide_unsaved_files_remove (self, file);
      return;
    }

  for (i = 0; i < priv->unsaved_files->len; i++)
    {
      unsaved = g_ptr_array_index (priv->unsaved_files, i);

      if (g_file_equal (file, unsaved->file))
        {
          if (content != unsaved->content)
            {
              g_clear_pointer (&unsaved->content, g_bytes_unref);
              unsaved->content = g_bytes_ref (content);
              unsaved->sequence = priv->sequence;
            }

          /*
           * A file that get's updated is the most likely to get updated on
           * the next attempt. Therefore, we will simply move this entry to
           * the beginning of the array to increase it's chances of being the
           * first entry we check.
           */
          if (i != 0)
            ide_unsaved_files_move_to_front (self, i);

          return;
        }
    }

  unsaved = g_slice_new0 (UnsavedFile);
  unsaved->file = g_object_ref (file);
  unsaved->content = g_bytes_ref (content);
  unsaved->sequence = priv->sequence;

  g_ptr_array_insert (priv->unsaved_files, 0, unsaved);
}

/**
 * ide_unsaved_files_get_unsaved_files:
 *
 * This retrieves all of the unsaved file buffers known to the context.
 * These are handy if you need to pass modified state to parsers such as
 * clang.
 *
 * Call g_ptr_array_unref() on the resulting #GPtrArray when no longer in use.
 *
 * If you would like to hold onto an unsaved file instance, call
 * ide_unsaved_file_ref() to increment it's reference count.
 *
 * Returns: (transfer container) (element-type IdeUnsavedFile*): A #GPtrArray
 *   containing #IdeUnsavedFile elements.
 */
GPtrArray *
ide_unsaved_files_get_unsaved_files (IdeUnsavedFiles *self)
{
  IdeUnsavedFilesPrivate *priv;
  GPtrArray *ar;
  gsize i;

  g_return_val_if_fail (IDE_IS_UNSAVED_FILES (self), NULL);

  priv = ide_unsaved_files_get_instance_private (self);

  ar = g_ptr_array_new ();
  g_ptr_array_set_free_func (ar, (GDestroyNotify)ide_unsaved_file_unref);

  for (i = 0; i < priv->unsaved_files->len; i++)
    {
      IdeUnsavedFile *item;
      UnsavedFile *uf;

      uf = g_ptr_array_index (priv->unsaved_files, i);
      item = _ide_unsaved_file_new (uf->file, uf->content, uf->sequence);

      g_ptr_array_add (ar, item);
    }

  return ar;
}

gint64
ide_unsaved_files_get_sequence (IdeUnsavedFiles *self)
{
  IdeUnsavedFilesPrivate *priv = ide_unsaved_files_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_UNSAVED_FILES (self), -1);

  return priv->sequence;
}

static void
ide_unsaved_files_finalize (GObject *object)
{
  IdeUnsavedFiles *self = (IdeUnsavedFiles *)object;
  IdeUnsavedFilesPrivate *priv = ide_unsaved_files_get_instance_private (self);

  g_clear_pointer (&priv->unsaved_files, g_ptr_array_unref);

  G_OBJECT_CLASS (ide_unsaved_files_parent_class)->finalize (object);
}

static void
ide_unsaved_files_class_init (IdeUnsavedFilesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_unsaved_files_finalize;
}

static void
ide_unsaved_files_init (IdeUnsavedFiles *self)
{
  IdeUnsavedFilesPrivate *priv = ide_unsaved_files_get_instance_private (self);

  priv->unsaved_files = g_ptr_array_new_with_free_func (unsaved_file_free);
}
