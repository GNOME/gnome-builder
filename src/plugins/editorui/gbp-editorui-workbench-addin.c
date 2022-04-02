/* gbp-editorui-workbench-addin.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-editorui-workbench-addin"

#include "config.h"

#include <libide-gui.h>

#include "gbp-editorui-workbench-addin.h"

struct _GbpEditoruiWorkbenchAddin
{
  GObject       parent_instance;
  IdeWorkbench *workbench;
};

static void
gbp_editorui_workbench_addin_load (IdeWorkbenchAddin *addin,
                                   IdeWorkbench      *workbench)
{
  GbpEditoruiWorkbenchAddin *self = (GbpEditoruiWorkbenchAddin *)addin;

  g_assert (GBP_IS_EDITORUI_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = workbench;
}

static void
gbp_editorui_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                     IdeWorkbench      *workbench)
{
  GbpEditoruiWorkbenchAddin *self = (GbpEditoruiWorkbenchAddin *)addin;

  g_assert (GBP_IS_EDITORUI_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = NULL;
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gbp_editorui_workbench_addin_load;
  iface->unload = gbp_editorui_workbench_addin_unload;
}

G_DEFINE_TYPE_WITH_CODE (GbpEditoruiWorkbenchAddin, gbp_editorui_workbench_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, workbench_addin_iface_init))

static void
gbp_editorui_workbench_addin_class_init (GbpEditoruiWorkbenchAddinClass *klass)
{
}

static void
gbp_editorui_workbench_addin_init (GbpEditoruiWorkbenchAddin *self)
{
}
