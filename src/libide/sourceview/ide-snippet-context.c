/* ide-snippet-context.c
 *
 * Copyright 2013-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-snippets-context"

#include "config.h"

#include <errno.h>
#include <glib/gi18n.h>
#include <stdlib.h>

#include "ide-snippet-context.h"

/**
 * SECTION:ide-snippet-context
 * @title: IdeSnippetContext
 * @short_description: Context for expanding #IdeSnippetChunk
 *
 * This class is currently used primary as a hashtable. However, the longer
 * term goal is to have it hold onto a GjsContext as well as other languages
 * so that #IdeSnippetChunk can expand themselves by executing
 * script within the context.
 *
 * The #IdeSnippet will build the context and then expand each of the
 * chunks during the insertion/edit phase.
 *
 * Since: 3.32
 */

struct _IdeSnippetContext
{
  GObject     parent_instance;

  GHashTable *shared;
  GHashTable *variables;
  gchar      *line_prefix;
  gint        tab_width;
  guint       use_spaces : 1;
};

struct _IdeSnippetContextClass
{
  GObjectClass parent;
};

G_DEFINE_TYPE (IdeSnippetContext, ide_snippet_context, G_TYPE_OBJECT)

enum {
  CHANGED,
  LAST_SIGNAL
};

typedef gchar *(*InputFilter) (const gchar *input);

static GHashTable *filters;
static guint signals[LAST_SIGNAL];

IdeSnippetContext *
ide_snippet_context_new (void)
{
  return g_object_new (IDE_TYPE_SNIPPET_CONTEXT, NULL);
}

void
ide_snippet_context_dump (IdeSnippetContext *context)
{
  GHashTableIter iter;
  gpointer key;
  gpointer value;

  g_return_if_fail (IDE_IS_SNIPPET_CONTEXT (context));

  g_hash_table_iter_init (&iter, context->variables);
  while (g_hash_table_iter_next (&iter, &key, &value))
    g_print (" %s=%s\n", (gchar *) key, (gchar *) value);
}

void
ide_snippet_context_clear_variables (IdeSnippetContext *context)
{
  g_return_if_fail (IDE_IS_SNIPPET_CONTEXT (context));

  g_hash_table_remove_all (context->variables);
}

void
ide_snippet_context_add_variable (IdeSnippetContext *context,
                                  const gchar       *key,
                                  const gchar       *value)
{
  g_return_if_fail (IDE_IS_SNIPPET_CONTEXT (context));
  g_return_if_fail (key);

  g_hash_table_replace (context->variables, g_strdup (key), g_strdup (value));
}

void
ide_snippet_context_add_shared_variable (IdeSnippetContext *context,
                                         const gchar       *key,
                                         const gchar       *value)
{
  g_return_if_fail (IDE_IS_SNIPPET_CONTEXT (context));
  g_return_if_fail (key);

  g_hash_table_replace (context->shared, g_strdup (key), g_strdup (value));
}

const gchar *
ide_snippet_context_get_variable (IdeSnippetContext *context,
                                  const gchar       *key)
{
  const gchar *ret;

  g_return_val_if_fail (IDE_IS_SNIPPET_CONTEXT (context), NULL);

  if (!(ret = g_hash_table_lookup (context->variables, key)))
    ret = g_hash_table_lookup (context->shared, key);

  return ret;
}

static gchar *
filter_lower (const gchar *input)
{
  return g_utf8_strdown (input, -1);
}

static gchar *
filter_upper (const gchar *input)
{
  return g_utf8_strup (input, -1);
}

static gchar *
filter_capitalize (const gchar *input)
{
  gunichar c;
  GString *str;

  if (!*input)
    return g_strdup ("");

  c = g_utf8_get_char (input);

  if (g_unichar_isupper (c))
    return g_strdup (input);

  str = g_string_new (NULL);
  input = g_utf8_next_char (input);
  g_string_append_unichar (str, g_unichar_toupper (c));
  if (*input)
    g_string_append (str, input);

  return g_string_free (str, FALSE);
}

static gchar *
filter_decapitalize (const gchar *input)
{
  gunichar c;
  GString *str;

  c = g_utf8_get_char (input);

  if (g_unichar_islower (c))
    return g_strdup (input);

  str = g_string_new (NULL);
  input = g_utf8_next_char (input);
  g_string_append_unichar (str, g_unichar_tolower (c));
  g_string_append (str, input);

  return g_string_free (str, FALSE);
}

static gchar *
filter_html (const gchar *input)
{
  gunichar c;
  GString *str;

  str = g_string_new (NULL);

  for (; *input; input = g_utf8_next_char (input))
    {
      c = g_utf8_get_char (input);
      switch (c)
        {
        case '<':
          g_string_append_len (str, "&lt;", 4);
          break;

        case '>':
          g_string_append_len (str, "&gt;", 4);
          break;

        default:
          g_string_append_unichar (str, c);
          break;
        }
    }

  return g_string_free (str, FALSE);
}

static gchar *
filter_camelize (const gchar *input)
{
  gboolean next_is_upper = TRUE;
  gboolean skip = FALSE;
  gunichar c;
  GString *str;

  if (!strchr (input, '_') && !strchr (input, ' ') && !strchr (input, '-'))
    return filter_capitalize (input);

  str = g_string_new (NULL);

  for (; *input; input = g_utf8_next_char (input))
    {
      c = g_utf8_get_char (input);

      switch (c)
        {
        case '_':
        case '-':
        case ' ':
          next_is_upper = TRUE;
          skip = TRUE;
          break;

        default:
          break;
        }

      if (skip)
        {
          skip = FALSE;
          continue;
        }

      if (next_is_upper)
        {
          c = g_unichar_toupper (c);
          next_is_upper = FALSE;
        }
      else
        c = g_unichar_tolower (c);

      g_string_append_unichar (str, c);
    }

  if (g_str_has_suffix (str->str, "Private"))
    g_string_truncate (str, str->len - strlen ("Private"));

  return g_string_free (str, FALSE);
}

static gchar *
filter_functify (const gchar *input)
{
  gunichar last = 0;
  gunichar c;
  gunichar n;
  GString *str;

  str = g_string_new (NULL);

  for (; *input; input = g_utf8_next_char (input))
    {
      c = g_utf8_get_char (input);
      n = g_utf8_get_char (g_utf8_next_char (input));

      if (last)
        {
          if ((g_unichar_islower (last) && g_unichar_isupper (c)) ||
              (g_unichar_isupper (c) && g_unichar_islower (n)))
            g_string_append_c (str, '_');
        }

      if ((c == ' ') || (c == '-'))
        c = '_';

      g_string_append_unichar (str, g_unichar_tolower (c));

      last = c;
    }

  if (g_str_has_suffix (str->str, "_private") ||
      g_str_has_suffix (str->str, "_PRIVATE"))
    g_string_truncate (str, str->len - strlen ("_private"));

  return g_string_free (str, FALSE);
}

static gchar *
filter_namespace (const gchar *input)
{
  gunichar last = 0;
  gunichar c;
  gunichar n;
  GString *str;
  gboolean first_is_lower = FALSE;

  str = g_string_new (NULL);

  for (; *input; input = g_utf8_next_char (input))
    {
      c = g_utf8_get_char (input);
      n = g_utf8_get_char (g_utf8_next_char (input));

      if (c == '_')
        break;

      if (last)
        {
          if ((g_unichar_islower (last) && g_unichar_isupper (c)) ||
              (g_unichar_isupper (c) && g_unichar_islower (n)))
            break;
        }
      else
        first_is_lower = g_unichar_islower (c);

      if ((c == ' ') || (c == '-'))
        break;

      g_string_append_unichar (str, c);

      last = c;
    }

  if (first_is_lower)
    {
      gchar *ret;

      ret = filter_capitalize (str->str);
      g_string_free (str, TRUE);
      return ret;
    }

  return g_string_free (str, FALSE);
}

static gchar *
filter_class (const gchar *input)
{
  gchar *camel;
  gchar *ns;
  gchar *ret = NULL;

  camel = filter_camelize (input);
  ns = filter_namespace (input);

  if (g_str_has_prefix (camel, ns))
    ret = g_strdup (camel + strlen (ns));
  else
    {
      ret = camel;
      camel = NULL;
    }

  g_free (camel);
  g_free (ns);

  return ret;
}

static gchar *
filter_instance (const gchar *input)
{
  const gchar *tmp;
  gchar *funct = NULL;
  gchar *ret;

  if (!strchr (input, '_'))
    {
      funct = filter_functify (input);
      input = funct;
    }

  if ((tmp = strrchr (input, '_')))
    ret = g_strdup (tmp+1);
  else
    ret = g_strdup (input);

  g_free (funct);

  return ret;
}

static gchar *
filter_space (const gchar *input)
{
  GString *str;

  str = g_string_new (NULL);
  for (; *input; input = g_utf8_next_char (input))
    g_string_append_c (str, ' ');

  return g_string_free (str, FALSE);
}

static gchar *
filter_descend_path (const gchar *input)
{
  const gchar *pos;

  if (input == NULL)
    return NULL;

  while (*input == G_DIR_SEPARATOR)
    input++;

  if ((pos = strchr (input, G_DIR_SEPARATOR)))
    return g_strdup (pos + 1);

   return NULL;
}

static gchar *
filter_stripsuffix (const gchar *input)
{
  const gchar *endpos;

  g_return_val_if_fail (input, NULL);

  endpos = strrchr (input, '.');
  if (endpos)
    return g_strndup (input, (endpos - input));

  return g_strdup (input);
}

static gchar *
filter_slash_to_dots (const gchar *input)
{
  GString *str;
  gunichar ch;

  if (input == NULL)
    return NULL;

  str = g_string_new (NULL);

  for (; *input; input = g_utf8_next_char (input))
    {
      ch = g_utf8_get_char (input);

      if (ch == G_DIR_SEPARATOR)
        g_string_append_c (str, '.');
      else
        g_string_append_unichar (str, ch);
    }

  return g_string_free (str, FALSE);
}

static gchar *
apply_filter (gchar       *input,
              const gchar *filter)
{
  InputFilter filter_func;
  gchar *tmp;

  if ((filter_func = g_hash_table_lookup (filters, filter)))
    {
      tmp = input;
      input = filter_func (input);
      g_free (tmp);
    }

  return input;
}

static gchar *
apply_filters (GString     *str,
               const gchar *filters_list)
{
  gchar **filter_names;
  gchar *input = g_string_free (str, FALSE);
  gint i;

  filter_names = g_strsplit (filters_list, "|", 0);

  for (i = 0; filter_names[i]; i++)
    input = apply_filter (input, filter_names[i]);

  g_strfreev (filter_names);

  return input;
}

static gchar *
scan_forward (const gchar  *input,
              const gchar **endpos,
              gunichar      needle)
{
  const gchar *begin = input;

  for (; *input; input = g_utf8_next_char (input))
    {
      gunichar c = g_utf8_get_char (input);

      if (c == needle)
        {
          *endpos = input;
          return g_strndup (begin, (input - begin));
        }
    }

  *endpos = NULL;

  return NULL;
}

gchar *
ide_snippet_context_expand (IdeSnippetContext *context,
                            const gchar       *input)
{
  const gchar *expand;
  gunichar c;
  gboolean is_dynamic;
  GString *str;
  gchar key[12];
  glong n;
  gint i;

  g_return_val_if_fail (IDE_IS_SNIPPET_CONTEXT (context), NULL);
  g_return_val_if_fail (input, NULL);

  is_dynamic = (*input == '$');

  str = g_string_new (NULL);

  for (; *input; input = g_utf8_next_char (input))
    {
      c = g_utf8_get_char (input);
      if (c == '\\')
        {
          input = g_utf8_next_char (input);
          if (!*input)
            break;
          c = g_utf8_get_char (input);
        }
      else if (is_dynamic && c == '$')
        {
          input = g_utf8_next_char (input);
          if (!*input)
            break;
          c = g_utf8_get_char (input);
          if (g_unichar_isdigit (c))
            {
              errno = 0;
              n = strtol (input, (gchar * *) &input, 10);
              if (((n == LONG_MIN) || (n == LONG_MAX)) && errno == ERANGE)
                break;
              input--;
              g_snprintf (key, sizeof key, "%ld", n);
              key[sizeof key - 1] = '\0';
              expand = ide_snippet_context_get_variable (context, key);
              if (expand)
                g_string_append (str, expand);
              continue;
            }
          else
            {
              if (strchr (input, '|'))
                {
                  g_autofree gchar *lkey = NULL;

                  lkey = g_strndup (input, strchr (input, '|') - input);
                  expand = ide_snippet_context_get_variable (context, lkey);
                  if (expand)
                    {
                      g_string_append (str, expand);
                      input = strchr (input, '|') - 1;
                    }
                  else
                    input += strlen (input) - 1;
                }
              else
                {
                  expand = ide_snippet_context_get_variable (context, input);
                  if (expand)
                    g_string_append (str, expand);
                  else
                    {
                      g_string_append_c (str, '$');
                      g_string_append (str, input);
                    }
                  input += strlen (input) - 1;
                }
              continue;
            }
        }
      else if (is_dynamic && c == '|')
        return apply_filters (str, input + 1);
      else if (c == '`')
        {
          const gchar *endpos = NULL;
          gchar *slice;

          slice = scan_forward (input + 1, &endpos, '`');

          if (slice)
            {
              gchar *expanded;

              input = endpos;

              expanded = ide_snippet_context_expand (context, slice);

              g_string_append (str, expanded);

              g_free (expanded);
              g_free (slice);

              continue;
            }
        }
      else if (c == '\t')
        {
          if (context->use_spaces)
            for (i = 0; i < context->tab_width; i++)
              g_string_append_c (str, ' ');

          else
            g_string_append_c (str, '\t');
          continue;
        }
      else if (c == '\n')
        {
          g_string_append_c (str, '\n');
          if (context->line_prefix)
            g_string_append (str, context->line_prefix);
          continue;
        }
      g_string_append_unichar (str, c);
    }

  return g_string_free (str, FALSE);
}

void
ide_snippet_context_set_tab_width (IdeSnippetContext *context,
                                   gint               tab_width)
{
  g_return_if_fail (IDE_IS_SNIPPET_CONTEXT (context));
  context->tab_width = tab_width;
}

void
ide_snippet_context_set_use_spaces (IdeSnippetContext *context,
                                    gboolean           use_spaces)
{
  g_return_if_fail (IDE_IS_SNIPPET_CONTEXT (context));
  context->use_spaces = !!use_spaces;
}

void
ide_snippet_context_set_line_prefix (IdeSnippetContext *context,
                                     const gchar       *line_prefix)
{
  g_return_if_fail (IDE_IS_SNIPPET_CONTEXT (context));
  g_free (context->line_prefix);
  context->line_prefix = g_strdup (line_prefix);
}

void
ide_snippet_context_emit_changed (IdeSnippetContext *context)
{
  g_return_if_fail (IDE_IS_SNIPPET_CONTEXT (context));
  g_signal_emit (context, signals[CHANGED], 0);
}

static void
ide_snippet_context_finalize (GObject *object)
{
  IdeSnippetContext *context = (IdeSnippetContext *)object;

  g_clear_pointer (&context->shared, g_hash_table_unref);
  g_clear_pointer (&context->variables, g_hash_table_unref);
  g_clear_pointer (&context->line_prefix, g_free);

  G_OBJECT_CLASS (ide_snippet_context_parent_class)->finalize (object);
}

static void
ide_snippet_context_class_init (IdeSnippetContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_snippet_context_finalize;

  signals[CHANGED] = g_signal_new ("changed",
                                    IDE_TYPE_SNIPPET_CONTEXT,
                                    G_SIGNAL_RUN_FIRST,
                                    0,
                                    NULL, NULL, NULL,
                                    G_TYPE_NONE,
                                    0);

  filters = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (filters, (gpointer) "lower", filter_lower);
  g_hash_table_insert (filters, (gpointer) "upper", filter_upper);
  g_hash_table_insert (filters, (gpointer) "capitalize", filter_capitalize);
  g_hash_table_insert (filters, (gpointer) "decapitalize", filter_decapitalize);
  g_hash_table_insert (filters, (gpointer) "html", filter_html);
  g_hash_table_insert (filters, (gpointer) "camelize", filter_camelize);
  g_hash_table_insert (filters, (gpointer) "functify", filter_functify);
  g_hash_table_insert (filters, (gpointer) "namespace", filter_namespace);
  g_hash_table_insert (filters, (gpointer) "class", filter_class);
  g_hash_table_insert (filters, (gpointer) "space", filter_space);
  g_hash_table_insert (filters, (gpointer) "stripsuffix", filter_stripsuffix);
  g_hash_table_insert (filters, (gpointer) "instance", filter_instance);
  g_hash_table_insert (filters, (gpointer) "slash_to_dots", filter_slash_to_dots);
  g_hash_table_insert (filters, (gpointer) "descend_path", filter_descend_path);
}

static void
ide_snippet_context_init (IdeSnippetContext *context)
{
  GDateTime *dt;
  gchar *str;

  context->variables = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  context->shared = g_hash_table_new_full (g_str_hash,
                                                 g_str_equal,
                                                 g_free,
                                                 g_free);

#define ADD_VARIABLE(k, v) \
  g_hash_table_insert (context->shared, g_strdup (k), g_strdup (v))

  ADD_VARIABLE ("username", g_get_user_name ());
  ADD_VARIABLE ("fullname", g_get_real_name ());
  ADD_VARIABLE ("author", g_get_real_name ());

  dt = g_date_time_new_now_local ();
  str = g_date_time_format (dt, "%Y");
  ADD_VARIABLE ("year", str);
  g_free (str);
  str = g_date_time_format (dt, "%b");
  ADD_VARIABLE ("shortmonth", str);
  g_free (str);
  str = g_date_time_format (dt, "%d");
  ADD_VARIABLE ("day", str);
  g_free (str);
  str = g_date_time_format (dt, "%a");
  ADD_VARIABLE ("shortweekday", str);
  g_free (str);
  g_date_time_unref (dt);

  ADD_VARIABLE ("email", "unknown@domain.org");

#undef ADD_VARIABLE
}
