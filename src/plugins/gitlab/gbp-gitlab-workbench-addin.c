/* gbp-gitlab-workbench-addin.c
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

#define G_LOG_DOMAIN "gbp-gitlab-workbench-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-gui.h>
#include <libide-threading.h>

#include "gbp-gitlab-workbench-addin.h"

struct _GbpGitlabWorkbenchAddin
{
  GObject       parent_instance;

  IdeWorkbench *workbench;

  guint         project_loaded : 1;
};

IDE_DEFINE_ACTION_GROUP (GbpGitlabWorkbenchAddin, gbp_gitlab_workbench_addin, {
})

static void
gbp_gitlab_workbench_addin_load (IdeWorkbenchAddin *addin,
                                 IdeWorkbench      *workbench)
{
  GbpGitlabWorkbenchAddin *self = (GbpGitlabWorkbenchAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GITLAB_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = workbench;
}

static void
gbp_gitlab_workbench_addin_project_loaded (IdeWorkbenchAddin *addin,
                                           IdeProjectInfo    *project_info)
{
  GbpGitlabWorkbenchAddin *self = (GbpGitlabWorkbenchAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GITLAB_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (self->workbench));

  self->project_loaded = TRUE;
}

static void
gbp_gitlab_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                   IdeWorkbench      *workbench)
{
  GbpGitlabWorkbenchAddin *self = (GbpGitlabWorkbenchAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GITLAB_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = NULL;
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gbp_gitlab_workbench_addin_load;
  iface->unload = gbp_gitlab_workbench_addin_unload;
  iface->project_loaded = gbp_gitlab_workbench_addin_project_loaded;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpGitlabWorkbenchAddin, gbp_gitlab_workbench_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, gbp_gitlab_workbench_addin_init_action_group)
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, workbench_addin_iface_init))

static void
gbp_gitlab_workbench_addin_class_init (GbpGitlabWorkbenchAddinClass *klass)
{
}

static void
gbp_gitlab_workbench_addin_init (GbpGitlabWorkbenchAddin *self)
{
}
