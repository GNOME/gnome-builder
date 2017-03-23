/* mi2-console-message.c
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

#define G_LOG_DOMAIN "mi2-console-message"

#include <string.h>

#include "mi2-console-message.h"
#include "mi2-util.h"

struct _Mi2ConsoleMessage
{
  Mi2Message  parent;
  gchar      *message;
};

enum {
  PROP_0,
  PROP_MESSAGE,
  N_PROPS
};

G_DEFINE_TYPE (Mi2ConsoleMessage, mi2_console_message, MI2_TYPE_MESSAGE)

static GParamSpec *properties [N_PROPS];

static GBytes *
mi2_console_message_serialize (Mi2Message *message)
{
  Mi2ConsoleMessage *self = (Mi2ConsoleMessage *)message;
  g_autofree gchar *escaped = NULL;
  g_autofree gchar *str = NULL;

  g_assert (MI2_IS_CONSOLE_MESSAGE (message));

  escaped = g_strescape (self->message ? self->message : "", "");
  str = g_strdup_printf ("~\"%s\"\n", escaped);

  return g_bytes_new_take (g_steal_pointer (&str), strlen (str));
}

static void
mi2_console_message_finalize (GObject *object)
{
  Mi2ConsoleMessage *self = (Mi2ConsoleMessage *)object;

  g_clear_pointer (&self->message, g_free);

  G_OBJECT_CLASS (mi2_console_message_parent_class)->finalize (object);
}

static void
mi2_console_message_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  Mi2ConsoleMessage *self = MI2_CONSOLE_MESSAGE (object);

  switch (prop_id)
    {
    case PROP_MESSAGE:
      g_value_set_string (value, mi2_console_message_get_message (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mi2_console_message_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  Mi2ConsoleMessage *self = MI2_CONSOLE_MESSAGE (object);

  switch (prop_id)
    {
    case PROP_MESSAGE:
      mi2_console_message_set_message (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mi2_console_message_class_init (Mi2ConsoleMessageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  Mi2MessageClass *message_class = MI2_MESSAGE_CLASS (klass);

  object_class->finalize = mi2_console_message_finalize;
  object_class->get_property = mi2_console_message_get_property;
  object_class->set_property = mi2_console_message_set_property;

  message_class->serialize = mi2_console_message_serialize;

  properties [PROP_MESSAGE] =
    g_param_spec_string ("message", NULL, NULL, NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
mi2_console_message_init (Mi2ConsoleMessage *self)
{
}

const gchar *
mi2_console_message_get_message (Mi2ConsoleMessage *self)
{
  g_return_val_if_fail (MI2_IS_CONSOLE_MESSAGE (self), NULL);

  return self->message;
}

void
mi2_console_message_set_message (Mi2ConsoleMessage *self,
                                 const gchar       *message)
{
  if (message != self->message)
    {
      g_free (self->message);
      self->message = g_strdup (message);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MESSAGE]);
    }
}

Mi2Message *
mi2_console_message_new_from_string (const gchar *line)
{
  Mi2ConsoleMessage *ret;

  ret = g_object_new (MI2_TYPE_CONSOLE_MESSAGE, NULL);

  if (line != NULL && line[0] == '~')
    ret->message = mi2_util_parse_string (&line[1], NULL);

  return MI2_MESSAGE (ret);
}
