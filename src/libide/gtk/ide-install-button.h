/* ide-install-button.h
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

#pragma once

#include <gtk/gtk.h>

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_INSTALL_BUTTON (ide_install_button_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeInstallButton, ide_install_button, IDE, INSTALL_BUTTON, GtkWidget)

IDE_AVAILABLE_IN_ALL
GtkWidget  *ide_install_button_new       (void);
IDE_AVAILABLE_IN_ALL
void        ide_install_button_cancel    (IdeInstallButton *self);
IDE_AVAILABLE_IN_ALL
const char *ide_install_button_get_label (IdeInstallButton *self);
IDE_AVAILABLE_IN_ALL
void        ide_install_button_set_label (IdeInstallButton *self,
                                          const char       *label);

G_END_DECLS
