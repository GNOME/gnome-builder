/* gbp-dspy-application-addin.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-dspy-application-addin"

#include "config.h"

#include <glib/gi18n.h>

#include "gbp-dspy-application-addin.h"
#include "gbp-dspy-workspace.h"

struct _GbpDspyApplicationAddin
{
  GObject parent_instance;
};

static void
gbp_dspy_application_addin_add_option_entries (IdeApplicationAddin *addin,
                                               IdeApplication      *app)
{
  g_assert (GBP_IS_DSPY_APPLICATION_ADDIN (addin));
  g_assert (G_IS_APPLICATION (app));

  g_application_add_main_option (G_APPLICATION (app),
                                 "dspy",
                                 0,
                                 G_OPTION_FLAG_IN_MAIN,
                                 G_OPTION_ARG_NONE,
                                 _("Display D-Bus inspector"),
                                 NULL);
}

static void
gbp_dspy_application_addin_handle_command_line (IdeApplicationAddin     *addin,
                                                IdeApplication          *application,
                                                GApplicationCommandLine *cmdline)
{
  IdeApplication *app = (IdeApplication *)application;
  GVariantDict *options;

  g_assert (IDE_IS_APPLICATION_ADDIN (addin));
  g_assert (IDE_IS_APPLICATION (app));
  g_assert (G_IS_APPLICATION_COMMAND_LINE (cmdline));

  if ((options = g_application_command_line_get_options_dict (cmdline)) &&
      g_variant_dict_contains (options, "dspy"))
    {
      g_autoptr(IdeWorkbench) workbench = NULL;
      g_autoptr(GFile) workdir = NULL;
      GbpDspyWorkspace *workspace;
      IdeContext *context;

      workbench = ide_workbench_new ();
      ide_application_add_workbench (app, workbench);

      context = ide_workbench_get_context (workbench);

      workdir = g_application_command_line_create_file_for_arg (cmdline, ".");
      ide_context_set_workdir (context, workdir);

      workspace = gbp_dspy_workspace_new (application);
      ide_workbench_add_workspace (workbench, IDE_WORKSPACE (workspace));
      ide_workbench_focus_workspace (workbench, IDE_WORKSPACE (workspace));

      ide_application_set_command_line_handled (application, cmdline, TRUE);
    }
}

static void
dspy_action_cb (GSimpleAction *action,
                GVariant      *param,
                gpointer       user_data)
{
  g_autoptr(IdeWorkbench) workbench = NULL;
  g_autoptr(GFile) workdir = NULL;
  GbpDspyWorkspace *workspace;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_DSPY_APPLICATION_ADDIN (user_data));

  workbench = ide_workbench_new ();
  ide_application_add_workbench (IDE_APPLICATION_DEFAULT, workbench);

  context = ide_workbench_get_context (workbench);

  workdir = g_file_new_for_path (ide_get_projects_dir ());
  ide_context_set_workdir (context, workdir);

  workspace = gbp_dspy_workspace_new (IDE_APPLICATION_DEFAULT);
  ide_workbench_add_workspace (workbench, IDE_WORKSPACE (workspace));
  ide_workbench_focus_workspace (workbench, IDE_WORKSPACE (workspace));
}

static GActionEntry actions[] = {
  { "dspy", dspy_action_cb },
};

static void
gbp_dspy_application_addin_load (IdeApplicationAddin *addin,
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
gbp_dspy_application_addin_unload (IdeApplicationAddin *addin,
                                   IdeApplication      *application)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_APPLICATION_ADDIN (addin));
  g_assert (IDE_IS_APPLICATION (application));

  for (guint i = 0; i < G_N_ELEMENTS (actions); i++)
    g_action_map_remove_action (G_ACTION_MAP (application), actions[i].name);
}

static void
app_addin_iface_init (IdeApplicationAddinInterface *iface)
{
  iface->load = gbp_dspy_application_addin_load;
  iface->unload = gbp_dspy_application_addin_unload;
  iface->add_option_entries = gbp_dspy_application_addin_add_option_entries;
  iface->handle_command_line = gbp_dspy_application_addin_handle_command_line;
}

G_DEFINE_TYPE_WITH_CODE (GbpDspyApplicationAddin, gbp_dspy_application_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_APPLICATION_ADDIN, app_addin_iface_init))

static void
gbp_dspy_application_addin_class_init (GbpDspyApplicationAddinClass *klass)
{
}

static void
gbp_dspy_application_addin_init (GbpDspyApplicationAddin *self)
{
}
