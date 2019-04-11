/* gbp-dspy-workspace-addin.c
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

#define G_LOG_DOMAIN "gbp-dspy-workspace-addin"

#include <libide-editor.h>

#include "gbp-dspy-surface.h"
#include "gbp-dspy-workspace-addin.h"

struct _GbpDspyWorkspaceAddin
{
  GObject         parent_instance;
  GbpDspySurface *surface;
};

static void
gbp_dspy_workspace_addin_load (IdeWorkspaceAddin *addin,
                               IdeWorkspace      *workspace)
{
  GbpDspyWorkspaceAddin *self = (GbpDspyWorkspaceAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_DSPY_WORKSPACE_ADDIN (addin));
  g_assert (IDE_IS_PRIMARY_WORKSPACE (workspace) || IDE_IS_EDITOR_WORKSPACE (workspace));

  self->surface = gbp_dspy_surface_new ();
  g_signal_connect (self->surface,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->surface);
  ide_workspace_add_surface (workspace, IDE_SURFACE (self->surface));
  gtk_widget_show (GTK_WIDGET (self->surface));
}

static void
gbp_dspy_workspace_addin_unload (IdeWorkspaceAddin *addin,
                                 IdeWorkspace      *workspace)
{
  GbpDspyWorkspaceAddin *self = (GbpDspyWorkspaceAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_DSPY_WORKSPACE_ADDIN (addin));
  g_assert (IDE_IS_PRIMARY_WORKSPACE (workspace) || IDE_IS_EDITOR_WORKSPACE (workspace));

  if (self->surface != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->surface));

}

static void
workspace_addin_iface_init (IdeWorkspaceAddinInterface *iface)
{
  iface->load = gbp_dspy_workspace_addin_load;
  iface->unload = gbp_dspy_workspace_addin_unload;
}

G_DEFINE_TYPE_WITH_CODE (GbpDspyWorkspaceAddin, gbp_dspy_workspace_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKSPACE_ADDIN, workspace_addin_iface_init))

static void
gbp_dspy_workspace_addin_class_init (GbpDspyWorkspaceAddinClass *klass)
{
}

static void
gbp_dspy_workspace_addin_init (GbpDspyWorkspaceAddin *self)
{
}
