/* mi2-reply-mesage.c
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

#define G_LOG_DOMAIN "mi2-reply-message"

#include "mi2-error.h"
#include "mi2-reply-message.h"
#include "mi2-util.h"

struct _Mi2ReplyMessage
{
  Mi2Message  parent_instance;
  gchar      *name;
};

enum {
  PROP_0,
  PROP_NAME,
  N_PROPS
};

G_DEFINE_TYPE (Mi2ReplyMessage, mi2_reply_mesage, MI2_TYPE_MESSAGE)

static GParamSpec *properties [N_PROPS];

static void
mi2_reply_mesage_finalize (GObject *object)
{
  Mi2ReplyMessage *self = (Mi2ReplyMessage *)object;

  g_clear_pointer (&self->name, g_free);

  G_OBJECT_CLASS (mi2_reply_mesage_parent_class)->finalize (object);
}

static void
mi2_reply_mesage_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  Mi2ReplyMessage *self = MI2_REPLY_MESSAGE (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, mi2_reply_message_get_name (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mi2_reply_mesage_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  Mi2ReplyMessage *self = MI2_REPLY_MESSAGE (object);

  switch (prop_id)
    {
    case PROP_NAME:
      mi2_reply_message_set_name (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mi2_reply_mesage_class_init (Mi2ReplyMessageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = mi2_reply_mesage_finalize;
  object_class->get_property = mi2_reply_mesage_get_property;
  object_class->set_property = mi2_reply_mesage_set_property;

  properties [PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "Name",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
mi2_reply_mesage_init (Mi2ReplyMessage *self)
{
}

/**
 * mi2_reply_message_new_from_string:
 * @line: the string to be parsed
 *
 * Returns: (transfer full): An #Mi2Message
 */
Mi2Message *
mi2_reply_message_new_from_string (const gchar *line)
{
  Mi2ReplyMessage *ret;

  ret = g_object_new (MI2_TYPE_REPLY_MESSAGE, NULL);

  if (line && *line)
    {
      ret->name = mi2_util_parse_word (&line[1], &line);

      while (line != NULL && *line != '\0')
        {
          g_autofree gchar *key = NULL;
          g_autofree gchar *value = NULL;

          if (!(key = mi2_util_parse_word (line, &line)) ||
              !(value = mi2_util_parse_string (line, &line)))
            break;

          mi2_message_set_param_string (MI2_MESSAGE (ret), key, value);
        }
    }

  return MI2_MESSAGE (ret);
}

const gchar *
mi2_reply_message_get_name (Mi2ReplyMessage *self)
{
  g_return_val_if_fail (MI2_IS_REPLY_MESSAGE (self), NULL);

  return self->name;
}

void
mi2_reply_message_set_name (Mi2ReplyMessage *self,
                            const gchar     *name)
{
  if (name != self->name)
    {
      g_free (self->name);
      self->name = g_strdup (name);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_NAME]);
    }
}

gboolean
mi2_reply_message_check_error (Mi2ReplyMessage  *self,
                               GError          **error)
{
  g_return_val_if_fail (MI2_IS_REPLY_MESSAGE (self), FALSE);

  if (g_strcmp0 (self->name, "error") == 0)
    {
      const gchar *msg = mi2_message_get_param_string (MI2_MESSAGE (self), "msg");

      if (msg == NULL || *msg == '\0')
        msg = "An unknown error occrred";

      g_set_error_literal (error,
                           MI2_ERROR,
                           MI2_ERROR_UNKNOWN_ERROR,
                           msg);

      return TRUE;
    }

  return FALSE;
}
