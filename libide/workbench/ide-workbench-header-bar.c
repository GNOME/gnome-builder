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

#include <dazzle.h>

#include "application/ide-application.h"
#include "search/ide-search-entry.h"
#include "util/ide-gtk.h"
#include "workbench/ide-perspective.h"
#include "workbench/ide-workbench.h"
#include "workbench/ide-workbench-header-bar.h"

typedef struct
{
  GtkMenuButton   *menu_button;
  DzlPriorityBox  *right_box;
  DzlPriorityBox  *left_box;
  IdeOmniBar      *omni_bar;
  IdeSearchEntry  *search_entry;
} IdeWorkbenchHeaderBarPrivate;

static void buildable_iface_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_EXTENDED (IdeWorkbenchHeaderBar, ide_workbench_header_bar, GTK_TYPE_HEADER_BAR, 0,
                        G_ADD_PRIVATE (IdeWorkbenchHeaderBar)
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, buildable_iface_init))

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
  gtk_widget_class_bind_template_child_private (widget_class, IdeWorkbenchHeaderBar, left_box);
  gtk_widget_class_bind_template_child_private (widget_class, IdeWorkbenchHeaderBar, menu_button);
  gtk_widget_class_bind_template_child_private (widget_class, IdeWorkbenchHeaderBar, omni_bar);
  gtk_widget_class_bind_template_child_private (widget_class, IdeWorkbenchHeaderBar, right_box);
  gtk_widget_class_bind_template_child_private (widget_class, IdeWorkbenchHeaderBar, search_entry);

  g_type_ensure (IDE_TYPE_SEARCH_ENTRY);
}

static void
ide_workbench_header_bar_init (IdeWorkbenchHeaderBar *self)
{
  IdeWorkbenchHeaderBarPrivate *priv = ide_workbench_header_bar_get_instance_private (self);
  GtkWidget *popover;
  GMenu *model;

  gtk_widget_init_template (GTK_WIDGET (self));

  model = dzl_application_get_menu_by_id (DZL_APPLICATION_DEFAULT, "gear-menu");
  popover = gtk_popover_new_from_model (NULL, G_MENU_MODEL (model));
  gtk_widget_set_size_request (popover, 225, -1);
  gtk_menu_button_set_popover (priv->menu_button, popover);
  gtk_container_set_border_width (GTK_CONTAINER (popover), 10);
}

void
ide_workbench_header_bar_focus_search (IdeWorkbenchHeaderBar *self)
{
  IdeWorkbenchHeaderBarPrivate *priv = ide_workbench_header_bar_get_instance_private (self);

  g_return_if_fail (IDE_IS_WORKBENCH_HEADER_BAR (self));

  gtk_widget_grab_focus (GTK_WIDGET (priv->search_entry));
}

void
ide_workbench_header_bar_insert_left (IdeWorkbenchHeaderBar *self,
                                      GtkWidget             *widget,
                                      GtkPackType            pack_type,
                                      gint                   priority)
{
  IdeWorkbenchHeaderBarPrivate *priv = ide_workbench_header_bar_get_instance_private (self);

  g_return_if_fail (IDE_IS_WORKBENCH_HEADER_BAR (self));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (pack_type == GTK_PACK_START || pack_type == GTK_PACK_END);

  gtk_container_add_with_properties (GTK_CONTAINER (priv->left_box), widget,
                                     "pack-type", pack_type,
                                     "priority", priority,
                                     NULL);
}

void
ide_workbench_header_bar_insert_right (IdeWorkbenchHeaderBar *self,
                                       GtkWidget             *widget,
                                       GtkPackType            pack_type,
                                       gint                   priority)
{
  IdeWorkbenchHeaderBarPrivate *priv = ide_workbench_header_bar_get_instance_private (self);

  g_return_if_fail (IDE_IS_WORKBENCH_HEADER_BAR (self));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (pack_type == GTK_PACK_START || pack_type == GTK_PACK_END);

  gtk_container_add_with_properties (GTK_CONTAINER (priv->right_box), widget,
                                     "pack-type", pack_type,
                                     "priority", priority,
                                     NULL);
}

static GObject *
ide_workbench_header_bar_get_internal_child (GtkBuildable *buildable,
                                             GtkBuilder   *builder,
                                             const gchar  *childname)
{
  IdeWorkbenchHeaderBar *self = (IdeWorkbenchHeaderBar *)buildable;
  IdeWorkbenchHeaderBarPrivate *priv = ide_workbench_header_bar_get_instance_private (self);

  g_assert (GTK_IS_BUILDABLE (buildable));
  g_assert (GTK_IS_BUILDER (builder));
  g_assert (childname != NULL);

  if (g_str_equal (childname, "left"))
    return G_OBJECT (priv->left_box);
  else if (g_str_equal (childname, "right"))
    return G_OBJECT (priv->right_box);
  else
    return NULL;
}

static void
buildable_iface_init (GtkBuildableIface *iface)
{
  iface->get_internal_child = ide_workbench_header_bar_get_internal_child;
}

/**
 * ide_workbench_header_bar_get_omni_bar:
 *
 * Returns: (transfer none): An #IdeOmniBar.
 */
IdeOmniBar *
ide_workbench_header_bar_get_omni_bar (IdeWorkbenchHeaderBar *self)
{
  IdeWorkbenchHeaderBarPrivate *priv = ide_workbench_header_bar_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_WORKBENCH_HEADER_BAR (self), NULL);

  return priv->omni_bar;
}
