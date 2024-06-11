/*
 * gbp-git-commit-dialog.c
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

#include "gbp-git-commit-dialog.h"
#include "gbp-git-commit-entry.h"
#include "gbp-git-commit-item.h"
#include "gbp-git-commit-model.h"
#include "gbp-git-vcs.h"

struct _GbpGitCommitDialog
{
  AdwDialog          parent_instance;

  IdeContext        *context;
  GbpGitCommitModel *model;

  GtkListView       *list_view;
  GbpGitCommitEntry *entry;
};

enum {
  PROP_0,
  PROP_CONTEXT,
  PROP_REPOSITORY,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpGitCommitDialog, gbp_git_commit_dialog, ADW_TYPE_DIALOG)

static GParamSpec *properties[N_PROPS];

static void
gbp_git_commit_dialog_bind_cb (GbpGitCommitDialog       *self,
                               GtkListItem              *list_item,
                               GtkSignalListItemFactory *factory)
{
  g_assert (GBP_IS_GIT_COMMIT_DIALOG (self));
  g_assert (GTK_IS_LIST_ITEM (list_item));
  g_assert (GTK_IS_SIGNAL_LIST_ITEM_FACTORY (factory));

  gbp_git_commit_item_bind (gtk_list_item_get_item (list_item), list_item);
}

static void
gbp_git_commit_dialog_unbind_cb (GbpGitCommitDialog       *self,
                                 GtkListItem              *list_item,
                                 GtkSignalListItemFactory *factory)
{
  g_assert (GBP_IS_GIT_COMMIT_DIALOG (self));
  g_assert (GTK_IS_LIST_ITEM (list_item));
  g_assert (GTK_IS_SIGNAL_LIST_ITEM_FACTORY (factory));

  gbp_git_commit_item_unbind (gtk_list_item_get_item (list_item), list_item);
}

static void
cancel_action (GtkWidget  *widget,
               const char *action_name,
               GVariant   *param)
{
  adw_dialog_close (ADW_DIALOG (widget));
}

static IpcGitRepository *
gbp_git_commit_dialog_get_repository (GbpGitCommitDialog *self)
{
  IdeVcs *vcs;

  g_assert (GBP_IS_GIT_COMMIT_DIALOG (self));

  if (self->context == NULL)
    return NULL;

  if ((vcs = ide_vcs_from_context (self->context)) && GBP_IS_GIT_VCS (vcs))
    return gbp_git_vcs_get_repository (GBP_GIT_VCS (vcs));

  return NULL;
}

static gboolean
gbp_git_commit_dialog_grab_focus (GtkWidget *widget)
{
  GbpGitCommitDialog *self = (GbpGitCommitDialog *)widget;

  g_assert (GBP_IS_GIT_COMMIT_DIALOG (self));

  return gtk_widget_grab_focus (GTK_WIDGET (self->entry));
}

static void
gbp_git_commit_dialog_constructed (GObject *object)
{
  GbpGitCommitDialog *self = (GbpGitCommitDialog *)object;
  g_autoptr(GtkNoSelection) no = gtk_no_selection_new (NULL);

  G_OBJECT_CLASS (gbp_git_commit_dialog_parent_class)->constructed (object);

  self->model = gbp_git_commit_model_new (self->context);
  gtk_no_selection_set_model (no, G_LIST_MODEL (self->model));
  gtk_list_view_set_model (self->list_view, GTK_SELECTION_MODEL (no));

  gtk_widget_grab_focus (GTK_WIDGET (self->entry));
}

static void
gbp_git_commit_dialog_dispose (GObject *object)
{
  GbpGitCommitDialog *self = (GbpGitCommitDialog *)object;

  gtk_widget_dispose_template (GTK_WIDGET (self), GBP_TYPE_GIT_COMMIT_DIALOG);

  g_clear_object (&self->context);
  g_clear_object (&self->model);

  G_OBJECT_CLASS (gbp_git_commit_dialog_parent_class)->dispose (object);
}

static void
gbp_git_commit_dialog_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GbpGitCommitDialog *self = GBP_GIT_COMMIT_DIALOG (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
      break;

    case PROP_REPOSITORY:
      g_value_set_object (value, gbp_git_commit_dialog_get_repository (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_git_commit_dialog_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GbpGitCommitDialog *self = GBP_GIT_COMMIT_DIALOG (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      self->context = g_value_dup_object (value);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_REPOSITORY]);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_git_commit_dialog_class_init (GbpGitCommitDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gbp_git_commit_dialog_constructed;
  object_class->dispose = gbp_git_commit_dialog_dispose;
  object_class->get_property = gbp_git_commit_dialog_get_property;
  object_class->set_property = gbp_git_commit_dialog_set_property;

  widget_class->grab_focus = gbp_git_commit_dialog_grab_focus;

  properties[PROP_CONTEXT] =
    g_param_spec_object ("context", NULL, NULL,
                         IDE_TYPE_CONTEXT,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_REPOSITORY] =
    g_param_spec_object ("repository", NULL, NULL,
                         IPC_TYPE_GIT_REPOSITORY,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/git/gbp-git-commit-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, GbpGitCommitDialog, entry);
  gtk_widget_class_bind_template_child (widget_class, GbpGitCommitDialog, list_view);

  gtk_widget_class_bind_template_callback (widget_class, gbp_git_commit_dialog_bind_cb);
  gtk_widget_class_bind_template_callback (widget_class, gbp_git_commit_dialog_unbind_cb);

  gtk_widget_class_install_action (widget_class, "dialog.cancel", NULL, cancel_action);

  g_type_ensure (GBP_TYPE_GIT_COMMIT_ENTRY);
  g_type_ensure (IPC_TYPE_GIT_REPOSITORY);
}

static void
gbp_git_commit_dialog_init (GbpGitCommitDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GbpGitCommitDialog *
gbp_git_commit_dialog_new (IdeContext *context)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  return g_object_new (GBP_TYPE_GIT_COMMIT_DIALOG,
                       "context", context,
                       NULL);
}
