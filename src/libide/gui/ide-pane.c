/* ide-pane.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-pane"

#include "config.h"

#include <libide-tree.h>

#include "ide-pane.h"

typedef struct
{
  GList *popovers;
} IdePanePrivate;

static void popover_positioner_iface_init (IdePopoverPositionerInterface *iface);

G_DEFINE_TYPE_WITH_CODE (IdePane, ide_pane, PANEL_TYPE_WIDGET,
                         G_ADD_PRIVATE (IdePane)
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_POPOVER_POSITIONER, popover_positioner_iface_init))

static void
ide_pane_popover_closed_cb (IdePane    *self,
                            GtkPopover *popover)
{
  IdePanePrivate *priv = ide_pane_get_instance_private (self);

  g_assert (IDE_IS_PANE (self));
  g_assert (GTK_IS_POPOVER (popover));

  g_signal_handlers_disconnect_by_func (popover,
                                        G_CALLBACK (ide_pane_popover_closed_cb),
                                        self);
  priv->popovers = g_list_remove (priv->popovers, popover);
  gtk_widget_unparent (GTK_WIDGET (popover));
}

static void
ide_pane_popover_positioner_present (IdePopoverPositioner *positioner,
                                     GtkPopover           *popover,
                                     GtkWidget            *relative_to,
                                     const GdkRectangle   *pointing_to)
{
  IdePane *self = (IdePane *)positioner;
  IdePanePrivate *priv = ide_pane_get_instance_private (self);
  GdkRectangle translated;
  double x, y;

  g_assert (IDE_IS_PANE (self));
  g_assert (GTK_IS_POPOVER (popover));
  g_assert (GTK_IS_WIDGET (relative_to));
  g_assert (pointing_to != NULL);

  gtk_widget_translate_coordinates (GTK_WIDGET (relative_to),
                                    GTK_WIDGET (self),
                                    pointing_to->x, pointing_to->y,
                                    &x, &y);
  translated = (GdkRectangle) { x, y, pointing_to->width, pointing_to->height };
  gtk_popover_set_pointing_to (popover, &translated);

  priv->popovers = g_list_append (priv->popovers, popover);
  gtk_widget_set_parent (GTK_WIDGET (popover), GTK_WIDGET (self));
  g_signal_connect_object (popover,
                           "closed",
                           G_CALLBACK (ide_pane_popover_closed_cb),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_popover_popup (popover);
}

static void
popover_positioner_iface_init (IdePopoverPositionerInterface *iface)
{
  iface->present = ide_pane_popover_positioner_present;
}

static void
ide_pane_size_allocate (GtkWidget *widget,
                        int        width,
                        int        height,
                        int        baseline)
{
  IdePane *self = (IdePane *)widget;
  IdePanePrivate *priv = ide_pane_get_instance_private (self);

  g_assert (IDE_IS_PANE (self));

  GTK_WIDGET_CLASS (ide_pane_parent_class)->size_allocate (widget, width, height, baseline);

  for (const GList *iter = priv->popovers; iter != NULL; iter = iter->next)
    {
      GtkPopover *popover = iter->data;

      g_assert (GTK_IS_POPOVER (popover));

      gtk_popover_present (popover);
    }
}

static void
ide_pane_class_init (IdePaneClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->size_allocate = ide_pane_size_allocate;
}

static void
ide_pane_init (IdePane *self)
{
  panel_widget_set_kind (PANEL_WIDGET (self), PANEL_WIDGET_KIND_UTILITY);
}

/**
 * ide_pane_new:
 *
 * Creates a new #IdePane widget.
 *
 * These widgets are meant to be added to #IdePanel widgets.
 *
 * Returns: (transfer full): a new #IdePane
 */
GtkWidget *
ide_pane_new (void)
{
  return g_object_new (IDE_TYPE_PANE, NULL);
}

void
ide_pane_destroy (IdePane *self)
{
  GtkWidget *frame;

  g_return_if_fail (IDE_IS_PANE (self));

  if ((frame = gtk_widget_get_ancestor (GTK_WIDGET (self), PANEL_TYPE_FRAME)))
    panel_frame_remove (PANEL_FRAME (frame), PANEL_WIDGET (self));
}

void
ide_pane_observe (IdePane  *self,
                  IdePane **location)
{
  g_return_if_fail (IDE_IS_PANE (self));
  g_return_if_fail (location != NULL);

  *location = self;
  g_signal_connect_swapped (self,
                            "destroy",
                            G_CALLBACK (g_nullify_pointer),
                            location);
}

void
ide_pane_unobserve (IdePane  *self,
                    IdePane **location)
{
  g_return_if_fail (IDE_IS_PANE (self));
  g_return_if_fail (location != NULL);

  g_signal_handlers_disconnect_by_func (self,
                                        G_CALLBACK (g_nullify_pointer),
                                        location);
  *location = NULL;
}

void
ide_clear_pane (IdePane **location)
{
  IdePane *self = *location;

  if (self == NULL)
    return;

  ide_pane_unobserve (self, location);
  ide_pane_destroy (self);
}
