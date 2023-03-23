/* ide-property-action-group.h
 *
 * Copyright (C) 2017-2022 Christian Hergert <chergert@redhat.com>
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

#if !defined (IDE_CORE_INSIDE) && !defined (IDE_CORE_COMPILATION)
# error "Only <libide-core.h> can be included directly."
#endif

#include <gio/gio.h>

#include "ide-version-macros.h"

G_BEGIN_DECLS

#define IDE_TYPE_PROPERTY_ACTION_GROUP (ide_property_action_group_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdePropertyActionGroup, ide_property_action_group, IDE, PROPERTY_ACTION_GROUP, GObject)

IDE_AVAILABLE_IN_ALL
IdePropertyActionGroup *ide_property_action_group_new           (GType                  item_type);
IDE_AVAILABLE_IN_ALL
GType                   ide_property_action_group_get_item_type (IdePropertyActionGroup *self);
IDE_AVAILABLE_IN_ALL
gpointer                ide_property_action_group_dup_item      (IdePropertyActionGroup *self);
IDE_AVAILABLE_IN_ALL
void                    ide_property_action_group_set_item      (IdePropertyActionGroup *self,
                                                                 gpointer                item);
IDE_AVAILABLE_IN_ALL
void                    ide_property_action_group_add_all       (IdePropertyActionGroup *self);
IDE_AVAILABLE_IN_ALL
void                    ide_property_action_group_add           (IdePropertyActionGroup *self,
                                                                 const char             *action_name,
                                                                 const char             *property_name);
IDE_AVAILABLE_IN_ALL
void                    ide_property_action_group_add_string    (IdePropertyActionGroup *self,
                                                                 const char             *action_name,
                                                                 const char             *property_name,
                                                                 gboolean                treat_null_as_empty);

G_END_DECLS
