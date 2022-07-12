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

#include "gbp-debuggerui-resources.h"
#include "ide-debugger-disassembly-view.h"
#include "ide-debugger-instruction.h"

#define TAG_CURRENT_BKPT "-Builder:current-breakpoint"
#define TAG_CURRENT_LINE "current-line"

struct _IdeDebuggerDisassemblyView
{
  IdePage             parent_instance;

  /* Owned references */
  GPtrArray          *instructions;

  /* Template references */
  GtkSourceView      *source_view;
  GtkSourceBuffer    *source_buffer;
  GtkTextTag         *breakpoint;

  IdeDebuggerAddress  current_address;
};

G_DEFINE_FINAL_TYPE (IdeDebuggerDisassemblyView, ide_debugger_disassembly_view, IDE_TYPE_PAGE)

static GdkRGBA fallback_paragraph_bg;

static gboolean
style_scheme_name_to_object (GBinding     *binding,
                             const GValue *from_value,
                             GValue       *to_value,
                             gpointer      user_data)
{
  GtkSourceStyleSchemeManager *manager = gtk_source_style_scheme_manager_get_default ();
  GtkSourceStyleScheme *scheme = gtk_source_style_scheme_manager_get_scheme (manager, g_value_get_string (from_value));

  g_value_set_object (to_value, scheme);

  return TRUE;
}

static void
setup_breakpoint_tag (IdeDebuggerDisassemblyView *self)
{
  GtkSourceStyleScheme *scheme;
  GtkSourceStyle *style;

  g_assert (IDE_IS_DEBUGGER_DISASSEMBLY_VIEW (self));

  if (self->breakpoint == NULL)
    self->breakpoint = gtk_text_buffer_create_tag (GTK_TEXT_BUFFER (self->source_buffer), NULL, NULL);
  else
    g_object_set (self->breakpoint,
                  "paragraph-background", NULL,
                  "background", NULL,
                  "foreground", NULL,
                  "paragraph-background-set", FALSE,
                  "background-set", FALSE,
                  "foreground-set", FALSE,
                  NULL);

  if ((scheme = gtk_source_buffer_get_style_scheme (self->source_buffer)))
    {
      if ((style = gtk_source_style_scheme_get_style (scheme, TAG_CURRENT_BKPT)))
        gtk_source_style_apply (style, self->breakpoint);
      else if ((style = gtk_source_style_scheme_get_style (scheme, TAG_CURRENT_LINE)))
        {
          g_autoptr(GdkRGBA) background = NULL;
          gboolean background_set = FALSE;

          gtk_source_style_apply (style, self->breakpoint);

          g_object_get (self->breakpoint,
                        "background-rgba", &background,
                        "background-set", &background_set,
                        NULL);

          /* Use paragraph background instead of background */
          if (background_set)
            g_object_set (self->breakpoint,
                          "background-set", FALSE,
                          "paragraph-background-rgba", background,
                          NULL);
        }
      else
        g_object_set (self->breakpoint,
                      "paragraph-background-rgba", &fallback_paragraph_bg,
                      NULL);
    }
}

static void
notify_style_scheme_cb (IdeDebuggerDisassemblyView *self,
                        GParamSpec                 *pspec,
                        GtkSourceBuffer            *buffer)
{
  g_assert (IDE_IS_DEBUGGER_DISASSEMBLY_VIEW (self));
  g_assert (GTK_SOURCE_IS_BUFFER (buffer));

  setup_breakpoint_tag (self);
}

static gboolean
scroll_to_insert_in_idle_cb (gpointer user_data)
{
  IdeDebuggerDisassemblyView *self = user_data;
  GtkTextMark *mark;
  GtkTextIter iter;

  g_assert (IDE_IS_DEBUGGER_DISASSEMBLY_VIEW (self));

  mark = gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (self->source_buffer));
  gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (self->source_buffer), &iter, mark);
  ide_source_view_jump_to_iter (GTK_TEXT_VIEW (self->source_view), &iter,
                                0.0, TRUE, 1.0, 0.5);

  return G_SOURCE_REMOVE;
}

static void
ide_debugger_disassembly_view_root (GtkWidget *widget)
{
  IdeDebuggerDisassemblyView *self = (IdeDebuggerDisassemblyView *)widget;

  g_assert (IDE_IS_DEBUGGER_DISASSEMBLY_VIEW (self));

  GTK_WIDGET_CLASS (ide_debugger_disassembly_view_parent_class)->root (widget);

  g_idle_add_full (G_PRIORITY_LOW,
                   scroll_to_insert_in_idle_cb,
                   g_object_ref (self),
                   g_object_unref);
}

static void
ide_debugger_disassembly_view_dispose (GObject *object)
{
  IdeDebuggerDisassemblyView *self = (IdeDebuggerDisassemblyView *)object;

  g_clear_pointer (&self->instructions, g_ptr_array_unref);

  G_OBJECT_CLASS (ide_debugger_disassembly_view_parent_class)->dispose (object);
}

static void
ide_debugger_disassembly_view_class_init (IdeDebuggerDisassemblyViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ide_debugger_disassembly_view_dispose;

  widget_class->root = ide_debugger_disassembly_view_root;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/debuggerui/ide-debugger-disassembly-view.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerDisassemblyView, source_buffer);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerDisassemblyView, source_view);
  gtk_widget_class_bind_template_callback (widget_class, notify_style_scheme_cb);

  g_resources_register (gbp_debuggerui_get_resource ());

  gdk_rgba_parse (&fallback_paragraph_bg, "#ffff0099");
}

static void
ide_debugger_disassembly_view_init (IdeDebuggerDisassemblyView *self)
{
  GtkSourceLanguageManager *langs;
  GtkSourceLanguage *lang;

  gtk_widget_init_template (GTK_WIDGET (self));

  langs = gtk_source_language_manager_get_default ();
  lang = gtk_source_language_manager_get_language (langs, "builder-disassembly");
  g_assert (lang != NULL);
  gtk_source_buffer_set_language (self->source_buffer, lang);

  g_object_bind_property_full (IDE_APPLICATION_DEFAULT, "style-scheme",
                               self->source_buffer, "style-scheme",
                               G_BINDING_SYNC_CREATE,
                               style_scheme_name_to_object,
                               NULL, NULL, NULL);
}

static void
apply_breakpoint_tag (IdeDebuggerDisassemblyView *self,
                      const GtkTextIter          *begin,
                      const GtkTextIter          *end)
{
  g_assert (IDE_IS_DEBUGGER_DISASSEMBLY_VIEW (self));
  g_assert (begin != NULL);
  g_assert (end != NULL);

  setup_breakpoint_tag (self);
  gtk_text_buffer_apply_tag (GTK_TEXT_BUFFER (self->source_buffer),
                             self->breakpoint,
                             begin, end);
}

void
ide_debugger_disassembly_view_set_current_address (IdeDebuggerDisassemblyView *self,
                                                   IdeDebuggerAddress          current_address)
{
  g_autofree char *key = NULL;
  GtkTextIter iter;
  GtkTextIter limit;
  GtkTextIter begin;
  GtkTextIter end;

  g_return_if_fail (IDE_IS_DEBUGGER_DISASSEMBLY_VIEW (self));

  self->current_address = current_address;

  gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (self->source_buffer), &iter, &limit);
  key = g_strdup_printf ("0x%"G_GINT64_MODIFIER"x", current_address);
  while (gtk_text_iter_forward_search (&iter, key, 0, &begin, &end, &limit))
    {
      if (gtk_text_iter_starts_line (&begin))
        {
          end = begin;
          gtk_text_iter_forward_line (&end);
          apply_breakpoint_tag (self, &begin, &end);
          gtk_text_buffer_select_range (GTK_TEXT_BUFFER (self->source_buffer), &begin, &begin);
          break;
        }

      iter = end;
    }
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
          g_autofree gchar *str = g_strdup_printf ("0x%"G_GINT64_MODIFIER"x <+%03"G_GINT64_MODIFIER"u>:\t%s\n",
                                                   ide_debugger_instruction_get_address (inst),
                                                   ide_debugger_instruction_get_address (inst) - first,
                                                   ide_debugger_instruction_get_display (inst));
          gtk_text_buffer_insert (GTK_TEXT_BUFFER (self->source_buffer), &iter, str, -1);
        }

      /* Trim the trailing \n */
      trim = iter;
      gtk_text_iter_backward_char (&iter);
      gtk_text_buffer_delete (GTK_TEXT_BUFFER (self->source_buffer), &iter, &trim);

      gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (self->source_buffer), &iter);
      gtk_text_buffer_select_range (GTK_TEXT_BUFFER (self->source_buffer), &iter, &iter);
    }
}
