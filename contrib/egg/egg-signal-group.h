/* egg-signal-group.h
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

#ifndef EGG_SIGNAL_GROUP_H
#define EGG_SIGNAL_GROUP_H

#include <glib-object.h>

G_BEGIN_DECLS

#define EGG_TYPE_SIGNAL_GROUP (egg_signal_group_get_type())

G_DECLARE_FINAL_TYPE (EggSignalGroup, egg_signal_group, EGG, SIGNAL_GROUP, GObject)

EggSignalGroup *egg_signal_group_new             (GType           target_type);

void            egg_signal_group_set_target      (EggSignalGroup *self,
                                                  gpointer        target);
gpointer        egg_signal_group_get_target      (EggSignalGroup *self);

void            egg_signal_group_block           (EggSignalGroup *self);
void            egg_signal_group_unblock         (EggSignalGroup *self);

void            egg_signal_group_connect_object  (EggSignalGroup *self,
                                                  const gchar    *detailed_signal,
                                                  GCallback       c_handler,
                                                  gpointer        object,
                                                  GConnectFlags   flags);
void            egg_signal_group_connect_data    (EggSignalGroup *self,
                                                  const gchar    *detailed_signal,
                                                  GCallback       c_handler,
                                                  gpointer        data,
                                                  GClosureNotify  notify,
                                                  GConnectFlags   flags);
void            egg_signal_group_connect         (EggSignalGroup *self,
                                                  const gchar    *detailed_signal,
                                                  GCallback       c_handler,
                                                  gpointer        data);
void            egg_signal_group_connect_after   (EggSignalGroup *self,
                                                  const gchar    *detailed_signal,
                                                  GCallback       c_handler,
                                                  gpointer        data);
void            egg_signal_group_connect_swapped (EggSignalGroup *self,
                                                  const gchar    *detailed_signal,
                                                  GCallback       c_handler,
                                                  gpointer        data);

G_END_DECLS

#endif /* EGG_SIGNAL_GROUP_H */
