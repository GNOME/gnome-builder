/* ide-application-command-line.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "ide-application-command-line"

#include "config.h"

#include <stdlib.h>

#include <glib/gi18n.h>

#include <libide-core.h>

#include "ide-application-addin.h"
#include "ide-application-private.h"
#include "ide-primary-workspace.h"

static void
add_option_entries_foreach_cb (PeasExtensionSet *set,
                               PeasPluginInfo   *plugin_info,
                               GObject    *exten,
                               gpointer          user_data)
{
  IdeApplicationAddin *addin = (IdeApplicationAddin *)exten;
  IdeApplication *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_APPLICATION_ADDIN (addin));
  g_assert (IDE_IS_APPLICATION (self));

  ide_application_addin_add_option_entries (addin, self);
}

/**
 * _ide_application_add_option_entries:
 *
 * Inflate all early stage plugins asking them to let us know about what
 * command-line options they support.
 */
void
_ide_application_add_option_entries (IdeApplication *self)
{
  static const GOptionEntry main_entries[] = {
    { "preferences", 0, 0, G_OPTION_ARG_NONE, NULL, N_("Show the application preferences") },
    { "project", 'p', 0, G_OPTION_ARG_FILENAME, NULL, N_("Open project in new workbench"), N_("FILE")  },

    /* The following are handled in main(), but needed here so that --help
     * will display them to the user (which is handled later on).
     */
    { "version", 'V', 0, G_OPTION_ARG_NONE, NULL, N_("Print version information and exit") },
    { "verbose", 'v', 0, G_OPTION_ARG_NONE, NULL, N_("Increase log verbosity (may be repeated)") },
    { NULL }
  };

  g_assert (IDE_IS_APPLICATION (self));

  g_application_add_main_option_entries (G_APPLICATION (self), main_entries);
  peas_extension_set_foreach (self->addins, add_option_entries_foreach_cb, self);
}

static void
command_line_foreach_cb (PeasExtensionSet *set,
                         PeasPluginInfo   *plugin_info,
                         GObject    *exten,
                         gpointer          user_data)
{
  IdeApplicationAddin *addin = (IdeApplicationAddin *)exten;
  GApplicationCommandLine *cmdline = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_APPLICATION_ADDIN (addin));
  g_assert (G_IS_APPLICATION_COMMAND_LINE (cmdline));

  /* Stop if we've already handled things */
  if (ide_application_get_command_line_handled (IDE_APPLICATION_DEFAULT, cmdline))
    return;

  ide_application_addin_handle_command_line (addin, IDE_APPLICATION_DEFAULT, cmdline);
}

static void
ide_application_command_line_open_project_cb (GObject      *object,
                                              GAsyncResult *result,
                                              gpointer      user_data)
{
  IdeApplication *app = (IdeApplication *)object;
  g_autoptr(GApplicationCommandLine) cmdline = user_data;
  g_autoptr(IdeWorkbench) workbench = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_APPLICATION (app));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_APPLICATION_COMMAND_LINE (cmdline));

  g_application_release (G_APPLICATION (app));

  if (!(workbench = ide_application_open_project_finish (app, result, &error)))
    {
      g_application_command_line_printerr (cmdline,
                                           _("Failed to open project: %s"),
                                           error->message);
      return;
    }

  g_application_command_line_set_exit_status (cmdline, workbench ? EXIT_SUCCESS : EXIT_FAILURE);
}

/**
 * _ide_application_command_line:
 *
 * This function will dispatch the command-line to the various
 * plugins who have elected to handle command-line options. Some
 * of them, like the greeter, may create an initial workbench
 * and workspace window in response.
 */
void
_ide_application_command_line (IdeApplication          *self,
                               GApplicationCommandLine *cmdline)
{
  g_autofree gchar *project = NULL;
  GVariantDict *dict;

  g_assert (IDE_IS_APPLICATION (self));
  g_assert (G_IS_APPLICATION_COMMAND_LINE (cmdline));

  dict = g_application_command_line_get_options_dict (cmdline);

  /* Short-circuit with --preferences if we can */
  if (g_variant_dict_contains (dict, "preferences"))
    {
      g_action_group_activate_action (G_ACTION_GROUP (self), "preferences", NULL);
      return;
    }

  /*
   * Allow any plugin that has registered a command-line handler to
   * handle the command-line options. They may return an exit status
   * in the process of our iteration, at which point we shoudl bail
   * any furter processings.
   *
   * This is done before -p/--project parsing so that options may be
   * changed before loading a project.
   */
  peas_extension_set_foreach (self->addins, command_line_foreach_cb, cmdline);

  /* Short-circuit if there is nothing more to do */
  if (ide_application_get_command_line_handled (self, cmdline))
    return;

  /*
   * Open the project if --project/-p was spefified by the invoking
   * processes command-line.
   */
  if (g_variant_dict_lookup (dict, "project", "^ay", &project))
    {
      g_autoptr(IdeProjectInfo) project_info = NULL;
      g_autoptr(GFile) project_file = NULL;
      g_autoptr(GFile) parent = NULL;

      project_file = g_application_command_line_create_file_for_arg (cmdline, project);
      parent = g_file_get_parent (project_file);

      project_info = ide_project_info_new ();
      ide_project_info_set_file (project_info, project_file);

      /* If it's a directory, set that too, otherwise use the parent */
      if (g_file_query_file_type (project_file, 0, NULL) == G_FILE_TYPE_DIRECTORY)
        ide_project_info_set_directory (project_info, project_file);
      else
        ide_project_info_set_directory (project_info, parent);

      g_application_hold (G_APPLICATION (self));

      ide_application_open_project_async (self,
                                          project_info,
                                          G_TYPE_INVALID,
                                          NULL,
                                          ide_application_command_line_open_project_cb,
                                          g_object_ref (cmdline));

      return;
    }

  g_application_activate (G_APPLICATION (self));
}

/**
 * ide_application_get_argv:
 * @self: an #IdeApplication
 * @cmdline: a #GApplicationCommandLine
 *
 * Gets the commandline for @cmdline as it was before any processing.
 * This is useful to handle both local and remote processing of argv
 * when you need to know what the arguments were before further
 * options parsing.
 *
 * Returns: (transfer full) (nullable) (array zero-terminated=1): an
 *   array of strings or %NULL
 */
gchar **
ide_application_get_argv (IdeApplication          *self,
                          GApplicationCommandLine *cmdline)
{
  g_autoptr(GVariant) ret = NULL;
  GVariant *platform_data;

  g_return_val_if_fail (IDE_IS_APPLICATION (self), NULL);
  g_return_val_if_fail (G_IS_APPLICATION_COMMAND_LINE (cmdline), NULL);

  if (!g_application_command_line_get_is_remote (cmdline))
    return g_strdupv (self->argv);

  if (!(platform_data = g_application_command_line_get_platform_data (cmdline)))
    return NULL;

  if ((ret = g_variant_lookup_value (platform_data, "argv", G_VARIANT_TYPE_STRING_ARRAY)))
    return g_variant_dup_strv (ret, NULL);

  return NULL;
}
