/* gbp-vcsui-application-addin.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-vcsui-application-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-greeter.h>

#include "gbp-vcsui-application-addin.h"
#include "gbp-vcsui-clone-page.h"

struct _GbpVcsuiApplicationAddin
{
  GObject parent_instance;
};

static void
gbp_vcsui_application_addin_handle_command_line (IdeApplicationAddin     *addin,
                                                 IdeApplication          *app,
                                                 GApplicationCommandLine *cmdline)
{
  g_auto(GStrv) argv = NULL;
  GVariantDict *dict;
  const char *clone_uri = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_VCSUI_APPLICATION_ADDIN (addin));
  g_assert (IDE_IS_APPLICATION (app));
  g_assert (G_IS_APPLICATION_COMMAND_LINE (cmdline));

  dict = g_application_command_line_get_options_dict (cmdline);
  argv = ide_application_get_argv (app, cmdline);

  /*
   * If the --clone=URI option was provided, switch the greeter to the
   * clone page and begin cloning.
   */
  if (dict != NULL && g_variant_dict_lookup (dict, "clone", "&s", &clone_uri))
    {
      IdeGreeterWorkspace *workspace;
      IdeWorkbench *workbench;
      AdwNavigationPage *page;

      workbench = ide_workbench_new ();
      ide_application_add_workbench (app, workbench);

      workspace = ide_greeter_workspace_new (app);
      ide_workbench_add_workspace (workbench, IDE_WORKSPACE (workspace));

      ide_greeter_workspace_push_page_by_tag (workspace, "clone");
      page = ide_greeter_workspace_find_page (workspace, "clone");

      if (GBP_IS_VCSUI_CLONE_PAGE (page))
        gbp_vcsui_clone_page_set_uri (GBP_VCSUI_CLONE_PAGE (page), clone_uri);

      ide_workbench_focus_workspace (workbench, IDE_WORKSPACE (workspace));
    }

  IDE_EXIT;
}

static void
gbp_vcsui_application_addin_add_option_entries (IdeApplicationAddin *addin,
                                                IdeApplication      *app)
{
  g_assert (GBP_IS_VCSUI_APPLICATION_ADDIN (addin));
  g_assert (IDE_IS_APPLICATION (app));

  g_application_add_main_option (G_APPLICATION (app),
                                 "clone",
                                 0,
                                 G_OPTION_FLAG_IN_MAIN,
                                 G_OPTION_ARG_STRING,
                                 _("Begin cloning project from URI"),
                                 "URI");
}

static void
application_addin_iface_init (IdeApplicationAddinInterface *iface)
{
  iface->add_option_entries = gbp_vcsui_application_addin_add_option_entries;
  iface->handle_command_line = gbp_vcsui_application_addin_handle_command_line;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpVcsuiApplicationAddin, gbp_vcsui_application_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_APPLICATION_ADDIN, application_addin_iface_init))

static void
gbp_vcsui_application_addin_class_init (GbpVcsuiApplicationAddinClass *klass)
{
}

static void
gbp_vcsui_application_addin_init (GbpVcsuiApplicationAddin *self)
{
}
