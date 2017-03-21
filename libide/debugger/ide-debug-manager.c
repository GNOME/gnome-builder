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

#include "ide-debug-manager.h"

struct _IdeDebugManager
{
  IdeObject           parent_instance;
  GSimpleActionGroup *actions;
};

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
  IdeDebugManager *self = IDE_DEBUG_MANAGER (object);

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
  IdeDebugManager *self = IDE_DEBUG_MANAGER (object);

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

gboolean
ide_debug_manager_start (IdeDebugManager  *self,
                         IdeRunner        *runner,
                         GError          **error)
{
  g_set_error (error,
               G_IO_ERROR,
               G_IO_ERROR_NOT_SUPPORTED,
               _("A suitable debugger could not be found."));

  return FALSE;
}
