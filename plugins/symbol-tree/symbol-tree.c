/* symbol-tree.c
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
#include <ide.h>
#include <libpeas/peas.h>

#include "egg-task-cache.h"

#include "gb-editor-view.h"
#include "gb-tree.h"
#include "gb-workspace.h"

#include "symbol-tree.h"
#include "symbol-tree-builder.h"
#include "symbol-tree-resources.h"

struct _SymbolTree
{
  GtkBox          parent_instance;

  GCancellable   *cancellable;
  EggTaskCache   *symbols_cache;
  GbWorkbench    *workbench;
  GbTree         *tree;
  GtkSearchEntry *search_entry;
};

enum {
  PROP_0,
  PROP_WORKBENCH,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

static void workbench_addin_init (GbWorkbenchAddinInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (SymbolTree, symbol_tree, GTK_TYPE_BOX, 0,
                                G_IMPLEMENT_INTERFACE_DYNAMIC (GB_TYPE_WORKBENCH_ADDIN,
                                                               workbench_addin_init))

static void
get_cached_symbol_tree_cb (GObject      *object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  EggTaskCache *cache = (EggTaskCache *)object;
  g_autoptr(SymbolTree) self = user_data;
  g_autoptr(IdeSymbolTree) symbol_tree = NULL;
  g_autoptr(GError) error = NULL;
  GbTreeNode *root;

  g_assert (EGG_IS_TASK_CACHE (cache));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (SYMBOL_IS_TREE (self));

  if (!(symbol_tree = egg_task_cache_get_finish (cache, result, &error)))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED))
        g_warning ("%s", error->message);
      return;
    }

  gb_tree_set_filter (self->tree, NULL, NULL, NULL);
  gtk_entry_set_text (GTK_ENTRY (self->search_entry), "");

  root = g_object_new (GB_TYPE_TREE_NODE,
                       "item", symbol_tree,
                       NULL);
  gb_tree_set_root (self->tree, root);
}

static void
notify_active_view_cb (SymbolTree  *self,
                       GParamFlags *pspec,
                       GbWorkbench *workbench)
{
  GbDocument *document = NULL;
  GtkWidget *active_view;
  GbTreeNode *root;

  g_assert (SYMBOL_IS_TREE (self));
  g_assert (pspec != NULL);
  g_assert (GB_IS_WORKBENCH (workbench));

  if ((active_view = gb_workbench_get_active_view (workbench)) && GB_IS_EDITOR_VIEW (active_view))
    document = gb_view_get_document (GB_VIEW (active_view));

  root = gb_tree_get_root (self->tree);

  if ((GObject *)document != gb_tree_node_get_item (root))
    {
      /*
       * First, clear the old tree items.
       */
      gb_tree_set_root (self->tree, gb_tree_node_new ());;

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

  g_assert (IDE_IS_SYMBOL_RESOLVER (resolver));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  symbol_tree = ide_symbol_resolver_get_symbol_tree_finish (resolver, result, &error);

  if (!symbol_tree)
    g_task_return_error (task, error);
  else
    g_task_return_pointer (task, g_object_ref (symbol_tree), g_object_unref);
}

static void
populate_cache_cb (EggTaskCache  *cache,
                   gconstpointer  key,
                   GTask         *task,
                   gpointer       user_data)
{
  GbEditorDocument *document = (GbEditorDocument *)key;
  IdeLanguage *language;
  IdeFile *file;
  IdeSymbolResolver *resolver;

  g_assert (EGG_IS_TASK_CACHE (cache));
  g_assert (GB_IS_EDITOR_DOCUMENT (document));
  g_assert (G_IS_TASK (task));

  if ((file = ide_buffer_get_file (IDE_BUFFER (document))) &&
      (language = ide_file_get_language (file)) &&
      (resolver = ide_language_get_symbol_resolver (language)))
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
}

static void
symbol_tree_load (GbWorkbenchAddin *addin)
{
  SymbolTree *self = (SymbolTree *)addin;
  GbWorkspace *workspace;
  GtkWidget *right_pane;

  g_assert (SYMBOL_IS_TREE (self));
  g_assert (GB_IS_WORKBENCH (self->workbench));

  g_signal_connect_object (self->workbench,
                           "notify::active-view",
                           G_CALLBACK (notify_active_view_cb),
                           self,
                           G_CONNECT_SWAPPED);

  workspace = GB_WORKSPACE (gb_workbench_get_workspace (self->workbench));
  right_pane = gb_workspace_get_right_pane (workspace);
  gb_workspace_pane_add_page (GB_WORKSPACE_PANE (right_pane),
                              GTK_WIDGET (self),
                              _("Symbol Tree"),
                              "lang-function-symbolic");

  gtk_container_child_set (GTK_CONTAINER (gtk_widget_get_parent (GTK_WIDGET (self))),
                           GTK_WIDGET (self),
                           "position", 1,
                           NULL);
}

static void
symbol_tree_unload (GbWorkbenchAddin *addin)
{
  SymbolTree *self = (SymbolTree *)addin;
  GbWorkspace *workspace;
  GtkWidget *right_pane;

  g_assert (SYMBOL_IS_TREE (self));
  g_assert (GB_IS_WORKBENCH (self->workbench));

  workspace = GB_WORKSPACE (gb_workbench_get_workspace (self->workbench));
  right_pane = gb_workspace_get_right_pane (workspace);
  gb_workspace_pane_remove_page (GB_WORKSPACE_PANE (right_pane), GTK_WIDGET (self));
}

static void
symbol_tree_set_workbench (SymbolTree  *self,
                           GbWorkbench *workbench)
{
  g_assert (SYMBOL_IS_TREE (self));
  g_assert (GB_IS_WORKBENCH (workbench));

  ide_set_weak_pointer (&self->workbench, workbench);
}

static gboolean
filter_symbols_cb (GbTree     *tree,
                   GbTreeNode *node,
                   gpointer    user_data)
{
  IdePatternSpec *spec = user_data;

  return ide_pattern_spec_match (spec, gb_tree_node_get_text (node));
}

static void
symbol_tree__search_entry_changed (SymbolTree     *self,
                                   GtkSearchEntry *search_entry)
{
  const gchar *text;

  g_return_if_fail (SYMBOL_IS_TREE (self));
  g_return_if_fail (GTK_IS_SEARCH_ENTRY (search_entry));

  text = gtk_entry_get_text (GTK_ENTRY (search_entry));

  if (ide_str_empty0 (text))
    {
      gb_tree_set_filter (self->tree, NULL, NULL, NULL);
    }
  else
    {
      IdePatternSpec *spec;

      spec = ide_pattern_spec_new (text);
      gb_tree_set_filter (self->tree,
                          filter_symbols_cb,
                          spec,
                          (GDestroyNotify)ide_pattern_spec_unref);
    }
}

static void
symbol_tree_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  SymbolTree *self = (SymbolTree *)object;

  switch (prop_id)
    {
    case PROP_WORKBENCH:
      symbol_tree_set_workbench (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
symbol_tree_finalize (GObject *object)
{
  SymbolTree *self = (SymbolTree *)object;

  ide_clear_weak_pointer (&self->workbench);
  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (symbol_tree_parent_class)->finalize (object);
}

static void
workbench_addin_init (GbWorkbenchAddinInterface *iface)
{
  iface->load = symbol_tree_load;
  iface->unload = symbol_tree_unload;
}

static void
symbol_tree_class_init (SymbolTreeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = symbol_tree_finalize;
  object_class->set_property = symbol_tree_set_property;

  gParamSpecs [PROP_WORKBENCH] =
    g_param_spec_object ("workbench",
                         _("Workbench"),
                         _("Workbench"),
                         GB_TYPE_WORKBENCH,
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/symbol-tree/symbol-tree.ui");
  gtk_widget_class_bind_template_child (widget_class, SymbolTree, tree);
  gtk_widget_class_bind_template_child (widget_class, SymbolTree, search_entry);

  g_type_ensure (GB_TYPE_TREE);
}

static void
symbol_tree_class_finalize (SymbolTreeClass *klass)
{
}

static void
symbol_tree_init (SymbolTree *self)
{
  GbTreeNode *root;
  GbTreeBuilder *builder;

  self->symbols_cache = egg_task_cache_new (g_direct_hash,
                                            g_direct_equal,
                                            g_object_ref,
                                            g_object_unref,
                                            g_object_ref,
                                            g_object_unref,
                                            G_USEC_PER_SEC * 20L,
                                            populate_cache_cb,
                                            self,
                                            NULL);

  gtk_widget_init_template (GTK_WIDGET (self));

  root = gb_tree_node_new ();
  gb_tree_set_root (self->tree, root);

  builder = g_object_new (SYMBOL_TYPE_TREE_BUILDER, NULL);
  gb_tree_add_builder (self->tree, builder);

  g_signal_connect_object (self->search_entry,
                           "changed",
                           G_CALLBACK (symbol_tree__search_entry_changed),
                           self,
                           G_CONNECT_SWAPPED);
}

void
peas_register_types (PeasObjectModule *module)
{
  symbol_tree_register_type (G_TYPE_MODULE (module));

  peas_object_module_register_extension_type (module,
                                              GB_TYPE_WORKBENCH_ADDIN,
                                              SYMBOL_TYPE_TREE);
}
