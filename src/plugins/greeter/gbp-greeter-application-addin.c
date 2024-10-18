/* gbp-greeter-application-addin.c
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

#define G_LOG_DOMAIN "gbp-greeter-application-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-greeter.h>
#include <libide-gui.h>

#include "gbp-greeter-application-addin.h"

struct _GbpGreeterApplicationAddin
{
  GObject         parent_instance;
  IdeApplication *application;
};

static void
find_existing_greeter_workspace_cb (IdeWorkspace *workspace,
                                    gpointer      data)
{
  IdeGreeterWorkspace **greeter = data;

  if (*greeter != NULL)
    return;

  if (IDE_IS_GREETER_WORKSPACE (workspace) &&
      !ide_greeter_workspace_is_busy (IDE_GREETER_WORKSPACE (workspace)))
    *greeter = IDE_GREETER_WORKSPACE (workspace);
}

static void
find_existing_greeter_cb (IdeWorkbench         *workbench,
                          IdeGreeterWorkspace **greeter)
{
  if (*greeter != NULL)
    return;

  ide_workbench_foreach_workspace (workbench,
                                   find_existing_greeter_workspace_cb,
                                   greeter);
}

static void
present_greeter_with_page (GSimpleAction *action,
                           GVariant      *param,
                           gpointer       user_data)
{
  GbpGreeterApplicationAddin *self = user_data;
  g_autoptr(IdeWorkbench) workbench = NULL;
  IdeGreeterWorkspace *workspace = NULL;
  const gchar *name;

  g_assert (!action || G_IS_SIMPLE_ACTION (action));
  g_assert (!param || g_variant_is_of_type (param, G_VARIANT_TYPE_STRING));
  g_assert (GBP_IS_GREETER_APPLICATION_ADDIN (self));
  g_assert (IDE_IS_APPLICATION (self->application));

  ide_application_foreach_workbench (self->application,
                                     (GFunc)find_existing_greeter_cb,
                                     &workspace);

  if (workspace == NULL)
    {
      workbench = ide_workbench_new ();
      ide_application_add_workbench (self->application, workbench);

      workspace = ide_greeter_workspace_new (self->application);
      ide_workbench_add_workspace (workbench, IDE_WORKSPACE (workspace));
    }

  if (param != NULL &&
      (name = g_variant_get_string (param, NULL)) &&
      !ide_str_empty0 (name))
    ide_greeter_workspace_push_page_by_tag (workspace, name);

  ide_workbench_focus_workspace (ide_workspace_get_workbench (IDE_WORKSPACE (workspace)),
                                 IDE_WORKSPACE (workspace));
}

static void
new_window (GSimpleAction *action,
            GVariant      *param,
            gpointer       user_data)
{
  GbpGreeterApplicationAddin *self = user_data;

  g_assert (!action || G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_GREETER_APPLICATION_ADDIN (self));
  g_assert (IDE_IS_APPLICATION (self->application));

  present_greeter_with_page (NULL, NULL, self);
}

static void
clone_repo_cb (GSimpleAction *action,
               GVariant      *param,
               gpointer       user_data)
{
  g_autoptr(GVariant) clone_param = NULL;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_GREETER_APPLICATION_ADDIN (user_data));

  clone_param = g_variant_take_ref (g_variant_new_string ("clone"));
  present_greeter_with_page (NULL, clone_param, user_data);
}

static void
open_project (GSimpleAction *action,
              GVariant      *param,
              gpointer       user_data)
{
  GbpGreeterApplicationAddin *self = user_data;
  g_autoptr(IdeWorkbench) workbench = NULL;
  IdeGreeterWorkspace *workspace;

  g_assert (!action || G_IS_SIMPLE_ACTION (action));
  g_assert (!param || g_variant_is_of_type (param, G_VARIANT_TYPE_STRING));
  g_assert (GBP_IS_GREETER_APPLICATION_ADDIN (self));
  g_assert (IDE_IS_APPLICATION (self->application));

  workbench = ide_workbench_new ();
  ide_application_add_workbench (self->application, workbench);

  workspace = ide_greeter_workspace_new (self->application);
  ide_workbench_add_workspace (workbench, IDE_WORKSPACE (workspace));
  ide_workbench_focus_workspace (workbench, IDE_WORKSPACE (workspace));
}

static const GActionEntry actions[] = {
  { "present-greeter-with-page", present_greeter_with_page, "s" },
  { "open-project", open_project },
  { "clone-repo", clone_repo_cb },
  { "new-window", new_window },
};

static void
gbp_greeter_application_addin_add_option_entries (IdeApplicationAddin *addin,
                                                  IdeApplication      *app)
{
  g_assert (IDE_IS_APPLICATION_ADDIN (addin));
  g_assert (G_IS_APPLICATION (app));

  g_application_add_main_option (G_APPLICATION (app),
                                 "greeter",
                                 'g',
                                 G_OPTION_FLAG_IN_MAIN,
                                 G_OPTION_ARG_NONE,
                                 _("Display a new greeter window"),
                                 NULL);
}

static void
gbp_greeter_application_addin_handle_command_line (IdeApplicationAddin     *addin,
                                                   IdeApplication          *application,
                                                   GApplicationCommandLine *cmdline)
{
  GbpGreeterApplicationAddin *self = (GbpGreeterApplicationAddin *)addin;
  g_auto(GStrv) argv = NULL;
  GVariantDict *dict;
  int argc;

  g_assert (GBP_IS_GREETER_APPLICATION_ADDIN (self));
  g_assert (IDE_IS_APPLICATION (application));
  g_assert (G_IS_APPLICATION_COMMAND_LINE (cmdline));

  dict = g_application_command_line_get_options_dict (cmdline);
  argv = ide_application_get_argv (IDE_APPLICATION (application), cmdline);
  argc = g_strv_length (argv);

  /*
   * If we are processing the arguments for the startup of the primary
   * instance, then we want to show the greeter if no arguments are
   * provided. (That means argc == 1, the programe executable).
   *
   * Also, if they provided --greeter or -g we'll show a new greeter.
   */
  if ((!g_application_command_line_get_is_remote (cmdline) && argc == 1) ||
      g_variant_dict_contains (dict, "greeter"))
    {
      present_greeter_with_page (NULL, NULL, addin);
      return;
    }
}

static void
gbp_greeter_application_addin_load (IdeApplicationAddin *addin,
                                    IdeApplication      *application)
{
  GbpGreeterApplicationAddin *self = (GbpGreeterApplicationAddin *)addin;

  g_assert (GBP_IS_GREETER_APPLICATION_ADDIN (self));
  g_assert (IDE_IS_APPLICATION (application));

  self->application = application;

  g_action_map_add_action_entries (G_ACTION_MAP (application),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);
}

static void
gbp_greeter_application_addin_unload (IdeApplicationAddin *addin,
                                      IdeApplication      *application)
{
  GbpGreeterApplicationAddin *self = (GbpGreeterApplicationAddin *)addin;

  g_assert (GBP_IS_GREETER_APPLICATION_ADDIN (self));
  g_assert (IDE_IS_APPLICATION (application));

  for (guint i = 0; i < G_N_ELEMENTS (actions); i++)
    g_action_map_remove_action (G_ACTION_MAP (application), actions[i].name);

  self->application = NULL;
}

static void
gbp_greeter_application_addin_activate (IdeApplicationAddin *addin,
                                        IdeApplication      *app)
{
  GtkWindow *window;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GREETER_APPLICATION_ADDIN (addin));
  g_assert (IDE_IS_APPLICATION (app));

  if (!(window = gtk_application_get_active_window (GTK_APPLICATION (app))))
    present_greeter_with_page (NULL, NULL, addin);
}

static void
application_addin_iface_init (IdeApplicationAddinInterface *iface)
{
  iface->load = gbp_greeter_application_addin_load;
  iface->unload = gbp_greeter_application_addin_unload;
  iface->add_option_entries = gbp_greeter_application_addin_add_option_entries;
  iface->handle_command_line = gbp_greeter_application_addin_handle_command_line;
  iface->activate = gbp_greeter_application_addin_activate;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpGreeterApplicationAddin, gbp_greeter_application_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_APPLICATION_ADDIN, application_addin_iface_init))

static void
gbp_greeter_application_addin_class_init (GbpGreeterApplicationAddinClass *klass)
{
}

static void
gbp_greeter_application_addin_init (GbpGreeterApplicationAddin *self)
{
}
