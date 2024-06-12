/*
 * gbp-git-staged-model.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#include <gio/gio.h>

#include <libgit2-glib/ggit.h>

#include "gbp-git-dex.h"
#include "gbp-git-staged-item.h"
#include "gbp-git-staged-model.h"
#include "gbp-git-vcs.h"

struct _GbpGitStagedModel
{
  GObject           parent_instance;
  IdeContext       *context;
  IpcGitRepository *repository;
  DexFuture        *update;
  GListModel       *model;
};

static guint
gbp_git_staged_model_get_n_items (GListModel *model)
{
  GbpGitStagedModel *self = GBP_GIT_STAGED_MODEL (model);

  return g_list_model_get_n_items (G_LIST_MODEL (self->model));
}

static GType
gbp_git_staged_model_get_item_type (GListModel *model)
{
  return GBP_TYPE_GIT_STAGED_ITEM;
}

static gpointer
gbp_git_staged_model_get_item (GListModel *model,
                               guint       position)
{
  GbpGitStagedModel *self = GBP_GIT_STAGED_MODEL (model);

  return g_list_model_get_item (G_LIST_MODEL (self->model), position);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_n_items = gbp_git_staged_model_get_n_items;
  iface->get_item = gbp_git_staged_model_get_item;
  iface->get_item_type = gbp_git_staged_model_get_item_type;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpGitStagedModel, gbp_git_staged_model, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

enum {
  PROP_0,
  PROP_CONTEXT,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

GbpGitStagedModel *
gbp_git_staged_model_new (IdeContext *context)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  return g_object_new (GBP_TYPE_GIT_STAGED_MODEL,
                       "context", context,
                       NULL);
}

static DexFuture *
gbp_git_staged_model_update_apply (DexFuture *completed,
                                   gpointer   user_data)
{
  GbpGitStagedModel *self = user_data;
  g_autoptr(GListModel) model = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (DEX_IS_FUTURE (completed));
  g_assert (GBP_IS_GIT_STAGED_MODEL (self));

  model = dex_await_object (dex_ref (completed), NULL);

  g_assert (G_IS_LIST_STORE (model));

  if (completed == self->update)
    {
      guint old_n_items = g_list_model_get_n_items (self->model);
      guint new_n_items = g_list_model_get_n_items (model);

      g_signal_handlers_disconnect_by_func (self->model,
                                            G_CALLBACK (g_list_model_items_changed),
                                            self);
      g_signal_connect_object (model,
                               "items-changed",
                               G_CALLBACK (g_list_model_items_changed),
                               self,
                               G_CONNECT_SWAPPED);

      g_set_object (&self->model, model);

      g_list_model_items_changed (G_LIST_MODEL (self), 0, old_n_items, new_n_items);
    }

  return NULL;
}

static DexFuture *
gbp_git_staged_model_update_fiber (gpointer user_data)
{
  IpcGitRepository *repository = user_data;
  g_autoptr(GListStore) store = NULL;
  g_autoptr(GVariant) files = NULL;
  g_autoptr(GError) error = NULL;
  GgitStatusOption status_option;
  g_autofree char *workdir = NULL;
  GVariantIter iter;
  const char *path;
  guint flags;

  g_assert (!IDE_IS_MAIN_THREAD ());
  g_assert (IPC_IS_GIT_REPOSITORY (repository));

  status_option = 0;

  if (!g_set_str (&workdir, ipc_git_repository_get_workdir (repository)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_INITIALIZED,
                                  "Git repository is in broken state");

  if (!(files = dex_await_variant (ipc_git_repository_list_status (repository, status_option, ""), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  store = g_list_store_new (GBP_TYPE_GIT_STAGED_ITEM);

  g_variant_iter_init (&iter, files);

  while (g_variant_iter_next (&iter, "(^&ayu)", &path, &flags))
    {
      g_autoptr(GbpGitStagedItem) item = NULL;
      g_autoptr(GFile) file = NULL;
      g_autofree char *title = NULL;

      if ((flags & (GGIT_STATUS_INDEX_NEW |
                    GGIT_STATUS_INDEX_MODIFIED |
                    GGIT_STATUS_INDEX_DELETED |
                    GGIT_STATUS_INDEX_RENAMED |
                    GGIT_STATUS_INDEX_TYPECHANGE)) == 0)
        continue;

      file = g_file_new_build_filename (workdir, path, NULL);
      title = g_filename_to_utf8 (path, -1, NULL, NULL, NULL);
      item = g_object_new (GBP_TYPE_GIT_STAGED_ITEM,
                           "file", file,
                           "title", title,
                           NULL);

      g_list_store_append (store, item);
    }

  return dex_future_new_take_object (g_steal_pointer (&store));
}

static void
gbp_git_staged_model_update (GbpGitStagedModel *self)
{
  IDE_ENTRY;

  g_assert (GBP_IS_GIT_STAGED_MODEL (self));
  g_assert (IPC_IS_GIT_REPOSITORY (self->repository));

  dex_clear (&self->update);

  self->update = dex_scheduler_spawn (dex_thread_pool_scheduler_get_default (),
                                      0,
                                      gbp_git_staged_model_update_fiber,
                                      g_object_ref (self->repository),
                                      g_object_unref);

  dex_future_disown (dex_future_then (dex_ref (self->update),
                                      gbp_git_staged_model_update_apply,
                                      g_object_ref (self),
                                      g_object_unref));

  IDE_EXIT;
}

static void
gbp_git_staged_model_repository_changed_cb (GbpGitStagedModel *self,
                                            IpcGitRepository  *repository)
{
  IDE_ENTRY;

  g_assert (GBP_IS_GIT_STAGED_MODEL (self));
  g_assert (IPC_IS_GIT_REPOSITORY (repository));

  gbp_git_staged_model_update (self);

  IDE_EXIT;
}

static void
gbp_git_staged_model_constructed (GObject *object)
{
  GbpGitStagedModel *self = (GbpGitStagedModel *)object;
  IpcGitRepository *repository;
  IdeVcs *vcs;

  G_OBJECT_CLASS (gbp_git_staged_model_parent_class)->constructed (object);

  if (!(vcs = ide_vcs_from_context (self->context)) ||
      !GBP_IS_GIT_VCS (vcs) ||
      !(repository = gbp_git_vcs_get_repository (GBP_GIT_VCS (vcs))))
    return;

  g_set_object (&self->repository, repository);

  g_signal_connect_object (repository,
                           "changed",
                           G_CALLBACK (gbp_git_staged_model_repository_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  gbp_git_staged_model_update (self);
}

static void
gbp_git_staged_model_dispose (GObject *object)
{
  GbpGitStagedModel *self = (GbpGitStagedModel *)object;

  g_clear_object (&self->context);
  g_clear_object (&self->repository);
  g_clear_object (&self->model);
  dex_clear (&self->update);

  G_OBJECT_CLASS (gbp_git_staged_model_parent_class)->dispose (object);
}

static void
gbp_git_staged_model_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GbpGitStagedModel *self = GBP_GIT_STAGED_MODEL (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_git_staged_model_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GbpGitStagedModel *self = GBP_GIT_STAGED_MODEL (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      self->context = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_git_staged_model_class_init (GbpGitStagedModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gbp_git_staged_model_constructed;
  object_class->dispose = gbp_git_staged_model_dispose;
  object_class->get_property = gbp_git_staged_model_get_property;
  object_class->set_property = gbp_git_staged_model_set_property;

  properties[PROP_CONTEXT] =
    g_param_spec_object ("context", NULL, NULL,
                         IDE_TYPE_CONTEXT,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_git_staged_model_init (GbpGitStagedModel *self)
{
  self->model = G_LIST_MODEL (g_list_store_new (GBP_TYPE_GIT_COMMIT_ITEM));
}
