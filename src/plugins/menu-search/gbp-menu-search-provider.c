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

#include <glib/gi18n.h>

#include <libide-gui.h>
#include <libide-search.h>
#include <libide-sourceview.h>

#include "ide-application-private.h"
#include "ide-shortcut-manager-private.h"

#include "gbp-menu-search-provider.h"
#include "gbp-menu-search-result.h"

struct _GbpMenuSearchProvider
{
  IdeObject  parent_instance;
  GPtrArray *items;
};

static void
populate_shortcut_info_cb (const IdeShortcutInfo *info,
                           gpointer               user_data)
{
  GPtrArray *items = user_data;
  GbpMenuSearchResult *result;
  g_autoptr(GIcon) icon = NULL;
  const char *icon_name;

  if ((icon_name = ide_shortcut_info_get_icon_name (info)))
    icon = g_themed_icon_new (icon_name);

  result = g_object_new (GBP_TYPE_MENU_SEARCH_RESULT,
                         "accelerator", ide_shortcut_info_get_accelerator (info),
                         "title", ide_shortcut_info_get_title (info),
                         "subtitle", ide_shortcut_info_get_subtitle (info),
                         "gicon", icon,
                         NULL);
  gbp_menu_search_result_set_action (result,
                                     ide_shortcut_info_get_action_name (info),
                                     ide_shortcut_info_get_action_target (info));
  g_ptr_array_add (items, g_steal_pointer (&result));
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
gbp_menu_search_provider_search_worker (IdeTask      *task,
                                        gpointer      source_object,
                                        gpointer      task_data,
                                        GCancellable *cancellable)
{
  g_autoptr(IdeSearchResultClass) klass = NULL;
  GbpMenuSearchProvider *self = source_object;
  g_autoptr(GListStore) store = NULL;
  g_autoptr(GPtrArray) matches = NULL;
  const char *query = task_data;

  IDE_ENTRY;

  g_assert (!IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_MENU_SEARCH_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  store = g_list_store_new (IDE_TYPE_SEARCH_RESULT);

  if (query == NULL)
    {
      if (self->items->len > 0)
        g_list_store_splice (store, 0, 0, self->items->pdata, self->items->len);
      ide_task_return_pointer (task, g_steal_pointer (&store), g_object_unref);
      IDE_EXIT;
    }

  klass = g_type_class_ref (GBP_TYPE_MENU_SEARCH_RESULT);
  matches = g_ptr_array_new ();

  for (guint i = 0; i < self->items->len; i++)
    {
      IdeSearchResult *result = g_ptr_array_index (self->items, i);

      if (klass->matches (result, query))
        g_ptr_array_add (matches, result);
    }

  g_ptr_array_sort (matches, sort_results);

  if (matches->len > 0)
    g_list_store_splice (store, 0, 0, matches->pdata, matches->len);
  ide_task_return_pointer (task, g_steal_pointer (&store), g_object_unref);

  IDE_EXIT;
}

static void
gbp_menu_search_provider_search_async (IdeSearchProvider   *provider,
                                       const char          *query,
                                       guint                max_results,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  GbpMenuSearchProvider *self = (GbpMenuSearchProvider *)provider;
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_MENU_SEARCH_PROVIDER (self));
  g_assert (query != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (provider, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_menu_search_provider_search_async);

  if (self->items->len == 0)
    {
      IdeContext *context = ide_object_get_context (IDE_OBJECT (self));
      IdeShortcutManager *shortcut_manager = ide_shortcut_manager_from_context (context);

      ide_shortcut_info_foreach (G_LIST_MODEL (shortcut_manager),
                                 populate_shortcut_info_cb,
                                 self->items);

      g_ptr_array_sort (self->items, sort_results);
    }

  if (query != NULL)
    ide_task_set_task_data (task,
                            g_utf8_casefold (query, -1),
                            g_free);

  ide_task_run_in_thread (task, gbp_menu_search_provider_search_worker);

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

static char *
gbp_menu_search_provider_dup_title (IdeSearchProvider *provider)
{
  return g_strdup (_("Actions"));
}

static GIcon *
gbp_menu_search_provider_dup_icon (IdeSearchProvider *provider)
{
  return g_themed_icon_new ("builder-keyboard-shortcuts-symbolic");
}

static IdeSearchCategory
gbp_menu_search_provider_get_category (IdeSearchProvider *provider)
{
  return IDE_SEARCH_CATEGORY_ACTIONS;
}

static void
search_provider_iface_init (IdeSearchProviderInterface *iface)
{
  iface->search_async = gbp_menu_search_provider_search_async;
  iface->search_finish = gbp_menu_search_provider_search_finish;
  iface->dup_title = gbp_menu_search_provider_dup_title;
  iface->dup_icon = gbp_menu_search_provider_dup_icon;
  iface->get_category = gbp_menu_search_provider_get_category;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpMenuSearchProvider, gbp_menu_search_provider, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_SEARCH_PROVIDER, search_provider_iface_init))

static void
gbp_menu_search_provider_destroy (IdeObject *object)
{
  GbpMenuSearchProvider *self = (GbpMenuSearchProvider *)object;

  g_clear_pointer (&self->items, g_ptr_array_unref);

  IDE_OBJECT_CLASS (gbp_menu_search_provider_parent_class)->destroy (object);
}

static void
gbp_menu_search_provider_class_init (GbpMenuSearchProviderClass *klass)
{
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  i_object_class->destroy = gbp_menu_search_provider_destroy;
}

static void
gbp_menu_search_provider_init (GbpMenuSearchProvider *self)
{
  self->items = g_ptr_array_new_with_free_func (g_object_unref);
}
