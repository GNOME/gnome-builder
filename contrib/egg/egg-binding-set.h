/* egg-binding-set.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#ifndef EGG_BINDING_SET_H
#define EGG_BINDING_SET_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define EGG_TYPE_BINDING_SET (egg_binding_set_get_type())

G_DECLARE_FINAL_TYPE (EggBindingSet, egg_binding_set, EGG, BINDING_SET, GObject)

EggBindingSet *egg_binding_set_new        (void);

GObject       *egg_binding_set_get_source (EggBindingSet         *self);
void           egg_binding_set_set_source (EggBindingSet         *self,
                                           gpointer               source);

void           egg_binding_set_bind       (EggBindingSet         *self,
                                           const gchar           *source_property,
                                           gpointer               target,
                                           const gchar           *target_property,
                                           GBindingFlags          flags);
void           egg_binding_set_bind_full  (EggBindingSet         *self,
                                           const gchar           *source_property,
                                           gpointer               target,
                                           const gchar           *target_property,
                                           GBindingFlags          flags,
                                           GBindingTransformFunc  transform_to,
                                           GBindingTransformFunc  transform_from,
                                           gpointer               user_data,
                                           GDestroyNotify         notify);
void           egg_binding_set_bind_with_closures
                                          (EggBindingSet         *self,
                                           const gchar           *source_property,
                                           gpointer               target,
                                           const gchar           *target_property,
                                           GBindingFlags          flags,
                                           GClosure              *transform_to,
                                           GClosure              *transform_from);

G_END_DECLS

#endif /* EGG_BINDING_SET_H */
