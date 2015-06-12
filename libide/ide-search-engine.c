/* ide-search-engine.c
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
#include <libpeas/peas.h>

#include "ide-internal.h"
#include "ide-search-context.h"
#include "ide-search-engine.h"
#include "ide-search-provider.h"
#include "ide-search-result.h"

struct _IdeSearchEngine
{
  IdeObject         parent_instance;

  PeasExtensionSet *extensions;
};

G_DEFINE_TYPE (IdeSearchEngine, ide_search_engine, IDE_TYPE_OBJECT)

static void
add_provider_to_context (PeasExtensionSet *extensions,
                         PeasPluginInfo   *plugin_info,
                         PeasExtension    *extension,
                         IdeSearchContext *search_context)
{
  g_assert (PEAS_IS_EXTENSION_SET (extensions));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_SEARCH_PROVIDER (extension));
  g_assert (IDE_IS_SEARCH_CONTEXT (search_context));

  _ide_search_context_add_provider (search_context, IDE_SEARCH_PROVIDER (extension), 0);
}

/**
 * ide_search_engine_search:
 * @search_terms: The search terms.
 *
 * Begins a query against the requested search providers.
 *
 * If @providers is %NULL, all registered providers will be used.
 *
 * Returns: (transfer full) (nullable): An #IdeSearchContext or %NULL if no
 *   providers could be loaded.
 */
IdeSearchContext *
ide_search_engine_search (IdeSearchEngine *self,
                          const gchar     *search_terms)
{
  IdeSearchContext *search_context;
  IdeContext *context;

  g_return_val_if_fail (IDE_IS_SEARCH_ENGINE (self), NULL);
  g_return_val_if_fail (search_terms, NULL);

  context = ide_object_get_context (IDE_OBJECT (self));
  search_context = g_object_new (IDE_TYPE_SEARCH_CONTEXT,
                                 "context", context,
                                 NULL);

  peas_extension_set_foreach (self->extensions,
                              (PeasExtensionSetForeachFunc)add_provider_to_context,
                              search_context);

  return search_context;
}

static void
ide_search_engine_constructed (GObject *object)
{
  IdeSearchEngine *self = (IdeSearchEngine *)object;
  IdeContext *context;

  context = ide_object_get_context (IDE_OBJECT (self));

  self->extensions = peas_extension_set_new (peas_engine_get_default (),
                                             IDE_TYPE_SEARCH_PROVIDER,
                                             "context", context,
                                             NULL);

  G_OBJECT_CLASS (ide_search_engine_parent_class)->constructed (object);
}

static void
ide_search_engine_dispose (GObject *object)
{
  IdeSearchEngine *self = (IdeSearchEngine *)object;

  g_clear_object (&self->extensions);

  G_OBJECT_CLASS (ide_search_engine_parent_class)->dispose (object);
}

static void
ide_search_engine_class_init (IdeSearchEngineClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_search_engine_constructed;
  object_class->dispose = ide_search_engine_dispose;
}

static void
ide_search_engine_init (IdeSearchEngine *self)
{
}
