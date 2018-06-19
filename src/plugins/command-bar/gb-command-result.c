/* gb-command-result.c
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

#include <dazzle.h>
#include <glib/gi18n.h>

#include "gb-command-result.h"

struct _GbCommandResult
{
  GObject  parent_instance;

  gchar   *command_text;
  gchar   *result_text;
  guint    is_error : 1;
  guint    is_running : 1;
};

G_DEFINE_TYPE (GbCommandResult, gb_command_result, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_COMMAND_TEXT,
  PROP_IS_ERROR,
  PROP_IS_RUNNING,
  PROP_RESULT_TEXT,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

GbCommandResult *
gb_command_result_new (void)
{
  return g_object_new (GB_TYPE_COMMAND_RESULT, NULL);
}

const gchar *
gb_command_result_get_command_text (GbCommandResult *result)
{
  g_return_val_if_fail (GB_IS_COMMAND_RESULT (result), NULL);

  return result->command_text;
}

void
gb_command_result_set_command_text (GbCommandResult *result,
                                    const gchar     *command_text)
{
  g_return_if_fail (GB_IS_COMMAND_RESULT (result));

  if (result->command_text != command_text)
    {
      g_free (result->command_text);
      result->command_text = g_strdup (command_text);
      g_object_notify_by_pspec (G_OBJECT (result),
                                properties [PROP_COMMAND_TEXT]);
    }
}

const gchar *
gb_command_result_get_result_text (GbCommandResult *result)
{
  g_return_val_if_fail (GB_IS_COMMAND_RESULT (result), NULL);

  return result->result_text;
}

void
gb_command_result_set_result_text (GbCommandResult *result,
                                   const gchar     *result_text)
{
  g_return_if_fail (GB_IS_COMMAND_RESULT (result));

  if (result->result_text != result_text)
    {
      g_free (result->result_text);
      result->result_text = g_strdup (result_text);
      g_object_notify_by_pspec (G_OBJECT (result),
                                properties [PROP_RESULT_TEXT]);
    }
}

gboolean
gb_command_result_get_is_running (GbCommandResult *result)
{
  g_return_val_if_fail (GB_IS_COMMAND_RESULT (result), FALSE);

  return result->is_running;
}

void
gb_command_result_set_is_running (GbCommandResult *result,
                                  gboolean         is_running)
{
  g_return_if_fail (GB_IS_COMMAND_RESULT (result));

  if (result->is_running != is_running)
    {
      result->is_running = !!is_running;
      g_object_notify_by_pspec (G_OBJECT (result),
                                properties [PROP_IS_RUNNING]);
    }
}

gboolean
gb_command_result_get_is_error (GbCommandResult *result)
{
  g_return_val_if_fail (GB_IS_COMMAND_RESULT (result), FALSE);

  return result->is_error;
}

void
gb_command_result_set_is_error (GbCommandResult *result,
                                gboolean         is_error)
{
  g_return_if_fail (GB_IS_COMMAND_RESULT (result));

  if (result->is_error != is_error)
    {
      result->is_error = !!is_error;
      g_object_notify_by_pspec (G_OBJECT (result),
                                properties [PROP_IS_ERROR]);
    }
}

static void
gb_command_result_finalize (GObject *object)
{
  GbCommandResult *self = GB_COMMAND_RESULT (object);

  dzl_clear_pointer (&self->command_text, g_free);
  dzl_clear_pointer (&self->result_text, g_free);

  G_OBJECT_CLASS (gb_command_result_parent_class)->finalize (object);
}

static void
gb_command_result_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GbCommandResult *self = GB_COMMAND_RESULT (object);

  switch (prop_id)
    {
    case PROP_COMMAND_TEXT:
      g_value_set_string (value, gb_command_result_get_command_text (self));
      break;

    case PROP_IS_ERROR:
      g_value_set_boolean (value, gb_command_result_get_is_error (self));
      break;

    case PROP_IS_RUNNING:
      g_value_set_boolean (value, gb_command_result_get_is_running (self));
      break;

    case PROP_RESULT_TEXT:
      g_value_set_string (value, gb_command_result_get_result_text (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_command_result_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GbCommandResult *self = GB_COMMAND_RESULT (object);

  switch (prop_id)
    {
    case PROP_COMMAND_TEXT:
      gb_command_result_set_command_text (self, g_value_get_string (value));
      break;

    case PROP_IS_ERROR:
      gb_command_result_set_is_error (self, g_value_get_boolean (value));
      break;

    case PROP_IS_RUNNING:
      gb_command_result_set_is_running (self, g_value_get_boolean (value));
      break;

    case PROP_RESULT_TEXT:
      gb_command_result_set_result_text (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_command_result_class_init (GbCommandResultClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gb_command_result_finalize;
  object_class->get_property = gb_command_result_get_property;
  object_class->set_property = gb_command_result_set_property;

  properties [PROP_COMMAND_TEXT] =
    g_param_spec_string ("command-text",
                         "Command Text",
                         "The command text if any.",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_IS_ERROR] =
    g_param_spec_boolean ("is-error",
                          "Is Error",
                          "If the result is an error.",
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_STATIC_STRINGS));

  properties [PROP_IS_RUNNING] =
    g_param_spec_boolean ("is-running",
                          "Is Running",
                          "If the command is still running.",
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_STATIC_STRINGS));

  properties [PROP_RESULT_TEXT] =
    g_param_spec_string ("result-text",
                         "Result Text",
                         "The result text if any.",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
gb_command_result_init (GbCommandResult *self)
{
}
