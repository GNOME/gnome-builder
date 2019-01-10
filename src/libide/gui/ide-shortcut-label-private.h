/* ide-shortcut-label-private.h
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define IDE_TYPE_SHORTCUT_LABEL (ide_shortcut_label_get_type())

G_DECLARE_FINAL_TYPE (IdeShortcutLabel, ide_shortcut_label, IDE, SHORTCUT_LABEL, GtkBox)

GtkWidget   *ide_shortcut_label_new             (void);
const gchar *ide_shortcut_label_get_accel       (IdeShortcutLabel *self);
void         ide_shortcut_label_set_accel       (IdeShortcutLabel *self,
                                                 const gchar      *accel);
const gchar *ide_shortcut_label_get_action      (IdeShortcutLabel *self);
void         ide_shortcut_label_set_action      (IdeShortcutLabel *self,
                                                 const gchar      *action);
const gchar *ide_shortcut_label_get_command     (IdeShortcutLabel *self);
void         ide_shortcut_label_set_command     (IdeShortcutLabel *self,
                                                 const gchar      *command);
const gchar *ide_shortcut_label_get_title       (IdeShortcutLabel *self);
void         ide_shortcut_label_set_title       (IdeShortcutLabel *self,
                                                 const gchar      *title);

G_END_DECLS
