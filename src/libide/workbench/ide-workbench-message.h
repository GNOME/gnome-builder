/* ide-workbench-message.h
 *
 * Copyright 2017 Christian Hergert <chergert@redhat.com>
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
 */

#pragma once

#include <gtk/gtk.h>

#include "ide-version-macros.h"

G_BEGIN_DECLS

#define IDE_TYPE_WORKBENCH_MESSAGE (ide_workbench_message_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeWorkbenchMessage, ide_workbench_message, IDE, WORKBENCH_MESSAGE, GtkInfoBar)

IDE_AVAILABLE_IN_ALL
GtkWidget   *ide_workbench_message_new          (void);
IDE_AVAILABLE_IN_ALL
const gchar *ide_workbench_message_get_id       (IdeWorkbenchMessage *self);
IDE_AVAILABLE_IN_ALL
void         ide_workbench_message_set_id       (IdeWorkbenchMessage *self,
                                                 const gchar         *id);
IDE_AVAILABLE_IN_ALL
const gchar *ide_workbench_message_get_title    (IdeWorkbenchMessage *self);
IDE_AVAILABLE_IN_ALL
void         ide_workbench_message_set_title    (IdeWorkbenchMessage *self,
                                                 const gchar         *title);
IDE_AVAILABLE_IN_ALL
const gchar *ide_workbench_message_get_subtitle (IdeWorkbenchMessage *self);
IDE_AVAILABLE_IN_ALL
void         ide_workbench_message_set_subtitle (IdeWorkbenchMessage *self,
                                                 const gchar         *subtitle);
IDE_AVAILABLE_IN_ALL
void         ide_workbench_message_add_action   (IdeWorkbenchMessage *self,
                                                 const gchar         *label,
                                                 const gchar         *action_name);

G_END_DECLS
