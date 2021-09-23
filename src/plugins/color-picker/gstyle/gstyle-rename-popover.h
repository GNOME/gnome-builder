/* gstyle-rename-popover.h
 *
 * Copyright 2016 Sebastien Lafargue <slafargue@gnome.org>
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

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GSTYLE_TYPE_RENAME_POPOVER (gstyle_rename_popover_get_type())

G_DECLARE_FINAL_TYPE (GstyleRenamePopover, gstyle_rename_popover, GSTYLE, RENAME_POPOVER, GtkPopover)

GstyleRenamePopover     *gstyle_rename_popover_new                (void);
const gchar             *gstyle_rename_popover_get_label          (GstyleRenamePopover    *self);
const gchar             *gstyle_rename_popover_get_message        (GstyleRenamePopover    *self);
const gchar             *gstyle_rename_popover_get_name           (GstyleRenamePopover    *self);
void                     gstyle_rename_popover_set_label          (GstyleRenamePopover    *self,
                                                                   const gchar            *label);
void                     gstyle_rename_popover_set_message        (GstyleRenamePopover    *self,
                                                                   const gchar            *message);
void                     gstyle_rename_popover_set_name           (GstyleRenamePopover    *self,
                                                                   const gchar            *name);

G_END_DECLS
