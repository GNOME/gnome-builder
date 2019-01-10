/* gbp-deviceui-workspace-addin.c
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

#define G_LOG_DOMAIN "gbp-deviceui-workspace-addin"

#include "config.h"

#include <libide-foundry.h>
#include <libide-gui.h>

#include "ide-device-private.h"

#include "gbp-deviceui-workspace-addin.h"

struct _GbpDeviceuiWorkspaceAddin
{
  GObject    parent_instance;
  GtkWidget *button;
};

static gboolean
device_to_icon_name (GBinding     *binding,
                     const GValue *from_value,
                     GValue       *to_value,
                     gpointer      user_data)
{
  IdeDevice *device;
  const gchar *icon_name;

  if (G_VALUE_HOLDS (from_value, IDE_TYPE_DEVICE) &&
      (device = g_value_get_object (from_value)) &&
      (icon_name = ide_device_get_icon_name (device)))
    g_value_set_string (to_value, icon_name);
  else
    g_value_set_static_string (to_value, "computer-symbolic");

  return TRUE;
}

static void
gbp_deviceui_workspace_addin_load (IdeWorkspaceAddin *addin,
                                   IdeWorkspace      *workspace)
{
  GbpDeviceuiWorkspaceAddin *self = (GbpDeviceuiWorkspaceAddin *)addin;
  IdeDeviceManager *device_manager;
  IdeHeaderBar *header;
  IdeContext *context;
  GMenu *menu;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_PRIMARY_WORKSPACE (workspace));

  header = ide_workspace_get_header_bar (workspace);
  context = ide_widget_get_context (GTK_WIDGET (workspace));
  device_manager = ide_device_manager_from_context (context);
  menu = _ide_device_manager_get_menu (device_manager);

  self->button = g_object_new (DZL_TYPE_MENU_BUTTON,
                               "focus-on-click", FALSE,
                               "model", menu,
                               "show-arrow", TRUE,
                               "show-icons", TRUE,
                               "visible", TRUE,
                               NULL);
  g_signal_connect (self->button,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->button);
  ide_header_bar_add_center_left (header, self->button);

  g_object_bind_property_full (device_manager, "device",
                               self->button, "icon-name",
                               G_BINDING_SYNC_CREATE,
                               device_to_icon_name,
                               NULL, NULL, NULL);
}

static void
gbp_deviceui_workspace_addin_unload (IdeWorkspaceAddin *addin,
                                     IdeWorkspace      *workspace)
{
  GbpDeviceuiWorkspaceAddin *self = (GbpDeviceuiWorkspaceAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_PRIMARY_WORKSPACE (workspace));

  if (self->button)
    gtk_widget_destroy (self->button);
}

static void
workspace_addin_iface_init (IdeWorkspaceAddinInterface *iface)
{
  iface->load = gbp_deviceui_workspace_addin_load;
  iface->unload = gbp_deviceui_workspace_addin_unload;
}

G_DEFINE_TYPE_WITH_CODE (GbpDeviceuiWorkspaceAddin, gbp_deviceui_workspace_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKSPACE_ADDIN, workspace_addin_iface_init))

static void
gbp_deviceui_workspace_addin_class_init (GbpDeviceuiWorkspaceAddinClass *klass)
{
}

static void
gbp_deviceui_workspace_addin_init (GbpDeviceuiWorkspaceAddin *self)
{
}
