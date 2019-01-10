/* ide-panel.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "ide-panel"

#include "config.h"

#include <dazzle.h>
#include <glib/gi18n.h>

#include "ide-panel.h"

typedef struct
{
  DzlDockStack *dock_stack;
} IdePanelPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdePanel, ide_panel, DZL_TYPE_DOCK_BIN_EDGE)

static void
ide_panel_add (GtkContainer *container,
               GtkWidget    *widget)
{
  IdePanel *self = (IdePanel *)container;
  IdePanelPrivate *priv = ide_panel_get_instance_private (self);

  g_assert (IDE_IS_PANEL (self));

  if (DZL_IS_DOCK_WIDGET (widget))
    gtk_container_add (GTK_CONTAINER (priv->dock_stack), widget);
  else
    GTK_CONTAINER_CLASS (ide_panel_parent_class)->add (container, widget);
}

static void
ide_panel_class_init (IdePanelClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  container_class->add = ide_panel_add;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-gui/ui/ide-panel.ui");
  gtk_widget_class_bind_template_child_private (widget_class, IdePanel, dock_stack);
}

static void
ide_panel_init (IdePanel *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

/**
 * ide_panel_new:
 *
 * Creates a new #IdePanel widget.
 *
 * These are meant to be added to #IdeSurface widgets within a workspace.
 *
 * Returns: an #IdePanel
 *
 * Since: 3.32
 */
GtkWidget *
ide_panel_new (void)
{
  return g_object_new (IDE_TYPE_PANEL, NULL);
}
