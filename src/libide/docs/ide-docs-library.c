/* ide-docs-library.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-docs-library"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-plugins.h>
#include <libide-threading.h>

#include "ide-docs-library.h"
#include "ide-docs-provider.h"

struct _IdeDocsLibrary
{
  IdeObject               parent_instance;
  IdeExtensionSetAdapter *providers;
};

typedef struct
{
  GCancellable *cancellable;
  IdeDocsQuery *query;
  IdeDocsItem  *results;
  guint         n_active;
} Search;

G_DEFINE_TYPE (IdeDocsLibrary, ide_docs_library, IDE_TYPE_OBJECT)

static void
search_free (Search *search)
{
  g_clear_object (&search->results);
  g_clear_object (&search->cancellable);
  g_clear_object (&search->query);
  g_slice_free (Search, search);
}

static void
ide_docs_library_init_provider_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  IdeDocsProvider *provider = (IdeDocsProvider *)object;
  g_autoptr(IdeDocsLibrary) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_DOCS_PROVIDER (provider));
  g_assert (G_IS_ASYNC_INITABLE (provider));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_DOCS_LIBRARY (self));

  if (!g_async_initable_init_finish (G_ASYNC_INITABLE (provider), result, &error))
    g_warning ("%s failed to initialize: %s", G_OBJECT_TYPE_NAME (provider), error->message);
}

static void
on_extension_added_cb (IdeExtensionSetAdapter *adapter,
                       PeasPluginInfo         *plugin,
                       PeasExtension          *exten,
                       gpointer                user_data)
{
  IdeDocsProvider *provider = (IdeDocsProvider *)exten;
  IdeDocsLibrary *self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (adapter));
  g_assert (plugin != NULL);
  g_assert (IDE_IS_DOCS_PROVIDER (provider));
  g_assert (IDE_IS_DOCS_LIBRARY (self));

  if (G_IS_INITABLE (provider) && !g_initable_init (G_INITABLE (provider), NULL, &error))
    g_warning ("%s failed to initialize: %s", G_OBJECT_TYPE_NAME (provider), error->message);
  else if (G_IS_ASYNC_INITABLE (provider))
    g_async_initable_init_async (G_ASYNC_INITABLE (provider),
                                 G_PRIORITY_DEFAULT,
                                 NULL,
                                 ide_docs_library_init_provider_cb,
                                 g_object_ref (self));
}

static void
ide_docs_library_parent_set (IdeObject *object,
                             IdeObject *parent)
{
  IdeDocsLibrary *self = (IdeDocsLibrary *)object;

  g_assert (IDE_IS_OBJECT (object));
  g_assert (!parent || IDE_IS_OBJECT (parent));

  if (parent == NULL)
    return;

  self->providers = ide_extension_set_adapter_new (object,
                                                   peas_engine_get_default (),
                                                   IDE_TYPE_DOCS_PROVIDER,
                                                   NULL, NULL);
  g_signal_connect (self->providers,
                    "extension-added",
                    G_CALLBACK (on_extension_added_cb),
                    self);
  ide_extension_set_adapter_foreach (self->providers, on_extension_added_cb, self);
}

static void
ide_docs_library_destroy (IdeObject *object)
{
  IdeDocsLibrary *self = (IdeDocsLibrary *)object;

  ide_clear_and_destroy_object (&self->providers);

  IDE_OBJECT_CLASS (ide_docs_library_parent_class)->destroy (object);
}

static void
ide_docs_library_class_init (IdeDocsLibraryClass *klass)
{
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  i_object_class->parent_set = ide_docs_library_parent_set;
  i_object_class->destroy = ide_docs_library_destroy;
}

static void
ide_docs_library_init (IdeDocsLibrary *self)
{
}

/**
 * ide_docs_library_from_context:
 *
 * Gets the #IdeDocsLibrary for the context.
 *
 * Returns: (transfer none): an #IdeDocsLibrary
 *
 * Since: 3.34
 */
IdeDocsLibrary *
ide_docs_library_from_context (IdeContext *context)
{
  g_autoptr(IdeDocsLibrary) ensured = NULL;

  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  if ((ensured = ide_object_ensure_child_typed (IDE_OBJECT (context), IDE_TYPE_DOCS_LIBRARY)))
    return ide_context_peek_child_typed (context, IDE_TYPE_DOCS_LIBRARY);

  return NULL;
}

static void
ide_docs_library_search_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  IdeDocsProvider *provider = (IdeDocsProvider *)object;
  g_autoptr(GListModel) model = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  Search *search;

  g_assert (IDE_IS_TASK (task));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_DOCS_PROVIDER (provider));

  search = ide_task_get_task_data (task);

  if (!ide_docs_provider_search_finish (provider, result, &error))
    {
      if (!ide_error_ignore (error))
        g_warning ("Search failed: %s: %s",
                   G_OBJECT_TYPE_NAME (provider), error->message);
    }

  search->n_active--;

  if (search->n_active == 0)
    {
      if (!ide_task_return_error_if_cancelled (task))
        ide_task_return_boolean (task, TRUE);
    }
}

static void
ide_docs_library_search_foreach_cb (IdeExtensionSetAdapter *adapter,
                                    PeasPluginInfo         *plugin,
                                    PeasExtension          *exten,
                                    gpointer                user_data)
{
  IdeDocsProvider *provider = (IdeDocsProvider *)exten;
  IdeTask *task = user_data;
  Search *search;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (adapter));
  g_assert (plugin != NULL);
  g_assert (IDE_IS_DOCS_PROVIDER (provider));
  g_assert (IDE_IS_TASK (task));

  search = ide_task_get_task_data (task);
  search->n_active++;

  ide_docs_provider_search_async (provider,
                                  search->query,
                                  search->results,
                                  search->cancellable,
                                  ide_docs_library_search_cb,
                                  g_object_ref (task));
}

/**
 * ide_docs_library_search_async:
 * @self: an #IdeDocsLibrary
 * @query: an #IdeDocsQuery
 * @results: an #IdeDocsItem to place results
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @callback: a #GAsyncReadyCallback to execute upon completion
 * @user_data: closure data for @callback
 *
 * Asynchronously queries the documentation providers for docs that
 * match @query.
 *
 * @callback should call ide_docs_library_search_finish() to obtain
 * the result.
 *
 * Since: 3.34
 */
void
ide_docs_library_search_async (IdeDocsLibrary      *self,
                               IdeDocsQuery        *query,
                               IdeDocsItem         *results,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  Search *search;
  static const struct {
    const gchar *id;
    const gchar *title;
  } default_groups[] = {
    { "api", N_("API") },
    { "tutorials", N_("Tutorials and Guides") },
    { "guidelines", N_("Guidelines") },
    { "other", N_("Other") },
  };

  g_return_if_fail (IDE_IS_DOCS_LIBRARY (self));
  g_return_if_fail (IDE_IS_DOCS_QUERY (query));
  g_return_if_fail (IDE_IS_DOCS_ITEM (results));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  search = g_slice_new0 (Search);
  search->results = g_object_ref (results);
  search->query = g_object_ref (query);
  search->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
  search->n_active = 0;

  for (guint i = 0; i < G_N_ELEMENTS (default_groups); i++)
    {
      const gchar *id = default_groups[i].id;
      const gchar *group = g_dgettext (GETTEXT_PACKAGE, default_groups[i].title);
      g_autoptr(IdeDocsItem) child = NULL;

      child = ide_docs_item_new ();
      ide_docs_item_set_id (child, id);
      ide_docs_item_set_title (child, group);
      ide_docs_item_append (results, child);
    }

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_docs_library_search_async);
  ide_task_set_task_data (task, search, search_free);

  ide_extension_set_adapter_foreach (self->providers,
                                     ide_docs_library_search_foreach_cb,
                                     task);

  if (search->n_active == 0)
    ide_task_return_boolean (task, TRUE);
}

/**
 * ide_docs_library_search_finish:
 * @self: an #IdeDocsLibrary
 * @result: a #GAsyncResult provided to callack
 * @error: a location for a #GError, or %NULL
 *
 * Completes a request to search the library.
 *
 * Since: 3.34
 */
gboolean
ide_docs_library_search_finish (IdeDocsLibrary  *self,
                                GAsyncResult    *result,
                                GError         **error)
{
  g_return_val_if_fail (IDE_IS_DOCS_LIBRARY (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}
