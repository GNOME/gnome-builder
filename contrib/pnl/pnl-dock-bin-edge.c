/* pnl-dock-bin-edge.c
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

#include "pnl-dock-bin.h"
#include "pnl-dock-bin-edge.h"
#include "pnl-dock-bin-edge-private.h"
#include "pnl-dock-revealer.h"

typedef struct
{
  GtkPositionType edge : 3;
} PnlDockBinEdgePrivate;

G_DEFINE_TYPE_EXTENDED (PnlDockBinEdge, pnl_dock_bin_edge, PNL_TYPE_DOCK_REVEALER, 0,
                        G_ADD_PRIVATE (PnlDockBinEdge)
                        G_IMPLEMENT_INTERFACE (PNL_TYPE_DOCK_ITEM, NULL))

enum {
  PROP_0,
  PROP_EDGE,
  N_PROPS
};

enum {
  MOVE_TO_BIN_CHILD,
  N_SIGNALS
};

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static void
pnl_dock_bin_edge_update_edge (PnlDockBinEdge *self)
{
  PnlDockBinEdgePrivate *priv = pnl_dock_bin_edge_get_instance_private (self);
  GtkStyleContext *style_context;
  PnlDockRevealerTransitionType transition_type;
  const gchar *class_name = NULL;
  GtkWidget *child;
  GtkOrientation orientation;

  g_assert (PNL_IS_DOCK_BIN_EDGE (self));

  style_context = gtk_widget_get_style_context (GTK_WIDGET (self));

  gtk_style_context_remove_class (style_context, "left");
  gtk_style_context_remove_class (style_context, "right");
  gtk_style_context_remove_class (style_context, "top");
  gtk_style_context_remove_class (style_context, "bottom");

  if (priv->edge == GTK_POS_LEFT)
    {
      class_name = "left";
      transition_type = PNL_DOCK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT;
      orientation = GTK_ORIENTATION_VERTICAL;
    }
  else if (priv->edge == GTK_POS_RIGHT)
    {
      class_name = "right";
      transition_type = PNL_DOCK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT;
      orientation = GTK_ORIENTATION_VERTICAL;
    }
  else if (priv->edge == GTK_POS_TOP)
    {
      class_name = "top";
      transition_type = PNL_DOCK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN;
      orientation = GTK_ORIENTATION_HORIZONTAL;
    }
  else if (priv->edge == GTK_POS_BOTTOM)
    {
      class_name = "bottom";
      transition_type = PNL_DOCK_REVEALER_TRANSITION_TYPE_SLIDE_UP;
      orientation = GTK_ORIENTATION_HORIZONTAL;
    }
  else
    {
      g_assert_not_reached ();
      return;
    }

  gtk_style_context_add_class (style_context, class_name);
  pnl_dock_revealer_set_transition_type (PNL_DOCK_REVEALER (self), transition_type);

  child = gtk_bin_get_child (GTK_BIN (self));

  if (PNL_IS_DOCK_PANED (child))
    gtk_orientable_set_orientation (GTK_ORIENTABLE (child), orientation);
}

GtkPositionType
pnl_dock_bin_edge_get_edge (PnlDockBinEdge *self)
{
  PnlDockBinEdgePrivate *priv = pnl_dock_bin_edge_get_instance_private (self);

  g_return_val_if_fail (PNL_IS_DOCK_BIN_EDGE (self), 0);

  return priv->edge;
}

void
pnl_dock_bin_edge_set_edge (PnlDockBinEdge  *self,
                            GtkPositionType  edge)
{
  PnlDockBinEdgePrivate *priv = pnl_dock_bin_edge_get_instance_private (self);

  g_return_if_fail (PNL_IS_DOCK_BIN_EDGE (self));

  if (edge != priv->edge)
    {
      priv->edge = edge;
      pnl_dock_bin_edge_update_edge (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_EDGE]);
    }
}

static void
pnl_dock_bin_edge_add (GtkContainer *container,
                       GtkWidget    *widget)
{
  GtkWidget *child;

  g_assert (GTK_IS_CONTAINER (container));
  g_assert (GTK_IS_WIDGET (widget));

  child = gtk_bin_get_child (GTK_BIN (container));

  if (GTK_IS_CONTAINER (child))
    gtk_container_add (GTK_CONTAINER (child), widget);
  else
    GTK_CONTAINER_CLASS (pnl_dock_bin_edge_parent_class)->add (container, widget);
}

static void
pnl_dock_bin_edge_real_move_to_bin_child (PnlDockBinEdge *self)
{
  GtkWidget *parent;

  g_assert (PNL_IS_DOCK_BIN_EDGE (self));

  parent = gtk_widget_get_parent (GTK_WIDGET (self));

  if (PNL_IS_DOCK_BIN (parent))
    gtk_widget_grab_focus (parent);
}

static void
pnl_dock_bin_edge_constructed (GObject *object)
{
  PnlDockBinEdge *self = (PnlDockBinEdge *)object;

  G_OBJECT_CLASS (pnl_dock_bin_edge_parent_class)->constructed (object);

  pnl_dock_bin_edge_update_edge (self);
}

static void
pnl_dock_bin_edge_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  PnlDockBinEdge *self = PNL_DOCK_BIN_EDGE (object);

  switch (prop_id)
    {
    case PROP_EDGE:
      g_value_set_enum (value, pnl_dock_bin_edge_get_edge (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
pnl_dock_bin_edge_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  PnlDockBinEdge *self = PNL_DOCK_BIN_EDGE (object);

  switch (prop_id)
    {
    case PROP_EDGE:
      pnl_dock_bin_edge_set_edge (self, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
pnl_dock_bin_edge_class_init (PnlDockBinEdgeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
  GtkBindingSet *binding_set;

  object_class->constructed = pnl_dock_bin_edge_constructed;
  object_class->get_property = pnl_dock_bin_edge_get_property;
  object_class->set_property = pnl_dock_bin_edge_set_property;

  container_class->add = pnl_dock_bin_edge_add;

  klass->move_to_bin_child = pnl_dock_bin_edge_real_move_to_bin_child;

  properties [PROP_EDGE] =
    g_param_spec_enum ("edge",
                       "Edge",
                       "The edge of the dock this widget is attached to",
                       GTK_TYPE_POSITION_TYPE,
                       GTK_POS_LEFT,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [MOVE_TO_BIN_CHILD] =
    g_signal_new ("move-to-bin-child",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (PnlDockBinEdgeClass, move_to_bin_child),
                  NULL, NULL, NULL, G_TYPE_NONE, 0);

  binding_set = gtk_binding_set_by_class (klass);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Escape, 0, "move-to-bin-child", 0);

  gtk_widget_class_set_css_name (widget_class, "dockbinedge");
}

static void
pnl_dock_bin_edge_init (PnlDockBinEdge *self)
{
  GtkWidget *child;

  child = g_object_new (PNL_TYPE_DOCK_PANED,
                        "visible", TRUE,
                        NULL);
  GTK_CONTAINER_CLASS (pnl_dock_bin_edge_parent_class)->add (GTK_CONTAINER (self), child);

  pnl_dock_bin_edge_update_edge (self);
}
