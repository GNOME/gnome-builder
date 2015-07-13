/* ide-ctags-service.c
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

#define G_LOG_DOMAIN "ide-ctags-service"

#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>

#include "egg-task-cache.h"

#include "ide-context.h"
#include "ide-ctags-builder.h"
#include "ide-ctags-completion-provider.h"
#include "ide-ctags-service.h"
#include "ide-ctags-index.h"
#include "ide-debug.h"
#include "ide-global.h"
#include "ide-project.h"
#include "ide-vcs.h"
#include "ide-ctags-highlighter.h"

struct _IdeCtagsService
{
  IdeObject                    parent_instance;

  EggTaskCache                *indexes;
  GCancellable                *cancellable;
  IdeCtagsBuilder             *builder;
  GPtrArray                   *highlighters;
  GPtrArray                   *completions;

  guint                        miner_ran : 1;
};

static void service_iface_init (IdeServiceInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (IdeCtagsService, ide_ctags_service, IDE_TYPE_OBJECT, 0,
                                G_IMPLEMENT_INTERFACE (IDE_TYPE_SERVICE, service_iface_init))

static void
ide_ctags_service_build_index_init_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  IdeCtagsIndex *index = (IdeCtagsIndex *)object;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;

  g_assert (IDE_IS_CTAGS_INDEX (index));
  g_assert (G_IS_TASK (task));

  if (!g_async_initable_init_finish (G_ASYNC_INITABLE (index), result, &error))
    g_task_return_error (task, error);
  else
    g_task_return_pointer (task, g_object_ref (index), g_object_unref);
}

static void
ide_ctags_service_build_index_cb (EggTaskCache  *cache,
                                  gconstpointer  key,
                                  GTask         *task,
                                  gpointer       user_data)
{
  IdeCtagsService *self = user_data;
  g_autoptr(IdeCtagsIndex) index = NULL;
  GFile *file = (GFile *)key;
  g_autofree gchar *uri = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_CTAGS_SERVICE (self));
  g_assert (key != NULL);
  g_assert (G_IS_FILE (key));
  g_assert (G_IS_TASK (task));

  index = ide_ctags_index_new (file);

  uri = g_file_get_uri (file);
  g_debug ("Building ctags in memory index for %s", uri);

  g_async_initable_init_async (G_ASYNC_INITABLE (index),
                               G_PRIORITY_DEFAULT,
                               g_task_get_cancellable (task),
                               ide_ctags_service_build_index_init_cb,
                               g_object_ref (task));

  IDE_EXIT;
}

static void
ide_ctags_service_tags_loaded_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  EggTaskCache *cache = (EggTaskCache *)object;
  g_autoptr(IdeCtagsService) self = user_data;
  g_autoptr(IdeCtagsIndex) index = NULL;
  GError *error = NULL;
  gsize i;

  IDE_ENTRY;

  g_assert (EGG_IS_TASK_CACHE (cache));
  g_assert (IDE_IS_CTAGS_SERVICE (self));

  if (!(index = egg_task_cache_get_finish (cache, result, &error)))
    {
      g_debug ("%s", error->message);
      g_clear_error (&error);
      IDE_EXIT;
    }

  g_assert (IDE_IS_CTAGS_INDEX (index));

  for (i = 0; i < self->highlighters->len; i++)
    {
      IdeCtagsHighlighter *highlighter = g_ptr_array_index (self->highlighters, i);
      ide_ctags_highlighter_add_index (highlighter, index);
    }

  for (i = 0; i < self->completions->len; i++)
    {
      IdeCtagsCompletionProvider *provider = g_ptr_array_index (self->completions, i);
      ide_ctags_completion_provider_add_index (provider, index);
    }

  IDE_EXIT;
}

static gboolean
do_load (gpointer data)
{
  struct {
    IdeCtagsService *self;
    GFile *file;
  } *pair = data;

  egg_task_cache_get_async (pair->self->indexes,
                            pair->file,
                            TRUE,
                            pair->self->cancellable,
                            ide_ctags_service_tags_loaded_cb,
                            g_object_ref (pair->self));

  g_object_unref (pair->self);
  g_object_unref (pair->file);
  g_slice_free1 (sizeof *pair, pair);

  return G_SOURCE_REMOVE;
}

static void
ide_ctags_service_load_tags (IdeCtagsService *self,
                             GFile           *file)
{
  struct {
    IdeCtagsService *self;
    GFile *file;
  } *pair;

  g_assert (IDE_IS_CTAGS_SERVICE (self));
  g_assert (G_IS_FILE (file));

  pair = g_slice_alloc0 (sizeof *pair);
  pair->self = g_object_ref (self);
  pair->file = g_object_ref (file);
  g_timeout_add (0, do_load, pair);
}

static void
ide_ctags_service_mine_directory (IdeCtagsService *self,
                                  GFile           *directory,
                                  gboolean         recurse,
                                  GCancellable    *cancellable)
{
  GFileEnumerator *enumerator = NULL;
  gpointer infoptr;
  GFile *child;

  g_assert (IDE_IS_CTAGS_SERVICE (self));
  g_assert (G_IS_FILE (directory));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (g_cancellable_is_cancelled (cancellable))
    return;

  child = g_file_get_child (directory, "tags");
  if (g_file_query_file_type (child, 0, cancellable) == G_FILE_TYPE_REGULAR)
    ide_ctags_service_load_tags (self, child);
  g_clear_object (&child);

  child = g_file_get_child (directory, ".tags");
  if (g_file_query_file_type (child, 0, cancellable) == G_FILE_TYPE_REGULAR)
    ide_ctags_service_load_tags (self, child);
  g_clear_object (&child);

  if (!recurse)
    return;

  enumerator = g_file_enumerate_children (directory,
                                          G_FILE_ATTRIBUTE_STANDARD_NAME","
                                          G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                          G_FILE_QUERY_INFO_NONE,
                                          cancellable,
                                          NULL);

  while ((infoptr = g_file_enumerator_next_file (enumerator, cancellable, NULL)))
    {
      g_autoptr(GFileInfo) file_info = infoptr;
      const gchar *name = g_file_info_get_name (file_info);
      GFileType type = g_file_info_get_file_type (file_info);

      if (type == G_FILE_TYPE_DIRECTORY)
        {
          child = g_file_get_child (directory, name);
          ide_ctags_service_mine_directory (self, child, recurse, cancellable);
          g_clear_object (&child);
        }
    }

  g_file_enumerator_close (enumerator, cancellable, NULL);

  g_object_unref (enumerator);
}

static void
ide_ctags_service_miner (GTask        *task,
                         gpointer      source_object,
                         gpointer      task_data,
                         GCancellable *cancellable)
{
  g_autofree gchar *project_tags = NULL;
  IdeCtagsService *self = source_object;
  IdeContext *context;
  IdeProject *project;
  IdeVcs *vcs;
  GFile *file;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_CTAGS_SERVICE (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  vcs = ide_context_get_vcs (context);
  project = ide_context_get_project (context);
  project_tags = g_build_filename (g_get_user_cache_dir (),
                                   ide_get_program_name (),
                                   ide_project_get_id (project),
                                   "tags",
                                   NULL);

  /* mine ~/.cache/gnome-builder/<name>/tags */
  file = g_file_new_for_path (project_tags);
  ide_ctags_service_load_tags (self, file);
  g_object_unref (file);

  /* mine the project tree */
  file = g_object_ref (ide_vcs_get_working_directory (vcs));
  ide_ctags_service_mine_directory (self, file, TRUE, cancellable);
  g_object_unref (file);

  /* mine ~/.tags */
  file = g_file_new_for_path (g_get_home_dir ());
  ide_ctags_service_mine_directory (self, file, FALSE, cancellable);
  g_object_unref (file);

  /* mine /usr/include */
  file = g_file_new_for_path ("/usr/include");
  ide_ctags_service_mine_directory (self, file, TRUE, cancellable);
  g_object_unref (file);

  ide_object_release (IDE_OBJECT (self));
}

static void
ide_ctags_service_mine (IdeCtagsService *self)
{
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_CTAGS_SERVICE (self));

  /* prevent unloading until we release from worker thread */
  ide_object_hold (IDE_OBJECT (self));

  self->cancellable = g_cancellable_new ();

  task = g_task_new (self, self->cancellable, NULL, NULL);
  g_task_run_in_thread (task, ide_ctags_service_miner);
}

static void
ide_ctags_service_tags_built_cb (IdeCtagsService *self,
                                 GFile           *tags_file,
                                 IdeCtagsBuilder *builder)
{
  IDE_ENTRY;

  g_assert (IDE_IS_CTAGS_SERVICE (self));
  g_assert (G_IS_FILE (tags_file));
  g_assert (IDE_IS_CTAGS_BUILDER (builder));

  egg_task_cache_get_async (self->indexes,
                            tags_file,
                            TRUE,
                            self->cancellable,
                            ide_ctags_service_tags_loaded_cb,
                            g_object_ref (self));

  IDE_EXIT;
}

static void
ide_ctags_service_loaded (IdeService *service)
{
  IdeCtagsService *self = (IdeCtagsService *)service;

  g_assert (IDE_IS_CTAGS_SERVICE (self));

  ide_ctags_service_mine (self);
}

static void
ide_ctags_service_start (IdeService *service)
{
  IdeCtagsService *self = (IdeCtagsService *)service;
  IdeContext *context;

  g_return_if_fail (IDE_IS_CTAGS_SERVICE (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  self->builder = g_object_new (IDE_TYPE_CTAGS_BUILDER,
                                "context", context,
                                NULL);
  g_signal_connect_object (self->builder,
                           "tags-built",
                           G_CALLBACK (ide_ctags_service_tags_built_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
ide_ctags_service_stop (IdeService *service)
{
  IdeCtagsService *self = (IdeCtagsService *)service;

  g_return_if_fail (IDE_IS_CTAGS_SERVICE (self));

  if (self->cancellable && !g_cancellable_is_cancelled (self->cancellable))
    g_cancellable_cancel (self->cancellable);

  g_clear_object (&self->cancellable);
  g_clear_object (&self->builder);
}

static void
ide_ctags_service_finalize (GObject *object)
{
  IdeCtagsService *self = (IdeCtagsService *)object;

  IDE_ENTRY;

  g_clear_object (&self->indexes);
  g_clear_object (&self->cancellable);
  g_clear_pointer (&self->highlighters, g_ptr_array_unref);
  g_clear_pointer (&self->completions, g_ptr_array_unref);

  G_OBJECT_CLASS (ide_ctags_service_parent_class)->finalize (object);

  IDE_EXIT;
}

static void
ide_ctags_service_class_init (IdeCtagsServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_ctags_service_finalize;
}

static void
service_iface_init (IdeServiceInterface *iface)
{
  iface->loaded = ide_ctags_service_loaded;
  iface->start = ide_ctags_service_start;
  iface->stop = ide_ctags_service_stop;
}

static void
ide_ctags_service_class_finalize (IdeCtagsServiceClass *klass)
{
}

static void
ide_ctags_service_init (IdeCtagsService *self)
{
  self->highlighters = g_ptr_array_new ();
  self->completions = g_ptr_array_new ();

  self->indexes = egg_task_cache_new ((GHashFunc)g_file_hash,
                                      (GEqualFunc)g_file_equal,
                                      g_object_ref,
                                      g_object_unref,
                                      g_object_ref,
                                      g_object_unref,
                                      0,
                                      ide_ctags_service_build_index_cb,
                                      self,
                                      NULL);
}

void
_ide_ctags_service_register_type (GTypeModule *module)
{
  ide_ctags_service_register_type (module);
}

void
ide_ctags_service_register_highlighter (IdeCtagsService     *self,
                                        IdeCtagsHighlighter *highlighter)
{
  g_autoptr(GPtrArray) values = NULL;
  gsize i;

  g_return_if_fail (IDE_IS_CTAGS_SERVICE (self));
  g_return_if_fail (IDE_IS_CTAGS_HIGHLIGHTER (highlighter));

  values = egg_task_cache_get_values (self->indexes);

  for (i = 0; i < values->len; i++)
    {
      IdeCtagsIndex *index = g_ptr_array_index (values, i);
      ide_ctags_highlighter_add_index (highlighter, index);
    }

  g_ptr_array_add (self->highlighters, highlighter);
}

void
ide_ctags_service_unregister_highlighter (IdeCtagsService     *self,
                                          IdeCtagsHighlighter *highlighter)
{
  g_return_if_fail (IDE_IS_CTAGS_SERVICE (self));
  g_return_if_fail (IDE_IS_CTAGS_HIGHLIGHTER (highlighter));

  g_ptr_array_remove (self->highlighters, highlighter);
}

void
ide_ctags_service_register_completion (IdeCtagsService            *self,
                                       IdeCtagsCompletionProvider *completion)
{
  g_autoptr(GPtrArray) values = NULL;
  gsize i;

  g_return_if_fail (IDE_IS_CTAGS_SERVICE (self));
  g_return_if_fail (IDE_IS_CTAGS_COMPLETION_PROVIDER (completion));

  values = egg_task_cache_get_values (self->indexes);

  for (i = 0; i < values->len; i++)
    {
      IdeCtagsIndex *index = g_ptr_array_index (values, i);
      ide_ctags_completion_provider_add_index (completion, index);
    }

  g_ptr_array_add (self->completions, completion);
}

void
ide_ctags_service_unregister_completion (IdeCtagsService            *self,
                                         IdeCtagsCompletionProvider *completion)
{
  g_return_if_fail (IDE_IS_CTAGS_SERVICE (self));
  g_return_if_fail (IDE_IS_CTAGS_COMPLETION_PROVIDER (completion));

  g_ptr_array_remove (self->completions, completion);
}
