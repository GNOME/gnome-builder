/* gbp-editorui-search-provider.c
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-editorui-search-provider"

#include "config.h"

#include <libide-gui.h>
#include <libide-search.h>
#include <libide-threading.h>

#include "gbp-editorui-search-provider.h"
#include "gbp-editorui-search-result.h"

struct _GbpEditoruiSearchProvider
{
  IdeObject           parent_instance;
  GListStore         *all;
  GtkFilterListModel *filter_model;
  GtkStringFilter    *filter;
};

static void
gbp_editorui_search_provider_search_async (IdeSearchProvider   *provider,
                                           const char          *query,
                                           guint                max_results,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data)
{
  GbpEditoruiSearchProvider *self = (GbpEditoruiSearchProvider *)provider;
  g_autoptr(IdeTask) task = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_EDITORUI_SEARCH_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (provider, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_editorui_search_provider_search_async);

  gtk_string_filter_set_search (self->filter, query);

  ide_task_return_pointer (task, g_object_ref (self->filter_model), g_object_unref);
}

static GListModel *
gbp_editorui_search_provider_search_finish (IdeSearchProvider  *provider,
                                            GAsyncResult       *result,
                                            gboolean           *was_truncated,
                                            GError            **error)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_EDITORUI_SEARCH_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  *was_truncated = FALSE;

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static IdeSearchCategory
gbp_editorui_search_provider_get_category (IdeSearchProvider *provider)
{
  return IDE_SEARCH_CATEGORY_ACTIONS;
}

static void
gbp_editorui_search_provider_notify_scheme_ids (GtkSourceStyleSchemeManager *manager,
                                                GParamSpec                  *pspec,
                                                GListStore                  *store)
{
  const char * const *scheme_ids;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GTK_SOURCE_IS_STYLE_SCHEME_MANAGER (manager));
  g_assert (G_IS_LIST_STORE (store));

  g_list_store_remove_all (store);

  scheme_ids = gtk_source_style_scheme_manager_get_scheme_ids (manager);

  for (guint i = 0; scheme_ids[i]; i++)
    {
      const char *scheme_id = scheme_ids[i];
      GtkSourceStyleScheme *scheme = gtk_source_style_scheme_manager_get_scheme (manager, scheme_id);
      g_autoptr(IdeSearchResult) result = gbp_editorui_search_result_new (scheme);

      g_list_store_append (store, result);
    }
}

static void
gbp_editorui_search_provider_load (IdeSearchProvider *provider)
{
  GbpEditoruiSearchProvider *self = (GbpEditoruiSearchProvider *)provider;
  GtkSourceStyleSchemeManager *manager = gtk_source_style_scheme_manager_get_default ();
  g_autoptr(GtkExpression) expression = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_EDITORUI_SEARCH_PROVIDER (self));

  expression = gtk_property_expression_new (GBP_TYPE_EDITORUI_SEARCH_RESULT, NULL, "name");

  self->all = g_list_store_new (IDE_TYPE_SEARCH_RESULT);
  self->filter = g_object_new (GTK_TYPE_STRING_FILTER,
                               "expression", expression,
                               "ignore-case", TRUE,
                               "match-mode", GTK_STRING_FILTER_MATCH_MODE_SUBSTRING,
                               NULL);
  self->filter_model = gtk_filter_list_model_new (g_object_ref (G_LIST_MODEL (self->all)),
                                                  g_object_ref (GTK_FILTER (self->filter)));

  g_signal_connect_object (manager,
                           "notify::scheme-ids",
                           G_CALLBACK (gbp_editorui_search_provider_notify_scheme_ids),
                           self->all,
                           0);
  gbp_editorui_search_provider_notify_scheme_ids (manager, NULL, self->all);
}

static void
gbp_editorui_search_provider_unload (IdeSearchProvider *provider)
{
  GbpEditoruiSearchProvider *self = (GbpEditoruiSearchProvider *)provider;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_EDITORUI_SEARCH_PROVIDER (self));

  g_clear_object (&self->all);
  g_clear_object (&self->filter);
  g_clear_object (&self->filter_model);
}

static void
search_provider_iface_init (IdeSearchProviderInterface *iface)
{
  iface->load = gbp_editorui_search_provider_load;
  iface->unload = gbp_editorui_search_provider_unload;
  iface->search_async = gbp_editorui_search_provider_search_async;
  iface->search_finish = gbp_editorui_search_provider_search_finish;
  iface->get_category = gbp_editorui_search_provider_get_category;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpEditoruiSearchProvider, gbp_editorui_search_provider, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_SEARCH_PROVIDER, search_provider_iface_init))

static void
gbp_editorui_search_provider_class_init (GbpEditoruiSearchProviderClass *klass)
{
}

static void
gbp_editorui_search_provider_init (GbpEditoruiSearchProvider *self)
{
}
