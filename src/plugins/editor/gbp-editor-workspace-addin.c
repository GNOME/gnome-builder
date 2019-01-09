/* gbp-editor-workspace-addin.c
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

#define G_LOG_DOMAIN "gbp-editor-workspace-addin"

#include "config.h"

#include <libide-editor.h>
#include <libide-gui.h>

#include "gbp-editor-workspace-addin.h"

struct _GbpEditorWorkspaceAddin
{
  GObject               parent_instance;

  DzlSignalGroup       *buffer_manager_signals;
  DzlShortcutTooltip   *tooltip1;
  DzlShortcutTooltip   *tooltip2;

  IdeWorkspace         *workspace;
  IdeEditorSurface     *surface;
  GtkBox               *panels_box;
  DzlMenuButton        *new_button;
};

static void
find_topmost_editor (GtkWidget *widget,
                     gpointer   user_data)
{
  IdeWorkspace **workspace = user_data;
  IdeSurface *surface;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_WORKSPACE (widget));
  g_assert (workspace != NULL);

  if (*workspace)
    return;

  if ((surface = ide_workspace_get_surface_by_name (IDE_WORKSPACE (widget), "editor")) &&
      IDE_IS_EDITOR_SURFACE (surface))
    *workspace = IDE_WORKSPACE (widget);
}

static gboolean
is_topmost_workspace_with_editor (GbpEditorWorkspaceAddin *self)
{
  IdeWorkbench *workbench;
  IdeWorkspace *topmost = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_EDITOR_WORKSPACE_ADDIN (self));

  workbench = ide_widget_get_workbench (GTK_WIDGET (self->workspace));
  ide_workbench_foreach_workspace (workbench, find_topmost_editor, &topmost);

  return topmost == self->workspace;
}

static void
on_load_buffer (GbpEditorWorkspaceAddin *self,
                IdeBuffer               *buffer,
                gboolean                 create_new_view,
                IdeBufferManager        *buffer_manager)
{
  g_autofree gchar *title = NULL;

  g_assert (GBP_IS_EDITOR_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));

  /* We only want to create a new view when the buffer is originally created,
   * not when it's reloaded.
   */
  if (!create_new_view)
    return;

  /* If another workspace is active and it has an editor surface, then we
   * don't want to open the buffer in this window.
   */
  if (!is_topmost_workspace_with_editor (self))
    return;

  title = ide_buffer_dup_title (buffer);
  g_debug ("Loading editor page for \"%s\"", title);

  ide_editor_surface_focus_buffer (self->surface, buffer);
}

static void
bind_buffer_manager (GbpEditorWorkspaceAddin *self,
                     IdeBufferManager        *buffer_manager,
                     DzlSignalGroup          *signal_group)
{
  guint n_items;

  g_assert (GBP_IS_EDITOR_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));
  g_assert (DZL_IS_SIGNAL_GROUP (signal_group));

  n_items = g_list_model_get_n_items (G_LIST_MODEL (buffer_manager));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(IdeBuffer) buffer = NULL;

      buffer = g_list_model_get_item (G_LIST_MODEL (buffer_manager), i);
      ide_editor_surface_focus_buffer (self->surface, buffer);
    }
}

static void
add_buttons (GbpEditorWorkspaceAddin *self,
             IdeHeaderBar            *header)
{
  GtkWidget *button;

  g_assert (GBP_IS_EDITOR_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_HEADER_BAR (header));

  self->new_button = g_object_new (DZL_TYPE_MENU_BUTTON,
                                   "icon-name", "document-open-symbolic",
                                   "focus-on-click", FALSE,
                                   "show-arrow", TRUE,
                                   "show-icons", FALSE,
                                   "show-accels", FALSE,
                                   "menu-id", "new-document-menu",
                                   "visible", TRUE,
                                   NULL);
  g_signal_connect (self->new_button,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->new_button);
  ide_header_bar_add_primary (header, GTK_WIDGET (self->new_button));

  self->panels_box = g_object_new (GTK_TYPE_BOX,
                                   "margin-start", 6,
                                   "margin-end", 6,
                                   "visible", TRUE,
                                   NULL);
  g_signal_connect (self->panels_box,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->panels_box);
  dzl_gtk_widget_add_style_class (GTK_WIDGET (self->panels_box), "linked");
  ide_header_bar_add_primary (header, GTK_WIDGET (self->panels_box));

  button = g_object_new (GTK_TYPE_TOGGLE_BUTTON,
                         "action-name", "dockbin.left-visible",
                         "focus-on-click", FALSE,
                         "child", g_object_new (GTK_TYPE_IMAGE,
                                                "icon-name", "builder-view-left-pane-symbolic",
                                                "margin-start", 12,
                                                "margin-end", 12,
                                                "visible", TRUE,
                                                NULL),
                         "visible", TRUE,
                         NULL);
  self->tooltip1 = g_object_new (DZL_TYPE_SHORTCUT_TOOLTIP,
                                 "command-id", "org.gnome.builder.editor.navigation-panel",
                                 "widget", button,
                                 NULL);
  gtk_container_add (GTK_CONTAINER (self->panels_box), button);

  button = g_object_new (GTK_TYPE_TOGGLE_BUTTON,
                         "action-name", "dockbin.bottom-visible",
                         "focus-on-click", FALSE,
                         "child", g_object_new (GTK_TYPE_IMAGE,
                                                "icon-name", "builder-view-bottom-pane-symbolic",
                                                "margin-start", 12,
                                                "margin-end", 12,
                                                "visible", TRUE,
                                                NULL),
                         "visible", TRUE,
                         NULL);
  self->tooltip2 = g_object_new (DZL_TYPE_SHORTCUT_TOOLTIP,
                                 "command-id", "org.gnome.builder.editor.utilities-panel",
                                 "widget", button,
                                 NULL);
  gtk_container_add (GTK_CONTAINER (self->panels_box), button);
}

static void
gbp_editor_workspace_addin_load (IdeWorkspaceAddin *addin,
                                 IdeWorkspace      *workspace)
{
  GbpEditorWorkspaceAddin *self = (GbpEditorWorkspaceAddin *)addin;
  IdeBufferManager *buffer_manager;
  IdeHeaderBar *header_bar;
  IdeContext *context;

  g_assert (GBP_IS_EDITOR_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));
  g_assert (IDE_IS_PRIMARY_WORKSPACE (workspace) ||
            IDE_IS_EDITOR_WORKSPACE (workspace));

  self->workspace = workspace;

  /* Get our buffer manager for future use */
  context = ide_widget_get_context (GTK_WIDGET (workspace));
  buffer_manager = ide_buffer_manager_from_context (context);

  /* Monitor buffer manager for new buffers */
  self->buffer_manager_signals = dzl_signal_group_new (IDE_TYPE_BUFFER_MANAGER);
  g_signal_connect_swapped (self->buffer_manager_signals,
                            "bind",
                            G_CALLBACK (bind_buffer_manager),
                            self);
  dzl_signal_group_connect_swapped (self->buffer_manager_signals,
                                    "load-buffer",
                                    G_CALLBACK (on_load_buffer),
                                    self);
  dzl_signal_group_set_target (self->buffer_manager_signals, buffer_manager);

  /* Add buttons to the header bar */
  header_bar = ide_workspace_get_header_bar (workspace);
  add_buttons (self, header_bar);

  /* Add the editor surface to the workspace */
  self->surface = g_object_new (IDE_TYPE_EDITOR_SURFACE,
                                "name", "editor",
                                "visible", TRUE,
                                NULL);
  g_signal_connect (self->surface,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->surface);
  ide_workspace_add_surface (IDE_WORKSPACE (workspace), IDE_SURFACE (self->surface));
  ide_workspace_set_visible_surface_name (IDE_WORKSPACE (workspace), "editor");
}

static void
gbp_editor_workspace_addin_unload (IdeWorkspaceAddin *addin,
                                   IdeWorkspace      *workspace)
{
  GbpEditorWorkspaceAddin *self = (GbpEditorWorkspaceAddin *)addin;

  g_assert (GBP_IS_EDITOR_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));
  g_assert (IDE_IS_PRIMARY_WORKSPACE (workspace) ||
            IDE_IS_EDITOR_WORKSPACE (workspace));

  dzl_signal_group_set_target (self->buffer_manager_signals, NULL);
  g_clear_object (&self->buffer_manager_signals);

  if (self->surface != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->surface));

  if (self->panels_box)
    gtk_widget_destroy (GTK_WIDGET (self->panels_box));

  if (self->new_button)
    gtk_widget_destroy (GTK_WIDGET (self->new_button));

  g_clear_object (&self->tooltip1);
  g_clear_object (&self->tooltip2);

  self->workspace = NULL;
}

static void
gbp_editor_workspace_addin_surface_set (IdeWorkspaceAddin *addin,
                                        IdeSurface        *surface)
{
  GbpEditorWorkspaceAddin *self = (GbpEditorWorkspaceAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_EDITOR_WORKSPACE_ADDIN (self));
  g_assert (!surface || IDE_IS_SURFACE (surface));

  if (self->panels_box)
    gtk_widget_set_visible (GTK_WIDGET (self->panels_box),
                            IDE_IS_EDITOR_SURFACE (surface));
  if (self->new_button)
    gtk_widget_set_visible (GTK_WIDGET (self->new_button),
                            IDE_IS_EDITOR_SURFACE (surface));
}

static void
workspace_addin_iface_init (IdeWorkspaceAddinInterface *iface)
{
  iface->load = gbp_editor_workspace_addin_load;
  iface->unload = gbp_editor_workspace_addin_unload;
  iface->surface_set = gbp_editor_workspace_addin_surface_set;
}

G_DEFINE_TYPE_WITH_CODE (GbpEditorWorkspaceAddin, gbp_editor_workspace_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKSPACE_ADDIN,
                                                workspace_addin_iface_init))

static void
gbp_editor_workspace_addin_class_init (GbpEditorWorkspaceAddinClass *klass)
{
}

static void
gbp_editor_workspace_addin_init (GbpEditorWorkspaceAddin *self)
{
}
