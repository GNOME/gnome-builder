/* gbp-sessionui-workbench-addin.c
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

#define G_LOG_DOMAIN "gbp-sessionui-workbench-addin"

#include "config.h"

#include <libide-gui.h>

#include "gbp-sessionui-workbench-addin.h"

struct _GbpSessionuiWorkbenchAddin
{
  GObject       parent_instance;
  IdeWorkbench *workbench;
};

static void
gbp_sessionui_workbench_addin_load (IdeWorkbenchAddin *addin,
                                    IdeWorkbench      *workbench)
{
  GbpSessionuiWorkbenchAddin *self = (GbpSessionuiWorkbenchAddin *)addin;

  g_assert (GBP_IS_SESSIONUI_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = workbench;
}

static void
gbp_sessionui_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                      IdeWorkbench      *workbench)
{
  GbpSessionuiWorkbenchAddin *self = (GbpSessionuiWorkbenchAddin *)addin;

  g_assert (GBP_IS_SESSIONUI_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = NULL;
}

static void
gbp_sessionui_workbench_addin_save_session (IdeWorkbenchAddin *addin,
                                            IdeSession        *session)
{
  GbpSessionuiWorkbenchAddin *self = (GbpSessionuiWorkbenchAddin *)addin;

  g_assert (GBP_IS_SESSIONUI_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_SESSION (session));
}

static void
gbp_sessionui_workbench_addin_restore_session (IdeWorkbenchAddin *addin,
                                               IdeSession        *session)
{
  GbpSessionuiWorkbenchAddin *self = (GbpSessionuiWorkbenchAddin *)addin;

  g_assert (GBP_IS_SESSIONUI_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_SESSION (session));
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gbp_sessionui_workbench_addin_load;
  iface->unload = gbp_sessionui_workbench_addin_unload;
  iface->save_session = gbp_sessionui_workbench_addin_save_session;
  iface->restore_session = gbp_sessionui_workbench_addin_restore_session;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpSessionuiWorkbenchAddin, gbp_sessionui_workbench_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, workbench_addin_iface_init))

static void
gbp_sessionui_workbench_addin_class_init (GbpSessionuiWorkbenchAddinClass *klass)
{
}

static void
gbp_sessionui_workbench_addin_init (GbpSessionuiWorkbenchAddin *self)
{
}
