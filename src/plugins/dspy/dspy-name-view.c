/* dspy-name-view.c
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

#define G_LOG_DOMAIN "dspy-name-view"

#include "config.h"

#include <dazzle.h>

#include "dspy-name-view.h"

struct _DspyNameView
{
  GtkBin           parent;

  GDBusConnection *connection;
  DspyName        *name;

  GtkLabel        *address_label;
  GtkLabel        *name_label;
  GtkLabel        *unique_label;
} DspyNameViewPrivate;

G_DEFINE_TYPE (DspyNameView, dspy_name_view, GTK_TYPE_BIN)

static void
dspy_name_view_finalize (GObject *object)
{
  DspyNameView *self = (DspyNameView *)object;

  g_clear_object (&self->connection);
  g_clear_object (&self->name);

  G_OBJECT_CLASS (dspy_name_view_parent_class)->finalize (object);
}

static void
dspy_name_view_class_init (DspyNameViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = dspy_name_view_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/dspy/dspy-name-view.ui");
  gtk_widget_class_bind_template_child (widget_class, DspyNameView, address_label);
  gtk_widget_class_bind_template_child (widget_class, DspyNameView, name_label);
  gtk_widget_class_bind_template_child (widget_class, DspyNameView, unique_label);

  g_type_ensure (DZL_TYPE_THREE_GRID);
}

static void
dspy_name_view_init (DspyNameView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

DspyNameView *
dspy_name_view_new (void)
{
  return g_object_new (DSPY_TYPE_NAME_VIEW, NULL);
}

static void
dspy_name_view_clear (DspyNameView *self)
{
  g_return_if_fail (DSPY_IS_NAME_VIEW (self));

  gtk_label_set_label (self->address_label, NULL);
  gtk_label_set_label (self->name_label, NULL);
  gtk_label_set_label (self->unique_label, NULL);
}

void
dspy_name_view_set_name (DspyNameView    *self,
                         GDBusConnection *connection,
                         GBusType         bus_type,
                         const gchar     *address,
                         DspyName        *name)
{
  g_autofree gchar *bus_address = NULL;

  g_return_if_fail (DSPY_IS_NAME_VIEW (self));

  if (self->connection == connection && self->name == name)
    return;

  dspy_name_view_clear (self);

  if (name == NULL)
    return;

  g_set_object (&self->connection, connection);
  g_set_object (&self->name, name);

  if (bus_type != G_BUS_TYPE_NONE)
    address = bus_address = g_dbus_address_get_for_bus_sync (bus_type, NULL, NULL);

  gtk_label_set_label (self->address_label, address);
  gtk_label_set_label (self->name_label, dspy_name_get_name (name));
  gtk_label_set_label (self->unique_label, dspy_name_get_owner (name));
}
