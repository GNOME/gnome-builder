/* ide-binding-group.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 * Copyright (C) 2015 Garrett Regier <garrettregier@gmail.com>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define IDE_TYPE_BINDING_GROUP (ide_binding_group_get_type())

G_DECLARE_FINAL_TYPE (IdeBindingGroup, ide_binding_group, IDE, BINDING_GROUP, GObject)

IdeBindingGroup *ide_binding_group_new                 (void);
GObject         *ide_binding_group_get_source         (IdeBindingGroup       *self);
void             ide_binding_group_set_source         (IdeBindingGroup       *self,
                                                       gpointer               source);
void             ide_binding_group_bind               (IdeBindingGroup       *self,
                                                       const gchar           *source_property,
                                                       gpointer               target,
                                                       const gchar           *target_property,
                                                       GBindingFlags          flags);
void             ide_binding_group_bind_full          (IdeBindingGroup       *self,
                                                       const gchar           *source_property,
                                                       gpointer               target,
                                                       const gchar           *target_property,
                                                       GBindingFlags          flags,
                                                       GBindingTransformFunc  transform_to,
                                                       GBindingTransformFunc  transform_from,
                                                       gpointer               user_data,
                                                       GDestroyNotify         user_data_destroy);
void             ide_binding_group_bind_with_closures (IdeBindingGroup       *self,
                                                       const gchar           *source_property,
                                                       gpointer               target,
                                                       const gchar           *target_property,
                                                       GBindingFlags          flags,
                                                       GClosure              *transform_to,
                                                       GClosure              *transform_from);

G_END_DECLS

