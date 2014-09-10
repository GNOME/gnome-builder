/*
 * gedit-menu-stack-switcher.c
 * This file is part of gedit
 *
 * Copyright (C) 2014 - Steve Frécinaux
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include "gedit-menu-stack-switcher.h"

struct _GeditMenuStackSwitcherPrivate
{
  GtkStack *stack;
  GtkWidget *label;
  GtkWidget *button_box;
  GtkWidget *popover;
  GHashTable *buttons;
  gboolean in_child_changed;
};

enum {
  PROP_0,
  PROP_STACK
};

G_DEFINE_TYPE_WITH_PRIVATE (GeditMenuStackSwitcher, gedit_menu_stack_switcher, GTK_TYPE_MENU_BUTTON)

static void
gedit_menu_stack_switcher_init (GeditMenuStackSwitcher *switcher)
{
  GeditMenuStackSwitcherPrivate *priv;
  GtkWidget *box;
  GtkWidget *arrow;
  GtkStyleContext *context;

  priv = gedit_menu_stack_switcher_get_instance_private (switcher);
  switcher->priv = priv;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);

  arrow = gtk_image_new_from_icon_name ("pan-down-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_box_pack_end (GTK_BOX (box), arrow, FALSE, TRUE, 6);
  gtk_widget_set_valign (arrow, GTK_ALIGN_BASELINE);

  priv->label = gtk_label_new (NULL);
  gtk_widget_set_valign (priv->label, GTK_ALIGN_BASELINE);
  gtk_box_pack_start (GTK_BOX (box), priv->label, TRUE, TRUE, 6);

  // FIXME: this is not correct if this widget becomes more generic
  // and used also outside the header bar, but for now we just want
  // the same style as title labels
  context = gtk_widget_get_style_context (priv->label);
  gtk_style_context_add_class (context, "title");

  gtk_widget_show_all (box);
  gtk_container_add (GTK_CONTAINER (switcher), box);

  priv->popover = gtk_popover_new (GTK_WIDGET (switcher));
  gtk_popover_set_position (GTK_POPOVER (priv->popover), GTK_POS_BOTTOM);
  context = gtk_widget_get_style_context (priv->popover);
  gtk_style_context_add_class (context, "gedit-menu-stack-switcher");

  priv->button_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);

  gtk_widget_show (priv->button_box);

  gtk_container_add (GTK_CONTAINER (priv->popover), priv->button_box);

  gtk_menu_button_set_popover (GTK_MENU_BUTTON (switcher), priv->popover);

  priv->buttons = g_hash_table_new (g_direct_hash, g_direct_equal);
}

static void
clear_popover (GeditMenuStackSwitcher *switcher)
{
  GeditMenuStackSwitcherPrivate *priv = switcher->priv;

  gtk_container_foreach (GTK_CONTAINER (priv->button_box), (GtkCallback) gtk_widget_destroy, switcher);
}

static void
on_button_clicked (GtkWidget              *widget,
                   GeditMenuStackSwitcher *switcher)
{
  GeditMenuStackSwitcherPrivate *priv = switcher->priv;
  GtkWidget *child;

  if (!priv->in_child_changed)
    {
      child = g_object_get_data (G_OBJECT (widget), "stack-child");
      gtk_stack_set_visible_child (priv->stack, child);
      gtk_widget_hide (priv->popover);
    }
}

static void
update_button (GeditMenuStackSwitcher *switcher,
               GtkWidget              *widget,
               GtkWidget              *button)
{
  GeditMenuStackSwitcherPrivate *priv = switcher->priv;
  GList *children;

  /* We get spurious notifications while the stack is being
   * destroyed, so for now check the child actually exists
   */
  children = gtk_container_get_children (GTK_CONTAINER (priv->stack));
  if (g_list_index (children, widget) >= 0)
    {
      gchar *title;

      gtk_container_child_get (GTK_CONTAINER (priv->stack), widget,
                               "title", &title,
                               NULL);

      gtk_button_set_label (GTK_BUTTON (button), title);
      gtk_widget_set_visible (button, gtk_widget_get_visible (widget) && (title != NULL));
      gtk_widget_set_size_request (button, 100, -1);

      if (widget == gtk_stack_get_visible_child (priv->stack))
        {
          gtk_label_set_label (GTK_LABEL (priv->label), title);
        }

      g_free (title);
    }

   g_list_free (children);
}

static void
on_title_icon_visible_updated (GtkWidget              *widget,
                               GParamSpec             *pspec,
                               GeditMenuStackSwitcher *switcher)
{
  GeditMenuStackSwitcherPrivate *priv = switcher->priv;
  GtkWidget *button;

  button = g_hash_table_lookup (priv->buttons, widget);
  update_button (switcher, widget, button);
}

static void
on_position_updated (GtkWidget        *widget,
                     GParamSpec       *pspec,
                     GeditMenuStackSwitcher *switcher)
{
  GeditMenuStackSwitcherPrivate *priv = switcher->priv;
  GtkWidget *button;
  gint position;

  button = g_hash_table_lookup (priv->buttons, widget);

  gtk_container_child_get (GTK_CONTAINER (priv->stack), widget,
                           "position", &position,
                           NULL);

  gtk_box_reorder_child (GTK_BOX (priv->button_box), button, position);
}

static void
add_child (GeditMenuStackSwitcher *switcher,
           GtkWidget              *widget)
{
  GeditMenuStackSwitcherPrivate *priv = switcher->priv;
  GtkWidget *button;
  GList *group;

  button = gtk_radio_button_new (NULL);
  gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (button), FALSE);
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);

  update_button (switcher, widget, button);

  group = gtk_container_get_children (GTK_CONTAINER (priv->button_box));
  if (group != NULL)
    {
      gtk_radio_button_join_group (GTK_RADIO_BUTTON (button), GTK_RADIO_BUTTON (group->data));
      g_list_free (group);
    }

  gtk_container_add (GTK_CONTAINER (priv->button_box), button);

  g_object_set_data (G_OBJECT (button), "stack-child", widget);
  g_signal_connect (button, "clicked", G_CALLBACK (on_button_clicked), switcher);
  g_signal_connect (widget, "notify::visible", G_CALLBACK (on_title_icon_visible_updated), switcher);
  g_signal_connect (widget, "child-notify::title", G_CALLBACK (on_title_icon_visible_updated), switcher);
  g_signal_connect (widget, "child-notify::icon-name", G_CALLBACK (on_title_icon_visible_updated), switcher);
  g_signal_connect (widget, "child-notify::position", G_CALLBACK (on_position_updated), switcher);

  g_hash_table_insert (priv->buttons, widget, button);
}

static void
foreach_stack (GtkWidget              *widget,
               GeditMenuStackSwitcher *switcher)
{
  add_child (switcher, widget);
}

static void
populate_popover (GeditMenuStackSwitcher *switcher)
{
  GeditMenuStackSwitcherPrivate *priv = switcher->priv;

  gtk_container_foreach (GTK_CONTAINER (priv->stack), (GtkCallback)foreach_stack, switcher);
}

static void
on_child_changed (GtkWidget              *widget,
                  GParamSpec             *pspec,
                  GeditMenuStackSwitcher *switcher)
{
  GeditMenuStackSwitcherPrivate *priv = switcher->priv;
  GtkWidget *child;
  GtkWidget *button;

  child = gtk_stack_get_visible_child (GTK_STACK (widget));
  if (child)
    {
      gchar *title;

      gtk_container_child_get (GTK_CONTAINER (priv->stack), child,
                               "title", &title,
                               NULL);

      gtk_label_set_label (GTK_LABEL (priv->label), title);
      g_free (title);
    }

  button = g_hash_table_lookup (priv->buttons, child);
  if (button != NULL)
    {
      priv->in_child_changed = TRUE;
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
      priv->in_child_changed = FALSE;
    }
}

static void
on_stack_child_added (GtkStack               *stack,
                      GtkWidget              *widget,
                      GeditMenuStackSwitcher *switcher)
{
  add_child (switcher, widget);
}

static void
on_stack_child_removed (GtkStack               *stack,
                        GtkWidget              *widget,
                        GeditMenuStackSwitcher *switcher)
{
  GeditMenuStackSwitcherPrivate *priv = switcher->priv;
  GtkWidget *button;

  button = g_hash_table_lookup (priv->buttons, widget);
  gtk_container_remove (GTK_CONTAINER (priv->button_box), button);
  g_hash_table_remove (priv->buttons, widget);
}

static void
disconnect_stack_signals (GeditMenuStackSwitcher *switcher)
{
  GeditMenuStackSwitcherPrivate *priv = switcher->priv;

  g_signal_handlers_disconnect_by_func (priv->stack, on_stack_child_added, switcher);
  g_signal_handlers_disconnect_by_func (priv->stack, on_stack_child_removed, switcher);
  g_signal_handlers_disconnect_by_func (priv->stack, on_child_changed, switcher);
  g_signal_handlers_disconnect_by_func (priv->stack, disconnect_stack_signals, switcher);
}

static void
connect_stack_signals (GeditMenuStackSwitcher *switcher)
{
  GeditMenuStackSwitcherPrivate *priv = switcher->priv;

  g_signal_connect (priv->stack, "add",
                    G_CALLBACK (on_stack_child_added), switcher);
  g_signal_connect (priv->stack, "remove",
                    G_CALLBACK (on_stack_child_removed), switcher);
  g_signal_connect (priv->stack, "notify::visible-child",
                    G_CALLBACK (on_child_changed), switcher);
  g_signal_connect_swapped (priv->stack, "destroy",
                            G_CALLBACK (disconnect_stack_signals), switcher);
}

void
gedit_menu_stack_switcher_set_stack (GeditMenuStackSwitcher *switcher,
                                     GtkStack               *stack)
{
  GeditMenuStackSwitcherPrivate *priv;

  g_return_if_fail (GEDIT_IS_MENU_STACK_SWITCHER (switcher));
  g_return_if_fail (stack == NULL || GTK_IS_STACK (stack));

  priv = switcher->priv;

  if (priv->stack == stack)
    return;

  if (priv->stack)
    {
      disconnect_stack_signals (switcher);
      clear_popover (switcher);
      g_clear_object (&priv->stack);
    }

  if (stack)
    {
      priv->stack = g_object_ref (stack);
      populate_popover (switcher);
      connect_stack_signals (switcher);
    }

  gtk_widget_queue_resize (GTK_WIDGET (switcher));

  g_object_notify (G_OBJECT (switcher), "stack");
}

GtkStack *
gedit_menu_stack_switcher_get_stack (GeditMenuStackSwitcher *switcher)
{
  g_return_val_if_fail (GEDIT_IS_MENU_STACK_SWITCHER (switcher), NULL);

  return switcher->priv->stack;
}

static void
gedit_menu_stack_switcher_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  GeditMenuStackSwitcher *switcher = GEDIT_MENU_STACK_SWITCHER (object);
  GeditMenuStackSwitcherPrivate *priv = switcher->priv;

  switch (prop_id)
    {
    case PROP_STACK:
      g_value_set_object (value, priv->stack);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gedit_menu_stack_switcher_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  GeditMenuStackSwitcher *switcher = GEDIT_MENU_STACK_SWITCHER (object);

  switch (prop_id)
    {
    case PROP_STACK:
      gedit_menu_stack_switcher_set_stack (switcher, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gedit_menu_stack_switcher_dispose (GObject *object)
{
  GeditMenuStackSwitcher *switcher = GEDIT_MENU_STACK_SWITCHER (object);

  gedit_menu_stack_switcher_set_stack (switcher, NULL);

  G_OBJECT_CLASS (gedit_menu_stack_switcher_parent_class)->dispose (object);
}

static void
gedit_menu_stack_switcher_finalize (GObject *object)
{
  GeditMenuStackSwitcher *switcher = GEDIT_MENU_STACK_SWITCHER (object);
  GeditMenuStackSwitcherPrivate *priv = switcher->priv;

  g_hash_table_destroy (priv->buttons);

  G_OBJECT_CLASS (gedit_menu_stack_switcher_parent_class)->finalize (object);
}

static void
gedit_menu_stack_switcher_class_init (GeditMenuStackSwitcherClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gedit_menu_stack_switcher_get_property;
  object_class->set_property = gedit_menu_stack_switcher_set_property;
  object_class->dispose = gedit_menu_stack_switcher_dispose;
  object_class->finalize = gedit_menu_stack_switcher_finalize;

  g_object_class_install_property (object_class,
                                   PROP_STACK,
                                   g_param_spec_object ("stack",
                                                        "Stack",
                                                        "Stack",
                                                        GTK_TYPE_STACK,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT));
}

GtkWidget *
gedit_menu_stack_switcher_new (void)
{
  return g_object_new (GEDIT_TYPE_MENU_STACK_SWITCHER, NULL);
}

/* ex:set ts=2 sw=2 et: */
