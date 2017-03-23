/* mi2-info-message.c
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

#define G_LOG_DOMAIN "mi2-info-message"

#include <string.h>

#include "mi2-info-message.h"
#include "mi2-util.h"

struct _Mi2InfoMessage
{
  Mi2Message  parent;
  gchar      *message;
};

enum {
  PROP_0,
  PROP_MESSAGE,
  N_PROPS
};

G_DEFINE_TYPE (Mi2InfoMessage, mi2_info_message, MI2_TYPE_MESSAGE)

static GParamSpec *properties [N_PROPS];

static void
mi2_info_message_finalize (GObject *object)
{
  Mi2InfoMessage *self = (Mi2InfoMessage *)object;

  g_clear_pointer (&self->message, g_free);

  G_OBJECT_CLASS (mi2_info_message_parent_class)->finalize (object);
}

static void
mi2_info_message_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  Mi2InfoMessage *self = MI2_INFO_MESSAGE (object);

  switch (prop_id)
    {
    case PROP_MESSAGE:
      g_value_set_string (value, mi2_info_message_get_message (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mi2_info_message_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  Mi2InfoMessage *self = MI2_INFO_MESSAGE (object);

  switch (prop_id)
    {
    case PROP_MESSAGE:
      mi2_info_message_set_message (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mi2_info_message_class_init (Mi2InfoMessageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = mi2_info_message_finalize;
  object_class->get_property = mi2_info_message_get_property;
  object_class->set_property = mi2_info_message_set_property;

  properties [PROP_MESSAGE] =
    g_param_spec_string ("message", NULL, NULL, NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
mi2_info_message_init (Mi2InfoMessage *self)
{
}

const gchar *
mi2_info_message_get_message (Mi2InfoMessage *self)
{
  g_return_val_if_fail (MI2_IS_INFO_MESSAGE (self), NULL);

  return self->message;
}

void
mi2_info_message_set_message (Mi2InfoMessage *self,
                              const gchar    *message)
{
  if (message != self->message)
    {
      g_free (self->message);
      self->message = g_strdup (message);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MESSAGE]);
    }
}

Mi2Message *
mi2_info_message_new_from_string (const gchar *line)
{
  Mi2InfoMessage *ret;

  ret = g_object_new (MI2_TYPE_INFO_MESSAGE, NULL);

  if (line != NULL && line[0] == '&')
    ret->message = mi2_util_parse_string (&line[1], NULL);

  return MI2_MESSAGE (ret);
}
