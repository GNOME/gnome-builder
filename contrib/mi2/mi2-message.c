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
#include "mi2-error.h"
#include "mi2-event-message.h"
#include "mi2-info-message.h"
#include "mi2-reply-message.h"
#include "mi2-util.h"

typedef struct
{
  GHashTable *params;
} Mi2MessagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (Mi2Message, mi2_message, G_TYPE_OBJECT)

static GHashTable *
make_hashtable (void)
{
  return g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_variant_unref);
}

static void
mi2_message_finalize (GObject *object)
{
  Mi2Message *self = (Mi2Message *)object;
  Mi2MessagePrivate *priv = mi2_message_get_instance_private (self);

  g_clear_pointer (&priv->params, g_hash_table_unref);

  G_OBJECT_CLASS (mi2_message_parent_class)->finalize (object);
}

static void
mi2_message_class_init (Mi2MessageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = mi2_message_finalize;
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
  const gchar *begin = line;
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

    case '^':
      ret = mi2_reply_message_new_from_string (line);
      break;

    case '=':
    case '*':
      ret = mi2_event_message_new_from_string (line);
      break;

    case '-':
      ret = mi2_command_message_new_from_string (line);
      break;

    default:
      break;
    }

  if (ret == NULL)
    g_set_error (error,
                 MI2_ERROR,
                 MI2_ERROR_INVALID_DATA,
                 "Failed to parse: %s", begin);

  return ret;
}

void
mi2_message_parse_params (Mi2Message  *self,
                          const gchar *line)
{
  g_autoptr(GVariant) params = NULL;

  g_return_if_fail (MI2_IS_MESSAGE (self));
  g_return_if_fail (line != NULL);

  params = mi2_util_parse_record (line, NULL);

  if (params)
    {
      GVariantIter iter;
      const gchar *key;
      GVariant *value;

      g_variant_iter_init (&iter, params);
      while (g_variant_iter_loop (&iter, "{sv}", &key, &value))
        mi2_message_set_param (self, key, value);
    }
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

const gchar *
mi2_message_get_param_string (Mi2Message  *self,
                              const gchar *name)
{
  Mi2MessagePrivate *priv = mi2_message_get_instance_private (self);
  GVariant *variant = NULL;

  g_return_val_if_fail (MI2_IS_MESSAGE (self), NULL);

  if (priv->params != NULL)
    variant = g_hash_table_lookup (priv->params, name);

  return variant == NULL ? NULL : g_variant_get_string (variant, NULL);
}

void
mi2_message_set_param_string (Mi2Message  *self,
                              const gchar *name,
                              const gchar *value)
{
  Mi2MessagePrivate *priv = mi2_message_get_instance_private (self);

  g_return_if_fail (MI2_IS_MESSAGE (self));
  g_return_if_fail (name != NULL);

  if (priv->params == NULL)
    priv->params = make_hashtable ();

  if (value == NULL)
    g_hash_table_remove (priv->params, name);
  else
    g_hash_table_insert (priv->params,
                         g_strdup (name),
                         g_variant_ref_sink (g_variant_new_string (value)));
}

GVariant *
mi2_message_get_param (Mi2Message  *self,
                       const gchar *param)
{
  Mi2MessagePrivate *priv = mi2_message_get_instance_private (self);

  g_return_val_if_fail (MI2_IS_MESSAGE (self), NULL);
  g_return_val_if_fail (param != NULL, NULL);

  if (priv->params)
    return g_hash_table_lookup (priv->params, param);

  return NULL;
}

void
mi2_message_set_param (Mi2Message  *self,
                       const gchar *param,
                       GVariant    *variant)
{
  Mi2MessagePrivate *priv = mi2_message_get_instance_private (self);

  g_return_if_fail (MI2_IS_MESSAGE (self));
  g_return_if_fail (param != NULL);

  if (priv->params == NULL)
    priv->params = make_hashtable ();

  if (variant == NULL)
    g_hash_table_remove (priv->params, param);
  else
    g_hash_table_insert (priv->params,
                         g_strdup (param),
                         g_variant_ref_sink (variant));
}

/**
 * mi2_message_get_params:
 * @self: An #Mi2Message
 *
 * Gets the keys for params that are stored in the message, free the
 * result with g_free() as ownership of the fields is owned by the
 * #Mi2Message.
 *
 * Returns: (transfer container): A %NULL-terminated array of param names.
 */
const gchar **
mi2_message_get_params (Mi2Message *self)
{
  Mi2MessagePrivate *priv = mi2_message_get_instance_private (self);

  g_return_val_if_fail (MI2_IS_MESSAGE (self), NULL);

  if (priv->params != NULL)
    return (const gchar **)g_hash_table_get_keys_as_array (priv->params, NULL);

  return (const gchar **)g_new0 (gchar *, 1);
}
