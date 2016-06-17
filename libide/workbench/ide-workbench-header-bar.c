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
#include "workbench/ide-workbench-header-bar.h"

typedef struct
{
  GtkMenuButton *menu_button;
} IdeWorkbenchHeaderBarPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeWorkbenchHeaderBar, ide_workbench_header_bar, GTK_TYPE_HEADER_BAR)

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
