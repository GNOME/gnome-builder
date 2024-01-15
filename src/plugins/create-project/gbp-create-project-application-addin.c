/* gbp-create-project-application-addin.c
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

#define G_LOG_DOMAIN "gbp-create-project-application-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-greeter.h>
#include <libide-gui.h>

#include "gbp-create-project-application-addin.h"

struct _GbpCreateProjectApplicationAddin
{
  GObject parent_instance;
};

static void
gbp_create_project_application_addin_add_option_entries (IdeApplicationAddin *addin,
                                                         IdeApplication      *app)
{
  g_assert (IDE_IS_APPLICATION_ADDIN (addin));
  g_assert (G_IS_APPLICATION (app));

  g_application_add_main_option (G_APPLICATION (app),
                                 "create-project",
                                 0,
                                 G_OPTION_FLAG_IN_MAIN,
                                 G_OPTION_ARG_NONE,
                                 _("Display the project creation guide"),
                                 NULL);
}

static void
gbp_create_project_application_addin_handle_command_line (IdeApplicationAddin     *addin,
                                                          IdeApplication          *application,
                                                          GApplicationCommandLine *cmdline)
{
  GVariantDict *dict;

  g_assert (IDE_IS_APPLICATION_ADDIN (addin));
  g_assert (IDE_IS_APPLICATION (application));
  g_assert (G_IS_APPLICATION_COMMAND_LINE (cmdline));

  dict = g_application_command_line_get_options_dict (cmdline);

  /*
   * If we are processing the arguments for the startup of the primary
   * instance, then we want to show the greeter if no arguments are
   * provided. (That means argc == 1, the programe executable).
   *
   * Also, if they provided --greeter or -g we'll show a new greeter.
   */
  if (g_variant_dict_contains (dict, "create-project"))
    {
      g_autoptr(IdeWorkbench) workbench = NULL;
      IdeGreeterWorkspace *workspace;
      IdeApplication *app = IDE_APPLICATION (application);

      workbench = ide_workbench_new ();
      ide_application_add_workbench (app, workbench);

      workspace = ide_greeter_workspace_new (app);
      ide_workbench_add_workspace (workbench, IDE_WORKSPACE (workspace));

      ide_greeter_workspace_push_page_by_tag (workspace, "create-project");
      ide_workbench_focus_workspace (workbench, IDE_WORKSPACE (workspace));
    }
}

static void
create_project_cb (GSimpleAction *action,
                   GVariant      *param,
                   gpointer       user_data)
{
  g_autoptr(IdeWorkbench) workbench = NULL;
  IdeGreeterWorkspace *workspace;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_APPLICATION_ADDIN (user_data));

  workbench = ide_workbench_new ();
  ide_application_add_workbench (IDE_APPLICATION_DEFAULT, workbench);

  workspace = ide_greeter_workspace_new (IDE_APPLICATION_DEFAULT);
  ide_workbench_add_workspace (workbench, IDE_WORKSPACE (workspace));

  ide_greeter_workspace_push_page_by_tag (workspace, "create-project");
  ide_workbench_focus_workspace (workbench, IDE_WORKSPACE (workspace));
}

static const GActionEntry actions[] = {
  { "create-project", create_project_cb },
};

static void
gbp_create_project_application_load (IdeApplicationAddin *addin,
                                     IdeApplication      *application)
{

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_APPLICATION_ADDIN (addin));
  g_assert (IDE_IS_APPLICATION (application));

  g_action_map_add_action_entries (G_ACTION_MAP (application),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   addin);
}

static void
gbp_create_project_application_unload (IdeApplicationAddin *addin,
                                       IdeApplication      *application)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_APPLICATION_ADDIN (addin));
  g_assert (IDE_IS_APPLICATION (application));

  for (guint i = 0; i < G_N_ELEMENTS (actions); i++)
    g_action_map_remove_action (G_ACTION_MAP (application), actions[i].name);
}

static void
cmdline_addin_iface_init (IdeApplicationAddinInterface *iface)
{
  iface->load = gbp_create_project_application_load;
  iface->unload = gbp_create_project_application_unload;
  iface->add_option_entries = gbp_create_project_application_addin_add_option_entries;
  iface->handle_command_line = gbp_create_project_application_addin_handle_command_line;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpCreateProjectApplicationAddin, gbp_create_project_application_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_APPLICATION_ADDIN, cmdline_addin_iface_init))

static void
gbp_create_project_application_addin_class_init (GbpCreateProjectApplicationAddinClass *klass)
{
}

static void
gbp_create_project_application_addin_init (GbpCreateProjectApplicationAddin *self)
{
}
