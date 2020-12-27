/* ide-debugger-actions.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-debugger-actions"

#include "config.h"

#include "ide-debugger-private.h"

typedef struct _IdeDebuggerActionEntry IdeDebuggerActionEntry;

typedef void (*IdeDebuggerActionHandler) (IdeDebugger                  *debugger,
                                          const IdeDebuggerActionEntry *entry,
                                          GVariant                     *param);

struct _IdeDebuggerActionEntry
{
  const gchar              *action_name;
  IdeDebuggerActionHandler  handler;
  gint                      movement;
  gint                      running_state;
};

enum {
  RUNNING_STARTED     = 1,
  RUNNING_NOT_STARTED = 1 << 1,
  RUNNING_ACTIVE      = 1 << 2,
  RUNNING_NOT_ACTIVE  = 1 << 3,
};

static gboolean
check_running_state (IdeDebugger *self,
                     gint         state)
{
  if (state & RUNNING_STARTED)
    {
      if (!_ide_debugger_get_has_started (self))
        return FALSE;
    }

  if (state & RUNNING_NOT_STARTED)
    {
      if (_ide_debugger_get_has_started (self))
        return FALSE;
    }

  if (state & RUNNING_ACTIVE)
    {
      if (!ide_debugger_get_is_running (self))
        return FALSE;
    }

  if (state & RUNNING_NOT_ACTIVE)
    {
      if (ide_debugger_get_is_running (self))
        return FALSE;
    }

  return TRUE;
}

static void
ide_debugger_actions_movement (IdeDebugger                  *self,
                               const IdeDebuggerActionEntry *entry,
                               GVariant                     *param)
{
  g_assert (IDE_IS_DEBUGGER (self));
  g_assert (entry != NULL);

  ide_debugger_move_async (self, entry->movement, NULL, NULL, NULL);
}

static void
ide_debugger_actions_stop (IdeDebugger                  *self,
                           const IdeDebuggerActionEntry *entry,
                           GVariant                     *param)
{
  g_assert (IDE_IS_DEBUGGER (self));
  g_assert (entry != NULL);

  ide_debugger_interrupt_async (self, NULL, NULL, NULL, NULL);
}

static void
ide_debugger_actions_clear_breakpoints (IdeDebugger                  *self,
                                        const IdeDebuggerActionEntry *entry,
                                        GVariant                     *param)
{
  g_autoptr (GPtrArray) breakpoints = NULL;
  GListModel *breakpoint_list_model;
  guint n_elements;

  g_assert (IDE_IS_DEBUGGER (self));
  g_assert (entry != NULL);

  breakpoint_list_model = ide_debugger_get_breakpoints (self);
  n_elements = g_list_model_get_n_items (breakpoint_list_model);
  g_debug ("Number of breakpoints: %d", n_elements);

  breakpoints = g_ptr_array_new_with_free_func (g_object_unref);

  for (guint i = 0; i < n_elements; i++)
    {
      g_ptr_array_add (breakpoints, g_list_model_get_item (breakpoint_list_model, i));
    }

  for (guint i = 0; i < n_elements; i++)
    {
      ide_debugger_remove_breakpoint_async (self, g_ptr_array_index (breakpoints, i),
                                            NULL, NULL, NULL);
    }
}

static IdeDebuggerActionEntry action_info[] = {
  { "start",                ide_debugger_actions_movement,          IDE_DEBUGGER_MOVEMENT_START,     RUNNING_NOT_STARTED },
  { "stop",                 ide_debugger_actions_stop,              -1,                              RUNNING_STARTED | RUNNING_ACTIVE },
  { "continue",             ide_debugger_actions_movement,          IDE_DEBUGGER_MOVEMENT_CONTINUE,  RUNNING_STARTED | RUNNING_NOT_ACTIVE },
  { "step-in",              ide_debugger_actions_movement,          IDE_DEBUGGER_MOVEMENT_STEP_IN,   RUNNING_STARTED | RUNNING_NOT_ACTIVE },
  { "step-over",            ide_debugger_actions_movement,          IDE_DEBUGGER_MOVEMENT_STEP_OVER, RUNNING_STARTED | RUNNING_NOT_ACTIVE },
  { "finish",               ide_debugger_actions_movement,          IDE_DEBUGGER_MOVEMENT_FINISH,    RUNNING_STARTED | RUNNING_NOT_ACTIVE },
  { "clear-breakpoints",    ide_debugger_actions_clear_breakpoints, -1,                              RUNNING_STARTED | RUNNING_NOT_ACTIVE },
};

static gboolean
ide_debugger_get_action_enabled (GActionGroup *group,
                                 const gchar  *action_name)
{
  IdeDebugger *self = IDE_DEBUGGER (group);

  for (guint i = 0; i < G_N_ELEMENTS (action_info); i++)
    {
      const IdeDebuggerActionEntry *entry = &action_info[i];
      if (g_strcmp0 (entry->action_name, action_name) == 0)
        return check_running_state (self, entry->running_state);
    }

  return FALSE;
}

void
_ide_debugger_update_actions (IdeDebugger *self)
{
  g_assert (IDE_IS_DEBUGGER (self));

  for (guint i = 0; i < G_N_ELEMENTS (action_info); i++)
    {
      const IdeDebuggerActionEntry *entry = &action_info[i];
      gboolean enabled;

      enabled = ide_debugger_get_action_enabled (G_ACTION_GROUP (self), entry->action_name);
      g_action_group_action_enabled_changed (G_ACTION_GROUP (self), entry->action_name, enabled);
    }
}

static gboolean
ide_debugger_has_action (GActionGroup *group,
                         const gchar  *action_name)
{
  g_assert (IDE_IS_DEBUGGER (group));
  g_assert (action_name != NULL);

  for (guint i = 0; i < G_N_ELEMENTS (action_info); i++)
    {
      const IdeDebuggerActionEntry *entry = &action_info[i];

      if (g_strcmp0 (action_name, entry->action_name) == 0)
        return TRUE;
    }

  return FALSE;
}

static gchar **
ide_debugger_list_actions (GActionGroup *group)
{
  GPtrArray *ar = g_ptr_array_new ();

  for (guint i = 0; i < G_N_ELEMENTS (action_info); i++)
    g_ptr_array_add (ar, g_strdup (action_info[i].action_name));
  g_ptr_array_add (ar, NULL);

  return (gchar **)g_ptr_array_free (ar, FALSE);
}

static gpointer
null_return_func (void)
{
  return NULL;
}

static void
ide_debugger_activate_action (GActionGroup *group,
                              const gchar  *action_name,
                              GVariant     *parameter)
{
  IdeDebugger *self = IDE_DEBUGGER (group);

  g_assert (IDE_IS_DEBUGGER (self));
  g_assert (action_name != NULL);

  for (guint i = 0; i < G_N_ELEMENTS (action_info); i++)
    {
      const IdeDebuggerActionEntry *entry = &action_info[i];

      if (g_strcmp0 (entry->action_name, action_name) == 0)
        {
          entry->handler (self, entry, parameter);
          break;
        }
    }
}

void
_ide_debugger_class_init_actions (GActionGroupInterface *iface)
{
  iface->has_action = ide_debugger_has_action;
  iface->list_actions = ide_debugger_list_actions;
  iface->get_action_enabled = ide_debugger_get_action_enabled;
  iface->activate_action = ide_debugger_activate_action;
  iface->get_action_parameter_type = (gpointer)null_return_func;
  iface->get_action_state_type = (gpointer)null_return_func;
  iface->get_action_state_hint = (gpointer)null_return_func;
  iface->get_action_state = (gpointer)null_return_func;
}
