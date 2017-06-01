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

#include <dazzle.h>
#include <glib/gi18n.h>

#include "ide-context.h"
#include "ide-debug.h"

#include "buffers/ide-buffer-manager.h"
#include "buffers/ide-buffer.h"
#include "editor/ide-editor-perspective.h"
#include "editor/ide-editor-spell-widget.h"
#include "editor/ide-editor-view.h"
#include "util/ide-gtk.h"
#include "workbench/ide-layout-grid.h"
#include "workbench/ide-layout-pane.h"
#include "workbench/ide-workbench.h"
#include "workbench/ide-workbench-header-bar.h"

#define OVERLAY_REVEAL_DURATION 300

struct _IdeEditorPerspective
{
  DzlDockOverlay         parent_instance;

  GtkWidget             *active_view;
  IdeLayout             *layout;
  IdeLayoutGrid         *grid;
  GSimpleActionGroup    *actions;

  DzlSignalGroup        *buffer_manager_signals;

  gint                   right_pane_position;
  guint                  spellchecker_opened : 1;
};

typedef struct
{
  IdeEditorPerspective *self;
  IdeSourceLocation    *location;
} FocusLocation;

static void ide_perspective_iface_init                 (IdePerspectiveInterface *iface);
static void ide_editor_perspective_add                 (GtkContainer            *container,
                                                        GtkWidget               *widget);
static void ide_editor_perspective_focus_location_full (IdeEditorPerspective    *self,
                                                        IdeSourceLocation       *location,
                                                        gboolean                 open_if_not_found);

G_DEFINE_TYPE_EXTENDED (IdeEditorPerspective, ide_editor_perspective, DZL_TYPE_DOCK_OVERLAY, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_PERSPECTIVE, ide_perspective_iface_init))

enum {
  PROP_0,
  PROP_ACTIVE_VIEW,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

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

  pane = dzl_dock_bin_get_left_edge (DZL_DOCK_BIN (self->layout));
  reveal = g_settings_get_boolean (settings, "left-visible");
  position = g_settings_get_int (settings, "left-position");
  dzl_dock_revealer_set_reveal_child (DZL_DOCK_REVEALER (pane), reveal);
  dzl_dock_revealer_set_position (DZL_DOCK_REVEALER (pane), position);

  pane = dzl_dock_bin_get_right_edge (DZL_DOCK_BIN (self->layout));
  reveal = g_settings_get_boolean (settings, "right-visible");
  position = g_settings_get_int (settings, "right-position");
  dzl_dock_revealer_set_reveal_child (DZL_DOCK_REVEALER (pane), reveal);
  dzl_dock_revealer_set_position (DZL_DOCK_REVEALER (pane), position);

  pane = dzl_dock_bin_get_bottom_edge (DZL_DOCK_BIN (self->layout));
  reveal = g_settings_get_boolean (settings, "bottom-visible");
  position = g_settings_get_int (settings, "bottom-position");
  dzl_dock_revealer_set_reveal_child (DZL_DOCK_REVEALER (pane), reveal);
  dzl_dock_revealer_set_position (DZL_DOCK_REVEALER (pane), position);
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

  pane = dzl_dock_bin_get_left_edge (DZL_DOCK_BIN (self->layout));
  position = dzl_dock_revealer_get_position (DZL_DOCK_REVEALER (pane));
  reveal = dzl_dock_revealer_get_reveal_child (DZL_DOCK_REVEALER (pane));
  g_settings_set_boolean (settings, "left-visible", reveal);
  g_settings_set_int (settings, "left-position", position);

  pane = dzl_dock_bin_get_right_edge (DZL_DOCK_BIN (self->layout));
  position = dzl_dock_revealer_get_position (DZL_DOCK_REVEALER (pane));
  reveal = dzl_dock_revealer_get_reveal_child (DZL_DOCK_REVEALER (pane));
  g_settings_set_boolean (settings, "right-visible", reveal);
  g_settings_set_int (settings, "right-position", position);

  pane = dzl_dock_bin_get_bottom_edge (DZL_DOCK_BIN (self->layout));
  position = dzl_dock_revealer_get_position (DZL_DOCK_REVEALER (pane));
  reveal = dzl_dock_revealer_get_reveal_child (DZL_DOCK_REVEALER (pane));
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

  dzl_signal_group_set_target (self->buffer_manager_signals, buffer_manager);
}

static void
ide_editor_perspective_load_buffer (IdeEditorPerspective *self,
                                    IdeBuffer            *buffer,
                                    gboolean              create_new_view,
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
  if (!create_new_view)
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
ide_editor_perspective_view_destroyed (IdeEditorPerspective *self,
                                       IdeLayoutView        *view)
{
  IDE_ENTRY;

  g_assert (IDE_IS_EDITOR_PERSPECTIVE (self));
  g_assert (IDE_IS_LAYOUT_VIEW (view));

  g_signal_emit (self, signals [VIEW_REMOVED], 0, view);

  IDE_EXIT;
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
      g_signal_connect_object (widget,
                               "destroy",
                               G_CALLBACK (ide_editor_perspective_view_destroyed),
                               self,
                               G_CONNECT_SWAPPED);
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
ide_editor_perspective_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  IdeEditorPerspective *self = IDE_EDITOR_PERSPECTIVE (object);

  switch (prop_id)
    {
    case PROP_ACTIVE_VIEW:
      g_value_set_object (value, ide_editor_perspective_get_active_view (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
ide_editor_perspective_class_init (IdeEditorPerspectiveClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->get_property = ide_editor_perspective_get_property;
  object_class->finalize = ide_editor_perspective_finalize;

  container_class->add = ide_editor_perspective_add;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-editor-perspective.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeEditorPerspective, layout);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorPerspective, actions);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorPerspective, grid);

  properties [PROP_ACTIVE_VIEW] =
    g_param_spec_object ("active-view",
                         "Active View",
                         "Active View",
                         GTK_TYPE_WIDGET,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

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
ide_editor_perspective_active_view_notify_cb (IdeEditorPerspective *self,
                                              GParamSpec           *pspec,
                                              IdeLayout            *layout)
{
  g_assert (IDE_IS_EDITOR_PERSPECTIVE (self));
  g_assert (IDE_IS_LAYOUT (layout));

  self->active_view = ide_layout_get_active_view (layout);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ACTIVE_VIEW]);
}

static void
ide_editor_perspective_init (IdeEditorPerspective *self)
{
  static const gchar *proxy_actions[] = {
    "bottom-visible",
    "left-visible",
    "right-visible",
    NULL
  };
  static const GActionEntry entries[] = {
    { "new-file", new_file_activate },
  };

  GActionGroup *actions;
  guint i;

  self->buffer_manager_signals = dzl_signal_group_new (IDE_TYPE_BUFFER_MANAGER);

  dzl_signal_group_connect_object (self->buffer_manager_signals,
                                   "load-buffer",
                                   G_CALLBACK (ide_editor_perspective_load_buffer),
                                   self,
                                   G_CONNECT_SWAPPED);

  dzl_signal_group_connect_object (self->buffer_manager_signals,
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

  actions = gtk_widget_get_action_group (GTK_WIDGET (self->layout), "dockbin");

  for (i = 0; proxy_actions[i]; i++)
    {
      GAction *action;

      action = g_action_map_lookup_action (G_ACTION_MAP (actions), proxy_actions[i]);
      g_action_map_add_action (G_ACTION_MAP (self->actions), action);
    }

  ide_editor_perspective_restore_panel_state (self);

  ide_widget_set_context_handler (GTK_WIDGET (self),
                                  ide_editor_perspective_context_set);

  g_signal_connect_swapped (self->layout,
                            "notify::active-view",
                            G_CALLBACK (ide_editor_perspective_active_view_notify_cb),
                            self);

  ide_editor_perspective_active_view_notify_cb (self, NULL, self->layout);
}

static gchar *
ide_editor_perspective_get_title (IdePerspective *perspective)
{
  return g_strdup (_("Editor"));
}

static gchar *
ide_editor_perspective_get_icon_name (IdePerspective *perspective)
{
  return g_strdup ("builder-editor-symbolic");
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

static gchar *
ide_editor_perspective_get_accelerator (IdePerspective *perspective)
{
  return g_strdup ("<alt>1");
}

static void
ide_perspective_iface_init (IdePerspectiveInterface *iface)
{
  iface->agree_to_shutdown = ide_editor_perspective_agree_to_shutdown;
  iface->get_actions = ide_editor_perspective_get_actions;
  iface->get_icon_name = ide_editor_perspective_get_icon_name;
  iface->get_id = ide_editor_perspective_get_id;
  iface->get_title = ide_editor_perspective_get_title;
  iface->views_foreach = ide_editor_perspective_views_foreach;
  iface->get_accelerator = ide_editor_perspective_get_accelerator;
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

  stack = gtk_widget_get_ancestor (GTK_WIDGET (lookup.view), IDE_TYPE_LAYOUT_STACK);
  ide_layout_stack_set_active_view (IDE_LAYOUT_STACK (stack), GTK_WIDGET (lookup.view));
  ide_layout_view_navigate_to (IDE_LAYOUT_VIEW (lookup.view), location);
  gtk_widget_grab_focus (GTK_WIDGET (lookup.view));
}

void
ide_editor_perspective_focus_location (IdeEditorPerspective *self,
                                       IdeSourceLocation    *location)
{
  ide_editor_perspective_focus_location_full (self, location, TRUE);
}

/**
 * ide_editor_perspective_get_layout:
 * @self: A #IdeEditorPerspective
 *
 * Gets the #IdeLayout widget for the editor perspective.
 *
 * Returns: (transfer none) (nullable): A #IdeLayout or %NULL.
 */
IdeLayout *
ide_editor_perspective_get_layout (IdeEditorPerspective *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_PERSPECTIVE (self), NULL);

  return self->layout;
}

/**
 * ide_editor_perspective_get_active_view:
 *
 * Returns: (transfer none) (nullable): An #IdeLayoutView or %NULL.
 */
GtkWidget *
ide_editor_perspective_get_active_view (IdeEditorPerspective *self)
{

  g_return_val_if_fail (IDE_IS_EDITOR_PERSPECTIVE (self), NULL);

  return self->active_view;
}

/**
 * ide_editor_perspective_get_center_widget:
 * @self: A #IdeEditorPerspective
 *
 * Gets the center widget for the editor perspective.
 *
 * Returns: (transfer none) (nullable): A #GtkWidget or %NULL.
 */
GtkWidget *
ide_editor_perspective_get_center_widget (IdeEditorPerspective *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_PERSPECTIVE (self), NULL);

  return dzl_dock_bin_get_center_widget (DZL_DOCK_BIN (self->layout));
}

/**
 * ide_editor_perspective_get_top_edge:
 * Returns: (transfer none): A #GtkWidget
 */
GtkWidget *
ide_editor_perspective_get_top_edge (IdeEditorPerspective *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_PERSPECTIVE (self), NULL);

  return dzl_dock_bin_get_top_edge (DZL_DOCK_BIN (self->layout));
}

/**
 * ide_editor_perspective_get_left_edge:
 * Returns: (transfer none): A #GtkWidget
 */
GtkWidget *
ide_editor_perspective_get_left_edge (IdeEditorPerspective *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_PERSPECTIVE (self), NULL);

  return dzl_dock_bin_get_left_edge (DZL_DOCK_BIN (self->layout));
}

/**
 * ide_editor_perspective_get_bottom_edge:
 * Returns: (transfer none): A #GtkWidget
 */
GtkWidget *
ide_editor_perspective_get_bottom_edge (IdeEditorPerspective *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_PERSPECTIVE (self), NULL);

  return dzl_dock_bin_get_bottom_edge (DZL_DOCK_BIN (self->layout));
}

/**
 * ide_editor_perspective_get_right_edge:
 * Returns: (transfer none): A #GtkWidget
 */
GtkWidget *
ide_editor_perspective_get_right_edge (IdeEditorPerspective *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_PERSPECTIVE (self), NULL);

  return dzl_dock_bin_get_right_edge (DZL_DOCK_BIN (self->layout));
}

/**
 * ide_editor_perspective_get_overlay_edge:
 * self: an #IdeEditorPerspective.
 * position: a #GtkPositionType.
 *
 * Returns: (transfer none): A #DzlDockOverlayEdge
 */
DzlDockOverlayEdge *
ide_editor_perspective_get_overlay_edge (IdeEditorPerspective *self,
                                         GtkPositionType       position)
{
  g_return_val_if_fail (IDE_IS_EDITOR_PERSPECTIVE (self), NULL);

  return dzl_dock_overlay_get_edge (DZL_DOCK_OVERLAY (self), position);
}

static GtkOrientation
get_orientation_from_position_type (GtkPositionType position_type)
{
  if (position_type == GTK_POS_LEFT || position_type == GTK_POS_RIGHT)
    return GTK_ORIENTATION_HORIZONTAL;
  else
    return GTK_ORIENTATION_VERTICAL;
}

/* Triggered at the start of the animation */
static void
overlay_child_reveal_notify_cb (IdeEditorPerspective *self,
                                GParamSpec           *pspec,
                                DzlDockOverlayEdge   *edge)
{
  IdeLayoutPane *pane;
  gboolean reveal;

  g_assert (IDE_IS_EDITOR_PERSPECTIVE (self));
  g_assert (DZL_IS_DOCK_OVERLAY_EDGE (edge));

  gtk_container_child_get (GTK_CONTAINER (self), GTK_WIDGET (edge),
                           "reveal", &reveal,
                           NULL);

  if (!reveal && self->spellchecker_opened)
    {
      g_signal_handlers_disconnect_by_func (edge,
                                            overlay_child_reveal_notify_cb,
                                            self);

      pane = IDE_LAYOUT_PANE (dzl_dock_bin_get_right_edge (DZL_DOCK_BIN (self->layout)));
      dzl_dock_revealer_animate_to_position (DZL_DOCK_REVEALER (pane),
                                             self->right_pane_position,
                                             OVERLAY_REVEAL_DURATION);
    }
}

/* Triggered at the end of the animation */
static void
overlay_child_revealed_notify_cb (IdeEditorPerspective *self,
                                  GParamSpec           *pspec,
                                  DzlDockOverlayEdge   *edge)
{
  GtkWidget *child;
  gboolean revealed;

  g_assert (IDE_IS_EDITOR_PERSPECTIVE (self));
  g_assert (DZL_IS_DOCK_OVERLAY_EDGE (edge));

  gtk_container_child_get (GTK_CONTAINER (self), GTK_WIDGET (edge),
                           "revealed", &revealed,
                           NULL);

  if (!revealed && self->spellchecker_opened)
    {
      g_signal_handlers_disconnect_by_func (edge,
                                            overlay_child_revealed_notify_cb,
                                            self);

      child = gtk_bin_get_child (GTK_BIN (edge));
      g_assert (child != NULL);
      gtk_container_remove (GTK_CONTAINER (edge), child);
      self->spellchecker_opened = FALSE;
    }
  else if (revealed)
    self->spellchecker_opened = TRUE;
}

static void
show_spell_checker (IdeEditorPerspective *self,
                    DzlDockOverlayEdge   *overlay_edge,
                    IdeLayoutPane        *pane)
{
  GtkOrientation pane_orientation;
  GtkPositionType pane_position_type;
  GtkOrientation overlay_orientation;
  GtkPositionType overlay_position_type;
  gint overlay_size;

  g_assert (IDE_IS_EDITOR_PERSPECTIVE (self));
  g_assert (gtk_bin_get_child (GTK_BIN (overlay_edge)) != NULL);

  pane_position_type = dzl_dock_bin_edge_get_edge (DZL_DOCK_BIN_EDGE (pane));
  overlay_position_type = dzl_dock_overlay_edge_get_edge (overlay_edge);

  pane_orientation = get_orientation_from_position_type (pane_position_type);
  overlay_orientation = get_orientation_from_position_type (overlay_position_type);

  g_assert (pane_orientation == overlay_orientation);

  if (dzl_dock_revealer_get_position_set (DZL_DOCK_REVEALER (pane)))
    self->right_pane_position = dzl_dock_revealer_get_position (DZL_DOCK_REVEALER (pane));
  else
    {
      if (overlay_orientation == GTK_ORIENTATION_HORIZONTAL)
        gtk_widget_get_preferred_width (GTK_WIDGET (pane), NULL, &self->right_pane_position);
      else
        gtk_widget_get_preferred_height (GTK_WIDGET (pane), NULL, &self->right_pane_position);
    }

  if (overlay_orientation == GTK_ORIENTATION_HORIZONTAL)
    gtk_widget_get_preferred_width (GTK_WIDGET (overlay_edge), NULL, &overlay_size);
  else
    gtk_widget_get_preferred_height (GTK_WIDGET (overlay_edge), NULL, &overlay_size);

  g_signal_connect_object (overlay_edge,
                           "child-notify::reveal",
                           G_CALLBACK (overlay_child_reveal_notify_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (overlay_edge,
                           "child-notify::revealed",
                           G_CALLBACK (overlay_child_revealed_notify_cb),
                           self,
                           G_CONNECT_SWAPPED);

  dzl_dock_revealer_animate_to_position (DZL_DOCK_REVEALER (pane),
                                         overlay_size,
                                         OVERLAY_REVEAL_DURATION);
  gtk_container_child_set (GTK_CONTAINER (self), GTK_WIDGET (overlay_edge),
                           "reveal", TRUE,
                           NULL);
}

static GtkWidget *
create_spellchecker_widget (IdeSourceView *source_view)
{
  GtkWidget *spellchecker_widget;
  GtkWidget *scroll_window;
  GtkWidget *spell_widget;

  g_assert (IDE_IS_SOURCE_VIEW (source_view));

  spellchecker_widget = g_object_new (GTK_TYPE_BOX,
                                      "visible", TRUE,
                                      "expand", TRUE,
                                      NULL);
  scroll_window = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                                "visible", TRUE,
                                "expand", TRUE,
                                "propagate-natural-width", TRUE,
                                NULL);
  spell_widget = ide_editor_spell_widget_new (source_view);
  gtk_box_pack_start (GTK_BOX (spellchecker_widget), scroll_window, TRUE, TRUE, 0);
  gtk_container_add (GTK_CONTAINER (scroll_window), spell_widget);
  gtk_widget_show_all (spellchecker_widget);

  return spellchecker_widget;
}

void
ide_editor_perspective_show_spellchecker (IdeEditorPerspective *self,
                                          IdeSourceView        *source_view)
{
  GtkWidget *spellchecker_widget;
  DzlDockOverlayEdge *overlay_edge;
  IdeLayoutPane *pane;

  g_return_if_fail (IDE_IS_EDITOR_PERSPECTIVE (self));
  g_return_if_fail (IDE_IS_SOURCE_VIEW (source_view));

  if (!self->spellchecker_opened)
    {
      self->spellchecker_opened = TRUE;
      spellchecker_widget = create_spellchecker_widget (source_view);

      dzl_overlay_add_child (DZL_DOCK_OVERLAY (self), spellchecker_widget, "right");
      overlay_edge = ide_editor_perspective_get_overlay_edge (self, GTK_POS_RIGHT);
      gtk_widget_set_child_visible (GTK_WIDGET (overlay_edge), TRUE);

      pane = IDE_LAYOUT_PANE (dzl_dock_bin_get_right_edge (DZL_DOCK_BIN (self->layout)));
      show_spell_checker (self, overlay_edge, pane);
    }
}
