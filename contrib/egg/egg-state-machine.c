/* egg-state-machine.c
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

#define G_LOG_DOMAIN "egg-state-machine"

#include <glib/gi18n.h>

#include "egg-binding-set.h"
#include "egg-signal-group.h"

#include "egg-state-machine.h"
#include "egg-state-machine-action.h"
#include "egg-state-machine-buildable.h"
#include "egg-state-machine-private.h"

G_DEFINE_QUARK (egg_state_machine_error, egg_state_machine_error)

G_DEFINE_TYPE_WITH_CODE (EggStateMachine, egg_state_machine, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (EggStateMachine)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                egg_state_machine_buildable_iface_init))

enum {
  PROP_0,
  PROP_STATE,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

static void
egg_state_free (gpointer data)
{
  EggState *state = data;

  g_free (state->name);
  g_hash_table_unref (state->signals);
  g_hash_table_unref (state->bindings);
  g_ptr_array_unref (state->properties);
  g_ptr_array_unref (state->styles);
  g_slice_free (EggState, state);
}

static void
egg_state_property_free (gpointer data)
{
  EggStateProperty *prop = data;

  g_free (prop->property);
  g_value_unset (&prop->value);
  g_slice_free (EggStateProperty, prop);
}

static void
egg_state_style_free (gpointer data)
{
  EggStateStyle *style = data;

  g_free (style->name);
  g_slice_free (EggStateStyle, style);
}

static void
egg_state_apply (EggStateMachine *self,
                 EggState        *state)
{
  GHashTableIter iter;
  gpointer key;
  gpointer value;
  gsize i;

  g_assert (EGG_IS_STATE_MACHINE (self));
  g_assert (state != NULL);

  g_hash_table_iter_init (&iter, state->bindings);
  while (g_hash_table_iter_next (&iter, &key, &value))
    egg_binding_set_set_source (value, key);

  g_hash_table_iter_init (&iter, state->signals);
  while (g_hash_table_iter_next (&iter, &key, &value))
    egg_signal_group_set_target (value, key);

  for (i = 0; i < state->properties->len; i++)
    {
      EggStateProperty *prop;

      prop = g_ptr_array_index (state->properties, i);
      g_object_set_property (prop->object, prop->property, &prop->value);
    }

  for (i = 0; i < state->styles->len; i++)
    {
      EggStateStyle *style;
      GtkStyleContext *style_context;

      style = g_ptr_array_index (state->styles, i);
      style_context = gtk_widget_get_style_context (GTK_WIDGET (style->widget));
      gtk_style_context_add_class (style_context, style->name);
    }
}

static void
egg_state_unapply (EggStateMachine *self,
                   EggState        *state)
{
  GHashTableIter iter;
  gpointer key;
  gpointer value;
  gsize i;

  g_assert (EGG_IS_STATE_MACHINE (self));
  g_assert (state != NULL);

  g_hash_table_iter_init (&iter, state->bindings);
  while (g_hash_table_iter_next (&iter, &key, &value))
    egg_binding_set_set_source (value, NULL);

  g_hash_table_iter_init (&iter, state->signals);
  while (g_hash_table_iter_next (&iter, &key, &value))
    egg_signal_group_set_target (value, NULL);

  for (i = 0; i < state->styles->len; i++)
    {
      EggStateStyle *style;
      GtkStyleContext *style_context;

      style = g_ptr_array_index (state->styles, i);
      style_context = gtk_widget_get_style_context (GTK_WIDGET (style->widget));
      gtk_style_context_remove_class (style_context, style->name);
    }
}

static EggState *
egg_state_machine_get_state_obj (EggStateMachine *self,
                                 const gchar     *state)
{
  EggStateMachinePrivate *priv = egg_state_machine_get_instance_private (self);
  EggState *state_obj;

  g_assert (EGG_IS_STATE_MACHINE (self));

  state_obj = g_hash_table_lookup (priv->states, state);

  if (state_obj == NULL)
    {
      state_obj = g_slice_new0 (EggState);
      state_obj->name = g_strdup (state);
      state_obj->signals = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_object_unref);
      state_obj->bindings = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_object_unref);
      state_obj->properties = g_ptr_array_new_with_free_func (egg_state_property_free);
      state_obj->styles = g_ptr_array_new_with_free_func (egg_state_style_free);
      g_hash_table_insert (priv->states, g_strdup (state), state_obj);
    }

  return state_obj;
}

static void
egg_state_machine_transition (EggStateMachine *self,
                              const gchar     *old_state,
                              const gchar     *new_state)
{
  EggState *state_obj;

  g_assert (EGG_IS_STATE_MACHINE (self));

  g_object_freeze_notify (G_OBJECT (self));

  if (old_state && (state_obj = egg_state_machine_get_state_obj (self, old_state)))
    egg_state_unapply (self, state_obj);

  if (new_state && (state_obj = egg_state_machine_get_state_obj (self, new_state)))
    egg_state_apply (self, state_obj);

  g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_STATE]);

  g_object_thaw_notify (G_OBJECT (self));
}

static void
egg_state_machine_finalize (GObject *object)
{
  EggStateMachine *self = (EggStateMachine *)object;
  EggStateMachinePrivate *priv = egg_state_machine_get_instance_private (self);

  g_clear_pointer (&priv->states, g_hash_table_unref);
  g_clear_pointer (&priv->state, g_free);

  G_OBJECT_CLASS (egg_state_machine_parent_class)->finalize (object);
}

static void
egg_state_machine_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  EggStateMachine *self = EGG_STATE_MACHINE (object);

  switch (prop_id)
    {
    case PROP_STATE:
      g_value_set_string (value, egg_state_machine_get_state (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
egg_state_machine_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  EggStateMachine *self = EGG_STATE_MACHINE (object);

  switch (prop_id)
    {
    case PROP_STATE:
      egg_state_machine_set_state (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
egg_state_machine_class_init (EggStateMachineClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = egg_state_machine_finalize;
  object_class->get_property = egg_state_machine_get_property;
  object_class->set_property = egg_state_machine_set_property;

  gParamSpecs [PROP_STATE] =
    g_param_spec_string ("state",
                         _("State"),
                         _("The current state of the machine."),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);
}

static void
egg_state_machine_init (EggStateMachine *self)
{
  EggStateMachinePrivate *priv = egg_state_machine_get_instance_private (self);

  priv->states = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, egg_state_free);
}

EggStateMachine *
egg_state_machine_new (void)
{
  return g_object_new (EGG_TYPE_STATE_MACHINE, NULL);
}

/**
 * egg_state_machine_get_state:
 * @self: the #EggStateMachine.
 *
 * Gets the #EggStateMachine:state property. This is the name of the
 * current state of the machine.
 *
 * Returns: The current state of the machine.
 */
const gchar *
egg_state_machine_get_state (EggStateMachine *self)
{
  EggStateMachinePrivate *priv = egg_state_machine_get_instance_private (self);

  g_return_if_fail (EGG_IS_STATE_MACHINE (self));

  return priv->state;
}

/**
 * egg_state_machine_set_state:
 * @self: the #EggStateMachine @self: the #
 *
 * Sets the #EggStateMachine:state property.
 *
 * Registered state transformations will be applied during the state
 * transformation.
 *
 * If the transition results in a cyclic operation, the state will stop at
 * the last state before the cycle was detected.
 */
void
egg_state_machine_set_state (EggStateMachine *self,
                             const gchar     *state)
{
  EggStateMachinePrivate *priv = egg_state_machine_get_instance_private (self);

  g_return_if_fail (EGG_IS_STATE_MACHINE (self));

  if (g_strcmp0 (priv->state, state) != 0)
    {
      gchar *old_state = priv->state;
      gchar *new_state = g_strdup (state);

      /*
       * Steal ownership of old state and create a copy for new state
       * to ensure that we own the references. State machines tend to
       * get used in re-entrant fashion.
       */

      priv->state = g_strdup (state);

      if (priv->freeze_count == 0)
        egg_state_machine_transition (self, old_state, state);

      g_free (new_state);
      g_free (old_state);
    }
}

GAction *
egg_state_machine_create_action (EggStateMachine *self,
                                 const gchar     *name)
{
  g_return_val_if_fail (EGG_IS_STATE_MACHINE (self), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  return g_object_new (EGG_TYPE_STATE_MACHINE_ACTION,
                       "state-machine", self,
                       "name", name,
                       NULL);
}

void
egg_state_machine_add_property (EggStateMachine *self,
                                const gchar     *state,
                                gpointer         object,
                                const gchar     *property,
                                const GValue    *value)
{
  EggState *state_obj;
  EggStateProperty *state_prop;

  g_return_if_fail (EGG_IS_STATE_MACHINE (self));
  g_return_if_fail (state != NULL);
  g_return_if_fail (object != NULL);
  g_return_if_fail (property != NULL);
  g_return_if_fail (G_IS_VALUE (value));

  state_obj = egg_state_machine_get_state_obj (self, state);

  state_prop = g_slice_new0 (EggStateProperty);
  state_prop->object = object;
  state_prop->property = g_strdup (property);
  g_value_init (&state_prop->value, G_VALUE_TYPE (value));
  g_value_copy (value, &state_prop->value);

  g_ptr_array_add (state_obj->properties, state_prop);
}

void
egg_state_machine_add_binding (EggStateMachine *self,
                               const gchar     *state,
                               gpointer         source_object,
                               const gchar     *source_property,
                               gpointer         target_object,
                               const gchar     *target_property,
                               GBindingFlags    flags)
{
  EggBindingSet *bindings;
  EggState *state_obj;

  g_return_if_fail (EGG_IS_STATE_MACHINE (self));
  g_return_if_fail (state != NULL);
  g_return_if_fail (source_object != NULL);
  g_return_if_fail (source_property != NULL);
  g_return_if_fail (target_object != NULL);
  g_return_if_fail (target_property != NULL);

  state_obj = egg_state_machine_get_state_obj (self, state);

  bindings = g_hash_table_lookup (state_obj->bindings, source_object);

  if (bindings == NULL)
    {
      bindings = egg_binding_set_new ();
      g_hash_table_insert (state_obj->bindings, source_object, bindings);
    }

  egg_binding_set_bind (bindings, source_property, target_object, target_property, flags);
}

void
egg_state_machine_add_style (EggStateMachine *self,
                             const gchar     *state,
                             GtkWidget       *widget,
                             const gchar     *style)
{
  EggState *state_obj;
  EggStateStyle *style_obj;

  g_return_if_fail (EGG_IS_STATE_MACHINE (self));
  g_return_if_fail (state != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (style);

  state_obj = egg_state_machine_get_state_obj (self, state);

  style_obj = g_slice_new0 (EggStateStyle);
  style_obj->name = g_strdup (style);
  style_obj->widget = widget;

  g_ptr_array_add (state_obj->styles, style_obj);
}

void
egg_state_machine_connect_object (EggStateMachine *self,
                                  const gchar     *state,
                                  gpointer         source,
                                  const gchar     *detailed_signal,
                                  GCallback        callback,
                                  gpointer         user_data,
                                  GConnectFlags    flags)
{
  EggState *state_obj;
  EggSignalGroup *signals;

  g_return_if_fail (EGG_IS_STATE_MACHINE (self));
  g_return_if_fail (state != NULL);
  g_return_if_fail (G_IS_OBJECT (source));
  g_return_if_fail (detailed_signal != NULL);
  g_return_if_fail (callback != NULL);

  state_obj = egg_state_machine_get_state_obj (self, state);

  if (!(signals = g_hash_table_lookup (state_obj->signals, source)))
    {
      signals = egg_signal_group_new (G_OBJECT_TYPE (source));
      g_hash_table_insert (state_obj->signals, source, signals);
    }

  egg_signal_group_connect_object (signals, detailed_signal, callback, user_data, flags);
}

void
egg_state_machine_freeze (EggStateMachine *self)
{
  EggStateMachinePrivate *priv = egg_state_machine_get_instance_private (self);

  g_return_if_fail (EGG_IS_STATE_MACHINE (self));
  g_return_if_fail (priv->freeze_count >= 0);

  if (++priv->freeze_count == 1)
    {
      g_assert (priv->freeze_state == NULL);
      priv->freeze_state = g_strdup (priv->state);
    }
}

void
egg_state_machine_thaw (EggStateMachine *self)
{
  EggStateMachinePrivate *priv = egg_state_machine_get_instance_private (self);

  g_return_if_fail (EGG_IS_STATE_MACHINE (self));
  g_return_if_fail (priv->freeze_count > 0);

  if (--priv->freeze_count == 0)
    {
      if (g_strcmp0 (priv->freeze_state, priv->state) != 0)
        {
          gchar *state;

          state = priv->freeze_state;
          priv->freeze_state = NULL;
          egg_state_machine_set_state (self, state);
          g_free (state);
        }
    }
}
