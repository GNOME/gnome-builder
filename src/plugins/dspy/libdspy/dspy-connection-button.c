/* dspy-connection-button.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#define G_LOG_DOMAIN "dspy-connection-button"

#include "config.h"

#include <glib/gi18n.h>

#include "dspy-connection-button.h"

typedef struct
{
  DspyConnection *connection;

  GtkImage *image;
  GtkLabel *label;
} DspyConnectionButtonPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (DspyConnectionButton, dspy_connection_button, GTK_TYPE_RADIO_BUTTON)

enum {
  PROP_0,
  PROP_BUS_TYPE,
  PROP_CONNECTION,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

/**
 * dspy_connection_button_new:
 *
 * Create a new #DspyConnectionButton.
 *
 * Returns: (transfer full): a newly created #DspyConnectionButton
 */
GtkWidget *
dspy_connection_button_new (void)
{
  return g_object_new (DSPY_TYPE_CONNECTION_BUTTON, NULL);
}

static gboolean
dspy_connection_button_query_tooltip (GtkWidget  *widget,
                                      gint        x,
                                      gint        y,
                                      gboolean    keyboard,
                                      GtkTooltip *tooltip)
{
  DspyConnectionButton *self = (DspyConnectionButton *)widget;
  DspyConnection *connection;

  g_assert (DSPY_IS_CONNECTION_BUTTON (self));

  if ((connection = dspy_connection_button_get_connection (self)))
    {
      GDBusConnection *bus = dspy_connection_get_connection (connection);
      const gchar *address = dspy_connection_get_address (connection);

      if (bus != NULL && address != NULL)
        {
          /* translators: %s is replaced with the address of the D-Bus bus */
          g_autofree gchar *text = g_strdup_printf (_("Connected to “%s”"), address);
          gtk_tooltip_set_text (tooltip, text);
          return TRUE;
        }
    }

  return FALSE;
}

static void
dspy_connection_button_finalize (GObject *object)
{
  DspyConnectionButton *self = (DspyConnectionButton *)object;
  DspyConnectionButtonPrivate *priv = dspy_connection_button_get_instance_private (self);

  g_clear_object (&priv->connection);

  G_OBJECT_CLASS (dspy_connection_button_parent_class)->finalize (object);
}

static void
dspy_connection_button_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  DspyConnectionButton *self = DSPY_CONNECTION_BUTTON (object);

  switch (prop_id)
    {
    case PROP_BUS_TYPE:
        {
          DspyConnection *conn = dspy_connection_button_get_connection (self);

          if (conn != NULL)
            g_value_set_enum (value, dspy_connection_get_bus_type (conn));
        }
      break;

    case PROP_CONNECTION:
      g_value_set_object (value, dspy_connection_button_get_connection (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
dspy_connection_button_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  DspyConnectionButton *self = DSPY_CONNECTION_BUTTON (object);

  switch (prop_id)
    {
    case PROP_BUS_TYPE:
        {
          GBusType bus_type = g_value_get_enum (value);

          if (bus_type == G_BUS_TYPE_SESSION || bus_type == G_BUS_TYPE_SYSTEM)
            {
              g_autoptr(DspyConnection) conn = dspy_connection_new_for_bus (bus_type);
              dspy_connection_button_set_connection (self, conn);
            }
        }
      break;

    case PROP_CONNECTION:
      dspy_connection_button_set_connection (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
dspy_connection_button_class_init (DspyConnectionButtonClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = dspy_connection_button_finalize;
  object_class->get_property = dspy_connection_button_get_property;
  object_class->set_property = dspy_connection_button_set_property;

  widget_class->query_tooltip = dspy_connection_button_query_tooltip;

  properties [PROP_BUS_TYPE] =
    g_param_spec_enum ("bus-type",
                       "Bus Type",
                       "Bus Type",
                       G_TYPE_BUS_TYPE,
                       G_BUS_TYPE_SESSION,
                       (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_CONNECTION] =
    g_param_spec_object ("connection",
                         "Connection",
                         "The connection underlying the button",
                         DSPY_TYPE_CONNECTION,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
dspy_connection_button_init (DspyConnectionButton *self)
{
  DspyConnectionButtonPrivate *priv = dspy_connection_button_get_instance_private (self);
  GtkBox *box;

  g_object_set (self,
                "has-tooltip", TRUE,
                "draw-indicator", FALSE,
                NULL);

  box = g_object_new (GTK_TYPE_BOX,
                      "halign", GTK_ALIGN_CENTER,
                      "orientation", GTK_ORIENTATION_HORIZONTAL,
                      "visible", TRUE,
                      NULL);
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (box));

  priv->image = g_object_new (GTK_TYPE_IMAGE,
                              "icon-name", "dialog-warning-symbolic",
                              "valign", GTK_ALIGN_CENTER,
                              "pixel-size", 16,
                              "margin-end", 6,
                              "visible", FALSE,
                              NULL);
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (priv->image));

  priv->label = g_object_new (GTK_TYPE_LABEL,
                              "visible", TRUE,
                              NULL);
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (priv->label));
}

/**
 * dspy_connection_button_get_connection:
 * @self: a #DspyConnection
 *
 * Returns: (transfer none) (nullable): a #DspyConnection or %NULL
 */
DspyConnection *
dspy_connection_button_get_connection (DspyConnectionButton *self)
{
  DspyConnectionButtonPrivate *priv = dspy_connection_button_get_instance_private (self);

  g_return_val_if_fail (DSPY_IS_CONNECTION_BUTTON (self), NULL);

  return priv->connection;
}

void
dspy_connection_button_set_connection (DspyConnectionButton *self,
                                       DspyConnection       *connection)
{
  DspyConnectionButtonPrivate *priv = dspy_connection_button_get_instance_private (self);

  g_return_if_fail (DSPY_IS_CONNECTION_BUTTON (self));
  g_return_if_fail (DSPY_IS_CONNECTION (connection));

  if (g_set_object (&priv->connection, connection))
    {
      GBusType bus_type = dspy_connection_get_bus_type (connection);

      if (bus_type == G_BUS_TYPE_SYSTEM)
        gtk_label_set_label (priv->label, _("System"));
      else if (bus_type == G_BUS_TYPE_SESSION)
        gtk_label_set_label (priv->label, _("Session"));
      else
        gtk_label_set_label (priv->label, _("Other"));

      g_object_bind_property (connection, "has-error", priv->image, "visible", G_BINDING_SYNC_CREATE);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CONNECTION]);
    }
}
