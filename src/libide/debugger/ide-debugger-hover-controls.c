/* ide-debugger-hover-controls.c
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#define G_LOG_DOMAIN "ide-debugger-hover-controls"

#include "debugger/ide-debugger-hover-controls.h"
#include "debugger/ide-debugger-breakpoints.h"
#include "debugger/ide-debug-manager.h"

struct _IdeDebuggerHoverControls
{
  GtkBin parent_instance;

  IdeDebugManager *debug_manager;
  GFile *file;
  guint line;

  GtkToggleButton *nobreak;
  GtkToggleButton *breakpoint;
  GtkToggleButton *countpoint;
};

G_DEFINE_TYPE (IdeDebuggerHoverControls, ide_debugger_hover_controls, GTK_TYPE_BIN)

static void
ide_debugger_hover_controls_destroy (GtkWidget *widget)
{
  IdeDebuggerHoverControls *self = (IdeDebuggerHoverControls *)widget;

  g_clear_object (&self->debug_manager);
  g_clear_object (&self->file);

  GTK_WIDGET_CLASS (ide_debugger_hover_controls_parent_class)->destroy (widget);
}

static void
ide_debugger_hover_controls_class_init (IdeDebuggerHoverControlsClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->destroy = ide_debugger_hover_controls_destroy;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-debugger-hover-controls.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerHoverControls, nobreak);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerHoverControls, breakpoint);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerHoverControls, countpoint);
}

static void
ide_debugger_hover_controls_init (IdeDebuggerHoverControls *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
ide_debugger_hover_controls_new (IdeDebugManager *debug_manager,
                                 GFile           *file,
                                 guint            line)
{
  IdeDebuggerHoverControls *self;
  IdeDebuggerBreakpoints *breakpoints;

  self = g_object_new (IDE_TYPE_DEBUGGER_HOVER_CONTROLS, NULL);
  self->debug_manager = g_object_ref (debug_manager);
  self->file = g_object_ref (file);
  self->line = line;

  if ((breakpoints = ide_debug_manager_get_breakpoints_for_file (debug_manager, file)))
    {
      IdeDebuggerBreakMode mode;

      mode = ide_debugger_breakpoints_get_line_mode (breakpoints, line);

      switch (mode)
        {
        default:
        case IDE_DEBUGGER_BREAK_NONE:
          gtk_toggle_button_set_active (self->nobreak, TRUE);
          break;

        case IDE_DEBUGGER_BREAK_BREAKPOINT:
          gtk_toggle_button_set_active (self->breakpoint, TRUE);
          break;

        case IDE_DEBUGGER_BREAK_COUNTPOINT:
          gtk_toggle_button_set_active (self->countpoint, TRUE);
          break;

        case IDE_DEBUGGER_BREAK_WATCHPOINT:
          /* TODO: not currently supported */
          break;
        }
    }

  return GTK_WIDGET (self);
}
