/* mi2-event-mesage.c
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

#define G_LOG_DOMAIN "mi2-event-message"

#include "mi2-event-message.h"
#include "mi2-util.h"

struct _Mi2EventMessage
{
  Mi2Message  parent_instance;
  gchar      *name;
};

enum {
  PROP_0,
  PROP_NAME,
  N_PROPS
};

G_DEFINE_TYPE (Mi2EventMessage, mi2_event_mesage, MI2_TYPE_MESSAGE)

static GParamSpec *properties [N_PROPS];

static void
mi2_event_mesage_finalize (GObject *object)
{
  Mi2EventMessage *self = (Mi2EventMessage *)object;

  g_clear_pointer (&self->name, g_free);

  G_OBJECT_CLASS (mi2_event_mesage_parent_class)->finalize (object);
}

static void
mi2_event_mesage_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  Mi2EventMessage *self = MI2_EVENT_MESSAGE (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, mi2_event_message_get_name (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mi2_event_mesage_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  Mi2EventMessage *self = MI2_EVENT_MESSAGE (object);

  switch (prop_id)
    {
    case PROP_NAME:
      mi2_event_message_set_name (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mi2_event_mesage_class_init (Mi2EventMessageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = mi2_event_mesage_finalize;
  object_class->get_property = mi2_event_mesage_get_property;
  object_class->set_property = mi2_event_mesage_set_property;

  properties [PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "Name",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
mi2_event_mesage_init (Mi2EventMessage *self)
{
}

/**
 * mi2_event_message_new_from_string:
 * @line: the string to be parsed
 *
 * Returns: (transfer full): An #Mi2Message
 */
Mi2Message *
mi2_event_message_new_from_string (const gchar *line)
{
  Mi2EventMessage *ret;

  ret = g_object_new (MI2_TYPE_EVENT_MESSAGE, NULL);

  if (line && *line)
    {
      ret->name = mi2_util_parse_word (&line[1], &line);
      mi2_message_parse_params (MI2_MESSAGE (ret), line);
    }

  return MI2_MESSAGE (ret);
}

const gchar *
mi2_event_message_get_name (Mi2EventMessage *self)
{
  g_return_val_if_fail (MI2_IS_EVENT_MESSAGE (self), NULL);

  return self->name;
}

void
mi2_event_message_set_name (Mi2EventMessage *self,
                            const gchar     *name)
{
  if (name != self->name)
    {
      g_free (self->name);
      self->name = g_strdup (name);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_NAME]);
    }
}
