/* gb-html-completion-provider.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#include <string.h>

#include "gb-html-completion-provider.h"
#include "trie.h"

static Trie *attributes;
static Trie *css_styles;
static Trie *elements;

enum {
  MODE_NONE,
  MODE_ELEMENT_START,
  MODE_ELEMENT_END,
  MODE_ATTRIBUTE_NAME,
  MODE_ATTRIBUTE_VALUE,
  MODE_CSS,
};

typedef struct
{
  GList *results;
} SearchState;

static void completion_provider_init (GtkSourceCompletionProviderIface *);

G_DEFINE_TYPE_EXTENDED (GbHtmlCompletionProvider,
                        gb_html_completion_provider,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROVIDER,
                                               completion_provider_init))

GtkSourceCompletionProvider *
gb_html_completion_provider_new (void)
{
  return g_object_new (GB_TYPE_HTML_COMPLETION_PROVIDER, NULL);
}

static gchar *
get_word (GtkSourceCompletionContext *context)
{
  GtkTextIter word_start;
  GtkTextIter iter;
  gchar *word = NULL;

  g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (context), NULL);

  if (gtk_source_completion_context_get_iter (context, &iter))
    {
      word_start = iter;

      do {
        gunichar ch;

        if (!gtk_text_iter_backward_char (&word_start))
          break;

        ch = gtk_text_iter_get_char (&word_start);

        if (g_unichar_isalnum (ch) || ch == '_')
          continue;

        gtk_text_iter_forward_char (&word_start);
        break;

      } while (TRUE);

      word = gtk_text_iter_get_slice (&word_start, &iter);
    }

  return word;
}

static gboolean
in_element (const GtkTextIter *iter)
{
  GtkTextIter copy = *iter;

  /*
   * this is a stupidly simple algorithm. walk backwards, until we reach either
   * <, >, or start of buffer.
   */

  while (gtk_text_iter_backward_char (&copy))
    {
      gunichar ch;

      ch = gtk_text_iter_get_char (&copy);

      if (ch == '/')
        {
          gtk_text_iter_backward_char (&copy);
          ch = gtk_text_iter_get_char (&copy);
          if (ch == '<')
            return MODE_ELEMENT_END;
        }

      if (ch == '>')
        return FALSE;
      else if (ch == '<')
        return TRUE;
    }

  return FALSE;
}

static gboolean
in_attribute_value (const GtkTextIter *iter,
                    gunichar           looking_for)
{
  GtkTextIter copy = *iter;

  if (!gtk_text_iter_backward_char (&copy))
    return FALSE;

  do
    {
      gunichar ch;

      if (gtk_text_iter_ends_line (&copy))
        return FALSE;

      ch = gtk_text_iter_get_char (&copy);

      if (ch == looking_for)
        {
          gtk_text_iter_backward_char (&copy);
          return (gtk_text_iter_get_char (&copy) == '=');
        }
    }
  while (gtk_text_iter_backward_char (&copy));

  return FALSE;
}

static gboolean
in_attribute_named (const GtkTextIter *iter,
                    const gchar       *name)
{
  GtkTextIter copy = *iter;
  GtkTextIter match_start;
  GtkTextIter match_end;
  gboolean ret = FALSE;

  if (gtk_text_iter_backward_search (&copy, "='",
                                     GTK_TEXT_SEARCH_TEXT_ONLY,
                                     &match_start, &match_end,
                                     NULL) ||
      gtk_text_iter_backward_search (&copy, "=\"",
                                     GTK_TEXT_SEARCH_TEXT_ONLY,
                                     &match_start, &match_end,
                                     NULL))
    {
      GtkTextIter word_begin = match_start;
      gchar *word;

      gtk_text_iter_backward_chars (&word_begin, strlen (name));
      word = gtk_text_iter_get_slice (&word_begin, &match_start);
      ret = (g_strcmp0 (word, name) == 0);
      g_free (word);
    }

  return ret;
}

static gint
get_mode (GtkSourceCompletionContext *context)
{
  GtkTextIter iter;
  GtkTextIter back;

  g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (context), -1);

  gtk_source_completion_context_get_iter (context, &iter);

  /*
   * Ignore the = after attribute name.
   */
  back = iter;
  gtk_text_iter_backward_char (&back);
  if (gtk_text_iter_get_char (&back) == '=')
    return MODE_NONE;

  /*
   * Check for various state inside of element start (<).
   */
  if (in_element (&iter))
    {
      GtkTextIter copy = iter;
      gunichar ch;

      /*
       * If there are no spaces until we reach <, then we are in element name.
       */
      while (gtk_text_iter_backward_char (&copy))
        {
          ch = gtk_text_iter_get_char (&copy);

          if (ch == '/')
            {
              GtkTextIter copy2 = copy;

              gtk_text_iter_backward_char (&copy2);
              if (gtk_text_iter_get_char (&copy2) == '<')
                return MODE_ELEMENT_END;
            }

          if (ch == '<')
            return MODE_ELEMENT_START;

          if (g_unichar_isalnum (ch))
            continue;

          break;
        }

      /*
       * Now check to see if we are in an attribute value.
       */
      if (in_attribute_value (&iter, '"') || in_attribute_value (&iter, '\''))
        {
          /*
           * If the attribute name is style, then we are in CSS.
           */
          if (in_attribute_named (&iter, "style"))
            return MODE_CSS;

          return MODE_ATTRIBUTE_VALUE;
        }

      /*
       * Not in attribute value, but in element (and not the name). Must be
       * attribute name. But only say so if we have moved past ' or ".
       */
      ch = gtk_text_iter_get_char (&back);
      if (ch != '\'' && ch != '"')
        return MODE_ATTRIBUTE_NAME;
    }

  return MODE_NONE;
}

static gboolean
traverse_cb (Trie        *trie,
             const gchar *key,
             gpointer     value,
             gpointer     user_data)
{
  SearchState *state = user_data;
  GtkSourceCompletionItem *item;

  g_return_val_if_fail (trie, FALSE);
  g_return_val_if_fail (state, FALSE);

  item = g_object_new (GTK_SOURCE_TYPE_COMPLETION_ITEM,
                       "text", key,
                       "label", key,
                       NULL);

  state->results = g_list_prepend (state->results, item);

  return FALSE;
}

static void
gb_html_completion_provider_populate (GtkSourceCompletionProvider *provider,
                                      GtkSourceCompletionContext  *context)
{
  SearchState state = { 0 };
  Trie *trie = NULL;
  gchar *word;
  gint mode;

  g_return_if_fail (GB_IS_HTML_COMPLETION_PROVIDER (provider));
  g_return_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));

  mode = get_mode (context);
  word = get_word (context);

  switch (mode)
    {
    case MODE_NONE:
      break;

    case MODE_ELEMENT_END:
    case MODE_ELEMENT_START:
      trie = elements;
      break;

    case MODE_ATTRIBUTE_NAME:
      trie = attributes;
      break;

    case MODE_CSS:
      trie = css_styles;
      break;

    case MODE_ATTRIBUTE_VALUE:
      break;

    default:
      break;
    }

  if (trie && word)
    {
      trie_traverse (trie, word, G_PRE_ORDER, G_TRAVERSE_LEAVES, -1,
                     traverse_cb, &state);
      state.results = g_list_reverse (state.results);
    }

  gtk_source_completion_context_add_proposals (context, provider,
                                               state.results, TRUE);

  g_list_foreach (state.results, (GFunc)g_object_unref, NULL);
  g_list_free (state.results);

  g_free (word);
}

static GdkPixbuf *
gb_html_completion_provider_get_icon (GtkSourceCompletionProvider *provider)
{
  return NULL;
}

static void
gb_html_completion_provider_class_init (GbHtmlCompletionProviderClass *klass)
{
  elements = trie_new (NULL);
  attributes = trie_new (NULL);
  css_styles = trie_new (NULL);

#define ADD_STRING(dict, str) trie_insert(dict,str,str)

  /*
   * TODO: We should determine what are valid attributes for given elements
   *       and only provide those based upon the completion context.
   */

  ADD_STRING (elements, "a");
  ADD_STRING (elements, "body");
  ADD_STRING (elements, "div");
  ADD_STRING (elements, "head");
  ADD_STRING (elements, "html");
  ADD_STRING (elements, "li");
  ADD_STRING (elements, "ol");
  ADD_STRING (elements, "p");
  ADD_STRING (elements, "table");
  ADD_STRING (elements, "title");
  ADD_STRING (elements, "ul");

  ADD_STRING (attributes, "style");
  ADD_STRING (attributes, "href");

  ADD_STRING (css_styles, "border");
  ADD_STRING (css_styles, "background");
  ADD_STRING (css_styles, "background-image");
  ADD_STRING (css_styles, "background-color");
  ADD_STRING (css_styles, "text-align");

#undef ADD_STRING
}

static void
gb_html_completion_provider_init (GbHtmlCompletionProvider *self)
{
}

static void
completion_provider_init (GtkSourceCompletionProviderIface *iface)
{
  iface->get_icon = gb_html_completion_provider_get_icon;
  iface->populate = gb_html_completion_provider_populate;
}
