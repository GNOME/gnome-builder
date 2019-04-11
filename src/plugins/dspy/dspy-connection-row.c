/* dspy-connection-row.c
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

#define G_LOG_DOMAIN "dspy-connection-row"

#include "config.h"

#include "dspy-connection-row.h"

struct _DspyConnectionRow
{
  GtkListBoxRow  parent;
  GtkLabel      *label;
  gchar         *address;
  GBusType       bus_type;
};

G_DEFINE_TYPE (DspyConnectionRow, dspy_connection_row, GTK_TYPE_LIST_BOX_ROW)

static void
dspy_connection_row_finalize (GObject *object)
{
  DspyConnectionRow *self = (DspyConnectionRow *)object;

  g_clear_pointer (&self->address, g_free);

  G_OBJECT_CLASS (dspy_connection_row_parent_class)->finalize (object);
}

static void
dspy_connection_row_class_init (DspyConnectionRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = dspy_connection_row_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/dspy/dspy-connection-row.ui");
  gtk_widget_class_bind_template_child (widget_class, DspyConnectionRow, label);
}

static void
dspy_connection_row_init (DspyConnectionRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

DspyConnectionRow *
dspy_connection_row_new (void)
{
  return g_object_new (DSPY_TYPE_CONNECTION_ROW, NULL);
}

const gchar *
dspy_connection_row_get_address (DspyConnectionRow *self)
{
  g_return_val_if_fail (DSPY_IS_CONNECTION_ROW (self), NULL);

  return self->address;
}

void
dspy_connection_row_set_address (DspyConnectionRow *self,
                                 const gchar       *address)
{
  g_return_if_fail (DSPY_IS_CONNECTION_ROW (self));

  if (g_strcmp0 (address, self->address) != 0)
    {
      g_free (self->address);
      self->address = g_strdup (address);
    }
}

GBusType
dspy_connection_row_get_bus_type (DspyConnectionRow *self)
{
  g_return_val_if_fail (DSPY_IS_CONNECTION_ROW (self), 0);

  return self->bus_type;
}

void
dspy_connection_row_set_bus_type (DspyConnectionRow *self,
                                  GBusType           bus_type)
{
  g_return_if_fail (DSPY_IS_CONNECTION_ROW (self));

  self->bus_type = bus_type;
}

void
dspy_connection_row_set_title (DspyConnectionRow *self,
                               const gchar       *title)
{
  g_return_if_fail (DSPY_IS_CONNECTION_ROW (self));

  gtk_label_set_label (self->label, title);
}
