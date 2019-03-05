/* gbp-editor-application-addin.c
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

#define G_LOG_DOMAIN "gbp-editor-application-addin"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-editor.h>
#include <libide-gui.h>
#include <stdlib.h>

#include "ide-window-settings-private.h"

#include "gbp-editor-application-addin.h"

struct _GbpEditorApplicationAddin
{
  GObject parent_instance;
};

static void
find_workbench_for_dir_cb (IdeWorkbench *workbench,
                           gpointer      user_data)
{
  g_autoptr(GFile) workdir = NULL;
  IdeContext *context;
  struct {
    GFile        *workdir;
    IdeWorkbench *workbench;
  } *lookup = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_WORKBENCH (workbench));
  g_assert (lookup != NULL);

  if (lookup->workbench != NULL)
    return;

  context = ide_workbench_get_context (workbench);
  workdir = ide_context_ref_workdir (context);

  if (g_file_has_prefix (lookup->workdir, workdir) ||
      g_file_equal (lookup->workdir, workdir))
    lookup->workbench = workbench;
}

static IdeWorkbench *
find_workbench_for_dir (IdeApplication *app,
                        GFile          *workdir)
{
  struct {
    GFile        *workdir;
    IdeWorkbench *workbench;
  } lookup = { workdir, NULL };

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_APPLICATION (app));
  g_assert (G_IS_FILE (workdir));

  ide_application_foreach_workbench (app,
                                     (GFunc)find_workbench_for_dir_cb,
                                     &lookup);

  return lookup.workbench ? g_object_ref (lookup.workbench) : NULL;
}

static GFile *
get_common_ancestor (GPtrArray *files)
{
  GFile *ancestor;

  if (files->len == 0)
    return NULL;

  ancestor = g_file_get_parent (g_ptr_array_index (files, 0));

  for (guint i = 1; i < files->len; i++)
    {
      GFile *file = g_ptr_array_index (files, i);

      while (!g_file_has_prefix (file, ancestor))
        {
          GFile *old = ancestor;
          ancestor = g_file_get_parent (old);
          if (g_file_equal (ancestor, old))
            break;
          g_object_unref (old);
        }
    }

  return g_steal_pointer (&ancestor);
}

static GFile *
get_common_ancestor_array (GFile **files,
                           gint    n_files)
{
  g_autoptr (GPtrArray) fileptrs = g_ptr_array_sized_new (n_files);

  g_assert (files != NULL || n_files == 0);

  for (guint i = 0; i < n_files; i++)
    g_ptr_array_add (fileptrs, files[i]);

  return get_common_ancestor (fileptrs);
}

static void
gbp_editor_application_addin_add_option_entries (IdeApplicationAddin *addin,
                                                 IdeApplication      *app)
{
  g_assert (IDE_IS_APPLICATION_ADDIN (addin));
  g_assert (G_IS_APPLICATION (app));

  g_application_add_main_option (G_APPLICATION (app),
                                 "editor",
                                 'e',
                                 G_OPTION_FLAG_IN_MAIN,
                                 G_OPTION_ARG_NONE,
                                 _("Use minimal editor interface"),
                                 NULL);
}

static void
gbp_editor_application_addin_open_all_cb (GObject      *object,
                                          GAsyncResult *result,
                                          gpointer      user_data)
{
  IdeWorkbench *workbench = (IdeWorkbench *) object;
  g_autoptr(GApplicationCommandLine) cmdline = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_WORKBENCH (workbench));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (!cmdline || G_IS_APPLICATION_COMMAND_LINE (cmdline));

  if (!ide_workbench_open_finish (workbench, result, &error))
    {
      if (error != NULL && cmdline != NULL)
        g_application_command_line_printerr (cmdline, "%s\n", error->message);
    }

  if (cmdline != NULL)
    g_application_command_line_set_exit_status (cmdline, error == NULL ? EXIT_SUCCESS : EXIT_FAILURE);
}

static void
gbp_editor_application_addin_handle_command_line (IdeApplicationAddin     *addin,
                                                  IdeApplication          *application,
                                                  GApplicationCommandLine *cmdline)
{
  g_autoptr(IdeWorkbench) workbench = NULL;
  IdeApplication *app = (IdeApplication *)application;
  g_autoptr(GPtrArray) files = NULL;
  g_autoptr(GFile) workdir = NULL;
  g_auto(GStrv) argv = NULL;
  GVariantDict *options;
  gint argc;

  g_assert (IDE_IS_APPLICATION_ADDIN (addin));
  g_assert (IDE_IS_APPLICATION (app));
  g_assert (G_IS_APPLICATION_COMMAND_LINE (cmdline));

  argv = g_application_command_line_get_arguments (cmdline, &argc);

  if ((options = g_application_command_line_get_options_dict (cmdline)) &&
      g_variant_dict_contains (options, "editor"))
    {
      ide_application_set_workspace_type (application, IDE_TYPE_EDITOR_WORKSPACE);

      /* Just open the editor workspace if no files were specified */
      if (argc < 2)
        {
          IdeEditorWorkspace *workspace;
          IdeContext *context;

          workdir = g_application_command_line_create_file_for_arg (cmdline, ".");
          ide_application_set_command_line_handled (application, cmdline, TRUE);

          workbench = ide_workbench_new ();
          ide_application_add_workbench (app, workbench);

          context = ide_workbench_get_context (workbench);
          ide_context_set_workdir (context, workdir);

          workspace = ide_editor_workspace_new (application);
          ide_workbench_add_workspace (workbench, IDE_WORKSPACE (workspace));

          _ide_window_settings_register (GTK_WINDOW (workspace));

          ide_workbench_focus_workspace (workbench, IDE_WORKSPACE (workspace));

          return;
        }
    }

  if (argc < 2)
    return;

  /*
   * If the user is trying to open various files using the command line with
   * something like "gnome-builder x.c y.c z.c" then instead of opening the
   * full project system, we'll open a simplified editor workspace for just
   * these files and avoid loading a project altogether. That means that they
   * wont get all of the IDE experience, but its faster to get quick editing
   * done and then exit.
   */

  files = g_ptr_array_new_with_free_func (g_object_unref);
  for (guint i = 1; i < argc; i++)
    g_ptr_array_add (files,
                     g_application_command_line_create_file_for_arg (cmdline, argv[i]));

  /* If we find an existing workbench that is an ancestor, or equal to the
   * common ancestor, then we'll re-use it instead of creating a new one.
   */
  workdir = get_common_ancestor (files);
  if (!(workbench = find_workbench_for_dir (application, workdir)))
    {
      IdeEditorWorkspace *workspace;
      IdeContext *context;

      workbench = ide_workbench_new ();
      ide_application_add_workbench (app, workbench);

      context = ide_workbench_get_context (workbench);

      /* Setup the working directory to top-most common ancestor of the
       * files. That way we can still get somewhat localized search results
       * and other workspace features.
       */
      if (workdir != NULL)
        ide_context_set_workdir (context, workdir);

      workspace = ide_editor_workspace_new (app);
      ide_workbench_add_workspace (workbench, IDE_WORKSPACE (workspace));

      /* Since we are opening a toplevel window, we want to restore it using
       * the same window sizing as the primary IDE window.
       */
      _ide_window_settings_register (GTK_WINDOW (workspace));

      ide_workbench_focus_workspace (workbench, IDE_WORKSPACE (workspace));
    }

  g_assert (files->len > 0);

  ide_workbench_open_all_async (workbench,
                                (GFile **)(gpointer)files->pdata,
                                files->len,
                                "editor",
                                NULL,
                                gbp_editor_application_addin_open_all_cb,
                                g_object_ref (cmdline));
}

static void
gbp_editor_application_addin_open (IdeApplicationAddin  *addin,
                                   IdeApplication       *application,
                                   GFile               **files,
                                   gint                  n_files,
                                   const gchar          *hint)
{
  g_autoptr(IdeWorkbench) workbench = NULL;
  g_autoptr(GFile) workdir = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_APPLICATION (application));
  g_assert (files != NULL);
  g_assert (n_files > 0);

  workdir = get_common_ancestor_array (files, n_files);

  if (!(workbench = find_workbench_for_dir (application, workdir)))
    {
      IdeEditorWorkspace *workspace;
      IdeContext *context;

      workbench = ide_workbench_new ();
      ide_application_add_workbench (application, workbench);

      context = ide_workbench_get_context (workbench);

      /* Setup the working directory to top-most common ancestor of the
       * files. That way we can still get somewhat localized search results
       * and other workspace features.
       */
      if (workdir != NULL)
        ide_context_set_workdir (context, workdir);

      workspace = ide_editor_workspace_new (application);
      ide_workbench_add_workspace (workbench, IDE_WORKSPACE (workspace));

      /* Since we are opening a toplevel window, we want to restore it using
       * the same window sizing as the primary IDE window.
       */
      _ide_window_settings_register (GTK_WINDOW (workspace));

      ide_workbench_focus_workspace (workbench, IDE_WORKSPACE (workspace));
    }

  ide_workbench_open_all_async (workbench,
                                files,
                                n_files,
                                "editor",
                                NULL,
                                gbp_editor_application_addin_open_all_cb,
                                NULL);

}

static void
new_editor_cb (GSimpleAction *action,
               GVariant      *param,
               gpointer       user_data)
{
  g_autoptr(IdeWorkbench) workbench = NULL;
  g_autoptr(GFile) workdir = NULL;
  IdeEditorWorkspace *workspace;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_EDITOR_APPLICATION_ADDIN (user_data));

  workbench = ide_workbench_new ();
  ide_application_add_workbench (IDE_APPLICATION_DEFAULT, workbench);

  context = ide_workbench_get_context (workbench);
  workdir = g_file_new_for_path (ide_get_projects_dir ());
  ide_context_set_workdir (context, workdir);

  workspace = ide_editor_workspace_new (IDE_APPLICATION_DEFAULT);
  ide_workbench_add_workspace (workbench, IDE_WORKSPACE (workspace));

  _ide_window_settings_register (GTK_WINDOW (workspace));

  ide_workbench_focus_workspace (workbench, IDE_WORKSPACE (workspace));
}

static GActionEntry actions[] = {
  { "new-editor", new_editor_cb },
};

static void
gbp_editor_application_addin_load (IdeApplicationAddin *addin,
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
gbp_editor_application_addin_unload (IdeApplicationAddin *addin,
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
  iface->add_option_entries = gbp_editor_application_addin_add_option_entries;
  iface->handle_command_line = gbp_editor_application_addin_handle_command_line;
  iface->open = gbp_editor_application_addin_open;
  iface->load = gbp_editor_application_addin_load;
  iface->unload = gbp_editor_application_addin_unload;
}

G_DEFINE_TYPE_WITH_CODE (GbpEditorApplicationAddin, gbp_editor_application_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_APPLICATION_ADDIN, cmdline_addin_iface_init))

static void
gbp_editor_application_addin_class_init (GbpEditorApplicationAddinClass *klass)
{
}

static void
gbp_editor_application_addin_init (GbpEditorApplicationAddin *self)
{
}
