/* ide-debugger-controls.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-debugger-controls"

#include "config.h"

#include "ide-debugger-controls.h"

struct _IdeDebuggerControls
{
  GtkWidget    parent_instance;
  GtkRevealer *revealer;
};

G_DEFINE_FINAL_TYPE (IdeDebuggerControls, ide_debugger_controls, GTK_TYPE_WIDGET)

static void
ide_debugger_controls_dispose (GObject *object)
{
  IdeDebuggerControls *self = (IdeDebuggerControls *)object;

  g_clear_pointer ((GtkWidget **)&self->revealer, gtk_widget_unparent);

  G_OBJECT_CLASS (ide_debugger_controls_parent_class)->dispose (object);
}

static void
ide_debugger_controls_class_init (IdeDebuggerControlsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ide_debugger_controls_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/debuggerui/ide-debugger-controls.ui");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "idedebuggercontrols");
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerControls, revealer);
}

static void
ide_debugger_controls_init (IdeDebuggerControls *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

void
ide_debugger_controls_set_reveal_child (IdeDebuggerControls *self,
                                        gboolean             reveal_child)
{
  g_return_if_fail (IDE_IS_DEBUGGER_CONTROLS (self));

  gtk_revealer_set_reveal_child (self->revealer, reveal_child);
}

gboolean
ide_debugger_controls_get_reveal_child (IdeDebuggerControls *self)
{
  g_return_val_if_fail (IDE_IS_DEBUGGER_CONTROLS (self), FALSE);

  return gtk_revealer_get_reveal_child (self->revealer);
}
