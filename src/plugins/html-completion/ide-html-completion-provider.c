/* ide-html-completion-provider.c
 *
 * Copyright 2014 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "html-completion"

#include <dazzle.h>
#include <libpeas/peas.h>
#include <string.h>

#include "ide-html-completion-provider.h"

static GHashTable *element_attrs;
static DzlTrie *css_styles;
static DzlTrie *elements;

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
  gint mode;
} SearchState;

struct _IdeHtmlCompletionProvider
{
  IdeObject parent_instance;
};

static void completion_provider_init (GtkSourceCompletionProviderIface *);

G_DEFINE_TYPE_EXTENDED (IdeHtmlCompletionProvider,
                        ide_html_completion_provider,
                        IDE_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROVIDER, completion_provider_init)
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_COMPLETION_PROVIDER, NULL))

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
        {
          GtkTextIter end = copy;

          if (gtk_text_iter_forward_char (&end) && '?' == gtk_text_iter_get_char (&end))
            return FALSE;

          return TRUE;
        }
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
traverse_cb (DzlTrie        *trie,
             const gchar *key,
             gpointer     value,
             gpointer     user_data)
{
  SearchState *state = user_data;
  GtkSourceCompletionItem *item;
  const gchar *text = key;
  gchar *tmp = NULL;

  g_return_val_if_fail (trie, FALSE);
  g_return_val_if_fail (state, FALSE);

  if (state->mode == MODE_ATTRIBUTE_NAME)
    {
      tmp = g_strdup_printf ("%s=", key);
      text = tmp;
    }

  item = g_object_new (GTK_SOURCE_TYPE_COMPLETION_ITEM,
                       "text", text,
                       "label", key,
                       NULL);

  state->results = g_list_prepend (state->results, item);

  g_free (tmp);

  return FALSE;
}

static gboolean
find_space (gunichar ch,
            gpointer user_data)
{
  return g_unichar_isspace (ch);
}

static gchar *
get_element (GtkSourceCompletionContext *context)
{
  GtkTextIter iter;
  GtkTextIter begin;
  GtkTextIter end;
  GtkTextIter match_begin;
  GtkTextIter match_end;

  g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (context), NULL);

  gtk_source_completion_context_get_iter (context, &iter);

  if (gtk_text_iter_backward_search (&iter, "<", GTK_TEXT_SEARCH_TEXT_ONLY,
                                     &match_begin, &match_end, NULL))
    {
      end = begin = match_end;

      if (gtk_text_iter_forward_find_char (&end, find_space, NULL, &iter))
        return gtk_text_iter_get_slice (&begin, &end);
    }

  return NULL;
}

static gint
sort_completion_items (gconstpointer a,
                       gconstpointer b)
{
  gchar *astr;
  gchar *bstr;
  gint ret;

  /*
   * XXX: This is very much not ideal. We are allocating a string for every
   *      compare! But we don't have accessor funcs into the completion item.
   *      We should probably make our completion item class.
   */

  g_object_get (GTK_SOURCE_COMPLETION_ITEM (a),
                "label", &astr,
                NULL);
  g_object_get (GTK_SOURCE_COMPLETION_ITEM (b),
                "label", &bstr,
                NULL);

  ret = g_strcmp0 (astr, bstr);

  g_free (astr);
  g_free (bstr);

  return ret;
}

static void
ide_html_completion_provider_populate (GtkSourceCompletionProvider *provider,
                                      GtkSourceCompletionContext  *context)
{
  SearchState state = { 0 };
  DzlTrie *trie = NULL;
  gchar *word;
  gint mode;

  g_return_if_fail (IDE_IS_HTML_COMPLETION_PROVIDER (provider));
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
      {
        gchar *element;

        if ((element = get_element (context)))
          {
            trie = g_hash_table_lookup (element_attrs, element);
            g_free (element);
          }

        break;
      }

    case MODE_CSS:
      trie = css_styles;
      break;

    case MODE_ATTRIBUTE_VALUE:
      break;

    default:
      break;
    }

  state.mode = mode;

  /*
   * Load the values for the context.
   */
  if (trie && word)
    {
      dzl_trie_traverse (trie, word, G_PRE_ORDER, G_TRAVERSE_LEAVES, -1,
                     traverse_cb, &state);
    }

  /*
   * If we are in an attribute, also load the global attributes values.
   */
  if (mode == MODE_ATTRIBUTE_NAME)
    {
      DzlTrie *global;

      global = g_hash_table_lookup (element_attrs, "*");
      dzl_trie_traverse (global, word, G_PRE_ORDER, G_TRAVERSE_LEAVES, -1,
                     traverse_cb, &state);
    }

  /*
   * TODO: Not exactly an ideal sort mechanism.
   */
  state.results = g_list_sort (state.results, sort_completion_items);

  gtk_source_completion_context_add_proposals (context, provider,
                                               state.results, TRUE);

  g_list_foreach (state.results, (GFunc)g_object_unref, NULL);
  g_list_free (state.results);

  g_free (word);
}

static GdkPixbuf *
ide_html_completion_provider_get_icon (GtkSourceCompletionProvider *provider)
{
  return NULL;
}

static void
ide_html_completion_provider_class_init (IdeHtmlCompletionProviderClass *klass)
{
  elements = dzl_trie_new (NULL);
  element_attrs = g_hash_table_new (g_str_hash, g_str_equal);
  css_styles = dzl_trie_new (NULL);

#define ADD_ELEMENT(str)        dzl_trie_insert(elements,str,str)
#define ADD_STRING(dict, str)   dzl_trie_insert(dict,str,str)
#define ADD_ATTRIBUTE(ele,attr) \
  G_STMT_START { \
    DzlTrie *t = g_hash_table_lookup (element_attrs, ele); \
    if (!t) { \
      t = dzl_trie_new (NULL); \
      g_hash_table_insert (element_attrs, ele, t); \
    } \
    dzl_trie_insert (t,attr,attr); \
  } G_STMT_END

  /*
   * TODO: We should determine what are valid attributes for given elements
   *       and only provide those based upon the completion context.
   */

  /*
   * http://www.w3.org/TR/html-markup/elements.html
   */

  ADD_ELEMENT ("a");
  ADD_ELEMENT ("abbr");
  ADD_ELEMENT ("acronym");
  ADD_ELEMENT ("address");
  ADD_ELEMENT ("applet");
  ADD_ELEMENT ("area");
  ADD_ELEMENT ("article");
  ADD_ELEMENT ("aside");
  ADD_ELEMENT ("audio");
  ADD_ELEMENT ("b");
  ADD_ELEMENT ("base");
  ADD_ELEMENT ("basefont");
  ADD_ELEMENT ("bdi");
  ADD_ELEMENT ("bdo");
  ADD_ELEMENT ("big");
  ADD_ELEMENT ("blockquote");
  ADD_ELEMENT ("body");
  ADD_ELEMENT ("br");
  ADD_ELEMENT ("button");
  ADD_ELEMENT ("canvas");
  ADD_ELEMENT ("caption");
  ADD_ELEMENT ("center");
  ADD_ELEMENT ("cite");
  ADD_ELEMENT ("code");
  ADD_ELEMENT ("col");
  ADD_ELEMENT ("colgroup");
  ADD_ELEMENT ("datalist");
  ADD_ELEMENT ("dd");
  ADD_ELEMENT ("del");
  ADD_ELEMENT ("details");
  ADD_ELEMENT ("dfn");
  ADD_ELEMENT ("dialog");
  ADD_ELEMENT ("dir");
  ADD_ELEMENT ("div");
  ADD_ELEMENT ("dl");
  ADD_ELEMENT ("dt");
  ADD_ELEMENT ("em");
  ADD_ELEMENT ("embed");
  ADD_ELEMENT ("fieldset");
  ADD_ELEMENT ("figcaption");
  ADD_ELEMENT ("figure");
  ADD_ELEMENT ("font");
  ADD_ELEMENT ("footer");
  ADD_ELEMENT ("form");
  ADD_ELEMENT ("frame");
  ADD_ELEMENT ("frameset");
  ADD_ELEMENT ("head");
  ADD_ELEMENT ("header");
  ADD_ELEMENT ("hgroup");
  ADD_ELEMENT ("h1");
  ADD_ELEMENT ("h2");
  ADD_ELEMENT ("h3");
  ADD_ELEMENT ("h4");
  ADD_ELEMENT ("h5");
  ADD_ELEMENT ("h6");
  ADD_ELEMENT ("hr");
  ADD_ELEMENT ("html");
  ADD_ELEMENT ("i");
  ADD_ELEMENT ("iframe");
  ADD_ELEMENT ("img");
  ADD_ELEMENT ("input");
  ADD_ELEMENT ("ins");
  ADD_ELEMENT ("kbd");
  ADD_ELEMENT ("keygen");
  ADD_ELEMENT ("label");
  ADD_ELEMENT ("legend");
  ADD_ELEMENT ("li");
  ADD_ELEMENT ("link");
  ADD_ELEMENT ("main");
  ADD_ELEMENT ("map");
  ADD_ELEMENT ("mark");
  ADD_ELEMENT ("menu");
  ADD_ELEMENT ("menuitem");
  ADD_ELEMENT ("meta");
  ADD_ELEMENT ("meter");
  ADD_ELEMENT ("nav");
  ADD_ELEMENT ("noframes");
  ADD_ELEMENT ("noscript");
  ADD_ELEMENT ("object");
  ADD_ELEMENT ("ol");
  ADD_ELEMENT ("optgroup");
  ADD_ELEMENT ("option");
  ADD_ELEMENT ("output");
  ADD_ELEMENT ("p");
  ADD_ELEMENT ("param");
  ADD_ELEMENT ("pre");
  ADD_ELEMENT ("progress");
  ADD_ELEMENT ("q");
  ADD_ELEMENT ("rp");
  ADD_ELEMENT ("rt");
  ADD_ELEMENT ("ruby");
  ADD_ELEMENT ("s");
  ADD_ELEMENT ("samp");
  ADD_ELEMENT ("script");
  ADD_ELEMENT ("section");
  ADD_ELEMENT ("select");
  ADD_ELEMENT ("small");
  ADD_ELEMENT ("source");
  ADD_ELEMENT ("span");
  ADD_ELEMENT ("strike");
  ADD_ELEMENT ("strong");
  ADD_ELEMENT ("style");
  ADD_ELEMENT ("sub");
  ADD_ELEMENT ("summary");
  ADD_ELEMENT ("sup");
  ADD_ELEMENT ("table");
  ADD_ELEMENT ("tbody");
  ADD_ELEMENT ("td");
  ADD_ELEMENT ("textarea");
  ADD_ELEMENT ("tfoot");
  ADD_ELEMENT ("th");
  ADD_ELEMENT ("thead");
  ADD_ELEMENT ("time");
  ADD_ELEMENT ("title");
  ADD_ELEMENT ("tr");
  ADD_ELEMENT ("track");
  ADD_ELEMENT ("tt");
  ADD_ELEMENT ("u");
  ADD_ELEMENT ("ul");
  ADD_ELEMENT ("var");
  ADD_ELEMENT ("video");
  ADD_ELEMENT ("wbr");

  ADD_ATTRIBUTE ("*", "accesskey");
  ADD_ATTRIBUTE ("*", "class");
  ADD_ATTRIBUTE ("*", "contenteditable");
  ADD_ATTRIBUTE ("*", "contextmenu");
  ADD_ATTRIBUTE ("*", "dir");
  ADD_ATTRIBUTE ("*", "draggable");
  ADD_ATTRIBUTE ("*", "dropzone");
  ADD_ATTRIBUTE ("*", "hidden");
  ADD_ATTRIBUTE ("*", "id");
  ADD_ATTRIBUTE ("*", "lang");
  ADD_ATTRIBUTE ("*", "spellcheck");
  ADD_ATTRIBUTE ("*", "style");
  ADD_ATTRIBUTE ("*", "tabindex");
  ADD_ATTRIBUTE ("*", "title");
  ADD_ATTRIBUTE ("*", "translate");

  ADD_ATTRIBUTE ("a", "href");
  ADD_ATTRIBUTE ("a", "target");
  ADD_ATTRIBUTE ("a", "rel");
  ADD_ATTRIBUTE ("a", "hreflang");
  ADD_ATTRIBUTE ("a", "media");
  ADD_ATTRIBUTE ("a", "type");

  ADD_ATTRIBUTE ("area", "alt");
  ADD_ATTRIBUTE ("area", "href");
  ADD_ATTRIBUTE ("area", "target");
  ADD_ATTRIBUTE ("area", "rel");
  ADD_ATTRIBUTE ("area", "media");
  ADD_ATTRIBUTE ("area", "hreflang");
  ADD_ATTRIBUTE ("area", "type");
  ADD_ATTRIBUTE ("area", "shape");
  ADD_ATTRIBUTE ("area", "coords");

  ADD_ATTRIBUTE ("audio", "autoplay");
  ADD_ATTRIBUTE ("audio", "preload");
  ADD_ATTRIBUTE ("audio", "controls");
  ADD_ATTRIBUTE ("audio", "loop");
  ADD_ATTRIBUTE ("audio", "mediagroup");
  ADD_ATTRIBUTE ("audio", "muted");
  ADD_ATTRIBUTE ("audio", "src");

  ADD_ATTRIBUTE ("base", "href");
  ADD_ATTRIBUTE ("base", "target");

  ADD_ATTRIBUTE ("blockquote", "cite");

  ADD_ATTRIBUTE ("button", "type");
  ADD_ATTRIBUTE ("button", "name");
  ADD_ATTRIBUTE ("button", "disabled");
  ADD_ATTRIBUTE ("button", "form");
  ADD_ATTRIBUTE ("button", "value");
  ADD_ATTRIBUTE ("button", "formaction");
  ADD_ATTRIBUTE ("button", "autofocus");
  ADD_ATTRIBUTE ("button", "formmethod");
  ADD_ATTRIBUTE ("button", "formtarget");
  ADD_ATTRIBUTE ("button", "formnovalidate");

  ADD_ATTRIBUTE ("canvas", "height");
  ADD_ATTRIBUTE ("canvas", "width");

  ADD_ATTRIBUTE ("col", "span");

  ADD_ATTRIBUTE ("colgroup", "span");

  ADD_ATTRIBUTE ("command", "type");
  ADD_ATTRIBUTE ("command", "label");
  ADD_ATTRIBUTE ("command", "icon");
  ADD_ATTRIBUTE ("command", "radiogroup");
  ADD_ATTRIBUTE ("command", "checked");
  ADD_ATTRIBUTE ("command", "type");

  ADD_ATTRIBUTE ("del", "cite");
  ADD_ATTRIBUTE ("del", "datetime");

  ADD_ATTRIBUTE ("details", "open");

  ADD_ATTRIBUTE ("embed", "src");
  ADD_ATTRIBUTE ("embed", "type");
  ADD_ATTRIBUTE ("embed", "height");
  ADD_ATTRIBUTE ("embed", "width");

  ADD_ATTRIBUTE ("fieldset", "name");
  ADD_ATTRIBUTE ("fieldset", "disabled");
  ADD_ATTRIBUTE ("fieldset", "form");

  ADD_ATTRIBUTE ("form", "action");
  ADD_ATTRIBUTE ("form", "method");
  ADD_ATTRIBUTE ("form", "enctype");
  ADD_ATTRIBUTE ("form", "name");
  ADD_ATTRIBUTE ("form", "accept-charset");
  ADD_ATTRIBUTE ("form", "novalidate");
  ADD_ATTRIBUTE ("form", "target");
  ADD_ATTRIBUTE ("form", "autocomplete");

  ADD_ATTRIBUTE ("html", "manifest");

  ADD_ATTRIBUTE ("iframe", "src");
  ADD_ATTRIBUTE ("iframe", "srcdoc");
  ADD_ATTRIBUTE ("iframe", "name");
  ADD_ATTRIBUTE ("iframe", "width");
  ADD_ATTRIBUTE ("iframe", "height");
  ADD_ATTRIBUTE ("iframe", "sandbox");
  ADD_ATTRIBUTE ("iframe", "seamless");

  ADD_ATTRIBUTE ("img", "src");
  ADD_ATTRIBUTE ("img", "alt");
  ADD_ATTRIBUTE ("img", "height");
  ADD_ATTRIBUTE ("img", "width");
  ADD_ATTRIBUTE ("img", "usemap");
  ADD_ATTRIBUTE ("img", "ismap");

  ADD_ATTRIBUTE ("input", "accept");
  ADD_ATTRIBUTE ("input", "alt");
  ADD_ATTRIBUTE ("input", "autocomplete");
  ADD_ATTRIBUTE ("input", "autofocus");
  ADD_ATTRIBUTE ("input", "dirname");
  ADD_ATTRIBUTE ("input", "disabled");
  ADD_ATTRIBUTE ("input", "form");
  ADD_ATTRIBUTE ("input", "formaction");
  ADD_ATTRIBUTE ("input", "formenctype");
  ADD_ATTRIBUTE ("input", "formmethod");
  ADD_ATTRIBUTE ("input", "formnovalidate");
  ADD_ATTRIBUTE ("input", "formtarget");
  ADD_ATTRIBUTE ("input", "height");
  ADD_ATTRIBUTE ("input", "list");
  ADD_ATTRIBUTE ("input", "list");
  ADD_ATTRIBUTE ("input", "max");
  ADD_ATTRIBUTE ("input", "maxlength");
  ADD_ATTRIBUTE ("input", "min");
  ADD_ATTRIBUTE ("input", "multiple");
  ADD_ATTRIBUTE ("input", "name");
  ADD_ATTRIBUTE ("input", "pattern");
  ADD_ATTRIBUTE ("input", "placeholder");
  ADD_ATTRIBUTE ("input", "readonly");
  ADD_ATTRIBUTE ("input", "required");
  ADD_ATTRIBUTE ("input", "size");
  ADD_ATTRIBUTE ("input", "src");
  ADD_ATTRIBUTE ("input", "step");
  ADD_ATTRIBUTE ("input", "type");
  ADD_ATTRIBUTE ("input", "value");
  ADD_ATTRIBUTE ("input", "width");

  ADD_ATTRIBUTE ("ins", "cite");
  ADD_ATTRIBUTE ("ins", "datetime");

  ADD_ATTRIBUTE ("keygen", "challenge");
  ADD_ATTRIBUTE ("keygen", "keytype");
  ADD_ATTRIBUTE ("keygen", "autofocus");
  ADD_ATTRIBUTE ("keygen", "name");
  ADD_ATTRIBUTE ("keygen", "disabled");
  ADD_ATTRIBUTE ("keygen", "form");

  ADD_ATTRIBUTE ("label", "for");
  ADD_ATTRIBUTE ("label", "form");

  ADD_ATTRIBUTE ("li", "value");

  ADD_ATTRIBUTE ("link", "href");
  ADD_ATTRIBUTE ("link", "rel");
  ADD_ATTRIBUTE ("link", "hreflang");
  ADD_ATTRIBUTE ("link", "media");
  ADD_ATTRIBUTE ("link", "type");
  ADD_ATTRIBUTE ("link", "sizes");

  ADD_ATTRIBUTE ("map", "name");

  ADD_ATTRIBUTE ("menu", "type");
  ADD_ATTRIBUTE ("menu", "label");

  ADD_ATTRIBUTE ("meta", "http-equiv");
  ADD_ATTRIBUTE ("meta", "content");
  ADD_ATTRIBUTE ("meta", "charset");

  ADD_ATTRIBUTE ("meter", "high");
  ADD_ATTRIBUTE ("meter", "low");
  ADD_ATTRIBUTE ("meter", "max");
  ADD_ATTRIBUTE ("meter", "min");
  ADD_ATTRIBUTE ("meter", "optimum");
  ADD_ATTRIBUTE ("meter", "value");

  ADD_ATTRIBUTE ("object", "data");
  ADD_ATTRIBUTE ("object", "type");
  ADD_ATTRIBUTE ("object", "height");
  ADD_ATTRIBUTE ("object", "width");
  ADD_ATTRIBUTE ("object", "usemap");
  ADD_ATTRIBUTE ("object", "name");
  ADD_ATTRIBUTE ("object", "form");

  ADD_ATTRIBUTE ("ol", "start");
  ADD_ATTRIBUTE ("ol", "reversed");
  ADD_ATTRIBUTE ("ol", "type");

  ADD_ATTRIBUTE ("optgroup", "label");
  ADD_ATTRIBUTE ("optgroup", "disabled");

  ADD_ATTRIBUTE ("option", "disabled");
  ADD_ATTRIBUTE ("option", "selected");
  ADD_ATTRIBUTE ("option", "label");
  ADD_ATTRIBUTE ("option", "value");

  ADD_ATTRIBUTE ("output", "name");
  ADD_ATTRIBUTE ("output", "form");
  ADD_ATTRIBUTE ("output", "for");

  ADD_ATTRIBUTE ("param", "name");
  ADD_ATTRIBUTE ("param", "value");

  ADD_ATTRIBUTE ("progress", "value");
  ADD_ATTRIBUTE ("progress", "max");

  ADD_ATTRIBUTE ("q", "cite");

  ADD_ATTRIBUTE ("script", "type");
  ADD_ATTRIBUTE ("script", "language");
  ADD_ATTRIBUTE ("script", "src");
  ADD_ATTRIBUTE ("script", "defer");
  ADD_ATTRIBUTE ("script", "async");
  ADD_ATTRIBUTE ("script", "charset");

  ADD_ATTRIBUTE ("select", "name");
  ADD_ATTRIBUTE ("select", "disabled");
  ADD_ATTRIBUTE ("select", "form");
  ADD_ATTRIBUTE ("select", "size");
  ADD_ATTRIBUTE ("select", "multiple");
  ADD_ATTRIBUTE ("select", "autofocus");
  ADD_ATTRIBUTE ("select", "required");

  ADD_ATTRIBUTE ("source", "src");
  ADD_ATTRIBUTE ("source", "type");
  ADD_ATTRIBUTE ("source", "media");

  ADD_ATTRIBUTE ("style", "type");
  ADD_ATTRIBUTE ("style", "media");
  ADD_ATTRIBUTE ("style", "scoped");

  ADD_ATTRIBUTE ("table", "border");

  ADD_ATTRIBUTE ("td", "colspan");
  ADD_ATTRIBUTE ("td", "rowspan");
  ADD_ATTRIBUTE ("td", "headers");

  ADD_ATTRIBUTE ("textarea", "name");
  ADD_ATTRIBUTE ("textarea", "disabled");
  ADD_ATTRIBUTE ("textarea", "form");
  ADD_ATTRIBUTE ("textarea", "readonly");
  ADD_ATTRIBUTE ("textarea", "maxlength");
  ADD_ATTRIBUTE ("textarea", "autofocus");
  ADD_ATTRIBUTE ("textarea", "required");
  ADD_ATTRIBUTE ("textarea", "placeholder");
  ADD_ATTRIBUTE ("textarea", "dirname");
  ADD_ATTRIBUTE ("textarea", "rows");
  ADD_ATTRIBUTE ("textarea", "wrap");
  ADD_ATTRIBUTE ("textarea", "cols");

  ADD_ATTRIBUTE ("th", "scope");
  ADD_ATTRIBUTE ("th", "colspan");
  ADD_ATTRIBUTE ("th", "rowspan");
  ADD_ATTRIBUTE ("th", "headers");

  ADD_ATTRIBUTE ("time", "datetime");

  ADD_ATTRIBUTE ("track", "kind");
  ADD_ATTRIBUTE ("track", "src");
  ADD_ATTRIBUTE ("track", "srclang");
  ADD_ATTRIBUTE ("track", "label");
  ADD_ATTRIBUTE ("track", "default");

  ADD_ATTRIBUTE ("video", "autoplay");
  ADD_ATTRIBUTE ("video", "preload");
  ADD_ATTRIBUTE ("video", "controls");
  ADD_ATTRIBUTE ("video", "loop");
  ADD_ATTRIBUTE ("video", "poster");
  ADD_ATTRIBUTE ("video", "height");
  ADD_ATTRIBUTE ("video", "width");
  ADD_ATTRIBUTE ("video", "mediagroup");
  ADD_ATTRIBUTE ("video", "muted");
  ADD_ATTRIBUTE ("video", "src");

  ADD_STRING (css_styles, "border");
  ADD_STRING (css_styles, "background");
  ADD_STRING (css_styles, "background-image");
  ADD_STRING (css_styles, "background-color");
  ADD_STRING (css_styles, "text-align");

#undef ADD_STRING
}

static void
ide_html_completion_provider_init (IdeHtmlCompletionProvider *self)
{
}

static void
completion_provider_init (GtkSourceCompletionProviderIface *iface)
{
  iface->get_icon = ide_html_completion_provider_get_icon;
  iface->populate = ide_html_completion_provider_populate;
}

void
ide_html_completion_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_COMPLETION_PROVIDER,
                                              IDE_TYPE_HTML_COMPLETION_PROVIDER);
}
