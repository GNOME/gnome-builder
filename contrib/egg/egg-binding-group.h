/* egg-binding-group.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 * Copyright (C) 2015 Garrett Regier <garrettregier@gmail.com>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
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

#ifndef EGG_BINDING_GROUP_H
#define EGG_BINDING_GROUP_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define EGG_TYPE_BINDING_GROUP (egg_binding_group_get_type())

G_DECLARE_FINAL_TYPE (EggBindingGroup, egg_binding_group, EGG, BINDING_GROUP, GObject)

EggBindingGroup *
          egg_binding_group_new        (void);

GObject  *egg_binding_group_get_source (EggBindingGroup       *self);
void      egg_binding_group_set_source (EggBindingGroup       *self,
                                        gpointer               source);

void      egg_binding_group_bind       (EggBindingGroup       *self,
                                        const gchar           *source_property,
                                        gpointer               target,
                                        const gchar           *target_property,
                                        GBindingFlags          flags);
void      egg_binding_group_bind_full  (EggBindingGroup       *self,
                                        const gchar           *source_property,
                                        gpointer               target,
                                        const gchar           *target_property,
                                        GBindingFlags          flags,
                                        GBindingTransformFunc  transform_to,
                                        GBindingTransformFunc  transform_from,
                                        gpointer               user_data,
                                        GDestroyNotify         user_data_destroy);
void           egg_binding_group_bind_with_closures
                                       (EggBindingGroup       *self,
                                        const gchar           *source_property,
                                        gpointer               target,
                                        const gchar           *target_property,
                                        GBindingFlags          flags,
                                        GClosure              *transform_to,
                                        GClosure              *transform_from);

G_END_DECLS

#endif /* EGG_BINDING_GROUP_H */
