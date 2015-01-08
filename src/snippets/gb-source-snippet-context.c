/* gb-source-snippet-context.c
 *
 * Copyright (C) 2013 Christian Hergert <christian@hergert.me>
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

#include <errno.h>
#include <glib/gi18n.h>
#include <stdlib.h>

#include "gb-source-snippet-context.h"

/**
 * SECTION:gb-source-snippet-context:
 * @title: GbSourceSnippetContext
 * @short_description: Context for expanding #GbSourceSnippetChunk<!-- -->'s.
 *
 * This class is currently used primary as a hashtable. However, the longer
 * term goal is to have it hold onto a GjsContext as well as other languages
 * so that #GbSourceSnippetChunk<!-- -->'s can expand themselves by executing
 * script within the context.
 *
 * The #GbSourceSnippet will build the context and then expand each of the
 * chunks during the insertion/edit phase.
 */

G_DEFINE_TYPE (GbSourceSnippetContext, gb_source_snippet_context, G_TYPE_OBJECT)

struct _GbSourceSnippetContextPrivate
{
  GHashTable *shared;
  GHashTable *variables;
  gchar      *line_prefix;
  gint        tab_width;
  gboolean    use_spaces;
};

enum {
  CHANGED,
  LAST_SIGNAL
};

typedef gchar *(*InputFilter) (const gchar *input);

static GHashTable *gFilters;
static guint gSignals[LAST_SIGNAL];

GbSourceSnippetContext *
gb_source_snippet_context_new (void)
{
  return g_object_new (GB_TYPE_SOURCE_SNIPPET_CONTEXT, NULL);
}

void
gb_source_snippet_context_dump (GbSourceSnippetContext *context)
{
  GbSourceSnippetContextPrivate *priv;
  GHashTableIter iter;
  gpointer key;
  gpointer value;

  g_return_if_fail (GB_IS_SOURCE_SNIPPET_CONTEXT (context));

  priv = context->priv;

  g_hash_table_iter_init (&iter, priv->variables);
  while (g_hash_table_iter_next (&iter, &key, &value))
    g_print (" %s=%s\n", (gchar *) key, (gchar *) value);
}

void
gb_source_snippet_context_clear_variables (GbSourceSnippetContext *context)
{
  g_return_if_fail (GB_IS_SOURCE_SNIPPET_CONTEXT (context));

  g_hash_table_remove_all (context->priv->variables);
}

void
gb_source_snippet_context_add_variable (GbSourceSnippetContext *context,
                                        const gchar            *key,
                                        const gchar            *value)
{
  g_return_if_fail (GB_IS_SOURCE_SNIPPET_CONTEXT (context));
  g_return_if_fail (key);

  g_hash_table_replace (context->priv->variables,
                        g_strdup (key),
                        g_strdup (value));
}

const gchar *
gb_source_snippet_context_get_variable (GbSourceSnippetContext *context,
                                        const gchar            *key)
{
  const gchar *ret;

  g_return_val_if_fail (GB_IS_SOURCE_SNIPPET_CONTEXT (context), NULL);

  if (!(ret = g_hash_table_lookup (context->priv->variables, key)))
    ret = g_hash_table_lookup (context->priv->shared, key);

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

  c = g_utf8_get_char (input);

  if (g_unichar_isupper (c))
    return g_strdup (input);

  str = g_string_new (NULL);
  input = g_utf8_next_char (input);
  g_string_append_unichar (str, g_unichar_toupper (c));
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
apply_filter (gchar       *input,
              const gchar *filter)
{
  InputFilter filter_func;
  gchar *tmp;

  filter_func = g_hash_table_lookup (gFilters, filter);
  if (filter_func)
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
gb_source_snippet_context_expand (GbSourceSnippetContext *context,
                                  const gchar            *input)
{
  GbSourceSnippetContextPrivate *priv;
  const gchar *expand;
  gunichar c;
  gboolean is_dynamic;
  GString *str;
  gchar key[12];
  glong n;
  gint i;

  g_return_val_if_fail (GB_IS_SOURCE_SNIPPET_CONTEXT (context), NULL);
  g_return_val_if_fail (input, NULL);

  priv = context->priv;

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
              expand = gb_source_snippet_context_get_variable (context, key);
              if (expand)
                g_string_append (str, expand);
              continue;
            }
          else
            {
              if (strchr (input, '|'))
                {
                  gchar *lkey;

                  lkey = g_strndup (input, strchr (input, '|') - input);
                  expand = gb_source_snippet_context_get_variable (context, lkey);
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
                  expand = gb_source_snippet_context_get_variable (context, input);
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

              expanded = gb_source_snippet_context_expand (context, slice);

              g_string_append (str, expanded);

              g_free (expanded);
              g_free (slice);

              continue;
            }
        }
      else if (c == '\t')
        {
          if (priv->use_spaces)
            for (i = 0; i < priv->tab_width; i++)
              g_string_append_c (str, ' ');

          else
            g_string_append_c (str, '\t');
          continue;
        }
      else if (c == '\n')
        {
          g_string_append_c (str, '\n');
          if (priv->line_prefix)
            g_string_append (str, priv->line_prefix);
          continue;
        }
      g_string_append_unichar (str, c);
    }

  return g_string_free (str, FALSE);
}

void
gb_source_snippet_context_set_tab_width (GbSourceSnippetContext *context,
                                         gint                    tab_width)
{
  g_return_if_fail (GB_IS_SOURCE_SNIPPET_CONTEXT (context));
  context->priv->tab_width = tab_width;
}

void
gb_source_snippet_context_set_use_spaces (GbSourceSnippetContext *context,
                                          gboolean                use_spaces)
{
  g_return_if_fail (GB_IS_SOURCE_SNIPPET_CONTEXT (context));
  context->priv->use_spaces = use_spaces;
}

void
gb_source_snippet_context_set_line_prefix (GbSourceSnippetContext *context,
                                           const gchar            *line_prefix)
{
  g_return_if_fail (GB_IS_SOURCE_SNIPPET_CONTEXT (context));
  g_free (context->priv->line_prefix);
  context->priv->line_prefix = g_strdup (line_prefix);
}

void
gb_source_snippet_context_emit_changed (GbSourceSnippetContext *context)
{
  g_return_if_fail (GB_IS_SOURCE_SNIPPET_CONTEXT (context));
  g_signal_emit (context, gSignals[CHANGED], 0);
}

static gchar *
run_command (const gchar *command)
{
  GError *error = NULL;
  gchar *output = NULL;
  gchar **argv = NULL;
  gint argc = 0;

  if (!g_shell_parse_argv (command, &argc, &argv, NULL))
    return NULL;

  /*
   * TODO: Run in project directory.
   */

  if (!g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, &output, NULL, NULL, &error))
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
    }

  g_strfreev (argv);

  return g_strstrip (output);
}

static void
gb_source_snippet_context_finalize (GObject *object)
{
  GbSourceSnippetContextPrivate *priv;

  priv = GB_SOURCE_SNIPPET_CONTEXT (object)->priv;

  g_clear_pointer (&priv->shared, (GDestroyNotify) g_hash_table_unref);
  g_clear_pointer (&priv->variables, (GDestroyNotify) g_hash_table_unref);

  G_OBJECT_CLASS (gb_source_snippet_context_parent_class)->finalize (object);
}

static void
gb_source_snippet_context_class_init (GbSourceSnippetContextClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = gb_source_snippet_context_finalize;
  g_type_class_add_private (object_class, sizeof (GbSourceSnippetContextPrivate));

  gSignals[CHANGED] = g_signal_new ("changed",
                                    GB_TYPE_SOURCE_SNIPPET_CONTEXT,
                                    G_SIGNAL_RUN_FIRST,
                                    0,
                                    NULL,
                                    NULL,
                                    g_cclosure_marshal_VOID__VOID,
                                    G_TYPE_NONE,
                                    0);

  gFilters = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (gFilters, (gpointer) "lower", filter_lower);
  g_hash_table_insert (gFilters, (gpointer) "upper", filter_upper);
  g_hash_table_insert (gFilters, (gpointer) "capitalize", filter_capitalize);
  g_hash_table_insert (gFilters, (gpointer) "html", filter_html);
  g_hash_table_insert (gFilters, (gpointer) "camelize", filter_camelize);
  g_hash_table_insert (gFilters, (gpointer) "functify", filter_functify);
  g_hash_table_insert (gFilters, (gpointer) "namespace", filter_namespace);
  g_hash_table_insert (gFilters, (gpointer) "class", filter_class);
  g_hash_table_insert (gFilters, (gpointer) "space", filter_space);
  g_hash_table_insert (gFilters, (gpointer) "stripsuffix", filter_stripsuffix);
  g_hash_table_insert (gFilters, (gpointer) "instance", filter_instance);
}

static void
gb_source_snippet_context_init (GbSourceSnippetContext *context)
{
  GDateTime *dt;
  gchar *str;

  context->priv =
    G_TYPE_INSTANCE_GET_PRIVATE (context,
                                 GB_TYPE_SOURCE_SNIPPET_CONTEXT,
                                 GbSourceSnippetContextPrivate);

  context->priv->variables = g_hash_table_new_full (g_str_hash,
                                                    g_str_equal,
                                                    g_free,
                                                    g_free);

  context->priv->shared = g_hash_table_new_full (g_str_hash,
                                                 g_str_equal,
                                                 g_free,
                                                 g_free);

#define ADD_VARIABLE(k, v) \
  g_hash_table_insert (context->priv->shared, g_strdup (k), g_strdup (v))

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

  str = run_command ("git config user.email");
  ADD_VARIABLE ("email", str);
  g_free (str);

#undef ADD_VARIABLE
}
