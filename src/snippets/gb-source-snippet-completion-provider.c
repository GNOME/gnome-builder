/* gb-source-snippet-completion-provider.c
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

#include <glib/gi18n.h>
#include <gtksourceview/gtksourcecompletionitem.h>

#include "gb-source-snippet-completion-item.h"
#include "gb-source-snippet-completion-provider.h"

static void init_provider (GtkSourceCompletionProviderIface *iface);

G_DEFINE_TYPE_EXTENDED (GbSourceSnippetCompletionProvider,
                        gb_source_snippet_completion_provider,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROVIDER,
                                               init_provider))

struct _GbSourceSnippetCompletionProviderPrivate
{
  GbSourceView     *source_view;
  GbSourceSnippets *snippets;
};

typedef struct
{
  GtkSourceCompletionProvider *provider;
  gchar                       *word;
  GList                       *list;
} SearchState;

enum {
  PROP_0,
  PROP_SNIPPETS,
  PROP_SOURCE_VIEW,
  LAST_PROP
};

static GParamSpec *gParamSpecs[LAST_PROP];

GtkSourceCompletionProvider *
gb_source_snippet_completion_provider_new (GbSourceView     *source_view,
                                           GbSourceSnippets *snippets)
{
  return g_object_new (GB_TYPE_SOURCE_SNIPPET_COMPLETION_PROVIDER,
                       "source-view", source_view,
                       "snippets", snippets,
                       NULL);
}

GbSourceSnippets *
gb_source_snippet_completion_provider_get_snippets (GbSourceSnippetCompletionProvider *provider)
{
  g_return_val_if_fail (GB_IS_SOURCE_SNIPPET_COMPLETION_PROVIDER (provider), NULL);

  return provider->priv->snippets;
}

void
gb_source_snippet_completion_provider_set_snippets (GbSourceSnippetCompletionProvider *provider,
                                                    GbSourceSnippets                  *snippets)
{
  g_return_if_fail (GB_IS_SOURCE_SNIPPET_COMPLETION_PROVIDER (provider));

  g_clear_object (&provider->priv->snippets);
  provider->priv->snippets = snippets ? g_object_ref (snippets) : NULL;
  g_object_notify_by_pspec (G_OBJECT (provider), gParamSpecs[PROP_SNIPPETS]);
}

static void
gb_source_snippet_completion_provider_finalize (GObject *object)
{
  GbSourceSnippetCompletionProviderPrivate *priv;

  priv = GB_SOURCE_SNIPPET_COMPLETION_PROVIDER (object)->priv;

  g_clear_object (&priv->snippets);

  if (priv->source_view)
    g_object_remove_weak_pointer (G_OBJECT (priv->source_view),
                                  (gpointer *) &priv->source_view);

  G_OBJECT_CLASS (gb_source_snippet_completion_provider_parent_class)->finalize (object);
}

static void
gb_source_snippet_completion_provider_get_property (GObject    *object,
                                                    guint       prop_id,
                                                    GValue     *value,
                                                    GParamSpec *pspec)
{
  GbSourceSnippetCompletionProvider *provider = GB_SOURCE_SNIPPET_COMPLETION_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_SOURCE_VIEW:
      g_value_set_object (value, provider->priv->source_view);
      break;

    case PROP_SNIPPETS:
      g_value_set_object (value, gb_source_snippet_completion_provider_get_snippets (provider));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_source_snippet_completion_provider_set_property (GObject      *object,
                                                    guint         prop_id,
                                                    const GValue *value,
                                                    GParamSpec   *pspec)
{
  GbSourceSnippetCompletionProvider *provider = GB_SOURCE_SNIPPET_COMPLETION_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_SOURCE_VIEW:
      if (provider->priv->source_view)
        {
          g_object_remove_weak_pointer (G_OBJECT (provider->priv->source_view),
                                        (gpointer *) &provider->priv->source_view);
          provider->priv->source_view = NULL;
        }
      if ((provider->priv->source_view = g_value_get_object (value)))
        g_object_add_weak_pointer (G_OBJECT (provider->priv->source_view),
                                   (gpointer *) &provider->priv->source_view);
      break;

    case PROP_SNIPPETS:
      gb_source_snippet_completion_provider_set_snippets (provider, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_source_snippet_completion_provider_class_init (GbSourceSnippetCompletionProviderClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = gb_source_snippet_completion_provider_finalize;
  object_class->get_property = gb_source_snippet_completion_provider_get_property;
  object_class->set_property = gb_source_snippet_completion_provider_set_property;
  g_type_class_add_private (object_class, sizeof (GbSourceSnippetCompletionProviderPrivate));

  gParamSpecs[PROP_SOURCE_VIEW] =
    g_param_spec_object ("source-view",
                         _ ("Source View"),
                         _ ("The source view to insert snippet into."),
                         GB_TYPE_SOURCE_VIEW,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SOURCE_VIEW,
                                   gParamSpecs[PROP_SOURCE_VIEW]);

  gParamSpecs[PROP_SNIPPETS] =
    g_param_spec_object ("snippets",
                         _ ("Snippets"),
                         _ ("The snippets to complete with this provider."),
                         GB_TYPE_SOURCE_SNIPPETS,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SNIPPETS,
                                   gParamSpecs[PROP_SNIPPETS]);
}

static void
gb_source_snippet_completion_provider_init (GbSourceSnippetCompletionProvider *provider)
{
  provider->priv =
    G_TYPE_INSTANCE_GET_PRIVATE (provider,
                                 GB_TYPE_SOURCE_SNIPPET_COMPLETION_PROVIDER,
                                 GbSourceSnippetCompletionProviderPrivate);
}

static gboolean
is_stop_char (gunichar c)
{
  switch (c)
    {
    case '_':
      return FALSE;

    case ')':
    case '(':
    case '&':
    case '*':
    case '{':
    case '}':
    case ' ':
    case '\t':
    case '[':
    case ']':
    case '=':
    case '"':
    case '\'':
      return TRUE;

    default:
      return !g_unichar_isalnum (c);
    }
}

static gchar *
get_word (GtkSourceCompletionProvider *provider,
          GtkTextIter                 *iter)
{
  GtkTextIter end;
  gboolean moved = FALSE;
  gunichar c;
  gchar *word;

  gtk_text_iter_assign (&end, iter);

  do
    {
      if (!gtk_text_iter_backward_char (iter))
        break;
      c = gtk_text_iter_get_char (iter);
      moved = TRUE;
    }
  while (!is_stop_char (c));

  if (moved && !gtk_text_iter_is_start (iter))
    gtk_text_iter_forward_char (iter);

  word = g_strstrip (gtk_text_iter_get_text (iter, &end));

  return word;
}

static GdkPixbuf *
provider_get_icon (GtkSourceCompletionProvider *provider)
{
  return NULL;
}

static gint
provider_get_interactive_delay (GtkSourceCompletionProvider *provider)
{
  return 0;
}

static gint
provider_get_priority (GtkSourceCompletionProvider *provider)
{
  return 200;
}

static gchar *
provider_get_name (GtkSourceCompletionProvider *provider)
{
  return g_strdup (_ ("Snippets"));
}

static void
foreach_snippet (gpointer data,
                 gpointer user_data)
{
  GtkSourceCompletionProposal *item;
  GbSourceSnippet *snippet = data;
  SearchState *state = user_data;
  const char *trigger;

  trigger = gb_source_snippet_get_trigger (snippet);
  if (!g_str_has_prefix (trigger, state->word))
    return;

  item = gb_source_snippet_completion_item_new (snippet);
  state->list = g_list_append (state->list, item);
}

static void
provider_populate (GtkSourceCompletionProvider *provider,
                   GtkSourceCompletionContext  *context)
{
  GbSourceSnippetCompletionProviderPrivate *priv;
  SearchState state = { 0 };
  GtkTextIter iter;

  priv = GB_SOURCE_SNIPPET_COMPLETION_PROVIDER (provider)->priv;

  if (!priv->snippets)
    {
      gtk_source_completion_context_add_proposals (context, provider, NULL, TRUE);
      return;
    }

  gtk_source_completion_context_get_iter (context, &iter);

  state.list = NULL;
  state.provider = provider;
  state.word = get_word (provider, &iter);

  if (state.word && *state.word)
    gb_source_snippets_foreach (priv->snippets, state.word, foreach_snippet,
                                &state);

  /*
   * XXX: GtkSourceView seems to be warning quite a bit inside here
   *      right now about g_object_ref(). But ... it doesn't seem to be us?
   */
  gtk_source_completion_context_add_proposals (context, provider, state.list, TRUE);

  g_list_foreach (state.list, (GFunc) g_object_unref, NULL);
  g_list_free (state.list);
  g_free (state.word);
}

static gboolean
provider_activate_proposal (GtkSourceCompletionProvider *provider,
                            GtkSourceCompletionProposal *proposal,
                            GtkTextIter                 *iter)
{
  GbSourceSnippetCompletionProviderPrivate *priv;
  GbSourceSnippetCompletionItem *item;
  GbSourceSnippet *snippet;
  GtkTextBuffer *buffer;
  GtkTextIter end;
  gchar *word;

  priv = GB_SOURCE_SNIPPET_COMPLETION_PROVIDER (provider)->priv;

  if (priv->source_view)
    {
      item = GB_SOURCE_SNIPPET_COMPLETION_ITEM (proposal);
      snippet = gb_source_snippet_completion_item_get_snippet (item);
      if (snippet)
        {
          /*
           * Fetching the word will move us back to the beginning of it.
           */
          gtk_text_iter_assign (&end, iter);
          word = get_word (provider, iter);
          g_free (word);

          /*
           * Now delete the current word since it will get overwritten
           * by the insertion of the snippet.
           */
          buffer = gtk_text_iter_get_buffer (iter);
          gtk_text_buffer_delete (buffer, iter, &end);

          /*
           * Now push snippet onto the snippet stack of the view.
           */
          snippet = gb_source_snippet_copy (snippet);
          gb_source_view_push_snippet (GB_SOURCE_VIEW (priv->source_view),
                                       snippet);
          g_object_unref (snippet);

          return TRUE;
        }
    }

  return FALSE;
}

static void
init_provider (GtkSourceCompletionProviderIface *iface)
{
  iface->activate_proposal = provider_activate_proposal;
  iface->get_icon = provider_get_icon;
  iface->get_interactive_delay = provider_get_interactive_delay;
  iface->get_name = provider_get_name;
  iface->get_priority = provider_get_priority;
  iface->populate = provider_populate;
}
