/* gb-search-context.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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
#include <gio/gio.h>

#include "gb-search-context.h"
#include "gb-search-provider.h"
#include "gb-search-result.h"

struct _GbSearchContextPrivate
{
  GCancellable *cancellable;
  GList        *providers;
  guint         executed : 1;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbSearchContext, gb_search_context, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_PROVIDERS,
  LAST_PROP
};

enum {
  COUNT_SET,
  RESULT_ADDED,
  RESULT_REMOVED,
  LAST_SIGNAL
};

static GParamSpec *gParamSpecs [LAST_PROP];
static guint       gSignals [LAST_SIGNAL];

GbSearchContext *
gb_search_context_new (void)
{
  return g_object_new (GB_TYPE_SEARCH_CONTEXT, NULL);
}

const GList *
gb_search_context_get_providers (GbSearchContext *context)
{
  g_return_val_if_fail (GB_IS_SEARCH_CONTEXT (context), NULL);

  return context->priv->providers;
}

void
gb_search_context_add_result (GbSearchContext  *context,
                              GbSearchProvider *provider,
                              GbSearchResult   *result)
{
  g_return_if_fail (GB_IS_SEARCH_CONTEXT (context));
  g_return_if_fail (GB_IS_SEARCH_PROVIDER (provider));
  g_return_if_fail (GB_IS_SEARCH_RESULT (result));

  g_signal_emit (context, gSignals [RESULT_ADDED], 0, provider, result);
}

void
gb_search_context_remove_result (GbSearchContext  *context,
                                 GbSearchProvider *provider,
                                 GbSearchResult   *result)
{
  g_return_if_fail (GB_IS_SEARCH_CONTEXT (context));
  g_return_if_fail (GB_IS_SEARCH_PROVIDER (provider));
  g_return_if_fail (GB_IS_SEARCH_RESULT (result));

  g_signal_emit (context, gSignals [RESULT_REMOVED], 0, provider, result);
}

void
gb_search_context_set_provider_count (GbSearchContext  *context,
                                      GbSearchProvider *provider,
                                      guint64           count)
{
  g_return_if_fail (GB_IS_SEARCH_CONTEXT (context));
  g_return_if_fail (GB_IS_SEARCH_PROVIDER (provider));

  g_signal_emit (context, gSignals [COUNT_SET], 0, provider, count);
}

void
gb_search_context_execute (GbSearchContext *context,
                           const gchar     *search_terms)
{
  GList *iter;

  g_return_if_fail (GB_IS_SEARCH_CONTEXT (context));
  g_return_if_fail (!context->priv->executed);
  g_return_if_fail (search_terms);

  context->priv->executed = TRUE;

  for (iter = context->priv->providers; iter; iter = iter->next)
    {
      gsize max_results = 0;

      /* TODO: Get the max results for this provider */

      gb_search_provider_populate (iter->data,
                                   context,
                                   search_terms,
                                   max_results,
                                   context->priv->cancellable);
    }
}

void
gb_search_context_cancel (GbSearchContext *context)
{
  g_return_if_fail (GB_IS_SEARCH_CONTEXT (context));

  if (!g_cancellable_is_cancelled (context->priv->cancellable))
    g_cancellable_cancel (context->priv->cancellable);
}

void
gb_search_context_add_provider (GbSearchContext  *context,
                                GbSearchProvider *provider,
                                gsize             max_results)
{
  g_return_if_fail (GB_IS_SEARCH_CONTEXT (context));
  g_return_if_fail (GB_IS_SEARCH_PROVIDER (provider));
  g_return_if_fail (!context->priv->executed);

  context->priv->providers = g_list_append (context->priv->providers,
                                            g_object_ref (provider));
}

static void
gb_search_context_finalize (GObject *object)
{
  GbSearchContextPrivate *priv = GB_SEARCH_CONTEXT (object)->priv;

  g_clear_object (&priv->cancellable);

  g_list_foreach (priv->providers, (GFunc)g_object_unref, NULL);
  g_list_free (priv->providers);

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

  gSignals [COUNT_SET] =
    g_signal_new ("count-set",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE,
                  2,
                  GB_TYPE_SEARCH_PROVIDER,
                  G_TYPE_UINT64);

  gSignals [RESULT_ADDED] =
    g_signal_new ("result-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE,
                  2,
                  GB_TYPE_SEARCH_PROVIDER,
                  GB_TYPE_SEARCH_RESULT);

  gSignals [RESULT_REMOVED] =
    g_signal_new ("result-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE,
                  2,
                  GB_TYPE_SEARCH_PROVIDER,
                  GB_TYPE_SEARCH_RESULT);
}

static void
gb_search_context_init (GbSearchContext *self)
{
  self->priv = gb_search_context_get_instance_private (self);
  self->priv->cancellable = g_cancellable_new ();
}
