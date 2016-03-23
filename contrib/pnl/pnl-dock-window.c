/* pnl-dock-window.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#include "pnl-dock-item.h"
#include "pnl-dock-window.h"

typedef struct
{
  void *foo;
} PnlDockWindowPrivate;

static void pnl_dock_window_init_dock_iface (PnlDockInterface *iface);

G_DEFINE_TYPE_EXTENDED (PnlDockWindow, pnl_dock_window, GTK_TYPE_WINDOW, 0,
                        G_ADD_PRIVATE (PnlDockWindow)
                        G_IMPLEMENT_INTERFACE (PNL_TYPE_DOCK_ITEM, NULL)
                        G_IMPLEMENT_INTERFACE (PNL_TYPE_DOCK, pnl_dock_window_init_dock_iface))

enum {
  PROP_0,
  PROP_MANAGER,
  N_PROPS
};

static void
pnl_dock_window_finalize (GObject *object)
{
  G_OBJECT_CLASS (pnl_dock_window_parent_class)->finalize (object);
}

static void
pnl_dock_window_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  PnlDockWindow *self = PNL_DOCK_WINDOW (object);

  switch (prop_id)
    {
    case PROP_MANAGER:
      g_value_set_object (value, pnl_dock_item_get_manager (PNL_DOCK_ITEM (self)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
pnl_dock_window_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  PnlDockWindow *self = PNL_DOCK_WINDOW (object);

  switch (prop_id)
    {
    case PROP_MANAGER:
      pnl_dock_item_set_manager (PNL_DOCK_ITEM (self), g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
pnl_dock_window_class_init (PnlDockWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = pnl_dock_window_finalize;
  object_class->get_property = pnl_dock_window_get_property;
  object_class->set_property = pnl_dock_window_set_property;

  g_object_class_override_property (object_class, PROP_MANAGER, "manager");

  gtk_widget_class_set_css_name (widget_class, "dockwindow");
}

static void
pnl_dock_window_init (PnlDockWindow *self)
{
}

GtkWidget *
pnl_dock_window_new (void)
{
  return g_object_new (PNL_TYPE_DOCK_WINDOW, NULL);
}

static void
pnl_dock_window_init_dock_iface (PnlDockInterface *iface)
{
}
