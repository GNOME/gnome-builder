/* gb-source-auto-indenter-c.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "indent"

#include <glib/gi18n.h>

#include "gb-log.h"
#include "gb-source-auto-indenter-c.h"

struct _GbSourceAutoIndenterCPrivate
{
  guint brace_indent;
};

enum
{
  PROP_0,
  PROP_BRACE_INDENT,
  LAST_PROP
};

G_DEFINE_TYPE_WITH_PRIVATE (GbSourceAutoIndenterC, gb_source_auto_indenter_c,
                            GB_TYPE_SOURCE_AUTO_INDENTER)

static GParamSpec *gParamSpecs [LAST_PROP];

GbSourceAutoIndenter *
gb_source_auto_indenter_c_new (void)
{
  return g_object_new (GB_TYPE_SOURCE_AUTO_INDENTER_C, NULL);
}

static inline void
build_indent (GbSourceAutoIndenterC *c,
              guint                  line_offset,
              GtkTextIter           *matching_line,
              GString               *str)
{
  GtkTextIter iter;
  gunichar ch;
  guint i;

  gtk_text_iter_assign (&iter, matching_line);
  while (!gtk_text_iter_starts_line (&iter))
    if (!gtk_text_iter_backward_char (&iter))
      goto fallback;

  do {
    ch = gtk_text_iter_get_char (&iter);

    switch (ch) {
    case '\t':
    case ' ':
      g_string_append_unichar (str, ch);
      break;
    default:
      g_string_append_c (str, ' ');
      break;
    }
  } while (gtk_text_iter_forward_char (&iter) &&
           gtk_text_iter_get_line_offset (&iter) <= line_offset);

fallback:
  for (i = str->len; i <= line_offset; i++)
    g_string_append_c (str, ' ');
}

static gboolean
backward_find_matching_char (GtkTextIter *iter,
                             gunichar     ch)
{
  gunichar match;
  gunichar cur;
  guint count = 1;

  switch (ch) {
  case ')':
    match = '(';
    break;
  case '}':
    match = '{';
    break;
  case '[':
    match = ']';
    break;
  default:
    g_assert_not_reached ();
    break;
  }

  while (gtk_text_iter_backward_char (iter))
    {
      cur = gtk_text_iter_get_char (iter);

      if (cur == match)
        {
          if (--count == 0)
            return TRUE;
        }
      else if (cur == ch)
        count++;
    }

  return FALSE;
}

static gchar *
gb_source_auto_indenter_c_query (GbSourceAutoIndenter *indenter,
                                 GtkTextView          *view,
                                 GtkTextBuffer        *buffer,
                                 GtkTextIter          *iter)
{
  GbSourceAutoIndenterC *c = (GbSourceAutoIndenterC *)indenter;
  GtkTextIter cur;
  gunichar ch;
  GString *str;
  gchar *ret;

  ENTRY;

  g_return_val_if_fail (GB_IS_SOURCE_AUTO_INDENTER_C (c), NULL);

  gtk_text_iter_assign (&cur, iter);

  str = g_string_new (NULL);

  /*
   * Move back to the character before the \n that was inserted.
   *
   * TODO: This assumption may change.
   */
  if (!gtk_text_iter_backward_char (iter))
    GOTO (cleanup);

  /*
   * Get our last non \n character entered.
   */
  ch = gtk_text_iter_get_char (iter);

  /*
   * If we just placed a terminating parenthesis, we need to work our way back
   * to it's match. That way we can peak at what it was and determine
   * indentation from that.
   */
  if (ch == ')' || ch == ']' || ch == '}')
    {
      if (!backward_find_matching_char (iter, ch))
        GOTO (cleanup);
    }

  /*
   * We are probably in a a function call or parameter list.  Let's try to work
   * our way back to the opening parenthesis. This should work when the target
   * is for, parameter lists, or function arguments.
   */
  if (ch == ',')
    {
      if (!backward_find_matching_char (iter, ')'))
        GOTO (cleanup);

      build_indent (c, gtk_text_iter_get_line_offset (iter), iter, str);
      GOTO (cleanup);
    }

  /*
   * Looks like the last line was a statement or expression. Let's try to
   * find the beginning of it.
   */
  if (ch == ';')
    {
    }

cleanup:
  gtk_text_iter_assign (iter, &cur);

  ret = g_string_free (str, FALSE);

  RETURN (ret);
}

static void
gb_source_auto_indenter_c_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  GbSourceAutoIndenterC *c = GB_SOURCE_AUTO_INDENTER_C (object);

  switch (prop_id) {
  case PROP_BRACE_INDENT:
    g_value_set_uint (value, c->priv->brace_indent);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
gb_source_auto_indenter_c_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  GbSourceAutoIndenterC *c = GB_SOURCE_AUTO_INDENTER_C (object);

  switch (prop_id) {
  case PROP_BRACE_INDENT:
    c->priv->brace_indent = g_value_get_uint (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }

  g_object_notify_by_pspec (object, pspec);
}

static void
gb_source_auto_indenter_c_class_init (GbSourceAutoIndenterCClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GbSourceAutoIndenterClass *indenter_class = GB_SOURCE_AUTO_INDENTER_CLASS (klass);

  object_class->get_property = gb_source_auto_indenter_c_get_property;
  object_class->set_property = gb_source_auto_indenter_c_set_property;

  indenter_class->query = gb_source_auto_indenter_c_query;

  gParamSpecs [PROP_BRACE_INDENT] =
    g_param_spec_uint ("brace-indent",
                       _("Name"),
                       _("Name"),
                       0,
                       32,
                       2,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_BRACE_INDENT,
                                   gParamSpecs [PROP_BRACE_INDENT]);
}

static void
gb_source_auto_indenter_c_init (GbSourceAutoIndenterC *c)
{
  c->priv = gb_source_auto_indenter_c_get_instance_private (c);

  c->priv->brace_indent = 2;
}
