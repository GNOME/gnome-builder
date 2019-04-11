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
#include "dspy-name.h"

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
  DspyName *name = item;
  GtkWidget *row;
  GtkWidget *label;

  g_assert (DSPY_IS_NAME (name));

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      "visible", TRUE,
                      NULL);
  label = g_object_new (GTK_TYPE_LABEL,
                        "label", dspy_name_get_name (name),
                        "xalign", 0.0f,
                        "margin", 6,
                        "visible", TRUE,
                        NULL);
  gtk_container_add (GTK_CONTAINER (row), GTK_WIDGET (label));

  return row;
}

static void
connection_row_activated_cb (GbpDspySurface *self,
                             GtkListBoxRow  *row,
                             GtkListBox     *list_box)
{
  g_autoptr(GDBusConnection) bus = NULL;
  g_autoptr(DspyConnectionModel) model = NULL;
  const gchar *addr;

  g_assert (GBP_IS_DSPY_SURFACE (self));
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  if ((addr = g_object_get_data (G_OBJECT (row), "ADDRESS")))
    bus = g_dbus_connection_new_for_address_sync (addr,
                                                  (G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION |
                                                   G_DBUS_CONNECTION_FLAGS_DELAY_MESSAGE_PROCESSING),
                                                  NULL, NULL, NULL);
  else
    bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);

  model = dspy_connection_model_new ();
  dspy_connection_model_set_connection (model, bus);

  gtk_list_box_bind_model (self->names_list_box,
                           G_LIST_MODEL (model),
                           create_names_row, NULL, NULL);
}

static void
add_connection (GbpDspySurface *self,
                const gchar    *name,
                const gchar    *addr)
{
  GtkWidget *row;
  GtkWidget *title;

  g_assert (GBP_IS_DSPY_SURFACE (self));

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      "visible", TRUE,
                      NULL);
  g_object_set_data_full (G_OBJECT (row), "ADDRESS", g_strdup (addr), g_free);
  title = g_object_new (GTK_TYPE_LABEL,
                        "margin", 6,
                        "visible", TRUE,
                        "label", name,
                        "xalign", 0.0f,
                        NULL);
  gtk_container_add (GTK_CONTAINER (row), title);
  gtk_container_add (GTK_CONTAINER (self->connections_list_box), row);
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
  const gchar *bus;

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

  add_connection (self, _("System Bus"), NULL);
  if ((bus = g_getenv ("DBUS_SESSION_BUS_ADDRESS")))
    add_connection (self, _("Session Bus"), bus);
}

GbpDspySurface *
gbp_dspy_surface_new (void)
{
  return g_object_new (GBP_TYPE_DSPY_SURFACE, NULL);
}
