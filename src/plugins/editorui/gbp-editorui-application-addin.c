/* gbp-editorui-application-addin.c
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

#define G_LOG_DOMAIN "gbp-editorui-application-addin"

#include "config.h"

#include <glib/gi18n.h>
#include <stdlib.h>

#include <libide-editor.h>
#include <libide-gui.h>

#include "gbp-editorui-application-addin.h"

struct _GbpEditoruiApplicationAddin
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
  g_autoptr(GFile) ancestor = NULL;

  if (files->len == 0)
    return NULL;

  for (guint i = 0; i < files->len; i++)
    {
      g_autoptr(GFile) parent = NULL;
      GFile *file = g_ptr_array_index (files, i);

      if (g_file_query_file_type (file, 0, NULL) == G_FILE_TYPE_DIRECTORY)
        parent = g_object_ref (file);
      else
        parent = g_file_get_parent (file);

      if (ancestor == NULL)
        {
          g_set_object (&ancestor, parent);
          continue;
        }

      if (g_file_equal (ancestor, parent))
        continue;

      if (g_file_has_prefix (ancestor, parent))
        g_set_object (&ancestor, parent);
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
gbp_editorui_application_addin_add_option_entries (IdeApplicationAddin *addin,
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
gbp_editorui_application_addin_open_all_cb (GObject      *object,
                                            GAsyncResult *result,
                                            gpointer      user_data)
{
  IdeWorkbench *workbench = (IdeWorkbench *) object;
  g_autoptr(GApplicationCommandLine) cmdline = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

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

  IDE_EXIT;
}

static void
gbp_editorui_application_addin_handle_command_line (IdeApplicationAddin     *addin,
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

  IDE_ENTRY;

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

          ide_workbench_focus_workspace (workbench, IDE_WORKSPACE (workspace));

          IDE_EXIT;
        }
    }

  if (argc < 2)
    IDE_EXIT;

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

      ide_workbench_focus_workspace (workbench, IDE_WORKSPACE (workspace));
    }

  g_assert (files->len > 0);

  ide_workbench_open_all_async (workbench,
                                (GFile **)(gpointer)files->pdata,
                                files->len,
                                "editorui",
                                NULL,
                                gbp_editorui_application_addin_open_all_cb,
                                g_object_ref (cmdline));

  IDE_EXIT;
}

static void
gbp_editorui_application_addin_open (IdeApplicationAddin  *addin,
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

      ide_workbench_focus_workspace (workbench, IDE_WORKSPACE (workspace));
    }

  ide_workbench_open_all_async (workbench,
                                files,
                                n_files,
                                "editorui",
                                NULL,
                                gbp_editorui_application_addin_open_all_cb,
                                NULL);

}

static void
new_editor_workspace_action (GSimpleAction *action,
                             GVariant      *param,
                             gpointer       user_data)
{
  g_autoptr(IdeWorkbench) workbench = NULL;
  g_autoptr(GFile) workdir = NULL;
  IdeEditorWorkspace *workspace;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_EDITORUI_APPLICATION_ADDIN (user_data));

  workbench = ide_workbench_new ();
  ide_application_add_workbench (IDE_APPLICATION_DEFAULT, workbench);

  context = ide_workbench_get_context (workbench);
  workdir = g_file_new_for_path (ide_get_projects_dir ());
  ide_context_set_workdir (context, workdir);

  workspace = ide_editor_workspace_new (IDE_APPLICATION_DEFAULT);
  ide_workbench_add_workspace (workbench, IDE_WORKSPACE (workspace));

  ide_workbench_focus_workspace (workbench, IDE_WORKSPACE (workspace));
}

static void
update_menus (IdeApplication *app)
{
  g_autoptr(GMenuItem) lf = NULL;
  const char *lf_name = NULL;
  GMenu *menu;

  g_assert (IDE_IS_APPLICATION (app));

#if defined(G_OS_UNIX)
# if defined(__APPLE__)
  lf_name = "macOS (LF)";
# elif  defined(__linux__)
  lf_name = "Linux (LF)";
# else
  lf_name = "Unix (LF)";
# endif
#else
  /* G_OS_WIN32 */
  lf_name = "Linux (LF)";
#endif

  g_assert (lf_name != NULL);

  menu = ide_application_get_menu_by_id (app, "editorui-line-ends-section");
  lf = g_menu_item_new (lf_name, NULL);
  g_menu_item_set_action_and_target (lf, "workspace.editorui.buffer.newline-type", "s", "lf");
  g_menu_prepend_item (menu, lf);
}

static GActionEntry actions[] = {
  { "workbench.new", new_editor_workspace_action },

  /* Used by org.gnome.Builder.desktop */
  { "new-editor", new_editor_workspace_action },
};

static void
gbp_editorui_application_addin_load (IdeApplicationAddin *addin,
                                     IdeApplication      *application)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_APPLICATION_ADDIN (addin));
  g_assert (IDE_IS_APPLICATION (application));

  g_action_map_add_action_entries (G_ACTION_MAP (application),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   addin);

  update_menus (application);
}

static void
gbp_editorui_application_addin_unload (IdeApplicationAddin *addin,
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
  iface->add_option_entries = gbp_editorui_application_addin_add_option_entries;
  iface->handle_command_line = gbp_editorui_application_addin_handle_command_line;
  iface->open = gbp_editorui_application_addin_open;
  iface->load = gbp_editorui_application_addin_load;
  iface->unload = gbp_editorui_application_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpEditoruiApplicationAddin, gbp_editorui_application_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_APPLICATION_ADDIN, cmdline_addin_iface_init))

static void
gbp_editorui_application_addin_class_init (GbpEditoruiApplicationAddinClass *klass)
{
}

static void
gbp_editorui_application_addin_init (GbpEditoruiApplicationAddin *self)
{
}
