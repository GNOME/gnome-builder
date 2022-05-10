/* gbp-buildui-runtime-categories.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-buildui-runtime-categories"

#include "config.h"

#include <string.h>

#include "gbp-buildui-runtime-categories.h"

struct _GbpBuilduiRuntimeCategories
{
  GObject            parent_instance;
  GPtrArray         *items;
  gchar             *prefix;
  gchar             *name;
  IdeRuntimeManager *runtime_manager;
};

static GType
gbp_buildui_runtime_categories_get_item_type (GListModel *model)
{
  return G_TYPE_OBJECT;
}

static guint
gbp_buildui_runtime_categories_get_n_items (GListModel *model)
{
  GbpBuilduiRuntimeCategories *self = (GbpBuilduiRuntimeCategories *)model;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDUI_RUNTIME_CATEGORIES (self));

  return self->items->len;
}

static gpointer
gbp_buildui_runtime_categories_get_item (GListModel *model,
                                         guint       position)
{
  GbpBuilduiRuntimeCategories *self = GBP_BUILDUI_RUNTIME_CATEGORIES (model);

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDUI_RUNTIME_CATEGORIES (self));

  if (self->items->len > position)
    {
      const gchar *category = g_ptr_array_index (self->items, position);
      return gbp_buildui_runtime_categories_create_child_model (self, category);
    }

  return NULL;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = gbp_buildui_runtime_categories_get_item_type;
  iface->get_n_items = gbp_buildui_runtime_categories_get_n_items;
  iface->get_item = gbp_buildui_runtime_categories_get_item;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpBuilduiRuntimeCategories, gbp_buildui_runtime_categories, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static void
gbp_buildui_runtime_categories_finalize (GObject *object)
{
  GbpBuilduiRuntimeCategories *self = (GbpBuilduiRuntimeCategories *)object;

  g_clear_object (&self->runtime_manager);
  g_clear_pointer (&self->items, g_ptr_array_unref);
  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->prefix, g_free);

  G_OBJECT_CLASS (gbp_buildui_runtime_categories_parent_class)->finalize (object);
}

static void
gbp_buildui_runtime_categories_class_init (GbpBuilduiRuntimeCategoriesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_buildui_runtime_categories_finalize;
}

static void
gbp_buildui_runtime_categories_init (GbpBuilduiRuntimeCategories *self)
{
  self->items = g_ptr_array_new_with_free_func (g_free);
}

static gint
sort_by_name (const gchar **name_a,
              const gchar **name_b)
{
  return g_strcmp0 (*name_a, *name_b);
}

static void
on_items_changed_cb (GbpBuilduiRuntimeCategories *self,
                     guint                        position,
                     guint                        added,
                     guint                        removed,
                     IdeRuntimeManager           *runtime_manager)
{
  g_autoptr(GHashTable) found = NULL;
  guint old_len;
  guint n_items;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDUI_RUNTIME_CATEGORIES (self));
  g_assert (IDE_IS_RUNTIME_MANAGER (runtime_manager));

  old_len = self->items->len;

  if (old_len > 0)
    g_ptr_array_remove_range (self->items, 0, old_len);

  found = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  n_items = g_list_model_get_n_items (G_LIST_MODEL (runtime_manager));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(IdeRuntime) runtime = g_list_model_get_item (G_LIST_MODEL (runtime_manager), i);
      g_autofree gchar *word = NULL;
      const gchar *category = ide_runtime_get_category (runtime);
      const gchar *next = category;
      const gchar *slash;

      if (self->prefix != NULL && !g_str_has_prefix (category, self->prefix))
        continue;

      if (self->prefix)
        next += strlen (self->prefix);

      if ((slash = strchr (next, '/')))
        next = word = g_strndup (next, slash + 1 - next);

      if (!g_hash_table_contains (found, next))
        {
          g_hash_table_insert (found, g_strdup (next), NULL);
          g_ptr_array_add (self->items, g_strdup (next));
        }
    }

  g_ptr_array_sort (self->items, (GCompareFunc)sort_by_name);

  g_list_model_items_changed (G_LIST_MODEL (self), 0, old_len, self->items->len);
}

GbpBuilduiRuntimeCategories *
gbp_buildui_runtime_categories_new (IdeRuntimeManager *runtime_manager,
                                    const gchar       *prefix)
{
  GbpBuilduiRuntimeCategories *ret;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_RUNTIME_MANAGER (runtime_manager), NULL);

  ret = g_object_new (GBP_TYPE_BUILDUI_RUNTIME_CATEGORIES, NULL);
  ret->runtime_manager = g_object_ref (runtime_manager);
  ret->prefix = g_strdup (prefix);
  ret->name = prefix ? g_path_get_basename (prefix) : NULL;

  g_signal_connect_object (runtime_manager,
                           "items-changed",
                           G_CALLBACK (on_items_changed_cb),
                           ret,
                           G_CONNECT_SWAPPED);

  on_items_changed_cb (ret, 0, 0, 0, runtime_manager);

  return g_steal_pointer (&ret);
}

const gchar *
gbp_buildui_runtime_categories_get_name (GbpBuilduiRuntimeCategories *self)
{
  g_return_val_if_fail (GBP_IS_BUILDUI_RUNTIME_CATEGORIES (self), NULL);

  return self->name;
}

const gchar *
gbp_buildui_runtime_categories_get_prefix (GbpBuilduiRuntimeCategories *self)
{
  g_return_val_if_fail (GBP_IS_BUILDUI_RUNTIME_CATEGORIES (self), NULL);

  return self->prefix;
}

GListModel *
gbp_buildui_runtime_categories_create_child_model (GbpBuilduiRuntimeCategories *self,
                                                   const gchar                 *category)
{
  g_autofree gchar *prefix = NULL;
  g_autofree gchar *name = NULL;
  GtkFilterListModel *filter_model;
  GtkStringFilter *filter;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDUI_RUNTIME_CATEGORIES (self));
  g_assert (category != NULL);

  if (self->prefix == NULL)
    prefix = g_strdup (category);
  else
    prefix = g_strdup_printf ("%s%s", self->prefix, category);

  name = g_path_get_basename (prefix);

  if (g_str_has_suffix (category, "/"))
    return G_LIST_MODEL (gbp_buildui_runtime_categories_new (self->runtime_manager, prefix));

  filter = gtk_string_filter_new (gtk_property_expression_new (IDE_TYPE_RUNTIME, NULL, "caregory"));
  filter_model = gtk_filter_list_model_new (G_LIST_MODEL (self->runtime_manager), GTK_FILTER (filter));
  g_object_set_data_full (G_OBJECT (filter), "CATEGORY", g_steal_pointer (&name), g_free);

  return G_LIST_MODEL (g_steal_pointer (&filter_model));
}
