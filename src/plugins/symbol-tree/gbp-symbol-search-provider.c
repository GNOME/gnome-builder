/* gbp-symbol-search-provider.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-symbol-search-provider"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-gui.h>
#include <libide-search.h>
#include <libide-threading.h>

#include "gbp-symbol-search-provider.h"
#include "gbp-symbol-search-result.h"
#include "gbp-symbol-workspace-addin.h"

struct _GbpSymbolSearchProvider
{
  IdeObject parent_instance;
};

static gpointer
gbp_symbol_search_provider_map_func (gpointer item,
                                     gpointer user_data)
{
  g_autoptr(GtkTreeListRow) row = item;
  g_autoptr(IdeSymbolNode) node = NULL;
  GFile *file = user_data;

  g_assert (GTK_IS_TREE_LIST_ROW (item));
  g_assert (!file || G_IS_FILE (file));

  node = gtk_tree_list_row_get_item (row);
  g_assert (IDE_IS_SYMBOL_NODE (node));

  return gbp_symbol_search_result_new (node, file);
}

static void
gbp_symbol_search_provider_foreach_workspace_cb (IdeWorkspace *workspace,
                                                 gpointer      user_data)
{
  g_autoptr(GtkMapListModel) map = NULL;
  GtkFilterListModel *filtered;
  IdeWorkspaceAddin *addin;
  GtkStringFilter *filter;
  GListStore *store = user_data;
  GListModel *model;
  IdeBuffer *buffer;
  GFile *file = NULL;

  g_assert (IDE_IS_WORKSPACE (workspace));
  g_assert (G_IS_LIST_STORE (user_data));

  if (!(addin = ide_workspace_addin_find_by_module_name (workspace, "symbol-tree")) ||
      !(model = gbp_symbol_workspace_addin_get_model (GBP_SYMBOL_WORKSPACE_ADDIN (addin))))
    return;

  if ((buffer = gbp_symbol_workspace_addin_get_buffer (GBP_SYMBOL_WORKSPACE_ADDIN (addin))))
    file = g_object_ref (ide_buffer_get_file (buffer));

  filter = gtk_string_filter_new (gtk_property_expression_new (IDE_TYPE_SYMBOL_NODE,
                                                               gtk_property_expression_new (GTK_TYPE_TREE_LIST_ROW, NULL, "item"),
                                                               "display-name"));
  gtk_string_filter_set_search (filter, g_object_get_data (G_OBJECT (store), "SEARCH"));
  gtk_string_filter_set_ignore_case (filter, TRUE);
  gtk_string_filter_set_match_mode (filter, GTK_STRING_FILTER_MATCH_MODE_SUBSTRING);
  filtered = gtk_filter_list_model_new (g_object_ref (model), GTK_FILTER (filter));

  map = gtk_map_list_model_new (G_LIST_MODEL (filtered),
                                gbp_symbol_search_provider_map_func,
                                file,
                                file ? g_object_unref : NULL);

  g_list_store_append (store, map);
}

static void
gbp_symbol_search_provider_search_async (IdeSearchProvider   *provider,
                                         const char          *query,
                                         guint                max_results,
                                         GCancellable        *cancellable,
                                         GAsyncReadyCallback  callback,
                                         gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GListStore) store = NULL;
  IdeWorkbench *workbench;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYMBOL_SEARCH_PROVIDER (provider));
  g_assert (query != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (provider, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_symbol_search_provider_search_async);

  context = ide_object_get_context (IDE_OBJECT (provider));
  workbench = ide_workbench_from_context (context);
  store = g_list_store_new (G_TYPE_LIST_MODEL);

  g_object_set_data_full (G_OBJECT (store),
                          "SEARCH",
                          g_strdup (query),
                          g_free);

  ide_workbench_foreach_workspace (workbench,
                                   gbp_symbol_search_provider_foreach_workspace_cb,
                                   store);

  if (g_list_model_get_n_items (G_LIST_MODEL (store)) == 0)
    ide_task_return_unsupported_error (task);
  else
    ide_task_return_pointer (task,
                             g_object_new (GTK_TYPE_FLATTEN_LIST_MODEL,
                                           "model", store,
                                           NULL),
                             g_object_unref);
}

static GListModel *
gbp_symbol_search_provider_search_finish (IdeSearchProvider  *provider,
                                          GAsyncResult       *result,
                                          gboolean           *truncated,
                                          GError            **error)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYMBOL_SEARCH_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static char *
gbp_symbol_search_provider_dup_title (IdeSearchProvider *provider)
{
  return g_strdup (_("Symbols in File"));
}

static GIcon *
gbp_symbol_search_provider_dup_icon (IdeSearchProvider *provider)
{
  return g_themed_icon_new ("lang-function-symbolic");
}

static IdeSearchCategory
gbp_symbol_search_provider_get_category (IdeSearchProvider *provider)
{
  return IDE_SEARCH_CATEGORY_SYMBOLS;
}

static void
search_provider_iface_init (IdeSearchProviderInterface *iface)
{
  iface->search_async = gbp_symbol_search_provider_search_async;
  iface->search_finish = gbp_symbol_search_provider_search_finish;
  iface->dup_title = gbp_symbol_search_provider_dup_title;
  iface->dup_icon = gbp_symbol_search_provider_dup_icon;
  iface->get_category = gbp_symbol_search_provider_get_category;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpSymbolSearchProvider, gbp_symbol_search_provider, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_SEARCH_PROVIDER, search_provider_iface_init))

static void
gbp_symbol_search_provider_class_init (GbpSymbolSearchProviderClass *klass)
{
}

static void
gbp_symbol_search_provider_init (GbpSymbolSearchProvider *self)
{
}
