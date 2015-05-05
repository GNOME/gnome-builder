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

#include <gio/gio.h>

G_BEGIN_DECLS

#define EGG_TYPE_STATE_MACHINE       (egg_state_machine_get_type())
#define EGG_TYPE_STATE_MACHINE_ERROR (egg_state_machine_get_type())
#define EGG_STATE_MACHINE_ERROR      (egg_state_machine_error_quark())
#define EGG_TYPE_STATE_TRANSITION    (egg_state_transition_get_type())

G_DECLARE_DERIVABLE_TYPE (EggStateMachine, egg_state_machine, EGG, STATE_MACHINE, GObject)

typedef enum
{
  EGG_STATE_TRANSITION_IGNORED = 0,
  EGG_STATE_TRANSITION_SUCCESS = 1,
  EGG_STATE_TRANSITION_INVALID = 2,
} EggStateTransition;

typedef enum
{
  EGG_STATE_MACHINE_ERROR_INVALID_TRANSITION = 1,
} EggStateMachineError;

struct _EggStateMachineClass
{
  GObjectClass parent;

  EggStateTransition (*transition) (EggStateMachine  *self,
                                    const gchar      *old_state,
                                    const gchar      *new_state,
                                    GError          **error);
};

GType               egg_state_transition_get_type    (void);
GType               egg_state_machine_error_get_type (void);
GQuark              egg_state_machine_error_quark    (void);
EggStateMachine    *egg_state_machine_new            (void);
EggStateTransition  egg_state_machine_transition     (EggStateMachine  *self,
                                                      const gchar      *new_state,
                                                      GError          **error);
const gchar        *egg_state_machine_get_state      (EggStateMachine  *self);
void                egg_state_machine_bind           (EggStateMachine  *self,
                                                      const gchar      *state,
                                                      gpointer          source,
                                                      const gchar      *source_property,
                                                      gpointer          target,
                                                      const gchar      *target_property,
                                                      GBindingFlags     flags);
void                egg_state_machine_connect_object (EggStateMachine  *self,
                                                      const gchar      *state,
                                                      gpointer          instance,
                                                      const gchar      *detailed_signal,
                                                      GCallback         callback,
                                                      gpointer          user_data,
                                                      GConnectFlags     flags);
void                egg_state_machine_add_action     (EggStateMachine  *self,
                                                      const gchar      *state,
                                                      GSimpleAction    *action,
                                                      gboolean          invert_enabled);

G_END_DECLS

#endif /* EGG_STATE_MACHINE_H */
