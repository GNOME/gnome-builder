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

#include <glib/gi18n.h>

#include "egg-binding-set.h"
#include "egg-signal-group.h"
#include "egg-state-machine.h"

typedef struct
{
  gchar *state;

  /*
   * Containers for lazily bound signals and bindings.
   *
   * Each is a GHashTable indexed by state name, containing another GHashTable indexed by
   * source object.
   */
  GHashTable *binding_sets_by_state;
  GHashTable *signal_groups_by_state;

  /*
   * Container for actions which should have sensitivity mutated during state transitions.
   *
   * GHashTable of GPtrArray of ActionState.
   */
  GHashTable *actions_by_state;

  gsize       sequence;
} EggStateMachinePrivate;

typedef struct
{
  GSimpleAction *action;
  guint          invert_enabled : 1;
} ActionState;

G_DEFINE_TYPE_WITH_PRIVATE (EggStateMachine, egg_state_machine, G_TYPE_OBJECT)
G_DEFINE_QUARK (EggStateMachineError, egg_state_machine_error)

enum {
  PROP_0,
  PROP_STATE,
  LAST_PROP
};

enum {
  TRANSITION,
  LAST_SIGNAL
};

static GParamSpec *gParamSpecs [LAST_PROP];
static guint gSignals [LAST_SIGNAL];

static void
action_state_free (gpointer data)
{
  ActionState *state = data;

  g_clear_object (&state->action);
  g_slice_free (ActionState, state);
}

static gboolean
egg_state_transition_accumulator (GSignalInvocationHint *hint,
                                  GValue                *return_value,
                                  const GValue          *handler_return,
                                  gpointer               data)
{
  EggStateTransition ret;

  ret = g_value_get_enum (handler_return);

  if (ret == EGG_STATE_TRANSITION_INVALID)
    {
      g_value_set_enum (return_value, ret);
      return FALSE;
    }

  return TRUE;
}

const gchar *
egg_state_machine_get_state (EggStateMachine *self)
{
  EggStateMachinePrivate *priv = egg_state_machine_get_instance_private (self);

  g_return_val_if_fail (EGG_IS_STATE_MACHINE (self), NULL);

  return priv->state;
}

static void
egg_state_machine_do_transition (EggStateMachine *self,
                                 const gchar     *new_state)
{
  EggStateMachinePrivate *priv = egg_state_machine_get_instance_private (self);
  GHashTableIter iter;
  const gchar *key;
  GPtrArray *action_states;
  GHashTable *value;
  gsize i;

  g_assert (EGG_IS_STATE_MACHINE (self));
  g_assert (new_state != NULL);

  priv->sequence++;

  g_free (priv->state);
  priv->state = g_strdup (new_state);

  g_hash_table_iter_init (&iter, priv->signal_groups_by_state);
  while (g_hash_table_iter_next (&iter, (gpointer *)&key, (gpointer *)&value))
    {
      GHashTable *signal_groups = value;
      GHashTableIter groups_iter;
      EggSignalGroup *signal_group;
      gpointer instance;
      gboolean enabled = (g_strcmp0 (key, new_state) == 0);

      g_hash_table_iter_init (&groups_iter, signal_groups);

      while (g_hash_table_iter_next (&groups_iter, &instance, (gpointer *)&signal_group))
        {
          g_assert (G_IS_OBJECT (instance));
          g_assert (EGG_IS_SIGNAL_GROUP (signal_group));

          egg_signal_group_set_target (signal_group, enabled ? instance : NULL);
        }
    }

  g_hash_table_iter_init (&iter, priv->binding_sets_by_state);
  while (g_hash_table_iter_next (&iter, (gpointer *)&key, (gpointer *)&value))
    {
      GHashTable *binding_sets = value;
      GHashTableIter groups_iter;
      EggBindingSet *binding_set;
      gpointer instance;
      gboolean enabled = (g_strcmp0 (key, new_state) == 0);

      g_hash_table_iter_init (&groups_iter, binding_sets);

      while (g_hash_table_iter_next (&groups_iter, &instance, (gpointer *)&binding_set))
        {
          g_assert (G_IS_OBJECT (instance));
          g_assert (EGG_IS_BINDING_SET (binding_set));

          egg_binding_set_set_source (binding_set, enabled ? instance : NULL);
        }
    }

  /* apply GSimpleAction:enabled to non-matching states */
  g_hash_table_iter_init (&iter, priv->actions_by_state);
  while (g_hash_table_iter_next (&iter, (gpointer *)&key, (gpointer *)&action_states))
    {
      if (g_strcmp0 (key, priv->state) == 0)
        continue;

      for (i = 0; i < action_states->len; i++)
        {
          ActionState *action_state;

          action_state = g_ptr_array_index (action_states, i);
          g_simple_action_set_enabled (action_state->action, action_state->invert_enabled);
        }
    }

  /* apply GSimpleAction:enabled to matching state */
  action_states = g_hash_table_lookup (priv->actions_by_state, priv->state);
  if (action_states != NULL)
    {
      for (i = 0; i < action_states->len; i++)
        {
          ActionState *action_state;

          action_state = g_ptr_array_index (action_states, i);
          g_simple_action_set_enabled (action_state->action, !action_state->invert_enabled);
        }
    }
}

/**
 * egg_state_machine_transition:
 * @self: A #EggStateMachine.
 * @new_state: The name of the new state.
 * @error: A location for a #GError, or %NULL.
 *
 * Attempts to change the state of the state machine to @new_state.
 *
 * This operation can fail, in which %EGG_STATE_TRANSITION_INVALID will be
 * returned and @error will be set.
 *
 * Upon success, %EGG_STATE_TRANSITION_SUCCESS is returned.
 *
 * Returns: An #EggStateTransition.
 */
EggStateTransition
egg_state_machine_transition (EggStateMachine  *self,
                              const gchar      *new_state,
                              GError          **error)
{
  EggStateMachinePrivate *priv = egg_state_machine_get_instance_private (self);
  g_autofree gchar *old_state = NULL;
  g_autofree gchar *new_state_copy = NULL;
  EggStateTransition ret = EGG_STATE_TRANSITION_IGNORED;
  g_autoptr(GError) local_error = NULL;
  gsize sequence;

  g_return_val_if_fail (EGG_IS_STATE_MACHINE (self), EGG_STATE_TRANSITION_INVALID);
  g_return_val_if_fail (new_state != NULL, EGG_STATE_TRANSITION_INVALID);
  g_return_val_if_fail (error == NULL || *error == NULL, EGG_STATE_TRANSITION_INVALID);

  if (g_strcmp0 (new_state, priv->state) == 0)
    return EGG_STATE_TRANSITION_SUCCESS;

  /* Be careful with reentrancy. */

  old_state = g_strdup (priv->state);
  new_state_copy = g_strdup (new_state);
  sequence = priv->sequence;

  g_signal_emit (self, gSignals [TRANSITION], 0, old_state, new_state, &local_error, &ret);

  if (ret == EGG_STATE_TRANSITION_INVALID)
    {
      if (local_error == NULL)
        local_error = g_error_new_literal (EGG_STATE_MACHINE_ERROR,
                                           EGG_STATE_MACHINE_ERROR_INVALID_TRANSITION,
                                           "Unknown error during state transition.");
      g_propagate_error (error, local_error);
      local_error = NULL;
      return ret;
    }

  if (sequence == priv->sequence)
    {
      egg_state_machine_do_transition (self, new_state);
      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_STATE]);
    }

  return EGG_STATE_TRANSITION_SUCCESS;
}

static EggStateTransition
egg_state_machine_real_transition (EggStateMachine  *self,
                                   const gchar      *old_state,
                                   const gchar      *new_state,
                                   GError          **error)
{
  return EGG_STATE_TRANSITION_IGNORED;
}

static void
egg_state_machine_finalize (GObject *object)
{
  EggStateMachine *self = (EggStateMachine *)object;
  EggStateMachinePrivate *priv = egg_state_machine_get_instance_private (self);

  g_clear_pointer (&priv->state, g_free);
  g_clear_pointer (&priv->binding_sets_by_state, g_hash_table_unref);
  g_clear_pointer (&priv->signal_groups_by_state, g_hash_table_unref);
  g_clear_pointer (&priv->actions_by_state, g_hash_table_unref);

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
  EggStateMachinePrivate *priv = egg_state_machine_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_STATE:
      priv->state = g_value_dup_string (value);
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

  klass->transition = egg_state_machine_real_transition;

  gParamSpecs [PROP_STATE] =
    g_param_spec_string ("state",
                         _("State"),
                         _("The current state of the machine."),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);

  /**
   * EggStateMachine::transition:
   * @self: An #EggStateMachine.
   * @old_state: The current state.
   * @new_state: The new state.
   * @error: (ctype GError**): A location for a #GError, or %NULL.
   *
   * Determines if the transition is allowed.
   *
   * If the state transition is invalid, @error should be set to a new #GError.
   *
   * Returns: %TRUE if the state transition is acceptable.
   */
  gSignals [TRANSITION] =
    g_signal_new ("transition",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (EggStateMachineClass, transition),
                  egg_state_transition_accumulator, NULL,
                  NULL,
                  EGG_TYPE_STATE_TRANSITION,
                  3,
                  G_TYPE_STRING,
                  G_TYPE_STRING,
                  G_TYPE_POINTER);
}

static void
egg_state_machine_init (EggStateMachine *self)
{
  EggStateMachinePrivate *priv = egg_state_machine_get_instance_private (self);

  priv->binding_sets_by_state =
    g_hash_table_new_full (g_str_hash,
                           g_str_equal,
                           g_free,
                           (GDestroyNotify)g_hash_table_destroy);

  priv->signal_groups_by_state =
    g_hash_table_new_full (g_str_hash,
                           g_str_equal,
                           g_free,
                           (GDestroyNotify)g_hash_table_destroy);

  priv->actions_by_state =
    g_hash_table_new_full (g_str_hash,
                           g_str_equal,
                           g_free,
                           (GDestroyNotify)g_ptr_array_unref);
}

static void
egg_state_machine__connect_object_weak_notify (gpointer  data,
                                               GObject  *where_object_was)
{
  EggStateMachine *self = data;
  EggStateMachinePrivate *priv = egg_state_machine_get_instance_private (self);
  GHashTableIter iter;
  const gchar *key;
  GHashTable *value;

  g_assert (EGG_IS_STATE_MACHINE (self));
  g_assert (where_object_was != NULL);

  g_hash_table_iter_init (&iter, priv->signal_groups_by_state);
  while (g_hash_table_iter_next (&iter, (gpointer *)&key, (gpointer *)&value))
    {
      GHashTable *signal_groups = value;

      g_hash_table_remove (signal_groups, where_object_was);
    }
}

void
egg_state_machine_connect_object (EggStateMachine *self,
                                  const gchar     *state,
                                  gpointer         instance,
                                  const gchar     *detailed_signal,
                                  GCallback        callback,
                                  gpointer         user_data,
                                  GConnectFlags    flags)
{
  EggStateMachinePrivate *priv = egg_state_machine_get_instance_private (self);
  GHashTable *signal_groups;
  EggSignalGroup *signal_group;
  gboolean created = FALSE;

  g_return_if_fail (EGG_IS_STATE_MACHINE (self));
  g_return_if_fail (state != NULL);
  g_return_if_fail (G_IS_OBJECT (instance));
  g_return_if_fail (detailed_signal != NULL);
  g_return_if_fail (g_signal_parse_name (detailed_signal,
                                         G_TYPE_FROM_INSTANCE (instance),
                                         NULL, NULL, FALSE) != 0);
  g_return_if_fail (callback != NULL);

  signal_groups = g_hash_table_lookup (priv->signal_groups_by_state, state);

  if (signal_groups == NULL)
    {
      signal_groups = g_hash_table_new_full (g_direct_hash,
                                             g_direct_equal,
                                             NULL,
                                             g_object_unref);
      g_hash_table_insert (priv->signal_groups_by_state, g_strdup (state), signal_groups);
    }

  g_assert (signal_groups != NULL);

  signal_group = g_hash_table_lookup (signal_groups, instance);

  if (signal_group == NULL)
    {
      created = TRUE;
      signal_group = egg_signal_group_new (G_TYPE_FROM_INSTANCE (instance));
      g_hash_table_insert (signal_groups, instance, signal_group);
      g_object_weak_ref (instance,
                         (GWeakNotify)egg_state_machine__connect_object_weak_notify,
                         self);
    }

  egg_signal_group_connect_object (signal_group, detailed_signal, callback, user_data, flags);

  if ((created == TRUE) && (g_strcmp0 (state, priv->state) == 0))
    egg_signal_group_set_target (signal_group, instance);
}

static void
egg_state_machine__bind_source_weak_notify (gpointer  data,
                                            GObject  *where_object_was)
{
  EggStateMachine *self = data;
  EggStateMachinePrivate *priv = egg_state_machine_get_instance_private (self);
  GHashTableIter iter;
  const gchar *key;
  GHashTable *value;

  g_assert (EGG_IS_STATE_MACHINE (self));
  g_assert (where_object_was != NULL);

  g_hash_table_iter_init (&iter, priv->binding_sets_by_state);
  while (g_hash_table_iter_next (&iter, (gpointer *)&key, (gpointer *)&value))
    {
      GHashTable *binding_sets = value;

      g_hash_table_remove (binding_sets, where_object_was);
    }
}

void
egg_state_machine_bind (EggStateMachine *self,
                        const gchar     *state,
                        gpointer         source,
                        const gchar     *source_property,
                        gpointer         target,
                        const gchar     *target_property,
                        GBindingFlags    flags)
{
  EggStateMachinePrivate *priv = egg_state_machine_get_instance_private (self);
  GHashTable *binding_sets;
  EggBindingSet *binding_set;
  gboolean created = FALSE;

  g_return_if_fail (EGG_IS_STATE_MACHINE (self));
  g_return_if_fail (state != NULL);
  g_return_if_fail (G_IS_OBJECT (source));
  g_return_if_fail (source_property != NULL);
  g_return_if_fail (g_object_class_find_property (G_OBJECT_GET_CLASS (source),
                                                  source_property) != NULL);
  g_return_if_fail (G_IS_OBJECT (target));
  g_return_if_fail (target_property != NULL);
  g_return_if_fail (g_object_class_find_property (G_OBJECT_GET_CLASS (target),
                                                  target_property) != NULL);

  /* Use G_BINDING_SYNC_CREATE as we lazily connect them. */
  flags |= G_BINDING_SYNC_CREATE;

  binding_sets = g_hash_table_lookup (priv->binding_sets_by_state, state);

  if (binding_sets == NULL)
    {
      binding_sets = g_hash_table_new_full (g_direct_hash,
                                            g_direct_equal,
                                            NULL,
                                            g_object_unref);
      g_hash_table_insert (priv->binding_sets_by_state, g_strdup (state), binding_sets);
    }

  g_assert (binding_sets != NULL);

  binding_set = g_hash_table_lookup (binding_sets, source);

  if (binding_set == NULL)
    {
      created = TRUE;
      binding_set = egg_binding_set_new ();
      g_hash_table_insert (binding_sets, source, binding_set);
      g_object_weak_ref (source,
                         (GWeakNotify)egg_state_machine__bind_source_weak_notify,
                         self);
    }

  egg_binding_set_bind (binding_set,
                        source_property,
                        target,
                        target_property,
                        flags);

  if ((created == TRUE) && (g_strcmp0 (state, priv->state) == 0))
    egg_binding_set_set_source (binding_set, source);
}

void
egg_state_machine_add_action (EggStateMachine *self,
                              const gchar     *state,
                              GSimpleAction   *action,
                              gboolean         invert_enabled)
{
  EggStateMachinePrivate *priv = egg_state_machine_get_instance_private (self);
  ActionState *action_state;
  GPtrArray *actions;
  gboolean enabled;

  g_return_if_fail (EGG_IS_STATE_MACHINE (self));
  g_return_if_fail (state != NULL);
  g_return_if_fail (G_IS_SIMPLE_ACTION (action));

  action_state = g_slice_new0 (ActionState);
  action_state->action = g_object_ref (action);
  action_state->invert_enabled = invert_enabled;

  actions = g_hash_table_lookup (priv->actions_by_state, state);

  if (actions == NULL)
    {
      actions = g_ptr_array_new_with_free_func (action_state_free);
      g_hash_table_insert (priv->actions_by_state, g_strdup (state), actions);
    }

  g_ptr_array_add (actions, action_state);

  enabled = (g_strcmp0 (state, priv->state) == 0);
  if (invert_enabled)
    enabled = !enabled;

  g_simple_action_set_enabled (action, enabled);
}

EggStateMachine *
egg_state_machine_new (void)
{
  return g_object_new (EGG_TYPE_STATE_MACHINE, NULL);
}

GType
egg_state_machine_error_get_type (void)
{
  static gsize type_id;

  if (g_once_init_enter (&type_id))
    {
      static const GEnumValue values[] = {
        { EGG_STATE_MACHINE_ERROR_INVALID_TRANSITION,
          "EGG_STATE_MACHINE_ERROR_INVALID_TRANSITION",
          "invalid-transition" },
        { 0 }
      };
      gsize _type_id;

      _type_id = g_enum_register_static ("EggStateMachineError", values);
      g_once_init_leave (&type_id, _type_id);
    }

  return type_id;
}

GType
egg_state_transition_get_type (void)
{
  static gsize type_id;

  if (g_once_init_enter (&type_id))
    {
      static const GEnumValue values[] = {
        { EGG_STATE_TRANSITION_IGNORED, "EGG_STATE_TRANSITION_IGNORED", "ignored" },
        { EGG_STATE_TRANSITION_INVALID, "EGG_STATE_TRANSITION_INVALID", "invalid" },
        { EGG_STATE_TRANSITION_SUCCESS, "EGG_STATE_TRANSITION_SUCCESS", "success" },
        { 0 }
      };
      gsize _type_id;

      _type_id = g_enum_register_static ("EggStateTransition", values);
      g_once_init_leave (&type_id, _type_id);
    }

  return type_id;
}
