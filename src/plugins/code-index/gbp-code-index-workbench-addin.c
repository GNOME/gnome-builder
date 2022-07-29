/* gbp-code-index-workbench-addin.c
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

#define G_LOG_DOMAIN "gbp-code-index-workbench-addin"

#include "config.h"

#include <libide-foundry.h>
#include <libide-gui.h>
#include <libide-plugins.h>

#include "gbp-code-index-service.h"
#include "gbp-code-index-workbench-addin.h"

struct _GbpCodeIndexWorkbenchAddin
{
  GObject       parent_instance;
  IdeWorkbench *workbench;
};

static void gbp_code_index_workbench_addin_notify_paused (GbpCodeIndexWorkbenchAddin *self,
                                                          GParamSpec                 *pspec,
                                                          GbpCodeIndexService        *service);

static void
gbp_code_index_workbench_addin_load (IdeWorkbenchAddin *addin,
                                     IdeWorkbench      *workbench)
{
  GbpCodeIndexWorkbenchAddin *self = (GbpCodeIndexWorkbenchAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = workbench;
}

static void
gbp_code_index_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                       IdeWorkbench      *workbench)
{
  GbpCodeIndexWorkbenchAddin *self = (GbpCodeIndexWorkbenchAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  if (ide_workbench_has_project (workbench))
    {
      GbpCodeIndexService *service;
      IdeContext *context;

      context = ide_workbench_get_context (workbench);
      service = gbp_code_index_service_from_context (context);
      ide_object_destroy (IDE_OBJECT (service));
    }

  self->workbench = NULL;
}

static void
gbp_code_index_workbench_addin_project_loaded (IdeWorkbenchAddin *addin,
                                               IdeProjectInfo    *project_info)
{
  GbpCodeIndexWorkbenchAddin *self = (GbpCodeIndexWorkbenchAddin *)addin;
  GbpCodeIndexService *service;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_PROJECT_INFO (project_info));

  context = ide_workbench_get_context (self->workbench);
  service = gbp_code_index_service_from_context (context);
  g_signal_connect_object (service,
                           "notify::paused",
                           G_CALLBACK (gbp_code_index_workbench_addin_notify_paused),
                           self,
                           G_CONNECT_SWAPPED);
  gbp_code_index_workbench_addin_notify_paused (self, NULL, service);
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gbp_code_index_workbench_addin_load;
  iface->unload = gbp_code_index_workbench_addin_unload;
  iface->project_loaded = gbp_code_index_workbench_addin_project_loaded;
}

static void
gbp_code_index_workbench_addin_paused (GbpCodeIndexWorkbenchAddin *self,
                                       GVariant                   *state)
{
  GbpCodeIndexService *service;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_WORKBENCH_ADDIN (self));

  if (state == NULL || !g_variant_is_of_type (state, G_VARIANT_TYPE_BOOLEAN))
    return;

  context = ide_workbench_get_context (self->workbench);
  service = gbp_code_index_service_from_context (context);
  gbp_code_index_service_set_paused (service, g_variant_get_boolean (state));
}

IDE_DEFINE_ACTION_GROUP (GbpCodeIndexWorkbenchAddin, gbp_code_index_workbench_addin, {
  { "paused", NULL, NULL, "false", gbp_code_index_workbench_addin_paused },
})

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpCodeIndexWorkbenchAddin, gbp_code_index_workbench_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, gbp_code_index_workbench_addin_init_action_group)
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, workbench_addin_iface_init))

static void
gbp_code_index_workbench_addin_class_init (GbpCodeIndexWorkbenchAddinClass *klass)
{
}

static void
gbp_code_index_workbench_addin_init (GbpCodeIndexWorkbenchAddin *self)
{
}

static void
gbp_code_index_workbench_addin_notify_paused (GbpCodeIndexWorkbenchAddin *self,
                                              GParamSpec                 *pspec,
                                              GbpCodeIndexService        *service)
{
  gboolean paused;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_WORKBENCH_ADDIN (self));
  g_assert (GBP_IS_CODE_INDEX_SERVICE (service));

  paused = gbp_code_index_service_get_paused (service);
  gbp_code_index_workbench_addin_set_action_state (self,
                                                   "paused",
                                                   g_variant_new_boolean (paused));
}

