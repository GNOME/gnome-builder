/* ide-workbench-header-bar.c
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

#define G_LOG_DOMAIN "ide-workbench-header-bar"

#include "application/ide-application.h"
#include "util/ide-gtk.h"
#include "workbench/ide-perspective.h"
#include "workbench/ide-workbench.h"
#include "workbench/ide-workbench-header-bar.h"
#include "workbench/ide-workbench-header-bar-private.h"

typedef struct
{
  GtkMenuButton *menu_button;
  GtkListBox    *perspectives_list_box;
  GtkMenuButton *perspectives_menu_button;
  GtkImage      *perspectives_menu_button_image;
  GtkPopover    *perspectives_popover;
} IdeWorkbenchHeaderBarPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeWorkbenchHeaderBar, ide_workbench_header_bar, GTK_TYPE_HEADER_BAR)

static void
perspective_row_selected (IdeWorkbenchHeaderBar *self,
                          GtkListBoxRow         *row,
                          GtkListBox            *list_box)
{
  IdeWorkbenchHeaderBarPrivate *priv = ide_workbench_header_bar_get_instance_private (self);
  IdeWorkbench *workbench;
  const gchar *id;

  g_assert (IDE_IS_WORKBENCH_HEADER_BAR (self));
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  id = g_object_get_data (G_OBJECT (row), "IDE_PERSPECTIVE_ID");
  if (id == NULL)
    return;

  workbench = ide_widget_get_workbench (GTK_WIDGET (self));
  if (workbench == NULL)
    return;

  gtk_widget_hide (GTK_WIDGET (priv->perspectives_popover));

  ide_workbench_set_visible_perspective_name (workbench, id);
}

GtkWidget *
ide_workbench_header_bar_new (void)
{
  return g_object_new (IDE_TYPE_WORKBENCH_HEADER_BAR, NULL);
}

static void
ide_workbench_header_bar_class_init (IdeWorkbenchHeaderBarClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-workbench-header-bar.ui");
  gtk_widget_class_bind_template_child_private (widget_class, IdeWorkbenchHeaderBar, menu_button);
  gtk_widget_class_bind_template_child_private (widget_class, IdeWorkbenchHeaderBar, perspectives_list_box);
  gtk_widget_class_bind_template_child_private (widget_class, IdeWorkbenchHeaderBar, perspectives_menu_button);
  gtk_widget_class_bind_template_child_private (widget_class, IdeWorkbenchHeaderBar, perspectives_menu_button_image);
  gtk_widget_class_bind_template_child_private (widget_class, IdeWorkbenchHeaderBar, perspectives_popover);
}

static void
ide_workbench_header_bar_init (IdeWorkbenchHeaderBar *self)
{
  IdeWorkbenchHeaderBarPrivate *priv = ide_workbench_header_bar_get_instance_private (self);
  GtkWidget *popover;
  GMenu *model;

  gtk_widget_init_template (GTK_WIDGET (self));

  model = ide_application_get_menu_by_id (IDE_APPLICATION_DEFAULT, "gear-menu");
  popover = gtk_popover_new_from_model (NULL, G_MENU_MODEL (model));
  gtk_menu_button_set_popover (priv->menu_button, popover);

  g_signal_connect_object (priv->perspectives_list_box,
                           "row-activated",
                           G_CALLBACK (perspective_row_selected),
                           self,
                           G_CONNECT_SWAPPED);
}

void
ide_workbench_header_bar_focus_search (IdeWorkbenchHeaderBar *self)
{
  GtkWidget *entry;

  g_return_if_fail (IDE_IS_WORKBENCH_HEADER_BAR (self));

  entry = gtk_header_bar_get_custom_title (GTK_HEADER_BAR (self));
  if (GTK_IS_ENTRY (entry))
    gtk_widget_grab_focus (GTK_WIDGET (entry));
}

static GtkWidget *
create_perspective_row (gpointer item,
                        gpointer user_data)
{
  IdePerspective *perspective = item;
  g_autofree gchar *title = NULL;
  g_autofree gchar *icon_name = NULL;
  GtkListBoxRow *row;
  GtkLabel *label;
  GtkImage *image;
  GtkBox *box;

  g_assert (IDE_IS_PERSPECTIVE (perspective));

  title = ide_perspective_get_title (perspective);
  icon_name = ide_perspective_get_icon_name (perspective);

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      "can-focus", FALSE,
                      "selectable", FALSE,
                      "visible", TRUE,
                      NULL);

  g_object_set_data_full (G_OBJECT (row),
                          "IDE_PERSPECTIVE_ID",
                          ide_perspective_get_id (perspective),
                          g_free);

  box = g_object_new (GTK_TYPE_BOX,
                      "visible", TRUE,
                      NULL);
  gtk_container_add (GTK_CONTAINER (row), GTK_WIDGET (box));

  image = g_object_new (GTK_TYPE_IMAGE,
                        "hexpand", FALSE,
                        "icon-name", icon_name,
                        "visible", TRUE,
                        NULL);
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (image));

  label = g_object_new (GTK_TYPE_LABEL,
                        "label", title,
                        "hexpand", TRUE,
                        "xalign", 0.0f,
                        "visible", TRUE,
                        NULL);
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (label));

  return GTK_WIDGET (row);
}

void
_ide_workbench_header_bar_set_perspectives (IdeWorkbenchHeaderBar *self,
                                            GListModel            *model)
{
  IdeWorkbenchHeaderBarPrivate *priv = ide_workbench_header_bar_get_instance_private (self);

  g_assert (IDE_IS_WORKBENCH_HEADER_BAR (self));
  g_assert (!model || G_IS_LIST_MODEL (model));

  gtk_list_box_bind_model (priv->perspectives_list_box,
                           model,
                           create_perspective_row,
                           NULL, NULL);
}

void
_ide_workbench_header_bar_set_perspective (IdeWorkbenchHeaderBar *self,
                                           IdePerspective        *perspective)
{
  IdeWorkbenchHeaderBarPrivate *priv = ide_workbench_header_bar_get_instance_private (self);
  g_autofree gchar *icon_name = NULL;

  g_assert (IDE_IS_WORKBENCH_HEADER_BAR (self));
  g_assert (IDE_IS_PERSPECTIVE (perspective));

  icon_name = ide_perspective_get_icon_name (perspective);

  g_object_set (priv->perspectives_menu_button_image,
                "icon-name", icon_name,
                NULL);
}
