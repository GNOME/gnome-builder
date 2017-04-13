/* test-suggestion.c
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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
#include <webkit2/webkit2.h>

#include "egg-suggestion.h"
#include "egg-suggestion-entry.h"
#include "fuzzy.h"

/*
 * Most of this is exactly how you SHOULD NOT write a web browser
 * shell. It's just dummy code to test the suggestions widget.
 * Think for yourself before copying any of this code.
 */

typedef struct
{
  const gchar *icon_name;
  const gchar *url;
  const gchar *title;
  const gchar *suffix;
} DemoData;

static Fuzzy *search_index;
static const DemoData demo_data[] = {
  { NULL, "https://twitter.com", "Twitter", "twitter.com" },
  { NULL, "https://facebook.com", "Facebook", "facebook.com" },
  { NULL, "https://google.com", "Google", "google.com" },
  { NULL, "https://images.google.com", "Google Images", "images.google.com" },
  { NULL, "https://news.ycombinator.com", "Hacker News", "news.ycombinator.com" },
  { NULL, "https://reddit.com/r/gnome", "GNOME Desktop Environment", "reddit.com/r/gnome" },
  { NULL, "https://reddit.com/r/linux", "Linux, GNU/Linux, free software", "reddit.com/r/linux" },
  { NULL, "https://wiki.gnome.org", "GNOME Wiki", "wiki.gnome.org" },
  { NULL, "https://gnome.org", "GNOME", "gnome.org" },
  { NULL, "https://planet.gnome.org", "Planet GNOME", "planet.gnome.org" },
  { NULL, "https://wiki.gnome.org/Apps/Builder", "GNOME Builder", "wiki.gnome.org/Apps/Builder" },
};

static void
take_item (GListStore    *store,
           EggSuggestion *suggestion)
{
  g_list_store_append (store, suggestion);
  g_object_unref (suggestion);
}

static gboolean
is_a_url (const gchar *str)
{
  /* you obviously want something better */

  if (strstr (str, ".com") ||
      strstr (str, ".net") ||
      strstr (str, ".org") ||
      strstr (str, ".io") ||
      strstr (str, ".ly"))
    return TRUE;

  return FALSE;
}

static gint
compare_match (gconstpointer a,
               gconstpointer b)
{
  const FuzzyMatch *match_a = a;
  const FuzzyMatch *match_b = b;

  if (match_a->score < match_b->score)
    return 1;
  else if (match_a->score > match_b->score)
    return -1;
  else
    return 0;
}

static gchar *
suggest_suffix (EggSuggestion  *suggestion,
                const gchar    *typed_text,
                const DemoData *data)
{
  //g_print ("Suffix: Typed_Text=%s\n", typed_text);

  if (g_str_has_prefix (data->suffix, typed_text))
    return g_strdup (data->suffix + strlen (typed_text));

  return NULL;
}

static gchar *
replace_typed_text (EggSuggestion  *suggestion,
                    const gchar    *typed_text,
                    const DemoData *data)
{
  return g_strdup (data->url);
}

static GListModel *
create_search_results (const gchar *full_query,
                       const gchar *query)
{
  GListStore *store = g_list_store_new (EGG_TYPE_SUGGESTION);
  g_autoptr(GArray) matches = NULL;
  g_autofree gchar *search_url = NULL;
  g_autofree gchar *with_slashes = g_strdup_printf ("://%s", query);
  gboolean exact = FALSE;

  matches = fuzzy_match (search_index, query, 20);

  g_array_sort (matches, compare_match);

  for (guint i = 0; i < matches->len; i++)
    {
      const FuzzyMatch *match = &g_array_index (matches, FuzzyMatch, i);
      const DemoData *data = match->value;
      g_autofree gchar *markup = NULL;
      EggSuggestion *item;

      markup = fuzzy_highlight (search_index, data->url, query);

      if (g_str_has_suffix (data->url, with_slashes))
        exact = TRUE;

      item = g_object_new (EGG_TYPE_SUGGESTION,
                           "id", data->url,
                           "icon-name", data->icon_name,
                           "title", markup,
                           "subtitle", data->title,
                           NULL);
      g_signal_connect (item, "suggest-suffix", G_CALLBACK (suggest_suffix), (gpointer)data);
      g_signal_connect (item, "replace-typed-text", G_CALLBACK (replace_typed_text), (gpointer)data);
      take_item (store, item);
    }

  if (!exact && is_a_url (full_query))
    {
      g_autofree gchar *url = g_strdup_printf ("http://%s", full_query);
      take_item (store,
                 g_object_new (EGG_TYPE_SUGGESTION,
                               "id", url,
                               "icon-name", NULL,
                               "title", query,
                               "subtitle", NULL,
                               NULL));
    }

  search_url = g_strdup_printf ("https://www.google.com/search?q=%s", full_query);
  take_item (store,
             g_object_new (EGG_TYPE_SUGGESTION,
                           "id", search_url,
                           "title", full_query,
                           "subtitle", "Google Search",
                           "icon-name", "edit-find-symbolic",
                           NULL));

  return G_LIST_MODEL (store);
}

static gboolean
key_press (GtkWidget   *widget,
           GdkEventKey *key,
           GtkEntry    *param)
{
  if (key->keyval == GDK_KEY_l && (key->state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK)
    {
      gtk_widget_grab_focus (GTK_WIDGET (param));
      gtk_editable_select_region (GTK_EDITABLE (param), 0, -1);
    }
  else if (key->keyval == GDK_KEY_w && (key->state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK)
    gtk_window_close (GTK_WINDOW (widget));

  return FALSE;
}

static void
search_changed (EggSuggestionEntry *entry,
                gpointer            user_data)
{
  g_autoptr(GListModel) model = NULL;
  GString *str = g_string_new (NULL);
  const gchar *text;

  g_assert (EGG_IS_SUGGESTION_ENTRY (entry));

  text = egg_suggestion_entry_get_typed_text (entry);

  for (const gchar *iter = text; *iter; iter = g_utf8_next_char (iter))
    {
      gunichar ch = g_utf8_get_char (iter);

      if (!g_unichar_isspace (ch))
        g_string_append_unichar (str, ch);
    }

  if (str->len)
    model = create_search_results (text, str->str);

  egg_suggestion_entry_set_model (entry, model);
}

static void
suggestion_activated (EggSuggestionEntry *entry,
                      EggSuggestion      *suggestion,
                      WebKitWebView      *webview)
{
  const gchar *uri = egg_suggestion_get_id (suggestion);

  g_print ("Activated suggestion: %s\n", uri);

  gtk_widget_grab_focus (GTK_WIDGET (webview));
  webkit_web_view_load_uri (webview, uri);
  gtk_entry_set_text (GTK_ENTRY (entry), uri);
}

static void
notify_uri_cb (WebKitWebView      *webview,
               GParamSpec         *pspec,
               EggSuggestionEntry *entry)
{
  const gchar *uri;

  g_assert (WEBKIT_IS_WEB_VIEW (webview));
  g_assert (EGG_IS_SUGGESTION_ENTRY (entry));

  uri = webkit_web_view_get_uri (webview);

  gtk_entry_set_text (GTK_ENTRY (entry), uri);
}

int
main (gint   argc,
      gchar *argv[])
{
  GtkWidget *window;
  GtkWidget *header;
  GtkWidget *entry;
  GtkWidget *box;
  GtkWidget *button;
  GtkWidget *webkit;

  gtk_init (&argc, &argv);

  search_index = fuzzy_new (FALSE);

  for (guint i = 0; i < G_N_ELEMENTS (demo_data); i++)
    {
      const DemoData *data = &demo_data[i];

      fuzzy_insert (search_index, data->url, (gpointer)data);
    }

  window = g_object_new (GTK_TYPE_WINDOW,
                         "default-width", 1100,
                         "default-height", 600,
                         NULL);
  header = g_object_new (GTK_TYPE_HEADER_BAR,
                         "show-close-button", TRUE,
                         "visible", TRUE,
                         NULL);

  webkit = g_object_new (WEBKIT_TYPE_WEB_VIEW,
                         "visible", TRUE,
                         NULL);
  webkit_web_view_load_html (WEBKIT_WEB_VIEW (webkit),
                             "<html><body style='background: #4a86cf;'></body></html>",
                             NULL);
  gtk_container_add (GTK_CONTAINER (window), webkit);

  box = g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_HORIZONTAL,
                      "visible", TRUE,
                      "spacing", 0,
                      NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (box)), "linked");

  entry = g_object_new (EGG_TYPE_SUGGESTION_ENTRY,
                        "halign", GTK_ALIGN_CENTER,
                        "hexpand", FALSE,
                        "max-width-chars", 55,
                        "visible", TRUE,
                        "width-chars", 30,
                        NULL);
  gtk_box_set_center_widget (GTK_BOX (box), entry);
  g_signal_connect (entry, "changed", G_CALLBACK (search_changed), NULL);
  g_signal_connect (entry, "suggestion-activated", G_CALLBACK (suggestion_activated), webkit);

  button = g_object_new (GTK_TYPE_BUTTON,
                         "halign", GTK_ALIGN_START,
                         "hexpand", FALSE,
                         "visible", TRUE,
                         "child", g_object_new (GTK_TYPE_IMAGE,
                                                "visible", TRUE,
                                                "icon-name", "web-browser-symbolic",
                                                NULL),
                         NULL);
  gtk_box_pack_end (GTK_BOX (box), button, FALSE, FALSE, 0);

  gtk_window_set_titlebar (GTK_WINDOW (window), header);
  gtk_header_bar_set_custom_title (GTK_HEADER_BAR (header), box);

  g_signal_connect (webkit, "notify::uri", G_CALLBACK (notify_uri_cb), entry);
  g_signal_connect (window, "delete-event", gtk_main_quit, NULL);
  g_signal_connect (window, "key-press-event", G_CALLBACK (key_press), entry);
  gtk_window_present (GTK_WINDOW (window));

  gtk_main ();

  return 0;
}
