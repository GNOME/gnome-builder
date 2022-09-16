/* ide-session-item.h
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#if !defined (IDE_GUI_INSIDE) && !defined (IDE_GUI_COMPILATION)
# error "Only <libide-gui.h> can be included directly."
#endif

#include <libpanel.h>

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_SESSION_ITEM (ide_session_item_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeSessionItem, ide_session_item, IDE, SESSION_ITEM, GObject)

IDE_AVAILABLE_IN_ALL
IdeSessionItem *ide_session_item_new                    (void);
IDE_AVAILABLE_IN_ALL
PanelPosition  *ide_session_item_get_position           (IdeSessionItem      *self);
IDE_AVAILABLE_IN_ALL
void            ide_session_item_set_position           (IdeSessionItem      *self,
                                                         PanelPosition       *position);
IDE_AVAILABLE_IN_ALL
const char     *ide_session_item_get_id                 (IdeSessionItem      *self);
IDE_AVAILABLE_IN_ALL
void            ide_session_item_set_id                 (IdeSessionItem      *self,
                                                         const char          *id);
IDE_AVAILABLE_IN_ALL
const char     *ide_session_item_get_workspace          (IdeSessionItem      *self);
IDE_AVAILABLE_IN_ALL
void            ide_session_item_set_workspace          (IdeSessionItem      *self,
                                                         const char          *workspace);
IDE_AVAILABLE_IN_ALL
const char     *ide_session_item_get_module_name        (IdeSessionItem      *self);
IDE_AVAILABLE_IN_ALL
void            ide_session_item_set_module_name        (IdeSessionItem      *self,
                                                         const char          *module_name);
IDE_AVAILABLE_IN_ALL
const char     *ide_session_item_get_type_hint          (IdeSessionItem      *self);
IDE_AVAILABLE_IN_ALL
void            ide_session_item_set_type_hint          (IdeSessionItem      *self,
                                                         const char          *type_hint);
IDE_AVAILABLE_IN_ALL
gboolean        ide_session_item_has_metadata           (IdeSessionItem      *self,
                                                         const char          *key,
                                                         const GVariantType **value_type);
IDE_AVAILABLE_IN_ALL
gboolean        ide_session_item_has_metadata_with_type (IdeSessionItem      *self,
                                                         const char          *key,
                                                         const GVariantType  *expected_type);
IDE_AVAILABLE_IN_ALL
gboolean        ide_session_item_get_metadata           (IdeSessionItem      *self,
                                                         const char          *key,
                                                         const char          *format,
                                                         ...);
IDE_AVAILABLE_IN_ALL
void            ide_session_item_set_metadata           (IdeSessionItem      *self,
                                                         const char          *key,
                                                         const char          *format,
                                                         ...);
IDE_AVAILABLE_IN_ALL
GVariant       *ide_session_item_get_metadata_value     (IdeSessionItem      *self,
                                                         const char          *key,
                                                         const GVariantType  *expected_type);
IDE_AVAILABLE_IN_ALL
void            ide_session_item_set_metadata_value     (IdeSessionItem      *self,
                                                         const char          *key,
                                                         GVariant            *value);

G_END_DECLS
