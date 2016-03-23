/* pnl-dock-widget.c
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
#include "pnl-dock-widget.h"
#include "pnl-util-private.h"

typedef struct
{
  gchar *title;
} PnlDockWidgetPrivate;

G_DEFINE_TYPE_EXTENDED (PnlDockWidget, pnl_dock_widget, GTK_TYPE_BIN, 0,
                        G_ADD_PRIVATE (PnlDockWidget)
                        G_IMPLEMENT_INTERFACE (PNL_TYPE_DOCK_ITEM, NULL))

enum {
  PROP_0,
  PROP_MANAGER,
  PROP_TITLE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
pnl_dock_widget_grab_focus (GtkWidget *widget)
{
  PnlDockWidget *self = (PnlDockWidget *)widget;
  GtkWidget *child;

  g_assert (PNL_IS_DOCK_WIDGET (self));

  pnl_dock_item_present (PNL_DOCK_ITEM (self));

  child = gtk_bin_get_child (GTK_BIN (self));

  if (child != NULL)
    gtk_widget_child_focus (child, GTK_DIR_TAB_FORWARD);
}

static void
pnl_dock_widget_finalize (GObject *object)
{
  PnlDockWidget *self = (PnlDockWidget *)object;
  PnlDockWidgetPrivate *priv = pnl_dock_widget_get_instance_private (self);

  g_clear_pointer (&priv->title, g_free);

  G_OBJECT_CLASS (pnl_dock_widget_parent_class)->finalize (object);
}

static void
pnl_dock_widget_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  PnlDockWidget *self = PNL_DOCK_WIDGET (object);

  switch (prop_id)
    {
    case PROP_MANAGER:
      g_value_set_object (value, pnl_dock_item_get_manager (PNL_DOCK_ITEM (self)));
      break;

    case PROP_TITLE:
      g_value_set_string (value, pnl_dock_widget_get_title (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
pnl_dock_widget_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  PnlDockWidget *self = PNL_DOCK_WIDGET (object);

  switch (prop_id)
    {
    case PROP_MANAGER:
      pnl_dock_item_set_manager (PNL_DOCK_ITEM (self), g_value_get_object (value));
      break;

    case PROP_TITLE:
      pnl_dock_widget_set_title (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
pnl_dock_widget_class_init (PnlDockWidgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = pnl_dock_widget_finalize;
  object_class->get_property = pnl_dock_widget_get_property;
  object_class->set_property = pnl_dock_widget_set_property;

  widget_class->draw = pnl_gtk_bin_draw;
  widget_class->grab_focus = pnl_dock_widget_grab_focus;
  widget_class->size_allocate = pnl_gtk_bin_size_allocate;

  properties [PROP_MANAGER] =
    g_param_spec_object ("manager",
                         "Manager",
                         "The panel manager",
                         PNL_TYPE_DOCK_MANAGER,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "Title",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "dockwidget");
}

static void
pnl_dock_widget_init (PnlDockWidget *self)
{
  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);
  gtk_widget_set_can_focus (GTK_WIDGET (self), TRUE);
}

GtkWidget *
pnl_dock_widget_new (void)
{
  return g_object_new (PNL_TYPE_DOCK_WIDGET, NULL);
}

const gchar *
pnl_dock_widget_get_title (PnlDockWidget *self)
{
  PnlDockWidgetPrivate *priv = pnl_dock_widget_get_instance_private (self);

  g_return_val_if_fail (PNL_IS_DOCK_WIDGET (self), NULL);

  return priv->title;
}

void
pnl_dock_widget_set_title (PnlDockWidget *self,
                           const gchar   *title)
{
  PnlDockWidgetPrivate *priv = pnl_dock_widget_get_instance_private (self);

  g_return_if_fail (PNL_IS_DOCK_WIDGET (self));

  if (g_strcmp0 (title, priv->title) != 0)
    {
      g_free (priv->title);
      priv->title = g_strdup (title);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TITLE]);
    }
}
