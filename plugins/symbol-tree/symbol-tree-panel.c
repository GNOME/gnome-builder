/* symbol-tree-panel.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "symbol-tree-panel"

#include <ide.h>

#include <glib/gi18n.h>
#include <ide.h>

#include "egg-task-cache.h"

#include "symbol-tree.h"
#include "symbol-tree-builder.h"
#include "symbol-tree-panel.h"
#include "symbol-tree-resources.h"

#define REFRESH_TREE_INTERVAL_MSEC (15 * 1000)

struct _SymbolTreePanel
{
  PnlDockWidget   parent_instance;

  GCancellable   *cancellable;
  EggTaskCache   *symbols_cache;
  IdeTree        *tree;
  GtkSearchEntry *search_entry;

  IdeBuffer      *last_document;
  gsize           last_change_count;

  guint           refresh_tree_timeout;
};

G_DEFINE_TYPE (SymbolTreePanel, symbol_tree_panel, PNL_TYPE_DOCK_WIDGET)

static void refresh_tree (SymbolTreePanel *self);

static gboolean
refresh_tree_timeout (gpointer user_data)
{
  SymbolTreePanel *self = user_data;
  refresh_tree (self);
  return G_SOURCE_CONTINUE;
}

static void
get_cached_symbol_tree_cb (GObject      *object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  EggTaskCache *cache = (EggTaskCache *)object;
  g_autoptr(SymbolTreePanel) self = user_data;
  g_autoptr(IdeSymbolTree) symbol_tree = NULL;
  g_autoptr(GError) error = NULL;
  IdeTreeNode *root;
  GtkTreeIter iter;
  GtkTreeModel *model;

  IDE_ENTRY;

  g_assert (EGG_IS_TASK_CACHE (cache));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (SYMBOL_IS_TREE_PANEL (self));

  if (!(symbol_tree = egg_task_cache_get_finish (cache, result, &error)))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED))
        g_warning ("%s", error->message);
      return;
    }

  self->refresh_tree_timeout = g_timeout_add (REFRESH_TREE_INTERVAL_MSEC,
                                              refresh_tree_timeout,
                                              self);

  root = g_object_new (IDE_TYPE_TREE_NODE,
                       "item", symbol_tree,
                       NULL);
  ide_tree_set_root (self->tree, root);

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (self->tree));

  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      do
        {
          g_autoptr(IdeTreeNode) node = NULL;

          gtk_tree_model_get (model, &iter, 0, &node, -1);
          if (node != NULL)
            ide_tree_node_expand (node, FALSE);
        }
      while (gtk_tree_model_iter_next (model, &iter));
    }

  IDE_EXIT;
}

static void
refresh_tree (SymbolTreePanel *self)
{
  GtkWidget *active_view;
  IdePerspective *perspective;
  IdeWorkbench *workbench;
  IdeBuffer *document = NULL;
  gsize change_count = 0;

  IDE_ENTRY;

  g_assert (SYMBOL_IS_TREE_PANEL (self));

  workbench = IDE_WORKBENCH (gtk_widget_get_ancestor (GTK_WIDGET (self), IDE_TYPE_WORKBENCH));
  if (workbench == NULL)
    IDE_EXIT;

  perspective = ide_workbench_get_perspective_by_name (workbench, "editor");
  g_assert (perspective != NULL);

  if ((active_view = ide_layout_get_active_view (IDE_LAYOUT (perspective))) &&
      IDE_IS_EDITOR_VIEW (active_view))
    {
      document = ide_editor_view_get_document (IDE_EDITOR_VIEW  (active_view));
      if (IDE_IS_BUFFER (document))
        change_count = ide_buffer_get_change_count (IDE_BUFFER (document));
    }

  if ((document != self->last_document) || (self->last_change_count < change_count))
    {
      ide_clear_source (&self->refresh_tree_timeout);

      self->last_document = document;
      self->last_change_count = change_count;

      /*
       * Clear the old tree items.
       *
       * TODO: Get cross compile names for nodes so that we can
       *       recompute the open state.
       */
      ide_tree_set_root (self->tree, ide_tree_node_new ());

      /*
       * Fetch the symbols via the transparent cache.
       */
      if (document != NULL)
        {
          if (self->cancellable != NULL)
            {
              g_cancellable_cancel (self->cancellable);
              g_clear_object (&self->cancellable);
            }

          self->cancellable = g_cancellable_new ();

          egg_task_cache_get_async (self->symbols_cache,
                                    document,
                                    FALSE,
                                    self->cancellable,
                                    get_cached_symbol_tree_cb,
                                    g_object_ref (self));
        }
    }

  IDE_EXIT;
}

static void
get_symbol_tree_cb (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  IdeSymbolResolver *resolver = (IdeSymbolResolver *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(IdeSymbolTree) symbol_tree = NULL;
  GError *error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_SYMBOL_RESOLVER (resolver));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  symbol_tree = ide_symbol_resolver_get_symbol_tree_finish (resolver, result, &error);

  if (!symbol_tree)
    g_task_return_error (task, error);
  else
    g_task_return_pointer (task, g_object_ref (symbol_tree), g_object_unref);

  IDE_EXIT;
}

static void
populate_cache_cb (EggTaskCache  *cache,
                   gconstpointer  key,
                   GTask         *task,
                   gpointer       user_data)
{
  IdeBuffer *document = (IdeBuffer *)key;
  IdeSymbolResolver *resolver;
  IdeFile *file;

  IDE_ENTRY;

  g_assert (EGG_IS_TASK_CACHE (cache));
  g_assert (IDE_IS_BUFFER (document));
  g_assert (G_IS_TASK (task));

  if ((resolver = ide_buffer_get_symbol_resolver (IDE_BUFFER (document))) &&
      (file = ide_buffer_get_file (IDE_BUFFER (document))))
    {
      ide_symbol_resolver_get_symbol_tree_async (resolver,
                                                 ide_file_get_file (file),
                                                 g_task_get_cancellable (task),
                                                 get_symbol_tree_cb,
                                                 g_object_ref (task));
    }
  else
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               _("Current language does not support symbol resolvers"));
    }

  IDE_EXIT;
}

static gboolean
filter_symbols_cb (IdeTree     *tree,
                   IdeTreeNode *node,
                   gpointer    user_data)
{
  IdePatternSpec *spec = user_data;
  const gchar *text;

  g_assert (IDE_IS_TREE (tree));
  g_assert (IDE_IS_TREE_NODE (node));
  g_assert (spec != NULL);

  if ((text = ide_tree_node_get_text (node)) != NULL)
    return ide_pattern_spec_match (spec, text);

  return FALSE;
}

static void
symbol_tree__search_entry_changed (SymbolTreePanel *self,
                                   GtkSearchEntry  *search_entry)
{
  const gchar *text;

  g_return_if_fail (SYMBOL_IS_TREE_PANEL (self));
  g_return_if_fail (GTK_IS_SEARCH_ENTRY (search_entry));

  text = gtk_entry_get_text (GTK_ENTRY (search_entry));

  if (ide_str_empty0 (text))
    {
      ide_tree_set_filter (self->tree, NULL, NULL, NULL);
    }
  else
    {
      IdePatternSpec *spec;

      spec = ide_pattern_spec_new (text);
      ide_tree_set_filter (self->tree,
                          filter_symbols_cb,
                          spec,
                          (GDestroyNotify)ide_pattern_spec_unref);
      gtk_tree_view_expand_all (GTK_TREE_VIEW (self->tree));
    }
}

static void
symbol_tree_panel_finalize (GObject *object)
{
  SymbolTreePanel *self = (SymbolTreePanel *)object;

  ide_clear_source (&self->refresh_tree_timeout);
  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (symbol_tree_panel_parent_class)->finalize (object);
}

static void
symbol_tree_panel_class_init (SymbolTreePanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = symbol_tree_panel_finalize;

  gtk_widget_class_set_css_name (widget_class, "symboltreepanel");
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/symbol-tree/symbol-tree-panel.ui");
  gtk_widget_class_bind_template_child (widget_class, SymbolTreePanel, tree);
  gtk_widget_class_bind_template_child (widget_class, SymbolTreePanel, search_entry);
}

static void
symbol_tree_panel_init (SymbolTreePanel *self)
{
  IdeTreeNode *root;
  IdeTreeBuilder *builder;

  self->symbols_cache = egg_task_cache_new (g_direct_hash,
                                            g_direct_equal,
                                            g_object_ref,
                                            g_object_unref,
                                            g_object_ref,
                                            g_object_unref,
                                            20 * 1000L,
                                            populate_cache_cb,
                                            self,
                                            NULL);

  gtk_widget_init_template (GTK_WIDGET (self));

  g_object_set (self, "title", _("Symbols"), NULL);

  root = ide_tree_node_new ();
  ide_tree_set_root (self->tree, root);

  builder = g_object_new (SYMBOL_TYPE_TREE_BUILDER, NULL);
  ide_tree_add_builder (self->tree, builder);

  g_signal_connect_object (self->search_entry,
                           "changed",
                           G_CALLBACK (symbol_tree__search_entry_changed),
                           self,
                           G_CONNECT_SWAPPED);
}

void
symbol_tree_panel_reset (SymbolTreePanel *self)
{
  IDE_ENTRY;

  refresh_tree (self);
  gtk_entry_set_text (GTK_ENTRY (self->search_entry), "");

  IDE_EXIT;
}
