/*
 * gbp-manuals-panel.c
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

#include <libide-tree.h>

#include "gbp-manuals-panel.h"
#include "gbp-manuals-page.h"
#include "gbp-manuals-workspace-addin.h"

#include "manuals-keyword.h"
#include "manuals-navigatable.h"
#include "manuals-navigatable-model.h"
#include "manuals-search-query.h"
#include "manuals-search-result.h"
#include "manuals-tag.h"

struct _GbpManualsPanel
{
  IdePane             parent_instance;

  ManualsRepository  *repository;
  DexFuture          *query;
  ManualsNavigatable *reveal;

  IdeTree            *tree;
  GtkListView        *search_view;
  GtkStack           *stack;
  GtkSearchEntry     *search_entry;
};

enum {
  PROP_0,
  PROP_REPOSITORY,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpManualsPanel, gbp_manuals_panel, IDE_TYPE_PANE)

static GParamSpec *properties[N_PROPS];

static void
gbp_manuals_panel_search_changed_cb (GbpManualsPanel *self,
                                     GtkSearchEntry  *search_entry)
{
  g_autofree char *text = NULL;

  g_assert (GBP_IS_MANUALS_PANEL (self));
  g_assert (GTK_IS_SEARCH_ENTRY (search_entry));

  dex_clear (&self->query);

  text = g_strstrip (g_strdup (gtk_editable_get_text (GTK_EDITABLE (search_entry))));

  if (ide_str_empty0 (text))
    {
      gtk_stack_set_visible_child_name (self->stack, "tree");
    }
  else
    {
      g_autoptr(ManualsSearchQuery) query = manuals_search_query_new ();
      g_autoptr(GtkNoSelection) selection = NULL;

      manuals_search_query_set_text (query, text);

      /* Hold on to the future so we can cancel it by releasing when a
       * new search comes in.
       */
      self->query = manuals_search_query_execute (query, self->repository);
      selection = gtk_no_selection_new (g_object_ref (G_LIST_MODEL (query)));
      gtk_list_view_set_model (self->search_view, GTK_SELECTION_MODEL (selection));

      gtk_stack_set_visible_child_name (self->stack, "search");
    }
}

static void
gbp_manuals_panel_search_view_activate_cb (GbpManualsPanel *self,
                                           guint            position,
                                           GtkListView     *list_view)
{
  g_autoptr(ManualsSearchResult) result = NULL;
  ManualsNavigatable *navigatable;
  GtkSelectionModel *model;
  IdeWorkspaceAddin *workspace_addin;
  IdeWorkspace *workspace;
  GbpManualsPage *page;

  g_assert (GBP_IS_MANUALS_PANEL (self));
  g_assert (GTK_IS_LIST_VIEW (list_view));

  model = gtk_list_view_get_model (list_view);
  result = g_list_model_get_item (G_LIST_MODEL (model), position);
  navigatable = manuals_search_result_get_item (result);

  if (navigatable == NULL)
    return;

  g_assert (MANUALS_IS_NAVIGATABLE (navigatable));

  workspace = ide_widget_get_workspace (GTK_WIDGET (self));
  workspace_addin = ide_workspace_addin_find_by_module_name (workspace, "manuals");
  page = gbp_manuals_workspace_addin_get_page (GBP_MANUALS_WORKSPACE_ADDIN (workspace_addin));

  gbp_manuals_page_navigate_to (page, navigatable);

  panel_widget_raise (PANEL_WIDGET (page));
  gtk_widget_grab_focus (GTK_WIDGET (page));
}

static gboolean
nonempty_to_boolean (gpointer    instance,
                     const char *data)
{
  return !ide_str_empty0 (data);
}

static char *
lookup_sdk_title (gpointer        instance,
                  ManualsKeyword *keyword)
{
  g_autoptr(ManualsRepository) repository = NULL;
  gint64 book_id;
  gint64 sdk_id;

  g_assert (!keyword || MANUALS_IS_KEYWORD (keyword));

  if (keyword == NULL)
    return NULL;

  g_object_get (keyword, "repository", &repository, NULL);
  book_id = manuals_keyword_get_book_id (keyword);
  sdk_id = manuals_repository_get_cached_sdk_id (repository, book_id);

  return g_strdup (manuals_repository_get_cached_sdk_title (repository, sdk_id));
}

static void
gbp_manuals_panel_dispose (GObject *object)
{
  GbpManualsPanel *self = (GbpManualsPanel *)object;

  gtk_widget_dispose_template (GTK_WIDGET (self), GBP_TYPE_MANUALS_PANEL);

  dex_clear (&self->query);
  g_clear_object (&self->repository);
  g_clear_object (&self->reveal);

  G_OBJECT_CLASS (gbp_manuals_panel_parent_class)->dispose (object);
}

static void
gbp_manuals_panel_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GbpManualsPanel *self = GBP_MANUALS_PANEL (object);

  switch (prop_id)
    {
    case PROP_REPOSITORY:
      g_value_set_object (value, self->repository);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_manuals_panel_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GbpManualsPanel *self = GBP_MANUALS_PANEL (object);

  switch (prop_id)
    {
    case PROP_REPOSITORY:
      gbp_manuals_panel_set_repository (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_manuals_panel_class_init (GbpManualsPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gbp_manuals_panel_dispose;
  object_class->get_property = gbp_manuals_panel_get_property;
  object_class->set_property = gbp_manuals_panel_set_property;

  properties[PROP_REPOSITORY] =
    g_param_spec_object ("repository", NULL, NULL,
                         MANUALS_TYPE_REPOSITORY,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "GbpManualsPanel");
  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/manuals/gbp-manuals-panel.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpManualsPanel, search_entry);
  gtk_widget_class_bind_template_child (widget_class, GbpManualsPanel, search_view);
  gtk_widget_class_bind_template_child (widget_class, GbpManualsPanel, stack);
  gtk_widget_class_bind_template_child (widget_class, GbpManualsPanel, tree);
  gtk_widget_class_bind_template_callback (widget_class, gbp_manuals_panel_search_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, gbp_manuals_panel_search_view_activate_cb);
  gtk_widget_class_bind_template_callback (widget_class, nonempty_to_boolean);
  gtk_widget_class_bind_template_callback (widget_class, lookup_sdk_title);

  g_type_ensure (MANUALS_TYPE_NAVIGATABLE);
  g_type_ensure (MANUALS_TYPE_SEARCH_RESULT);
  g_type_ensure (MANUALS_TYPE_TAG);
}

static void
gbp_manuals_panel_init (GbpManualsPanel *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

void
gbp_manuals_panel_set_repository (GbpManualsPanel   *self,
                                  ManualsRepository *repository)
{
  g_autoptr(IdeTreeNode) root = NULL;

  g_return_if_fail (GBP_IS_MANUALS_PANEL (self));
  g_return_if_fail (MANUALS_IS_REPOSITORY (repository));

  if (!g_set_object (&self->repository, repository))
    return;

  root = ide_tree_node_new ();
  ide_tree_node_set_item (root, repository);
  ide_tree_set_root (self->tree, root);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_REPOSITORY]);
}

GbpManualsPanel *
gbp_manuals_panel_new (void)
{
  return g_object_new (GBP_TYPE_MANUALS_PANEL, NULL);
}

static void
expand_node_cb (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
  g_autoptr(DexPromise) promise = user_data;
  g_autoptr(GError) error = NULL;

  if (!ide_tree_expand_node_finish (IDE_TREE (object), result, &error))
    dex_promise_reject (promise, g_steal_pointer (&error));
  else
    dex_promise_resolve_boolean (promise, TRUE);
}

static DexFuture *
expand_node (IdeTree     *tree,
             IdeTreeNode *node)
{
  DexPromise *promise = dex_promise_new_cancellable ();
  ide_tree_expand_node_async (tree,
                              node,
                              dex_promise_get_cancellable (promise),
                              expand_node_cb,
                              dex_ref (promise));
  return DEX_FUTURE (promise);
}

static gboolean
node_matches (IdeTreeNode        *node,
              ManualsNavigatable *navigatable)
{
  gpointer node_item = ide_tree_node_get_item (node);
  gpointer nav_item = manuals_navigatable_get_item (navigatable);
  gint64 node_id = 0;
  gint64 nav_id = 0;

  if (node_item == nav_item)
    return TRUE;

  if (G_OBJECT_TYPE (node_item) != G_OBJECT_TYPE (nav_item))
    return FALSE;

  g_object_get (node_item, "id", &node_id, NULL);
  g_object_get (nav_item, "id", &nav_id, NULL);

  return node_id == nav_id;
}

static DexFuture *
gbp_manuals_panel_reveal_fiber (gpointer user_data)
{
  GbpManualsPanel *self = user_data;
  g_autoptr(ManualsNavigatable) reveal = NULL;
  g_autoptr(GPtrArray) chain = NULL;
  ManualsNavigatable *parent;
  IdeTreeNode *node;

  g_assert (GBP_IS_MANUALS_PANEL (self));

  if (!(reveal = g_steal_pointer (&self->reveal)))
    goto completed;

  chain = g_ptr_array_new_with_free_func (g_object_unref);
  parent = g_object_ref (reveal);

  while (parent != NULL)
    {
      g_ptr_array_insert (chain, 0, parent);
      parent = dex_await_object (manuals_navigatable_find_parent (parent), NULL);
    }

  /* repository is always index 0 */
  g_ptr_array_remove_index (chain, 0);

  node = ide_tree_get_root (self->tree);

  while (node != NULL && chain->len > 0)
    {
      g_autoptr(ManualsNavigatable) navigatable = g_object_ref (g_ptr_array_index (chain, 0));
      IdeTreeNode *child;
      gboolean found = FALSE;

      g_ptr_array_remove_index (chain, 0);

      dex_await (expand_node (self->tree, node), NULL);

      for (child = ide_tree_node_get_first_child (node);
           child != NULL;
           child = ide_tree_node_get_next_sibling (child))
        {
          if (node_matches (child, navigatable))
            {
              node = child;
              found = TRUE;
              break;
            }
        }

      if (!found)
        break;
    }

  if (node != NULL)
    ide_tree_set_selected_node (self->tree, node);

  gtk_stack_set_visible_child_name (self->stack, "tree");
  panel_widget_raise (PANEL_WIDGET (self));

completed:
  return dex_future_new_for_boolean (TRUE);
}

void
gbp_manuals_panel_reveal (GbpManualsPanel    *self,
                          ManualsNavigatable *navigatable)
{
  g_return_if_fail (GBP_IS_MANUALS_PANEL (self));
  g_return_if_fail (MANUALS_IS_NAVIGATABLE (navigatable));

  g_set_object (&self->reveal, navigatable);

  dex_future_disown (dex_scheduler_spawn (NULL, 0,
                                          gbp_manuals_panel_reveal_fiber,
                                          g_object_ref (self),
                                          g_object_unref));
}

void
gbp_manuals_panel_begin_search (GbpManualsPanel *self)
{
  g_return_if_fail (GBP_IS_MANUALS_PANEL (self));

  gtk_widget_grab_focus (GTK_WIDGET (self->search_entry));
  gtk_editable_select_region (GTK_EDITABLE (self->search_entry), 0, -1);
}
