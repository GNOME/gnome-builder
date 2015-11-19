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

  EggSignalGroup        *buffer_manager_signals;
};

static void ide_perspective_iface_init (IdePerspectiveInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdeEditorPerspective, ide_editor_perspective, IDE_TYPE_LAYOUT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_PERSPECTIVE, ide_perspective_iface_init))

enum {
  PROP_0,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static void
ide_editor_perspective_restore_panel_state (IdeEditorPerspective *self)
{
  g_autoptr(GSettings) settings = NULL;
  GtkWidget *pane;
  gboolean reveal;
  guint position;

  g_assert (IDE_IS_EDITOR_PERSPECTIVE (self));

  settings = g_settings_new ("org.gnome.builder.workbench");

  pane = ide_layout_get_left_pane (IDE_LAYOUT (self));
  reveal = g_settings_get_boolean (settings, "left-visible");
  position = g_settings_get_int (settings, "left-position");
  gtk_container_child_set (GTK_CONTAINER (self), pane,
                           "position", position,
                           "reveal", reveal,
                           NULL);

  pane = ide_layout_get_right_pane (IDE_LAYOUT (self));
  reveal = g_settings_get_boolean (settings, "right-visible");
  position = g_settings_get_int (settings, "right-position");
  gtk_container_child_set (GTK_CONTAINER (self), pane,
                           "position", position,
                           "reveal", reveal,
                           NULL);

  pane = ide_layout_get_bottom_pane (IDE_LAYOUT (self));
  reveal = g_settings_get_boolean (settings, "bottom-visible");
  position = g_settings_get_int (settings, "bottom-position");
  gtk_container_child_set (GTK_CONTAINER (self), pane,
                           "position", position,
                           "reveal", reveal,
                           NULL);
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
                                    IdeBufferManager     *buffer_manager)
{
  IdeEditorView *view;
  GtkWidget *stack;

  g_assert (IDE_IS_EDITOR_PERSPECTIVE (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));

  IDE_TRACE_MSG ("Loading %s", ide_buffer_get_title (buffer));

  view = g_object_new (IDE_TYPE_EDITOR_VIEW,
                       "document", buffer,
                       "visible", TRUE,
                       NULL);

  stack = ide_layout_grid_get_last_focus (self->grid);

  gtk_container_add (GTK_CONTAINER (stack), GTK_WIDGET (view));
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
              ide_layout_stack_set_active_view (IDE_LAYOUT_STACK (stack), view);
              *buffer = NULL;
            }
        }
    }
}

static void
ide_editor_perspective_focus_buffer (IdeEditorPerspective *self,
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
ide_editor_perspective_finalize (GObject *object)
{
  IdeEditorPerspective *self = (IdeEditorPerspective *)object;

  g_clear_object (&self->buffer_manager_signals);

  G_OBJECT_CLASS (ide_editor_perspective_parent_class)->finalize (object);
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
      return;
    }

  GTK_CONTAINER_CLASS (ide_editor_perspective_parent_class)->add (container, widget);
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
  gtk_widget_class_bind_template_child (widget_class, IdeEditorPerspective, grid);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorPerspective, titlebar);
}

static void
ide_editor_perspective_init (IdeEditorPerspective *self)
{
  GActionGroup *actions;

  self->buffer_manager_signals = egg_signal_group_new (IDE_TYPE_BUFFER_MANAGER);

  egg_signal_group_connect_object (self->buffer_manager_signals,
                                   "load-buffer",
                                   G_CALLBACK (ide_editor_perspective_load_buffer),
                                   self,
                                   G_CONNECT_SWAPPED);

  egg_signal_group_connect_object (self->buffer_manager_signals,
                                   "notify::focus-buffer",
                                   G_CALLBACK (ide_editor_perspective_focus_buffer),
                                   self,
                                   G_CONNECT_SWAPPED);

  gtk_widget_init_template (GTK_WIDGET (self));

  actions = gtk_widget_get_action_group (GTK_WIDGET (self), "panels");
  gtk_widget_insert_action_group (GTK_WIDGET (self->titlebar), "panels", actions);

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

static void
ide_perspective_iface_init (IdePerspectiveInterface *iface)
{
  iface->get_id = ide_editor_perspective_get_id;
  iface->get_title = ide_editor_perspective_get_title;
  iface->get_titlebar = ide_editor_perspective_get_titlebar;
  iface->get_icon_name = ide_editor_perspective_get_icon_name;
  iface->views_foreach = ide_editor_perspective_views_foreach;
}
