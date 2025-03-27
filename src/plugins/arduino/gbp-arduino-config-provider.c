/*
 * gbp-arduino-config-provider.c
 *
 * Copyright 2025 Nokse <nokse@posteo.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#define G_LOG_DOMAIN "ide-arduino-config-provider"

#include "config.h"

#include <string.h>
#include <yaml.h>

#include <libide-core.h>
#include <libide-foundry.h>
#include <libide-threading.h>

#include "gbp-arduino-profile.h"
#include "gbp-arduino-platform.h"
#include "gbp-arduino-config-provider.h"

G_DEFINE_AUTOPTR_CLEANUP_FUNC (yaml_emitter_t, yaml_emitter_delete);
G_DEFINE_AUTOPTR_CLEANUP_FUNC (yaml_parser_t, yaml_parser_delete);
G_DEFINE_AUTOPTR_CLEANUP_FUNC (yaml_event_t, yaml_event_delete);

typedef enum
{
  STATE_START,
  STATE_STREAM,
  STATE_DOCUMENT,
  STATE_ROOT,

  STATE_PROFILES,
  STATE_PROFILES_MAPPING,
  STATE_PROFILE,
  STATE_PROFILE_ATTRIBS,
  STATE_PROFILE_ATTRIBS_KEY_VAL,

  STATE_PLATFORMS_SEQUENCE,
  STATE_PLATFORMS,
  STATE_PLATFORM,
  STATE_PLATFORM_ID,
  STATE_PLATFORM_URL,

  STATE_LIBRARIES,
  STATE_LIBRARIES_LIST,

  STATE_PORT_CONFIG,
  STATE_PORT_CONFIG_MAPPING,
  STATE_PORT_CONFIG_KEY_VAL,

  STATE_DEFAULTS,

  STATE_STOP
} ParserState;

typedef struct
{
  ParserState         state;
  GbpArduinoProfile  *current_profile;
  GbpArduinoPlatform *current_platform;
  char               *current_key;
  GPtrArray          *configs_to_remove;

} ParserContext;

void
parser_context_free (ParserContext *self)
{
  if (self == NULL)
    return;

  g_clear_pointer (&self->current_key, g_free);

  g_free (self);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (ParserContext, parser_context_free)

struct _GbpArduinoConfigProvider
{
  IdeObject parent_instance;

  GFile   *yaml_file;
  gboolean project_file_parsed_correclty;
  gboolean parsing;

  GPtrArray *profiles;

  char *default_profile;
  char *default_fqbn;
  char *default_programmer;
  char *default_port;
  char *default_protocol;

  GFileMonitor *file_monitor;
  gulong        file_change_sig_id;
};

static gboolean
emit_yaml_scalar (yaml_emitter_t *emitter,
                  yaml_event_t   *event,
                  const char     *value)
{
  yaml_scalar_event_initialize (event,
                                NULL,
                                (yaml_char_t *) YAML_STR_TAG,
                                (yaml_char_t *) value,
                                strlen (value),
                                1,
                                0,
                                YAML_PLAIN_SCALAR_STYLE);

  return yaml_emitter_emit (emitter, event);
}

static gboolean
emit_yaml_mapping_start (yaml_emitter_t *emitter,
                         yaml_event_t   *event)
{
  yaml_mapping_start_event_initialize (event,
                                       NULL,
                                       (yaml_char_t *) YAML_MAP_TAG,
                                       1,
                                       YAML_BLOCK_MAPPING_STYLE);

  return yaml_emitter_emit (emitter, event);
}

static gboolean
emit_yaml_mapping_end (yaml_emitter_t *emitter,
                         yaml_event_t   *event)
{
  yaml_mapping_end_event_initialize (event);

  return yaml_emitter_emit (emitter, event);
}

static gboolean
emit_yaml_sequence_start (yaml_emitter_t *emitter,
                          yaml_event_t   *event)
{
  yaml_sequence_start_event_initialize (event,
                                        NULL,
                                        (yaml_char_t *) YAML_SEQ_TAG,
                                        1,
                                        YAML_BLOCK_SEQUENCE_STYLE);

  return yaml_emitter_emit (emitter, event);
}

static gboolean
emit_yaml_sequence_end (yaml_emitter_t *emitter,
                        yaml_event_t   *event)
{
  yaml_sequence_end_event_initialize (event);

  return yaml_emitter_emit (emitter, event);
}

static gboolean
emit_yaml_keyval (yaml_emitter_t *emitter,
                  yaml_event_t   *event,
                  const char     *key,
                  const char     *value)
{
  if (key == NULL || value == NULL || ide_str_empty0 (value) || ide_str_empty0 (key))
    return TRUE;

  if (!emit_yaml_scalar (emitter, event, key))
    return FALSE;

  return emit_yaml_scalar (emitter, event, value);
}

static void
gbp_arduino_config_provider_block_monitor (GbpArduinoConfigProvider *self)
{
  g_assert (GBP_IS_ARDUINO_CONFIG_PROVIDER (self));

  g_signal_handler_block (self->file_monitor,
                          self->file_change_sig_id);
}

static void
gbp_arduino_config_provider_unblock_monitor (GbpArduinoConfigProvider *self)
{
  g_assert (GBP_IS_ARDUINO_CONFIG_PROVIDER (self));

  g_signal_handler_unblock (self->file_monitor,
                            self->file_change_sig_id);
}

static gboolean
emit_platform (yaml_emitter_t     *emitter,
               yaml_event_t       *event,
               GbpArduinoPlatform *platform)
{
  const char *name = gbp_arduino_platform_get_name (platform);
  const char *version = gbp_arduino_platform_get_version (platform);
  const char *index_url = gbp_arduino_platform_get_index_url (platform);
  g_autofree char *platform_str = NULL;

  if (name == NULL || version == NULL)
    return TRUE;

  /* Start platform mapping */
  if (!emit_yaml_mapping_start (emitter, event))
    return FALSE;

  /* Emit platform key with name and version */
  platform_str = g_strdup_printf ("%s (%s)", name, version);
  if (!emit_yaml_keyval (emitter, event, "platform", platform_str))
    return FALSE;

  /* Emit platform_index_url if present */
  if (index_url != NULL && *index_url != '\0')
    {
      if (!emit_yaml_keyval (emitter, event, "platform_index_url", index_url))
        return FALSE;
    }

  /* End platform mapping */
  if (!emit_yaml_mapping_end (emitter, event))
    return FALSE;

  return TRUE;
}

static gboolean
emit_platforms (yaml_emitter_t    *emitter,
                yaml_event_t      *event,
                GbpArduinoProfile *profile)
{
  GListModel *platforms = gbp_arduino_profile_get_platforms (profile);

  if (platforms == NULL || g_list_model_get_n_items (platforms) == 0)
    return TRUE;

  /* Emit platforms key */
  if (!emit_yaml_scalar (emitter, event, "platforms"))
    return FALSE;

  /* Start platforms sequence */
  if (!emit_yaml_sequence_start (emitter, event))
    return FALSE;

  for (guint i = 0; i < g_list_model_get_n_items (platforms); i++)
    {
      g_autoptr(GbpArduinoPlatform) platform = g_list_model_get_item (platforms, i);
      if (!emit_platform (emitter, event, platform))
        return FALSE;
    }

  if (!emit_yaml_sequence_end (emitter, event))
    return FALSE;

  return TRUE;
}

static gboolean
emit_libraries (yaml_emitter_t    *emitter,
                yaml_event_t      *event,
                GbpArduinoProfile *profile)
{
  const char *const *libraries = gbp_arduino_profile_get_libraries (profile);

  if (libraries == NULL || libraries[0] == NULL)
    return TRUE;

  /* Emit libraries key */
  if (!emit_yaml_scalar (emitter, event, "libraries"))
    return FALSE;

  /* Start libraries sequence */
  if (!emit_yaml_sequence_start (emitter, event))
    return FALSE;

  /* Emit each library name */
  for (guint i = 0; libraries[i] != NULL; i++)
    {
      if (!emit_yaml_scalar (emitter, event, libraries[i]))
        return FALSE;
    }

  /* End libraries sequence */
  if (!emit_yaml_sequence_end (emitter, event))
    return FALSE;

  return TRUE;
}

static gboolean
save_configs (GbpArduinoConfigProvider *self,
              GCancellable             *cancellable)
{
  g_autoptr (GFileOutputStream) output_stream = NULL;
  g_autoptr (yaml_emitter_t) emitter = g_new0 (yaml_emitter_t, 1);
  g_autoptr (yaml_event_t) event = g_new0 (yaml_event_t, 1);
  g_autoptr (GError) error = NULL;
  unsigned char output_buffer[8192];
  size_t size_written;
  gboolean need_to_save = FALSE;

  if (self->profiles == NULL)
    return FALSE;

  if (self->parsing || !self->project_file_parsed_correclty)
    return TRUE;

  for (guint i = 0; i < self->profiles->len; i++)
    {
      GbpArduinoProfile *profile = g_ptr_array_index (self->profiles, i);
      if (ide_config_get_dirty (IDE_CONFIG (profile)))
        {
          need_to_save = TRUE;
          break;
        }
    }

  if (!need_to_save)
    return TRUE;

  yaml_emitter_initialize (emitter);
  yaml_emitter_set_output_string (emitter, output_buffer, sizeof (output_buffer), &size_written);

  /* Start document */
  yaml_stream_start_event_initialize (event, YAML_UTF8_ENCODING);
  if (!yaml_emitter_emit (emitter, event))
    return FALSE;

  yaml_document_start_event_initialize (event, NULL, NULL, NULL, 1);
  if (!yaml_emitter_emit (emitter, event))
    return FALSE;

  if (!emit_yaml_mapping_start (emitter, event))
    return FALSE;

  /* Profiles section */
  if (!emit_yaml_scalar (emitter, event, "profiles"))
    return FALSE;

  /* Start profiles mapping */
  if (!emit_yaml_mapping_start (emitter, event))
    return FALSE;

  for (guint i = 0; i < self->profiles->len; i++)
    {
      GbpArduinoProfile *profile = g_ptr_array_index (self->profiles, i);
      const char *id = ide_config_get_id (IDE_CONFIG (profile));

      /* Profile ID */
      if (!emit_yaml_scalar (emitter, event, id))
        return FALSE;

      /* Profile mapping */
      if (!emit_yaml_mapping_start (emitter, event))
        return FALSE;

      if (!emit_yaml_keyval (emitter, event, "fqbn", gbp_arduino_profile_get_fqbn (profile)))
        return FALSE;

      if (!emit_yaml_keyval (emitter, event, "notes", gbp_arduino_profile_get_notes (profile)))
        return FALSE;

      if (!emit_yaml_keyval (emitter, event, "programmer", gbp_arduino_profile_get_programmer (profile)))
        return FALSE;

      if (!emit_yaml_keyval (emitter, event, "port", gbp_arduino_profile_get_port (profile)))
        return FALSE;

      /* Add platforms */
      if (!emit_platforms (emitter, event, profile))
        return FALSE;

      /* Add libraries */
      if (!emit_libraries (emitter, event, profile))
        return FALSE;

      /* End profile mapping */
      if (!emit_yaml_mapping_end (emitter, event))
        return FALSE;
    }

  /* End profiles mapping */
  if (!emit_yaml_mapping_end (emitter, event))
    return FALSE;

  /* Default values */
  if (!emit_yaml_keyval (emitter, event, "default_fqbn", self->default_fqbn))
    return FALSE;

  if (!emit_yaml_keyval (emitter, event, "default_programmer", self->default_programmer))
    return FALSE;

  if (!emit_yaml_keyval (emitter, event, "default_port", self->default_port))
    return FALSE;

  if (!emit_yaml_keyval (emitter, event, "default_protocol", self->default_protocol))
    return FALSE;

  if (!emit_yaml_keyval (emitter, event, "default_profile", self->default_profile))
    return FALSE;

  /* Closing document */
  if (!emit_yaml_mapping_end (emitter, event))
    return FALSE;

  yaml_document_end_event_initialize (event, 1);
  if (!yaml_emitter_emit (emitter, event))
    return FALSE;

  yaml_stream_end_event_initialize (event);
  if (!yaml_emitter_emit (emitter, event))
    return FALSE;

  /* Writing to sketch.yaml */
  if (g_utf8_validate((const char *) output_buffer, 8192, NULL))
    return FALSE;

  gbp_arduino_config_provider_block_monitor (self);

  output_stream = g_file_replace (self->yaml_file,
                                  NULL,
                                  FALSE,
                                  G_FILE_CREATE_REPLACE_DESTINATION,
                                  cancellable,
                                  &error);
  if (output_stream == NULL)
    {
      g_warning ("Failed to open Arduino output stream: %s", error->message);
      gbp_arduino_config_provider_unblock_monitor (self);
      return FALSE;
    }

  if (!g_output_stream_write_all (G_OUTPUT_STREAM (output_stream),
                                  output_buffer,
                                  size_written,
                                  NULL,
                                  cancellable,
                                  &error))
    {
      g_warning ("Failed to write Arduino configuration file: %s", error->message);
      gbp_arduino_config_provider_unblock_monitor (self);
      return FALSE;
    }

  /* Close the stream */
  if (!g_output_stream_close (G_OUTPUT_STREAM (output_stream), cancellable, &error))
    {
      g_warning ("Failed to close Arduino output stream: %s", error->message);
      gbp_arduino_config_provider_unblock_monitor (self);
      return FALSE;
    }

  gbp_arduino_config_provider_unblock_monitor (self);

  return TRUE;
}

static gboolean
gbp_arduino_config_provider_get_profile_by_id (GbpArduinoConfigProvider *self,
                                               const char               *config_id,
                                               GbpArduinoProfile       **config_out)
{
  for (guint i = 0; i < self->profiles->len; i++)
    {
      GbpArduinoProfile *profile = g_ptr_array_index (self->profiles, i);
      if (ide_str_equal (ide_config_get_id (IDE_CONFIG (profile)), config_id))
        {
          *config_out = g_ptr_array_index (self->profiles, i);
          return TRUE;
        }
    }

  return FALSE;
}

static gboolean
parse_yaml_event (GbpArduinoConfigProvider *self,
                  yaml_event_t             *event,
                  ParserContext            *ctx,
                  GError                  **error)
{
  const char *value;

  switch (ctx->state)
    {
    case STATE_START:
      if (event->type == YAML_STREAM_START_EVENT)
        {
          ctx->state = STATE_STREAM;
        }
      else
        {
          g_warning ("Unexpected event %d in state YAML_STREAM_START_EVENT.\n", event->type);
          return FALSE;
        }
      break;

    case STATE_STREAM:
      if (event->type == YAML_DOCUMENT_START_EVENT)
        {
          ctx->state = STATE_DOCUMENT;
        }
      else if (event->type == YAML_STREAM_END_EVENT)
        {
          ctx->state = STATE_STOP;
        }
      else
        {
          g_warning ("Unexpected event %d in state STATE_STREAM.\n", event->type);
          return FALSE;
        }
      break;

    case STATE_DOCUMENT:
      if (event->type == YAML_MAPPING_START_EVENT)
        {
          ctx->state = STATE_ROOT;
        }
      else if (event->type == YAML_DOCUMENT_END_EVENT)
        {
          ctx->state = STATE_STREAM;
        }
      else
        {
          g_warning ("Unexpected event %d in state STATE_DOCUMENT.\n", event->type);
          return FALSE;
        }
      break;

    case STATE_ROOT:
      if (event->type == YAML_SCALAR_EVENT)
        {
          value = (char *) event->data.scalar.value;
          if (ide_str_equal (value, "profiles"))
            {
              ctx->state = STATE_PROFILES;
            }
          else if (g_str_has_prefix (value, "default_"))
            {
              ctx->state = STATE_DEFAULTS;
              ctx->current_key = g_strdup (value);
            }
          else
            {
              g_warning ("Unexpected scalar: %s\n", value);
              return FALSE;
            }
        }
      else if (event->type == YAML_MAPPING_END_EVENT)
        {
          ctx->state = STATE_DOCUMENT;
        }
      else
        {
          g_warning ("Unexpected event %d in state STATE_ROOT.\n", event->type);
          return FALSE;
        }
      break;

    case STATE_PROFILES:
      if (event->type == YAML_MAPPING_START_EVENT)
        {
          ctx->state = STATE_PROFILES_MAPPING;
        }
      else
        {
          g_warning ("Unexpected event %d in state STATE_PROFILES.\n", event->type);
          return FALSE;
        }
      break;

    case STATE_PROFILES_MAPPING:
      if (event->type == YAML_SCALAR_EVENT)
        {
          value = (char *) event->data.scalar.value;
          ctx->state = STATE_PROFILE;

          if (gbp_arduino_config_provider_get_profile_by_id (self, value, &ctx->current_profile))
            {
              gbp_arduino_profile_reset (ctx->current_profile);
              g_ptr_array_remove (ctx->configs_to_remove, ctx->current_profile);
            }
          else
            {
              ctx->current_profile = g_object_new (GBP_TYPE_ARDUINO_PROFILE,
                                                   "id", value,
                                                   "parent", self,
                                                   "display-name", value,
                                                   NULL);
              g_ptr_array_add (self->profiles, ctx->current_profile);
              g_object_ref (ctx->current_profile);
              ide_config_provider_emit_added (IDE_CONFIG_PROVIDER (self), IDE_CONFIG (ctx->current_profile));
            }
        }

      else if (event->type == YAML_MAPPING_END_EVENT)
        {
          ctx->state = STATE_ROOT;
        }
      else
        {
          g_warning ("Unexpected event %d in state STATE_PROFILES_MAPPING.\n", event->type);
          return FALSE;
        }
      break;

    case STATE_PROFILE:
      if (event->type == YAML_MAPPING_START_EVENT)
        {
          ctx->state = STATE_PROFILE_ATTRIBS;
        }
      else if (event->type == YAML_SCALAR_EVENT)
        {
          ctx->state = STATE_PROFILE_ATTRIBS;
        }
      else
        {
          g_warning ("Unexpected event %d in state STATE_PROFILE.\n", event->type);
          return FALSE;
        }
      break;

    case STATE_PROFILE_ATTRIBS:
      if (event->type == YAML_SCALAR_EVENT)
        {
          value = (char *) event->data.scalar.value;
          ctx->current_key = g_strdup (value);
          if (ide_str_equal (value, "platforms"))
            {
              ctx->state = STATE_PLATFORMS;
            }
          else if (ide_str_equal (value, "libraries"))
            {
              ctx->state = STATE_LIBRARIES;
            }
          else if (ide_str_equal (value, "port_config"))
            {
              ctx->state = STATE_PORT_CONFIG;
            }
          else
            {
              ctx->state = STATE_PROFILE_ATTRIBS_KEY_VAL;
            }
        }
      else if (event->type == YAML_MAPPING_START_EVENT)
        {
          ctx->state = STATE_PORT_CONFIG;
        }
      else if (event->type == YAML_MAPPING_END_EVENT)
        {
          ctx->state = STATE_PROFILES_MAPPING;
        }
      else
        {
          g_warning ("Unexpected event %d in state STATE_PROFILE_ATTRIBS.\n", event->type);
          return FALSE;
        }
      break;

    case STATE_PROFILE_ATTRIBS_KEY_VAL:
      if (event->type == YAML_SCALAR_EVENT)
        {
          value = (char *) event->data.scalar.value;
          if (ide_str_equal (ctx->current_key, "fqbn"))
            {
              gbp_arduino_profile_set_fqbn (ctx->current_profile, value);
            }
          else if (ide_str_equal (ctx->current_key, "port"))
            {
              gbp_arduino_profile_set_port (ctx->current_profile, value);
            }
          else if (ide_str_equal (ctx->current_key, "notes"))
            {
              gbp_arduino_profile_set_notes (ctx->current_profile, value);
            }
          else if (ide_str_equal (ctx->current_key, "programmer"))
            {
              gbp_arduino_profile_set_programmer (ctx->current_profile, value);
            }
          ctx->state = STATE_PROFILE_ATTRIBS;
        }
      else
        {
          g_warning ("Unexpected event %d in state STATE_PROFILE_ATTRIBS_KEY_VAL.\n", event->type);
          return FALSE;
        }
      break;

    case STATE_PLATFORMS:
      if (event->type == YAML_SEQUENCE_START_EVENT)
        {
          ctx->state = STATE_PLATFORMS_SEQUENCE;
        }
      else
        {
          g_warning ("Unexpected event %d in state STATE_PLATFORMS.\n", event->type);
          return FALSE;
        }
      break;

    case STATE_PLATFORMS_SEQUENCE:
      if (event->type == YAML_MAPPING_START_EVENT)
        {
          ctx->state = STATE_PLATFORM;
        }
      else if (event->type == YAML_SEQUENCE_END_EVENT)
        {
          ctx->state = STATE_PROFILE_ATTRIBS;
        }
      else
        {
          g_warning ("Unexpected event %d in state STATE_PLATFORMS_SEQUENCE.\n", event->type);
          return FALSE;
        }
      break;

    case STATE_PLATFORM:
      if (event->type == YAML_SCALAR_EVENT)
        {
          value = (char *) event->data.scalar.value;
          if (ide_str_equal (value, "platform"))
            {
              ctx->state = STATE_PLATFORM_ID;
            }
          else if (ide_str_equal (value, "platform_index_url"))
            {
              ctx->state = STATE_PLATFORM_URL;
            }
        }
      else if (event->type == YAML_MAPPING_END_EVENT)
        {
          ctx->state = STATE_PLATFORMS_SEQUENCE;
        }
      else
        {
          g_warning ("Unexpected event %d in state STATE_PLATFORM.\n", event->type);
          return FALSE;
        }
      break;

    case STATE_PLATFORM_ID:
      if (event->type == YAML_SCALAR_EVENT)
        {
          g_auto (GStrv) platform_parts;

          value = (char *) event->data.scalar.value;
          platform_parts = g_regex_split_simple ("\\s+\\((.*?)\\)", value, 0, 0);

          if (platform_parts && platform_parts[0] && platform_parts[1])
            {
              ctx->current_platform = gbp_arduino_platform_new (g_strdup (platform_parts[0]),
                                                                g_strdup (g_strchomp (platform_parts[1])),
                                                                "");

              gbp_arduino_profile_add_platform (ctx->current_profile, ctx->current_platform);
            }
          ctx->state = STATE_PLATFORM;
        }
      else
        {
          g_warning ("Unexpected event %d in state STATE_PLATFORM_ID.\n", event->type);
          return FALSE;
        }
      break;

    case STATE_PLATFORM_URL:
      if (event->type == YAML_SCALAR_EVENT)
        {
          value = (char *) event->data.scalar.value;
          gbp_arduino_platform_set_index_url (ctx->current_platform, value);
          ctx->state = STATE_PLATFORM;
        }
      else
        {
          g_warning ("Unexpected event %d in state STATE_PLATFORM_URL.\n", event->type);
          return FALSE;
        }
      break;

    case STATE_LIBRARIES:
      if (event->type == YAML_SEQUENCE_START_EVENT)
        {
          ctx->state = STATE_LIBRARIES_LIST;
        }
      else
        {
          g_warning ("Unexpected event %d in state STATE_LIBRARIES.\n", event->type);
          return FALSE;
        }
      break;

    case STATE_LIBRARIES_LIST:
      if (event->type == YAML_SCALAR_EVENT)
        {
          value = (char *) event->data.scalar.value;
          gbp_arduino_profile_add_library (ctx->current_profile, value);
        }
      else if (event->type == YAML_SEQUENCE_END_EVENT)
        {
          ctx->state = STATE_PROFILE_ATTRIBS;
        }
      else
        {
          g_warning ("Unexpected event %d in state STATE_LIBRARIES_LIST.\n", event->type);
          return FALSE;
        }
      break;

    case STATE_PORT_CONFIG:
      if (event->type == YAML_MAPPING_START_EVENT)
        {
          ctx->state = STATE_PORT_CONFIG_MAPPING;
        }
      else
        {
          g_warning ("Unexpected event %d in state STATE_PORT_CONFIG.\n", event->type);
          return FALSE;
        }
      break;

    case STATE_PORT_CONFIG_MAPPING:
      if (event->type == YAML_SCALAR_EVENT)
        {
          value = (char *) event->data.scalar.value;
          ctx->current_key = g_strdup (value);
        }
      else if (event->type == YAML_MAPPING_END_EVENT)
        {
          ctx->state = STATE_PROFILE_ATTRIBS;
        }
      else
        {
          g_warning ("Unexpected event %d in state STATE_PORT_CONFIG_MAPPING.\n", event->type);
          return FALSE;
        }
      break;

    case STATE_PORT_CONFIG_KEY_VAL:
      if (event->type == YAML_SCALAR_EVENT)
        {
          /* We don't support port config for now */
          ctx->state = STATE_PORT_CONFIG_MAPPING;
        }
      else
        {
          g_warning ("Unexpected event %d in state STATE_PORT_CONFIG_KEY_VAL.\n", event->type);
          return FALSE;
        }
      break;

    case STATE_DEFAULTS:
      if (event->type == YAML_SCALAR_EVENT)
        {
          value = (char *) event->data.scalar.value;
          if (ide_str_equal (ctx->current_key, "default_profile") == 0)
            {
              self->default_profile = g_strdup (value);
            }
          else if (ide_str_equal (ctx->current_key, "default_fqbn") == 0)
            {
              self->default_fqbn = g_strdup (value);
            }
          else if (ide_str_equal (ctx->current_key, "default_programmer") == 0)
            {
              self->default_programmer = g_strdup (value);
            }
          else if (ide_str_equal (ctx->current_key, "default_port") == 0)
            {
              self->default_port = g_strdup (value);
            }
          else if (ide_str_equal (ctx->current_key, "default_protocol") == 0)
            {
              self->default_protocol = g_strdup (value);
            }

          g_clear_pointer (&ctx->current_key, g_free);
          ctx->state = STATE_ROOT;
        }
      else
        {
          g_warning ("Unexpected event %d in state STATE_DEFAULTS.\n", event->type);
          return FALSE;
        }
      break;

    case STATE_STOP:
      break;

    default:
      g_warning ("Unexpected STATE in Arduino sketch.yaml parser\n");
      return FALSE;
    }

  if (ctx->current_profile)
    {
      ide_config_set_dirty (IDE_CONFIG (ctx->current_profile), FALSE);
    }

  return TRUE;
}

static void
update_configs_from_file (GbpArduinoConfigProvider *self)
{
  g_autoptr (GBytes) yaml_bytes = NULL;
  g_autoptr (GError) error = NULL;
  g_autoptr (yaml_parser_t) parser = g_new0 (yaml_parser_t, 1);
  g_autoptr (yaml_event_t) event = g_new0 (yaml_event_t, 1);
  g_autoptr (ParserContext) ctx = g_new0 (ParserContext, 1);

  g_return_if_fail (GBP_IS_ARDUINO_CONFIG_PROVIDER (self));

  if (!g_file_query_exists (self->yaml_file, NULL))
    {
      g_warning ("Arduino profiles YAML file not found");
      self->project_file_parsed_correclty = FALSE;
      return;
    }

  yaml_bytes = g_file_load_bytes (self->yaml_file, NULL, NULL, &error);
  if (error)
    {
      g_warning ("Failed to load Arduino profiles YAML: %s", error->message);
      self->project_file_parsed_correclty = FALSE;
      return;
    }

  yaml_parser_initialize (parser);
  yaml_parser_set_input_string (parser,
                                g_bytes_get_data (yaml_bytes, NULL),
                                g_bytes_get_size (yaml_bytes));

  ctx->state = STATE_START;
  ctx->configs_to_remove = g_ptr_array_copy (self->profiles, NULL, NULL);

  self->parsing = TRUE;

  /* Parse the file */
  do
    {
      if (!yaml_parser_parse (parser, event))
        {
          g_set_error (&error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                       "Failed to parse YAML: %s", parser->problem);
          self->project_file_parsed_correclty = FALSE;
          self->parsing = FALSE;
          return;
        }

      if (!parse_yaml_event (self, event, ctx, &error))
        {
          self->project_file_parsed_correclty = FALSE;
          self->parsing = FALSE;
          return;
        }
      if (event->type != YAML_STREAM_END_EVENT)
        {
          yaml_event_delete (event);
        }
    }
  while (event->type != YAML_STREAM_END_EVENT);

  /* Remove profiles not present in the project file anymore */
  for (guint i = 0; i < ctx->configs_to_remove->len; i++)
    {
      GbpArduinoProfile *profile = g_ptr_array_index (ctx->configs_to_remove, i);
      ide_config_provider_emit_removed (IDE_CONFIG_PROVIDER (self), IDE_CONFIG (profile));
      g_ptr_array_remove (self->profiles, profile);
    }

  /* Set all profiles to dirty */
  for (guint i = 0; i < self->profiles->len; i++)
    {
      GbpArduinoProfile *profile = g_ptr_array_index (self->profiles, i);
      ide_config_set_dirty (IDE_CONFIG (profile), FALSE);
    }

  self->project_file_parsed_correclty = TRUE;
  self->parsing = FALSE;
}

static void
gbp_arduino_sketch_file_changed (GbpArduinoConfigProvider *self,
                                 GFile                    *file,
                                 GFile                    *other_file,
                                 GFileMonitorEvent         event,
                                 GFileMonitor             *file_monitor)
{
  g_assert (GBP_IS_ARDUINO_CONFIG_PROVIDER (self));
  g_assert (G_IS_FILE_MONITOR (file_monitor));

  if (event == G_FILE_MONITOR_EVENT_CHANGED || event == G_FILE_MONITOR_EVENT_CREATED)
    {
      update_configs_from_file (self);
    }
}

static void
gbp_arduino_config_provider_load_async (IdeConfigProvider  *provider,
                                        GCancellable       *cancellable,
                                        GAsyncReadyCallback callback,
                                        gpointer            user_data)
{
  GbpArduinoConfigProvider *self = (GbpArduinoConfigProvider *) provider;
  g_autoptr (IdeTask) task = NULL;
  gulong sig_id;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_ARDUINO_CONFIG_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (provider, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_arduino_config_provider_load_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  self->yaml_file = ide_context_build_file (ide_object_get_context (IDE_OBJECT (self)), "sketch.yaml");

  if (!g_file_query_exists (self->yaml_file, NULL))
    {
      g_clear_object (&self->yaml_file);
      self->yaml_file = ide_context_build_file (ide_object_get_context (IDE_OBJECT (self)), "sketch.yml");
    }

  if (!g_file_query_exists (self->yaml_file, NULL))
    {
      ide_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  self->file_monitor = g_file_monitor_file (self->yaml_file, G_FILE_MONITOR_NONE, NULL, NULL);

  sig_id = g_signal_connect_object (self->file_monitor,
                                    "changed",
                                    G_CALLBACK (gbp_arduino_sketch_file_changed),
                                    self,
                                    G_CONNECT_SWAPPED);

  self->file_change_sig_id = sig_id;

  update_configs_from_file (self);

  ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static gboolean
gbp_arduino_config_provider_load_finish (IdeConfigProvider *provider,
                                         GAsyncResult      *result,
                                         GError           **error)
{
  g_assert (IDE_IS_TASK (result));
  g_assert (ide_task_is_valid (IDE_TASK (result), provider));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
gbp_arduino_config_provider_save_async (IdeConfigProvider  *provider,
                                        GCancellable       *cancellable,
                                        GAsyncReadyCallback callback,
                                        gpointer            user_data)
{
  GbpArduinoConfigProvider *self = (GbpArduinoConfigProvider *) provider;
  g_autoptr (IdeTask) task = NULL;
  g_autoptr (GError) error = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_ARDUINO_CONFIG_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_arduino_config_provider_save_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  save_configs (self, cancellable);
  ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static gboolean
gbp_arduino_config_provider_save_finish (IdeConfigProvider *provider,
                                         GAsyncResult      *result,
                                         GError           **error)
{
  g_assert (GBP_IS_ARDUINO_CONFIG_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));
  g_assert (ide_task_is_valid (IDE_TASK (result), provider));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
gbp_arduino_config_provider_unload (IdeConfigProvider *provider)
{
}

char *
get_config_copy_id (GbpArduinoConfigProvider *self,
                    const char               *initial_name)
{
  char *new_name;
  guint suffix = 0;
  gboolean name_exists;

  g_assert (GBP_IS_ARDUINO_CONFIG_PROVIDER (self));

  do {
    name_exists = FALSE;

    if (suffix == 0)
      new_name = g_strdup(initial_name);
    else
      new_name = g_strdup_printf("%s_%u", initial_name, suffix);

    for (guint i = 0; i < self->profiles->len; i++)
      {
        GbpArduinoProfile *profile = g_ptr_array_index (self->profiles, i);
        const char *id = ide_config_get_id (IDE_CONFIG (profile));

        if (ide_str_equal0 (id, new_name))
          {
            name_exists = TRUE;
            g_free(new_name);
            suffix++;
            break;
          }
      }
  } while (name_exists);

  return new_name;
}

static void
gbp_arduino_config_provider_duplicate (IdeConfigProvider *provider,
                                       IdeConfig         *config)
{
  GbpArduinoConfigProvider *self = (GbpArduinoConfigProvider *) provider;
  GbpArduinoProfile *profile = (GbpArduinoProfile *) config;
  g_autoptr (GbpArduinoProfile) new_profile = NULL;
  g_autoptr (GError) error = NULL;
  g_autofree char *new_id;

  g_assert (GBP_IS_ARDUINO_CONFIG_PROVIDER (self));
  g_assert (GBP_IS_ARDUINO_PROFILE (profile));

  new_id = get_config_copy_id (self, ide_config_get_id (config));

  new_profile = g_object_new (GBP_TYPE_ARDUINO_PROFILE,
                              "id", new_id,
                              "display-name", new_id,
                              "fqbn", gbp_arduino_profile_get_fqbn (profile),
                              "libraries", gbp_arduino_profile_get_libraries (profile),
                              "platforms", gbp_arduino_profile_get_platforms (profile),
                              "notes", gbp_arduino_profile_get_notes (profile),
                              "programmer", gbp_arduino_profile_get_programmer (profile),
                              "parent", self,
                              "dirty", TRUE,
                              NULL);

  g_ptr_array_add (self->profiles, new_profile);

  ide_config_provider_emit_added (IDE_CONFIG_PROVIDER (self), IDE_CONFIG (new_profile));
}

static void
gbp_arduino_config_provider_delete (IdeConfigProvider *provider,
                                    IdeConfig         *config)
{
  GbpArduinoConfigProvider *self = (GbpArduinoConfigProvider *) provider;

  g_assert (GBP_IS_ARDUINO_CONFIG_PROVIDER (self));
  g_assert (GBP_IS_ARDUINO_PROFILE (config));

  for (guint i = 0; i < self->profiles->len; i++)
    {
      if (g_ptr_array_index (self->profiles, i) == config)
        {
          ide_config_provider_emit_removed (IDE_CONFIG_PROVIDER (self), config);
          g_ptr_array_remove_index (self->profiles, i);
          for (guint j = 0; j < self->profiles->len; j++)
            {
              GbpArduinoProfile *profile = g_ptr_array_index (self->profiles, j);
              ide_config_set_dirty (IDE_CONFIG (profile), TRUE);
            }
          return;
        }
    }
}

static void
configuration_provider_iface_init (IdeConfigProviderInterface *iface)
{
  iface->load_async = gbp_arduino_config_provider_load_async;
  iface->load_finish = gbp_arduino_config_provider_load_finish;
  iface->save_async = gbp_arduino_config_provider_save_async;
  iface->save_finish = gbp_arduino_config_provider_save_finish;
  iface->unload = gbp_arduino_config_provider_unload;
  iface->duplicate = gbp_arduino_config_provider_duplicate;
  iface->delete = gbp_arduino_config_provider_delete;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpArduinoConfigProvider,
                               gbp_arduino_config_provider,
                               IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_CONFIG_PROVIDER,
                                                      configuration_provider_iface_init))

static void
gbp_arduino_config_provider_destroy (IdeObject *object)
{
  GbpArduinoConfigProvider *self = (GbpArduinoConfigProvider *) object;

  g_clear_pointer (&self->profiles, g_ptr_array_unref);
  g_clear_pointer (&self->default_profile, g_free);
  g_clear_pointer (&self->default_fqbn, g_free);
  g_clear_pointer (&self->default_programmer, g_free);
  g_clear_pointer (&self->default_port, g_free);
  g_clear_pointer (&self->default_protocol, g_free);

  g_clear_signal_handler (&self->file_change_sig_id, self->file_monitor);
  g_clear_object (&self->file_monitor);
  g_clear_object (&self->yaml_file);

  IDE_OBJECT_CLASS (gbp_arduino_config_provider_parent_class)->destroy (object);
}

static void
gbp_arduino_config_provider_class_init (GbpArduinoConfigProviderClass *klass)
{
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  i_object_class->destroy = gbp_arduino_config_provider_destroy;
}

static void
gbp_arduino_config_provider_init (GbpArduinoConfigProvider *self)
{
  self->parsing = FALSE;
  self->profiles = g_ptr_array_new ();
}
