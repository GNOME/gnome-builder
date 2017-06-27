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

#include "editor/ide-editor-perspective.h"
#include "editor/ide-editor-private.h"
#include "editor/ide-editor-view.h"
#include "workbench/ide-perspective.h"

struct _IdeEditorPerspective
{
  IdeLayout      parent_instance;

  /* Template widgets */
  IdeLayoutGrid *grid;
};

static void perspective_iface_init (IdePerspectiveInterface *iface);

G_DEFINE_TYPE_WITH_CODE (IdeEditorPerspective, ide_editor_perspective, IDE_TYPE_LAYOUT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_PERSPECTIVE, perspective_iface_init))

static void
ide_editor_perspective_class_init (IdeEditorPerspectiveClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-editor-perspective.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeEditorPerspective, grid);
}

static void
ide_editor_perspective_init (IdeEditorPerspective *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  _ide_editor_perspective_init_actions (self);
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

void
ide_editor_perspective_focus_location (IdeEditorPerspective *self,
                                       IdeSourceLocation    *location)
{
  g_return_if_fail (IDE_IS_EDITOR_PERSPECTIVE (self));
  g_return_if_fail (location != NULL);

  /* TODO: */
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

void
ide_editor_perspective_focus_buffer_in_current_stack (IdeEditorPerspective *self,
                                                      IdeBuffer            *buffer)
{
  IdeLayoutStack *stack;
  IdeEditorView *view;
  struct {
    IdeBuffer     *buffer;
    IdeLayoutView *view;
  } lookup = { buffer };

  g_return_if_fail (IDE_IS_EDITOR_PERSPECTIVE (self));
  g_return_if_fail (IDE_IS_BUFFER (buffer));

  stack = ide_layout_grid_get_current_stack (self->grid);

  ide_layout_stack_foreach_view (stack, locate_view_for_buffer, &lookup);

  if (lookup.view != NULL)
    {
      ide_layout_stack_set_visible_child (stack, lookup.view);
      gtk_widget_grab_focus (GTK_WIDGET (lookup.view));
      return;
    }

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
 * ide_editor_perspective_get_right_edge:
 *
 * Returns: (transfer none): A #GtkWidget
 */
GtkWidget *
ide_editor_perspective_get_right_edge (IdeEditorPerspective *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_PERSPECTIVE (self), NULL);
  return dzl_dock_bin_get_right_edge (DZL_DOCK_BIN (self));
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
  reveal = g_settings_get_boolean (settings, "right-visible");
  position = g_settings_get_int (settings, "right-position");
  dzl_dock_revealer_set_position (DZL_DOCK_REVEALER (pane), position);
  set_reveal_child_without_transition (DZL_DOCK_REVEALER (pane), reveal);

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
perspective_iface_init (IdePerspectiveInterface *iface)
{
  iface->agree_to_shutdown = ide_editor_perspective_agree_to_shutdown;
  iface->get_accelerator = ide_editor_perspective_get_accelerator;
  iface->get_icon_name = ide_editor_perspective_get_icon_name;
  iface->get_id = ide_editor_perspective_get_id;
  iface->get_title = ide_editor_perspective_get_title;
  iface->restore_state = ide_editor_perspective_restore_state;
  iface->views_foreach = ide_editor_perspective_views_foreach;
}
