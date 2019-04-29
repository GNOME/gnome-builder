/* ide-terminal-surface.c
 *
 * Copyright 2018 Christian Hergert <unknown@domain.org>
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

#define G_LOG_DOMAIN "ide-terminal-surface"

#include "config.h"

#include "ide-terminal-page.h"
#include "ide-terminal-surface.h"

struct _IdeTerminalSurface
{
  IdeSurface  parent_instance;

  IdeGrid    *grid;
};

G_DEFINE_TYPE (IdeTerminalSurface, ide_terminal_surface, IDE_TYPE_SURFACE)

/**
 * ide_terminal_surface_new:
 *
 * Create a new #IdeTerminalSurface.
 *
 * Returns: (transfer full): a newly created #IdeTerminalSurface
 *
 * Since: 3.32
 */
IdeTerminalSurface *
ide_terminal_surface_new (void)
{
  return g_object_new (IDE_TYPE_TERMINAL_SURFACE, NULL);
}

static IdeFrame *
ide_terminal_surface_create_frame_cb (IdeTerminalSurface *self,
                                      IdeGrid            *grid)
{
  IdeFrame *frame;

  g_assert (IDE_IS_TERMINAL_SURFACE (self));
  g_assert (IDE_IS_GRID (grid));

  frame = g_object_new (IDE_TYPE_FRAME,
                        "expand", TRUE,
                        "visible", TRUE,
                        NULL);
  ide_frame_set_placeholder (frame, gtk_label_new (NULL));

  return frame;
}

static void
ide_terminal_surface_add (GtkContainer *container,
                          GtkWidget    *child)
{
  IdeTerminalSurface *self = (IdeTerminalSurface *)container;

  g_assert (IDE_IS_TERMINAL_SURFACE (self));

  if (IDE_IS_PAGE (child))
    gtk_container_add (GTK_CONTAINER (self->grid), child);
  else
    GTK_CONTAINER_CLASS (ide_terminal_surface_parent_class)->add (container, child);
}

static void
ide_terminal_surface_class_init (IdeTerminalSurfaceClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  container_class->add = ide_terminal_surface_add;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-terminal/ui/ide-terminal-surface.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeTerminalSurface, grid);
}

static void
ide_terminal_surface_init (IdeTerminalSurface *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_widget_set_name (GTK_WIDGET (self), "terminal");

  g_signal_connect_object (self->grid,
                           "create-frame",
                           G_CALLBACK (ide_terminal_surface_create_frame_cb),
                           self,
                           G_CONNECT_SWAPPED);
}
