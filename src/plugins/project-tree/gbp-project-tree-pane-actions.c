/* gbp-project-tree-pane-actions.c
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

#define G_LOG_DOMAIN "gbp-project-tree-pane-actions"

#include "config.h"

#include <vte/vte.h>

#include <libide-editor.h>

#include <libide-gtk.h>
#include <libide-projects.h>

#include "gbp-project-tree-private.h"
#include "gbp-rename-file-popover.h"
#include "gbp-new-file-popover.h"

typedef struct
{
  IdeTreeNode *node;
  GFile       *file;
  GFileType    file_type;
  guint        needs_collapse : 1;
} NewState;

typedef struct
{
  IdeWorkspace  *workspace;
  PanelPosition *position;
} ClosedPosition;

typedef struct
{
  GFile     *src;
  GFile     *dst;
  GPtrArray *positions;
} RenameState;

static void
closed_position_free (ClosedPosition *state)
{
  g_clear_object (&state->workspace);
  g_clear_object (&state->position);
  g_slice_free (ClosedPosition, state);
}

static void
rename_state_free (RenameState *state)
{
  g_clear_object (&state->dst);
  g_clear_object (&state->src);
  g_clear_pointer (&state->positions, g_ptr_array_unref);
  g_slice_free (RenameState, state);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (RenameState, rename_state_free)

static void
new_state_free (NewState *state)
{
  g_clear_object (&state->node);
  g_clear_object (&state->file);
  g_slice_free (NewState, state);
}

static void
rename_save_cb (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
  IdeBuffer *buffer = (IdeBuffer *)object;
  g_autoptr(IdePage) page = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_PAGE (page));

  if (!ide_buffer_save_file_finish (buffer, result, &error))
    g_warning ("Failed to save file: %s", error->message);

  panel_widget_close (PANEL_WIDGET (page));
}

static void
rename_load_cb (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
  IdeBufferManager *buffer_manager = (IdeBufferManager *)object;
  g_autoptr(RenameState) state = user_data;
  g_autoptr(IdeBuffer) buffer = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (state != NULL);
  g_assert (state->positions != NULL);

  if ((buffer = ide_buffer_manager_load_file_finish (buffer_manager, result, &error)))
    {
      for (guint i = 0; i < state->positions->len; i++)
        {
          ClosedPosition *cp = g_ptr_array_index (state->positions, i);
          GtkWidget *page = ide_editor_page_new (buffer);

          g_assert (cp != NULL);
          g_assert (IDE_IS_WORKSPACE (cp->workspace));
          g_assert (PANEL_IS_POSITION (cp->position));

          ide_workspace_add_page (cp->workspace, IDE_PAGE (page), cp->position);
        }
    }

  IDE_EXIT;
}

static void
rename_state_close_matching_and_save_position (IdePage *page,
                                               gpointer user_data)
{
  RenameState *state = user_data;
  GFile *this_file;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_PAGE (page));

  if (!IDE_IS_EDITOR_PAGE (page))
    return;

  if (!(this_file = ide_editor_page_get_file (IDE_EDITOR_PAGE (page))))
    return;

  g_assert (state != NULL);
  g_assert (G_IS_FILE (state->src));
  g_assert (G_IS_FILE (state->dst));
  g_assert (G_IS_FILE (this_file));

  if (g_file_equal (this_file, state->src))
    {
      IdeBuffer *buffer = ide_editor_page_get_buffer (IDE_EDITOR_PAGE (page));
      ClosedPosition *cp = g_slice_new0 (ClosedPosition);

      cp->workspace = g_object_ref (ide_widget_get_workspace (GTK_WIDGET (page)));
      cp->position = panel_widget_get_position (PANEL_WIDGET (page));
      g_ptr_array_add (state->positions, cp);

      ide_buffer_save_file_async (buffer, NULL, NULL,
                                  NULL,
                                  rename_save_cb,
                                  g_object_ref (page));
    }
}

static void
new_action_completed_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  GbpProjectTreePane *self = (GbpProjectTreePane *)object;
  NewState *state;

  g_assert (GBP_IS_PROJECT_TREE_PANE (self));
  g_assert (IDE_IS_TASK (result));

  state = ide_task_get_task_data (IDE_TASK (result));
  g_assert (state != NULL);
  g_assert (IDE_IS_TREE_NODE (state->node));

  if (state->needs_collapse)
    ide_tree_collapse_node (self->tree, state->node);

  /* Open the file if we created a regular file */
  if (state->file_type == G_FILE_TYPE_REGULAR)
    {
      IdeWorkbench *workbench;

      if (!(workbench = ide_widget_get_workbench (GTK_WIDGET (self->tree))))
        return;

      if (state->file != NULL)
        ide_workbench_open_async (workbench,
                                  state->file,
                                  "editorui",
                                  0,
                                  NULL,
                                  NULL,
                                  NULL,
                                  NULL);
    }
}

static void
gbp_project_tree_pane_actions_mkdir_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  GFile *file = (GFile *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!g_file_make_directory_finish (file, result, &error))
    g_warning ("Failed to make directory: %s", error->message);

  ide_task_return_boolean (task, TRUE);
}

static void
gbp_project_tree_pane_actions_mkfile_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  GFile *file = (GFile *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!g_file_create_finish (file, result, &error))
    g_warning ("Failed to make file: %s", error->message);

  ide_task_return_boolean (task, TRUE);
}

static void
gbp_project_tree_pane_actions_new_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  GbpNewFilePopover *popover = (GbpNewFilePopover *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) file = NULL;
  GCancellable *cancellable;
  NewState *state;

  g_assert (GBP_IS_NEW_FILE_POPOVER (popover));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!(file = gbp_new_file_popover_display_finish (popover, result, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  cancellable = ide_task_get_cancellable (task);
  state = ide_task_get_task_data (task);

  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (state != NULL);
  g_assert (IDE_IS_TREE_NODE (state->node));
  g_assert (state->file_type);
  g_assert (state->file == NULL);

  state->file = g_object_ref (file);

  if (state->file_type == G_FILE_TYPE_DIRECTORY)
    g_file_make_directory_async (file,
                                 G_PRIORITY_DEFAULT,
                                 cancellable,
                                 gbp_project_tree_pane_actions_mkdir_cb,
                                 g_steal_pointer (&task));
  else if (state->file_type == G_FILE_TYPE_REGULAR)
    g_file_create_async (file,
                         G_FILE_CREATE_NONE,
                         G_PRIORITY_DEFAULT,
                         cancellable,
                         gbp_project_tree_pane_actions_mkfile_cb,
                         g_steal_pointer (&task));
  else
    g_assert_not_reached ();

  gtk_popover_popdown (GTK_POPOVER (popover));
}

static void
gbp_project_tree_pane_actions_new (GbpProjectTreePane *self,
                                   GFileType           file_type)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GFile) directory = NULL;
  GbpNewFilePopover *popover;
  IdeProjectFile *project_file;
  IdeTreeNode *selected;
  NewState *state;

  g_assert (GBP_IS_PROJECT_TREE_PANE (self));
  g_assert (file_type == G_FILE_TYPE_REGULAR ||
            file_type == G_FILE_TYPE_DIRECTORY);

  /* Nothing to do if there was no selection */
  if (!(selected = ide_tree_get_selected_node (self->tree)))
    return;

  /* Select parent if we got an empty node or it's not a directory */
  if (!ide_tree_node_holds (selected, IDE_TYPE_PROJECT_FILE) ||
      !(project_file = ide_tree_node_get_item (selected)) ||
      !ide_project_file_is_directory (project_file))
    {
      IdeTreeNode *parent = ide_tree_node_get_parent (selected);

      if (!ide_tree_node_holds (parent, IDE_TYPE_PROJECT_FILE))
        return;

      project_file = ide_tree_node_get_item (parent);
      selected = parent;

      ide_tree_set_selected_node (self->tree, parent);
    }

  /* Now create our async task to keep track of everything during
   * the asynchronous nature of this workflow (the user entering
   * infromation, maybe cancelling, and async file creation).
   */
  directory = ide_project_file_ref_file (project_file);

  popover = g_object_new (GBP_TYPE_NEW_FILE_POPOVER,
                          "directory", directory,
                          "file-type", file_type,
                          "position", GTK_POS_RIGHT,
                          NULL);

  state = g_slice_new0 (NewState);
  state->needs_collapse = !ide_tree_is_node_expanded (self->tree, selected);
  state->file_type = file_type;
  state->node = g_object_ref (selected);

  task = ide_task_new (self, NULL, new_action_completed_cb, NULL);
  ide_task_set_source_tag (task, gbp_project_tree_pane_actions_new);
  ide_task_set_task_data (task, state, new_state_free);

  gbp_new_file_popover_display_async (popover,
                                      self->tree,
                                      selected,
                                      NULL,
                                      gbp_project_tree_pane_actions_new_cb,
                                      g_steal_pointer (&task));
}

static void
close_matching_pages (IdePage  *page,
                      gpointer  user_data)
{
  GFile *file = user_data;
  GFile *this_file;

  g_assert (IDE_IS_PAGE (page));
  g_assert (G_IS_FILE (file));

  if (!IDE_IS_EDITOR_PAGE (page))
    return;

  if (!(this_file = ide_editor_page_get_file (IDE_EDITOR_PAGE (page))))
    return;

  if (g_file_equal (this_file, file))
    {
      IdeBuffer *buffer = ide_editor_page_get_buffer (IDE_EDITOR_PAGE (page));
      ide_buffer_save_file_async (buffer, NULL, NULL,
                                  NULL,
                                  rename_save_cb,
                                  g_object_ref (page));
    }
}

static void
gbp_project_tree_pane_actions_new_file (GSimpleAction *action,
                                        GVariant      *param,
                                        gpointer       user_data)
{
  GbpProjectTreePane *self = user_data;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_PROJECT_TREE_PANE (self));

  gbp_project_tree_pane_actions_new (self, G_FILE_TYPE_REGULAR);
}

static void
gbp_project_tree_pane_actions_new_folder (GSimpleAction *action,
                                          GVariant      *param,
                                          gpointer       user_data)
{
  GbpProjectTreePane *self = user_data;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_PROJECT_TREE_PANE (self));

  gbp_project_tree_pane_actions_new (self, G_FILE_TYPE_DIRECTORY);
}

static void
gbp_project_tree_pane_actions_open (GSimpleAction *action,
                                    GVariant      *param,
                                    gpointer       user_data)
{
  GbpProjectTreePane *self = user_data;
  IdeProjectFile *project_file;
  g_autoptr(GFile) file = NULL;
  IdeWorkbench *workbench;
  IdeTreeNode *selected;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_PROJECT_TREE_PANE (self));

  if (!(selected = ide_tree_get_selected_node (self->tree)) ||
      !ide_tree_node_holds (selected, IDE_TYPE_PROJECT_FILE) ||
      !(project_file = ide_tree_node_get_item (selected)))
    return;

  file = ide_project_file_ref_file (project_file);
  workbench = ide_widget_get_workbench (GTK_WIDGET (self));

  ide_workbench_open_async (workbench,
                            file,
                            NULL,
                            IDE_BUFFER_OPEN_FLAGS_NONE,
                            NULL,
                            NULL,
                            NULL,
                            NULL);
}

static void
gbp_project_tree_pane_actions_copy (GSimpleAction *action,
                                    GVariant      *param,
                                    gpointer       user_data)
{
  GbpProjectTreePane *self = user_data;
  IdeProjectFile *project_file;
  g_autoptr(GFile) file = NULL;
  g_auto(GValue) value = G_VALUE_INIT;
  GdkClipboard *clipboard;
  IdeTreeNode *selected;

  IDE_ENTRY;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_PROJECT_TREE_PANE (self));

  if (!(selected = ide_tree_get_selected_node (self->tree)) ||
      !ide_tree_node_holds (selected, IDE_TYPE_PROJECT_FILE) ||
      !(project_file = ide_tree_node_get_item (selected)) ||
      !(file = ide_project_file_ref_file (project_file)))
    IDE_EXIT;

  g_value_init (&value, G_TYPE_FILE);
  g_value_set_object (&value, file);

  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (self->tree));
  gdk_clipboard_set_value (clipboard, &value);

  IDE_EXIT;
}

static void
gbp_project_tree_pane_actions_rename_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  IdeProject *project = (IdeProject *)object;
  g_autoptr(RenameState) state = user_data;
  g_autoptr(GError) error = NULL;
  IdeBufferManager *buffer_manager;
  IdeContext *context;
  GFile *file;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (state != NULL);
  g_assert (G_IS_FILE (state->src));
  g_assert (G_IS_FILE (state->dst));
  g_assert (state->positions != NULL);

  if (!ide_project_rename_file_finish (project, result, &error))
    g_warning ("Failed to rename file: %s", error->message);

  context = ide_object_get_context (IDE_OBJECT (project));
  buffer_manager = ide_buffer_manager_from_context (context);

  if (state->positions->len == 0)
    IDE_EXIT;

  file = state->dst;

  ide_buffer_manager_load_file_async (buffer_manager,
                                      file,
                                      IDE_BUFFER_OPEN_FLAGS_NONE,
                                      NULL,
                                      NULL,
                                      rename_load_cb,
                                      g_steal_pointer (&state));

  IDE_EXIT;
}

static void
gbp_project_tree_pane_actions_rename_display_cb (GObject      *object,
                                                 GAsyncResult *result,
                                                 gpointer      user_data)
{
  GbpRenameFilePopover *popover = (GbpRenameFilePopover *)object;
  g_autoptr(GbpProjectTreePane) self = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) dst = NULL;
  IdeWorkbench *workbench;
  RenameState *state;
  IdeProject *project;
  IdeContext *context;
  GFile *src;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_PROJECT_TREE_PANE (self));

  if (!(dst = gbp_rename_file_popover_display_finish (popover, result, &error)))
    goto done;

  src = gbp_rename_file_popover_get_file (popover);
  context = ide_widget_get_context (GTK_WIDGET (self));
  project = ide_project_from_context (context);
  workbench = ide_workbench_from_context (context);

  state = g_slice_new0 (RenameState);
  state->src = g_object_ref (src);
  state->dst = g_object_ref (dst);
  state->positions = g_ptr_array_new_with_free_func ((GDestroyNotify)closed_position_free);

  ide_workbench_foreach_page (workbench,
                              rename_state_close_matching_and_save_position,
                              state);

  ide_project_rename_file_async (project,
                                 src,
                                 dst,
                                 NULL,
                                 gbp_project_tree_pane_actions_rename_cb,
                                 g_steal_pointer (&state));

done:
  gtk_popover_popdown (GTK_POPOVER (popover));
}

static void
gbp_project_tree_pane_actions_rename (GSimpleAction *action,
                                      GVariant      *param,
                                      gpointer       user_data)
{
  GbpProjectTreePane *self = user_data;
  IdeProjectFile *project_file;
  g_autoptr(GFile) file = NULL;
  GbpRenameFilePopover *popover;
  IdeTreeNode *selected;
  gboolean is_dir;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_PROJECT_TREE_PANE (self));

  if (!(selected = ide_tree_get_selected_node (self->tree)) ||
      !ide_tree_node_holds (selected, IDE_TYPE_PROJECT_FILE) ||
      !(project_file = ide_tree_node_get_item (selected)))
    return;

  is_dir = ide_project_file_is_directory (project_file);
  file = ide_project_file_ref_file (project_file);

  popover = g_object_new (GBP_TYPE_RENAME_FILE_POPOVER,
                          "position", GTK_POS_RIGHT,
                          "is-directory", is_dir,
                          "file", file,
                          NULL);

  gbp_rename_file_popover_display_async (popover,
                                         self->tree,
                                         selected,
                                         NULL,
                                         gbp_project_tree_pane_actions_rename_display_cb,
                                         g_object_ref (self));
}

static void
gbp_project_tree_pane_actions_trash_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  IdeProjectFile *project_file = (IdeProjectFile *)object;
  g_autoptr(IdeTreeNode) node = user_data;
  g_autoptr(GError) error = NULL;
  IdeTreeNode *parent;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_PROJECT_FILE (project_file));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TREE_NODE (node));

  if (!ide_project_file_trash_finish (project_file, result, &error))
    {
      g_warning ("Failed to trash file: %s", error->message);
      IDE_EXIT;
    }

  if ((parent = ide_tree_node_get_parent (node)))
    ide_tree_node_remove (parent, node);

  IDE_EXIT;
}

static void
gbp_project_tree_pane_actions_trash (GSimpleAction *action,
                                     GVariant      *param,
                                     gpointer       user_data)
{
  GbpProjectTreePane *self = user_data;
  IdeProjectFile *project_file;
  g_autoptr(GFile) file = NULL;
  IdeWorkbench *workbench;
  IdeTreeNode *selected;

  IDE_ENTRY;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_PROJECT_TREE_PANE (self));

  if (!(selected = ide_tree_get_selected_node (self->tree)) ||
      !ide_tree_node_holds (selected, IDE_TYPE_PROJECT_FILE) ||
      !(project_file = ide_tree_node_get_item (selected)))
    IDE_EXIT;

  file = ide_project_file_ref_file (project_file);
  workbench = ide_widget_get_workbench (GTK_WIDGET (self->tree));
  ide_workbench_foreach_page (workbench, close_matching_pages, file);

  ide_project_file_trash_async (project_file,
                                NULL,
                                gbp_project_tree_pane_actions_trash_cb,
                                g_object_ref (selected));

  IDE_EXIT;
}

static void
gbp_project_tree_pane_actions_open_containing_folder (GSimpleAction *action,
                                                      GVariant      *param,
                                                      gpointer       user_data)
{
  GbpProjectTreePane *self = user_data;
  IdeProjectFile *project_file;
  g_autoptr(GFile) file = NULL;
  IdeTreeNode *selected;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_PROJECT_TREE_PANE (self));

  if (!(selected = ide_tree_get_selected_node (self->tree)) ||
      !ide_tree_node_holds (selected, IDE_TYPE_PROJECT_FILE) ||
      !(project_file = ide_tree_node_get_item (selected)))
    return;

  file = ide_project_file_ref_file (project_file);
  ide_file_manager_show (file, NULL);
}

static void
gbp_project_tree_pane_actions_open_with_hint (GSimpleAction *action,
                                              GVariant      *param,
                                              gpointer       user_data)
{
  GbpProjectTreePane *self = user_data;
  IdeProjectFile *project_file;
  g_autoptr(GFile) file = NULL;
  IdeWorkbench *workbench;
  IdeTreeNode *selected;
  const gchar *hint;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_PROJECT_TREE_PANE (self));

  if (!(selected = ide_tree_get_selected_node (self->tree)) ||
      !ide_tree_node_holds (selected, IDE_TYPE_PROJECT_FILE) ||
      !(project_file = ide_tree_node_get_item (selected)) ||
      !(hint = g_variant_get_string (param, NULL)))
    return;

  workbench = ide_widget_get_workbench (GTK_WIDGET (self));
  file = ide_project_file_ref_file (project_file);

  ide_workbench_open_async (workbench,
                            file,
                            hint,
                            IDE_BUFFER_OPEN_FLAGS_NONE,
                            NULL,
                            NULL,
                            NULL,
                            NULL);
}

/* Based on gdesktopappinfo.c in GIO */
static gchar *
find_terminal_executable (void)
{
  g_autofree gchar *path = NULL;
  g_autofree gchar *gsettings_terminal = NULL;
  const gchar *terminals[] = {
    "x-terminal-emulator",    /* Debian's alternative system */
    "ptyxis",
    "xdg-terminal-exec",
    "gnome-terminal",
    g_getenv ("TERM"),        /* This is generally one of the fallback terminals */
    "nxterm", "color-xterm",
    "rxvt", "xterm", "dtterm"
  };

  /* TODO: Probably should look at ~/.config/xdg-terminals.list */

  for (guint i = 0; i < G_N_ELEMENTS (terminals) && path == NULL; ++i)
    {
      g_debug ("Checking for terminal priority %u `%s`", i, terminals[i]);

      if (terminals[i] != NULL)
        path = ide_find_program_in_host_path (terminals[i]);
    }

  return g_steal_pointer (&path);
}

static void
gbp_project_tree_pane_actions_open_in_terminal (GSimpleAction *action,
                                                GVariant      *param,
                                                gpointer       user_data)
{
  GbpProjectTreePane *self = user_data;
  IdeProjectFile *project_file;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  IdeTreeNode *selected;
  g_autofree gchar *terminal_executable = NULL;
  g_autoptr(GFile) workdir = NULL;
  g_autoptr(GError) error = NULL;

  if (!(selected = ide_tree_get_selected_node (self->tree)) ||
      !ide_tree_node_holds (selected, IDE_TYPE_PROJECT_FILE) ||
      !(project_file = ide_tree_node_get_item (selected)))
    return;

  if (ide_project_file_is_directory (project_file))
    workdir = ide_project_file_ref_file (project_file);
  else
    workdir = g_object_ref (ide_project_file_get_directory (project_file));

  if (!g_file_is_native (workdir))
    {
      g_autofree gchar *uri = g_file_get_uri (workdir);
      g_warning ("Not a native file, cannot open terminal here: %s", uri);
      return;
    }

  terminal_executable = find_terminal_executable ();
  g_return_if_fail (terminal_executable != NULL);

  /* Launch the terminal, on the host */
  launcher = ide_subprocess_launcher_new (0);
  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);
  ide_subprocess_launcher_push_argv (launcher, terminal_executable);
  if (g_str_has_suffix (terminal_executable, "/ptyxis"))
    ide_subprocess_launcher_push_args (launcher, IDE_STRV_INIT ("--tab", "-d", g_file_peek_path (workdir)));
  else
    ide_subprocess_launcher_set_cwd (launcher, g_file_peek_path (workdir));

  if (!(subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error)))
    g_warning ("Failed to spawn terminal: %s", error->message);
  else
    ide_subprocess_wait_async (subprocess, NULL, NULL, NULL);
}

static const GActionEntry entries[] = {
  { "new-file", gbp_project_tree_pane_actions_new_file },
  { "new-folder", gbp_project_tree_pane_actions_new_folder },
  { "open", gbp_project_tree_pane_actions_open },
  { "open-with-hint", gbp_project_tree_pane_actions_open_with_hint, "s" },
  { "open-containing-folder", gbp_project_tree_pane_actions_open_containing_folder },
  { "open-in-terminal", gbp_project_tree_pane_actions_open_in_terminal },
  { "rename", gbp_project_tree_pane_actions_rename },
  { "trash", gbp_project_tree_pane_actions_trash },
  { "copy", gbp_project_tree_pane_actions_copy },
};

void
_gbp_project_tree_pane_init_actions (GbpProjectTreePane *self)
{
  g_autoptr(GSimpleActionGroup) actions = NULL;
  g_autoptr(GSettings) settings = NULL;
  g_autoptr(GAction) ignored_action = NULL;
  g_autoptr(GAction) sort_action = NULL;

  g_assert (GBP_IS_PROJECT_TREE_PANE (self));

  settings = g_settings_new ("org.gnome.builder.project-tree");
  sort_action = g_settings_create_action (settings, "sort-directories-first");
  ignored_action = g_settings_create_action (settings, "show-ignored-files");

  actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (actions),
                                   entries,
                                   G_N_ELEMENTS (entries),
                                   self);
  g_action_map_add_action (G_ACTION_MAP (actions), ignored_action);
  g_action_map_add_action (G_ACTION_MAP (actions), sort_action);
  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  "project-tree",
                                  G_ACTION_GROUP (actions));

  self->actions = g_object_ref (G_ACTION_GROUP (actions));

  _gbp_project_tree_pane_update_actions (self);
}

G_GNUC_NULL_TERMINATED static void
action_map_set (GActionMap *map,
                const char *name,
                const char *first_property,
                ...)
{
  GAction *action;
  va_list args;

  action = g_action_map_lookup_action (map, name);

  va_start (args, first_property);
  g_object_set_valist (G_OBJECT (action), first_property, args);
  va_end (args);
}

void
_gbp_project_tree_pane_update_actions (GbpProjectTreePane *self)
{
  IdeTreeNode *node;
  gboolean is_file = FALSE;
  gboolean is_dir = FALSE;

  g_assert (GBP_IS_PROJECT_TREE_PANE (self));

  if ((node = ide_tree_get_selected_node (self->tree)))
    {
      GObject *item = ide_tree_node_get_item (node);

      if ((is_file = IDE_IS_PROJECT_FILE (item)))
        is_dir = ide_project_file_is_directory (IDE_PROJECT_FILE (item));
    }

  action_map_set (G_ACTION_MAP (self->actions), "new-file",
                  "enabled", is_file,
                  NULL);
  action_map_set (G_ACTION_MAP (self->actions), "new-folder",
                  "enabled", is_file,
                  NULL);
  action_map_set (G_ACTION_MAP (self->actions), "trash",
                  "enabled", is_file,
                  NULL);
  action_map_set (G_ACTION_MAP (self->actions), "rename",
                  "enabled", is_file,
                  NULL);
  action_map_set (G_ACTION_MAP (self->actions), "open",
                  "enabled", is_file && !is_dir,
                  NULL);
  action_map_set (G_ACTION_MAP (self->actions), "open-with-hint",
                  "enabled", is_file,
                  NULL);
  action_map_set (G_ACTION_MAP (self->actions), "open-containing-folder",
                  "enabled", is_file,
                  NULL);
  action_map_set (G_ACTION_MAP (self->actions), "open-in-terminal",
                  "enabled", is_file,
                  NULL);
  action_map_set (G_ACTION_MAP (self->actions), "copy",
                  "enabled", is_file,
                  NULL);
}
