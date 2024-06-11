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

#include <libgit2-glib/ggit.h>

#include "gbp-git-staged-model.h"
#include "gbp-git-vcs.h"

struct _GbpGitStagedModel
{
  GObject           parent_instance;
  IdeContext       *context;
  IpcGitRepository *repository;
  DexPromise       *update;
};

static guint
gbp_git_staged_model_get_n_items (GListModel *model)
{
  return 0;
}

static GType
gbp_git_staged_model_get_item_type (GListModel *model)
{
  return G_TYPE_OBJECT;
}

static gpointer
gbp_git_staged_model_get_item (GListModel *model,
                               guint       position)
{
  return NULL;
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

static void
gbp_git_staged_model_update_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  IpcGitRepository *repository = (IpcGitRepository *)object;
  g_autoptr(DexPromise) promise = user_data;
  g_autoptr(GVariant) files = NULL;
  g_autoptr(GError) error = NULL;
  GVariantIter iter;
  const char *path;
  guint flags;

  IDE_ENTRY;

  g_assert (IPC_IS_GIT_REPOSITORY (repository));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (DEX_IS_PROMISE (promise));

  if (!ipc_git_repository_call_list_status_finish (repository, &files, result, &error))
    {
      dex_promise_reject (promise, g_steal_pointer (&error));
      IDE_EXIT;
    }

  g_variant_iter_init (&iter, files);

  while (g_variant_iter_next (&iter, "(^&ayu)", &path, &flags))
    {
      g_print ("%s: %u\n", path, flags);
    }

  dex_promise_resolve_boolean (promise, TRUE);

  IDE_EXIT;
}

static void
gbp_git_staged_model_update (GbpGitStagedModel *self)
{
  IDE_ENTRY;

  g_assert (GBP_IS_GIT_STAGED_MODEL (self));
  g_assert (IPC_IS_GIT_REPOSITORY (self->repository));

  dex_clear (&self->update);

  self->update = dex_promise_new_cancellable ();

  ipc_git_repository_call_list_status (self->repository,
                                       (GGIT_STATUS_OPTION_INCLUDE_UNTRACKED |
                                        GGIT_STATUS_OPTION_RECURSE_UNTRACKED_DIRS |
                                        GGIT_STATUS_OPTION_EXCLUDE_SUBMODULES |
                                        GGIT_STATUS_OPTION_DISABLE_PATHSPEC_MATCH |
                                        GGIT_STATUS_OPTION_SORT_CASE_INSENSITIVELY),
                                       "",
                                       dex_promise_get_cancellable (self->update),
                                       gbp_git_staged_model_update_cb,
                                       dex_ref (self->update));

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
}
