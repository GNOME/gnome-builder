/* gbp-confirm-save-dialog.h
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
#include <libide-code.h>

G_BEGIN_DECLS

#define GBP_TYPE_CONFIRM_SAVE_DIALOG (gbp_confirm_save_dialog_get_type())

G_DECLARE_FINAL_TYPE (GbpConfirmSaveDialog, gbp_confirm_save_dialog, GBP, CONFIRM_SAVE_DIALOG, GtkMessageDialog)

GbpConfirmSaveDialog *gbp_confirm_save_dialog_new        (GtkWindow             *transient_for);
void                  gbp_confirm_save_dialog_add_buffer (GbpConfirmSaveDialog  *self,
                                                          IdeBuffer             *buffer);
void                  gbp_confirm_save_dialog_run_async  (GbpConfirmSaveDialog  *self,
                                                          GCancellable          *cancellable,
                                                          GAsyncReadyCallback    callback,
                                                          gpointer               user_data);
gboolean              gbp_confirm_save_dialog_run_finish (GbpConfirmSaveDialog  *self,
                                                          GAsyncResult          *result,
                                                          GError               **error);

G_END_DECLS
