/* gbp-dspy-surface.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "gbp-dspy-surface"

#include "config.h"

#include <dazzle.h>
#include <glib/gi18n.h>

#include "dspy-connection-model.h"
#include "dspy-connection-row.h"
#include "dspy-name.h"
#include "dspy-name-row.h"

#include "gbp-dspy-surface.h"

struct _GbpDspySurface
{
  IdeSurface parent_instance;

  GtkListBox        *connections_list_box;
  GtkScrolledWindow *connections_scroller;
  DzlMultiPaned     *multi_paned;
  GtkScrolledWindow *names_scroller;
  GtkListBox        *names_list_box;
};

G_DEFINE_TYPE (GbpDspySurface, gbp_dspy_surface, IDE_TYPE_SURFACE)

static GtkWidget *
create_names_row (gpointer item,
                  gpointer user_data)
{
  DspyNameRow *row;
  DspyName *name = item;

  g_assert (DSPY_IS_NAME (name));

  row = dspy_name_row_new (name);
  gtk_widget_show (GTK_WIDGET (row));

  return GTK_WIDGET (row);
}

static void
connection_row_activated_cb (GbpDspySurface *self,
                             GtkListBoxRow  *row,
                             GtkListBox     *list_box)
{
  g_autoptr(GDBusConnection) bus = NULL;
  g_autoptr(DspyConnectionModel) model = NULL;
  g_autoptr(GError) error = NULL;
  const gchar *addr;
  GBusType bus_type;

  g_assert (GBP_IS_DSPY_SURFACE (self));
  g_assert (DSPY_IS_CONNECTION_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  if ((bus_type = dspy_connection_row_get_bus_type (DSPY_CONNECTION_ROW (row))))
    bus = g_bus_get_sync (bus_type, NULL, &error);
  else if ((addr = dspy_connection_row_get_address (DSPY_CONNECTION_ROW (row))))
    bus = g_dbus_connection_new_for_address_sync (addr,
                                                  G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
                                                  NULL, NULL, &error);
  else
    g_return_if_reached ();

  if (error != NULL)
    {
      g_critical ("Failed to connect to bus: %s", error->message);
      return;
    }

  model = dspy_connection_model_new ();
  dspy_connection_model_set_connection (model, bus);

  gtk_list_box_bind_model (self->names_list_box, G_LIST_MODEL (model), create_names_row, NULL, NULL);
}

static void
add_connection (GbpDspySurface *self,
                const gchar    *name,
                GBusType        bus_type,
                const gchar    *addr)
{
  DspyConnectionRow *row;

  g_assert (GBP_IS_DSPY_SURFACE (self));

  row = dspy_connection_row_new ();
  dspy_connection_row_set_title (row, name);

  if (bus_type != G_BUS_TYPE_NONE)
    dspy_connection_row_set_bus_type (row, bus_type);
  else if (addr)
    dspy_connection_row_set_address (row, addr);
  else
    g_return_if_reached ();

  gtk_container_add (GTK_CONTAINER (self->connections_list_box), GTK_WIDGET (row));

  gtk_widget_show (GTK_WIDGET (row));
}

static void
gbp_dspy_surface_finalize (GObject *object)
{
  G_OBJECT_CLASS (gbp_dspy_surface_parent_class)->finalize (object);
}

static void
gbp_dspy_surface_class_init (GbpDspySurfaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gbp_dspy_surface_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/dspy/gbp-dspy-surface.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpDspySurface, connections_list_box);
  gtk_widget_class_bind_template_child (widget_class, GbpDspySurface, connections_scroller);
  gtk_widget_class_bind_template_child (widget_class, GbpDspySurface, multi_paned);
  gtk_widget_class_bind_template_child (widget_class, GbpDspySurface, names_list_box);
  gtk_widget_class_bind_template_child (widget_class, GbpDspySurface, names_scroller);
}

static void
gbp_dspy_surface_init (GbpDspySurface *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_widget_set_name (GTK_WIDGET (self), "dspy");
  ide_surface_set_icon_name (IDE_SURFACE (self), "edit-find-symbolic");
  ide_surface_set_title (IDE_SURFACE (self), _("DBus Inspector"));

  gtk_container_child_set (GTK_CONTAINER (self->multi_paned), GTK_WIDGET (self->connections_scroller),
                           "position", 200,
                           NULL);
  gtk_container_child_set (GTK_CONTAINER (self->multi_paned), GTK_WIDGET (self->names_scroller),
                           "position", 300,
                           NULL);

  g_signal_connect_object (self->connections_list_box,
                           "row-activated",
                           G_CALLBACK (connection_row_activated_cb),
                           self,
                           G_CONNECT_SWAPPED);

  add_connection (self, _("System Bus"), G_BUS_TYPE_SYSTEM, NULL);
  add_connection (self, _("Session Bus"), G_BUS_TYPE_SESSION, NULL);
}

GbpDspySurface *
gbp_dspy_surface_new (void)
{
  return g_object_new (GBP_TYPE_DSPY_SURFACE, NULL);
}
