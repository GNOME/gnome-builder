/* mi2-message.c
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

#define G_LOG_DOMAIN "mi2-message"

#include "mi2-message.h"

#include "mi2-command-message.h"
#include "mi2-console-message.h"
#include "mi2-event-message.h"
#include "mi2-info-message.h"

typedef struct
{
  gpointer dummy;
} Mi2MessagePrivate;

enum {
  PROP_0,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (Mi2Message, mi2_message, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
mi2_message_finalize (GObject *object)
{
  Mi2Message *self = (Mi2Message *)object;
  Mi2MessagePrivate *priv = mi2_message_get_instance_private (self);

  G_OBJECT_CLASS (mi2_message_parent_class)->finalize (object);
}

static void
mi2_message_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  Mi2Message *self = MI2_MESSAGE (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mi2_message_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  Mi2Message *self = MI2_MESSAGE (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mi2_message_class_init (Mi2MessageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = mi2_message_finalize;
  object_class->get_property = mi2_message_get_property;
  object_class->set_property = mi2_message_set_property;
}

static void
mi2_message_init (Mi2Message *self)
{
}

/**
 * mi2_message_parse:
 * @line: the line to parse
 * @error: a location for a #GError or %NULL
 *
 * Parses @line into #Mi2Message.
 *
 * Returns: (transfer full): An #Mi2Message if successful; otherwise %NULL
 *   and @error is set.
 */
Mi2Message *
mi2_message_parse (const gchar  *line,
                   gsize         len,
                   GError      **error)
{
  Mi2Message *ret = NULL;

  g_return_val_if_fail (line != NULL, NULL);
  g_return_val_if_fail (len > 0, NULL);

  switch (line[0])
    {
    case '~':
      ret = mi2_console_message_new_from_string (line);
      break;

    case '&':
      ret = mi2_info_message_new_from_string (line);
      break;

    case '=':
    case '*':
    case '^':
      ret = mi2_event_message_new_from_string (line);
      break;

    case '-':
      ret = mi2_command_message_new_from_string (line);
      break;

    default:
      break;
    }

  return ret;
}

/**
 * mi2_message_serialize:
 * @self: An #Mi2Message
 *
 * Serialize the message to be sent to the peer.
 *
 * Returns: (transfer full): A #GBytes.
 */
GBytes *
mi2_message_serialize (Mi2Message *self)
{
  g_return_val_if_fail (MI2_IS_MESSAGE (self), NULL);

  return MI2_MESSAGE_GET_CLASS (self)->serialize (self);
}
