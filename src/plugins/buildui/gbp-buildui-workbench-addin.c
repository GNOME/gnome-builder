/* gbp-buildui-workbench-addin.c
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

#define G_LOG_DOMAIN "gbp-buildui-workbench-addin"

#include "config.h"

#include <libide-foundry.h>
#include <libide-gui.h>

#include "gbp-buildui-workbench-addin.h"

struct _GbpBuilduiWorkbenchAddin
{
  GObject     parent_instance;
  IdeContext *context;
};

static void
gbp_buildui_workbench_addin_save_session (IdeWorkbenchAddin *addin,
                                          IdeSession        *session)
{
  GbpBuilduiWorkbenchAddin *self = (GbpBuilduiWorkbenchAddin *)addin;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDUI_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_SESSION (session));

  if (ide_context_has_project (self->context))
    {
      g_autoptr(IdeSessionItem) item = NULL;
      IdeDeviceManager *device_manager = NULL;
      const char *device_id = NULL;
      IdeDevice *device = NULL;

      device_manager = ide_device_manager_from_context (self->context);
      device = ide_device_manager_get_device (device_manager);
      device_id = ide_device_get_id (device);
      item = ide_session_item_new ();

      ide_session_item_set_id (item, "ide.context.foundry.device-manager.device");
      ide_session_item_set_module_name (item, "buildui");
      ide_session_item_set_metadata (item, "id", "s", device_id);

      ide_session_append (session, item);
    }

  IDE_EXIT;
}

static void
gbp_buildui_workbench_addin_restore_session (IdeWorkbenchAddin *addin,
                                             IdeSession        *session)
{
  GbpBuilduiWorkbenchAddin *self = (GbpBuilduiWorkbenchAddin *)addin;
  g_autoptr(IdeSessionItem) item = NULL;
  g_autofree char *device_id = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDUI_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_SESSION (session));

  if ((item = ide_session_lookup_by_id (session, "ide.context.foundry.device-manager.device")) &&
      ide_session_item_get_metadata (item, "id", "s", &device_id))
    {
      IdeDeviceManager *device_manager = ide_device_manager_from_context (self->context);
      IdeDevice *device = ide_device_manager_get_device_by_id (device_manager, device_id);

      if (device != NULL)
        ide_device_manager_set_device (device_manager, device);
    }

  IDE_EXIT;
}

static void
gbp_buildui_workbench_addin_load (IdeWorkbenchAddin *addin,
                                  IdeWorkbench      *workbench)
{
  GbpBuilduiWorkbenchAddin *self = (GbpBuilduiWorkbenchAddin *)addin;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDUI_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->context = g_object_ref (ide_workbench_get_context (workbench));

  IDE_EXIT;
}

static void
gbp_buildui_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                    IdeWorkbench      *workbench)
{
  GbpBuilduiWorkbenchAddin *self = (GbpBuilduiWorkbenchAddin *)addin;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDUI_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  g_clear_object (&self->context);

  IDE_EXIT;
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gbp_buildui_workbench_addin_load;
  iface->unload = gbp_buildui_workbench_addin_unload;
  iface->save_session = gbp_buildui_workbench_addin_save_session;
  iface->restore_session = gbp_buildui_workbench_addin_restore_session;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpBuilduiWorkbenchAddin, gbp_buildui_workbench_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, workbench_addin_iface_init))

static void
gbp_buildui_workbench_addin_class_init (GbpBuilduiWorkbenchAddinClass *klass)
{
}

static void
gbp_buildui_workbench_addin_init (GbpBuilduiWorkbenchAddin *self)
{
}
