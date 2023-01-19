/*
 * gbp-codesearch-workbench-addin.c
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-codesearch-workbench-addin"

#include "config.h"

#include <libide-gui.h>

#include "gbp-codesearch-workbench-addin.h"

struct _GbpCodesearchWorkbenchAddin
{
  GObject       parent_instance;
  GSignalGroup *signals;
  GSignalGroup *monitor_signals;
};

static void
gbp_codesearch_workbench_addin_notify_vcs_cb (GbpCodesearchWorkbenchAddin *self,
                                              GParamSpec                  *pspec,
                                              IdeWorkbench                *workbench)
{
  IdeVcsMonitor *vcs_monitor;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODESEARCH_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  vcs_monitor = ide_workbench_get_vcs_monitor (workbench);
  g_signal_group_set_target (self->monitor_signals, vcs_monitor);

  IDE_EXIT;
}

static void
gbp_codesearch_workbench_addin_changed_cb (GbpCodesearchWorkbenchAddin *self,
                                           GFile                       *file,
                                           GFile                       *other_file,
                                           GFileMonitorEvent            event,
                                           IdeVcsMonitor               *vcs_monitor)
{
  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODESEARCH_WORKBENCH_ADDIN (self));
  g_assert (G_IS_FILE (file));
  g_assert (!other_file || G_IS_FILE (other_file));
  g_assert (IDE_IS_VCS_MONITOR (vcs_monitor));


  IDE_EXIT;
}

static void
gbp_codesearch_workbench_addin_load (IdeWorkbenchAddin *addin,
                                     IdeWorkbench      *workbench)
{
  GbpCodesearchWorkbenchAddin *self = (GbpCodesearchWorkbenchAddin *)addin;
  IdeVcsMonitor *vcs_monitor;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODESEARCH_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  vcs_monitor = ide_workbench_get_vcs_monitor (workbench);

  self->signals = g_signal_group_new (IDE_TYPE_WORKBENCH);
  g_signal_group_connect_object (self->signals,
                                 "notify::vcs",
                                 G_CALLBACK (gbp_codesearch_workbench_addin_notify_vcs_cb),
                                 self,
                                 G_CONNECT_SWAPPED);

  self->monitor_signals = g_signal_group_new (IDE_TYPE_VCS_MONITOR);
  g_signal_group_connect_object (self->monitor_signals,
                                 "changed",
                                 G_CALLBACK (gbp_codesearch_workbench_addin_changed_cb),
                                 self,
                                 G_CONNECT_SWAPPED);

  g_signal_group_set_target (self->signals, workbench);
  g_signal_group_set_target (self->monitor_signals, vcs_monitor);

  IDE_EXIT;
}

static void
gbp_codesearch_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                       IdeWorkbench      *workbench)
{
  GbpCodesearchWorkbenchAddin *self = (GbpCodesearchWorkbenchAddin *)addin;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODESEARCH_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  g_signal_group_set_target (self->signals, NULL);
  g_signal_group_set_target (self->monitor_signals, NULL);

  g_clear_object (&self->signals);
  g_clear_object (&self->monitor_signals);

  IDE_EXIT;
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gbp_codesearch_workbench_addin_load;
  iface->unload = gbp_codesearch_workbench_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpCodesearchWorkbenchAddin, gbp_codesearch_workbench_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, workbench_addin_iface_init))

static void
gbp_codesearch_workbench_addin_class_init (GbpCodesearchWorkbenchAddinClass *klass)
{
}

static void
gbp_codesearch_workbench_addin_init (GbpCodesearchWorkbenchAddin *self)
{

}
