/* egg-centering-bin.c
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

#include "egg-centering-bin.h"
#include "egg-signal-group.h"

/**
 * SECTION:egg-centering-bin:
 * @title: EggCenteringBin
 * @short_description: center a widget with respect to the toplevel
 *
 * First off, you probably want to use GtkBox with a center widget instead
 * of this widget. However, the case where this widget is useful is when
 * you cannot control your layout within the width of the toplevel, but
 * still want your child centered within the toplevel.
 *
 * This is done by translating coordinates of the widget with respect to
 * the toplevel and anchoring the child at TRUE_CENTER-(alloc.width/2).
 */

typedef struct
{
  EggSignalGroup *signals;
} EggCenteringBinPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (EggCenteringBin, egg_centering_bin, GTK_TYPE_BIN)

GtkWidget *
egg_centering_bin_new (void)
{
  return g_object_new (EGG_TYPE_CENTERING_BIN, NULL);
}

static void
egg_centering_bin_size_allocate (GtkWidget     *widget,
                                 GtkAllocation *allocation)
{
  GtkWidget *child;

  g_assert (GTK_IS_WIDGET (widget));
  g_assert (allocation != NULL);

  gtk_widget_set_allocation (widget, allocation);

  child = gtk_bin_get_child (GTK_BIN (widget));

  if ((child != NULL) && gtk_widget_get_visible (child))
    {
      GtkWidget *toplevel = gtk_widget_get_toplevel (child);
      GtkAllocation top_allocation;
      GtkAllocation child_allocation;
      GtkRequisition nat_child_req;
      gint translated_x;
      gint translated_y;

      gtk_widget_get_allocation (toplevel, &top_allocation);
      gtk_widget_translate_coordinates (toplevel, widget,
                                        top_allocation.x + (top_allocation.width / 2),
                                        0,
                                        &translated_x,
                                        &translated_y);

      gtk_widget_get_preferred_size (child, NULL, &nat_child_req);

      child_allocation.x = allocation->x;
      child_allocation.y = allocation->y;
      child_allocation.height = allocation->height;
      child_allocation.width = translated_x * 2;

      if (nat_child_req.width > child_allocation.width)
        child_allocation.width = MIN (nat_child_req.width, allocation->width);

      gtk_widget_size_allocate (child, &child_allocation);
    }
}

static gboolean
queue_allocate_in_idle (gpointer data)
{
  g_autoptr(EggCenteringBin) self = data;

  gtk_widget_queue_allocate (GTK_WIDGET (self));

  return G_SOURCE_REMOVE;
}

static void
egg_centering_bin_toplevel_size_allocate (EggCenteringBin *self,
                                          GtkAllocation   *allocation,
                                          GtkWindow       *toplevel)
{
  g_assert (EGG_IS_CENTERING_BIN (self));
  g_assert (GTK_IS_WINDOW (toplevel));

  /*
   * If we ::queue_allocate() immediately, we can get into a state where an
   * allocation is needed when ::draw() should be called. That causes a
   * warning on the command line, so instead just delay until we leave
   * our current main loop iteration.
   */
  g_timeout_add (0, queue_allocate_in_idle, g_object_ref (self));
}

static void
egg_centering_bin_hierarchy_changed (GtkWidget *widget,
                                     GtkWidget *previous_toplevel)
{
  EggCenteringBin *self = (EggCenteringBin *)widget;
  EggCenteringBinPrivate *priv = egg_centering_bin_get_instance_private (self);
  GtkWidget *toplevel;

  g_assert (EGG_IS_CENTERING_BIN (self));

  /*
   * The hierarcy has changed, so we need to ensure we get allocation change
   * from the toplevel so we can relayout our child to be centered.
   */

  toplevel = gtk_widget_get_toplevel (widget);
  if ((toplevel == NULL) || GTK_IS_WINDOW (toplevel))
    egg_signal_group_set_target (priv->signals, toplevel);
}

static void
egg_centering_bin_finalize (GObject *object)
{
  EggCenteringBin *self = (EggCenteringBin *)object;
  EggCenteringBinPrivate *priv = egg_centering_bin_get_instance_private (self);

  g_clear_object (&priv->signals);

  G_OBJECT_CLASS (egg_centering_bin_parent_class)->finalize (object);
}

static void
egg_centering_bin_class_init (EggCenteringBinClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = egg_centering_bin_finalize;

  widget_class->size_allocate = egg_centering_bin_size_allocate;
  widget_class->hierarchy_changed = egg_centering_bin_hierarchy_changed;
}

static void
egg_centering_bin_init (EggCenteringBin *self)
{
  EggCenteringBinPrivate *priv = egg_centering_bin_get_instance_private (self);

  priv->signals = egg_signal_group_new (GTK_TYPE_WINDOW);

  egg_signal_group_connect_object (priv->signals,
                                   "size-allocate",
                                   G_CALLBACK (egg_centering_bin_toplevel_size_allocate),
                                   self,
                                   G_CONNECT_SWAPPED);
}
