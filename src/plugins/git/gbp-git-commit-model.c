/*
 * gbp-git-commit-model.c
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

#define G_LOG_DOMAIN "gbp-git-commit-model"

#include "config.h"

#include <gtk/gtk.h>

#include "gbp-git-commit-model.h"
#include "gbp-git-staged-model.h"

struct _GbpGitCommitModel
{
  GObject              parent_instance;

  IdeContext          *context;
  GbpGitStagedModel   *staged;
  GListStore          *toplevel_models;
  GtkFlattenListModel *flatten;
};

static void list_model_iface_init    (GListModelInterface      *iface);
static void section_model_iface_init (GtkSectionModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpGitCommitModel, gbp_git_commit_model, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init)
                               G_IMPLEMENT_INTERFACE (GTK_TYPE_SECTION_MODEL, section_model_iface_init))

enum {
  PROP_0,
  PROP_CONTEXT,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

GbpGitCommitModel *
gbp_git_commit_model_new (IdeContext *context)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (ide_context_has_project (context), NULL);

  return g_object_new (GBP_TYPE_GIT_COMMIT_MODEL,
                       "context", context,
                       NULL);
}

static void
gbp_git_commit_model_constructed (GObject *object)
{
  GbpGitCommitModel *self = (GbpGitCommitModel *)object;

  G_OBJECT_CLASS (gbp_git_commit_model_parent_class)->constructed (object);

  self->staged = gbp_git_staged_model_new (self->context);
  g_list_store_append (self->toplevel_models, self->staged);
}

static void
gbp_git_commit_model_dispose (GObject *object)
{
  GbpGitCommitModel *self = (GbpGitCommitModel *)object;

  g_clear_object (&self->context);
  g_clear_object (&self->flatten);
  g_clear_object (&self->toplevel_models);

  G_OBJECT_CLASS (gbp_git_commit_model_parent_class)->dispose (object);
}

static void
gbp_git_commit_model_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GbpGitCommitModel *self = GBP_GIT_COMMIT_MODEL (object);

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
gbp_git_commit_model_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GbpGitCommitModel *self = GBP_GIT_COMMIT_MODEL (object);

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
gbp_git_commit_model_class_init (GbpGitCommitModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gbp_git_commit_model_constructed;
  object_class->dispose = gbp_git_commit_model_dispose;
  object_class->get_property = gbp_git_commit_model_get_property;
  object_class->set_property = gbp_git_commit_model_set_property;

  properties[PROP_CONTEXT] =
    g_param_spec_object ("context", NULL, NULL,
                         IDE_TYPE_CONTEXT,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_git_commit_model_init (GbpGitCommitModel *self)
{
  self->toplevel_models = g_list_store_new (G_TYPE_LIST_MODEL);
  self->flatten = g_object_new (GTK_TYPE_FLATTEN_LIST_MODEL,
                                "model", self->toplevel_models,
                                NULL);

  g_signal_connect_object (self->flatten,
                           "items-changed",
                           G_CALLBACK (g_list_model_items_changed),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->flatten,
                           "sections-changed",
                           G_CALLBACK (gtk_section_model_sections_changed),
                           self,
                           G_CONNECT_SWAPPED);
}

static GType
gbp_git_commit_model_get_item_type (GListModel *model)
{
  return G_TYPE_OBJECT;
}

static gpointer
gbp_git_commit_model_get_item (GListModel *model,
                               guint       position)
{
  GbpGitCommitModel *self = GBP_GIT_COMMIT_MODEL (model);

  return g_list_model_get_item (G_LIST_MODEL (self->flatten), position);
}

static guint
gbp_git_commit_model_get_n_items (GListModel *model)
{
  GbpGitCommitModel *self = GBP_GIT_COMMIT_MODEL (model);

  return g_list_model_get_n_items (G_LIST_MODEL (self->flatten));
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = gbp_git_commit_model_get_item_type;
  iface->get_item = gbp_git_commit_model_get_item;
  iface->get_n_items = gbp_git_commit_model_get_n_items;
}

static void
gbp_git_commit_model_get_section (GtkSectionModel *model,
                                  guint            position,
                                  guint           *out_start,
                                  guint           *out_end)
{
  GbpGitCommitModel *self = GBP_GIT_COMMIT_MODEL (model);

  return gtk_section_model_get_section (GTK_SECTION_MODEL (self->flatten), position, out_start, out_end);
}

static void
section_model_iface_init (GtkSectionModelInterface *iface)
{
  iface->get_section = gbp_git_commit_model_get_section;
}
