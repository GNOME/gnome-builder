/* rust-analyzer-workbench-addin.c
 *
 * Copyright 2020 Günther Wagner <info@gunibert.de>
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

#include "rust-analyzer-workbench-addin.h"
#include <libide-gui.h>
#include <libide-core.h>
#include "rust-analyzer-transfer.h"
#include "rust-analyzer-service.h"

struct _RustAnalyzerWorkbenchAddin
{
  IdeObject parent_instance;
};

static void provider_iface_init (IdeWorkbenchAddinInterface *iface);

G_DEFINE_TYPE_WITH_CODE (RustAnalyzerWorkbenchAddin, rust_analyzer_workbench_addin, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, provider_iface_init))

static void
rust_analyzer_workbench_addin_class_init (RustAnalyzerWorkbenchAddinClass *klass)
{
}

static void
rust_analyzer_workbench_addin_init (RustAnalyzerWorkbenchAddin *self)
{
}

static void
rust_analyzer_workbench_addin_load (IdeWorkbenchAddin *addin,
                                    IdeWorkbench      *workbench)
{
}

static void
rust_analyzer_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                      IdeWorkbench      *workbench)
{
}

static void
rust_analyzer_service_downloaded_lsp (GObject      *object,
                                      GAsyncResult *res,
                                      gpointer      user_data)
{
  RustAnalyzerService *service = NULL;
  IdeContext *context = IDE_CONTEXT (user_data);

  g_return_if_fail (IDE_IS_CONTEXT (context));

  service = ide_object_ensure_child_typed (IDE_OBJECT (context), RUST_TYPE_ANALYZER_SERVICE);
  rust_analyzer_service_set_state (service, RUST_ANALYZER_SERVICE_READY);
  rust_analyzer_service_ensure_started (service);
}

static void
rust_analyzer_workbench_addin_remove_lsp (IdeTransfer *transfer,
                                          gpointer     user_data)
{
  g_autofree gchar *rust_analyzer_path = NULL;
  g_autoptr(GFile) rust_analyzer_bin = NULL;

  rust_analyzer_path = g_build_filename (g_get_home_dir (), ".cargo", "bin", "rust-analyzer", NULL);
  rust_analyzer_bin = g_file_new_for_path (rust_analyzer_path);
  g_file_trash (rust_analyzer_bin, NULL, NULL);
}

static void
rust_analyzer_workbench_addin_install_rust_analyzer (GSimpleAction *action,
                                                     GVariant      *parameter,
                                                     gpointer       user_data)
{
  g_autoptr(RustAnalyzerTransfer) transfer = NULL;
  IdeNotifications *notifications = NULL;
  IdeNotification *notification = NULL;
  IdeTransferManager *transfer_manager = NULL;

  IdeContext *context = IDE_CONTEXT (user_data);

  notifications = ide_object_get_child_typed (IDE_OBJECT (context), IDE_TYPE_NOTIFICATIONS);
  notification = ide_notifications_find_by_id (notifications, "org.gnome-builder.rust-analyzer");

  if (notification != NULL)
    ide_notification_withdraw (notification);

  transfer_manager = ide_transfer_manager_get_default ();
  transfer = rust_analyzer_transfer_new ();
  g_signal_connect (transfer, "cancelled", G_CALLBACK (rust_analyzer_workbench_addin_remove_lsp), NULL);

  notification = ide_transfer_create_notification (IDE_TRANSFER (transfer));
  ide_notification_attach (notification, IDE_OBJECT (context));

  ide_transfer_manager_execute_async (transfer_manager,
                                      IDE_TRANSFER (transfer),
                                      g_cancellable_new (),
                                      rust_analyzer_service_downloaded_lsp,
                                      context);
}

static void
rust_analyzer_workbench_addin_workspace_added (IdeWorkbenchAddin *addin,
                                               IdeWorkspace      *workspace)
{
  GSimpleAction *install_rust_analyzer = NULL;

  install_rust_analyzer = g_simple_action_new ("install-rust-analyzer", NULL);
  g_simple_action_set_enabled (install_rust_analyzer, TRUE);
  g_signal_connect (install_rust_analyzer,
                    "activate",
                    G_CALLBACK (rust_analyzer_workbench_addin_install_rust_analyzer),
                    ide_workspace_get_context (workspace));
  g_action_map_add_action (G_ACTION_MAP (workspace), G_ACTION (install_rust_analyzer));
}

static void
provider_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = rust_analyzer_workbench_addin_load;
  iface->unload = rust_analyzer_workbench_addin_unload;
  iface->workspace_added = rust_analyzer_workbench_addin_workspace_added;
}
