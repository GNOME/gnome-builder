/* ide-debugger-disassembly-view.c
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

#define G_LOG_DOMAIN "ide-debugger-disassembly-view"

#include "config.h"

#include <libide-sourceview.h>

#include "ide-debugger-disassembly-view.h"
#include "ide-debugger-instruction.h"

struct _IdeDebuggerDisassemblyView
{
  IdePage             parent_instance;

  /* Owned references */
  GPtrArray          *instructions;

  /* Template references */
  GtkSourceView      *source_view;
  GtkSourceBuffer    *source_buffer;

  IdeDebuggerAddress  current_address;
};

G_DEFINE_TYPE (IdeDebuggerDisassemblyView, ide_debugger_disassembly_view, IDE_TYPE_PAGE)

static void
ide_debugger_disassembly_view_destroy (GtkWidget *widget)
{
  IdeDebuggerDisassemblyView *self = (IdeDebuggerDisassemblyView *)widget;

  g_clear_pointer (&self->instructions, g_ptr_array_unref);

  GTK_WIDGET_CLASS (ide_debugger_disassembly_view_parent_class)->destroy (widget);
}

static void
ide_debugger_disassembly_view_class_init (IdeDebuggerDisassemblyViewClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->destroy = ide_debugger_disassembly_view_destroy;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/debuggerui/ide-debugger-disassembly-view.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerDisassemblyView, source_buffer);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerDisassemblyView, source_view);
}

static void
ide_debugger_disassembly_view_init (IdeDebuggerDisassemblyView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

void
ide_debugger_disassembly_view_set_current_address (IdeDebuggerDisassemblyView *self,
                                                   IdeDebuggerAddress          current_address)
{
  g_return_if_fail (IDE_IS_DEBUGGER_DISASSEMBLY_VIEW (self));

  self->current_address = current_address;

  /* Update gutter/etc */
}

/**
 * ide_debugger_disassembly_view_set_instructions:
 * @self: a #IdeDebuggerDisassemblyView
 * @instructions: (nullable) (element-type Ide.DebuggerInstruction): An array of
 *   instructions or %NULL.
 *
 * Sets the instructions to display in the disassembly view.
 *
 * This will take a reference to @instructions if non-%NULL so it is
 * important that you do not modify @instructions after calling this.
 *
 * Since: 3.32
 */
void
ide_debugger_disassembly_view_set_instructions (IdeDebuggerDisassemblyView *self,
                                                GPtrArray                  *instructions)
{
  g_return_if_fail (IDE_IS_DEBUGGER_DISASSEMBLY_VIEW (self));

  if (self->instructions == instructions)
    return;

  g_clear_pointer (&self->instructions, g_ptr_array_unref);
  if (instructions != NULL)
    self->instructions = g_ptr_array_ref (instructions);

  gtk_text_buffer_set_text (GTK_TEXT_BUFFER (self->source_buffer), "", 0);

  if (self->instructions != NULL && self->instructions->len > 0)
    {
      IdeDebuggerAddress first;
      GtkTextIter iter;
      GtkTextIter trim;

      first = ide_debugger_instruction_get_address (g_ptr_array_index (self->instructions, 0));

      gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (self->source_buffer), &iter);

      for (guint i = 0; i < self->instructions->len; i++)
        {
          IdeDebuggerInstruction *inst = g_ptr_array_index (self->instructions, i);
          g_autofree gchar *str = g_strdup_printf ("0x%"G_GINT64_MODIFIER"x <+%03"G_GINT64_MODIFIER"u>:  %s\n",
                                                   ide_debugger_instruction_get_address (inst),
                                                   ide_debugger_instruction_get_address (inst) - first,
                                                   ide_debugger_instruction_get_display (inst));
          gtk_text_buffer_insert (GTK_TEXT_BUFFER (self->source_buffer), &iter, str, -1);
        }

      /* Trim the trailing \n */
      trim = iter;
      gtk_text_iter_backward_char (&iter);
      gtk_text_buffer_delete (GTK_TEXT_BUFFER (self->source_buffer), &iter, &trim);
    }
}
