/* gb-drawer.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#include <glib/gi18n.h>

#include "gb-drawer.h"

struct _GbDrawerPrivate
{
  GtkStackSwitcher *switcher;
  GtkStack         *stack;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbDrawer, gb_drawer, GTK_TYPE_BIN)

enum {
  PROP_0,
  PROP_CURRENT_PAGE,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

GtkWidget *
gb_drawer_new (void)
{
  return g_object_new (GB_TYPE_DRAWER, NULL);
}

GtkWidget *
gb_drawer_get_current_page (GbDrawer *drawer)
{
  g_return_val_if_fail (GB_IS_DRAWER (drawer), NULL);

  return gtk_stack_get_visible_child (drawer->priv->stack);
}

void
gb_drawer_set_current_page (GbDrawer  *drawer,
                            GtkWidget *widget)
{
  g_return_if_fail (GB_IS_DRAWER (drawer));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_stack_set_visible_child (drawer->priv->stack, widget);

  g_object_notify_by_pspec (G_OBJECT (drawer),
                            gParamSpecs [PROP_CURRENT_PAGE]);
}

static void
gb_drawer_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  GbDrawer *self = GB_DRAWER (object);

  switch (prop_id)
    {
    case PROP_CURRENT_PAGE:
      g_value_set_object (value, gb_drawer_get_current_page (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_drawer_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  GbDrawer *self = GB_DRAWER (object);

  switch (prop_id)
    {
    case PROP_CURRENT_PAGE:
      gb_drawer_set_current_page (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_drawer_class_init (GbDrawerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gb_drawer_get_property;
  object_class->set_property = gb_drawer_set_property;

  gParamSpecs [PROP_CURRENT_PAGE] =
    g_param_spec_object ("current-page",
                         _("Current Page"),
                         _("The current page of the drawer."),
                         GTK_TYPE_WIDGET,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_CURRENT_PAGE,
                                   gParamSpecs [PROP_CURRENT_PAGE]);
}

static void
gb_drawer_init (GbDrawer *self)
{
  GtkBox *box;

  self->priv = gb_drawer_get_instance_private (self);

  box = g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_VERTICAL,
                      "spacing", 0,
                      "visible", TRUE,
                      NULL);
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (box));

  self->priv->switcher = g_object_new (GTK_TYPE_STACK_SWITCHER,
                                       "vexpand", FALSE,
                                       "visible", TRUE,
                                       NULL);
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (self->priv->switcher));

  self->priv->stack = g_object_new (GTK_TYPE_STACK,
                                    "vexpand", TRUE,
                                    "visible", TRUE,
                                    NULL);
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (self->priv->stack));
}
