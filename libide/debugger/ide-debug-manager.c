/* ide-debug-manager.c
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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
 */

#define G_LOG_DOMAIN "ide-debug-manager"

#include <egg-binding-group.h>
#include <egg-signal-group.h>
#include <glib/gi18n.h>

#include "ide-debug.h"

#include "debugger/ide-debug-manager.h"
#include "debugger/ide-debugger.h"
#include "plugins/ide-extension-util.h"
#include "runner/ide-runner.h"

struct _IdeDebugManager
{
  IdeObject           parent_instance;

  GSimpleActionGroup *actions;
  IdeDebugger        *debugger;
  EggBindingGroup    *debugger_bindings;
  EggSignalGroup     *debugger_signals;
  IdeRunner          *runner;

  guint               active : 1;
};

typedef struct
{
  IdeDebugger *debugger;
  IdeRunner   *runner;
  gint         priority;
} DebuggerLookup;

enum {
  PROP_0,
  PROP_ACTIVE,
  PROP_DEBUGGER,
  N_PROPS
};

enum {
  BREAKPOINT_ADDED,
  BREAKPOINT_REMOVED,
  N_SIGNALS
};

static GParamSpec *properties [N_PROPS];

static void
ide_debug_manager_set_active (IdeDebugManager *self,
                              gboolean         active)
{
  g_assert (IDE_IS_DEBUG_MANAGER (self));

  if (active != self->active)
    {
      self->active = active;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ACTIVE]);
    }
}

static gboolean
ide_debug_manager_has_action (GActionGroup *group,
                              const gchar  *action_name)
{
  IdeDebugManager *self = (IdeDebugManager *)group;

  g_assert (IDE_IS_DEBUG_MANAGER (self));

  return g_action_group_has_action (G_ACTION_GROUP (self->actions), action_name);
}

static gchar **
ide_debug_manager_list_actions (GActionGroup *group)
{
  IdeDebugManager *self = (IdeDebugManager *)group;

  g_assert (IDE_IS_DEBUG_MANAGER (self));

  return g_action_group_list_actions (G_ACTION_GROUP (self->actions));
}

static gboolean
ide_debug_manager_query_action (GActionGroup        *group,
                                const gchar         *action_name,
                                gboolean            *enabled,
                                const GVariantType **parameter_type,
                                const GVariantType **state_type,
                                GVariant           **state_hint,
                                GVariant           **state)
{
  IdeDebugManager *self = (IdeDebugManager *)group;

  g_assert (IDE_IS_DEBUG_MANAGER (self));

  return g_action_group_query_action (G_ACTION_GROUP (self->actions),
                                      action_name,
                                      enabled,
                                      parameter_type,
                                      state_type,
                                      state_hint,
                                      state);
}

static void
ide_debug_manager_activate_action (GActionGroup *group,
                                   const gchar  *action_name,
                                   GVariant     *parameter)
{
  IdeDebugManager *self = (IdeDebugManager *)group;

  g_assert (IDE_IS_DEBUG_MANAGER (self));

  g_action_group_activate_action (G_ACTION_GROUP (self->actions), action_name, parameter);
}

static void
action_group_iface_init (GActionGroupInterface *iface)
{
  iface->has_action = ide_debug_manager_has_action;
  iface->list_actions = ide_debug_manager_list_actions;
  iface->query_action = ide_debug_manager_query_action;
  iface->activate_action = ide_debug_manager_activate_action;
}

G_DEFINE_TYPE_EXTENDED (IdeDebugManager, ide_debug_manager, IDE_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, action_group_iface_init))

static void
ide_debug_manager_action_step_in (GSimpleAction *action,
                                  GVariant      *param,
                                  gpointer       user_data)
{
  IdeDebugManager *self = user_data;

  g_assert (IDE_IS_DEBUG_MANAGER (self));
  g_assert (G_IS_SIMPLE_ACTION (action));

  if (self->debugger != NULL)
    ide_debugger_run (self->debugger, IDE_DEBUGGER_RUN_STEP_IN);
}

static void
ide_debug_manager_action_step_over (GSimpleAction *action,
                                    GVariant      *param,
                                    gpointer       user_data)
{
  IdeDebugManager *self = user_data;

  g_assert (IDE_IS_DEBUG_MANAGER (self));
  g_assert (G_IS_SIMPLE_ACTION (action));

  if (self->debugger != NULL)
    ide_debugger_run (self->debugger, IDE_DEBUGGER_RUN_STEP_OVER);
}

static void
ide_debug_manager_action_continue (GSimpleAction *action,
                                   GVariant      *param,
                                   gpointer       user_data)
{
  IdeDebugManager *self = user_data;

  g_assert (IDE_IS_DEBUG_MANAGER (self));
  g_assert (G_IS_SIMPLE_ACTION (action));

  if (self->debugger != NULL)
    ide_debugger_run (self->debugger, IDE_DEBUGGER_RUN_CONTINUE);
}

static void
ide_debug_manager_debugger_stopped (IdeDebugManager       *self,
                                    IdeDebuggerStopReason  reason,
                                    IdeSourceLocation     *location,
                                    IdeDebugger           *debugger)
{
  IDE_ENTRY;

  g_assert (IDE_IS_DEBUG_MANAGER (self));
  g_assert (IDE_IS_DEBUGGER (debugger));

  switch (reason)
    {
    case IDE_DEBUGGER_STOP_EXITED_FROM_SIGNAL:
    case IDE_DEBUGGER_STOP_EXITED_NORMALLY:
      /* Cleanup any lingering debugger process */
      if (self->runner != NULL)
        ide_runner_force_quit (self->runner);
      break;

    case IDE_DEBUGGER_STOP_UNDEFINED:
    case IDE_DEBUGGER_STOP_BREAKPOINT:
    case IDE_DEBUGGER_STOP_WATCHPOINT:
    case IDE_DEBUGGER_STOP_SIGNALED:
    default:
      break;
    }

  IDE_EXIT;
}

static void
ide_debug_manager_action_propagate_enabled (IdeDebugManager *self,
                                            GParamSpec      *pspec,
                                            GSimpleAction   *action)
{
  g_assert (IDE_IS_DEBUG_MANAGER (self));
  g_assert (pspec != NULL);
  g_assert (G_IS_SIMPLE_ACTION (action));

  g_action_group_action_enabled_changed (G_ACTION_GROUP (self),
                                         g_action_get_name (G_ACTION (action)),
                                         g_action_get_enabled (G_ACTION (action)));
}

static GActionEntry debugger_action_entries[] = {
  { "step-in",   ide_debug_manager_action_step_in },
  { "step-over", ide_debug_manager_action_step_over },
  { "continue",  ide_debug_manager_action_continue },
};

static void
ide_debug_manager_finalize (GObject *object)
{
  IdeDebugManager *self = (IdeDebugManager *)object;

  g_clear_object (&self->actions);
  g_clear_object (&self->debugger);
  g_clear_object (&self->debugger_bindings);
  g_clear_object (&self->debugger_signals);
  g_clear_object (&self->runner);

  G_OBJECT_CLASS (ide_debug_manager_parent_class)->finalize (object);
}

static void
ide_debug_manager_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  IdeDebugManager *self = IDE_DEBUG_MANAGER (object);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, self->active);
      break;

    case PROP_DEBUGGER:
      g_value_set_object (value, self->debugger);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debug_manager_class_init (IdeDebugManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_debug_manager_finalize;
  object_class->get_property = ide_debug_manager_get_property;

  /**
   * IdeDebugManager:active:
   *
   * If the debugger is active.
   *
   * This can be used to determine if the controls should be made visible
   * in the workbench.
   */
  properties [PROP_ACTIVE] =
    g_param_spec_boolean ("active",
                          "Active",
                          "If the debugger is running",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_DEBUGGER] =
    g_param_spec_object ("debugger",
                         "Debugger",
                         "The current debugger being used",
                         IDE_TYPE_DEBUGGER,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_debug_manager_init (IdeDebugManager *self)
{
  self->actions = g_simple_action_group_new ();

  g_action_map_add_action_entries (G_ACTION_MAP (self->actions),
                                   debugger_action_entries,
                                   G_N_ELEMENTS (debugger_action_entries),
                                   self);

  for (guint i = 0; i < G_N_ELEMENTS (debugger_action_entries); i++)
    {
      const gchar *name = debugger_action_entries[i].name;
      GAction *action = g_action_map_lookup_action (G_ACTION_MAP (self->actions), name);

      g_signal_connect_object (action,
                               "notify::enabled",
                               G_CALLBACK (ide_debug_manager_action_propagate_enabled),
                               self,
                               G_CONNECT_SWAPPED);
    }

#define BIND_PROPERTY_TO_ACTION(name, action_name)                                   \
  egg_binding_group_bind (self->debugger_bindings,                                   \
                          name,                                                      \
                          g_action_map_lookup_action (G_ACTION_MAP (self->actions),  \
                                                      action_name),                  \
                          "enabled",                                                 \
                          G_BINDING_SYNC_CREATE)

  self->debugger_bindings = egg_binding_group_new ();

  BIND_PROPERTY_TO_ACTION ("can-continue", "continue");
  BIND_PROPERTY_TO_ACTION ("can-step-in", "step-in");
  BIND_PROPERTY_TO_ACTION ("can-step-over", "step-over");

#undef BIND_PROPERTY_TO_ACTION

  self->debugger_signals = egg_signal_group_new (IDE_TYPE_DEBUGGER);

  egg_signal_group_connect_object (self->debugger_signals,
                                   "stopped",
                                   G_CALLBACK (ide_debug_manager_debugger_stopped),
                                   self,
                                   G_CONNECT_SWAPPED);
}

static void
debugger_lookup (PeasExtensionSet *set,
                 PeasPluginInfo   *plugin_info,
                 PeasExtension    *exten,
                 gpointer          user_data)
{
  DebuggerLookup *lookup = user_data;
  IdeDebugger *debugger = (IdeDebugger *)exten;
  gint priority = G_MAXINT;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_DEBUGGER (debugger));
  g_assert (lookup != NULL);

  if (ide_debugger_supports_runner (debugger, lookup->runner, &priority))
    {
      if (lookup->debugger == NULL || priority < lookup->priority)
        {
          g_set_object (&lookup->debugger, debugger);
          lookup->priority = priority;
        }
    }
}

IdeDebugger *
ide_debug_manager_find_debugger (IdeDebugManager *self,
                                 IdeRunner       *runner)
{
  g_autoptr(PeasExtensionSet) set = NULL;
  IdeContext *context;
  DebuggerLookup lookup;

  g_return_val_if_fail (IDE_IS_DEBUG_MANAGER (self), NULL);
  g_return_val_if_fail (IDE_IS_RUNNER (runner), NULL);

  context = ide_object_get_context (IDE_OBJECT (runner));

  lookup.debugger = NULL;
  lookup.runner = runner;
  lookup.priority = G_MAXINT;

  set = ide_extension_set_new (peas_engine_get_default (),
                               IDE_TYPE_DEBUGGER,
                               "context", context,
                               NULL);

  peas_extension_set_foreach (set, debugger_lookup, &lookup);

  return lookup.debugger;
}

static void
ide_debug_manager_runner_exited (IdeDebugManager *self,
                                 IdeRunner       *runner)
{
  g_assert (IDE_IS_DEBUG_MANAGER (self));
  g_assert (IDE_IS_RUNNER (runner));

  g_clear_object (&self->runner);

  ide_debug_manager_set_active (self, FALSE);
}

gboolean
ide_debug_manager_start (IdeDebugManager  *self,
                         IdeRunner        *runner,
                         GError          **error)
{
  g_autoptr(IdeDebugger) debugger = NULL;
  gboolean ret = FALSE;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_DEBUG_MANAGER (self), FALSE);
  g_return_val_if_fail (IDE_IS_RUNNER (runner), FALSE);

  debugger = ide_debug_manager_find_debugger (self, runner);

  if (debugger == NULL)
    {
      ide_runner_set_failed (runner, TRUE);
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_SUPPORTED,
                   _("A suitable debugger could not be found."));
      IDE_GOTO (failure);
    }

  ide_debugger_prepare (debugger, runner);

  g_signal_connect_object (runner,
                           "exited",
                           G_CALLBACK (ide_debug_manager_runner_exited),
                           self,
                           G_CONNECT_SWAPPED);

  self->runner = g_object_ref (runner);
  self->debugger = g_steal_pointer (&debugger);

  egg_binding_group_set_source (self->debugger_bindings, self->debugger);
  egg_signal_group_set_target (self->debugger_signals, self->debugger);

  ide_debug_manager_set_active (self, TRUE);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DEBUGGER]);

  ret = TRUE;

failure:
  IDE_RETURN (ret);
}

void
ide_debug_manager_stop (IdeDebugManager *self)
{
  g_return_if_fail (IDE_IS_DEBUG_MANAGER (self));

  egg_binding_group_set_source (self->debugger_bindings, NULL);
  egg_signal_group_set_target (self->debugger_signals, NULL);
  g_clear_object (&self->debugger);
}

gboolean
ide_debug_manager_get_active (IdeDebugManager *self)
{
  g_return_val_if_fail (IDE_IS_DEBUG_MANAGER (self), FALSE);

  return self->active;
}
