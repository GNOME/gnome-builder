/*
 * gbp-git-file-row.c
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

#include <libide-gui.h>

#include "ipc-git-repository.h"

#include "gbp-git-dex.h"
#include "gbp-git-file-row.h"
#include "gbp-git-vcs.h"

struct _GbpGitFileRow
{
  GtkWidget         parent_instance;
  GbpGitFileItem *item;
};

enum {
  PROP_0,
  PROP_ITEM,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpGitFileRow, gbp_git_file_row, GTK_TYPE_WIDGET)

static GParamSpec *properties[N_PROPS];

static void
gbp_git_file_row_stage_all (GtkWidget  *widget,
                            const char *action_name,
                            GVariant   *param)
{
  GbpGitFileRow *self = (GbpGitFileRow *)widget;
  IpcGitRepository *repository;
  g_autofree char *relative = NULL;
  g_autoptr(GFile) workdir = NULL;
  IdeContext *context;
  IdeVcs *vcs;
  GFile *file;

  IDE_ENTRY;

  g_assert (GBP_IS_GIT_FILE_ROW (self));

  if (self->item == NULL)
    IDE_EXIT;

  if (!(context = ide_widget_get_context (widget)))
    IDE_EXIT;

  if (!(vcs = ide_vcs_from_context (context)))
    IDE_EXIT;

  if (!GBP_IS_GIT_VCS (vcs))
    IDE_EXIT;

  if (!(repository = gbp_git_vcs_get_repository (GBP_GIT_VCS (vcs))))
    IDE_EXIT;

  workdir = ide_context_ref_workdir (context);
  file = gbp_git_file_item_get_file (self->item);
  relative = g_file_get_relative_path (workdir, file);

  if (relative == NULL)
    IDE_EXIT;

  dex_future_disown (ipc_git_repository_stage_file (repository, relative));

  IDE_EXIT;
}

static void
gbp_git_file_row_dispose (GObject *object)
{
  GbpGitFileRow *self = (GbpGitFileRow *)object;
  GtkWidget *child;

  gtk_widget_dispose_template (GTK_WIDGET (self), GBP_TYPE_GIT_FILE_ROW);

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self))))
    gtk_widget_unparent (child);

  g_clear_object (&self->item);

  G_OBJECT_CLASS (gbp_git_file_row_parent_class)->dispose (object);
}

static void
gbp_git_file_row_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GbpGitFileRow *self = GBP_GIT_FILE_ROW (object);

  switch (prop_id)
    {
    case PROP_ITEM:
      g_value_set_object (value, gbp_git_file_row_get_item (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_git_file_row_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GbpGitFileRow *self = GBP_GIT_FILE_ROW (object);

  switch (prop_id)
    {
    case PROP_ITEM:
      gbp_git_file_row_set_item (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_git_file_row_class_init (GbpGitFileRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gbp_git_file_row_dispose;
  object_class->get_property = gbp_git_file_row_get_property;
  object_class->set_property = gbp_git_file_row_set_property;

  properties[PROP_ITEM] =
    g_param_spec_object ("item", NULL, NULL,
                         GBP_TYPE_GIT_FILE_ITEM,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/git/gbp-git-file-row.ui");
  gtk_widget_class_set_css_name (widget_class, "GbpGitFileRow");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_install_action (widget_class, "file.stage", NULL, gbp_git_file_row_stage_all);
}

static void
gbp_git_file_row_init (GbpGitFileRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
gbp_git_file_row_new (void)
{
  return g_object_new (GBP_TYPE_GIT_FILE_ROW, NULL);
}

GbpGitFileItem *
gbp_git_file_row_get_item (GbpGitFileRow *self)
{
  g_return_val_if_fail (GBP_IS_GIT_FILE_ROW (self), NULL);

  return self->item;
}

void
gbp_git_file_row_set_item (GbpGitFileRow  *self,
                           GbpGitFileItem *item)
{
  g_return_if_fail (GBP_IS_GIT_FILE_ROW (self));
  g_return_if_fail (GBP_IS_GIT_FILE_ITEM (item));

  if (g_set_object (&self->item, item))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ITEM]);
}

