/*
 * gbp-arduino-device-monitor.c
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

#define G_LOG_DOMAIN "gbp-arduino-device-monitor"

#include <json-glib/json-glib.h>

#include "gbp-arduino-device-monitor.h"
#include "gbp-arduino-port.h"

struct _GbpArduinoDeviceMonitor
{
  GObject parent_instance;

  GListStore *available_ports;
  GCancellable *cancellable;
  IdeSubprocess *watch_subprocess;
  char *current_output;
};

G_DEFINE_FINAL_TYPE (GbpArduinoDeviceMonitor, gbp_arduino_device_monitor, G_TYPE_OBJECT)

enum
{
  ADDED,
  REMOVED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

static void process_port_event (GbpArduinoDeviceMonitor *self, JsonNode *root);

static void
read_watch_output_cb (GObject      *object,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  GbpArduinoDeviceMonitor *self = user_data;
  GDataInputStream *data_stream = (GDataInputStream *) object;
  g_autoptr (GError) error = NULL;
  g_autoptr (JsonParser) parser = NULL;
  g_autofree char *line = NULL;
  gsize length;
  JsonNode *root;

  if (g_cancellable_is_cancelled (g_task_get_cancellable (G_TASK (result))))
    return;

  g_assert (G_IS_DATA_INPUT_STREAM (data_stream));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_ARDUINO_DEVICE_MONITOR (self));

  line = g_data_input_stream_read_line_finish_utf8 (data_stream, result, &length, &error);
  if (error != NULL)
    {
      g_warning ("Error reading from arduino-cli: %s", error->message);
      return;
    }

  if (line != NULL)
    {
      g_autoptr (GString) new_string = g_string_new (self->current_output);
      g_string_append (new_string, line);
      g_set_str (&self->current_output, new_string->str);

      parser = json_parser_new ();
      if (json_parser_load_from_data (parser, self->current_output, -1, &error))
        {
          root = json_parser_get_root (parser);
          if (root != NULL)
            {
              process_port_event (self, root);
              g_clear_pointer (&self->current_output, g_free);
            }
        }
    }

  if (!g_cancellable_is_cancelled (self->cancellable))
    {
      g_data_input_stream_read_line_async (data_stream,
                                           G_PRIORITY_DEFAULT,
                                           self->cancellable,
                                           read_watch_output_cb,
                                           self);
    }
}

static void
process_port_event (GbpArduinoDeviceMonitor *self,
                    JsonNode                 *root)
{
  const char *port_name = NULL;
  const char *address;
  const char *protocol;
  const char *protocol_label;
  JsonObject *event_obj;
  const char *event_type;
  JsonObject *port_obj;

  g_assert (GBP_IS_ARDUINO_DEVICE_MONITOR (self));
  g_assert (root != NULL);

  if (!JSON_NODE_HOLDS_OBJECT (root) ||
      !(event_obj = json_node_get_object (root)) ||
      !json_object_has_member (event_obj, "eventType") ||
      !(event_type = json_object_get_string_member (event_obj, "eventType")) ||
      !json_object_has_member (event_obj, "port") ||
      !(port_obj = json_object_get_object_member (event_obj, "port")) ||
      !json_object_has_member (port_obj, "address") ||
      !(address = json_object_get_string_member (port_obj, "address"))
      )
    {
      return;
    }

  if (ide_str_equal0 (event_type, "add"))
    {
      g_autoptr (GbpArduinoPort) port = NULL;

      if (json_object_has_member (event_obj, "matching_boards"))
        {
          JsonArray *matching_boards = json_object_get_array_member (event_obj, "matching_boards");

          if (matching_boards != NULL && json_array_get_length (matching_boards) > 0)
            {
              JsonObject *matching_board = json_array_get_object_element (matching_boards, 0);

              if ((matching_board != NULL) &&
                  json_object_has_member (matching_board, "name"))
                {
                  const char *matching_board_name = json_object_get_string_member (matching_board, "name");
                  port_name = g_strdup_printf ("%s (%s)", address, matching_board_name);
                }
            }
        }

      protocol = json_object_get_string_member_with_default (port_obj, "protocol", "serial");
      protocol_label = json_object_get_string_member_with_default (port_obj, "protocol_label", "Serial Port");

      port = gbp_arduino_port_new (address,
                                   port_name ? port_name : address,
                                   protocol,
                                   protocol_label);

      g_list_store_append (self->available_ports, port);
      g_signal_emit (self, signals[ADDED], 0, port);
    }
  else if (ide_str_equal0 (event_type, "remove"))
    {
      guint ports_n = g_list_model_get_n_items (G_LIST_MODEL (self->available_ports));

      for (guint i = 0; i < ports_n; i++)
        {
          g_autoptr (GbpArduinoPort) existing_port = g_list_model_get_item (G_LIST_MODEL (self->available_ports), i);

          if (g_str_equal (ide_device_get_id (IDE_DEVICE (existing_port)), address))
            {
              g_signal_emit (self, signals[REMOVED], 0, existing_port);
              g_list_store_remove (self->available_ports, i);
              break;
            }
        }
    }
}

static void
gbp_arduino_device_monitor_finalize (GObject *object)
{
  GbpArduinoDeviceMonitor *self = (GbpArduinoDeviceMonitor *) object;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  g_clear_object (&self->available_ports);
  g_clear_object (&self->watch_subprocess);
  g_clear_pointer (&self->current_output, g_free);

  G_OBJECT_CLASS (gbp_arduino_device_monitor_parent_class)->finalize (object);
}

static void
gbp_arduino_device_monitor_class_init (GbpArduinoDeviceMonitorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_arduino_device_monitor_finalize;

  signals[ADDED] =
      g_signal_new ("added",
                    G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_LAST,
                    0,
                    NULL, NULL,
                    NULL,
                    G_TYPE_NONE,
                    1,
                    GBP_TYPE_ARDUINO_PORT);

  signals[REMOVED] =
      g_signal_new ("removed",
                    G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_LAST,
                    0,
                    NULL, NULL,
                    NULL,
                    G_TYPE_NONE,
                    1,
                    GBP_TYPE_ARDUINO_PORT);
}

static void
gbp_arduino_device_monitor_init (GbpArduinoDeviceMonitor *self)
{
  self->available_ports = g_list_store_new (GBP_TYPE_ARDUINO_PORT);
  self->cancellable = g_cancellable_new ();
}

GbpArduinoDeviceMonitor *
gbp_arduino_device_monitor_new (void)
{
  return g_object_new (GBP_TYPE_ARDUINO_DEVICE_MONITOR, NULL);
}

void
gbp_arduino_device_monitor_start (GbpArduinoDeviceMonitor *self)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (IdeRunContext) run_context = NULL;
  g_autoptr (IdeSubprocessLauncher) launcher = NULL;
  g_autoptr (IdeSubprocess) subprocess = NULL;
  GInputStream *stdout_stream;
  g_autoptr (GDataInputStream) data_stream = NULL;

  g_assert (GBP_IS_ARDUINO_DEVICE_MONITOR (self));

  run_context = ide_run_context_new ();
  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);

  ide_run_context_push_host (run_context);
  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);
  ide_subprocess_launcher_push_argv (launcher, "arduino-cli");
  ide_subprocess_launcher_push_argv (launcher, "board");
  ide_subprocess_launcher_push_argv (launcher, "list");
  ide_subprocess_launcher_push_argv (launcher, "--watch");
  ide_subprocess_launcher_push_argv (launcher, "--json");

  subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error);
  if (error != NULL)
    {
      g_warning ("Failed to launch arduino-cli: %s", error->message);
      return;
    }

  g_set_object (&self->watch_subprocess, subprocess);

  stdout_stream = ide_subprocess_get_stdout_pipe (subprocess);
  data_stream = g_data_input_stream_new (stdout_stream);
  g_data_input_stream_set_newline_type (data_stream, G_DATA_STREAM_NEWLINE_TYPE_ANY);

  g_data_input_stream_read_line_async (data_stream,
                                       G_PRIORITY_DEFAULT,
                                       self->cancellable,
                                       read_watch_output_cb,
                                       self);
}
