/* gbp-flatpak-install-dialog.h
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

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GBP_TYPE_FLATPAK_INSTALL_DIALOG (gbp_flatpak_install_dialog_get_type())

G_DECLARE_FINAL_TYPE (GbpFlatpakInstallDialog, gbp_flatpak_install_dialog, GBP, FLATPAK_INSTALL_DIALOG, AdwDialog)

GbpFlatpakInstallDialog  *gbp_flatpak_install_dialog_new              (void);
gboolean                  gbp_flatpak_install_dialog_is_empty         (GbpFlatpakInstallDialog  *self);
void                      gbp_flatpak_install_dialog_add_runtime      (GbpFlatpakInstallDialog  *self,
                                                                       const gchar              *runtime_id);
void                      gbp_flatpak_install_dialog_run_async        (GbpFlatpakInstallDialog  *self,
                                                                       GtkWidget                *parent,
                                                                       GCancellable             *cancellable,
                                                                       GAsyncReadyCallback       callback,
                                                                       gpointer                  user_data);
gboolean                  gbp_flatpak_install_dialog_run_finish       (GbpFlatpakInstallDialog  *self,
                                                                       GAsyncResult             *result,
                                                                       GError                  **error);

G_END_DECLS
