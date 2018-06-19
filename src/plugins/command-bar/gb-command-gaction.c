/* gb-command-gaction.c
 *
 * Copyright 2014 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "command-gaction"

#include <dazzle.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include "gb-command-gaction.h"

struct _GbCommandGaction
{
  GbCommand     parent_instance;

  GActionGroup *action_group;
  gchar        *action_name;
  GVariant     *parameters;
};

G_DEFINE_TYPE (GbCommandGaction, gb_command_gaction, GB_TYPE_COMMAND)

enum {
  PROP_0,
  PROP_ACTION_GROUP,
  PROP_ACTION_NAME,
  PROP_PARAMETERS,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static void
gb_command_gaction_set_action_group (GbCommandGaction *gaction,
                                     GActionGroup     *action_group)
{
  g_return_if_fail (GB_IS_COMMAND_GACTION (gaction));
  g_return_if_fail (G_IS_ACTION_GROUP (action_group));

  if (gaction->action_group != action_group)
    {
      g_clear_object (&gaction->action_group);
      gaction->action_group = g_object_ref (action_group);
    }
}

static void
gb_command_gaction_set_action_name (GbCommandGaction *gaction,
                                    const gchar      *action_name)
{
  g_return_if_fail (GB_IS_COMMAND_GACTION (gaction));

  if (gaction->action_name != action_name)
    {
      dzl_clear_pointer (&gaction->action_name, g_free);
      gaction->action_name = g_strdup (action_name);
    }
}

static void
gb_command_gaction_set_parameters (GbCommandGaction *gaction,
                                   GVariant         *variant)
{
  g_return_if_fail (GB_IS_COMMAND_GACTION (gaction));

  if (gaction->parameters != variant)
    {
      dzl_clear_pointer (&gaction->parameters, g_variant_unref);
      gaction->parameters = g_variant_ref (variant);
    }
}

static GbCommandResult *
gb_command_gaction_execute (GbCommand *command)
{
  GbCommandGaction *self = (GbCommandGaction *)command;

  g_assert (GB_IS_COMMAND_GACTION (self));

  if (self->action_group != NULL &&
      self->action_name != NULL &&
      g_action_group_has_action (self->action_group, self->action_name))
    g_action_group_activate_action (self->action_group, self->action_name, self->parameters);

  return NULL;
}

static void
gb_command_gaction_finalize (GObject *object)
{
  GbCommandGaction *self = GB_COMMAND_GACTION (object);

  g_clear_object (&self->action_group);
  dzl_clear_pointer (&self->action_name, g_free);
  dzl_clear_pointer (&self->parameters, g_variant_unref);

  G_OBJECT_CLASS (gb_command_gaction_parent_class)->finalize (object);
}

static void
gb_command_gaction_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GbCommandGaction *self = GB_COMMAND_GACTION (object);

  switch (prop_id)
    {
    case PROP_ACTION_GROUP:
      g_value_set_object (value, self->action_group);
      break;

    case PROP_ACTION_NAME:
      g_value_set_string (value, self->action_name);
      break;

    case PROP_PARAMETERS:
      g_value_set_variant (value, self->parameters);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_command_gaction_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GbCommandGaction *self = GB_COMMAND_GACTION (object);

  switch (prop_id)
    {
    case PROP_ACTION_GROUP:
      gb_command_gaction_set_action_group (self, g_value_get_object (value));
      break;

    case PROP_ACTION_NAME:
      gb_command_gaction_set_action_name (self, g_value_get_string (value));
      break;

    case PROP_PARAMETERS:
      gb_command_gaction_set_parameters (self, g_value_get_variant (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_command_gaction_class_init (GbCommandGactionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GbCommandClass *command_class = GB_COMMAND_CLASS (klass);

  object_class->finalize = gb_command_gaction_finalize;
  object_class->get_property = gb_command_gaction_get_property;
  object_class->set_property = gb_command_gaction_set_property;

  command_class->execute = gb_command_gaction_execute;

  properties [PROP_ACTION_GROUP] =
    g_param_spec_object ("action-group",
                         "Action Group",
                         "The GActionGroup containing the action.",
                         G_TYPE_ACTION_GROUP,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_ACTION_NAME] =
    g_param_spec_string ("action-name",
                         "Action Name",
                         "The name of the action to execute.",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_PARAMETERS] =
    g_param_spec_variant ("parameters",
                          "Parameters",
                          "The parameters for the action.",
                          G_VARIANT_TYPE_ANY,
                          NULL,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
gb_command_gaction_init (GbCommandGaction *self)
{
}
