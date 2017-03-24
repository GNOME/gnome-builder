/* mi2-command-message.c
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

#define G_LOG_DOMAIN "mi2-command-message"

#include <string.h>

#include "mi2-command-message.h"
#include "mi2-util.h"

struct _Mi2CommandMessage
{
  Mi2Message  parent;
  gchar      *command;
};

enum {
  PROP_0,
  PROP_COMMAND,
  N_PROPS
};

G_DEFINE_TYPE (Mi2CommandMessage, mi2_command_message, MI2_TYPE_MESSAGE)

static GParamSpec *properties [N_PROPS];

static GBytes *
mi2_command_message_serialize (Mi2Message *message)
{
  Mi2CommandMessage *self = (Mi2CommandMessage *)message;
  GString *str;

  g_assert (MI2_IS_COMMAND_MESSAGE (self));

  if (!self->command || !*self->command)
    return NULL;

  str = g_string_new (NULL);

  if (*self->command == '-')
    g_string_append (str, self->command);
  else
    g_string_append_printf (str, "-%s", self->command);

  /* TODO: Params? */

  g_string_append_c (str, '\n');

  return g_string_free_to_bytes (str);
}

static void
mi2_command_message_finalize (GObject *object)
{
  Mi2CommandMessage *self = (Mi2CommandMessage *)object;

  g_clear_pointer (&self->command, g_free);

  G_OBJECT_CLASS (mi2_command_message_parent_class)->finalize (object);
}

static void
mi2_command_message_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  Mi2CommandMessage *self = MI2_COMMAND_MESSAGE (object);

  switch (prop_id)
    {
    case PROP_COMMAND:
      g_value_set_string (value, mi2_command_message_get_command (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mi2_command_message_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  Mi2CommandMessage *self = MI2_COMMAND_MESSAGE (object);

  switch (prop_id)
    {
    case PROP_COMMAND:
      mi2_command_message_set_command (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mi2_command_message_class_init (Mi2CommandMessageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  Mi2MessageClass *message_class = MI2_MESSAGE_CLASS (klass);

  object_class->finalize = mi2_command_message_finalize;
  object_class->get_property = mi2_command_message_get_property;
  object_class->set_property = mi2_command_message_set_property;

  message_class->serialize = mi2_command_message_serialize;

  properties [PROP_COMMAND] =
    g_param_spec_string ("command", NULL, NULL, NULL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
mi2_command_message_init (Mi2CommandMessage *self)
{
}

const gchar *
mi2_command_message_get_command (Mi2CommandMessage *self)
{
  g_return_val_if_fail (MI2_IS_COMMAND_MESSAGE (self), NULL);

  return self->command;
}

void
mi2_command_message_set_command (Mi2CommandMessage *self,
                                 const gchar       *command)
{
  if (command != self->command)
    {
      g_free (self->command);
      self->command = g_strdup (command);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_COMMAND]);
    }
}

Mi2Message *
mi2_command_message_new_from_string (const gchar *line)
{
  Mi2CommandMessage *ret;

  ret = g_object_new (MI2_TYPE_COMMAND_MESSAGE, NULL);
  ret->command = g_strstrip (g_strdup (&line[1]));

  return MI2_MESSAGE (ret);
}
