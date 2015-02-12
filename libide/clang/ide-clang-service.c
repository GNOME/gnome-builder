/* ide-clang-service.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <clang-c/Index.h>
#include <glib/gi18n.h>

#include "ide-clang-private.h"
#include "ide-clang-service.h"
#include "ide-context.h"
#include "ide-file.h"
#include "ide-unsaved-file.h"
#include "ide-unsaved-files.h"

typedef struct
{
  GHashTable   *cached_units;
  GRWLock       cached_rwlock;
  CXIndex       index;
  GCancellable *cancellable;
} IdeClangServicePrivate;

typedef struct
{
  IdeFile    *file;
  CXIndex     index;
  gchar      *source_filename;
  gchar     **command_line_args;
  GPtrArray  *unsaved_files;
  gint64      sequence;
  guint       options;
} ParseRequest;

G_DEFINE_TYPE_WITH_PRIVATE (IdeClangService, ide_clang_service,
                            IDE_TYPE_SERVICE)

enum {
  PROP_0,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

static void
parse_request_free (gpointer data)
{
  ParseRequest *request = data;

  g_free (request->source_filename);
  g_strfreev (request->command_line_args);
  g_ptr_array_unref (request->unsaved_files);
  g_clear_object (&request->file);
  g_slice_free (ParseRequest, request);
}

static void
ide_clang_service_parse_worker (GTask        *task,
                                gpointer      source_object,
                                gpointer      task_data,
                                GCancellable *cancellable)
{
  IdeClangServicePrivate *priv;
  IdeClangTranslationUnit *ret;
  CXTranslationUnit tu = NULL;
  ParseRequest *request = task_data;
  IdeContext *context;
  struct CXUnsavedFile *unsaved_files;
  const gchar * const *argv;
  gsize argc = 0;
  const gchar *detail_error = NULL;
  enum CXErrorCode code;
  GArray *ar;
  gsize i;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_CLANG_SERVICE (source_object));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  priv = ide_clang_service_get_instance_private (source_object);

  ar = g_array_new (FALSE, FALSE, sizeof (struct CXUnsavedFile));

  for (i = 0; i < request->unsaved_files->len; i++)
    {
      IdeUnsavedFile *iuf = g_ptr_array_index (request->unsaved_files, i);
      struct CXUnsavedFile uf;
      GBytes *content;
      GFile *file;

      file = ide_unsaved_file_get_file (iuf);
      content = ide_unsaved_file_get_content (iuf);

      uf.Filename = g_file_get_path (ide_unsaved_file_get_file (iuf));
      uf.Contents = g_bytes_get_data (content, NULL);
      uf.Length = g_bytes_get_size (content);

      g_array_append_val (ar, uf);
    }

  argv = (const gchar * const *)request->command_line_args;
  argc = argv ? g_strv_length (request->command_line_args) : 0;

  code = clang_parseTranslationUnit2 (request->index,
                                      request->source_filename,
                                      argv, argc,
                                      (struct CXUnsavedFile *)ar->data,
                                      ar->len,
                                      request->options,
                                      &tu);

  switch (code)
    {
    case CXError_Success:
      break;

    case CXError_Failure:
      detail_error = _("Unknown failure");
      break;

    case CXError_Crashed:
      detail_error = _("Clang crashed");
      break;

    case CXError_InvalidArguments:
      detail_error = _("Invalid arguments");
      break;

    case CXError_ASTReadError:
      detail_error = _("AST read error");
      break;

    default:
      break;
    }

  if (!tu)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               _("Failed to create translation unit: %s"),
                               detail_error ? detail_error : "");
      goto cleanup;
    }

  context = ide_object_get_context (source_object);
  ret = _ide_clang_translation_unit_new (context, tu, request->sequence);

  g_rw_lock_writer_lock (&priv->cached_rwlock);
  g_hash_table_replace (priv->cached_units,
                        g_object_ref (request->file),
                        g_object_ref (ret));
  g_rw_lock_writer_unlock (&priv->cached_rwlock);

cleanup:
  g_array_unref (ar);
}

/**
 * ide_clang_service_get_translation_unit_async:
 * @min_sequence: The minimum change sequence number to reuse a cached unit.
 *
 * This function is used to asynchronously retrieve the translation unit for
 * a particular file.
 *
 * If the translation unit is up to date, then no parsing will occur and the
 * existing translation unit will be used.
 *
 * If the translation unit is out of date, then the source file(s) will be
 * parsed via clang_parseTranslationUnit() asynchronously.
 */
void
ide_clang_service_get_translation_unit_async (IdeClangService     *self,
                                              IdeFile             *file,
                                              gint64               min_sequence,
                                              GCancellable        *cancellable,
                                              GAsyncReadyCallback  callback,
                                              gpointer             user_data)
{
  IdeClangServicePrivate *priv = ide_clang_service_get_instance_private (self);
  g_autoptr(IdeClangTranslationUnit) cached = NULL;
  IdeUnsavedFiles *unsaved_files;
  IdeContext *context;
  g_autoptr(GTask) task = NULL;
  ParseRequest *request;
  const gchar *path;
  GFile *gfile;

  g_return_if_fail (IDE_IS_CLANG_SERVICE (self));

  task = g_task_new (self, cancellable, callback, user_data);
  context = ide_object_get_context (IDE_OBJECT (self));
  unsaved_files = ide_context_get_unsaved_files (context);

  g_rw_lock_reader_lock (&priv->cached_rwlock);
  cached = g_hash_table_lookup (priv->cached_units, file);
  if (cached)
    g_object_ref (cached);
  g_rw_lock_reader_unlock (&priv->cached_rwlock);

  if (min_sequence <= 0)
    min_sequence = ide_unsaved_files_get_sequence (unsaved_files);

  if (cached)
    {
      if (ide_clang_translation_unit_get_sequence (cached) >= min_sequence)
        {
          g_task_return_pointer (task, g_object_ref (cached), g_object_unref);
          return;
        }
    }

  gfile = ide_file_get_file (file);

  if (!gfile || !(path = g_file_get_path (gfile)))
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               _("File must be saved locally to parse."));
      return;
    }

  request = g_slice_new0 (ParseRequest);
  request->file = g_object_ref (file);
  request->index = priv->index;
  request->source_filename = g_strdup (path);
  request->command_line_args = NULL; /* TODO: Get from build system */
  request->unsaved_files = ide_unsaved_files_get_unsaved_files (unsaved_files);
  request->sequence = ide_unsaved_files_get_sequence (unsaved_files);
  request->options = 0;

  g_task_set_task_data (task, request, parse_request_free);
  g_task_run_in_thread (task, ide_clang_service_parse_worker);
}

IdeClangTranslationUnit *
ide_clang_service_get_translation_unit_finish (IdeClangService  *self,
                                               GAsyncResult     *result,
                                               GError          **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_CLANG_SERVICE (self), NULL);

  return g_task_propagate_pointer (task, error);
}

static void
ide_clang_service_start (IdeService *service)
{
  IdeClangService *self = (IdeClangService *)service;
  IdeClangServicePrivate *priv = ide_clang_service_get_instance_private (self);

  g_return_if_fail (IDE_IS_CLANG_SERVICE (self));
  g_return_if_fail (!priv->index);

  g_clear_object (&priv->cancellable);
  priv->cancellable = g_cancellable_new ();

  priv->index = clang_createIndex (0, 0);
  clang_CXIndex_setGlobalOptions (priv->index,
                                  CXGlobalOpt_ThreadBackgroundPriorityForAll);

  IDE_SERVICE_CLASS (ide_clang_service_parent_class)->start (service);
}

static void
ide_clang_service_stop (IdeService *service)
{
  IdeClangService *self = (IdeClangService *)service;
  IdeClangServicePrivate *priv = ide_clang_service_get_instance_private (self);

  g_return_if_fail (IDE_IS_CLANG_SERVICE (self));
  g_return_if_fail (!priv->index);

  g_cancellable_cancel (priv->cancellable);

  IDE_SERVICE_CLASS (ide_clang_service_parent_class)->start (service);
}

static void
ide_clang_service_dispose (GObject *object)
{
  IdeClangService *self = (IdeClangService *)object;
  IdeClangServicePrivate *priv = ide_clang_service_get_instance_private (self);

  g_clear_pointer (&priv->index, clang_disposeIndex);
  g_clear_object (&priv->cancellable);

  G_OBJECT_CLASS (ide_clang_service_parent_class)->dispose (object);
}

static void
ide_clang_service_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  IdeClangService *self = IDE_CLANG_SERVICE (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_clang_service_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  IdeClangService *self = IDE_CLANG_SERVICE (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_clang_service_class_init (IdeClangServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_clang_service_dispose;
  object_class->get_property = ide_clang_service_get_property;
  object_class->set_property = ide_clang_service_set_property;
}

static void
ide_clang_service_init (IdeClangService *self)
{
  IdeClangServicePrivate *priv = ide_clang_service_get_instance_private (self);

  g_rw_lock_init (&priv->cached_rwlock);

  priv->cached_units = g_hash_table_new_full ((GHashFunc)ide_file_hash,
                                              (GEqualFunc)ide_file_equal,
                                              g_object_unref,
                                              g_object_unref);
}
