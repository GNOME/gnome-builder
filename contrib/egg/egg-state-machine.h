/* egg-state-machine.h
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

#ifndef EGG_STATE_MACHINE_H
#define EGG_STATE_MACHINE_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EGG_TYPE_STATE_MACHINE  (egg_state_machine_get_type())

G_DECLARE_DERIVABLE_TYPE (EggStateMachine, egg_state_machine, EGG, STATE_MACHINE, GObject)

struct _EggStateMachineClass
{
  GObjectClass parent;
};

EggStateMachine *egg_state_machine_new               (void);
const gchar     *egg_state_machine_get_state         (EggStateMachine *self);
void             egg_state_machine_set_state         (EggStateMachine *self,
                                                      const gchar     *state);
GAction         *egg_state_machine_create_action     (EggStateMachine *self,
                                                      const gchar     *name);
void             egg_state_machine_add_property      (EggStateMachine *self,
                                                      const gchar     *state,
                                                      gpointer         object,
                                                      const gchar     *property,
                                                      const GValue    *value);
void             egg_state_machine_add_binding       (EggStateMachine *self,
                                                      const gchar     *state,
                                                      gpointer         source_object,
                                                      const gchar     *source_property,
                                                      gpointer         target_object,
                                                      const gchar     *target_property,
                                                      GBindingFlags    flags);
void             egg_state_machine_add_style         (EggStateMachine *self,
                                                      const gchar     *state,
                                                      GtkWidget       *widget,
                                                      const gchar     *style);
void             egg_state_machine_freeze            (EggStateMachine *self);
void             egg_state_machine_thaw              (EggStateMachine *self);
void             egg_state_machine_connect_object    (EggStateMachine *self,
                                                      const gchar     *state,
                                                      gpointer         source,
                                                      const gchar     *detailed_signal,
                                                      GCallback        callback,
                                                      gpointer         user_data,
                                                      GConnectFlags    flags);

G_END_DECLS

#endif /* EGG_STATE_MACHINE_H */
