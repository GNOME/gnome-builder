/* gb-search-context.c
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

#define G_LOG_DOMAIN "search-context"

#include <glib/gi18n.h>

#include "gb-log.h"
#include "gb-search-context.h"
#include "gb-search-provider.h"
#include "gb-search-result.h"

struct _GbSearchContextPrivate
{
  GCancellable *cancellable;
  GList        *providers;
  gchar        *search_text;
  GList        *results;
  guint         executed : 1;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbSearchContext, gb_search_context, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_PROVIDERS,
  PROP_SEARCH_TEXT,
  LAST_PROP
};

enum {
  RESULTS_ADDED,
  LAST_SIGNAL
};

static guint       gSignals [LAST_SIGNAL];
static GParamSpec *gParamSpecs [LAST_PROP];

/**
 * gb_search_context_new:
 * @providers: (element-type GbSearchProvider*) (transfer none): A #GList
 *
 * Creates a new search context with the provided search providers.
 *
 * Returns: (transfer full): A newly allocated #GbSearchContext.
 */
GbSearchContext *
gb_search_context_new (const GList *providers,
                       const gchar *search_text)
{
  return g_object_new (GB_TYPE_SEARCH_CONTEXT,
                       "providers", providers,
                       "search-text", search_text,
                       NULL);
}

void
gb_search_context_execute (GbSearchContext *context)
{
  GbSearchContextPrivate *priv;
  GList *iter;

  g_return_if_fail (GB_IS_SEARCH_CONTEXT (context));

  priv = context->priv;

  if (priv->executed)
    {
      g_warning ("GbSearchContext has already been executed.");
      return;
    }

  priv->executed = 1;

  for (iter = priv->providers; iter; iter = iter->next)
    gb_search_provider_populate (iter->data, context, priv->cancellable);
}

/**
 * gb_search_context_get_cancellable:
 * @context: A #GbSearchContext
 *
 * Retrieves the cancellable to cancel the search request. If the search has
 * completed, this will return NULL.
 *
 * Returns: (transfer none): A #GCancellable or %NULL.
 */
GCancellable *
gb_search_context_get_cancellable (GbSearchContext *context)
{
  g_return_val_if_fail (GB_IS_SEARCH_CONTEXT (context), NULL);

  return context->priv->cancellable;
}

void
gb_search_context_cancel (GbSearchContext *context)
{
  g_return_if_fail (GB_IS_SEARCH_CONTEXT (context));

  g_cancellable_cancel (context->priv->cancellable);
}

/**
 * gb_search_context_get_results:
 * @context: A #GbSearchContext
 *
 * Fetches the current results.
 *
 * Returns: (transfer none): A #GList of current results.
 */
const GList *
gb_search_context_get_results (GbSearchContext *context)
{
  g_return_val_if_fail (GB_IS_SEARCH_CONTEXT (context), NULL);

  return context->priv->results;
}

static void
gb_search_context_results_added (GbSearchContext  *context,
                                 GbSearchProvider *provider,
                                 GList            *results,
                                 gboolean          finished)
{
  ENTRY;

  g_return_if_fail (GB_IS_SEARCH_CONTEXT (context));
  g_return_if_fail (GB_IS_SEARCH_PROVIDER (provider));

  /* TODO: how should we deal with priority? */

  context->priv->results = g_list_concat (context->priv->results, results);

  EXIT;
}

/**
 * gb_search_context_add_results:
 * @results: (transfer full) (element-type GbSearchResult*): A #GList or %NULL
 * @finished: if the provider is finished adding results.
 *
 * This function will add a list of results to the context. Ownership of
 * @results and the contained elements will be transfered to @context.
 */
void
gb_search_context_add_results (GbSearchContext  *context,
                               GbSearchProvider *provider,
                               GList            *results,
                               gboolean          finished)
{
  ENTRY;

  g_return_if_fail (GB_IS_SEARCH_CONTEXT (context));
  g_return_if_fail (GB_IS_SEARCH_PROVIDER (provider));

  g_list_foreach (results, (GFunc)g_object_ref_sink, NULL);

  g_signal_emit (context, gSignals [RESULTS_ADDED], 0,
                 provider, results, finished);

  EXIT;
}

const gchar *
gb_search_context_get_search_text (GbSearchContext *context)
{
  g_return_val_if_fail (GB_IS_SEARCH_CONTEXT (context), NULL);

  return context->priv->search_text;
}

static void
gb_search_context_set_search_text (GbSearchContext *context,
                                   const gchar     *search_text)
{
  g_return_if_fail (GB_IS_SEARCH_CONTEXT (context));
  g_return_if_fail (search_text);

  if (search_text != context->priv->search_text)
    {
      g_free (context->priv->search_text);
      context->priv->search_text = g_strdup (search_text);
      g_object_notify_by_pspec (G_OBJECT (context),
                                gParamSpecs [PROP_SEARCH_TEXT]);
    }
}

static void
gb_search_context_set_providers (GbSearchContext *context,
                                 const GList     *providers)
{
  GbSearchContextPrivate *priv;

  g_return_if_fail (GB_IS_SEARCH_CONTEXT (context));

  priv = context->priv;

  g_list_foreach (priv->providers, (GFunc)g_object_unref, NULL);
  g_list_free (priv->providers);

  priv->providers = g_list_copy ((GList *)providers);
  g_list_foreach (priv->providers, (GFunc)g_object_ref, NULL);
}

static void
gb_search_context_finalize (GObject *object)
{
  GbSearchContextPrivate *priv = GB_SEARCH_CONTEXT (object)->priv;

  g_list_foreach (priv->providers, (GFunc)g_object_unref, NULL);
  g_clear_pointer (&priv->providers, g_list_free);

  g_list_foreach (priv->results, (GFunc)g_object_unref, NULL);
  g_clear_pointer (&priv->results, g_list_free);

  g_clear_pointer (&priv->search_text, g_free);

  G_OBJECT_CLASS (gb_search_context_parent_class)->finalize (object);
}

static void
gb_search_context_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GbSearchContext *self = GB_SEARCH_CONTEXT (object);

  switch (prop_id)
    {
    case PROP_SEARCH_TEXT:
      g_value_set_string (value, gb_search_context_get_search_text (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_search_context_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GbSearchContext *self = GB_SEARCH_CONTEXT (object);

  switch (prop_id)
    {
    case PROP_PROVIDERS:
      gb_search_context_set_providers (self, g_value_get_pointer (value));
      break;

    case PROP_SEARCH_TEXT:
      gb_search_context_set_search_text (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_search_context_class_init (GbSearchContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gb_search_context_finalize;
  object_class->get_property = gb_search_context_get_property;
  object_class->set_property = gb_search_context_set_property;

  klass->results_added = gb_search_context_results_added;

  gParamSpecs [PROP_SEARCH_TEXT] =
    g_param_spec_string ("search-text",
                         _("Search Text"),
                         _("The search text for the context."),
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SEARCH_TEXT,
                                   gParamSpecs [PROP_SEARCH_TEXT]);

  gParamSpecs [PROP_PROVIDERS] =
    g_param_spec_pointer ("providers",
                          _("Providers"),
                          _("The providers for the search context."),
                          (G_PARAM_READWRITE |
                           G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_PROVIDERS,
                                   gParamSpecs [PROP_PROVIDERS]);

  gSignals [RESULTS_ADDED] =
    g_signal_new ("results-added",
                  GB_TYPE_SEARCH_CONTEXT,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE,
                  3,
                  GB_TYPE_SEARCH_PROVIDER,
                  G_TYPE_POINTER,
                  G_TYPE_BOOLEAN);
}

static void
gb_search_context_init (GbSearchContext *self)
{
  ENTRY;
  self->priv = gb_search_context_get_instance_private (self);
  EXIT;
}
