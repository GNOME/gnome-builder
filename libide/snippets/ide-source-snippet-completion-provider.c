/* ide-source-snippet-completion-provider.c
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
#include <gtksourceview/gtksource.h>

#include "snippets/ide-source-snippet-completion-item.h"
#include "snippets/ide-source-snippet-completion-provider.h"
#include "sourceview/ide-completion-provider.h"

struct _IdeSourceSnippetCompletionProvider
{
  GObject            parent_instance;

  GSettings         *settings;
  IdeSourceView     *source_view;
  IdeSourceSnippets *snippets;

  guint              enabled : 1;
};


static void init_provider (GtkSourceCompletionProviderIface *iface);

G_DEFINE_TYPE_WITH_CODE (IdeSourceSnippetCompletionProvider,
                        ide_source_snippet_completion_provider,
                        G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROVIDER,
                                               init_provider))

typedef struct
{
  GtkSourceCompletionProvider *provider;
  gchar                       *word;
  GList                       *list;
} SearchState;

enum {
  PROP_0,
  PROP_ENABLED,
  PROP_SNIPPETS,
  PROP_SOURCE_VIEW,
  LAST_PROP
};

static GParamSpec *properties[LAST_PROP];

GtkSourceCompletionProvider *
ide_source_snippet_completion_provider_new (IdeSourceView     *source_view,
                                           IdeSourceSnippets *snippets)
{
  return g_object_new (IDE_TYPE_SOURCE_SNIPPET_COMPLETION_PROVIDER,
                       "source-view", source_view,
                       "snippets", snippets,
                       NULL);
}

IdeSourceSnippets *
ide_source_snippet_completion_provider_get_snippets (IdeSourceSnippetCompletionProvider *provider)
{
  g_return_val_if_fail (IDE_IS_SOURCE_SNIPPET_COMPLETION_PROVIDER (provider), NULL);

  return provider->snippets;
}

void
ide_source_snippet_completion_provider_set_snippets (IdeSourceSnippetCompletionProvider *provider,
                                                    IdeSourceSnippets                  *snippets)
{
  g_return_if_fail (IDE_IS_SOURCE_SNIPPET_COMPLETION_PROVIDER (provider));

  g_clear_object (&provider->snippets);
  provider->snippets = snippets ? g_object_ref (snippets) : NULL;
  g_object_notify_by_pspec (G_OBJECT (provider), properties[PROP_SNIPPETS]);
}

static gboolean
ide_source_snippet_completion_provider_match (GtkSourceCompletionProvider *provider,
                                              GtkSourceCompletionContext  *context)
{
  IdeSourceSnippetCompletionProvider *self = (IdeSourceSnippetCompletionProvider *)provider;
  GtkSourceCompletionActivation activation;
  GtkTextIter iter;

  g_assert (IDE_IS_SOURCE_SNIPPET_COMPLETION_PROVIDER (self));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));

  if (!self->enabled)
    return FALSE;

  if (ide_completion_provider_context_in_comment_or_string (context))
    return FALSE;

  if (!gtk_source_completion_context_get_iter (context, &iter))
    return FALSE;

  activation = gtk_source_completion_context_get_activation (context);

  if (activation == GTK_SOURCE_COMPLETION_ACTIVATION_INTERACTIVE)
    {
      gunichar ch;

      if (!gtk_text_iter_starts_line (&iter))
        gtk_text_iter_backward_char (&iter);

      ch = gtk_text_iter_get_char (&iter);

      if (!g_unichar_isalnum (ch))
        return FALSE;
    }

  return TRUE;
}

static void
ide_source_snippet_completion_provider_constructed (GObject *object)
{
  IdeSourceSnippetCompletionProvider *self = (IdeSourceSnippetCompletionProvider *)object;

  self->settings = g_settings_new ("org.gnome.builder.code-insight");
  g_settings_bind (self->settings, "snippet-completion", self, "enabled", G_SETTINGS_BIND_GET);

  G_OBJECT_CLASS (ide_source_snippet_completion_provider_parent_class)->constructed (object);
}

static void
ide_source_snippet_completion_provider_finalize (GObject *object)
{
  IdeSourceSnippetCompletionProvider *self = IDE_SOURCE_SNIPPET_COMPLETION_PROVIDER (object);

  g_clear_object (&self->snippets);
  g_clear_object (&self->settings);

  if (self->source_view)
    g_object_remove_weak_pointer (G_OBJECT (self->source_view),
                                  (gpointer *) &self->source_view);

  G_OBJECT_CLASS (ide_source_snippet_completion_provider_parent_class)->finalize (object);
}

static void
ide_source_snippet_completion_provider_get_property (GObject    *object,
                                                    guint       prop_id,
                                                    GValue     *value,
                                                    GParamSpec *pspec)
{
  IdeSourceSnippetCompletionProvider *provider = IDE_SOURCE_SNIPPET_COMPLETION_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_ENABLED:
      g_value_set_boolean (value, provider->enabled);
      break;

    case PROP_SOURCE_VIEW:
      g_value_set_object (value, provider->source_view);
      break;

    case PROP_SNIPPETS:
      g_value_set_object (value, ide_source_snippet_completion_provider_get_snippets (provider));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_source_snippet_completion_provider_set_property (GObject      *object,
                                                    guint         prop_id,
                                                    const GValue *value,
                                                    GParamSpec   *pspec)
{
  IdeSourceSnippetCompletionProvider *provider = IDE_SOURCE_SNIPPET_COMPLETION_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_ENABLED:
      provider->enabled = g_value_get_boolean (value);
      break;

    case PROP_SOURCE_VIEW:
      if (provider->source_view)
        {
          g_object_remove_weak_pointer (G_OBJECT (provider->source_view),
                                        (gpointer *) &provider->source_view);
          provider->source_view = NULL;
        }
      if ((provider->source_view = g_value_get_object (value)))
        g_object_add_weak_pointer (G_OBJECT (provider->source_view),
                                   (gpointer *) &provider->source_view);
      break;

    case PROP_SNIPPETS:
      ide_source_snippet_completion_provider_set_snippets (provider, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_source_snippet_completion_provider_class_init (IdeSourceSnippetCompletionProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_source_snippet_completion_provider_constructed;
  object_class->finalize = ide_source_snippet_completion_provider_finalize;
  object_class->get_property = ide_source_snippet_completion_provider_get_property;
  object_class->set_property = ide_source_snippet_completion_provider_set_property;

  properties [PROP_ENABLED] =
    g_param_spec_boolean ("enabled",
                          "Enabled",
                          "If the provider is enabled.",
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties[PROP_SOURCE_VIEW] =
    g_param_spec_object ("source-view",
                         "Source View",
                         "The source view to insert snippet into.",
                         IDE_TYPE_SOURCE_VIEW,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties[PROP_SNIPPETS] =
    g_param_spec_object ("snippets",
                         "Snippets",
                         "The snippets to complete with this provider.",
                         IDE_TYPE_SOURCE_SNIPPETS,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
ide_source_snippet_completion_provider_init (IdeSourceSnippetCompletionProvider *self)
{
}

static gboolean
stop_on_predicate (gunichar ch,
                   gpointer data)
{
  switch (ch)
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
      return !g_unichar_isalnum (ch);
    }
}

static gchar *
get_word (GtkSourceCompletionProvider *provider,
          GtkTextIter                 *iter)
{
  GtkTextBuffer *buffer;
  GtkTextIter end;

  gtk_text_iter_assign (&end, iter);
  buffer = gtk_text_iter_get_buffer (iter);

  if (!gtk_text_iter_backward_find_char (iter, stop_on_predicate, NULL, NULL))
    return gtk_text_buffer_get_text (buffer, iter, &end, TRUE);

  gtk_text_iter_forward_char (iter);

  return gtk_text_iter_get_text (iter, &end);
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
  /* We want snippets at highest priority because they are
   * used for quick hacking without having to think to much
   * about whether they are active or not.
   */
  return IDE_SOURCE_SNIPPET_COMPLETION_PROVIDER_PRIORITY;
}

static gchar *
provider_get_name (GtkSourceCompletionProvider *provider)
{
  return g_strdup (_("Snippets"));
}

static void
foreach_snippet (gpointer data,
                 gpointer user_data)
{
  GtkSourceCompletionProposal *item;
  IdeSourceSnippet *snippet = data;
  SearchState *state = user_data;
  const char *trigger;

  trigger = ide_source_snippet_get_trigger (snippet);
  if (!g_str_has_prefix (trigger, state->word))
    return;

  item = ide_source_snippet_completion_item_new (snippet);
  state->list = g_list_append (state->list, item);
}

static void
provider_populate (GtkSourceCompletionProvider *provider,
                   GtkSourceCompletionContext  *context)
{
  SearchState state = { 0 };
  GtkTextIter iter;
  IdeSourceSnippetCompletionProvider *self = IDE_SOURCE_SNIPPET_COMPLETION_PROVIDER (provider);

  if (!self->snippets)
    {
      gtk_source_completion_context_add_proposals (context, provider, NULL, TRUE);
      return;
    }

  gtk_source_completion_context_get_iter (context, &iter);

  state.list = NULL;
  state.provider = provider;
  state.word = get_word (provider, &iter);

  if (state.word && *state.word)
    ide_source_snippets_foreach (self->snippets, state.word, foreach_snippet,
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
  IdeSourceSnippetCompletionProvider *self = (IdeSourceSnippetCompletionProvider *)provider;
  IdeSourceSnippetCompletionItem *item = (IdeSourceSnippetCompletionItem *)proposal;

  g_assert (IDE_IS_SOURCE_SNIPPET_COMPLETION_PROVIDER (self));
  g_assert (IDE_IS_SOURCE_SNIPPET_COMPLETION_ITEM (item));

  if (self->source_view)
    {
      IdeSourceSnippet *snippet;
      GtkTextBuffer *buffer;
      GtkTextIter begin;
      gchar *word;

      if (NULL == (snippet = ide_source_snippet_completion_item_get_snippet (item)))
        return FALSE;

      /*
       * Fetching the word will move us back to the beginning of it.
       */
      begin = *iter;
      word = get_word (provider, &begin);
      g_free (word);

      /*
       * Now delete the current word since it will get overwritten
       * by the insertion of the snippet.
       */
      buffer = gtk_text_iter_get_buffer (iter);
      gtk_text_buffer_delete (buffer, &begin, iter);

      /*
       * Now push snippet onto the snippet stack of the view.
       */
      snippet = ide_source_snippet_copy (snippet);
      ide_source_view_push_snippet (IDE_SOURCE_VIEW (self->source_view), snippet, NULL);
      g_object_unref (snippet);

      return TRUE;
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
  iface->match = ide_source_snippet_completion_provider_match;
}
