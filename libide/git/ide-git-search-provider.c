/* ide-git-search-provider.c
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

#include <glib/gi18n.h>

#include "ide-context.h"
#include "ide-git-search-index.h"
#include "ide-git-search-provider.h"
#include "ide-git-vcs.h"
#include "ide-search-context.h"

struct _IdeGitSearchProvider
{
  IdeSearchProvider  parent_instance;

  IdeGitSearchIndex *index;
};

typedef struct
{
  IdeSearchContext *context;
  gchar            *search_terms;
  gsize             max_results;
} PopulateState;

G_DEFINE_TYPE (IdeGitSearchProvider, ide_git_search_provider,
               IDE_TYPE_SEARCH_PROVIDER)

static void
ide_git_search_provider_get_index_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  g_autoptr(GTask) task = user_data;
  GAsyncInitable *initable = (GAsyncInitable *)object;
  IdeGitSearchProvider *self;
  g_autoptr(GObject) res = NULL;
  GError *error = NULL;

  g_assert (G_IS_TASK (task));
  g_assert (G_IS_ASYNC_RESULT (result));

  self = g_task_get_source_object (task);

  res = g_async_initable_new_finish (initable, result, &error);

  if (!res)
    {
      g_task_return_error (task, error);
      return;
    }

  g_clear_object (&self->index);
  self->index = g_object_ref (res);

  g_task_return_pointer (task, g_object_ref (self->index), g_object_unref);
}

static void
ide_git_search_provider_get_index_async (IdeGitSearchProvider *self,
                                         GCancellable         *cancellable,
                                         GAsyncReadyCallback   callback,
                                         gpointer              user_data)
{
  g_autoptr(GTask) task = NULL;
  g_autoptr(GFile) location = NULL;
  GgitRepository *repository;
  IdeContext *context;
  IdeVcs *vcs;

  g_return_if_fail (IDE_IS_GIT_SEARCH_PROVIDER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  if (self->index)
    {
      g_task_return_pointer (task, g_object_ref (self->index), g_object_unref);
      return;
    }

  context = ide_object_get_context (IDE_OBJECT (self));
  vcs = ide_context_get_vcs (context);

  if (!IDE_IS_GIT_VCS (vcs))
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               _("Git search provider requires the Git VCS"));
      return;
    }

  repository = ide_git_vcs_get_repository (IDE_GIT_VCS (vcs));
  location = ggit_repository_get_location (repository);

  g_async_initable_new_async (IDE_TYPE_GIT_SEARCH_INDEX,
                              G_PRIORITY_DEFAULT,
                              cancellable,
                              ide_git_search_provider_get_index_cb,
                              g_object_ref (task),
                              "context", context,
                              "location", location,
                              NULL);
}

static IdeGitSearchIndex *
ide_git_search_provider_get_index_finish (IdeGitSearchProvider  *self,
                                          GAsyncResult          *result,
                                          GError               **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_GIT_SEARCH_PROVIDER (self), NULL);
  g_return_val_if_fail (G_IS_TASK (task), NULL);

  return g_task_propagate_pointer (task, error);
}

static void
populate_get_index_cb (GObject      *object,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  IdeGitSearchProvider *self = (IdeGitSearchProvider *)object;
  PopulateState *state = user_data;
  g_autoptr(IdeGitSearchIndex) index = NULL;
  GError *error = NULL;

  g_assert (state);
  g_assert (IDE_IS_SEARCH_CONTEXT (state->context));
  g_assert (IDE_IS_SEARCH_PROVIDER (self));
  g_assert (state->search_terms);

  index = ide_git_search_provider_get_index_finish (self, result, &error);

  if (index)
    ide_git_search_index_populate (index,
                                   IDE_SEARCH_PROVIDER (self),
                                   state->context,
                                   state->max_results,
                                   state->search_terms);

  ide_search_context_provider_completed (state->context,
                                         IDE_SEARCH_PROVIDER (self));

  g_free (state->search_terms);
  g_object_unref (state->context);
  g_slice_free (PopulateState, state);
}

static void
ide_git_search_provider_populate (IdeSearchProvider *provider,
                                  IdeSearchContext  *context,
                                  const gchar       *search_terms,
                                  gsize              max_results,
                                  GCancellable      *cancellable)
{
  IdeGitSearchProvider *self = (IdeGitSearchProvider *)provider;
  PopulateState *state;

  g_return_if_fail (IDE_IS_GIT_SEARCH_PROVIDER (self));
  g_return_if_fail (IDE_IS_SEARCH_CONTEXT (context));
  g_return_if_fail (search_terms);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  state = g_slice_new0 (PopulateState);
  state->context = g_object_ref (context);
  state->search_terms = g_strdup (search_terms);
  state->max_results = max_results;

  ide_git_search_provider_get_index_async (self,
                                           cancellable,
                                           populate_get_index_cb,
                                           state);
}

static void
ide_git_search_provider_finalize (GObject *object)
{
  IdeGitSearchProvider *self = (IdeGitSearchProvider *)object;

  g_clear_object (&self->index);

  G_OBJECT_CLASS (ide_git_search_provider_parent_class)->finalize (object);
}

static void
ide_git_search_provider_class_init (IdeGitSearchProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeSearchProviderClass *provider_class = IDE_SEARCH_PROVIDER_CLASS (klass);

  object_class->finalize = ide_git_search_provider_finalize;

  provider_class->populate = ide_git_search_provider_populate;
}

static void
ide_git_search_provider_init (IdeGitSearchProvider *self)
{
}
