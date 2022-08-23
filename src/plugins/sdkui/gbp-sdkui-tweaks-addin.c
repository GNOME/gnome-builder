/* gbp-sdkui-tweaks-addin.c
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

#define G_LOG_DOMAIN "gbp-sdkui-tweaks-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-foundry.h>
#include <libide-gtk.h>

#include "gbp-sdkui-tweaks-addin.h"

struct _GbpSdkuiTweaksAddin
{
  IdeTweaksAddin parent_instance;
};

G_DEFINE_FINAL_TYPE (GbpSdkuiTweaksAddin, gbp_sdkui_tweaks_addin, IDE_TYPE_TWEAKS_ADDIN)

static void
gbp_sdkui_tweaks_addin_update_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  g_autoptr(IdeInstallButton) button = user_data;

  IDE_ENTRY;

  g_assert (IDE_IS_INSTALL_BUTTON (button));

  ide_install_button_cancel (button);

  IDE_EXIT;
}

static void
gbp_sdkui_tweaks_addin_install_cb (IdeSdk           *sdk,
                                   IdeNotification  *notif,
                                   GCancellable     *cancellable,
                                   IdeInstallButton *button)
{
  IdeSdkProvider *provider;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SDK (sdk));
  g_assert (IDE_IS_INSTALL_BUTTON (button));
  g_assert (G_IS_CANCELLABLE (cancellable));
  g_assert (IDE_IS_NOTIFICATION (notif));

  if (!(provider = ide_sdk_get_provider (sdk)))
    IDE_EXIT;

  ide_sdk_provider_update_async (provider,
                                 sdk,
                                 notif,
                                 cancellable,
                                 gbp_sdkui_tweaks_addin_update_cb,
                                 g_object_ref (button));

  IDE_EXIT;
}

static GtkWidget *
create_sdk_row_cb (gpointer item,
                   gpointer user_data)
{
  IdeInstallButton *button;
  AdwActionRow *row;
  IdeSdk *sdk = item;

  g_assert (IDE_IS_SDK (sdk));

  row = g_object_new (ADW_TYPE_ACTION_ROW,
                      "title", ide_sdk_get_title (sdk),
                      "subtitle", ide_sdk_get_subtitle (sdk),
                      NULL);

  button = g_object_new (IDE_TYPE_INSTALL_BUTTON,
                         "label", _("Update"),
                         "valign", GTK_ALIGN_CENTER,
                         NULL);
  g_object_bind_property (sdk, "can-update",
                          button, "sensitive",
                          G_BINDING_SYNC_CREATE);
  g_signal_connect_object (button,
                           "install",
                           G_CALLBACK (gbp_sdkui_tweaks_addin_install_cb),
                           sdk,
                           G_CONNECT_SWAPPED);
  adw_action_row_add_suffix (row, GTK_WIDGET (button));

  return GTK_WIDGET (row);
}

static GtkWidget *
create_sdk_list_cb (GbpSdkuiTweaksAddin *self,
                    IdeTweaksWidget     *widget,
                    IdeTweaksWidget     *instance)
{
  IdeSdkManager *sdk_manager;
  GtkListBox *list;

  g_assert (GBP_IS_SDKUI_TWEAKS_ADDIN (self));
  g_assert (IDE_IS_TWEAKS_WIDGET (widget));
  g_assert (IDE_IS_TWEAKS_WIDGET (instance));

  sdk_manager = ide_sdk_manager_get_default ();

  list = g_object_new (GTK_TYPE_LIST_BOX,
                       "css-classes", IDE_STRV_INIT ("boxed-list"),
                       "selection-mode", GTK_SELECTION_NONE,
                       NULL);
  gtk_list_box_bind_model (list,
                           G_LIST_MODEL (sdk_manager),
                           create_sdk_row_cb,
                           NULL, NULL);

  return GTK_WIDGET (list);
}

static void
gbp_sdkui_tweaks_addin_class_init (GbpSdkuiTweaksAddinClass *klass)
{
}

static void
gbp_sdkui_tweaks_addin_init (GbpSdkuiTweaksAddin *self)
{
  ide_tweaks_addin_set_resource_paths (IDE_TWEAKS_ADDIN (self),
                                       IDE_STRV_INIT ("/plugins/sdkui/tweaks.ui"));
  ide_tweaks_addin_bind_callback (IDE_TWEAKS_ADDIN (self), create_sdk_list_cb);
}
