/* pnl-tab-strip.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "pnl-tab.h"
#include "pnl-tab-strip.h"

typedef struct
{
  GAction         *action;
  GtkStack        *stack;
  GtkPositionType  edge : 2;
} PnlTabStripPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (PnlTabStrip, pnl_tab_strip, GTK_TYPE_BOX)

enum {
  PROP_0,
  PROP_EDGE,
  PROP_STACK,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
set_tab_state (GSimpleAction *action,
               GVariant      *state,
               gpointer       user_data)
{
  PnlTabStrip *self = user_data;
  PnlTabStripPrivate *priv = pnl_tab_strip_get_instance_private (self);
  PnlTab *tab = NULL;
  const GList *iter;
  GList *list;
  gint stateval;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (PNL_IS_TAB_STRIP (self));
  g_assert (state != NULL);
  g_assert (g_variant_is_of_type (state, G_VARIANT_TYPE_INT32));

  g_simple_action_set_state (action, state);

  stateval = g_variant_get_int32 (state);

  list = gtk_container_get_children (GTK_CONTAINER (priv->stack));

  for (iter = list; iter != NULL; iter = iter->next)
    {
      GtkWidget *child = iter->data;
      gint position = 0;

      gtk_container_child_get (GTK_CONTAINER (priv->stack), GTK_WIDGET (child),
                               "position", &position,
                               NULL);

      if (position == stateval)
        {
          tab = g_object_get_data (G_OBJECT (child), "PNL_TAB");
          gtk_stack_set_visible_child (priv->stack, child);
          break;
        }
    }

  /*
   * When clicking an active toggle button, we get the state callback but then
   * the toggle button disables the checked state. So ensure it stays on by
   * manually setting the state.
   */
  if (PNL_IS_TAB (tab))
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tab), TRUE);

  g_list_free (list);
}

static void
pnl_tab_strip_add (GtkContainer *container,
                   GtkWidget    *widget)
{
  PnlTabStrip *self = (PnlTabStrip *)container;
  PnlTabStripPrivate *priv = pnl_tab_strip_get_instance_private (self);

  g_assert (PNL_IS_TAB_STRIP (self));
  g_assert (GTK_IS_WIDGET (widget));

  if (PNL_IS_TAB (widget))
    pnl_tab_set_edge (PNL_TAB (widget), priv->edge);

  GTK_CONTAINER_CLASS (pnl_tab_strip_parent_class)->add (container, widget);
}

static void
pnl_tab_strip_destroy (GtkWidget *widget)
{
  PnlTabStrip *self = (PnlTabStrip *)widget;
  PnlTabStripPrivate *priv = pnl_tab_strip_get_instance_private (self);

  g_assert (PNL_IS_TAB_STRIP (self));

  pnl_tab_strip_set_stack (self, NULL);

  g_clear_object (&priv->action);
  g_clear_object (&priv->stack);

  GTK_WIDGET_CLASS (pnl_tab_strip_parent_class)->destroy (widget);
}

static void
pnl_tab_strip_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  PnlTabStrip *self = PNL_TAB_STRIP (object);

  switch (prop_id)
    {
    case PROP_EDGE:
      g_value_set_enum (value, pnl_tab_strip_get_edge (self));
      break;

    case PROP_STACK:
      g_value_set_object (value, pnl_tab_strip_get_stack (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
pnl_tab_strip_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  PnlTabStrip *self = PNL_TAB_STRIP (object);

  switch (prop_id)
    {
    case PROP_EDGE:
      pnl_tab_strip_set_edge (self, g_value_get_enum (value));
      break;

    case PROP_STACK:
      pnl_tab_strip_set_stack (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
pnl_tab_strip_class_init (PnlTabStripClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->get_property = pnl_tab_strip_get_property;
  object_class->set_property = pnl_tab_strip_set_property;

  widget_class->destroy = pnl_tab_strip_destroy;

  container_class->add = pnl_tab_strip_add;

  properties [PROP_EDGE] =
    g_param_spec_enum ("edge",
                       "Edge",
                       "The edge for the tab-strip",
                       GTK_TYPE_POSITION_TYPE,
                       GTK_POS_TOP,
                       (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_STACK] =
    g_param_spec_object ("stack",
                         "Stack",
                         "The stack of items to manage.",
                         GTK_TYPE_STACK,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "tabstrip");
}

static void
pnl_tab_strip_init (PnlTabStrip *self)
{
  PnlTabStripPrivate *priv = pnl_tab_strip_get_instance_private (self);
  GSimpleActionGroup *group;
  static const GActionEntry entries[] = {
    { "tab", NULL, "i", "0", set_tab_state },
  };

  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_HORIZONTAL);

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group), entries, G_N_ELEMENTS (entries), self);
  priv->action = g_object_ref (g_action_map_lookup_action (G_ACTION_MAP (group), "tab"));
  gtk_widget_insert_action_group (GTK_WIDGET (self), "tab-strip", G_ACTION_GROUP (group));
  g_object_unref (group);

  pnl_tab_strip_set_edge (self, GTK_POS_TOP);
}

static void
pnl_tab_strip_child_position_changed (PnlTabStrip *self,
                                      GParamSpec  *pspec,
                                      GtkWidget   *child)
{
  GVariant *state;
  GtkWidget *parent;
  PnlTab *tab;
  guint position;

  g_assert (PNL_IS_TAB_STRIP (self));
  g_assert (GTK_IS_WIDGET (child));

  tab = g_object_get_data (G_OBJECT (child), "PNL_TAB");

  if (!tab || !PNL_IS_TAB (tab))
    return;

  parent = gtk_widget_get_parent (child);

  gtk_container_child_get (GTK_CONTAINER (parent), child,
                           "position", &position,
                           NULL);

  gtk_container_child_set (GTK_CONTAINER (self), GTK_WIDGET (tab),
                           "position", position,
                           NULL);

  state = g_variant_new_int32 (position);
  gtk_actionable_set_action_target_value (GTK_ACTIONABLE (tab), state);
}

static void
pnl_tab_strip_child_title_changed (PnlTabStrip *self,
                                   GParamSpec  *pspec,
                                   GtkWidget   *child)
{
  g_autofree gchar *title = NULL;
  GtkWidget *parent;
  PnlTab *tab;

  g_assert (PNL_IS_TAB_STRIP (self));
  g_assert (GTK_IS_WIDGET (child));

  tab = g_object_get_data (G_OBJECT (child), "PNL_TAB");

  if (!PNL_IS_TAB (tab))
    return;

  parent = gtk_widget_get_parent (child);

  gtk_container_child_get (GTK_CONTAINER (parent), child,
                           "title", &title,
                           NULL);

  pnl_tab_set_title (tab, title);
}

static void
pnl_tab_strip_stack_notify_visible_child (PnlTabStrip *self,
                                          GParamSpec  *pspec,
                                          GtkStack    *stack)
{
  GtkWidget *visible;

  g_assert (PNL_IS_TAB_STRIP (self));
  g_assert (GTK_IS_STACK (stack));

  visible = gtk_stack_get_visible_child (stack);

  if (visible != NULL)
    {
      PnlTab *tab = g_object_get_data (G_OBJECT (visible), "PNL_TAB");

      if (PNL_IS_TAB (tab))
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tab), TRUE);
    }
}

static void
pnl_tab_strip_stack_add (PnlTabStrip *self,
                         GtkWidget   *widget,
                         GtkStack    *stack)
{
  PnlTabStripPrivate *priv = pnl_tab_strip_get_instance_private (self);
  GVariant *target;
  PnlTab *tab;
  gint position = 0;

  g_assert (PNL_IS_TAB_STRIP (self));
  g_assert (GTK_IS_WIDGET (widget));
  g_assert (GTK_IS_STACK (stack));

  gtk_container_child_get (GTK_CONTAINER (stack), widget,
                           "position", &position,
                           NULL);

  target = g_variant_new_int32 (position);

  tab = g_object_new (PNL_TYPE_TAB,
                      "action-name", "tab-strip.tab",
                      "action-target", target,
                      "edge", priv->edge,
                      "widget", widget,
                      NULL);

  g_object_set_data (G_OBJECT (widget), "PNL_TAB", tab);

  g_signal_connect_object (widget,
                           "child-notify::position",
                           G_CALLBACK (pnl_tab_strip_child_position_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (widget,
                           "child-notify::title",
                           G_CALLBACK (pnl_tab_strip_child_title_changed),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (tab));

  g_object_bind_property (widget, "visible", tab, "visible", G_BINDING_SYNC_CREATE);

  pnl_tab_strip_child_title_changed (self, NULL, widget);
  pnl_tab_strip_stack_notify_visible_child (self, NULL, stack);
}

static void
pnl_tab_strip_stack_remove (PnlTabStrip *self,
                            GtkWidget   *widget,
                            GtkStack    *stack)
{
  PnlTab *tab;

  g_assert (PNL_IS_TAB_STRIP (self));
  g_assert (GTK_IS_WIDGET (widget));
  g_assert (GTK_IS_STACK (stack));

  tab = g_object_get_data (G_OBJECT (widget), "PNL_TAB");

  if (PNL_IS_TAB (tab))
    gtk_container_remove (GTK_CONTAINER (self), GTK_WIDGET (tab));
}

GtkWidget *
pnl_tab_strip_new (void)
{
  return g_object_new (PNL_TYPE_TAB_STRIP, NULL);
}

/**
 * pnl_tab_strip_get_stack:
 *
 * Returns: (transfer none) (nullable): A #GtkStack or %NULL.
 */
GtkStack *
pnl_tab_strip_get_stack (PnlTabStrip *self)
{
  PnlTabStripPrivate *priv = pnl_tab_strip_get_instance_private (self);

  g_return_val_if_fail (PNL_IS_TAB_STRIP (self), NULL);

  return priv->stack;
}

static void
pnl_tab_strip_cold_plug (GtkWidget *widget,
                         gpointer   user_data)
{
  PnlTabStrip *self = user_data;
  PnlTabStripPrivate *priv = pnl_tab_strip_get_instance_private (self);

  g_assert (PNL_IS_TAB_STRIP (self));
  g_assert (GTK_IS_WIDGET (widget));

  pnl_tab_strip_stack_add (self, widget, priv->stack);
}

void
pnl_tab_strip_set_stack (PnlTabStrip *self,
                         GtkStack    *stack)
{
  PnlTabStripPrivate *priv = pnl_tab_strip_get_instance_private (self);

  g_return_if_fail (PNL_IS_TAB_STRIP (self));
  g_return_if_fail (!stack || GTK_IS_STACK (stack));

  if (stack != priv->stack)
    {
      if (priv->stack != NULL)
        {
          g_signal_handlers_disconnect_by_func (priv->stack,
                                                G_CALLBACK (pnl_tab_strip_stack_notify_visible_child),
                                                self);

          g_signal_handlers_disconnect_by_func (priv->stack,
                                                G_CALLBACK (pnl_tab_strip_stack_add),
                                                self);

          g_signal_handlers_disconnect_by_func (priv->stack,
                                                G_CALLBACK (pnl_tab_strip_stack_remove),
                                                self);

          gtk_container_foreach (GTK_CONTAINER (self), (GtkCallback)gtk_widget_destroy, NULL);

          g_clear_object (&priv->stack);
        }

      if (stack != NULL)
        {
          priv->stack = g_object_ref (stack);

          g_signal_connect_object (priv->stack,
                                   "notify::visible-child",
                                   G_CALLBACK (pnl_tab_strip_stack_notify_visible_child),
                                   self,
                                   G_CONNECT_SWAPPED);

          g_signal_connect_object (priv->stack,
                                   "add",
                                   G_CALLBACK (pnl_tab_strip_stack_add),
                                   self,
                                   G_CONNECT_SWAPPED);

          g_signal_connect_object (priv->stack,
                                   "remove",
                                   G_CALLBACK (pnl_tab_strip_stack_remove),
                                   self,
                                   G_CONNECT_SWAPPED);

          gtk_container_foreach (GTK_CONTAINER (priv->stack),
                                 pnl_tab_strip_cold_plug,
                                 self);
        }
    }
}

static void
pnl_tab_strip_update_edge (GtkWidget *widget,
                           gpointer   user_data)
{
  GtkPositionType edge = GPOINTER_TO_INT (user_data);

  g_assert (GTK_IS_WIDGET (widget));

  if (PNL_IS_TAB (widget))
    pnl_tab_set_edge (PNL_TAB (widget), edge);
}

GtkPositionType
pnl_tab_strip_get_edge (PnlTabStrip *self)
{
  PnlTabStripPrivate *priv = pnl_tab_strip_get_instance_private (self);

  g_return_val_if_fail (PNL_IS_TAB_STRIP (self), 0);

  return priv->edge;
}

void
pnl_tab_strip_set_edge (PnlTabStrip     *self,
                        GtkPositionType  edge)
{
  PnlTabStripPrivate *priv = pnl_tab_strip_get_instance_private (self);

  g_return_if_fail (PNL_IS_TAB_STRIP (self));
  g_return_if_fail (edge >= 0);
  g_return_if_fail (edge <= 3);

  if (priv->edge != edge)
    {
      GtkStyleContext *style_context;
      const gchar *class_name = NULL;

      priv->edge = edge;

      gtk_container_foreach (GTK_CONTAINER (self),
                             pnl_tab_strip_update_edge,
                             GINT_TO_POINTER (edge));

      style_context = gtk_widget_get_style_context (GTK_WIDGET (self));

      gtk_style_context_remove_class (style_context, "left-edge");
      gtk_style_context_remove_class (style_context, "top-edge");
      gtk_style_context_remove_class (style_context, "right-edge");
      gtk_style_context_remove_class (style_context, "bottom-edge");

      switch (edge)
        {
        case GTK_POS_LEFT:
          class_name = "left";
          break;

        case GTK_POS_RIGHT:
          class_name = "right";
          break;

        case GTK_POS_TOP:
          class_name = "top";
          break;

        case GTK_POS_BOTTOM:
          class_name = "bottom";
          break;

        default:
          g_assert_not_reached ();
        }

      gtk_style_context_add_class (style_context, class_name);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_EDGE]);
    }
}
