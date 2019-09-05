/* gbp-terminal-application-addin.c
 *
 * Copyright 2018 Christian Hergert <unknown@domain.org>
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

#define G_LOG_DOMAIN "gbp-terminal-application-addin"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-gui.h>
#include <libide-terminal.h>

#include "gbp-terminal-application-addin.h"

struct _GbpTerminalApplicationAddin
{
  GObject parent_instance;
};

static void
gbp_terminal_application_addin_handle_command_line (IdeApplicationAddin     *addin,
                                                    IdeApplication          *application,
                                                    GApplicationCommandLine *cmdline)
{
  g_autoptr(IdeWorkbench) workbench = NULL;
  IdeApplication *app = (IdeApplication *)application;
  g_autoptr(GFile) workdir = NULL;
  g_auto(GStrv) argv = NULL;
  GVariantDict *options;
  gint argc;

  g_assert (IDE_IS_APPLICATION_ADDIN (addin));
  g_assert (IDE_IS_APPLICATION (application));
  g_assert (G_IS_APPLICATION_COMMAND_LINE (cmdline));

  argv = g_application_command_line_get_arguments (cmdline, &argc);

  if ((options = g_application_command_line_get_options_dict (cmdline)) &&
      g_variant_dict_contains (options, "terminal"))
    {
      ide_application_set_workspace_type (application, IDE_TYPE_TERMINAL_WORKSPACE);

      /* Just open the editor workspace if no files were specified */
      if (argc < 2)
        {
          IdeTerminalWorkspace *workspace;
          IdeContext *context;

          workdir = g_application_command_line_create_file_for_arg (cmdline, ".");
          ide_application_set_command_line_handled (application, cmdline, TRUE);

          workbench = ide_workbench_new ();
          ide_application_add_workbench (app, workbench);

          context = ide_workbench_get_context (workbench);
          ide_context_set_workdir (context, workdir);

          workspace = ide_terminal_workspace_new (application);
          ide_workbench_add_workspace (workbench, IDE_WORKSPACE (workspace));

          ide_workbench_focus_workspace (workbench, IDE_WORKSPACE (workspace));

          return;
        }
    }
}

static void
gbp_terminal_application_addin_add_option_entries (IdeApplicationAddin *addin,
                                                   IdeApplication      *app)
{
  g_assert (GBP_IS_TERMINAL_APPLICATION_ADDIN (addin));
  g_assert (G_IS_APPLICATION (app));

#if 0
  g_application_add_main_option (G_APPLICATION (app),
                                 "terminal",
                                 't',
                                 G_OPTION_FLAG_IN_MAIN,
                                 G_OPTION_ARG_NONE,
                                 _("Use terminal interface"),
                                 NULL);
#endif
}

static void
application_addin_iface_init (IdeApplicationAddinInterface *iface)
{
  iface->add_option_entries = gbp_terminal_application_addin_add_option_entries;
  iface->handle_command_line = gbp_terminal_application_addin_handle_command_line;
}

G_DEFINE_TYPE_WITH_CODE (GbpTerminalApplicationAddin, gbp_terminal_application_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_APPLICATION_ADDIN,
                                                application_addin_iface_init))

static void
gbp_terminal_application_addin_class_init (GbpTerminalApplicationAddinClass *klass)
{
}

static void
gbp_terminal_application_addin_init (GbpTerminalApplicationAddin *self)
{
}
