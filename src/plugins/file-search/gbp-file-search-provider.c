/* gbp-file-search-provider.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "gbp-file-search-provider"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-code.h>
#include <libide-projects.h>
#include <libide-search.h>
#include <libide-vcs.h>
#include <libpeas/peas.h>

#include "gbp-file-search-provider.h"
#include "gbp-file-search-index.h"

struct _GbpFileSearchProvider
{
  IdeObject           parent_instance;
  GbpFileSearchIndex *index;
};

static void search_provider_iface_init (IdeSearchProviderInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GbpFileSearchProvider,
                         gbp_file_search_provider,
                         IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_SEARCH_PROVIDER, search_provider_iface_init))

static void
gbp_file_search_provider_search_async (IdeSearchProvider   *provider,
                                      const gchar         *search_terms,
                                      guint                max_results,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  GbpFileSearchProvider *self = (GbpFileSearchProvider *)provider;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GPtrArray) results = NULL;

  g_assert (GBP_IS_FILE_SEARCH_PROVIDER (self));
  g_assert (search_terms != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_file_search_provider_search_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  if (self->index != NULL)
    results = gbp_file_search_index_populate (self->index, search_terms, max_results);
  else
    results = g_ptr_array_new_with_free_func (g_object_unref);

  ide_task_return_pointer (task, g_steal_pointer (&results), (GDestroyNotify)g_ptr_array_unref);
}

static GPtrArray *
gbp_file_search_provider_search_finish (IdeSearchProvider  *provider,
                                       GAsyncResult       *result,
                                       GError            **error)
{
  GPtrArray *ret;

  g_assert (GBP_IS_FILE_SEARCH_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  return IDE_PTR_ARRAY_STEAL_FULL (&ret);
}

static void
on_buffer_loaded (GbpFileSearchProvider *self,
                  IdeBuffer            *buffer,
                  IdeBufferManager     *bufmgr)
{
  g_autofree gchar *relative_path = NULL;
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(GFile) workdir = NULL;
  IdeVcs *vcs;
  GFile *file;

  g_assert (GBP_IS_FILE_SEARCH_PROVIDER (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (IDE_IS_BUFFER_MANAGER (bufmgr));

  if (self->index == NULL)
    return;

  file = ide_buffer_get_file (buffer);
  context = ide_buffer_ref_context (buffer);
  vcs = ide_vcs_from_context (context);
  workdir = ide_context_ref_workdir (context);
  relative_path = g_file_get_relative_path (workdir, file);

  if ((relative_path != NULL) &&
      !ide_vcs_is_ignored (vcs, file, NULL) &&
      !gbp_file_search_index_contains (self->index, relative_path))
    gbp_file_search_index_insert (self->index, relative_path);
}

static void
on_file_renamed (GbpFileSearchProvider *self,
                 GFile                *src_file,
                 GFile                *dst_file,
                 IdeProject           *project)
{
  g_autofree gchar *old_path = NULL;
  g_autofree gchar *new_path = NULL;
  g_autoptr(GFile) workdir = NULL;
  IdeContext *context;

  g_assert (GBP_IS_FILE_SEARCH_PROVIDER (self));
  g_assert (G_IS_FILE (src_file));
  g_assert (G_IS_FILE (dst_file));
  g_assert (IDE_IS_PROJECT (project));
  g_assert (GBP_IS_FILE_SEARCH_INDEX (self->index));

  context = ide_object_get_context (IDE_OBJECT (project));
  workdir = ide_context_ref_workdir (context);

  if (NULL != (old_path = g_file_get_relative_path (workdir, src_file)))
    gbp_file_search_index_remove (self->index, old_path);

  if (NULL != (new_path = g_file_get_relative_path (workdir, dst_file)))
    gbp_file_search_index_insert (self->index, new_path);
}

static void
on_file_trashed (GbpFileSearchProvider *self,
                 GFile                *file,
                 IdeProject           *project)
{
  g_autofree gchar *path = NULL;
  g_autoptr(GFile) workdir = NULL;
  IdeContext *context;

  g_assert (GBP_IS_FILE_SEARCH_PROVIDER (self));
  g_assert (G_IS_FILE (file));
  g_assert (IDE_IS_PROJECT (project));
  g_assert (GBP_IS_FILE_SEARCH_INDEX (self->index));

  context = ide_object_get_context (IDE_OBJECT (project));
  workdir = ide_context_ref_workdir (context);

  if (NULL != (path = g_file_get_relative_path (workdir, file)))
    gbp_file_search_index_remove (self->index, path);
}

static void
gbp_file_search_provider_build_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  GbpFileSearchIndex *index = (GbpFileSearchIndex *)object;
  g_autoptr(GbpFileSearchProvider) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (GBP_IS_FILE_SEARCH_INDEX (index));
  g_assert (GBP_IS_FILE_SEARCH_PROVIDER (self));

  if (!gbp_file_search_index_build_finish (index, result, &error))
    {
      g_warning ("%s", error->message);
      return;
    }

  g_set_object (&self->index, index);
}

#if 0
static void
gbp_file_search_provider_activate (IdeSearchProvider *provider,
                                  GtkWidget         *row,
                                  IdeSearchResult   *result)
{
  GtkWidget *toplevel;

  g_assert (IDE_IS_SEARCH_PROVIDER (provider));
  g_assert (GTK_IS_WIDGET (row));
  g_assert (IDE_IS_SEARCH_RESULT (result));

  toplevel = gtk_widget_get_toplevel (row);

  if (IDE_IS_WORKBENCH (toplevel))
    {
      g_autofree gchar *path = NULL;
      g_autoptr(GFile) file = NULL;
      g_autoptr(GFile) workdir = NULL;
      IdeContext *context;
      IdeVcs *vcs;

      context = ide_workbench_get_context (IDE_WORKBENCH (toplevel));
      vcs = ide_vcs_from_context (context);
      workdir = ide_context_ref_workdir (context);
      g_object_get (result, "path", &path, NULL);
      file = g_file_get_child (workdir, path);

      ide_workbench_open_files_async (IDE_WORKBENCH (toplevel),
                                      &file,
                                      1,
                                      NULL,
                                      IDE_WORKBENCH_OPEN_FLAGS_NONE,
                                      NULL,
                                      NULL,
                                      NULL);
    }
}
#endif

static void
gbp_file_search_provider_vcs_changed_cb (GbpFileSearchProvider *self,
                                         IdeVcs                *vcs)
{
  g_autoptr(GbpFileSearchIndex) index = NULL;
  g_autoptr(GFile) workdir = NULL;
  IdeContext *context;

  IDE_ENTRY;

  g_return_if_fail (GBP_IS_FILE_SEARCH_PROVIDER (self));
  g_return_if_fail (IDE_IS_VCS (vcs));

  context = ide_object_get_context (IDE_OBJECT (self));
  workdir = ide_context_ref_workdir (context);

  index = g_object_new (GBP_TYPE_FILE_SEARCH_INDEX,
                        "root-directory", workdir,
                        NULL);

  ide_object_append (IDE_OBJECT (self), IDE_OBJECT (index));

  gbp_file_search_index_build_async (index,
                                     NULL,
                                     gbp_file_search_provider_build_cb,
                                     g_object_ref (self));

  IDE_EXIT;
}

static void
gbp_file_search_provider_parent_set (IdeObject *object,
                                     IdeObject *parent)
{
  GbpFileSearchProvider *self = (GbpFileSearchProvider *)object;
  g_autoptr(GbpFileSearchIndex) index = NULL;
  g_autoptr(GFile) workdir = NULL;
  IdeBufferManager *bufmgr;
  IdeContext *context;
  IdeProject *project;
  IdeVcs *vcs;

  g_assert (GBP_IS_FILE_SEARCH_PROVIDER (self));
  g_assert (!parent || IDE_IS_OBJECT (parent));

  if (parent == NULL)
    return;

  context = ide_object_get_context (IDE_OBJECT (self));

  bufmgr = ide_buffer_manager_from_context (context);
  project = ide_project_from_context (context);
  vcs = ide_vcs_from_context (context);

  workdir = ide_context_ref_workdir (context);

  g_signal_connect_object (vcs,
                           "changed",
                           G_CALLBACK (gbp_file_search_provider_vcs_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (bufmgr,
                           "buffer-loaded",
                           G_CALLBACK (on_buffer_loaded),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (project,
                           "file-renamed",
                           G_CALLBACK (on_file_renamed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (project,
                           "file-trashed",
                           G_CALLBACK (on_file_trashed),
                           self,
                           G_CONNECT_SWAPPED);

  index = g_object_new (GBP_TYPE_FILE_SEARCH_INDEX,
                        "root-directory", workdir,
                        NULL);

  ide_object_append (IDE_OBJECT (self), IDE_OBJECT (index));

  gbp_file_search_index_build_async (index,
                                     NULL,
                                     gbp_file_search_provider_build_cb,
                                     g_object_ref (self));
}

static void
gbp_file_search_provider_finalize (GObject *object)
{
  GbpFileSearchProvider *self = (GbpFileSearchProvider *)object;

  g_clear_object (&self->index);

  G_OBJECT_CLASS (gbp_file_search_provider_parent_class)->finalize (object);
}

static void
gbp_file_search_provider_class_init (GbpFileSearchProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  object_class->finalize = gbp_file_search_provider_finalize;

  i_object_class->parent_set = gbp_file_search_provider_parent_set;
}

static void
gbp_file_search_provider_init (GbpFileSearchProvider *self)
{
}

static void
search_provider_iface_init (IdeSearchProviderInterface *iface)
{
  iface->search_async = gbp_file_search_provider_search_async;
  iface->search_finish = gbp_file_search_provider_search_finish;
}

void
gbp_file_search_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_SEARCH_PROVIDER,
                                              GBP_TYPE_FILE_SEARCH_PROVIDER);
}
