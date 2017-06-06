/* ide-layout-tab-bar.c
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

#define G_LOG_DOMAIN "ide-layout-tab-bar"

#include <dazzle.h>

#include "workbench/ide-layout-stack.h"
#include "workbench/ide-layout-tab-bar.h"
#include "workbench/ide-layout-tab-bar-private.h"
#include "workbench/ide-layout-tab.h"
#include "workbench/ide-layout-view.h"
#include "workbench/ide-workbench-private.h"
#include "workbench/ide-workbench.h"

G_DEFINE_TYPE (IdeLayoutTabBar, ide_tab_layout_bar, GTK_TYPE_EVENT_BOX)

enum {
  PROP_0,
  PROP_STACK,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static void
ide_layout_tab_bar_close_clicked (IdeLayoutTabBar *self,
                                  GtkButton       *button)
{
  GtkWidget *row;
  GtkWidget *view;

  g_assert (IDE_IS_LAYOUT_TAB_BAR (self));
  g_assert (GTK_IS_BUTTON (button));

  row = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_LIST_BOX_ROW);
  g_assert (row != NULL);

  view = g_object_get_data (G_OBJECT (row), "IDE_LAYOUT_VIEW");
  g_assert (IDE_IS_LAYOUT_VIEW (view));

  gtk_stack_set_visible_child (self->stack, view);
  dzl_gtk_widget_action (view, "view-stack", "close", NULL);
}

static GtkWidget *
create_row (IdeLayoutTabBar *self,
            IdeLayoutView   *view)
{
  GtkWidget *row;
  GtkWidget *box;
  GtkWidget *label;
  GtkWidget *modified;
  GtkWidget *expand;
  GtkWidget *button;

  g_assert (IDE_IS_LAYOUT_TAB_BAR (self));
  g_assert (IDE_IS_LAYOUT_VIEW (view));

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      "visible", TRUE,
                      NULL);

  box = g_object_new (GTK_TYPE_BOX,
                      "visible", TRUE,
                      NULL);

  label = g_object_new (GTK_TYPE_LABEL,
                        "ellipsize", PANGO_ELLIPSIZE_MIDDLE,
                        "visible", TRUE,
                        "xalign", 0.0f,
                        NULL);

  modified = g_object_new (GTK_TYPE_LABEL,
                           "margin-start", 6,
                           "label", "•",
                           NULL);

  expand = g_object_new (GTK_TYPE_LABEL,
                         "hexpand", TRUE,
                         "visible", TRUE,
                         NULL);

  button = g_object_new (GTK_TYPE_BUTTON,
                         "child", g_object_new (GTK_TYPE_IMAGE,
                                                "visible", TRUE,
                                                "icon-name", "window-close-symbolic",
                                                NULL),
                         "focus-on-click", FALSE,
                         "margin-start", 18,
                         "margin-end", 6,
                         "visible", TRUE,
                         NULL);

  g_signal_connect_object (button,
                           "clicked",
                           G_CALLBACK (ide_layout_tab_bar_close_clicked),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_container_add (GTK_CONTAINER (row), box);
  gtk_container_add (GTK_CONTAINER (box), label);
  gtk_container_add (GTK_CONTAINER (box), modified);
  gtk_container_add (GTK_CONTAINER (box), expand);
  gtk_container_add (GTK_CONTAINER (box), button);

  g_object_bind_property (view, "title", label, "label", G_BINDING_SYNC_CREATE);
  g_object_bind_property (view, "modified", modified, "visible", G_BINDING_SYNC_CREATE);

  g_object_set_data (G_OBJECT (row), "IDE_LAYOUT_VIEW", view);

  return row;
}

static void
ide_layout_tab_bar_add (IdeLayoutTabBar *self,
                        IdeLayoutView   *view,
                        GtkStack        *stack)
{
  g_assert (IDE_IS_LAYOUT_TAB_BAR (self));
  g_assert (IDE_IS_LAYOUT_VIEW (view));
  g_assert (GTK_IS_STACK (stack));

  self->child_count++;

  gtk_container_add (GTK_CONTAINER (self->views_list_box), create_row (self, view));

  if (self->child_count > 1)
    gtk_widget_show (GTK_WIDGET (self->views_list_button));

  gtk_widget_hide (GTK_WIDGET (self->tab_expander));
  gtk_widget_show (GTK_WIDGET (self->tab));
}

static void
find_row_cb (GtkWidget *widget,
             gpointer   user_data)
{
  struct {
    IdeLayoutView *view;
    GtkWidget     *row;
  } *lookup = user_data;
  IdeLayoutView *view;

  if (lookup->row != NULL)
    return;

  view = g_object_get_data (G_OBJECT (widget), "IDE_LAYOUT_VIEW");
  g_assert (view != NULL);

  if (lookup->view == view)
    lookup->row = widget;
}

GtkWidget *
find_row (IdeLayoutTabBar *self,
          IdeLayoutView   *view)
{
  struct {
    IdeLayoutView *view;
    GtkWidget     *row;
  } lookup = { view, NULL };

  g_assert (IDE_IS_LAYOUT_TAB_BAR (self));
  g_assert (IDE_IS_LAYOUT_VIEW (view));

  gtk_container_foreach (GTK_CONTAINER (self->views_list_box), find_row_cb, &lookup);

  return lookup.row;
}

static void
ide_layout_tab_bar_remove (IdeLayoutTabBar *self,
                           IdeLayoutView   *view,
                           GtkStack        *stack)
{
  GtkWidget *row;

  g_assert (IDE_IS_LAYOUT_TAB_BAR (self));
  g_assert (IDE_IS_LAYOUT_VIEW (view));
  g_assert (GTK_IS_STACK (stack));

  row = find_row (self, view);

  if (row != NULL)
    {
      gtk_container_remove (GTK_CONTAINER (self->views_list_box), row);

      self->child_count--;

      if (self->child_count <= 1)
        gtk_widget_hide (GTK_WIDGET (self->views_list_button));

      if (self->child_count == 0)
        {
          gtk_widget_hide (GTK_WIDGET (self->tab));
          gtk_widget_show (GTK_WIDGET (self->tab_expander));
        }
    }
}

static void
ide_layout_tab_bar_child_changed (IdeLayoutTabBar *self,
                                  GParamSpec      *pspec,
                                  GtkStack        *stack)
{
  GtkWidget *view;

  g_assert (IDE_IS_LAYOUT_TAB_BAR (self));
  g_assert (GTK_IS_STACK (stack));

  view = gtk_stack_get_visible_child (stack);

  if (IDE_IS_LAYOUT_VIEW (view))
    {
      GtkWidget *row = find_row (self, IDE_LAYOUT_VIEW (view));

      if (row != NULL)
        gtk_list_box_select_row (self->views_list_box, GTK_LIST_BOX_ROW (row));
    }
}

static void
ide_layout_tab_bar_row_selected (IdeLayoutTabBar *self,
                                 GtkListBoxRow   *row,
                                 GtkListBox      *list)
{
  GtkWidget *view;

  g_assert (IDE_IS_LAYOUT_TAB_BAR (self));
  g_assert (GTK_IS_LIST_BOX (list));
  g_assert (!row || GTK_IS_LIST_BOX_ROW (row));

  if (row == NULL)
    return;

  view = g_object_get_data (G_OBJECT (row), "IDE_LAYOUT_VIEW");

  if (view != NULL)
    {
      if (gtk_stack_get_visible_child (self->stack) != view)
        gtk_stack_set_visible_child (self->stack, view);
    }
}

static void
ide_layout_tab_bar_popover_closed (IdeLayoutTabBar *self,
                                   GtkPopover      *popover)
{
  GtkWidget *child;

  g_assert (IDE_IS_LAYOUT_TAB_BAR (self));

  child = gtk_stack_get_visible_child (self->stack);
  if (child != NULL)
    gtk_widget_grab_focus (child);
}

static void
ide_layout_tab_bar_set_stack (IdeLayoutTabBar *self,
                              GtkStack        *stack)
{
  g_assert (IDE_IS_LAYOUT_TAB_BAR (self));
  g_assert (GTK_IS_STACK (stack));

  self->stack = stack;

  g_signal_connect_object (stack,
                           "add",
                           G_CALLBACK (ide_layout_tab_bar_add),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (stack,
                           "remove",
                           G_CALLBACK (ide_layout_tab_bar_remove),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (stack,
                           "notify::visible-child",
                           G_CALLBACK (ide_layout_tab_bar_child_changed),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
ide_layout_tab_bar_hierarchy_changed (GtkWidget *widget,
                                      GtkWidget *old_toplevel)
{
  IdeLayoutTabBar *self = (IdeLayoutTabBar *)widget;
  GtkWidget *toplevel;

  g_assert (IDE_IS_LAYOUT_TAB_BAR (self));

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
  if (!GTK_IS_WINDOW (toplevel))
    toplevel = NULL;

  if (IDE_IS_WORKBENCH (toplevel))
    gtk_size_group_add_widget (IDE_WORKBENCH (toplevel)->header_size_group, widget);
}

static void
ide_layout_tab_bar_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  IdeLayoutTabBar *self = IDE_LAYOUT_TAB_BAR(object);

  switch (prop_id)
    {
    case PROP_STACK:
      g_value_set_object (value, self->stack);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
ide_layout_tab_bar_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  IdeLayoutTabBar *self = IDE_LAYOUT_TAB_BAR(object);

  switch (prop_id)
    {
    case PROP_STACK:
      ide_layout_tab_bar_set_stack (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
ide_tab_layout_bar_class_init (IdeLayoutTabBarClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = ide_layout_tab_bar_get_property;
  object_class->set_property = ide_layout_tab_bar_set_property;

  widget_class->hierarchy_changed = ide_layout_tab_bar_hierarchy_changed;

  properties [PROP_STACK] =
    g_param_spec_object ("stack",
                         "stack",
                         "stack",
                         GTK_TYPE_STACK,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  gtk_widget_class_set_css_name (widget_class, "layouttabbar");
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-layout-tab-bar.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeLayoutTabBar, tab);
  gtk_widget_class_bind_template_child (widget_class, IdeLayoutTabBar, tab_expander);
  gtk_widget_class_bind_template_child (widget_class, IdeLayoutTabBar, views_list_button);
  gtk_widget_class_bind_template_child (widget_class, IdeLayoutTabBar, views_list_box);
  gtk_widget_class_bind_template_child (widget_class, IdeLayoutTabBar, views_list_popover);
}

static void
ide_tab_layout_bar_init (IdeLayoutTabBar *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self->views_list_box,
                           "row-selected",
                           G_CALLBACK (ide_layout_tab_bar_row_selected),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->views_list_popover,
                           "closed",
                           G_CALLBACK (ide_layout_tab_bar_popover_closed),
                           self,
                           G_CONNECT_SWAPPED);
}

void
ide_layout_tab_bar_set_view (IdeLayoutTabBar *self,
                             GtkWidget       *view)
{
  g_return_if_fail (IDE_IS_LAYOUT_TAB_BAR (self));
  g_return_if_fail (!view || IDE_IS_LAYOUT_VIEW (view));

  ide_layout_tab_set_view (self->tab, view);
}

void
ide_layout_tab_bar_show_list (IdeLayoutTabBar *self)
{
  g_return_if_fail (IDE_IS_LAYOUT_TAB_BAR (self));

  gtk_widget_activate (GTK_WIDGET (self->views_list_button));
}
