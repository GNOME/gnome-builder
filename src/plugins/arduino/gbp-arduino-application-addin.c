/*
 * gbp-arduino-application-addin.c
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

#define G_LOG_DOMAIN "gbp-arduino-application-addin"

#include "config.h"

# include <glib/gi18n.h>

#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <libide-core.h>
#include <libide-foundry.h>
#include <libide-gui.h>
#include <libide-lsp.h>

#include "gbp-arduino-board-option.h"
#include "gbp-arduino-board.h"
#include "gbp-arduino-library-info.h"
#include "gbp-arduino-option-value.h"
#include "gbp-arduino-platform-info.h"
#include "gbp-arduino-platform.h"
#include "gbp-arduino-port.h"

#define GBP_TYPE_ARDUINO_APPLICATION_ADDIN (gbp_arduino_application_addin_get_type ())

G_DECLARE_FINAL_TYPE (GbpArduinoApplicationAddin, gbp_arduino_application_addin, GBP, ARDUINO_APPLICATION_ADDIN, GObject)

typedef struct
{
  GbpArduinoApplicationAddin *self;
  IdeNotification *notif;
} ProgressData;

static void
progress_data_free (ProgressData *data)
{
  g_clear_object (&data->notif);
  g_free (data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (ProgressData, progress_data_free)

struct _GbpArduinoApplicationAddin
{
  IdeObject parent_instance;

  IdeApplication *app;

  GListStore *available_boards;
  GListStore *installed_libraries;
  GListStore *installed_platforms;

  gboolean has_arduino_cli;
};

enum
{
  PROP_0,
  PROP_AVAILABLE_BOARDS,
  PROP_INSTALLED_LIBRARIES,
  PROP_INSTALLED_PLATFORMS,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
get_platforms_and_boards_communicated (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  GbpArduinoApplicationAddin *self = user_data;
  IdeSubprocess *subprocess = (IdeSubprocess *) object;
  g_autoptr (JsonParser) parser = NULL;
  g_autofree char *stdout_str = NULL;
  g_autofree char *stderr_buf = NULL;
  g_autoptr (GError) error = NULL;
  g_autoptr (GListStore) new_boards = g_list_store_new (GBP_TYPE_ARDUINO_BOARD);
  g_autoptr (GListStore) new_platforms = g_list_store_new (GBP_TYPE_ARDUINO_PLATFORM_INFO);
  JsonNode *root;
  JsonObject *root_obj;
  JsonArray *platforms_array;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_ARDUINO_APPLICATION_ADDIN (self));

  ide_subprocess_communicate_utf8_finish (subprocess, result, &stdout_str, &stderr_buf, &error);

  parser = json_parser_new ();
  if (!json_parser_load_from_data (parser, stdout_str, -1, &error))
    {
      g_warning ("Failed to parse JSON: %s", error->message);
      return;
    }

  root = json_parser_get_root (parser);
  root_obj = json_node_get_object (root);

  platforms_array = json_object_get_array_member (root_obj, "platforms");

  for (guint i = 0; i < json_array_get_length (platforms_array); i++)
    {
      g_autoptr (GbpArduinoPlatformInfo) platform_info = NULL;
      JsonObject *platform = json_array_get_object_element (platforms_array, i);
      JsonObject *releases = json_object_get_object_member (platform, "releases");
      const char *platform_id = json_object_get_string_member (platform, "id");
      const char *maintainer = json_object_get_string_member (platform, "maintainer");
      const char *installed_version = json_object_get_string_member (platform, "installed_version");
      const char *latest_version = json_object_get_string_member (platform, "latest_version");

      platform_info = gbp_arduino_platform_info_new (platform_id,
                                                     latest_version,
                                                     NULL,
                                                     maintainer,
                                                     platform_id,
                                                     installed_version);

      g_list_store_append (new_platforms, platform_info);

      if (releases && json_object_has_member (releases, installed_version))
        {
          JsonObject *release = json_object_get_object_member (releases, installed_version);
          if (release && json_object_has_member (release, "boards"))
            {
              JsonArray *boards_array = json_object_get_array_member (release, "boards");

              if (boards_array)
                {
                  for (guint j = 0; j < json_array_get_length (boards_array); j++)
                    {
                      g_autoptr (GbpArduinoBoard) new_board = NULL;
                      JsonObject *board = json_array_get_object_element (boards_array, j);
                      const char *board_name = json_object_get_string_member (board, "name");
                      const char *board_fqbn;

                      if (json_object_has_member (board, "fqbn"))
                        board_fqbn = json_object_get_string_member (board, "fqbn");
                      else
                        board_fqbn = "";

                      new_board = gbp_arduino_board_new (platform_id,
                                                         board_name,
                                                         board_fqbn);

                      g_list_store_append (new_boards, new_board);
                    }
                }
            }
        }
    }

  g_object_set (self,
                "installed-platforms", new_platforms,
                "available-boards", new_boards,
                NULL);
}

static void
get_platforms_and_boards (GbpArduinoApplicationAddin *self)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (IdeRunContext) run_context = NULL;
  g_autoptr (IdeSubprocessLauncher) launcher = NULL;
  g_autoptr (IdeSubprocess) subprocess = NULL;
  g_autoptr (GBytes) stdout_buf = NULL;
  g_autofree char *stdout_str = NULL;

  run_context = ide_run_context_new ();
  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);

  ide_run_context_push_host (run_context);
  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);
  ide_subprocess_launcher_push_argv (launcher, "arduino-cli");
  ide_subprocess_launcher_push_argv (launcher, "core");
  ide_subprocess_launcher_push_argv (launcher, "list");
  ide_subprocess_launcher_push_argv (launcher, "--json");

  subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error);
  if (error != NULL)
    {
      g_warning ("Failed to launch arduino-cli: %s", error->message);
      return;
    }

  ide_subprocess_communicate_utf8_async (subprocess,
                                         NULL,
                                         NULL,
                                         get_platforms_and_boards_communicated,
                                         self);
}

static void
get_libraries_communicated (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  GbpArduinoApplicationAddin *self = user_data;
  IdeSubprocess *subprocess = (IdeSubprocess *) object;
  g_autoptr (JsonParser) parser = NULL;
  g_autofree char *stdout_str = NULL;
  g_autofree char *stderr_buf = NULL;
  g_autoptr (GError) error = NULL;
  g_autoptr (GListStore) new_libraries = g_list_store_new (GBP_TYPE_ARDUINO_LIBRARY_INFO);
  JsonNode *root;
  JsonObject *root_obj;
  JsonArray *libraries_array;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_ARDUINO_APPLICATION_ADDIN (self));

  ide_subprocess_communicate_utf8_finish (subprocess, result, &stdout_str, &stderr_buf, &error);

  parser = json_parser_new ();
  if (!json_parser_load_from_data (parser, stdout_str, -1, &error))
    {
      g_warning ("Failed to parse JSON: %s", error->message);
      return;
    }

  root = json_parser_get_root (parser);
  root_obj = json_node_get_object (root);
  libraries_array = json_object_get_array_member (root_obj, "installed_libraries");

  for (guint i = 0; i < json_array_get_length (libraries_array); i++)
    {
      g_autoptr (GbpArduinoLibraryInfo) library_info = NULL;
      JsonObject *library_entry = json_array_get_object_element (libraries_array, i);
      JsonObject *library = json_object_get_object_member (library_entry, "library");

      const char *name = json_object_get_string_member (library, "name");
      const char *author = json_object_get_string_member (library, "author");
      const char *description = json_object_get_string_member (library, "sentence");
      const char *version = json_object_get_string_member (library, "version");
      const char *versions[] = { version, NULL };

      library_info = gbp_arduino_library_info_new (name,
                                                   author,
                                                   description,
                                                   versions);

      g_list_store_append (new_libraries, library_info);
    }

  g_object_set (self,
                "installed-libraries", new_libraries,
                NULL);
}

static void
get_libraries (GbpArduinoApplicationAddin *self)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (IdeRunContext) run_context = NULL;
  g_autoptr (IdeSubprocessLauncher) launcher = NULL;
  g_autoptr (IdeSubprocess) subprocess = NULL;
  g_autoptr (GBytes) stdout_buf = NULL;
  g_autofree char *stdout_str = NULL;

  run_context = ide_run_context_new ();
  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);

  ide_run_context_push_host (run_context);
  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);
  ide_subprocess_launcher_push_argv (launcher, "arduino-cli");
  ide_subprocess_launcher_push_argv (launcher, "lib");
  ide_subprocess_launcher_push_argv (launcher, "list");
  ide_subprocess_launcher_push_argv (launcher, "--json");

  subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error);
  if (error != NULL)
    {
      g_warning ("Failed to launch arduino-cli: %s", error->message);
      return;
    }

  ide_subprocess_communicate_utf8_async (subprocess,
                                         NULL,
                                         NULL,
                                         get_libraries_communicated,
                                         self);
}

static void
gbp_arduino_application_addin_load (IdeApplicationAddin *addin,
                                    IdeApplication      *application)
{
  GbpArduinoApplicationAddin *self = (GbpArduinoApplicationAddin *) addin;
  g_autoptr (GError) error = NULL;
  g_autoptr (IdeRunContext) run_context = NULL;
  g_autoptr (IdeSubprocessLauncher) launcher = NULL;
  g_autoptr (IdeSubprocess) subprocess = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_ARDUINO_APPLICATION_ADDIN (self));
  g_assert (IDE_IS_APPLICATION (application));

  self->app = application;

  run_context = ide_run_context_new ();
  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);

  ide_run_context_push_host (run_context);
  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);

  ide_subprocess_launcher_push_argv (launcher, "arduino-cli");
  ide_subprocess_launcher_push_argv (launcher, "version");
  ide_subprocess_launcher_push_argv (launcher, "--json");

  subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error);
  if (error != NULL)
    {
      return;
    }

  get_platforms_and_boards (self);
  get_libraries (self);

  self->has_arduino_cli = TRUE;
}

static void
gbp_arduino_application_addin_unload (IdeApplicationAddin *addin,
                                      IdeApplication      *application)
{
  GbpArduinoApplicationAddin *self = (GbpArduinoApplicationAddin *) addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_ARDUINO_APPLICATION_ADDIN (self));
  g_assert (IDE_IS_APPLICATION (application));
}

static void
app_addin_iface_init (IdeApplicationAddinInterface *iface)
{
  iface->load = gbp_arduino_application_addin_load;
  iface->unload = gbp_arduino_application_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpArduinoApplicationAddin, gbp_arduino_application_addin, G_TYPE_OBJECT, G_IMPLEMENT_INTERFACE (IDE_TYPE_APPLICATION_ADDIN, app_addin_iface_init))

static void
gbp_arduino_application_addin_set_property (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec)
{
  GbpArduinoApplicationAddin *self = GBP_ARDUINO_APPLICATION_ADDIN (object);

  switch (prop_id)
    {
    case PROP_AVAILABLE_BOARDS:
      g_set_object (&self->available_boards, g_value_get_object (value));
      break;
    case PROP_INSTALLED_LIBRARIES:
      g_set_object (&self->installed_libraries, g_value_get_object (value));
      break;
    case PROP_INSTALLED_PLATFORMS:
      g_set_object (&self->installed_platforms, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_arduino_application_addin_get_property (GObject    *object,
                                            guint       prop_id,
                                            GValue     *value,
                                            GParamSpec *pspec)
{
  GbpArduinoApplicationAddin *self = GBP_ARDUINO_APPLICATION_ADDIN (object);

  switch (prop_id)
    {
    case PROP_AVAILABLE_BOARDS:
      g_value_set_object (value, self->available_boards);
      break;
    case PROP_INSTALLED_LIBRARIES:
      g_value_set_object (value, self->installed_libraries);
      break;
    case PROP_INSTALLED_PLATFORMS:
      g_value_set_object (value, self->installed_platforms);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

gboolean
gbp_arduino_application_addin_get_options_for_fqbn (GbpArduinoApplicationAddin *self,
                                                    const char                 *fqbn,
                                                    GListStore                **flags_out,
                                                    GListStore                **programmers_out)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (JsonParser) parser = NULL;
  g_autoptr (IdeRunContext) run_context = NULL;
  g_autoptr (IdeSubprocessLauncher) launcher = NULL;
  g_autoptr (IdeSubprocess) subprocess = NULL;
  g_autofree char *stdout_buf = NULL;
  JsonNode *root;
  JsonObject *board_obj;
  GListStore *options_store;
  GListStore *programmers_store;

  g_assert (GBP_IS_ARDUINO_APPLICATION_ADDIN (self));

  g_return_val_if_fail (fqbn != NULL, FALSE);

  run_context = ide_run_context_new ();
  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);

  ide_run_context_push_host (run_context);
  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);

  ide_subprocess_launcher_push_argv (launcher, "arduino-cli");
  ide_subprocess_launcher_push_argv (launcher, "board");
  ide_subprocess_launcher_push_argv (launcher, "details");
  ide_subprocess_launcher_push_argv (launcher, "-b");
  ide_subprocess_launcher_push_argv (launcher, fqbn);
  ide_subprocess_launcher_push_argv (launcher, "--json");

  subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error);
  if (error != NULL)
    {
      g_warning ("Failed to launch arduino-cli: %s", error->message);
      return FALSE;
    }

  if (!ide_subprocess_communicate_utf8 (subprocess, NULL, NULL, &stdout_buf, NULL, &error))
    {
      g_warning ("Failed to communicate with arduino-cli: %s", error->message);
      return FALSE;
    }

  parser = json_parser_new ();
  if (!json_parser_load_from_data (parser, stdout_buf, -1, &error))
    {
      g_warning ("Failed to parse JSON output: %s", error->message);
      return FALSE;
    }

  programmers_store = g_list_store_new (GBP_TYPE_ARDUINO_OPTION_VALUE);
  options_store = g_list_store_new (GBP_TYPE_ARDUINO_BOARD_OPTION);

  *flags_out = options_store;
  *programmers_out = programmers_store;

  root = json_parser_get_root (parser);
  board_obj = json_node_get_object (root);

  if (flags_out != NULL && json_object_has_member (board_obj, "config_options"))
    {
      JsonArray *config_options;
      guint n_options;

      config_options = json_object_get_array_member (board_obj, "config_options");
      n_options = json_array_get_length (config_options);

      for (guint i = 0; i < n_options; i++)
        {
          JsonArray *values;
          guint n_values;
          const char *option;
          const char *option_label;
          g_autoptr (GbpArduinoBoardOption) board_option = NULL;
          JsonObject *option_obj;

          option_obj = json_array_get_object_element (config_options, i);

          option = json_object_get_string_member (option_obj, "option");
          option_label = json_object_get_string_member (option_obj, "option_label");

          board_option = gbp_arduino_board_option_new (option, option_label);

          values = json_object_get_array_member (option_obj, "values");
          n_values = json_array_get_length (values);

          for (guint j = 0; j < n_values; j++)
            {
              JsonObject *value_obj = json_array_get_object_element (values, j);

              const char *value = json_object_get_string_member (value_obj, "value");
              const char *value_label = json_object_get_string_member (value_obj, "value_label");

              gbp_arduino_board_option_add_value (board_option, value, value_label);
            }

          g_list_store_append (options_store, board_option);
        }
    }

  if (programmers_out != NULL && json_object_has_member (board_obj, "programmers"))
    {
      JsonArray *programmers_array;
      guint n_programmers;

      programmers_array = json_object_get_array_member (board_obj, "programmers");
      n_programmers = json_array_get_length (programmers_array);

      for (guint i = 0; i < n_programmers; i++)
        {
          const char *value;
          const char *value_label;
          g_autoptr (GbpArduinoOptionValue) programmer_option = NULL;
          JsonObject *programmer_element;

          programmer_element = json_array_get_object_element (programmers_array, i);

          value = json_object_get_string_member (programmer_element, "id");
          value_label = json_object_get_string_member (programmer_element, "name");

          programmer_option = gbp_arduino_option_value_new (value, value_label);

          g_list_store_append (programmers_store, programmer_option);
        }
    }

  return TRUE;
}

static void
gbp_arduino_application_addin_finalize (GObject *object)
{
  GbpArduinoApplicationAddin *self = GBP_ARDUINO_APPLICATION_ADDIN (object);

  g_clear_object (&self->available_boards);
  g_clear_object (&self->installed_libraries);
  g_clear_object (&self->installed_platforms);

  G_OBJECT_CLASS (gbp_arduino_application_addin_parent_class)->finalize (object);
}

static void
gbp_arduino_application_addin_class_init (GbpArduinoApplicationAddinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_arduino_application_addin_finalize;
  object_class->set_property = gbp_arduino_application_addin_set_property;
  object_class->get_property = gbp_arduino_application_addin_get_property;

  properties[PROP_AVAILABLE_BOARDS] =
      g_param_spec_object ("available-boards", NULL, NULL,
                           G_TYPE_LIST_MODEL,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_INSTALLED_LIBRARIES] =
      g_param_spec_object ("installed-libraries", NULL, NULL,
                           G_TYPE_LIST_MODEL,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_INSTALLED_PLATFORMS] =
      g_param_spec_object ("installed-platforms", NULL, NULL,
                           G_TYPE_LIST_MODEL,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_arduino_application_addin_init (GbpArduinoApplicationAddin *self)
{
  self->has_arduino_cli = FALSE;
}

GListModel *
gbp_arduino_application_addin_get_available_boards (GbpArduinoApplicationAddin *self)
{
  g_return_val_if_fail (GBP_IS_ARDUINO_APPLICATION_ADDIN (self), NULL);

  return G_LIST_MODEL (self->available_boards);
}

GListModel *
gbp_arduino_application_addin_get_installed_libraries (GbpArduinoApplicationAddin *self)
{
  g_return_val_if_fail (GBP_IS_ARDUINO_APPLICATION_ADDIN (self), NULL);

  return G_LIST_MODEL (self->installed_libraries);
}

GListModel *
gbp_arduino_application_addin_get_installed_platforms (GbpArduinoApplicationAddin *self)
{
  g_return_val_if_fail (GBP_IS_ARDUINO_APPLICATION_ADDIN (self), NULL);

  return G_LIST_MODEL (self->installed_platforms);
}

GListStore *
gbp_arduino_application_addin_search_library (GbpArduinoApplicationAddin *self,
                                              const char                 *search_text)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (JsonParser) parser = NULL;
  GListStore *libraries_store = NULL;
  g_autoptr (IdeRunContext) run_context = NULL;
  g_autoptr (IdeSubprocessLauncher) launcher = NULL;
  g_autoptr (IdeSubprocess) subprocess = NULL;
  g_autofree char *stdout_buf = NULL;
  g_autofree char *stderr_buf = NULL;
  JsonNode *root;
  JsonObject *libraries;
  JsonArray *config_options;
  guint n_options;

  g_return_val_if_fail (search_text != NULL, NULL);

  run_context = ide_run_context_new ();
  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);
  ide_run_context_push_host (run_context);
  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);

  ide_subprocess_launcher_push_argv (launcher, "arduino-cli");
  ide_subprocess_launcher_push_argv (launcher, "lib");
  ide_subprocess_launcher_push_argv (launcher, "search");
  ide_subprocess_launcher_push_argv (launcher, search_text);
  ide_subprocess_launcher_push_argv (launcher, "--json");

  subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error);
  if (error != NULL)
    {
      g_warning ("Failed to launch arduino-cli: %s", error->message);
      return NULL;
    }

  if (!ide_subprocess_communicate_utf8 (subprocess, NULL, NULL, &stdout_buf, &stderr_buf, &error))
    {
      g_warning ("Failed to communicate with arduino-cli: %s", error->message);
      return NULL;
    }

  if (stderr_buf != NULL)
    return NULL;

  parser = json_parser_new ();
  if (!json_parser_load_from_data (parser, stdout_buf, -1, &error))
    {
      g_warning ("Failed to parse JSON output: %s", error->message);
      return NULL;
    }

  libraries_store = g_list_store_new (GBP_TYPE_ARDUINO_LIBRARY_INFO);
  root = json_parser_get_root (parser);

  if (root == NULL)
    return NULL;

  libraries = json_node_get_object (root);

  if (json_object_has_member (libraries, "libraries"))
    {
      config_options = json_object_get_array_member (libraries, "libraries");
      n_options = json_array_get_length (config_options);

      for (guint i = 0; i < n_options; i++)
        {
          JsonObject *library_obj;
          JsonObject *latest_obj;
          GbpArduinoLibraryInfo *library_info;
          JsonArray *available_versions;
          g_autofree char *name = NULL;
          g_autofree char *author = NULL;
          g_autofree char *description = NULL;
          g_autoptr (GPtrArray) versions_array = NULL;

          library_obj = json_array_get_object_element (config_options, i);

          latest_obj = json_object_get_object_member (library_obj, "latest");
          if (!latest_obj)
            continue;

          name = g_strdup (json_object_get_string_member (library_obj, "name"));
          author = g_strdup (json_object_get_string_member (latest_obj, "author"));
          description = g_strdup (json_object_get_string_member (latest_obj, "sentence"));

          available_versions = json_object_get_array_member (library_obj, "available_versions");
          if (available_versions)
            {
              guint n_versions = json_array_get_length (available_versions);
              versions_array = g_ptr_array_new_full (n_versions + 1, g_free);

              for (guint j = 0; j < n_versions; j++)
                {
                  const char *version = json_array_get_string_element (available_versions, j);
                  g_ptr_array_add (versions_array, g_strdup (version));
                }
              g_ptr_array_add (versions_array, NULL);
            }

          library_info = gbp_arduino_library_info_new (name,
                                                       author,
                                                       description,
                                                       (const char **) versions_array->pdata);

          g_list_store_append (libraries_store, library_info);
        }

      return g_steal_pointer (&libraries_store);
    }

  return NULL;
}

GListStore *
gbp_arduino_application_addin_search_platform (GbpArduinoApplicationAddin *self,
                                               const char                 *search_text)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (JsonParser) parser = NULL;
  GListStore *platforms_store = NULL;
  g_autoptr (IdeRunContext) run_context = NULL;
  g_autoptr (IdeSubprocessLauncher) launcher = NULL;
  g_autoptr (IdeSubprocess) subprocess = NULL;
  g_autofree char *stdout_buf = NULL;
  g_autofree char *stderr_buf = NULL;
  JsonNode *root;
  JsonObject *root_obj;
  JsonArray *platforms;

  g_return_val_if_fail (search_text != NULL, NULL);

  run_context = ide_run_context_new ();
  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);
  ide_run_context_push_host (run_context);
  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);

  ide_subprocess_launcher_push_argv (launcher, "arduino-cli");
  ide_subprocess_launcher_push_argv (launcher, "core");
  ide_subprocess_launcher_push_argv (launcher, "search");
  ide_subprocess_launcher_push_argv (launcher, search_text);
  ide_subprocess_launcher_push_argv (launcher, "--json");

  subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error);
  if (error != NULL)
    {
      g_warning ("Failed to launch arduino-cli: %s", error->message);
      return NULL;
    }

  if (!ide_subprocess_communicate_utf8 (subprocess, NULL, NULL, &stdout_buf, &stderr_buf, &error))
    {
      g_warning ("Failed to communicate with arduino-cli: %s", error->message);
      return NULL;
    }

  if (stderr_buf != NULL)
    return NULL;

  parser = json_parser_new ();
  if (!json_parser_load_from_data (parser, stdout_buf, -1, &error))
    {
      g_warning ("Failed to parse JSON output: %s", error->message);
      return NULL;
    }

  platforms_store = g_list_store_new (GBP_TYPE_ARDUINO_PLATFORM_INFO);
  root = json_parser_get_root (parser);

  if (root == NULL)
    return NULL;

  root_obj = json_node_get_object (root);

  if (json_object_has_member (root_obj, "platforms"))
    {
      platforms = json_object_get_array_member (root_obj, "platforms");

      for (guint i = 0; i < json_array_get_length (platforms); i++)
        {
          JsonObject *platform_obj;
          JsonObject *releases_obj;
          JsonObject *latest_release;
          GbpArduinoPlatformInfo *platform_info;
          g_autofree char *id = NULL;
          g_autofree char *maintainer = NULL;
          g_autofree char *website = NULL;
          const char *latest_version = NULL;
          g_autoptr (GPtrArray) boards_array = NULL;
          JsonArray *boards;
          guint n_boards;

          platform_obj = json_array_get_object_element (platforms, i);

          id = g_strdup (json_object_get_string_member (platform_obj, "id"));
          maintainer = g_strdup (json_object_get_string_member (platform_obj, "maintainer"));
          website = g_strdup (json_object_get_string_member (platform_obj, "website"));
          latest_version = json_object_get_string_member (platform_obj, "latest_version");

          releases_obj = json_object_get_object_member (platform_obj, "releases");

          if (ide_str_empty0 (latest_version))
            {
              g_autoptr (GList) members = json_object_get_members (releases_obj);
              if (members != NULL)
                {
                  GList *last = g_list_last (members);
                  if (last != NULL && last->data != NULL)
                    {
                      latest_version = (const char *)last->data;
                    }
                }
            }

          latest_release = json_object_get_object_member (releases_obj, latest_version);

          if (latest_release && json_object_has_member (latest_release, "boards"))
            {
              boards = json_object_get_array_member (latest_release, "boards");
              n_boards = json_array_get_length (boards);
              boards_array = g_ptr_array_new_full (n_boards + 1, g_free);

              for (guint j = 0; j < n_boards; j++)
                {
                  JsonObject *board_obj = json_array_get_object_element (boards, j);
                  const char *board_name = json_object_get_string_member (board_obj, "name");
                  g_ptr_array_add (boards_array, g_strdup (board_name));
                }
              g_ptr_array_add (boards_array, NULL);
            }

          platform_info = gbp_arduino_platform_info_new (id,
                                                         latest_version,
                                                         (const char **) boards_array->pdata,
                                                         maintainer,
                                                         id,
                                                         NULL);

          g_list_store_append (platforms_store, platform_info);
        }

      return g_steal_pointer (&platforms_store);
    }

  return NULL;
}

IdeContext *
get_current_workbench_context (GbpArduinoApplicationAddin *self)
{
  IdeWorkbench *workbench;
  GtkWindow *window;

  if (!(window = gtk_application_get_active_window (GTK_APPLICATION (self->app))))
    return NULL;

  if (!(workbench = ide_widget_get_workbench (GTK_WIDGET (window))))
    return NULL;

  return ide_workbench_get_context (workbench);
}

static void
on_library_progress_finished (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  g_autoptr (ProgressData) progress_data = user_data;

  ide_notification_withdraw (progress_data->notif);
  get_libraries (progress_data->self);
}

gboolean
gbp_arduino_application_addin_install_library (GbpArduinoApplicationAddin *self,
                                               const char                 *library_name)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (IdeRunContext) run_context = NULL;
  g_autoptr (IdeSubprocessLauncher) launcher = NULL;
  g_autoptr (IdeSubprocess) subprocess = NULL;
  g_autoptr (IdeNotification) notif = NULL;
  g_autoptr (ProgressData) progress_data = NULL;

  g_assert (GBP_IS_ARDUINO_APPLICATION_ADDIN (self));

  run_context = ide_run_context_new ();
  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);

  ide_run_context_push_host (run_context);
  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);

  ide_subprocess_launcher_push_argv (launcher, "arduino-cli");
  ide_subprocess_launcher_push_argv (launcher, "lib");
  ide_subprocess_launcher_push_argv (launcher, "install");
  ide_subprocess_launcher_push_argv (launcher, library_name);
  ide_subprocess_launcher_push_argv (launcher, "--json");

  subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error);
  if (error != NULL)
    {
      g_warning ("Failed to launch arduino-cli: %s", error->message);
      return FALSE;
    }

  notif = ide_notification_new ();
  ide_notification_set_icon_name (notif, "text-arduino-symbolic");
  ide_notification_set_title (notif, _("Installing Arduino Library"));
  ide_notification_set_body (notif, g_strconcat (_("Downloading and installing"), " ", library_name, NULL));
  ide_notification_set_has_progress (notif, TRUE);
  ide_notification_set_progress_is_imprecise (notif, TRUE);
  ide_notification_attach (notif, IDE_OBJECT (get_current_workbench_context (self)));

  progress_data = g_slice_new0 (ProgressData);
  progress_data->self = self;
  progress_data->notif = g_object_ref (notif);

  ide_subprocess_wait_async (subprocess,
                             NULL,
                             (GAsyncReadyCallback) on_library_progress_finished,
                             g_steal_pointer (&progress_data));

  return TRUE;
}

gboolean
gbp_arduino_application_addin_uninstall_library (GbpArduinoApplicationAddin *self,
                                                 const char                 *library_name)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (IdeRunContext) run_context = NULL;
  g_autoptr (IdeSubprocessLauncher) launcher = NULL;
  g_autoptr (IdeSubprocess) subprocess = NULL;
  g_autoptr (IdeNotification) notif = NULL;
  g_autoptr (ProgressData) progress_data = NULL;

  g_assert (GBP_IS_ARDUINO_APPLICATION_ADDIN (self));

  run_context = ide_run_context_new ();
  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);

  ide_run_context_push_host (run_context);
  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);

  ide_subprocess_launcher_push_argv (launcher, "arduino-cli");
  ide_subprocess_launcher_push_argv (launcher, "lib");
  ide_subprocess_launcher_push_argv (launcher, "uninstall");
  ide_subprocess_launcher_push_argv (launcher, library_name);
  ide_subprocess_launcher_push_argv (launcher, "--json");

  subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error);
  if (error != NULL)
    {
      g_warning ("Failed to launch arduino-cli: %s", error->message);
      return FALSE;
    }

  notif = ide_notification_new ();
  ide_notification_set_icon_name (notif, "text-arduino-symbolic");
  ide_notification_set_title (notif, _("Uninstalling Arduino Library"));
  ide_notification_set_body (notif, g_strconcat (_("Uninstalling"), " ", library_name, NULL));
  ide_notification_set_has_progress (notif, TRUE);
  ide_notification_set_progress_is_imprecise (notif, TRUE);
  ide_notification_attach (notif, IDE_OBJECT (get_current_workbench_context (self)));

  progress_data = g_slice_new0 (ProgressData);
  progress_data->self = self;
  progress_data->notif = g_object_ref (notif);

  ide_subprocess_wait_async (subprocess,
                             NULL,
                             (GAsyncReadyCallback) on_library_progress_finished,
                             g_steal_pointer (&progress_data));

  return TRUE;
}

static void
on_platform_progress_finished (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  g_autoptr (ProgressData) progress_data = user_data;

  ide_notification_withdraw (progress_data->notif);
  get_platforms_and_boards (progress_data->self);
}

gboolean
gbp_arduino_application_addin_install_platform (GbpArduinoApplicationAddin *self,
                                                const char                 *platform_name)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (IdeRunContext) run_context = NULL;
  g_autoptr (IdeSubprocessLauncher) launcher = NULL;
  g_autoptr (IdeSubprocess) subprocess = NULL;
  g_autoptr (IdeNotification) notif = NULL;
  g_autoptr (ProgressData) progress_data = NULL;

  g_assert (GBP_IS_ARDUINO_APPLICATION_ADDIN (self));

  run_context = ide_run_context_new ();
  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);

  ide_run_context_push_host (run_context);
  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);

  ide_subprocess_launcher_push_argv (launcher, "arduino-cli");
  ide_subprocess_launcher_push_argv (launcher, "core");
  ide_subprocess_launcher_push_argv (launcher, "install");
  ide_subprocess_launcher_push_argv (launcher, platform_name);
  ide_subprocess_launcher_push_argv (launcher, "--json");

  subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error);
  if (error != NULL)
    {
      g_warning ("Failed to launch arduino-cli: %s", error->message);
      return FALSE;
    }

  notif = ide_notification_new ();
  ide_notification_set_icon_name (notif, "text-arduino-symbolic");
  ide_notification_set_title (notif, _("Installing Arduino Platform"));
  ide_notification_set_body (notif, g_strconcat (_("Downloading and installing"), " ", platform_name, NULL));
  ide_notification_set_has_progress (notif, TRUE);
  ide_notification_set_progress_is_imprecise (notif, TRUE);
  ide_notification_attach (notif, IDE_OBJECT (get_current_workbench_context (self)));

  progress_data = g_slice_new0 (ProgressData);
  progress_data->self = self;
  progress_data->notif = g_object_ref (notif);

  ide_subprocess_wait_async (subprocess,
                             NULL,
                             (GAsyncReadyCallback) on_platform_progress_finished,
                             g_steal_pointer (&progress_data));
  return TRUE;
}

gboolean
gbp_arduino_application_addin_uninstall_platform (GbpArduinoApplicationAddin *self,
                                                  const char                 *platform_name)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (IdeRunContext) run_context = NULL;
  g_autoptr (IdeSubprocessLauncher) launcher = NULL;
  g_autoptr (IdeSubprocess) subprocess = NULL;
  g_autoptr (IdeNotification) notif = NULL;
  g_autoptr (ProgressData) progress_data = NULL;

  g_assert (GBP_IS_ARDUINO_APPLICATION_ADDIN (self));

  run_context = ide_run_context_new ();
  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);

  ide_run_context_push_host (run_context);
  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);

  ide_subprocess_launcher_push_argv (launcher, "arduino-cli");
  ide_subprocess_launcher_push_argv (launcher, "core");
  ide_subprocess_launcher_push_argv (launcher, "uninstall");
  ide_subprocess_launcher_push_argv (launcher, platform_name);
  ide_subprocess_launcher_push_argv (launcher, "--json");

  subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error);
  if (error != NULL)
    {
      g_warning ("Failed to launch arduino-cli: %s", error->message);
      return FALSE;
    }

  notif = ide_notification_new ();
  ide_notification_set_icon_name (notif, "text-arduino-symbolic");
  ide_notification_set_title (notif, _("Uninstalling Arduino Platform"));
  ide_notification_set_body (notif, g_strconcat (_("Uninstalling"), " ", platform_name, NULL));
  ide_notification_set_has_progress (notif, TRUE);
  ide_notification_set_progress_is_imprecise (notif, TRUE);
  ide_notification_attach (notif, IDE_OBJECT (get_current_workbench_context (self)));

  progress_data = g_slice_new0 (ProgressData);
  progress_data->self = self;
  progress_data->notif = g_object_ref (notif);

  ide_subprocess_wait_async (subprocess,
                             NULL,
                             (GAsyncReadyCallback) on_platform_progress_finished,
                             g_steal_pointer (&progress_data));

  return TRUE;
}

const char * const *
gbp_arduino_application_addin_get_additional_urls (GbpArduinoApplicationAddin *self)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (JsonParser) parser = NULL;
  g_autoptr (IdeRunContext) run_context = NULL;
  g_autoptr (IdeSubprocessLauncher) launcher = NULL;
  g_autoptr (IdeSubprocess) subprocess = NULL;
  g_autofree char *stdout_buf = NULL;
  JsonNode *root;
  JsonArray *urls_array;
  char **urls = NULL;

  run_context = ide_run_context_new ();
  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);

  ide_run_context_push_host (run_context);
  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);

  ide_subprocess_launcher_push_argv (launcher, "arduino-cli");
  ide_subprocess_launcher_push_argv (launcher, "config");
  ide_subprocess_launcher_push_argv (launcher, "get");
  ide_subprocess_launcher_push_argv (launcher, "board_manager.additional_urls");
  ide_subprocess_launcher_push_argv (launcher, "--json");

  subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error);
  if (error != NULL)
    {
      g_warning ("Failed to launch arduino-cli: %s", error->message);
      return NULL;
    }

  if (!ide_subprocess_communicate_utf8 (subprocess, NULL, NULL, &stdout_buf, NULL, &error))
    {
      g_warning ("Failed to communicate with arduino-cli: %s", error->message);
      return NULL;
    }

  parser = json_parser_new ();
  if (!json_parser_load_from_data (parser, stdout_buf, -1, &error))
    {
      g_warning ("Failed to parse JSON output: %s", error->message);
      return NULL;
    }

  root = json_parser_get_root (parser);
  urls_array = json_node_get_array (root);

  if (urls_array != NULL)
    {
      guint n_urls = json_array_get_length (urls_array);

      for (guint i = 0; i < n_urls; i++)
        {
          const char *url = json_array_get_string_element (urls_array, i);
          ide_strv_add_to_set (&urls, g_strdup (url));
        }
    }

  return (const char * const *) urls;
}

gboolean
gbp_arduino_application_addin_add_additional_url (GbpArduinoApplicationAddin *self,
                                                  const char                 *new_url)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (IdeRunContext) run_context = NULL;
  g_autoptr (IdeSubprocessLauncher) launcher = NULL;
  g_autoptr (IdeSubprocess) subprocess = NULL;

  run_context = ide_run_context_new ();
  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);

  ide_run_context_push_host (run_context);
  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);

  ide_subprocess_launcher_push_argv (launcher, "arduino-cli");
  ide_subprocess_launcher_push_argv (launcher, "config");
  ide_subprocess_launcher_push_argv (launcher, "add");
  ide_subprocess_launcher_push_argv (launcher, "board_manager.additional_urls");
  ide_subprocess_launcher_push_argv (launcher, new_url);

  subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error);
  if (error != NULL)
    {
      g_warning ("Failed to launch arduino-cli: %s", error->message);
      return FALSE;
    }

  return TRUE;
}

gboolean
gbp_arduino_application_addin_remove_additional_url (GbpArduinoApplicationAddin *self,
                                                     const char                 *url_to_remove)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (IdeRunContext) run_context = NULL;
  g_autoptr (IdeSubprocessLauncher) launcher = NULL;
  g_autoptr (IdeSubprocess) subprocess = NULL;

  run_context = ide_run_context_new ();
  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);

  ide_run_context_push_host (run_context);
  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);

  ide_subprocess_launcher_push_argv (launcher, "arduino-cli");
  ide_subprocess_launcher_push_argv (launcher, "config");
  ide_subprocess_launcher_push_argv (launcher, "remove");
  ide_subprocess_launcher_push_argv (launcher, "board_manager.additional_urls");
  ide_subprocess_launcher_push_argv (launcher, url_to_remove);

  subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error);
  if (error != NULL)
    {
      g_warning ("Failed to launch arduino-cli: %s", error->message);
      return FALSE;
    }

  return TRUE;
}

gboolean
gbp_arduino_application_addin_has_arduino_cli (GbpArduinoApplicationAddin *self)
{
  return self->has_arduino_cli;
}
