/* ide-editor-perspective.c
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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

#include "egg-signal-group.h"

#include "ide-buffer.h"
#include "ide-buffer-manager.h"
#include "ide-context.h"
#include "ide-debug.h"
#include "ide-editor-perspective.h"
#include "ide-editor-view.h"
#include "ide-gtk.h"
#include "ide-layout-grid.h"
#include "ide-workbench-header-bar.h"

struct _IdeEditorPerspective
{
  IdeLayout              parent_instance;

  IdeLayoutGrid         *grid;
  IdeWorkbenchHeaderBar *titlebar;
  GSimpleActionGroup    *actions;

  EggSignalGroup        *buffer_manager_signals;
};

typedef struct
{
  IdeEditorPerspective *self;
  IdeSourceLocation    *location;
} FocusLocation;

static void ide_perspective_iface_init (IdePerspectiveInterface *iface);

static void ide_editor_perspective_add (GtkContainer *container, GtkWidget *widget);

G_DEFINE_TYPE_EXTENDED (IdeEditorPerspective, ide_editor_perspective, IDE_TYPE_LAYOUT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_PERSPECTIVE, ide_perspective_iface_init))

enum {
  VIEW_ADDED,
  VIEW_REMOVED,
  LAST_SIGNAL
};

static guint signals [LAST_SIGNAL];

static void
ide_editor_perspective_restore_panel_state (IdeEditorPerspective *self)
{
  g_autoptr(GSettings) settings = NULL;
  GtkWidget *pane;
  gboolean reveal;
  guint position;

  g_assert (IDE_IS_EDITOR_PERSPECTIVE (self));

  settings = g_settings_new ("org.gnome.builder.workbench");

  pane = pnl_dock_bin_get_left_edge (PNL_DOCK_BIN (self));
  reveal = g_settings_get_boolean (settings, "left-visible");
  position = g_settings_get_int (settings, "left-position");
  pnl_dock_revealer_set_reveal_child (PNL_DOCK_REVEALER (pane), reveal);
  pnl_dock_revealer_set_position (PNL_DOCK_REVEALER (pane), position);

  pane = pnl_dock_bin_get_right_edge (PNL_DOCK_BIN (self));
  reveal = g_settings_get_boolean (settings, "right-visible");
  position = g_settings_get_int (settings, "right-position");
  pnl_dock_revealer_set_reveal_child (PNL_DOCK_REVEALER (pane), reveal);
  pnl_dock_revealer_set_position (PNL_DOCK_REVEALER (pane), position);

  pane = pnl_dock_bin_get_bottom_edge (PNL_DOCK_BIN (self));
  reveal = g_settings_get_boolean (settings, "bottom-visible");
  position = g_settings_get_int (settings, "bottom-position");
  pnl_dock_revealer_set_reveal_child (PNL_DOCK_REVEALER (pane), reveal);
  pnl_dock_revealer_set_position (PNL_DOCK_REVEALER (pane), position);
}

static void
ide_editor_perspective_save_panel_state (IdeEditorPerspective *self)
{
  g_autoptr(GSettings) settings = NULL;
  GtkWidget *pane;
  gboolean reveal;
  guint position;

  g_assert (IDE_IS_EDITOR_PERSPECTIVE (self));

  settings = g_settings_new ("org.gnome.builder.workbench");

  pane = pnl_dock_bin_get_left_edge (PNL_DOCK_BIN (self));
  position = pnl_dock_revealer_get_position (PNL_DOCK_REVEALER (pane));
  reveal = pnl_dock_revealer_get_reveal_child (PNL_DOCK_REVEALER (pane));
  g_settings_set_boolean (settings, "left-visible", reveal);
  g_settings_set_int (settings, "left-position", position);

  pane = pnl_dock_bin_get_right_edge (PNL_DOCK_BIN (self));
  position = pnl_dock_revealer_get_position (PNL_DOCK_REVEALER (pane));
  reveal = pnl_dock_revealer_get_reveal_child (PNL_DOCK_REVEALER (pane));
  g_settings_set_boolean (settings, "right-visible", reveal);
  g_settings_set_int (settings, "right-position", position);

  pane = pnl_dock_bin_get_bottom_edge (PNL_DOCK_BIN (self));
  position = pnl_dock_revealer_get_position (PNL_DOCK_REVEALER (pane));
  reveal = pnl_dock_revealer_get_reveal_child (PNL_DOCK_REVEALER (pane));
  g_settings_set_boolean (settings, "bottom-visible", reveal);
  g_settings_set_int (settings, "bottom-position", position);
}

static void
ide_editor_perspective_context_set (GtkWidget  *widget,
                                    IdeContext *context)
{
  IdeEditorPerspective *self = (IdeEditorPerspective *)widget;
  IdeBufferManager *buffer_manager = NULL;

  g_assert (IDE_IS_EDITOR_PERSPECTIVE (self));
  g_assert (!context || IDE_IS_CONTEXT (context));

  if (context != NULL)
    buffer_manager = ide_context_get_buffer_manager (context);

  egg_signal_group_set_target (self->buffer_manager_signals, buffer_manager);
}

static void
ide_editor_perspective_load_buffer (IdeEditorPerspective *self,
                                    IdeBuffer            *buffer,
                                    gboolean              reloading,
                                    IdeBufferManager     *buffer_manager)
{
  IdeEditorView *view;
  GtkWidget *stack;
  IdeWorkbench *workbench;

  g_assert (IDE_IS_EDITOR_PERSPECTIVE (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));

  /*
   * We only want to create a new view when the buffer is originally
   * created, not when it's reloaded.
   */
  if (reloading)
    {
      ide_buffer_manager_set_focus_buffer (buffer_manager, buffer);
      return;
    }

  IDE_TRACE_MSG ("Loading %s", ide_buffer_get_title (buffer));

  view = g_object_new (IDE_TYPE_EDITOR_VIEW,
                       "document", buffer,
                       "visible", TRUE,
                       NULL);

  stack = ide_layout_grid_get_last_focus (self->grid);

  ide_editor_perspective_add (GTK_CONTAINER (self), GTK_WIDGET (view));

  workbench = ide_widget_get_workbench (GTK_WIDGET (stack));
  ide_workbench_focus (workbench, GTK_WIDGET (view));
}

static void
ide_editor_perspective_locate_buffer (GtkWidget *view,
                                      gpointer   user_data)
{
  IdeBuffer **buffer = user_data;

  g_assert (IDE_IS_LAYOUT_VIEW (view));
  g_assert (buffer != NULL);
  g_assert (!*buffer || IDE_IS_BUFFER (*buffer));

  if (!*buffer)
    return;

  if (IDE_IS_EDITOR_VIEW (view))
    {
      if (*buffer == ide_editor_view_get_document (IDE_EDITOR_VIEW (view)))
        {
          GtkWidget *stack;

          stack = gtk_widget_get_ancestor (view, IDE_TYPE_LAYOUT_STACK);

          if (stack != NULL)
            {
              IdeWorkbench *workbench;

              ide_layout_stack_set_active_view (IDE_LAYOUT_STACK (stack), view);
              *buffer = NULL;

              workbench = ide_widget_get_workbench (GTK_WIDGET (stack));
              ide_workbench_focus (workbench, GTK_WIDGET (view));
            }
        }
    }
}

void
ide_editor_perspective_focus_buffer_in_current_stack (IdeEditorPerspective *self,
                                                      IdeBuffer            *buffer)
{
  GtkWidget *focus_stack;

  g_assert (IDE_IS_EDITOR_PERSPECTIVE (self));
  g_assert (IDE_IS_BUFFER (buffer));

  focus_stack = ide_layout_grid_get_last_focus (self->grid);
  g_assert (!focus_stack || IDE_IS_LAYOUT_STACK (focus_stack));

  if (focus_stack != NULL)
    {
      IdeBuffer *search_buffer = buffer;
      GtkWidget *view;

      ide_layout_stack_foreach_view (IDE_LAYOUT_STACK (focus_stack),
                                     ide_editor_perspective_locate_buffer,
                                     &search_buffer);

      if (search_buffer != NULL)
        {
          view = g_object_new (IDE_TYPE_EDITOR_VIEW,
                               "document", buffer,
                               "visible", TRUE,
                               NULL);
          ide_editor_perspective_add (GTK_CONTAINER (self), view);
        }
    }
}

static void
ide_editor_perspective_notify_focus_buffer (IdeEditorPerspective *self,
                                            GParamSpec           *pspec,
                                            IdeBufferManager     *buffer_manager)
{
  IdeBuffer *buffer;

  g_assert (IDE_IS_EDITOR_PERSPECTIVE (self));
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));

  buffer = ide_buffer_manager_get_focus_buffer (buffer_manager);
  if (buffer == NULL)
    return;

  ide_layout_grid_foreach_view (self->grid,
                                ide_editor_perspective_locate_buffer,
                                &buffer);
}

static void
global_search_activate (GSimpleAction *action,
                        GVariant      *param,
                        gpointer       user_data)
{
  IdeEditorPerspective *self = user_data;

  g_assert (IDE_IS_EDITOR_PERSPECTIVE (self));

  ide_workbench_header_bar_focus_search (self->titlebar);
}

static void
new_file_activate (GSimpleAction *action,
                   GVariant      *param,
                   gpointer       user_data)
{
  IdeEditorPerspective *self = user_data;
  IdeWorkbench *workbench;
  IdeContext *context;
  IdeBufferManager *bufmgr;
  IdeBuffer *buffer;

  g_assert (IDE_IS_EDITOR_PERSPECTIVE (self));

  workbench = ide_widget_get_workbench (GTK_WIDGET (self));
  context = ide_workbench_get_context (workbench);
  bufmgr = ide_context_get_buffer_manager (context);
  buffer = ide_buffer_manager_create_temporary_buffer (bufmgr);

  g_clear_object (&buffer);
}

static void
ide_editor_perspective_finalize (GObject *object)
{
  IdeEditorPerspective *self = (IdeEditorPerspective *)object;

  g_clear_object (&self->buffer_manager_signals);

  G_OBJECT_CLASS (ide_editor_perspective_parent_class)->finalize (object);
}

static void
ide_editor_perspective_view_weak_cb (IdeEditorPerspective *self,
                                     IdeLayoutView        *view)
{
  g_signal_emit (self, signals [VIEW_REMOVED], 0, view);
}

static void
ide_editor_perspective_add (GtkContainer *container,
                            GtkWidget    *widget)
{
  IdeEditorPerspective *self = (IdeEditorPerspective *)container;

  g_assert (IDE_IS_EDITOR_PERSPECTIVE (self));
  g_assert (GTK_IS_WIDGET (widget));

  if (IDE_IS_LAYOUT_VIEW (widget))
    {
      GtkWidget *last_focus;

      last_focus = ide_layout_grid_get_last_focus (self->grid);
      gtk_container_add (GTK_CONTAINER (last_focus), widget);
      g_object_weak_ref (G_OBJECT (widget),
                         (GWeakNotify)ide_editor_perspective_view_weak_cb,
                         container);

      g_signal_emit (self, signals [VIEW_ADDED], 0, widget);
      return;
    }

  GTK_CONTAINER_CLASS (ide_editor_perspective_parent_class)->add (container, widget);
}

static void
ide_editor_perspective_grid_empty (IdeEditorPerspective *self,
                                   IdeLayoutGrid        *grid)
{
  GtkWidget *stack;

  g_assert (IDE_IS_EDITOR_PERSPECTIVE (self));
  g_assert (IDE_IS_LAYOUT_GRID (grid));

  stack = gtk_widget_get_ancestor (GTK_WIDGET (grid), GTK_TYPE_STACK);

  if (stack != NULL)
    gtk_stack_set_visible_child_name (GTK_STACK (stack), "empty_state");
}

static void
ide_editor_perspective_class_init (IdeEditorPerspectiveClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->finalize = ide_editor_perspective_finalize;

  container_class->add = ide_editor_perspective_add;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-editor-perspective.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeEditorPerspective, actions);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorPerspective, grid);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorPerspective, titlebar);

  signals[VIEW_ADDED] =
    g_signal_new ("view-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  GTK_TYPE_WIDGET);

  signals[VIEW_REMOVED] =
    g_signal_new ("view-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  GTK_TYPE_WIDGET);
}

static void
ide_editor_perspective_init (IdeEditorPerspective *self)
{
  GActionGroup *actions;
  static const GActionEntry entries[] = {
    { "new-file", new_file_activate },
    { "global-search", global_search_activate },
  };

  self->buffer_manager_signals = egg_signal_group_new (IDE_TYPE_BUFFER_MANAGER);

  egg_signal_group_connect_object (self->buffer_manager_signals,
                                   "load-buffer",
                                   G_CALLBACK (ide_editor_perspective_load_buffer),
                                   self,
                                   G_CONNECT_SWAPPED);

  egg_signal_group_connect_object (self->buffer_manager_signals,
                                   "notify::focus-buffer",
                                   G_CALLBACK (ide_editor_perspective_notify_focus_buffer),
                                   self,
                                   G_CONNECT_SWAPPED);

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self->grid,
                           "empty",
                           G_CALLBACK (ide_editor_perspective_grid_empty),
                           self,
                           G_CONNECT_SWAPPED);

  g_action_map_add_action_entries (G_ACTION_MAP (self->actions), entries,
                                   G_N_ELEMENTS (entries), self);

  actions = gtk_widget_get_action_group (GTK_WIDGET (self), "dockbin");
  gtk_widget_insert_action_group (GTK_WIDGET (self->titlebar), "dockbin", actions);

  ide_editor_perspective_restore_panel_state (self);

  ide_widget_set_context_handler (GTK_WIDGET (self),
                                  ide_editor_perspective_context_set);
}

static gchar *
ide_editor_perspective_get_title (IdePerspective *perspective)
{
  return g_strdup (_("Editor"));
}

static GtkWidget *
ide_editor_perspective_get_titlebar (IdePerspective *perspective)
{
  return GTK_WIDGET (IDE_EDITOR_PERSPECTIVE (perspective)->titlebar);
}

static gchar *
ide_editor_perspective_get_icon_name (IdePerspective *perspective)
{
  return g_strdup ("text-editor-symbolic");
}

static gchar *
ide_editor_perspective_get_id (IdePerspective *perspective)
{
  return g_strdup ("editor");
}

static void
ide_editor_perspective_views_foreach (IdePerspective *perspective,
                                      GtkCallback     callback,
                                      gpointer        user_data)
{
  IdeEditorPerspective *self = (IdeEditorPerspective *)perspective;

  g_assert (IDE_IS_EDITOR_PERSPECTIVE (self));

  ide_layout_grid_foreach_view (self->grid, callback, user_data);
}

static GActionGroup *
ide_editor_perspective_get_actions (IdePerspective *perspective)
{
  IdeEditorPerspective *self = (IdeEditorPerspective *)perspective;

  g_return_val_if_fail (IDE_IS_EDITOR_PERSPECTIVE (self), NULL);

  return g_object_ref (self->actions);
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
ide_perspective_iface_init (IdePerspectiveInterface *iface)
{
  iface->agree_to_shutdown = ide_editor_perspective_agree_to_shutdown;
  iface->get_actions = ide_editor_perspective_get_actions;
  iface->get_icon_name = ide_editor_perspective_get_icon_name;
  iface->get_id = ide_editor_perspective_get_id;
  iface->get_title = ide_editor_perspective_get_title;
  iface->get_titlebar = ide_editor_perspective_get_titlebar;
  iface->views_foreach = ide_editor_perspective_views_foreach;
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

  buffer = ide_editor_view_get_document (IDE_EDITOR_VIEW (widget));
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
  ide_editor_perspective_focus_location (state->self, state->location);

cleanup:
  g_object_unref (state->self);
  ide_source_location_unref (state->location);
  g_slice_free (FocusLocation, state);
}

void
ide_editor_perspective_focus_location (IdeEditorPerspective *self,
                                       IdeSourceLocation    *location)
{
  struct {
    IdeFile *file;
    IdeEditorView *view;
  } lookup = { 0 };
  GtkWidget *stack;

  g_return_if_fail (IDE_IS_EDITOR_PERSPECTIVE (self));
  g_return_if_fail (location != NULL);

  lookup.file = ide_source_location_get_file (location);
  lookup.view = NULL;

  ide_perspective_views_foreach (IDE_PERSPECTIVE (self),
                                 ide_editor_perspective_find_source_location,
                                 &lookup);

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
                                          NULL,
                                          NULL,
                                          ide_editor_perspective_focus_location_cb,
                                          state);
      return;
    }

  stack = gtk_widget_get_ancestor (GTK_WIDGET (lookup.view), IDE_TYPE_LAYOUT_STACK);
  ide_layout_stack_set_active_view (IDE_LAYOUT_STACK (stack), GTK_WIDGET (lookup.view));
  ide_layout_view_navigate_to (IDE_LAYOUT_VIEW (lookup.view), location);
  gtk_widget_grab_focus (GTK_WIDGET (lookup.view));
}
