/* ide-debugger-hover-controls.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-debugger-hover-controls"

#include "config.h"

#include <libide-debugger.h>
#include <libide-sourceview.h>

#include "ide-debugger-hover-controls.h"
#include "ide-debugger-breakpoints.h"
#include "ide-debugger-private.h"

struct _IdeDebuggerHoverControls
{
  AdwBin parent_instance;

  IdeDebugManager *debug_manager;
  GFile *file;
  guint line;

  GtkToggleButton *nobreak;
  GtkToggleButton *breakpoint;
  GtkToggleButton *countpoint;
};

G_DEFINE_FINAL_TYPE (IdeDebuggerHoverControls, ide_debugger_hover_controls, ADW_TYPE_BIN)

static void
ide_debugger_hover_controls_dispose (GObject *object)
{
  IdeDebuggerHoverControls *self = (IdeDebuggerHoverControls *)object;

  g_clear_object (&self->debug_manager);
  g_clear_object (&self->file);

  G_OBJECT_CLASS (ide_debugger_hover_controls_parent_class)->dispose (object);
}

static void
ide_debugger_hover_controls_class_init (IdeDebuggerHoverControlsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ide_debugger_hover_controls_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/debuggerui/ide-debugger-hover-controls.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerHoverControls, nobreak);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerHoverControls, breakpoint);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerHoverControls, countpoint);
}

static void
ide_debugger_hover_controls_init (IdeDebuggerHoverControls *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
on_toggle_cb (GtkToggleButton          *button,
              IdeDebuggerHoverControls *self)
{
  g_autoptr(IdeDebuggerBreakpoints) breakpoints = NULL;
  IdeDebuggerBreakMode break_type = IDE_DEBUGGER_BREAK_NONE;
  IdeDebuggerBreakpoint *breakpoint;

  g_assert (GTK_IS_TOGGLE_BUTTON (button));
  g_assert (IDE_IS_DEBUGGER_HOVER_CONTROLS (self));

  g_signal_handlers_block_by_func (self->nobreak, G_CALLBACK (on_toggle_cb), self);
  g_signal_handlers_block_by_func (self->breakpoint, G_CALLBACK (on_toggle_cb), self);
  g_signal_handlers_block_by_func (self->countpoint, G_CALLBACK (on_toggle_cb), self);

  breakpoints = ide_debug_manager_get_breakpoints_for_file (self->debug_manager, self->file);
  breakpoint = ide_debugger_breakpoints_get_line (breakpoints, self->line);

  if (button == self->nobreak)
    break_type = IDE_DEBUGGER_BREAK_NONE;
  else if (button == self->breakpoint)
    break_type = IDE_DEBUGGER_BREAK_BREAKPOINT;
  else if (button == self->countpoint)
    break_type = IDE_DEBUGGER_BREAK_COUNTPOINT;

  if (breakpoint != NULL)
    {
      _ide_debug_manager_remove_breakpoint (self->debug_manager, breakpoint);
      breakpoint = NULL;
    }

  switch (break_type)
    {
    default:
    case IDE_DEBUGGER_BREAK_NONE:
      gtk_toggle_button_set_active (self->nobreak, TRUE);
      gtk_toggle_button_set_active (self->breakpoint, FALSE);
      gtk_toggle_button_set_active (self->countpoint, FALSE);
      break;

    case IDE_DEBUGGER_BREAK_BREAKPOINT:
    case IDE_DEBUGGER_BREAK_COUNTPOINT:
      {
        g_autoptr(IdeDebuggerBreakpoint) to_insert = NULL;
        g_autofree gchar *path = g_file_get_path (self->file);

        to_insert = ide_debugger_breakpoint_new (NULL);

        ide_debugger_breakpoint_set_line (to_insert, self->line);
        ide_debugger_breakpoint_set_file (to_insert, path);
        ide_debugger_breakpoint_set_mode (to_insert, break_type);
        ide_debugger_breakpoint_set_enabled (to_insert, TRUE);

        _ide_debug_manager_add_breakpoint (self->debug_manager, to_insert);

        gtk_toggle_button_set_active (self->nobreak, FALSE);
        gtk_toggle_button_set_active (self->breakpoint, break_type == IDE_DEBUGGER_BREAK_BREAKPOINT);
        gtk_toggle_button_set_active (self->countpoint, break_type == IDE_DEBUGGER_BREAK_COUNTPOINT);
      }
      break;

    case IDE_DEBUGGER_BREAK_WATCHPOINT:
      /* TODO: watchpoint not yet supported */
      gtk_toggle_button_set_active (self->nobreak, FALSE);
      gtk_toggle_button_set_active (self->breakpoint, FALSE);
      gtk_toggle_button_set_active (self->countpoint, FALSE);
      break;
    }

  g_signal_handlers_unblock_by_func (self->nobreak, G_CALLBACK (on_toggle_cb), self);
  g_signal_handlers_unblock_by_func (self->breakpoint, G_CALLBACK (on_toggle_cb), self);
  g_signal_handlers_unblock_by_func (self->countpoint, G_CALLBACK (on_toggle_cb), self);
}

GtkWidget *
ide_debugger_hover_controls_new (IdeDebugManager *debug_manager,
                                 GFile           *file,
                                 guint            line)
{
  g_autoptr(IdeDebuggerBreakpoints) breakpoints = NULL;
  IdeDebuggerHoverControls *self;

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

  g_signal_connect (self->nobreak, "toggled", G_CALLBACK (on_toggle_cb), self);
  g_signal_connect (self->breakpoint, "toggled", G_CALLBACK (on_toggle_cb), self);
  g_signal_connect (self->countpoint, "toggled", G_CALLBACK (on_toggle_cb), self);

  return GTK_WIDGET (self);
}
