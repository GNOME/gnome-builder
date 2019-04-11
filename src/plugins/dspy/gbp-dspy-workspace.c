/* gbp-dspy-workspace.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-dspy-workspace"

#include "config.h"

#include "gbp-dspy-surface.h"
#include "gbp-dspy-workspace.h"

struct _GbpDspyWorkspace
{
  IdeWorkspace    parent_instance;
  IdeHeaderBar   *header_bar;
  GbpDspySurface *surface;
};

G_DEFINE_TYPE (GbpDspyWorkspace, gbp_dspy_workspace, IDE_TYPE_WORKSPACE)

static void
gbp_dspy_workspace_class_init (GbpDspyWorkspaceClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  IdeWorkspaceClass *workspace_class = IDE_WORKSPACE_CLASS (klass);

  ide_workspace_class_set_kind (workspace_class, "dspy");

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/dspy/gbp-dspy-workspace.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpDspyWorkspace, header_bar);
  gtk_widget_class_bind_template_child (widget_class, GbpDspyWorkspace, surface);
}

static void
gbp_dspy_workspace_init (GbpDspyWorkspace *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GbpDspyWorkspace *
gbp_dspy_workspace_new (IdeApplication *application)
{
  g_return_val_if_fail (IDE_IS_APPLICATION (application), NULL);

  return g_object_new (GBP_TYPE_DSPY_WORKSPACE,
                       "application", application,
                       NULL);
}
