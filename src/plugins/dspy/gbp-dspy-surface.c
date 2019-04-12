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
#include "dspy-name-row.h"
#include "dspy-name-view.h"

#include "gbp-dspy-surface.h"

struct _GbpDspySurface
{
  IdeSurface           parent_instance;

  GtkScrolledWindow   *names_scroller;
  GtkListBox          *names_list_box;
  GtkStack            *view_stack;
  DspyNameView        *name_view;
  GtkBox              *bus_box;

  DspyConnectionModel *model;
};

typedef struct
{
  gchar *addr;
  GBusType bus_type;
} ConnectionInfo;

G_DEFINE_TYPE (GbpDspySurface, gbp_dspy_surface, IDE_TYPE_SURFACE)

static void
connection_info_free (gpointer data)
{
  ConnectionInfo *info = data;

  if (info)
    {
      g_clear_pointer (&info->addr, g_free);
      g_slice_free (ConnectionInfo, info);
    }
}

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
name_row_activated_cb (GbpDspySurface *self,
                       DspyNameRow    *row,
                       GtkListBox     *list_box)
{
  g_assert (GBP_IS_DSPY_SURFACE (self));
  g_assert (DSPY_IS_NAME_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  dspy_name_view_set_name (self->name_view,
                           dspy_connection_model_get_connection (self->model),
                           dspy_connection_model_get_bus_type (self->model),
                           dspy_connection_model_get_address (self->model),
                           dspy_name_row_get_name (row));
  gtk_stack_set_visible_child_name (self->view_stack, "name");
}

static void
on_connection_clicked_cb (GtkButton      *button,
                          ConnectionInfo *info)
{
  GbpDspySurface *self;
  g_autoptr(GDBusConnection) bus = NULL;
  g_autoptr(DspyConnectionModel) model = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (GTK_IS_BUTTON (button));
  g_assert (info != NULL);

  self = GBP_DSPY_SURFACE (gtk_widget_get_ancestor (GTK_WIDGET (button), GBP_TYPE_DSPY_SURFACE));

  if (info->bus_type)
    bus = g_bus_get_sync (info->bus_type, NULL, &error);
  else if (info->addr)
    bus = g_dbus_connection_new_for_address_sync (info->addr,
                                                  (G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION |
                                                   G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT),
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
  dspy_connection_model_set_bus_type (model, info->bus_type);
  dspy_connection_model_set_address (model, info->addr);
  gtk_list_box_bind_model (self->names_list_box, G_LIST_MODEL (model), create_names_row, NULL, NULL);
  g_set_object (&self->model, model);

  g_object_set (gtk_scrolled_window_get_vadjustment (self->names_scroller),
                "value", 0.0,
                NULL);
}

static void
add_connection (GbpDspySurface *self,
                const gchar    *name,
                GBusType        bus_type,
                const gchar    *addr)
{
  ConnectionInfo *info;
  GtkRadioButton *button;
  GList *children = NULL;

  g_assert (GBP_IS_DSPY_SURFACE (self));

  children = gtk_container_get_children (GTK_CONTAINER (self->bus_box));
  button = g_object_new (GTK_TYPE_RADIO_BUTTON,
                         "label", name,
                         "visible", TRUE,
                         "draw-indicator", FALSE,
                         "group", children ? children->data : NULL,
                         NULL);
  gtk_container_add (GTK_CONTAINER (self->bus_box), GTK_WIDGET (button));
  g_list_free (children);

  info = g_slice_new0 (ConnectionInfo);
  info->addr = g_strdup (addr);
  info->bus_type = bus_type;

  g_signal_connect_data (button,
                         "clicked",
                         G_CALLBACK (on_connection_clicked_cb),
                         g_steal_pointer (&info),
                         (GClosureNotify) connection_info_free,
                         0);
}

static void
gbp_dspy_surface_finalize (GObject *object)
{
  GbpDspySurface *self = (GbpDspySurface *)object;

  g_clear_object (&self->model);

  G_OBJECT_CLASS (gbp_dspy_surface_parent_class)->finalize (object);
}

static void
gbp_dspy_surface_class_init (GbpDspySurfaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gbp_dspy_surface_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/dspy/gbp-dspy-surface.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpDspySurface, bus_box);
  gtk_widget_class_bind_template_child (widget_class, GbpDspySurface, names_list_box);
  gtk_widget_class_bind_template_child (widget_class, GbpDspySurface, names_scroller);
  gtk_widget_class_bind_template_child (widget_class, GbpDspySurface, name_view);
  gtk_widget_class_bind_template_child (widget_class, GbpDspySurface, view_stack);

  g_type_ensure (DSPY_TYPE_NAME_VIEW);
}

static void
gbp_dspy_surface_init (GbpDspySurface *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_widget_set_name (GTK_WIDGET (self), "dspy");
  ide_surface_set_icon_name (IDE_SURFACE (self), "edit-find-symbolic");
  ide_surface_set_title (IDE_SURFACE (self), _("DBus Inspector"));

  g_signal_connect_object (self->names_list_box,
                           "row-activated",
                           G_CALLBACK (name_row_activated_cb),
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
