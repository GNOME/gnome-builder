/* ipc-flatpak-transfer-impl.c
 *
 * Copyright 2021 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ipc-flatpak-transfer-impl"

#include "config.h"

#include <libide-core.h>
#include <libide-gui.h>

#include "ipc-flatpak-transfer-impl.h"
#include "gbp-flatpak-install-dialog.h"

struct _IpcFlatpakTransferImpl
{
  IpcFlatpakTransferSkeleton parent_instance;
  IdeContext *context;
};

static void transfer_iface_init (IpcFlatpakTransferIface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (IpcFlatpakTransferImpl, ipc_flatpak_transfer_impl, IPC_TYPE_FLATPAK_TRANSFER_SKELETON,
                         G_IMPLEMENT_INTERFACE (IPC_TYPE_FLATPAK_TRANSFER, transfer_iface_init))

typedef struct
{
  GDBusMethodInvocation *invocation;
  IpcFlatpakTransfer *transfer;
} Confirm;

static void
gbp_flatpak_runtime_provider_handle_confirm_cb (GObject      *object,
                                                GAsyncResult *result,
                                                gpointer      user_data)
{
  GbpFlatpakInstallDialog *dialog = (GbpFlatpakInstallDialog *)object;
  Confirm *state = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (GBP_IS_FLATPAK_INSTALL_DIALOG (dialog));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (state != NULL);
  g_assert (G_IS_DBUS_METHOD_INVOCATION (state->invocation));
  g_assert (IPC_IS_FLATPAK_TRANSFER (state->transfer));

  if (!gbp_flatpak_install_dialog_run_finish (dialog, result, &error))
    g_dbus_method_invocation_return_error (g_steal_pointer (&state->invocation),
                                           G_DBUS_ERROR,
                                           G_DBUS_ERROR_FAILED,
                                           "Unconfirmed request");
  else
    ipc_flatpak_transfer_complete_confirm (state->transfer,
                                           g_steal_pointer (&state->invocation));

  g_clear_object (&state->invocation);
  g_clear_object (&state->transfer);
  g_slice_free (Confirm, state);
}

static gboolean
ipc_flatpak_transfer_impl_handle_confirm (IpcFlatpakTransfer    *transfer,
                                          GDBusMethodInvocation *invocation,
                                          const char * const    *refs)
{
  IpcFlatpakTransferImpl *self = (IpcFlatpakTransferImpl *)transfer;
  GbpFlatpakInstallDialog *dialog;
  IdeWorkbench *workbench;
  IdeWorkspace *workspace;
  Confirm *state;

  g_assert (IPC_IS_FLATPAK_TRANSFER_IMPL (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (refs != NULL);

  workbench = ide_workbench_from_context (self->context);
  workspace = ide_workbench_get_current_workspace (workbench);
  dialog = gbp_flatpak_install_dialog_new (GTK_WINDOW (workspace));

  for (guint i = 0; refs[i]; i++)
    gbp_flatpak_install_dialog_add_runtime (dialog, refs[i]);

  if (gbp_flatpak_install_dialog_is_empty (dialog))
    {
      gtk_widget_destroy (GTK_WIDGET (dialog));
      ipc_flatpak_transfer_complete_confirm (transfer, g_steal_pointer (&invocation));
      return TRUE;
    }

  state = g_slice_new0 (Confirm);
  state->transfer = g_object_ref (transfer);
  state->invocation = g_object_ref (invocation);

  gbp_flatpak_install_dialog_run_async (dialog,
                                        NULL,
                                        gbp_flatpak_runtime_provider_handle_confirm_cb,
                                        state);

  return TRUE;
}

static void
ipc_flatpak_transfer_impl_finalize (GObject *object)
{
  IpcFlatpakTransferImpl *self = (IpcFlatpakTransferImpl *)object;

  g_clear_object (&self->context);

  G_OBJECT_CLASS (ipc_flatpak_transfer_impl_parent_class)->finalize (object);
}

static void
ipc_flatpak_transfer_impl_class_init (IpcFlatpakTransferImplClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ipc_flatpak_transfer_impl_finalize;
}

static void
ipc_flatpak_transfer_impl_init (IpcFlatpakTransferImpl *self)
{
}

IpcFlatpakTransfer *
ipc_flatpak_transfer_impl_new (IdeContext *context)
{
  IpcFlatpakTransferImpl *self = g_object_new (IPC_TYPE_FLATPAK_TRANSFER_IMPL, NULL);
  g_set_object (&self->context, context);
  return IPC_FLATPAK_TRANSFER (self);
}

static void
transfer_iface_init (IpcFlatpakTransferIface *iface)
{
  iface->handle_confirm = ipc_flatpak_transfer_impl_handle_confirm;
}
