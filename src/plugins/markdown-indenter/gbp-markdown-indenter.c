/*
 * gbp-markdown-indenter.c
 *
 * Copyright 2023 JCWasmx86 <JCWasmx86@t-online.de>
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

#include "config.h"

#include <libide-core.h>

#include "gbp-markdown-indenter.h"

struct _GbpMarkdownIndenter
{
  GObject parent_instance;
};

static gboolean
gbp_markdown_indenter_is_trigger (GtkSourceIndenter *self,
                                  GtkSourceView     *view,
                                  const GtkTextIter *location,
                                  GdkModifierType    state,
                                  guint              keyval)
{
  if ((state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_SUPER_MASK)) != 0)
    return FALSE;

  return (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter);
}

static void
gbp_markdown_indenter_indent (GtkSourceIndenter *self,
                              GtkSourceView     *view,
                              GtkTextIter       *location)
{
  static const char * const prefixes[] = {"- ", "* ", "+ ", NULL};
  GtkTextBuffer *buffer = NULL;
  GtkTextIter prev_iter = {0};
  g_autofree char *line = NULL;
  g_autofree char *indent = NULL;
  g_autofree char *line_copy = NULL;
  gsize old_len = 0;
  gsize new_len = 0;
  int prev_line_no = 0;
  int i = 0;

  IDE_ENTRY;

  g_assert (GBP_IS_MARKDOWN_INDENTER (self));
  g_assert (GTK_SOURCE_IS_VIEW (view));
  g_assert (location != NULL);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
  prev_line_no = gtk_text_iter_get_line (location);
  gtk_text_buffer_get_iter_at_line (buffer, &prev_iter, prev_line_no - 1);
  line = gtk_text_iter_get_text (&prev_iter, location);
  old_len = strlen (line);
  line_copy = g_strdup (line);
  line = g_strchug (line);
  new_len = strlen (line);

  g_assert (new_len <= old_len);

  indent = g_utf8_substring (line_copy, 0, old_len - new_len);

  /* Continue checklist */
  if (g_str_has_prefix (line, "- [ ] ") || g_str_has_prefix (line, "- [x] "))
    {
      gtk_text_buffer_insert (buffer, location, indent, -1);
      gtk_text_buffer_insert (buffer, location, "- [ ] ", -1);
      IDE_EXIT;
    }

  /* Continue unordered list */
  for (guint p = 0; prefixes[p]; p++)
    {
      const char *prefix = prefixes[p];

      if (g_str_has_prefix (line, prefix))
        {
          gtk_text_buffer_insert (buffer, location, indent, -1);
          gtk_text_buffer_insert (buffer, location, prefix, -1);
          IDE_EXIT;
        }
    }

  /* Continue ordered list */
  for (const char *iter = line; *iter; iter = g_utf8_next_char (iter))
    {
      gunichar ch = g_utf8_get_char (iter);

      /* Found a number */
      if (ch >= '0' && ch <= '9')
        {
          i++;
          continue;
        }

      /* We are at the end of a number. */
      if (ch == '.' && i != 0)
        {
          g_autofree char *str = NULL;
          gint64 parsed = 0;
          char *endptr = NULL;

          errno = 0;
          endptr = &line[i + 1];
          parsed = g_ascii_strtoll (line, &endptr, 10);

          if ((parsed == G_MAXINT64 || parsed == G_MININT64) && errno == ERANGE)
            break;

          gtk_text_buffer_insert (buffer, location, indent, -1);
          str = g_strdup_printf ("%"G_GINT64_MODIFIER"u", parsed+1);
          gtk_text_buffer_insert (buffer, location, str, -1);
          gtk_text_buffer_insert (buffer, location, ". ", -1);

          IDE_EXIT;
        }

      /* Found something other than "number." */
      break;
    }

  IDE_EXIT;
}

static void
indenter_interface_init (GtkSourceIndenterInterface *iface)
{
  iface->is_trigger = gbp_markdown_indenter_is_trigger;
  iface->indent = gbp_markdown_indenter_indent;
}

G_DEFINE_TYPE_WITH_CODE (GbpMarkdownIndenter, gbp_markdown_indenter, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_INDENTER, indenter_interface_init))

static void
gbp_markdown_indenter_init (GbpMarkdownIndenter *self)
{
}

static void
gbp_markdown_indenter_class_init (GbpMarkdownIndenterClass *klass)
{
}
