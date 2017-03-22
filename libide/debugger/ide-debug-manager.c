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
};

typedef struct
{
  IdeDebugger *debugger;
  IdeRunner   *runner;
  gint         priority;
} DebuggerLookup;

enum {
  PROP_0,
  N_PROPS
};

enum {
  BREAKPOINT_ADDED,
  BREAKPOING_REMOVED,
  N_SIGNALS
};

static GParamSpec *properties [N_PROPS];

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
ide_debug_manager_finalize (GObject *object)
{
  IdeDebugManager *self = (IdeDebugManager *)object;

  g_clear_object (&self->actions);

  G_OBJECT_CLASS (ide_debug_manager_parent_class)->finalize (object);
}

static void
ide_debug_manager_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debug_manager_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  switch (prop_id)
    {
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
  object_class->set_property = ide_debug_manager_set_property;
}

static void
ide_debug_manager_init (IdeDebugManager *self)
{
  self->actions = g_simple_action_group_new ();
}

static void
debugger_lookup (PeasExtensionSet *set,
                 PeasPluginInfo   *plugin_info,
                 PeasExtension    *exten,
                 gpointer          user_data)
{
  DebuggerLookup *lookup = user_data;
  IdeDebugger *debugger = (IdeDebugger *)debugger;
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

  ret = TRUE;

failure:
  IDE_RETURN (ret);
}

void
ide_debug_manager_stop (IdeDebugManager *self)
{
  g_return_if_fail (IDE_IS_DEBUG_MANAGER (self));
}
