/* dspy-name-row.c
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

#define G_LOG_DOMAIN "dspy-name-row"

#include "config.h"

#include <glib/gi18n.h>

#include "dspy-name-row.h"

struct _DspyNameRow
{
  GtkListBoxRow  parent;

  DspyName      *name;

  GtkLabel      *label;
  GtkLabel      *subtitle;
};

G_DEFINE_TYPE (DspyNameRow, dspy_name_row, GTK_TYPE_LIST_BOX_ROW)

static void
update_subtitle (DspyNameRow *self)
{
  g_autoptr(GString) str = g_string_new (NULL);
  GPid pid = dspy_name_get_pid (self->name);

  if (dspy_name_get_activatable (self->name))
    g_string_append_printf (str, _("%s: %s"), _("Activatable"), _("Yes"));
  else
    g_string_append_printf (str, _("%s: %s"), _("Activatable"), _("No"));

  if (pid != 0)
    {
      g_string_append (str, ", ");
      g_string_append_printf (str, _("%s: %u"), _("Pid"), pid);
    }

  gtk_label_set_label (self->subtitle, str->str);
}

static void
dspy_name_row_finalize (GObject *object)
{
  DspyNameRow *self = (DspyNameRow *)object;

  g_clear_object (&self->name);

  G_OBJECT_CLASS (dspy_name_row_parent_class)->finalize (object);
}

static void
dspy_name_row_class_init (DspyNameRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = dspy_name_row_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/dspy/dspy-name-row.ui");
  gtk_widget_class_bind_template_child (widget_class, DspyNameRow, label);
  gtk_widget_class_bind_template_child (widget_class, DspyNameRow, subtitle);
}

static void
dspy_name_row_init (DspyNameRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

DspyNameRow *
dspy_name_row_new (DspyName *name)
{
  DspyNameRow *self;

  g_return_val_if_fail (DSPY_IS_NAME (name), NULL);

  self = g_object_new (DSPY_TYPE_NAME_ROW, NULL);
  self->name = g_object_ref (name);

  gtk_label_set_label (self->label, dspy_name_get_name (name));

  g_signal_connect_object (name,
                           "notify::pid",
                           G_CALLBACK (update_subtitle),
                           self,
                           G_CONNECT_SWAPPED);

  update_subtitle (self);

  return g_steal_pointer (&self);
}

/**
 * dspy_name_row_get_name:
 * @self: a #DspyNameRow
 *
 * Returns: (transfer none): a #DspyNameRow or %NULL
 */
DspyName *
dspy_name_row_get_name (DspyNameRow *self)
{
  g_return_val_if_fail (DSPY_IS_NAME_ROW (self), NULL);

  return self->name;
}
