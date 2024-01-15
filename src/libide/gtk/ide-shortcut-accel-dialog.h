/* ide-shortcut-accel-dialog.h
 *
 * Copyright 2017-2022 Christian Hergert <chergert@redhat.com>
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

#if !defined (IDE_GTK_INSIDE) && !defined (IDE_GTK_COMPILATION)
# error "Only <libide-gtk.h> can be included directly."
#endif

#include <adwaita.h>

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_SHORTCUT_ACCEL_DIALOG (ide_shortcut_accel_dialog_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeShortcutAccelDialog, ide_shortcut_accel_dialog, IDE, SHORTCUT_ACCEL_DIALOG, AdwWindow)

IDE_AVAILABLE_IN_ALL
GtkWidget              *ide_shortcut_accel_dialog_new                (void);
IDE_AVAILABLE_IN_ALL
char                   *ide_shortcut_accel_dialog_get_accelerator    (IdeShortcutAccelDialog *self);
IDE_AVAILABLE_IN_ALL
void                    ide_shortcut_accel_dialog_set_accelerator    (IdeShortcutAccelDialog *self,
                                                                      const char             *accelerator);
IDE_AVAILABLE_IN_ALL
const char             *ide_shortcut_accel_dialog_get_shortcut_title (IdeShortcutAccelDialog *self);
IDE_AVAILABLE_IN_ALL
void                    ide_shortcut_accel_dialog_set_shortcut_title (IdeShortcutAccelDialog *self,
                                                                      const char             *title);

G_END_DECLS
