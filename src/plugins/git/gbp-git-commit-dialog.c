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

struct _GbpGitCommitDialog
{
  AdwDialog   parent_instance;
  IdeContext *context;
};

enum {
  PROP_0,
  PROP_CONTEXT,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpGitCommitDialog, gbp_git_commit_dialog, ADW_TYPE_DIALOG)

static GParamSpec *properties[N_PROPS];

static void
gbp_git_commit_dialog_dispose (GObject *object)
{
  GbpGitCommitDialog *self = (GbpGitCommitDialog *)object;

  gtk_widget_dispose_template (GTK_WIDGET (self), GBP_TYPE_GIT_COMMIT_DIALOG);

  g_clear_object (&self->context);

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

  object_class->dispose = gbp_git_commit_dialog_dispose;
  object_class->get_property = gbp_git_commit_dialog_get_property;
  object_class->set_property = gbp_git_commit_dialog_set_property;

  properties[PROP_CONTEXT] =
    g_param_spec_object ("context", NULL, NULL,
                         IDE_TYPE_CONTEXT,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/git/gbp-git-commit-dialog.ui");

  g_type_ensure (GBP_TYPE_GIT_COMMIT_ENTRY);
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
