/* ide-layout-pane.c
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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

#include <glib/gi18n.h>

#include "dazzle.h"

#include "layout/ide-layout-pane.h"

typedef struct
{
  DzlDockStack *dock_stack;
} IdeLayoutPanePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeLayoutPane, ide_layout_pane, DZL_TYPE_DOCK_BIN_EDGE)

static void
ide_layout_pane_add (GtkContainer *container,
                     GtkWidget    *widget)
{
  IdeLayoutPane *self = (IdeLayoutPane *)container;
  IdeLayoutPanePrivate *priv = ide_layout_pane_get_instance_private (self);

  g_assert (IDE_IS_LAYOUT_PANE (self));

  if (DZL_IS_DOCK_WIDGET (widget))
    gtk_container_add (GTK_CONTAINER (priv->dock_stack), widget);
  else
    GTK_CONTAINER_CLASS (ide_layout_pane_parent_class)->add (container, widget);
}

static void
ide_layout_pane_class_init (IdeLayoutPaneClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  container_class->add = ide_layout_pane_add;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-layout-pane.ui");
  gtk_widget_class_bind_template_child_private (widget_class, IdeLayoutPane, dock_stack);
}

static void
ide_layout_pane_init (IdeLayoutPane *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
