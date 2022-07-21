/* gbp-menu-search-provider.c
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

#define G_LOG_DOMAIN "gbp-menu-search-provider"

#include "config.h"

#include <libide-gui.h>
#include <libide-search.h>
#include <libide-sourceview.h>

#include "ide-application-private.h"

#include "gbp-menu-search-provider.h"
#include "gbp-menu-search-result.h"

struct _GbpMenuSearchProvider
{
  IdeObject parent_instance;
};

static GIcon *default_gicon;

static void
populate_from_menu_model (GPtrArray  *ar,
                          GMenuModel *menu,
                          const char *query)
{
  guint n_items;

  g_assert (ar != NULL);
  g_assert (G_IS_MENU_MODEL (menu));

  n_items = g_menu_model_get_n_items (menu);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(IdeSearchResult) result = NULL;
      g_autoptr(GVariant) target = NULL;
      g_autofree char *label = NULL;
      g_autofree char *icon_name = NULL;
      g_autofree char *description = NULL;
      g_autofree char *action = NULL;
      guint label_prio = 0;
      guint desc_prio = 0;

      if (!g_menu_model_get_item_attribute (menu, i, "label", "s", &label) ||
          !g_menu_model_get_item_attribute (menu, i, "action", "s", &action) ||
          !g_menu_model_get_item_attribute (menu, i, "description", "s", &description))
        continue;

      if (strcasestr (label, query) == NULL &&
          strcasestr (description, query) == NULL)
        continue;

      gtk_source_completion_fuzzy_match (label, query, &label_prio);
      gtk_source_completion_fuzzy_match (description, query, &desc_prio);

      result = g_object_new (GBP_TYPE_MENU_SEARCH_RESULT,
                             "title", label,
                             "subtitle", description,
                             "priority", MAX (label_prio, desc_prio),
                             NULL);

      if (g_menu_model_get_item_attribute (menu, i, "verb-icon", "s", &icon_name))
        {
          g_autoptr(GIcon) icon = g_themed_icon_new (icon_name);
          ide_search_result_set_gicon (result, icon);
        }
      else
        {
          ide_search_result_set_gicon (result, default_gicon);
        }

      target = g_menu_model_get_item_attribute_value (menu, i, "target", NULL);
      gbp_menu_search_result_set_action (GBP_MENU_SEARCH_RESULT (result), action, target);

      g_ptr_array_add (ar, g_steal_pointer (&result));
    }
}

static int
sort_results (gconstpointer a,
              gconstpointer b)
{
  const IdeSearchResult * const *ra = a;
  const IdeSearchResult * const *rb = b;

  return ide_search_result_compare (*ra, *rb);
}

static void
gbp_menu_search_provider_search_async (IdeSearchProvider   *provider,
                                       const char          *query,
                                       guint                max_results,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GListStore) store = NULL;
  g_autoptr(GPtrArray) ar = NULL;
  const char * const *menu_ids;
  IdeApplication *app;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_MENU_SEARCH_PROVIDER (provider));
  g_assert (query != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (provider, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_menu_search_provider_search_async);

  app = IDE_APPLICATION_DEFAULT;
  menu_ids = ide_menu_manager_get_menu_ids (app->menu_manager);
  store = g_list_store_new (IDE_TYPE_SEARCH_RESULT);
  ar = g_ptr_array_new_with_free_func (g_object_unref);

  for (guint i = 0; menu_ids[i]; i++)
    {
      GMenu *menu = ide_menu_manager_get_menu_by_id (app->menu_manager, menu_ids[i]);

      populate_from_menu_model (ar, G_MENU_MODEL (menu), query);
    }

  g_ptr_array_sort (ar, sort_results);
  g_list_store_splice (store, 0, 0, ar->pdata, ar->len);

  ide_task_return_pointer (task, g_steal_pointer (&store), g_object_unref);

  IDE_EXIT;
}

static GListModel *
gbp_menu_search_provider_search_finish (IdeSearchProvider  *provider,
                                        GAsyncResult       *result,
                                        gboolean           *truncated,
                                        GError            **error)
{
  GListModel *ret;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_MENU_SEARCH_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
search_provider_iface_init (IdeSearchProviderInterface *iface)
{
  iface->search_async = gbp_menu_search_provider_search_async;
  iface->search_finish = gbp_menu_search_provider_search_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpMenuSearchProvider, gbp_menu_search_provider, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_SEARCH_PROVIDER, search_provider_iface_init))

static void
gbp_menu_search_provider_class_init (GbpMenuSearchProviderClass *klass)
{
  default_gicon = g_themed_icon_new ("preferences-desktop-keyboard-shortcuts-symbolic");
}

static void
gbp_menu_search_provider_init (GbpMenuSearchProvider *self)
{
}
