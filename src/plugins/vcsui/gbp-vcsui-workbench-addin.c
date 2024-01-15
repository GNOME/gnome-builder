/* gbp-vcsui-workbench-addin.c
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

#define G_LOG_DOMAIN "gbp-vcsui-workbench-addin"

#include "config.h"

#include <libide-gui.h>
#include <libide-vcs.h>

#include "gbp-vcsui-workbench-addin.h"

struct _GbpVcsuiWorkbenchAddin
{
  GObject         parent_instance;
  GSignalGroup   *vcs_signals;
};

static void
on_notify_branch_name (GbpVcsuiWorkbenchAddin *self,
                       GParamSpec             *pspec,
                       IdeVcs                 *vcs)
{
  g_autoptr(IdeContext) context = NULL;
  IdeBuildManager *build_manager;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_VCSUI_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_VCS (vcs));

  context = ide_object_ref_context (IDE_OBJECT (vcs));

  /* Short-circuit if no project is loaded */
  if (!context || !ide_context_has_project (context))
    return;

  if ((build_manager = ide_build_manager_from_context (context)))
    ide_build_manager_invalidate (build_manager);

  IDE_EXIT;
}

static void
gbp_vcsui_workbench_addin_vcs_changed (IdeWorkbenchAddin *addin,
                                       IdeVcs            *vcs)
{
  GbpVcsuiWorkbenchAddin *self = (GbpVcsuiWorkbenchAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_VCSUI_WORKBENCH_ADDIN (self));
  g_assert (!vcs || IDE_IS_VCS (vcs));

  g_signal_group_set_target (self->vcs_signals, vcs);
}

static void
gbp_vcsui_workbench_addin_load (IdeWorkbenchAddin *addin,
                                IdeWorkbench      *workbench)
{
  GbpVcsuiWorkbenchAddin *self = (GbpVcsuiWorkbenchAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_VCSUI_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->vcs_signals = g_signal_group_new (G_TYPE_OBJECT);
  g_signal_group_connect_object (self->vcs_signals,
                                   "notify::branch-name",
                                   G_CALLBACK (on_notify_branch_name),
                                   self,
                                   G_CONNECT_SWAPPED);
}

static void
gbp_vcsui_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                  IdeWorkbench      *workbench)
{
  GbpVcsuiWorkbenchAddin *self = (GbpVcsuiWorkbenchAddin *)addin;

  g_assert (GBP_IS_VCSUI_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  g_signal_group_set_target (self->vcs_signals, NULL);
  g_clear_object (&self->vcs_signals);
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gbp_vcsui_workbench_addin_load;
  iface->unload = gbp_vcsui_workbench_addin_unload;
  iface->vcs_changed = gbp_vcsui_workbench_addin_vcs_changed;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpVcsuiWorkbenchAddin, gbp_vcsui_workbench_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, workbench_addin_iface_init))

static void
gbp_vcsui_workbench_addin_class_init (GbpVcsuiWorkbenchAddinClass *klass)
{
}

static void
gbp_vcsui_workbench_addin_init (GbpVcsuiWorkbenchAddin *self)
{
}
