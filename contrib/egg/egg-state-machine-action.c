/* egg-state-machine-action.c
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

#include <glib/gi18n.h>

#include "egg-state-machine.h"
#include "egg-state-machine-action.h"

struct _EggStateMachineAction
{
  GObject          parent_instance;

  gchar           *name;
  EggStateMachine *state_machine;
};

static void action_iface_init (GActionInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EggStateMachineAction, egg_state_machine_action, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ACTION, action_iface_init))

enum {
  PROP_0,
  PROP_NAME,
  PROP_STATE_MACHINE,
  LAST_PROP,

  PROP_ENABLED,
  PROP_PARAMETER_TYPE,
  PROP_STATE,
  PROP_STATE_TYPE,
};

static GParamSpec *gParamSpecs [LAST_PROP];

static gboolean
egg_state_machine_action_get_enabled (GAction *action)
{
  return TRUE;
}

static const gchar *
egg_state_machine_action_get_name (GAction *action)
{
  EggStateMachineAction *self = (EggStateMachineAction *)action;

  g_return_val_if_fail (EGG_IS_STATE_MACHINE_ACTION (self), NULL);

  return self->name;
}

static const GVariantType *
egg_state_machine_action_get_parameter_type (GAction *action)
{
  return G_VARIANT_TYPE_STRING;
}

static const GVariantType *
egg_state_machine_action_get_state_type (GAction *action)
{
  return G_VARIANT_TYPE_STRING;
}

static GVariant *
egg_state_machine_action_get_state (GAction *action)
{
  EggStateMachineAction *self = (EggStateMachineAction *)action;
  const gchar *state;

  g_return_val_if_fail (EGG_IS_STATE_MACHINE_ACTION (self), NULL);

  state = egg_state_machine_get_state (self->state_machine);

  if (state != NULL)
    return g_variant_ref_sink (g_variant_new_string (state));

  return NULL;
}

static void
egg_state_machine_action_state_set_cb (EggStateMachineAction *self,
                                       GParamSpec            *pspec,
                                       EggStateMachine       *state_machine)
{
  g_return_if_fail (EGG_IS_STATE_MACHINE_ACTION (self));

  g_object_notify (G_OBJECT (self), "state");
}

static void
egg_state_machine_action_set_state_machine (EggStateMachineAction *self,
                                            EggStateMachine       *state_machine)
{
  g_return_if_fail (EGG_IS_STATE_MACHINE_ACTION (self));
  g_return_if_fail (EGG_IS_STATE_MACHINE (state_machine));
  g_return_if_fail (self->state_machine == NULL);

  if (g_set_object (&self->state_machine, state_machine))
    {
      g_signal_connect_object (state_machine,
                               "notify::state",
                               G_CALLBACK (egg_state_machine_action_state_set_cb),
                               self,
                               G_CONNECT_SWAPPED);
    }
}

static void
egg_state_machine_action_activate (GAction  *action,
                                   GVariant *param)
{
  EggStateMachineAction *self = (EggStateMachineAction *)action;

  g_assert (EGG_IS_STATE_MACHINE_ACTION (self));
  g_assert (EGG_IS_STATE_MACHINE (self->state_machine));

  if ((param != NULL) && g_variant_is_of_type (param, G_VARIANT_TYPE_STRING))
    {
      const gchar *state;

      if ((state = g_variant_get_string (param, NULL)))
        egg_state_machine_set_state (self->state_machine, state);
    }
}

static void
egg_state_machine_action_finalize (GObject *object)
{
  EggStateMachineAction *self = (EggStateMachineAction *)object;

  g_clear_pointer (&self->name, g_free);
  g_clear_object (&self->state_machine);

  G_OBJECT_CLASS (egg_state_machine_action_parent_class)->finalize (object);
}

static void
egg_state_machine_action_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  EggStateMachineAction *self = EGG_STATE_MACHINE_ACTION (object);
  GAction *action = (GAction *)object;

  switch (prop_id)
    {
    case PROP_ENABLED:
      g_value_set_boolean (value, egg_state_machine_action_get_enabled (action));
      break;

    case PROP_NAME:
      g_value_set_string (value, egg_state_machine_action_get_name (action));
      break;

    case PROP_PARAMETER_TYPE:
      g_value_set_boxed (value, egg_state_machine_action_get_parameter_type (action));
      break;

    case PROP_STATE:
      g_value_set_boxed (value, egg_state_machine_action_get_state (action));
      break;

    case PROP_STATE_MACHINE:
      g_value_set_object (value, self->state_machine);
      break;

    case PROP_STATE_TYPE:
      g_value_set_boxed (value, egg_state_machine_action_get_state_type (action));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
egg_state_machine_action_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  EggStateMachineAction *self = EGG_STATE_MACHINE_ACTION (object);

  switch (prop_id)
    {
    case PROP_STATE_MACHINE:
      egg_state_machine_action_set_state_machine (self, g_value_get_object (value));
      break;

    case PROP_NAME:
      self->name = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
egg_state_machine_action_class_init (EggStateMachineActionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = egg_state_machine_action_finalize;
  object_class->get_property = egg_state_machine_action_get_property;
  object_class->set_property = egg_state_machine_action_set_property;

  gParamSpecs [PROP_NAME] =
    g_param_spec_string ("name",
                         _("Name"),
                         _("The name of the action"),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  gParamSpecs [PROP_STATE_MACHINE] =
    g_param_spec_object ("state-machine",
                         _("State Machine"),
                         _("State Machine"),
                         EGG_TYPE_STATE_MACHINE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);

  g_object_class_override_property (object_class, PROP_PARAMETER_TYPE, "parameter-type");
  g_object_class_override_property (object_class, PROP_ENABLED, "enabled");
  g_object_class_override_property (object_class, PROP_STATE_TYPE, "state-type");
  g_object_class_override_property (object_class, PROP_STATE, "state");
}

static void
egg_state_machine_action_init (EggStateMachineAction *self)
{
}

static void
action_iface_init (GActionInterface *iface)
{
  iface->get_enabled = egg_state_machine_action_get_enabled;
  iface->get_name = egg_state_machine_action_get_name;
  iface->get_parameter_type = egg_state_machine_action_get_parameter_type;
  iface->get_state_type = egg_state_machine_action_get_state_type;
  iface->get_state = egg_state_machine_action_get_state;
  iface->activate = egg_state_machine_action_activate;
}
