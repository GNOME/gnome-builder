/* ide-application-command-line.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-application-command-line"

#include "config.h"

#include <glib/gi18n.h>
#include <girepository.h>
#include <libpeas/peas.h>
#include <stdlib.h>
#include <stdio.h>

#include "application/ide-application.h"
#include "application/ide-application-private.h"
#include "logging/ide-log.h"

static PeasPluginInfo *
ide_application_locate_tool (IdeApplication *self,
                             const gchar    *tool_name)
{
  PeasEngine *engine;
  const GList *list;

  g_assert (IDE_IS_APPLICATION (self));
  g_assert (tool_name != NULL);

  engine = peas_engine_get_default ();
  list = peas_engine_get_plugin_list (engine);

  for (; list != NULL; list = list->next)
    {
      PeasPluginInfo *plugin_info = list->data;
      const gchar *name;

      name = peas_plugin_info_get_external_data (plugin_info, "Tool-Name");
      if (g_strcmp0 (name, tool_name) == 0)
        return plugin_info;
    }

  return NULL;
}

static PeasPluginInfo *
ide_application_locate_worker (IdeApplication *self,
                               const gchar    *worker_name)
{
  PeasEngine *engine;
  const GList *list;

  g_assert (IDE_IS_APPLICATION (self));
  g_assert (worker_name != NULL);

  engine = peas_engine_get_default ();
  list = peas_engine_get_plugin_list (engine);

  for (; list != NULL; list = list->next)
    {
      PeasPluginInfo *plugin_info = list->data;
      const gchar *name;

      name = peas_plugin_info_get_module_name (plugin_info);
      if (g_strcmp0 (name, worker_name) == 0)
        return plugin_info;
    }

  return NULL;
}

static gchar *
ide_application_get_command_help (IdeApplication *self,
                                  gboolean        long_form)
{
  PeasEngine *engine;
  const GList *list;
  GString *str;
  gint count = 0;

  g_assert (IDE_IS_APPLICATION (self));

  engine = peas_engine_get_default ();
  list = peas_engine_get_plugin_list (engine);

  str = g_string_new (NULL);

  if (long_form)
    g_string_append_printf (str, "%s\n", _("Commands:"));

  for (; list != NULL; list = list->next)
    {
      PeasPluginInfo *plugin_info = list->data;
      const gchar *name;
      const gchar *desc;

      name = peas_plugin_info_get_external_data (plugin_info, "Tool-Name");
      desc = peas_plugin_info_get_external_data (plugin_info, "Tool-Description");

      if (name != NULL)
        {
          if (long_form)
            g_string_append_printf (str, "  %-25s %s\n", name, desc);
          else
            g_string_append_printf (str, "%s\n", name);

          count++;
        }
    }

  if (count == 0)
    {
      g_string_free (str, TRUE);
      return NULL;
    }

  return g_strstrip (g_string_free (str, FALSE));
}

static gboolean
ide_application_increase_verbosity (void)
{
  /* handled during early init */
  return TRUE;
}

static gboolean
application_service_timeout_cb (gpointer data)
{
  g_autoptr(IdeApplication) self = data;

  g_assert (IDE_IS_APPLICATION (self));

  /*
   * We have a reference and a hold on the #IdeApplication as we are waiting
   * for operations to be received via DBus. If we got any requests, for
   * something like Activate(), we'll already have another hold on the
   * application for the window. Therefore, all we should need to do is drop
   * the application hold we took before registering our timeout.
   */
  g_application_release (G_APPLICATION (self));

  return G_SOURCE_REMOVE;
}

gboolean
ide_application_local_command_line (GApplication   *application,
                                    gchar        ***arguments,
                                    gint           *exit_status)
{
  IdeApplication *self = (IdeApplication *)application;
  g_autofree gchar *path_copy = NULL;
  g_autofree gchar *filename = NULL;
  g_autofree gchar *manifest = NULL;
  GOptionContext *context = NULL;
  GOptionGroup *group;
  const gchar *shortdesc = NULL;
  const gchar *prgname;
  GError *error = NULL;
  gchar *type = NULL;
  gchar *dbus_address = NULL;
  gboolean standalone = FALSE;
  gboolean version = FALSE;
  gboolean list_commands = FALSE;
  gboolean gapplication_service = FALSE;

  GOptionEntry entries[] = {
    /* keep list-commands as first entry */
    { "list-commands",
      0,
      G_OPTION_FLAG_HIDDEN,
      G_OPTION_ARG_NONE,
      &list_commands,
      N_("List available subcommands") },

    { "standalone",
      's',
      G_OPTION_FLAG_NONE,
      G_OPTION_ARG_NONE,
      &standalone,
      N_("Run Builder in standalone mode") },

    { "version",
      'V',
      G_OPTION_FLAG_NONE,
      G_OPTION_ARG_NONE,
      &version,
      N_("Show the application's version") },

    { "type",
      0,
      G_OPTION_FLAG_HIDDEN,
      G_OPTION_ARG_STRING,
      &type },

    { "dbus-address",
      0,
      G_OPTION_FLAG_HIDDEN,
      G_OPTION_ARG_STRING,
      &dbus_address},

    { "verbose",
      'v',
      G_OPTION_FLAG_NO_ARG | G_OPTION_FLAG_IN_MAIN,
      G_OPTION_ARG_CALLBACK,
      ide_application_increase_verbosity,
      N_("Increase verbosity, may be specified multiple times") },

    { "gapplication-service",
      0,
      G_OPTION_FLAG_NONE,
      G_OPTION_ARG_NONE,
      &gapplication_service,
      N_("Enter GApplication Service mode") },

    { "project",
      'p',
      G_OPTION_FLAG_IN_MAIN,
      G_OPTION_ARG_FILENAME,
      &filename,
      N_("Opens the project specified by PATH"),
      N_("PATH") },

    { "manifest",
      'm',
      G_OPTION_FLAG_IN_MAIN,
      G_OPTION_ARG_FILENAME,
      &manifest,
      N_("Clones the project specified by MANIFEST"),
      N_("MANIFEST") },

    { NULL }
  };

  g_assert (IDE_IS_APPLICATION (self));
  g_assert (arguments != NULL);
  g_assert (exit_status != NULL);

  *exit_status = EXIT_SUCCESS;

  prgname = g_get_prgname ();

  /*
   * Sometimes we can get a path like "/foo/bar/lt-test-foo"
   * and this let's us strip it to lt-test-foo.
   */
  if (g_path_is_absolute (prgname))
    {
      path_copy = g_path_get_basename (prgname);
      prgname = path_copy;
    }

  if (prgname && g_str_has_prefix (prgname, "lt-"))
    prgname += strlen ("lt-");

  if (g_str_equal (prgname, "gnome-builder-cli"))
    {
      g_assert_cmpstr (entries [0].long_name, ==, "list-commands");
      entries [0].flags = 0;
      shortdesc = _("COMMAND");
    }

  context = g_option_context_new (shortdesc);

  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);

  group = gtk_get_option_group (TRUE);
  g_option_context_add_group (context, group);

  group = g_irepository_get_option_group ();
  g_option_context_add_group (context, group);

  ide_application_discover_plugins (self);

  /*
   * If we are the "cli" program, then we want to setup ourselves for
   * verb style commands and add a commands group for help.
   */
  if (g_str_equal (prgname, "gnome-builder-cli"))
    {
      gchar *command_help;

      self->mode = IDE_APPLICATION_MODE_TOOL;

      g_option_context_set_strict_posix (context, TRUE);

      command_help = ide_application_get_command_help (self, TRUE);
      g_option_context_set_summary (context, command_help);
      g_free (command_help);
    }
  else if (g_str_equal (prgname, "gnome-builder-worker"))
    {
      self->mode = IDE_APPLICATION_MODE_WORKER;
    }
  else if (g_str_has_prefix (prgname, "test-"))
    {
      self->mode = IDE_APPLICATION_MODE_TESTS;

      if (!g_test_initialized ())
        {
          g_error ("Attempt to start IdeApplication in test mode, "
                   "but g_test_init() has not been called.");
        }
    }
  else if (gapplication_service)
    {
      GApplicationFlags flags;

      flags = g_application_get_flags (application);
      flags |= G_APPLICATION_IS_SERVICE;

      g_application_set_flags (application, flags);
    }

  /* Only the primary instance can be a --gapplication-service */
  if (self->mode != IDE_APPLICATION_MODE_PRIMARY)
    gapplication_service = FALSE;

  if (!g_option_context_parse_strv (context, arguments, &error))
    {
      g_printerr ("%s\n", error->message);
      *exit_status = EXIT_FAILURE;
      goto cleanup;
    }

  if (list_commands)
    {
      gchar *command_help;

      command_help = ide_application_get_command_help (self, FALSE);
      g_print ("%s\n", command_help ?: _("No commands available"));
      g_free (command_help);

      *exit_status = 0;
      goto cleanup;
    }

  if (standalone || (self->mode != IDE_APPLICATION_MODE_PRIMARY))
    {
      GApplicationFlags flags;

      flags = g_application_get_flags (application);
      flags |= G_APPLICATION_NON_UNIQUE;
      g_application_set_flags (application, flags);
    }

  if (version)
    {
      g_print (PACKAGE_STRING"\n");
      *exit_status = EXIT_SUCCESS;
      goto cleanup;
    }

  if (self->mode == IDE_APPLICATION_MODE_TOOL)
    {
      PeasPluginInfo *tool_plugin;
      const gchar *tool_name;

      if (g_strv_length (*arguments) < 2)
        {
          g_printerr ("%s\n", _("Please provide a command"));
          *exit_status = EXIT_FAILURE;
          goto cleanup;
        }

      tool_name = (*arguments) [1];
      tool_plugin = ide_application_locate_tool (self, tool_name);

      if (tool_plugin == NULL)
        {
          g_printerr ("%s: \"%s\"\n", _("No such tool"), tool_name);
          *exit_status = EXIT_FAILURE;
          goto cleanup;
        }

      self->tool = tool_plugin;
      self->tool_arguments = g_strdupv (*arguments);
    }
  else if (self->mode == IDE_APPLICATION_MODE_WORKER)
    {
      PeasPluginInfo *worker_plugin;

      if (type == NULL)
        {
          g_printerr ("%s\n", _("Please provide a worker type"));
          *exit_status = EXIT_FAILURE;
          goto cleanup;
        }

      if (dbus_address== NULL)
        {
          g_printerr ("%s\n", _("Please provide a D-Bus address"));
          *exit_status = EXIT_FAILURE;
          goto cleanup;
        }

      worker_plugin = ide_application_locate_worker (self, type);

      if (worker_plugin == NULL)
        {
          g_printerr ("%s: \"%s\"\n", _("No such worker"), type);
          *exit_status = EXIT_FAILURE;
          goto cleanup;
        }

      self->worker = worker_plugin;
      self->dbus_address = g_strdup (dbus_address);
    }

  ide_application_load_plugins (self);

  if (!g_application_register (application, NULL, &error))
    {
      g_printerr ("%s\n", error->message);
      *exit_status = EXIT_FAILURE;
      goto cleanup;
    }

  if (self->mode == IDE_APPLICATION_MODE_PRIMARY)
    {
      g_autoptr(GPtrArray) files = NULL;
      gint i;

      files = g_ptr_array_new_with_free_func (g_object_unref);

      for (i = 1; (*arguments) [i]; i++)
        {
          GFile *file;

          file = g_file_new_for_commandline_arg ((*arguments) [i]);
          if (file != NULL)
            g_ptr_array_add (files, file);
        }

      if (files->len > 0)
        {
          g_application_open (application, (GFile **)files->pdata, files->len, "");
          goto cleanup;
        }
    }

  if (gapplication_service)
    {
      g_application_hold (G_APPLICATION (self));
      g_timeout_add_seconds (10, application_service_timeout_cb, g_object_ref (self));
      goto cleanup;
    }

  if (filename != NULL)
    {
      GVariant *file;
      file = g_variant_new ("s", filename);
      g_action_group_activate_action ((GActionGroup *) application, "load-project", file);
      goto cleanup;
    }

  if (manifest != NULL)
    {
      GVariant *file;
      file = g_variant_new ("s", manifest);
      g_action_group_activate_action ((GActionGroup *) application, "load-flatpak", file);
      goto cleanup;
    }

  g_application_activate (application);

cleanup:
  g_clear_pointer (&type, g_free);
  g_clear_pointer (&dbus_address, g_free);
  g_clear_error (&error);
  g_option_context_free (context);

  return TRUE;
}
