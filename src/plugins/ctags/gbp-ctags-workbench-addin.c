/* gbp-ctags-workbench-addin.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-ctags-workbench-addin"

#include "config.h"

#include <libide-gui.h>

#include "gbp-ctags-workbench-addin.h"
#include "ide-ctags-service.h"

struct _GbpCtagsWorkbenchAddin
{
  GObject       parent_instance;
  IdeWorkbench *workbench;
};

static void
set_paused_state (GbpCtagsWorkbenchAddin *self,
                  GVariant               *param)
{
  IdeCtagsService *service;
  IdeContext *context;

  g_assert (GBP_IS_CTAGS_WORKBENCH_ADDIN (self));
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_BOOLEAN));

  if ((context = ide_workbench_get_context (self->workbench)) &&
      (service = ide_context_peek_child_typed (context, IDE_TYPE_CTAGS_SERVICE)))
    {
      if (g_variant_get_boolean (param))
        ide_ctags_service_pause (service);
      else
        ide_ctags_service_unpause (service);
    }
}

IDE_DEFINE_ACTION_GROUP (GbpCtagsWorkbenchAddin, gbp_ctags_workbench_addin, {
  { "paused", NULL, NULL, "false", set_paused_state },
})

static void
on_notify_paused_cb (GbpCtagsWorkbenchAddin *self,
                     GParamSpec             *pspec,
                     IdeCtagsService        *service)
{
  gboolean paused;

  g_assert (GBP_IS_CTAGS_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_CTAGS_SERVICE (service));

  g_object_get (service,
                "paused", &paused,
                NULL);
  gbp_ctags_workbench_addin_set_action_state (self,
                                              "paused",
                                              g_variant_new_boolean (paused));
}

static void
gbp_ctags_workbench_addin_project_loaded (IdeWorkbenchAddin *addin,
                                          IdeProjectInfo    *project_info)
{
  GbpCtagsWorkbenchAddin *self = (GbpCtagsWorkbenchAddin *)addin;
  g_autoptr(IdeCtagsService) service = NULL;
  IdeContext *context;

  g_assert (GBP_IS_CTAGS_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_PROJECT_INFO (project_info));

  /* We don't load the ctags service until a project is loaded so that
   * we have a stable workdir to use.
   */
  context = ide_workbench_get_context (self->workbench);
  service = ide_object_ensure_child_typed (IDE_OBJECT (context), IDE_TYPE_CTAGS_SERVICE);

  g_signal_connect_object (G_OBJECT (service),
                           "notify::paused",
                           G_CALLBACK (on_notify_paused_cb),
                           self,
                           G_CONNECT_SWAPPED);
  on_notify_paused_cb (self, NULL, service);
}

static void
gbp_ctags_workbench_addin_load (IdeWorkbenchAddin *addin,
                                IdeWorkbench      *workbench)
{
  GbpCtagsWorkbenchAddin *self = (GbpCtagsWorkbenchAddin *)addin;

  g_assert (GBP_IS_CTAGS_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = workbench;
}

static void
gbp_ctags_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                  IdeWorkbench      *workbench)
{
  GbpCtagsWorkbenchAddin *self = (GbpCtagsWorkbenchAddin *)addin;

  g_assert (GBP_IS_CTAGS_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = NULL;
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gbp_ctags_workbench_addin_load;
  iface->unload = gbp_ctags_workbench_addin_unload;
  iface->project_loaded = gbp_ctags_workbench_addin_project_loaded;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpCtagsWorkbenchAddin, gbp_ctags_workbench_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, gbp_ctags_workbench_addin_init_action_group)
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, workbench_addin_iface_init))

static void
gbp_ctags_workbench_addin_class_init (GbpCtagsWorkbenchAddinClass *klass)
{
}

static void
gbp_ctags_workbench_addin_init (GbpCtagsWorkbenchAddin *self)
{
}
