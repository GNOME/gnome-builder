/* ide-editor-perspective.c
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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
 */

#define G_LOG_DOMAIN "ide-editor-perspective"

#include <glib/gi18n.h>

#include "buffers/ide-buffer.h"
#include "buffers/ide-buffer-manager.h"
#include "diagnostics/ide-source-location.h"
#include "editor/ide-editor-addin.h"
#include "editor/ide-editor-perspective.h"
#include "editor/ide-editor-private.h"
#include "editor/ide-editor-properties.h"
#include "editor/ide-editor-sidebar.h"
#include "editor/ide-editor-view.h"
#include "layout/ide-layout-transient-sidebar.h"
#include "workbench/ide-perspective.h"
#include "workbench/ide-workbench.h"
#include "util/ide-gtk.h"

typedef struct
{
  IdeEditorPerspective *self;
  IdeSourceLocation    *location;
} FocusLocation;

static void perspective_iface_init                     (IdePerspectiveInterface *iface);
static void ide_editor_perspective_focus_location_full (IdeEditorPerspective    *self,
                                                        IdeSourceLocation       *location,
                                                        gboolean                 open_if_not_found);

G_DEFINE_TYPE_WITH_CODE (IdeEditorPerspective, ide_editor_perspective, IDE_TYPE_LAYOUT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_PERSPECTIVE, perspective_iface_init))

static void
ide_editor_perspective_addin_added (PeasExtensionSet *set,
                                    PeasPluginInfo   *plugin_info,
                                    PeasExtension    *exten,
                                    gpointer          user_data)
{
  IdeEditorPerspective *self = user_data;
  IdeEditorAddin *addin = (IdeEditorAddin *)exten;
  IdeLayoutView *view;

  g_assert (IDE_IS_EDITOR_PERSPECTIVE (self));
  g_assert (IDE_IS_EDITOR_ADDIN (addin));
  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);

  ide_editor_addin_load (addin, self);

  view = ide_layout_grid_get_current_view (self->grid);
  if (view != NULL)
    ide_editor_addin_view_set (addin, view);
}

static void
ide_editor_perspective_addin_removed (PeasExtensionSet *set,
                                      PeasPluginInfo   *plugin_info,
                                      PeasExtension    *exten,
                                      gpointer          user_data)
{
  IdeEditorPerspective *self = user_data;
  IdeEditorAddin *addin = (IdeEditorAddin *)exten;
  IdeLayoutView *view;

  g_assert (IDE_IS_EDITOR_PERSPECTIVE (self));
  g_assert (IDE_IS_EDITOR_ADDIN (addin));
  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);

  view = ide_layout_grid_get_current_view (self->grid);
  if (view != NULL)
    ide_editor_addin_view_set (addin, NULL);

  ide_editor_addin_unload (addin, self);
}

static void
ide_editor_perspective_hierarchy_changed (GtkWidget *widget,
                                          GtkWidget *old_toplevel)
{
  IdeEditorPerspective *self = (IdeEditorPerspective *)widget;

  g_assert (IDE_IS_EDITOR_PERSPECTIVE (self));
  g_assert (!old_toplevel || GTK_IS_WIDGET (old_toplevel));

  if (self->addins == NULL)
    {
      GtkWidget *toplevel;

      /*
       * If we just got a new toplevel and it is a workbench,
       * and we have not yet created our addins, do so now.
       */

      toplevel = gtk_widget_get_ancestor (widget, IDE_TYPE_WORKBENCH);

      if (toplevel != NULL)
        {
          self->addins = peas_extension_set_new (peas_engine_get_default (),
                                                 IDE_TYPE_EDITOR_ADDIN,
                                                 NULL);
          g_signal_connect (self->addins,
                            "extension-added",
                            G_CALLBACK (ide_editor_perspective_addin_added),
                            self);
          g_signal_connect (self->addins,
                            "extension-removed",
                            G_CALLBACK (ide_editor_perspective_addin_removed),
                            self);
          peas_extension_set_foreach (self->addins,
                                      ide_editor_perspective_addin_added,
                                      self);
        }
    }
}

static void
ide_editor_perspective_addins_view_set (PeasExtensionSet *set,
                                        PeasPluginInfo   *plugin_info,
                                        PeasExtension    *exten,
                                        gpointer          user_data)
{
  IdeEditorAddin *addin = (IdeEditorAddin *)exten;
  IdeLayoutView *view = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_EDITOR_ADDIN (addin));
  g_assert (!view || IDE_IS_LAYOUT_VIEW (view));

  ide_editor_addin_view_set (addin, view);
}

static void
ide_editor_perspective_notify_current_view (IdeEditorPerspective *self,
                                            GParamSpec           *pspec,
                                            IdeLayoutGrid        *grid)
{
  IdeLayoutView *view;

  g_assert (IDE_IS_EDITOR_PERSPECTIVE (self));
  g_assert (pspec != NULL);
  g_assert (IDE_IS_LAYOUT_GRID (grid));

  view = ide_layout_grid_get_current_view (grid);

  if (IDE_IS_EDITOR_VIEW (view))
    ide_editor_properties_set_view (self->properties, IDE_EDITOR_VIEW (view));
  else
    ide_editor_properties_set_view (self->properties, NULL);

  peas_extension_set_foreach (self->addins,
                              ide_editor_perspective_addins_view_set,
                              view);
}

static void
ide_editor_perspective_add (GtkContainer *container,
                            GtkWidget    *widget)
{
  IdeEditorPerspective *self = (IdeEditorPerspective *)container;

  g_assert (IDE_IS_EDITOR_PERSPECTIVE (self));
  g_assert (GTK_IS_WIDGET (widget));

  if (IDE_IS_LAYOUT_VIEW (widget))
    gtk_container_add (GTK_CONTAINER (self->grid), widget);
  else
    GTK_CONTAINER_CLASS (ide_editor_perspective_parent_class)->add (container, widget);
}

static GtkWidget *
ide_editor_perspective_create_edge (DzlDockBin      *dock_bin,
                                    GtkPositionType  edge)
{
  g_assert (DZL_IS_DOCK_BIN (dock_bin));
  g_assert (edge >= GTK_POS_LEFT);
  g_assert (edge <= GTK_POS_BOTTOM);

  if (edge == GTK_POS_LEFT)
    return g_object_new (IDE_TYPE_EDITOR_SIDEBAR,
                         "edge", edge,
                         "reveal-child", FALSE,
                         "visible", TRUE,
                         NULL);

  if (edge == GTK_POS_RIGHT)
    return g_object_new (IDE_TYPE_LAYOUT_TRANSIENT_SIDEBAR,
                         "edge", edge,
                         "reveal-child", FALSE,
                         "visible", FALSE,
                         NULL);

  return DZL_DOCK_BIN_CLASS (ide_editor_perspective_parent_class)->create_edge (dock_bin, edge);
}

static void
ide_editor_perspective_destroy (GtkWidget *widget)
{
  IdeEditorPerspective *self = (IdeEditorPerspective *)widget;

  g_assert (IDE_IS_EDITOR_PERSPECTIVE (self));

  g_clear_object (&self->addins);

  GTK_WIDGET_CLASS (ide_editor_perspective_parent_class)->destroy (widget);
}

static void
ide_editor_perspective_class_init (IdeEditorPerspectiveClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
  DzlDockBinClass *dock_bin_class = DZL_DOCK_BIN_CLASS (klass);

  widget_class->destroy = ide_editor_perspective_destroy;
  widget_class->hierarchy_changed = ide_editor_perspective_hierarchy_changed;

  container_class->add = ide_editor_perspective_add;

  dock_bin_class->create_edge = ide_editor_perspective_create_edge;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-editor-perspective.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeEditorPerspective, grid);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorPerspective, properties);

  g_type_ensure (IDE_TYPE_EDITOR_PROPERTIES);
  g_type_ensure (IDE_TYPE_EDITOR_SIDEBAR);
  g_type_ensure (IDE_TYPE_LAYOUT_GRID);
}

static void
ide_editor_perspective_init (IdeEditorPerspective *self)
{
  IdeEditorSidebar *sidebar;

  gtk_widget_init_template (GTK_WIDGET (self));

  _ide_editor_perspective_init_actions (self);
  _ide_editor_perspective_init_shortcuts (self);

  g_signal_connect_swapped (self->grid,
                            "notify::current-view",
                            G_CALLBACK (ide_editor_perspective_notify_current_view),
                            self);

  sidebar = ide_editor_perspective_get_sidebar (self);
  _ide_editor_sidebar_set_open_pages (sidebar, G_LIST_MODEL (self->grid));
}

/**
 * ide_editor_perspective_get_grid:
 * @self: a #IdeEditorPerspective
 *
 * Gets the grid for the perspective. This is the area containing
 * grid columns, stacks, and views.
 *
 * Returns: (transfer none): An #IdeLayoutGrid.
 *
 * Since: 3.26
 */
IdeLayoutGrid *
ide_editor_perspective_get_grid (IdeEditorPerspective *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_PERSPECTIVE (self), NULL);

  return self->grid;
}

static void
ide_editor_perspective_find_source_location (GtkWidget *widget,
                                             gpointer   user_data)
{
  struct {
    IdeFile *file;
    IdeEditorView *view;
  } *lookup = user_data;
  IdeBuffer *buffer;
  IdeFile *file;

  g_return_if_fail (IDE_IS_LAYOUT_VIEW (widget));

  if (lookup->view != NULL)
    return;

  if (!IDE_IS_EDITOR_VIEW (widget))
    return;

  buffer = ide_editor_view_get_buffer (IDE_EDITOR_VIEW (widget));
  file = ide_buffer_get_file (buffer);

  if (ide_file_equal (file, lookup->file))
    lookup->view = IDE_EDITOR_VIEW (widget);
}

static void
ide_editor_perspective_focus_location_cb (GObject      *object,
                                          GAsyncResult *result,
                                          gpointer      user_data)
{
  IdeBufferManager *bufmgr = (IdeBufferManager *)object;
  FocusLocation *state = user_data;
  GError *error = NULL;

  g_assert (IDE_IS_BUFFER_MANAGER (bufmgr));
  g_assert (state != NULL);
  g_assert (IDE_IS_EDITOR_PERSPECTIVE (state->self));
  g_assert (state->location != NULL);

  if (!ide_buffer_manager_load_file_finish (bufmgr, result, &error))
    {
      /* TODO: display warning breifly to the user in the frame? */
      g_warning ("%s", error->message);
      g_clear_error (&error);
      goto cleanup;
    }

  /* try again now that we have loaded */
  ide_editor_perspective_focus_location_full (state->self, state->location, FALSE);

cleanup:
  g_object_unref (state->self);
  ide_source_location_unref (state->location);
  g_slice_free (FocusLocation, state);
}

static void
ide_editor_perspective_focus_location_full (IdeEditorPerspective *self,
                                            IdeSourceLocation    *location,
                                            gboolean              open_if_not_found)
{
  struct {
    IdeFile *file;
    IdeEditorView *view;
  } lookup = { 0 };
  GtkWidget *stack;
  guint line;
  guint line_offset;

  g_return_if_fail (IDE_IS_EDITOR_PERSPECTIVE (self));
  g_return_if_fail (location != NULL);

  lookup.file = ide_source_location_get_file (location);
  lookup.view = NULL;

  if (lookup.file == NULL)
    {
      g_warning ("IdeSourceLocation does not contain a file");
      return;
    }

  ide_perspective_views_foreach (IDE_PERSPECTIVE (self),
                                 ide_editor_perspective_find_source_location,
                                 &lookup);

  if (!open_if_not_found && lookup.view == NULL)
    return;

  if (lookup.view == NULL)
    {
      FocusLocation *state;
      IdeBufferManager *bufmgr;
      IdeWorkbench *workbench;
      IdeContext *context;

      workbench = ide_widget_get_workbench (GTK_WIDGET (self));
      context = ide_workbench_get_context (workbench);
      bufmgr = ide_context_get_buffer_manager (context);

      state = g_slice_new0 (FocusLocation);
      state->self = g_object_ref (self);
      state->location = ide_source_location_ref (location);

      ide_buffer_manager_load_file_async (bufmgr,
                                          lookup.file,
                                          FALSE,
                                          IDE_WORKBENCH_OPEN_FLAGS_NONE,
                                          NULL,
                                          NULL,
                                          ide_editor_perspective_focus_location_cb,
                                          state);
      return;
    }

  line = ide_source_location_get_line (location);
  line_offset = ide_source_location_get_line_offset (location);

  stack = gtk_widget_get_ancestor (GTK_WIDGET (lookup.view), IDE_TYPE_LAYOUT_STACK);
  ide_layout_stack_set_visible_child (IDE_LAYOUT_STACK (stack), IDE_LAYOUT_VIEW (lookup.view));
  ide_editor_view_scroll_to_line_offset (lookup.view, line, line_offset);
}

void
ide_editor_perspective_focus_location (IdeEditorPerspective *self,
                                       IdeSourceLocation    *location)
{
  g_return_if_fail (IDE_IS_EDITOR_PERSPECTIVE (self));
  g_return_if_fail (location != NULL);

  ide_editor_perspective_focus_location_full (self, location, TRUE);
}

static void
locate_view_for_buffer (GtkWidget *widget,
                        gpointer   user_data)
{
  struct {
    IdeBuffer     *buffer;
    IdeLayoutView *view;
  } *lookup = user_data;

  if (lookup->view != NULL)
    return;

  if (IDE_IS_EDITOR_VIEW (widget))
    {
      if (ide_editor_view_get_buffer (IDE_EDITOR_VIEW (widget)) == lookup->buffer)
        lookup->view = IDE_LAYOUT_VIEW (widget);
    }
}

static gboolean
ide_editor_perspective_focus_if_found (IdeEditorPerspective *self,
                                       IdeBuffer            *buffer,
                                       gboolean              any_stack)
{
  IdeLayoutStack *stack;
  struct {
    IdeBuffer     *buffer;
    IdeLayoutView *view;
  } lookup = { buffer };

  g_return_val_if_fail (IDE_IS_EDITOR_PERSPECTIVE (self), FALSE);
  g_return_val_if_fail (IDE_IS_BUFFER (buffer), FALSE);

  stack = ide_layout_grid_get_current_stack (self->grid);

  if (any_stack)
    ide_layout_grid_foreach_view (self->grid, locate_view_for_buffer, &lookup);
  else
    ide_layout_stack_foreach_view (stack, locate_view_for_buffer, &lookup);

  if (lookup.view != NULL)
    {
      stack = IDE_LAYOUT_STACK (gtk_widget_get_ancestor (GTK_WIDGET (lookup.view),
                                                         IDE_TYPE_LAYOUT_STACK));
      ide_layout_stack_set_visible_child (stack, lookup.view);
      gtk_widget_grab_focus (GTK_WIDGET (lookup.view));
      return TRUE;
    }

  return FALSE;
}

void
ide_editor_perspective_focus_buffer (IdeEditorPerspective *self,
                                     IdeBuffer            *buffer)
{
  IdeEditorView *view;

  g_return_if_fail (IDE_IS_EDITOR_PERSPECTIVE (self));
  g_return_if_fail (IDE_IS_BUFFER (buffer));

  if (ide_editor_perspective_focus_if_found (self, buffer, TRUE))
    return;

  view = g_object_new (IDE_TYPE_EDITOR_VIEW,
                       "buffer", buffer,
                       "visible", TRUE,
                       NULL);
  gtk_container_add (GTK_CONTAINER (self->grid), GTK_WIDGET (view));
}

void
ide_editor_perspective_focus_buffer_in_current_stack (IdeEditorPerspective *self,
                                                      IdeBuffer            *buffer)
{
  IdeLayoutStack *stack;
  IdeEditorView *view;

  g_return_if_fail (IDE_IS_EDITOR_PERSPECTIVE (self));
  g_return_if_fail (IDE_IS_BUFFER (buffer));

  if (ide_editor_perspective_focus_if_found (self, buffer, FALSE))
    return;

  stack = ide_layout_grid_get_current_stack (self->grid);

  view = g_object_new (IDE_TYPE_EDITOR_VIEW,
                       "buffer", buffer,
                       "visible", TRUE,
                       NULL);

  gtk_container_add (GTK_CONTAINER (stack), GTK_WIDGET (view));
}

/**
 * ide_editor_perspective_get_active_view:
 * @self: a #IdeEditorPerspective
 *
 * Gets the active view for the perspective, or %NULL if there is not one.
 *
 * Returns: (nullable) (transfer none): An #IdeLayoutView or %NULL.
 *
 * Since: 3.26
 */
IdeLayoutView *
ide_editor_perspective_get_active_view (IdeEditorPerspective *self)
{
  IdeLayoutStack *stack;

  g_return_val_if_fail (IDE_IS_EDITOR_PERSPECTIVE (self), NULL);

  stack = ide_layout_grid_get_current_stack (self->grid);

  return ide_layout_stack_get_visible_child (stack);
}

/**
 * ide_editor_perspective_get_sidebar:
 * @self: a #IdeEditorPerspective
 *
 * Gets the #IdeEditorSidebar for the editor perspective.
 *
 * Returns: (transfer none): A #IdeEditorSidebar
 *
 * Since: 3.26
 */
IdeEditorSidebar *
ide_editor_perspective_get_sidebar (IdeEditorPerspective *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_PERSPECTIVE (self), NULL);

  return IDE_EDITOR_SIDEBAR (dzl_dock_bin_get_left_edge (DZL_DOCK_BIN (self)));
}

/**
 * ide_editor_perspective_get_transient_sidebar:
 * @self: a #IdeEditorPerspective
 *
 * Gets the transient sidebar for the editor perspective.
 *
 * The transient sidebar is a sidebar on the right side of the perspective. It
 * is displayed only when necessary. It animates in and out of view based on
 * focus tracking and other heuristics.
 *
 * Returns: (transfer none): An #IdeLayoutTransientSidebar
 *
 * Since: 3.26
 */
IdeLayoutTransientSidebar *
ide_editor_perspective_get_transient_sidebar (IdeEditorPerspective *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_PERSPECTIVE (self), NULL);

  return IDE_LAYOUT_TRANSIENT_SIDEBAR (dzl_dock_bin_get_right_edge (DZL_DOCK_BIN (self)));
}

/**
 * ide_editor_perspective_get_bottom_edge:
 *
 * Returns: (transfer none): A #GtkWidget
 */
GtkWidget *
ide_editor_perspective_get_bottom_edge (IdeEditorPerspective *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_PERSPECTIVE (self), NULL);
  return dzl_dock_bin_get_bottom_edge (DZL_DOCK_BIN (self));
}

static void
set_reveal_child_without_transition (DzlDockRevealer *revealer,
                                     gboolean         reveal)
{
  DzlDockRevealerTransitionType type;

  g_assert (DZL_IS_DOCK_REVEALER (revealer));

  type = dzl_dock_revealer_get_transition_type (revealer);
  dzl_dock_revealer_set_transition_type (revealer, DZL_DOCK_REVEALER_TRANSITION_TYPE_NONE);
  dzl_dock_revealer_set_reveal_child (revealer, reveal);
  dzl_dock_revealer_set_transition_type (revealer, type);
}

static void
ide_editor_perspective_restore_panel_state (IdeEditorPerspective *self)
{
  g_autoptr(GSettings) settings = NULL;
  GtkWidget *pane;
  gboolean reveal;
  guint position;

  g_assert (IDE_IS_EDITOR_PERSPECTIVE (self));

  /* TODO: This belongs in editor settings probably */

  settings = g_settings_new ("org.gnome.builder.workbench");

  pane = dzl_dock_bin_get_left_edge (DZL_DOCK_BIN (self));
  reveal = g_settings_get_boolean (settings, "left-visible");
  position = g_settings_get_int (settings, "left-position");
  dzl_dock_revealer_set_position (DZL_DOCK_REVEALER (pane), position);
  set_reveal_child_without_transition (DZL_DOCK_REVEALER (pane), reveal);

  pane = dzl_dock_bin_get_right_edge (DZL_DOCK_BIN (self));
  position = g_settings_get_int (settings, "right-position");
  dzl_dock_revealer_set_position (DZL_DOCK_REVEALER (pane), position);
  set_reveal_child_without_transition (DZL_DOCK_REVEALER (pane), FALSE);

  pane = dzl_dock_bin_get_bottom_edge (DZL_DOCK_BIN (self));
  reveal = g_settings_get_boolean (settings, "bottom-visible");
  position = g_settings_get_int (settings, "bottom-position");
  dzl_dock_revealer_set_position (DZL_DOCK_REVEALER (pane), position);
  set_reveal_child_without_transition (DZL_DOCK_REVEALER (pane), reveal);
}

static void
ide_editor_perspective_save_panel_state (IdeEditorPerspective *self)
{
  g_autoptr(GSettings) settings = NULL;
  GtkWidget *pane;
  gboolean reveal;
  guint position;

  g_assert (IDE_IS_EDITOR_PERSPECTIVE (self));

  /* TODO: possibly belongs in editor settings */
  settings = g_settings_new ("org.gnome.builder.workbench");

  pane = dzl_dock_bin_get_left_edge (DZL_DOCK_BIN (self));
  position = dzl_dock_revealer_get_position (DZL_DOCK_REVEALER (pane));
  reveal = dzl_dock_revealer_get_reveal_child (DZL_DOCK_REVEALER (pane));
  g_settings_set_boolean (settings, "left-visible", reveal);
  g_settings_set_int (settings, "left-position", position);

  pane = dzl_dock_bin_get_right_edge (DZL_DOCK_BIN (self));
  position = dzl_dock_revealer_get_position (DZL_DOCK_REVEALER (pane));
  reveal = dzl_dock_revealer_get_reveal_child (DZL_DOCK_REVEALER (pane));
  g_settings_set_boolean (settings, "right-visible", reveal);
  g_settings_set_int (settings, "right-position", position);

  pane = dzl_dock_bin_get_bottom_edge (DZL_DOCK_BIN (self));
  position = dzl_dock_revealer_get_position (DZL_DOCK_REVEALER (pane));
  reveal = dzl_dock_revealer_get_reveal_child (DZL_DOCK_REVEALER (pane));
  g_settings_set_boolean (settings, "bottom-visible", reveal);
  g_settings_set_int (settings, "bottom-position", position);
}

static void
ide_editor_perspective_views_foreach (IdePerspective *perspective,
                                      GtkCallback     callback,
                                      gpointer        user_data)
{
  IdeEditorPerspective *self = (IdeEditorPerspective *)perspective;

  g_assert (IDE_IS_EDITOR_PERSPECTIVE (self));
  g_assert (callback != NULL);

  ide_layout_grid_foreach_view (self->grid, callback, user_data);
}

static gchar *
ide_editor_perspective_get_id (IdePerspective *perspective)
{
  return g_strdup ("editor");
}

static gchar *
ide_editor_perspective_get_icon_name (IdePerspective *perspective)
{
  return g_strdup ("builder-editor-symbolic");
}

static gchar *
ide_editor_perspective_get_accelerator (IdePerspective *perspective)
{
  return g_strdup ("<Alt>1");
}

static gchar *
ide_editor_perspective_get_title (IdePerspective *perspective)
{
  return g_strdup (_("Editor"));
}

static void
ide_editor_perspective_restore_state (IdePerspective *perspective)
{
  IdeEditorPerspective *self = (IdeEditorPerspective *)perspective;
  g_assert (IDE_IS_EDITOR_PERSPECTIVE (self));
  ide_editor_perspective_restore_panel_state (self);
}

static gboolean
ide_editor_perspective_agree_to_shutdown (IdePerspective *perspective)
{
  IdeEditorPerspective *self = (IdeEditorPerspective *)perspective;

  g_assert (IDE_IS_EDITOR_PERSPECTIVE (self));

  ide_editor_perspective_save_panel_state (self);

  return TRUE;
}

static void
ide_editor_perspective_set_fullscreen (IdePerspective *perspective,
                                       gboolean        fullscreen)
{
  IdeEditorPerspective *self = (IdeEditorPerspective *)perspective;

  g_assert (IDE_IS_EDITOR_PERSPECTIVE (self));

  if (fullscreen)
    {
      gboolean left_visible;
      gboolean bottom_visible;

      g_object_get (self,
                    "left-visible", &left_visible,
                    "bottom-visible", &bottom_visible,
                    NULL);

      self->prefocus_had_left = left_visible;
      self->prefocus_had_bottom = bottom_visible;

      g_object_set (self,
                    "left-visible", FALSE,
                    "bottom-visible", FALSE,
                    NULL);
    }
  else
    {
      g_object_set (self,
                    "left-visible", self->prefocus_had_left,
                    "bottom-visible", self->prefocus_had_bottom,
                    NULL);
    }
}

static void
perspective_iface_init (IdePerspectiveInterface *iface)
{
  iface->agree_to_shutdown = ide_editor_perspective_agree_to_shutdown;
  iface->get_accelerator = ide_editor_perspective_get_accelerator;
  iface->get_icon_name = ide_editor_perspective_get_icon_name;
  iface->get_id = ide_editor_perspective_get_id;
  iface->get_title = ide_editor_perspective_get_title;
  iface->restore_state = ide_editor_perspective_restore_state;
  iface->views_foreach = ide_editor_perspective_views_foreach;
  iface->set_fullscreen = ide_editor_perspective_set_fullscreen;
}

void
_ide_editor_perspective_show_properties (IdeEditorPerspective *self,
                                         IdeEditorView        *view)
{
  IdeLayoutTransientSidebar *sidebar;

  g_return_if_fail (IDE_IS_EDITOR_PERSPECTIVE (self));
  g_return_if_fail (!view || IDE_IS_EDITOR_VIEW (view));

  sidebar = ide_editor_perspective_get_transient_sidebar (self);

  ide_editor_properties_set_view (self->properties, view);
  ide_layout_transient_sidebar_set_view (sidebar, (IdeLayoutView *)view);
  g_object_set (self, "right-visible", view != NULL, NULL);
}
