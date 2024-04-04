/*
 * gbp-manuals-tree-addin.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#include "gbp-manuals-tree-addin.h"

#include "manuals-book.h"
#include "manuals-heading.h"
#include "manuals-repository.h"
#include "manuals-sdk.h"

struct _GbpManualsTreeAddin
{
  GObject parent_instance;
};

static DexFuture *
apply_children_posible (DexFuture *completed,
                        gpointer   user_data)
{
  IdeTreeNode *node = user_data;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (dex_future_is_resolved (completed));
  g_assert (IDE_IS_TREE_NODE (node));

  ide_tree_node_set_children_possible (node, dex_await_boolean (dex_ref (completed), NULL));

  return NULL;
}

static void
gbp_manuals_tree_addin_build_node (IdeTreeAddin *addin,
                                   IdeTreeNode  *node)
{
  g_assert (GBP_IS_MANUALS_TREE_ADDIN (addin));
  g_assert (IDE_IS_TREE_NODE (node));

  if (ide_tree_node_holds (node, MANUALS_TYPE_SDK))
    {
      ManualsSdk *sdk = ide_tree_node_get_item (node);
      g_autofree char *title = manuals_sdk_dup_title (sdk);
      const char *icon_name = manuals_sdk_get_icon_name (sdk);

      ide_tree_node_set_title (node, title);
      ide_tree_node_set_icon_name (node, icon_name);
      ide_tree_node_set_children_possible (node, TRUE);
    }
  else if (ide_tree_node_holds (node, MANUALS_TYPE_BOOK))
    {
      ManualsBook *book = ide_tree_node_get_item (node);
      const char *title = manuals_book_get_title (book);

      ide_tree_node_set_title (node, title);
      ide_tree_node_set_icon_name (node, "builder-documentation-symbolic");
      ide_tree_node_set_children_possible (node, TRUE);
    }
  else if (ide_tree_node_holds (node, MANUALS_TYPE_HEADING))
    {
      ManualsHeading *heading = ide_tree_node_get_item (node);
      const char *title = manuals_heading_get_title (heading);
      DexFuture *future;

      ide_tree_node_set_title (node, title);
      ide_tree_node_set_children_possible (node, FALSE);

      future = manuals_heading_has_children (heading);
      future = dex_future_then (future,
                                apply_children_posible,
                                g_object_ref (node),
                                g_object_unref);
      dex_future_disown (future);
    }
}

static DexFuture *
gbp_manuals_tree_addin_add_children (DexFuture *completed,
                                     gpointer   user_data)
{
  IdeTreeNode *node = user_data;
  g_autoptr(GListModel) list = NULL;
  guint n_items;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (dex_future_is_resolved (completed));
  g_assert (IDE_IS_TREE_NODE (node));

  list = dex_await_object (dex_ref (completed), NULL);
  n_items = g_list_model_get_n_items (list);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(GObject) item = g_list_model_get_item (list, i);
      g_autoptr(IdeTreeNode) child = ide_tree_node_new ();

      ide_tree_node_set_item (child, item);
      ide_tree_node_insert_before (child, node, NULL);
    }

  return dex_future_new_for_boolean (TRUE);
}

static void
gbp_manuals_tree_addin_build_children_async (IdeTreeAddin        *addin,
                                             IdeTreeNode         *node,
                                             GCancellable        *cancellable,
                                             GAsyncReadyCallback  callback,
                                             gpointer             user_data)
{
  g_autoptr(DexAsyncResult) result = NULL;
  DexFuture *future = NULL;

  g_assert (GBP_IS_MANUALS_TREE_ADDIN (addin));
  g_assert (IDE_IS_TREE_NODE (node));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  result = dex_async_result_new (addin, cancellable, callback, user_data);

  if (ide_tree_node_holds (node, MANUALS_TYPE_REPOSITORY))
    {
      ManualsRepository *repository = ide_tree_node_get_item (node);

      future = manuals_repository_list_sdks (repository);
      future = dex_future_then (future,
                                gbp_manuals_tree_addin_add_children,
                                g_object_ref (node),
                                g_object_unref);
    }
  else if (ide_tree_node_holds (node, MANUALS_TYPE_SDK))
    {
      ManualsSdk *sdk = ide_tree_node_get_item (node);

      future = manuals_sdk_list_books (sdk);
      future = dex_future_then (future,
                                gbp_manuals_tree_addin_add_children,
                                g_object_ref (node),
                                g_object_unref);
    }
  else if (ide_tree_node_holds (node, MANUALS_TYPE_BOOK))
    {
      ManualsBook *book = ide_tree_node_get_item (node);

      future = manuals_book_list_headings (book);
      future = dex_future_then (future,
                                gbp_manuals_tree_addin_add_children,
                                g_object_ref (node),
                                g_object_unref);
    }
  else if (ide_tree_node_holds (node, MANUALS_TYPE_HEADING))
    {
      ManualsHeading *heading = ide_tree_node_get_item (node);

      future = manuals_heading_list_headings (heading);
      future = dex_future_then (future,
                                gbp_manuals_tree_addin_add_children,
                                g_object_ref (node),
                                g_object_unref);
    }
  else
    {
      future = dex_future_new_for_boolean (TRUE);
    }

  g_assert (DEX_IS_FUTURE (future));

  dex_async_result_await (result, g_steal_pointer (&future));
}

static gboolean
gbp_manuals_tree_addin_build_children_finish (IdeTreeAddin  *addin,
                                              GAsyncResult  *result,
                                              GError       **error)
{
  g_assert (GBP_IS_MANUALS_TREE_ADDIN (addin));
  g_assert (DEX_IS_ASYNC_RESULT (result));

  return dex_async_result_propagate_boolean (DEX_ASYNC_RESULT (result), error);
}

static void
tree_addin_iface_init (IdeTreeAddinInterface *iface)
{
  iface->build_node = gbp_manuals_tree_addin_build_node;
  iface->build_children_async = gbp_manuals_tree_addin_build_children_async;
  iface->build_children_finish = gbp_manuals_tree_addin_build_children_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpManualsTreeAddin, gbp_manuals_tree_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_TREE_ADDIN, tree_addin_iface_init))

static void
gbp_manuals_tree_addin_class_init (GbpManualsTreeAddinClass *klass)
{
}

static void
gbp_manuals_tree_addin_init (GbpManualsTreeAddin *self)
{
}
