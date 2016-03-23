/* pnl-dock-overlay.c
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

#include "pnl-animation.h"
#include "pnl-dock-overlay-edge-private.h"
#include "pnl-dock-item.h"
#include "pnl-dock-overlay.h"
#include "pnl-tab.h"
#include "pnl-tab-strip.h"
#include "pnl-util-private.h"

#define REVEAL_DURATION 300
#define MNEMONIC_REVEAL_DURATION 200

typedef struct
{
  GtkOverlay         *overlay;
  PnlDockOverlayEdge *edges [4];
  GtkAdjustment      *edge_adj [4];
  GtkAdjustment      *edge_handle_adj [4];
  guint               child_reveal : 4;
} PnlDockOverlayPrivate;

static void pnl_dock_overlay_init_dock_iface      (PnlDockInterface     *iface);
static void pnl_dock_overlay_init_dock_item_iface (PnlDockItemInterface *iface);
static void pnl_dock_overlay_init_buildable_iface (GtkBuildableIface    *iface);
static void pnl_dock_overlay_set_child_reveal     (PnlDockOverlay       *self,
                                                   GtkWidget            *child,
                                                   gboolean              reveal);

G_DEFINE_TYPE_EXTENDED (PnlDockOverlay, pnl_dock_overlay, GTK_TYPE_EVENT_BOX, 0,
                        G_ADD_PRIVATE (PnlDockOverlay)
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, pnl_dock_overlay_init_buildable_iface)
                        G_IMPLEMENT_INTERFACE (PNL_TYPE_DOCK_ITEM, pnl_dock_overlay_init_dock_item_iface)
                        G_IMPLEMENT_INTERFACE (PNL_TYPE_DOCK, pnl_dock_overlay_init_dock_iface))

enum {
  PROP_0,
  PROP_MANAGER,
  N_PROPS
};

enum {
  CHILD_PROP_0,
  CHILD_PROP_REVEAL,
  N_CHILD_PROPS
};

enum {
  HIDE_EDGES,
  N_SIGNALS
};

static GParamSpec *child_properties [N_CHILD_PROPS];
static guint signals [N_SIGNALS];

static void
pnl_dock_overlay_update_focus_chain (PnlDockOverlay *self)
{
  PnlDockOverlayPrivate *priv = pnl_dock_overlay_get_instance_private (self);
  GList *focus_chain = NULL;
  GtkWidget *child;
  guint i;

  g_assert (PNL_IS_DOCK_OVERLAY (self));

  for (i = G_N_ELEMENTS (priv->edges); i > 0; i--)
    {
      PnlDockOverlayEdge *edge = priv->edges [i - 1];

      if (edge != NULL)
        focus_chain = g_list_prepend (focus_chain, edge);
    }

  child = gtk_bin_get_child (GTK_BIN (self));

  if (child != NULL)
    focus_chain = g_list_prepend (focus_chain, child);

  if (focus_chain != NULL)
    {
      gtk_container_set_focus_chain (GTK_CONTAINER (self), focus_chain);
      g_list_free (focus_chain);
    }
}

static void
pnl_dock_overlay_get_edge_position (PnlDockOverlay     *self,
                                    PnlDockOverlayEdge *edge,
                                    GtkAllocation      *allocation)
{
  PnlDockOverlayPrivate *priv = pnl_dock_overlay_get_instance_private (self);
  GtkPositionType type;
  gdouble value;
  gdouble handle_value;
  gdouble flipped_value;
  gint nat_width;
  gint nat_height;

  g_assert (PNL_IS_DOCK_OVERLAY (self));
  g_assert (PNL_IS_DOCK_OVERLAY_EDGE (edge));
  g_assert (allocation != NULL);

  gtk_widget_get_allocation (GTK_WIDGET (self), allocation);

  allocation->x = 0;
  allocation->y = 0;

  type = pnl_dock_overlay_edge_get_edge (edge);

  if (type == GTK_POS_LEFT || type == GTK_POS_RIGHT)
    {
      nat_height = MAX (allocation->height, 1);
      gtk_widget_get_preferred_width_for_height (GTK_WIDGET (edge), nat_height, NULL, &nat_width);
    }
  else if (type == GTK_POS_TOP || type == GTK_POS_BOTTOM)
    {
      nat_width = MAX (allocation->width, 1);
      gtk_widget_get_preferred_height_for_width (GTK_WIDGET (edge), nat_width, NULL, &nat_height);
    }
  else
    {
      g_assert_not_reached ();
      return;
    }

  value = gtk_adjustment_get_value (priv->edge_adj [type]);
  flipped_value = 1.0 - value;

  handle_value = gtk_adjustment_get_value (priv->edge_handle_adj [type]);

  switch (type)
    {
    case GTK_POS_LEFT:
      allocation->width = nat_width;
      allocation->x -= nat_width * value;
      if (flipped_value * nat_width <= handle_value)
        allocation->x += (handle_value - (flipped_value * nat_width));
      break;

    case GTK_POS_RIGHT:
      allocation->x = allocation->x + allocation->width - nat_width;
      allocation->width = nat_width;
      allocation->x += nat_width * value;
      if (flipped_value * nat_width <= handle_value)
        allocation->x -= (handle_value - (flipped_value * nat_width));
      break;

    case GTK_POS_BOTTOM:
      allocation->y = allocation->y + allocation->height - nat_height;
      allocation->height = nat_height;
      allocation->y += nat_height * value;
      if (flipped_value * nat_height <= handle_value)
        allocation->y -= (handle_value - (flipped_value * nat_height));
      break;

    case GTK_POS_TOP:
      allocation->height = nat_height;
      allocation->y -= nat_height * value;
      if (flipped_value * nat_height <= handle_value)
        allocation->y += (handle_value - (flipped_value * nat_height));
      break;

    default:
      g_assert_not_reached ();
    }
}

static gboolean
pnl_dock_overlay_get_child_position (PnlDockOverlay *self,
                                     GtkWidget      *widget,
                                     GtkAllocation  *allocation)
{
  g_assert (PNL_IS_DOCK_OVERLAY (self));
  g_assert (GTK_IS_WIDGET (widget));
  g_assert (allocation != NULL);

  if (PNL_IS_DOCK_OVERLAY_EDGE (widget))
    {
      pnl_dock_overlay_get_edge_position (self, PNL_DOCK_OVERLAY_EDGE (widget), allocation);
      return TRUE;
    }

  return FALSE;
}

static void
pnl_dock_overlay_add (GtkContainer *container,
                      GtkWidget    *widget)
{
  PnlDockOverlay *self = (PnlDockOverlay *)container;

  g_assert (PNL_IS_DOCK_OVERLAY (self));
  g_assert (GTK_IS_WIDGET (widget));

  GTK_CONTAINER_CLASS (pnl_dock_overlay_parent_class)->add (container, widget);

  pnl_dock_overlay_update_focus_chain (self);

  if (PNL_IS_DOCK_ITEM (widget))
    {
      pnl_dock_item_adopt (PNL_DOCK_ITEM (self), PNL_DOCK_ITEM (widget));
      pnl_dock_item_update_visibility (PNL_DOCK_ITEM (widget));
    }
}

static void
pnl_dock_overlay_toplevel_mnemonics (PnlDockOverlay *self,
                                     GParamSpec     *pspec,
                                     GtkWindow      *toplevel)
{
  PnlDockOverlayPrivate *priv = pnl_dock_overlay_get_instance_private (self);
  const gchar *style_prop;
  gboolean mnemonics_visible;
  guint i;

  g_assert (PNL_IS_DOCK_OVERLAY (self));
  g_assert (pspec != NULL);
  g_assert (GTK_IS_WINDOW (toplevel));

  mnemonics_visible = gtk_window_get_mnemonics_visible (toplevel);
  style_prop = mnemonics_visible ? "mnemonic-overlap-size" : "overlap-size";

  for (i = 0; i < G_N_ELEMENTS (priv->edges); i++)
    {
      PnlDockOverlayEdge *edge = priv->edges [i];
      GtkAdjustment *handle_adj = priv->edge_handle_adj [i];
      gint overlap = 0;

      gtk_widget_style_get (GTK_WIDGET (edge), style_prop, &overlap, NULL);

      pnl_object_animate (handle_adj,
                          PNL_ANIMATION_EASE_IN_OUT_CUBIC,
                          MNEMONIC_REVEAL_DURATION,
                          gtk_widget_get_frame_clock (GTK_WIDGET (edge)),
                          "value", (gdouble)overlap,
                          NULL);
    }

  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

static void
pnl_dock_overlay_toplevel_set_focus (PnlDockOverlay *self,
                                     GtkWidget      *widget,
                                     GtkWindow      *toplevel)
{
  PnlDockOverlayPrivate *priv = pnl_dock_overlay_get_instance_private (self);
  guint i;

  g_assert (PNL_IS_DOCK_OVERLAY (self));
  g_assert (!widget || GTK_IS_WIDGET (widget));
  g_assert (GTK_IS_WINDOW (toplevel));

  /*
   * TODO: If the overlay obscurs the new focus widget,
   *       hide immediately. Otherwise, use a short timeout.
   */

  for (i = 0; i < G_N_ELEMENTS (priv->edges); i++)
    {
      PnlDockOverlayEdge *edge = priv->edges [i];

      if (!widget || !gtk_widget_is_ancestor (widget, GTK_WIDGET (edge)))
        gtk_container_child_set (GTK_CONTAINER (self), GTK_WIDGET (edge),
                                 "reveal", FALSE,
                                 NULL);
    }
}

static void
pnl_dock_overlay_hide_edges (PnlDockOverlay *self)
{
  PnlDockOverlayPrivate *priv = pnl_dock_overlay_get_instance_private (self);
  GtkWidget *child;
  guint i;

  g_assert (PNL_IS_DOCK_OVERLAY (self));

  for (i = 0; i < G_N_ELEMENTS (priv->edges); i++)
    {
      PnlDockOverlayEdge *edge = priv->edges [i];

      gtk_container_child_set (GTK_CONTAINER (self), GTK_WIDGET (edge),
                               "reveal", FALSE,
                               NULL);
    }

  child = gtk_bin_get_child (GTK_BIN (self));

  if (child != NULL)
    gtk_widget_grab_focus (child);
}

static void
pnl_dock_overlay_destroy (GtkWidget *widget)
{
  PnlDockOverlay *self = (PnlDockOverlay *)widget;
  PnlDockOverlayPrivate *priv = pnl_dock_overlay_get_instance_private (self);
  guint i;

  g_assert (GTK_IS_WIDGET (widget));

  for (i = 0; i < G_N_ELEMENTS (priv->edge_adj); i++)
    g_clear_object (&priv->edge_adj [i]);

  GTK_WIDGET_CLASS (pnl_dock_overlay_parent_class)->destroy (widget);
}

static void
pnl_dock_overlay_hierarchy_changed (GtkWidget *widget,
                                    GtkWidget *old_toplevel)
{
  PnlDockOverlay *self = (PnlDockOverlay *)widget;
  GtkWidget *toplevel;

  g_assert (PNL_IS_DOCK_OVERLAY (self));
  g_assert (!old_toplevel || GTK_IS_WIDGET (old_toplevel));

  if (old_toplevel != NULL)
    {
      g_signal_handlers_disconnect_by_func (old_toplevel,
                                            G_CALLBACK (pnl_dock_overlay_toplevel_mnemonics),
                                            self);
      g_signal_handlers_disconnect_by_func (old_toplevel,
                                            G_CALLBACK (pnl_dock_overlay_toplevel_set_focus),
                                            self);
    }

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));

  if (GTK_IS_WINDOW (toplevel))
    {
      g_signal_connect_object (toplevel,
                               "notify::mnemonics-visible",
                               G_CALLBACK (pnl_dock_overlay_toplevel_mnemonics),
                               self,
                               G_CONNECT_SWAPPED);

      g_signal_connect_object (toplevel,
                               "set-focus",
                               G_CALLBACK (pnl_dock_overlay_toplevel_set_focus),
                               self,
                               G_CONNECT_SWAPPED);
    }
}

static gboolean
pnl_dock_overlay_get_child_reveal (PnlDockOverlay *self,
                                   GtkWidget      *child)
{
  PnlDockOverlayPrivate *priv = pnl_dock_overlay_get_instance_private (self);

  g_assert (PNL_IS_DOCK_OVERLAY (self));
  g_assert (GTK_IS_WIDGET (child));

  if (PNL_IS_DOCK_OVERLAY_EDGE (child))
    {
      GtkPositionType edge;

      edge = pnl_dock_overlay_edge_get_edge (PNL_DOCK_OVERLAY_EDGE (child));

      return !!(priv->child_reveal & (1 << edge));
    }

  return FALSE;
}

static void
pnl_dock_overlay_set_child_reveal (PnlDockOverlay *self,
                                   GtkWidget      *child,
                                   gboolean        reveal)
{
  PnlDockOverlayPrivate *priv = pnl_dock_overlay_get_instance_private (self);
  GtkPositionType edge;
  guint child_reveal;

  g_assert (PNL_IS_DOCK_OVERLAY (self));
  g_assert (GTK_IS_WIDGET (child));

  if (!PNL_IS_DOCK_OVERLAY_EDGE (child))
    return;

  edge = pnl_dock_overlay_edge_get_edge (PNL_DOCK_OVERLAY_EDGE (child));

  if (reveal)
    child_reveal = priv->child_reveal | (1 << edge);
  else
    child_reveal = priv->child_reveal & ~(1 << edge);

  if (priv->child_reveal != child_reveal)
    {
      priv->child_reveal = child_reveal;

      pnl_object_animate (priv->edge_adj [edge],
                          PNL_ANIMATION_EASE_IN_OUT_CUBIC,
                          REVEAL_DURATION,
                          gtk_widget_get_frame_clock (child),
                          "value", reveal ? 0.0 : 1.0,
                          NULL);

      gtk_container_child_notify_by_pspec (GTK_CONTAINER (self),
                                           child,
                                           child_properties [CHILD_PROP_REVEAL]);
    }

}

static void
pnl_dock_overlay_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  PnlDockOverlay *self = PNL_DOCK_OVERLAY (object);

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
pnl_dock_overlay_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  PnlDockOverlay *self = PNL_DOCK_OVERLAY (object);

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
pnl_dock_overlay_get_child_property (GtkContainer *container,
                                     GtkWidget    *widget,
                                     guint         prop_id,
                                     GValue       *value,
                                     GParamSpec   *pspec)
{
  PnlDockOverlay *self = PNL_DOCK_OVERLAY (container);

  switch (prop_id)
    {
    case CHILD_PROP_REVEAL:
      g_value_set_boolean (value, pnl_dock_overlay_get_child_reveal (self, widget));
      break;

    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, prop_id, pspec);
    }
}

static void
pnl_dock_overlay_set_child_property (GtkContainer *container,
                                     GtkWidget    *widget,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  PnlDockOverlay *self = PNL_DOCK_OVERLAY (container);

  switch (prop_id)
    {
    case CHILD_PROP_REVEAL:
      pnl_dock_overlay_set_child_reveal (self, widget, g_value_get_boolean (value));
      break;

    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, prop_id, pspec);
    }
}

static void
pnl_dock_overlay_class_init (PnlDockOverlayClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
  GtkBindingSet *binding_set;

  object_class->get_property = pnl_dock_overlay_get_property;
  object_class->set_property = pnl_dock_overlay_set_property;

  widget_class->destroy = pnl_dock_overlay_destroy;
  widget_class->hierarchy_changed = pnl_dock_overlay_hierarchy_changed;

  container_class->add = pnl_dock_overlay_add;
  container_class->get_child_property = pnl_dock_overlay_get_child_property;
  container_class->set_child_property = pnl_dock_overlay_set_child_property;

  klass->hide_edges = pnl_dock_overlay_hide_edges;

  g_object_class_override_property (object_class, PROP_MANAGER, "manager");

  child_properties [CHILD_PROP_REVEAL] =
    g_param_spec_boolean ("reveal",
                          "Reveal",
                          "If the panel edge should be revealed",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gtk_container_class_install_child_properties (container_class, N_CHILD_PROPS, child_properties);

  gtk_widget_class_set_css_name (widget_class, "dockoverlay");

  signals [HIDE_EDGES] =
    g_signal_new ("hide-edges",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (PnlDockOverlayClass, hide_edges),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  binding_set = gtk_binding_set_by_class (klass);

  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_Escape,
                                0,
                                "hide-edges",
                                0);
}

static void
pnl_dock_overlay_init (PnlDockOverlay *self)
{
  PnlDockOverlayPrivate *priv = pnl_dock_overlay_get_instance_private (self);
  guint i;

  priv->overlay = g_object_new (GTK_TYPE_OVERLAY,
                                "visible", TRUE,
                                NULL);

  GTK_CONTAINER_CLASS (pnl_dock_overlay_parent_class)->add (GTK_CONTAINER (self),
                                                            GTK_WIDGET (priv->overlay));

  g_signal_connect_object (priv->overlay,
                           "get-child-position",
                           G_CALLBACK (pnl_dock_overlay_get_child_position),
                           self,
                           G_CONNECT_SWAPPED);

  for (i = 0; i <= GTK_POS_BOTTOM; i++)
    {
      PnlDockOverlayEdge *edge;

      edge = g_object_new (PNL_TYPE_DOCK_OVERLAY_EDGE,
                           "edge", (GtkPositionType)i,
                           "visible", TRUE,
                           NULL);

      pnl_set_weak_pointer (&priv->edges[i], edge);

      gtk_overlay_add_overlay (priv->overlay, GTK_WIDGET (priv->edges [i]));

      priv->edge_adj [i] = gtk_adjustment_new (1, 0, 1, 0, 0, 0);

      g_signal_connect_swapped (priv->edge_adj [i],
                                "value-changed",
                                G_CALLBACK (gtk_widget_queue_allocate),
                                priv->overlay);

      priv->edge_handle_adj [i] = gtk_adjustment_new (0, 0, 1000, 0, 0, 0);

      g_signal_connect_swapped (priv->edge_handle_adj [i],
                                "value-changed",
                                G_CALLBACK (gtk_widget_queue_allocate),
                                priv->overlay);
    }
}

static void
pnl_dock_overlay_init_dock_iface (PnlDockInterface *iface)
{
}

GtkWidget *
pnl_dock_overlay_new (void)
{
  return g_object_new (PNL_TYPE_DOCK_OVERLAY, NULL);
}

static void
pnl_dock_overlay_add_child (GtkBuildable *buildable,
                            GtkBuilder   *builder,
                            GObject      *child,
                            const gchar  *type)
{
  PnlDockOverlay *self = (PnlDockOverlay *)buildable;
  PnlDockOverlayPrivate *priv = pnl_dock_overlay_get_instance_private (self);
  PnlDockOverlayEdge *parent = NULL;

  g_assert (PNL_IS_DOCK_OVERLAY (self));
  g_assert (GTK_IS_BUILDER (builder));
  g_assert (G_IS_OBJECT (child));

  if (!GTK_IS_WIDGET (child))
    {
      g_warning ("Attempt to add a child of type \"%s\" to a \"%s\"",
                 G_OBJECT_TYPE_NAME (child), G_OBJECT_TYPE_NAME (self));
      return;
    }

  if ((type == NULL) || (g_strcmp0 ("center", type) == 0))
    {
      gtk_container_add (GTK_CONTAINER (priv->overlay), GTK_WIDGET (child));
      goto adopt;
    }

  if (g_strcmp0 ("top", type) == 0)
    parent = priv->edges [GTK_POS_TOP];
  else if (g_strcmp0 ("bottom", type) == 0)
    parent = priv->edges [GTK_POS_BOTTOM];
  else if (g_strcmp0 ("right", type) == 0)
    parent = priv->edges [GTK_POS_RIGHT];
  else
    parent = priv->edges [GTK_POS_LEFT];

  gtk_container_add (GTK_CONTAINER (parent), GTK_WIDGET (child));

adopt:
  if (PNL_IS_DOCK_ITEM (child))
    pnl_dock_item_adopt (PNL_DOCK_ITEM (self), PNL_DOCK_ITEM (child));
}

static void
pnl_dock_overlay_init_buildable_iface (GtkBuildableIface *iface)
{
  iface->add_child = pnl_dock_overlay_add_child;
}

static void
pnl_dock_overlay_present_child (PnlDockItem *item,
                                PnlDockItem *child)
{
  PnlDockOverlay *self = (PnlDockOverlay *)item;

  g_assert (PNL_IS_DOCK_OVERLAY (self));
  g_assert (PNL_IS_DOCK_ITEM (child));

  gtk_container_child_set (GTK_CONTAINER (self), GTK_WIDGET (child),
                           "reveal", TRUE,
                           NULL);
}

static void
pnl_dock_overlay_update_visibility (PnlDockItem *item)
{
  PnlDockOverlay *self = (PnlDockOverlay *)item;
  PnlDockOverlayPrivate *priv = pnl_dock_overlay_get_instance_private (self);
  guint i;

  g_assert (PNL_IS_DOCK_OVERLAY (self));

  for (i = 0; i < G_N_ELEMENTS (priv->edges); i++)
    {
      PnlDockOverlayEdge *edge = priv->edges [i];
      gboolean has_widgets;

      if (edge == NULL)
        continue;

      has_widgets = pnl_dock_item_has_widgets (PNL_DOCK_ITEM (edge));

      gtk_widget_set_child_visible (GTK_WIDGET (edge), has_widgets);
    }

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
pnl_dock_overlay_init_dock_item_iface (PnlDockItemInterface *iface)
{
  iface->present_child = pnl_dock_overlay_present_child;
  iface->update_visibility = pnl_dock_overlay_update_visibility;
}
