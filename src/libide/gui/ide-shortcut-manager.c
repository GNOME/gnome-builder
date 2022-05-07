/* ide-shortcut-manager.c
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

#define G_LOG_DOMAIN "ide-shortcut-manager"

#include "config.h"

#include <gtk/gtk.h>

#include "ide-shortcut-bundle-private.h"
#include "ide-shortcut-manager-private.h"

struct _IdeShortcutManager
{
  IdeObject   parent_instance;

  /* Holds [plugin_models,internal_models] so that plugin models take
   * priority over the others.
   */
  GListStore *toplevel;

  /* Holds bundles loaded from plugins, more recently loaded plugins
   * towards the head of the list.
   *
   * Plugins loaded dynamically could change ordering here, which might
   * be something we want to address someday. In practice, it doesn't
   * happen very often and people restart applications often.
   */
  GListStore *plugin_models;

  /* A flattened list model we proxy through our interface */
  GtkFlattenListModel *flatten;
};

static GType
ide_shortcut_manager_get_item_type (GListModel *model)
{
  return GTK_TYPE_SHORTCUT;
}

static guint
ide_shortcut_manager_get_n_items (GListModel *model)
{
  IdeShortcutManager *self = IDE_SHORTCUT_MANAGER (model);

  if (self->flatten)
    return g_list_model_get_n_items (G_LIST_MODEL (self->flatten));

  return 0;
}

static gpointer
ide_shortcut_manager_get_item (GListModel *model,
                               guint       position)
{
  IdeShortcutManager *self = IDE_SHORTCUT_MANAGER (model);

  if (self->flatten)
    return g_list_model_get_item (G_LIST_MODEL (self->flatten), position);

  return NULL;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = ide_shortcut_manager_get_item_type;
  iface->get_n_items = ide_shortcut_manager_get_n_items;
  iface->get_item = ide_shortcut_manager_get_item;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeShortcutManager, ide_shortcut_manager, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static GListModel *
get_internal_models (void)
{
  static GListStore *internal_models;

  if (internal_models == NULL)
    {
      static const char *names[] = {
        "libide-gui",
      };

      internal_models = g_list_store_new (G_TYPE_LIST_MODEL);

      for (guint i = 0; i < G_N_ELEMENTS (names); i++)
        {
          g_autoptr(IdeShortcutBundle) bundle = ide_shortcut_bundle_new ();
          g_autofree char *uri = g_strdup_printf ("resource:///org/gnome/%s/gtk/keybindings.json", names[i]);
          g_autoptr(GFile) file = g_file_new_for_uri (uri);
          g_autoptr(GError) error = NULL;

          if (!g_file_query_exists (file, NULL))
            continue;

          if (!ide_shortcut_bundle_parse (bundle, file, &error))
            g_critical ("Failed to parse %s: %s", uri, error->message);
          else
            g_list_store_append (internal_models, bundle);
        }
    }

  return G_LIST_MODEL (internal_models);
}

static void
ide_shortcut_manager_items_changed_cb (IdeShortcutManager *self,
                                       guint               position,
                                       guint               removed,
                                       guint               added,
                                       GListModel         *model)
{
  g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);
}

static void
ide_shortcut_manager_dispose (GObject *object)
{
  IdeShortcutManager *self = (IdeShortcutManager *)object;

  g_clear_object (&self->toplevel);
  g_clear_object (&self->plugin_models);
  g_clear_object (&self->flatten);

  G_OBJECT_CLASS (ide_shortcut_manager_parent_class)->dispose (object);
}

static void
ide_shortcut_manager_class_init (IdeShortcutManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_shortcut_manager_dispose;
}

static void
ide_shortcut_manager_init (IdeShortcutManager *self)
{
  self->toplevel = g_list_store_new (G_TYPE_LIST_MODEL);
  self->plugin_models = g_list_store_new (G_TYPE_LIST_MODEL);
  g_list_store_append (self->toplevel, G_LIST_MODEL (self->plugin_models));
  g_list_store_append (self->toplevel, get_internal_models ());

  self->flatten = gtk_flatten_list_model_new (g_object_ref (G_LIST_MODEL (self->toplevel)));
  g_signal_connect_object (self->flatten,
                           "items-changed",
                           G_CALLBACK (ide_shortcut_manager_items_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

/**
 * ide_shortcut_manager_from_context:
 * @context: an #IdeContext
 *
 * Gets the shortcut manager for the contenxt
 *
 * Returns: (transfer none): an #IdeShortcutManager
 */
IdeShortcutManager *
ide_shortcut_manager_from_context (IdeContext *context)
{
  IdeShortcutManager *ret;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  if (!(ret = ide_context_peek_child_typed (context, IDE_TYPE_SHORTCUT_MANAGER)))
    {
      g_autoptr(IdeObject) child = NULL;

      child = ide_object_ensure_child_typed (IDE_OBJECT (context), IDE_TYPE_SHORTCUT_MANAGER);
      ret = ide_context_peek_child_typed (context, IDE_TYPE_SHORTCUT_MANAGER);
    }

  return ret;
}
