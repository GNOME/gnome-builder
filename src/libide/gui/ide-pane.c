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

#include "ide-pane.h"

G_DEFINE_TYPE (IdePane, ide_pane, PANEL_TYPE_WIDGET)

static void
ide_pane_class_init (IdePaneClass *klass)
{
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
                            "destroyed",
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
