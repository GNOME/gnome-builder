/* ide-action-muxer.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "ide-action-muxer"

#include "config.h"

#include "ide-action-muxer.h"
#include "ide-macros.h"

struct _IdeActionMuxer
{
  GObject    parent_instance;
  GPtrArray *action_groups;
  guint      n_recurse;
};

typedef struct
{
  IdeActionMuxer *backptr;
  char           *prefix;
  GActionGroup   *action_group;
  GSignalGroup   *action_group_signals;
} PrefixedActionGroup;

static void action_group_iface_init (GActionGroupInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeActionMuxer, ide_action_muxer, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, action_group_iface_init))

static void
prefixed_action_group_finalize (gpointer data)
{
  PrefixedActionGroup *pag = data;

  g_assert (pag->backptr == NULL);

  g_clear_object (&pag->action_group_signals);
  g_clear_object (&pag->action_group);
  g_clear_pointer (&pag->prefix, g_free);
}

static void
prefixed_action_group_unref (PrefixedActionGroup *pag)
{
  g_rc_box_release_full (pag, prefixed_action_group_finalize);
}

static void
prefixed_action_group_drop (PrefixedActionGroup *pag)
{
  g_signal_group_set_target (pag->action_group_signals, NULL);
  pag->backptr = NULL;
  prefixed_action_group_unref (pag);
}

static PrefixedActionGroup *
prefixed_action_group_ref (PrefixedActionGroup *pag)
{
  return g_rc_box_acquire (pag);
}

static void
ide_action_muxer_dispose (GObject *object)
{
  IdeActionMuxer *self = (IdeActionMuxer *)object;

  if (self->action_groups->len > 0)
    g_ptr_array_remove_range (self->action_groups, 0, self->action_groups->len);

  G_OBJECT_CLASS (ide_action_muxer_parent_class)->finalize (object);
}

static void
ide_action_muxer_finalize (GObject *object)
{
  IdeActionMuxer *self = (IdeActionMuxer *)object;

  g_clear_pointer (&self->action_groups, g_ptr_array_unref);

  G_OBJECT_CLASS (ide_action_muxer_parent_class)->finalize (object);
}

static void
ide_action_muxer_class_init (IdeActionMuxerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_action_muxer_dispose;
  object_class->finalize = ide_action_muxer_finalize;
}

static void
ide_action_muxer_init (IdeActionMuxer *self)
{
  self->action_groups = g_ptr_array_new_with_free_func ((GDestroyNotify)prefixed_action_group_drop);
}

IdeActionMuxer *
ide_action_muxer_new (void)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);

  return g_object_new (IDE_TYPE_ACTION_MUXER, NULL);
}

/**
 * ide_action_muxer_list_groups:
 * @self: a #IdeActionMuxer
 *
 * Gets a list of group names in the muxer.
 *
 * Returns: (transfer full) (array zero-terminated=1) (element-type utf8):
 *   an array containing the names of groups within the muxer
 */
char **
ide_action_muxer_list_groups (IdeActionMuxer *self)
{
  GArray *ar;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_ACTION_MUXER (self), NULL);

  ar = g_array_new (TRUE, FALSE, sizeof (char *));

  for (guint i = 0; i < self->action_groups->len; i++)
    {
      const PrefixedActionGroup *pag = g_ptr_array_index (self->action_groups, i);
      char *prefix = g_strdup (pag->prefix);

      g_assert (prefix != NULL);
      g_assert (g_str_has_suffix (prefix, "."));

      *strrchr (prefix, '.') = 0;

      g_array_append_val (ar, prefix);
    }

  return (char **)g_array_free (ar, FALSE);
}

static void
ide_action_muxer_action_group_action_added_cb (GActionGroup        *action_group,
                                               const char          *action_name,
                                               PrefixedActionGroup *pag)
{
  g_autofree char *full_name = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (G_IS_ACTION_GROUP (action_group));
  g_assert (action_name != NULL);
  g_assert (pag != NULL);
  g_assert (pag->backptr != NULL);
  g_assert (IDE_IS_ACTION_MUXER (pag->backptr));

  full_name = g_strconcat (pag->prefix, action_name, NULL);
  g_action_group_action_added (G_ACTION_GROUP (pag->backptr), full_name);
}

static void
ide_action_muxer_action_group_action_removed_cb (GActionGroup        *action_group,
                                                 const char          *action_name,
                                                 PrefixedActionGroup *pag)
{
  g_autofree char *full_name = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (G_IS_ACTION_GROUP (action_group));
  g_assert (action_name != NULL);
  g_assert (pag != NULL);
  g_assert (pag->backptr != NULL);
  g_assert (IDE_IS_ACTION_MUXER (pag->backptr));

  full_name = g_strconcat (pag->prefix, action_name, NULL);
  g_action_group_action_removed (G_ACTION_GROUP (pag->backptr), full_name);
}

static void
ide_action_muxer_action_group_action_enabled_changed_cb (GActionGroup        *action_group,
                                                         const char          *action_name,
                                                         gboolean             enabled,
                                                         PrefixedActionGroup *pag)
{
  g_autofree char *full_name = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (G_IS_ACTION_GROUP (action_group));
  g_assert (action_name != NULL);
  g_assert (pag != NULL);
  g_assert (pag->backptr != NULL);
  g_assert (IDE_IS_ACTION_MUXER (pag->backptr));

  full_name = g_strconcat (pag->prefix, action_name, NULL);
  g_action_group_action_enabled_changed (G_ACTION_GROUP (pag->backptr), full_name, enabled);
}

static void
ide_action_muxer_action_group_action_state_changed_cb (GActionGroup        *action_group,
                                                       const char          *action_name,
                                                       GVariant            *value,
                                                       PrefixedActionGroup *pag)
{
  g_autofree char *full_name = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (G_IS_ACTION_GROUP (action_group));
  g_assert (action_name != NULL);
  g_assert (pag != NULL);
  g_assert (pag->backptr != NULL);
  g_assert (IDE_IS_ACTION_MUXER (pag->backptr));

  full_name = g_strconcat (pag->prefix, action_name, NULL);
  g_action_group_action_state_changed (G_ACTION_GROUP (pag->backptr), full_name, value);
}

void
ide_action_muxer_insert_action_group (IdeActionMuxer *self,
                                      const char     *prefix,
                                      GActionGroup   *action_group)
{
  g_autofree char *prefix_dot = NULL;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_ACTION_MUXER (self));
  g_return_if_fail (self->n_recurse == 0);
  g_return_if_fail (prefix != NULL);
  g_return_if_fail (!action_group || G_IS_ACTION_GROUP (action_group));

  /* Protect against recursion via signal emission. We don't want anything to
   * mess with our GArray while we are actively processing actions. To do so is
   * invalid API use.
   */
  self->n_recurse++;

  /* Precalculate with a dot suffix so we can simplify lookups */
  prefix_dot = g_strconcat (prefix, ".", NULL);

  /* Find our matching action group by prefix, and then notify it has been
   * removed from our known actions.
   */
  for (guint i = 0; i < self->action_groups->len; i++)
    {
      const PrefixedActionGroup *pag = g_ptr_array_index (self->action_groups, i);
      g_auto(GStrv) action_names = NULL;

      g_assert (pag->prefix != NULL);
      g_assert (G_IS_ACTION_GROUP (pag->action_group));

      if (g_strcmp0 (pag->prefix, prefix_dot) != 0)
        continue;

      /* Clear signal group first, since it has weak pointers */
      g_signal_group_set_target (pag->action_group_signals, NULL);

      /* Retrieve list of all the action names so we can drop references */
      action_names = g_action_group_list_actions (pag->action_group);

      /* Remove this entry from our knowledge, clear pag because it now
       * points to potentially invalid memory.
       */
      pag = NULL;
      g_ptr_array_remove_index_fast (self->action_groups, i);

      /* Notify any actiongroup listeners of removed actions */
      for (guint j = 0; action_names[j]; j++)
        {
          g_autofree char *action_name = g_strconcat (prefix_dot, action_names[j], NULL);
          g_action_group_action_removed (G_ACTION_GROUP (self), action_name);
        }

      break;
    }

  /* If we got a new action group to replace it, setup tracking of the
   * action group and then notify of all the current actions.
   */
  if (action_group != NULL)
    {
      g_auto(GStrv) action_names = g_action_group_list_actions (action_group);
      PrefixedActionGroup *new_pag = g_rc_box_new0 (PrefixedActionGroup);

      new_pag->backptr = self;
      new_pag->prefix = g_strdup (prefix_dot);
      new_pag->action_group = g_object_ref (action_group);
      new_pag->action_group_signals = g_signal_group_new (G_TYPE_ACTION_GROUP);
      g_ptr_array_add (self->action_groups, new_pag);

      g_signal_group_connect_data (new_pag->action_group_signals,
                                   "action-added",
                                   G_CALLBACK (ide_action_muxer_action_group_action_added_cb),
                                   prefixed_action_group_ref (new_pag),
                                   (GClosureNotify)prefixed_action_group_unref,
                                   0);
      g_signal_group_connect_data (new_pag->action_group_signals,
                                   "action-removed",
                                   G_CALLBACK (ide_action_muxer_action_group_action_removed_cb),
                                   prefixed_action_group_ref (new_pag),
                                   (GClosureNotify)prefixed_action_group_unref,
                                   0);
      g_signal_group_connect_data (new_pag->action_group_signals,
                                   "action-enabled-changed",
                                   G_CALLBACK (ide_action_muxer_action_group_action_enabled_changed_cb),
                                   prefixed_action_group_ref (new_pag),
                                   (GClosureNotify)prefixed_action_group_unref,
                                   0);
      g_signal_group_connect_data (new_pag->action_group_signals,
                                   "action-state-changed",
                                   G_CALLBACK (ide_action_muxer_action_group_action_state_changed_cb),
                                   prefixed_action_group_ref (new_pag),
                                   (GClosureNotify)prefixed_action_group_unref,
                                   0);

      g_signal_group_set_target (new_pag->action_group_signals, action_group);

      for (guint j = 0; action_names[j]; j++)
        {
          g_autofree char *action_name = g_strconcat (prefix_dot, action_names[j], NULL);
          g_action_group_action_added (G_ACTION_GROUP (self), action_name);
        }
    }

  self->n_recurse--;
}

void
ide_action_muxer_remove_action_group (IdeActionMuxer *self,
                                      const char     *prefix)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_ACTION_MUXER (self));
  g_return_if_fail (prefix != NULL);

  ide_action_muxer_insert_action_group (self, prefix, NULL);
}

static gboolean
ide_action_muxer_has_action (GActionGroup *group,
                             const char   *action_name)
{
  IdeActionMuxer *self = IDE_ACTION_MUXER (group);

  for (guint i = 0; i < self->action_groups->len; i++)
    {
      const PrefixedActionGroup *pag = g_ptr_array_index (self->action_groups, i);

      if (g_str_has_prefix (action_name, pag->prefix))
        {
          const char *short_name = action_name + strlen (pag->prefix);

          if (g_action_group_has_action (pag->action_group, short_name))
            return TRUE;
        }
    }

  return FALSE;
}

static char **
ide_action_muxer_list_actions (GActionGroup *group)
{
  IdeActionMuxer *self = IDE_ACTION_MUXER (group);
  GArray *ar = g_array_new (TRUE, FALSE, sizeof (char *));

  for (guint i = 0; i < self->action_groups->len; i++)
    {
      const PrefixedActionGroup *pag = g_ptr_array_index (self->action_groups, i);
      g_auto(GStrv) action_names = g_action_group_list_actions (pag->action_group);

      for (guint j = 0; action_names[j]; j++)
        {
          char *full_action_name = g_strconcat (pag->prefix, action_names[j], NULL);
          g_array_append_val (ar, full_action_name);
        }
    }

  return (char **)g_array_free (ar, FALSE);
}

static gboolean
ide_action_muxer_get_action_enabled (GActionGroup *group,
                                     const char   *action_name)
{
  IdeActionMuxer *self = IDE_ACTION_MUXER (group);

  for (guint i = 0; i < self->action_groups->len; i++)
    {
      const PrefixedActionGroup *pag = g_ptr_array_index (self->action_groups, i);

      if (g_str_has_prefix (action_name, pag->prefix))
        {
          const char *short_name = action_name + strlen (pag->prefix);

          if (g_action_group_has_action (pag->action_group, short_name))
            return g_action_group_get_action_enabled (pag->action_group, short_name);
        }
    }

  return FALSE;
}

static GVariant *
ide_action_muxer_get_action_state (GActionGroup *group,
                                   const char   *action_name)
{
  IdeActionMuxer *self = IDE_ACTION_MUXER (group);

  for (guint i = 0; i < self->action_groups->len; i++)
    {
      const PrefixedActionGroup *pag = g_ptr_array_index (self->action_groups, i);

      if (g_str_has_prefix (action_name, pag->prefix))
        {
          const char *short_name = action_name + strlen (pag->prefix);

          if (g_action_group_has_action (pag->action_group, short_name))
            return g_action_group_get_action_state (pag->action_group, short_name);
        }
    }

  return NULL;
}

static GVariant *
ide_action_muxer_get_action_state_hint (GActionGroup *group,
                                        const char   *action_name)
{
  IdeActionMuxer *self = IDE_ACTION_MUXER (group);

  for (guint i = 0; i < self->action_groups->len; i++)
    {
      const PrefixedActionGroup *pag = g_ptr_array_index (self->action_groups, i);

      if (g_str_has_prefix (action_name, pag->prefix))
        {
          const char *short_name = action_name + strlen (pag->prefix);

          if (g_action_group_has_action (pag->action_group, short_name))
            return g_action_group_get_action_state_hint (pag->action_group, short_name);
        }
    }

  return NULL;
}

static void
ide_action_muxer_change_action_state (GActionGroup *group,
                                      const char   *action_name,
                                      GVariant     *value)
{
  IdeActionMuxer *self = IDE_ACTION_MUXER (group);

  for (guint i = 0; i < self->action_groups->len; i++)
    {
      const PrefixedActionGroup *pag = g_ptr_array_index (self->action_groups, i);

      if (g_str_has_prefix (action_name, pag->prefix))
        {
          const char *short_name = action_name + strlen (pag->prefix);

          if (g_action_group_has_action (pag->action_group, short_name))
            {
              g_action_group_change_action_state (pag->action_group, short_name, value);
              break;
            }
        }
    }
}

static const GVariantType *
ide_action_muxer_get_action_state_type (GActionGroup *group,
                                        const char   *action_name)
{
  IdeActionMuxer *self = IDE_ACTION_MUXER (group);

  for (guint i = 0; i < self->action_groups->len; i++)
    {
      const PrefixedActionGroup *pag = g_ptr_array_index (self->action_groups, i);

      if (g_str_has_prefix (action_name, pag->prefix))
        {
          const char *short_name = action_name + strlen (pag->prefix);

          if (g_action_group_has_action (pag->action_group, short_name))
            return g_action_group_get_action_state_type (pag->action_group, short_name);
        }
    }

  return NULL;
}

static void
ide_action_muxer_activate_action (GActionGroup *group,
                                  const char   *action_name,
                                  GVariant     *parameter)
{
  IdeActionMuxer *self = IDE_ACTION_MUXER (group);

  for (guint i = 0; i < self->action_groups->len; i++)
    {
      const PrefixedActionGroup *pag = g_ptr_array_index (self->action_groups, i);

      if (g_str_has_prefix (action_name, pag->prefix))
        {
          const char *short_name = action_name + strlen (pag->prefix);

          if (g_action_group_has_action (pag->action_group, short_name))
            {
              g_action_group_activate_action (pag->action_group, short_name, parameter);
              break;
            }
        }
    }
}

static const GVariantType *
ide_action_muxer_get_action_parameter_type (GActionGroup *group,
                                            const char   *action_name)
{
  IdeActionMuxer *self = IDE_ACTION_MUXER (group);

  for (guint i = 0; i < self->action_groups->len; i++)
    {
      const PrefixedActionGroup *pag = g_ptr_array_index (self->action_groups, i);

      if (g_str_has_prefix (action_name, pag->prefix))
        {
          const char *short_name = action_name + strlen (pag->prefix);

          if (g_action_group_has_action (pag->action_group, short_name))
            return g_action_group_get_action_parameter_type (pag->action_group, short_name);
        }
    }

  return NULL;
}

static void
action_group_iface_init (GActionGroupInterface *iface)
{
  iface->has_action = ide_action_muxer_has_action;
  iface->list_actions = ide_action_muxer_list_actions;
  iface->get_action_parameter_type = ide_action_muxer_get_action_parameter_type;
  iface->get_action_enabled = ide_action_muxer_get_action_enabled;
  iface->get_action_state = ide_action_muxer_get_action_state;
  iface->get_action_state_hint = ide_action_muxer_get_action_state_hint;
  iface->get_action_state_type = ide_action_muxer_get_action_state_type;
  iface->change_action_state = ide_action_muxer_change_action_state;
  iface->activate_action = ide_action_muxer_activate_action;
}
