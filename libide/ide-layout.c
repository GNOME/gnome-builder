/* ide-layout.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include "ide-layout.h"
#include "ide-layout-pane.h"
#include "ide-layout-view.h"

typedef struct
{
  GtkWidget *active_view;

  gulong     focus_handler;
} IdeLayoutPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeLayout, ide_layout, PNL_TYPE_DOCK_BIN)

enum {
  PROP_0,
  PROP_ACTIVE_VIEW,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static void
ide_layout_active_view_weak_cb (IdeLayout *self,
                                GtkWidget *where_view_was)
{
  IdeLayoutPrivate *priv = ide_layout_get_instance_private (self);

  g_assert (IDE_IS_LAYOUT (self));

  if (where_view_was == priv->active_view)
    {
      priv->active_view = NULL;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ACTIVE_VIEW]);
    }
}

static void
ide_layout_set_active_view (IdeLayout *self,
                            GtkWidget *active_view)
{
  IdeLayoutPrivate *priv = ide_layout_get_instance_private (self);

  g_assert (IDE_IS_LAYOUT (self));
  g_assert (!active_view || GTK_IS_WIDGET (active_view));

  if (active_view != priv->active_view)
    {
      if (priv->active_view != NULL)
        {
          g_object_weak_unref (G_OBJECT (priv->active_view),
                               (GWeakNotify)ide_layout_active_view_weak_cb,
                               self);
          priv->active_view = NULL;
        }

      if (active_view != NULL)
        {
          priv->active_view = active_view;
          g_object_weak_ref (G_OBJECT (priv->active_view),
                             (GWeakNotify)ide_layout_active_view_weak_cb,
                             self);
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ACTIVE_VIEW]);
    }
}

static void
ide_layout_toplevel_set_focus (IdeLayout *self,
                               GtkWidget *widget,
                               GtkWidget *toplevel)
{
  g_assert (IDE_IS_LAYOUT (self));

  if (widget != NULL && !IDE_IS_LAYOUT_VIEW (widget))
    widget = gtk_widget_get_ancestor (widget, IDE_TYPE_LAYOUT_VIEW);

  if (widget != NULL)
    ide_layout_set_active_view (self, widget);
}

static void
ide_layout_hierarchy_changed (GtkWidget *widget,
                              GtkWidget *old_toplevel)
{
  IdeLayout *self = (IdeLayout *)widget;
  IdeLayoutPrivate *priv = ide_layout_get_instance_private (self);
  GtkWidget *toplevel;

  g_assert (IDE_IS_LAYOUT (self));
  g_assert (!old_toplevel || GTK_IS_WIDGET (old_toplevel));

  if ((old_toplevel != NULL) && (priv->focus_handler != 0))
    {
      g_signal_handler_disconnect (old_toplevel, priv->focus_handler);
      priv->focus_handler = 0;
      ide_clear_weak_pointer (&priv->active_view);
    }

  toplevel = gtk_widget_get_toplevel (widget);

  if (GTK_IS_WINDOW (toplevel))
    priv->focus_handler =
      g_signal_connect_swapped (toplevel,
                                "set-focus",
                                G_CALLBACK (ide_layout_toplevel_set_focus),
                                self);
}

static GtkWidget *
ide_layout_create_edge (PnlDockBin *dock)
{
  g_assert (IDE_IS_LAYOUT (dock));

  return g_object_new (IDE_TYPE_LAYOUT_PANE,
                       "visible", TRUE,
                       "reveal-child", FALSE,
                       NULL);
}

static void
ide_layout_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  IdeLayout *self = IDE_LAYOUT (object);

  switch (prop_id)
    {
    case PROP_ACTIVE_VIEW:
      g_value_set_object (value, ide_layout_get_active_view (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
ide_layout_class_init (IdeLayoutClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  PnlDockBinClass *dock_bin_class = PNL_DOCK_BIN_CLASS (klass);

  object_class->get_property = ide_layout_get_property;

  widget_class->hierarchy_changed = ide_layout_hierarchy_changed;

  dock_bin_class->create_edge = ide_layout_create_edge;

  gtk_widget_class_set_css_name (widget_class, "layout");

  properties [PROP_ACTIVE_VIEW] =
    g_param_spec_object ("active-view",
                         "Active View",
                         "Active View",
                         GTK_TYPE_WIDGET,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
ide_layout_init (IdeLayout *self)
{
}

/**
 * ide_layout_get_active_view:
 *
 * Returns: (transfer none) (nullable): An #IdeLayoutView or %NULL.
 */
GtkWidget *
ide_layout_get_active_view (IdeLayout *self)
{
  IdeLayoutPrivate *priv = ide_layout_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_LAYOUT (self), NULL);

  return priv->active_view;
}
