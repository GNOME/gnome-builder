/* ide-terminal-popover-row.c
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

#define G_LOG_DOMAIN "ide-terminal-popover-row"

#include "config.h"

#include "ide-terminal-popover-row.h"

struct _IdeTerminalPopoverRow
{
  GtkListBoxRow  parent_instance;

  IdeRuntime    *runtime;

  GtkLabel      *label;
  GtkImage      *check;
};

G_DEFINE_FINAL_TYPE (IdeTerminalPopoverRow, ide_terminal_popover_row, GTK_TYPE_LIST_BOX_ROW)

static void
ide_terminal_popover_row_finalize (GObject *object)
{
  IdeTerminalPopoverRow *self = (IdeTerminalPopoverRow *)object;

  g_clear_object (&self->runtime);

  G_OBJECT_CLASS (ide_terminal_popover_row_parent_class)->finalize (object);
}

static void
ide_terminal_popover_row_class_init (IdeTerminalPopoverRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ide_terminal_popover_row_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-terminal/ui/ide-terminal-popover-row.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeTerminalPopoverRow, label);
  gtk_widget_class_bind_template_child (widget_class, IdeTerminalPopoverRow, check);
}

static void
ide_terminal_popover_row_init (IdeTerminalPopoverRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
ide_terminal_popover_row_new (IdeRuntime *runtime)
{
  IdeTerminalPopoverRow *self;

  g_return_val_if_fail (IDE_IS_RUNTIME (runtime), NULL);

  self = g_object_new (IDE_TYPE_TERMINAL_POPOVER_ROW,
                       "visible", TRUE,
                       NULL);
  self->runtime = g_object_ref (runtime);
  gtk_label_set_label (self->label, ide_runtime_get_display_name (runtime));

  return GTK_WIDGET (g_steal_pointer (&self));
}

void
ide_terminal_popover_row_set_selected (IdeTerminalPopoverRow *self,
                                       gboolean               selected)
{
  g_return_if_fail (IDE_IS_TERMINAL_POPOVER_ROW (self));

  /* Always keep image visible */
  g_object_set (self->check,
                "icon-name", selected ? "object-select-symbolic" : NULL,
                NULL);
}

/**
 * ide_terminal_popover_row_get_runtime:
 * @self: a #IdeTerminalPopoverRow
 *
 * Gets the runtime for a row.
 *
 * Returns: (transfer none): an #IdeRuntime, or %NULL
 */
IdeRuntime *
ide_terminal_popover_row_get_runtime (IdeTerminalPopoverRow *self)
{
  g_return_val_if_fail (IDE_IS_TERMINAL_POPOVER_ROW (self), NULL);

  return self->runtime;
}
