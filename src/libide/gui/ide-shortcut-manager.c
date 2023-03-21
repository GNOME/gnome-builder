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

#include <libide-plugins.h>

#include "ide-application-private.h"
#include "ide-shortcut-bundle-private.h"
#include "ide-shortcut-manager-private.h"
#include "ide-shortcut-provider.h"

struct _IdeShortcutManager
{
  IdeObject   parent_instance;

  /* Holds [plugin_models,internal_models] so that plugin models take
   * priority over the others.
   */
  GListStore *toplevel;

  /* Holds the bundle for user shortcut overrides. */
  IdeShortcutBundle *user_bundle;

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

  /* Extension set of IdeShortcutProvider */
  IdeExtensionSetAdapter *providers;
  GListStore *providers_models;

  /* Used to track action-name -> accel mappings */
  IdeShortcutObserver *observer;

  /* Idle GSource to update triggers with new values from a user
   * bundle having changed.
   */
  guint update_triggers_source;
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
  GtkShortcut *ret = NULL;

  if (self->flatten)
    ret = g_list_model_get_item (G_LIST_MODEL (self->flatten), position);

  return ret;
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

static GListStore *plugin_models;
static IdeShortcutBundle *user_bundle;
static guint update_menu_source;

static gboolean
update_menus_cb (GHashTable *id_to_trigger)
{
  IdeApplication *app = IDE_APPLICATION_DEFAULT;
  const char * const *menu_ids;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (id_to_trigger != NULL);

  update_menu_source = 0;

  menu_ids = ide_menu_manager_get_menu_ids (app->menu_manager);

  for (guint i = 0; menu_ids[i]; i++)
    {
      GMenu *menu = ide_menu_manager_get_menu_by_id (app->menu_manager, menu_ids[i]);
      guint n_items = g_menu_model_get_n_items (G_MENU_MODEL (menu));

      for (guint j = 0; j < n_items; j++)
        {
          g_autofree char *shortcut_id = NULL;
          g_autofree char *accel = NULL;
          g_autofree char *original_accel = NULL;
          GtkShortcutTrigger *trigger;

          g_menu_model_get_item_attribute (G_MENU_MODEL (menu), j, "id", "s", &shortcut_id);

          if (shortcut_id == NULL)
            continue;

          g_menu_model_get_item_attribute (G_MENU_MODEL (menu), j, "accel", "s", &accel);
          g_menu_model_get_item_attribute (G_MENU_MODEL (menu), j, "original-accel", "s", &original_accel);

          if ((trigger = g_hash_table_lookup (id_to_trigger, shortcut_id)))
            {
              g_autofree char *new_accel = gtk_shortcut_trigger_to_string (trigger);

              if (ide_str_equal0 (new_accel, "never"))
                new_accel[0] = 0;

              /* Save original accel for re-use later */
              if (original_accel == NULL && accel != NULL)
                ide_menu_manager_set_attribute_string (app->menu_manager, menu, j, "original-accel", accel);

              ide_menu_manager_set_attribute_string (app->menu_manager, menu, j, "accel", new_accel);
            }
          else
            {
              if (original_accel != NULL && !ide_str_equal0 (accel, original_accel))
                ide_menu_manager_set_attribute_string (app->menu_manager, menu, j, "accel", original_accel);
            }
        }
    }

  return G_SOURCE_REMOVE;
}

static GListModel *
get_internal_shortcuts (void)
{
  static GtkFlattenListModel *flatten;

  if (flatten == NULL)
    {
      static const char *names[] = { "libide-gui", };
      GListStore *internal_models;

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

      flatten = gtk_flatten_list_model_new (G_LIST_MODEL (internal_models));
    }

  return G_LIST_MODEL (flatten);
}

static void
override_bundles (GListModel *model,
                  GHashTable *id_to_trigger)
{
  guint n_items;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (G_IS_LIST_MODEL (model));
  g_assert (id_to_trigger != NULL);

  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(IdeShortcutBundle) bundle = g_list_model_get_item (model, i);

      g_assert (bundle != NULL);
      g_assert (IDE_IS_SHORTCUT_BUNDLE (bundle));

      ide_shortcut_bundle_override_triggers (bundle, id_to_trigger);
    }
}

static gboolean
ide_shortcut_manager_update_overrides (IdeShortcutManager *self)
{
  g_autoptr(GHashTable) id_to_trigger = NULL;
  GListModel *model;
  guint n_items;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SHORTCUT_MANAGER (self));

  self->update_triggers_source = 0;

  if (user_bundle == NULL)
    IDE_RETURN (G_SOURCE_REMOVE);

  model = G_LIST_MODEL (user_bundle);
  n_items = g_list_model_get_n_items (model);
  id_to_trigger = g_hash_table_new_full (g_str_hash,
                                         g_str_equal,
                                         g_free,
                                         g_object_unref);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(GtkShortcut) shortcut = g_list_model_get_item (model, i);
      IdeShortcut *state = g_object_get_data (G_OBJECT (shortcut), "IDE_SHORTCUT");

      g_assert (GTK_IS_SHORTCUT (shortcut));
      g_assert (state != NULL);

      if (state->override != NULL && state->trigger != NULL)
        g_hash_table_insert (id_to_trigger,
                             g_strdup (state->override),
                             g_object_ref (state->trigger));
    }

  override_bundles (G_LIST_MODEL (self->plugin_models), id_to_trigger);

  if (update_menu_source == 0)
    update_menu_source = g_idle_add_full (G_PRIORITY_LOW,
                                          (GSourceFunc)update_menus_cb,
                                          g_hash_table_ref (id_to_trigger),
                                          (GDestroyNotify)g_hash_table_unref);

  IDE_RETURN (G_SOURCE_REMOVE);
}

static void
ide_shortcut_manager_user_items_changed_cb (IdeShortcutManager *self,
                                            guint               position,
                                            guint               removed,
                                            guint               added,
                                            IdeShortcutBundle  *bundle)
{
  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SHORTCUT_MANAGER (self));
  g_assert (IDE_IS_SHORTCUT_BUNDLE (bundle));

  if (self->update_triggers_source == 0)
    self->update_triggers_source =
      g_idle_add_full (G_PRIORITY_LOW,
                       (GSourceFunc) ide_shortcut_manager_update_overrides,
                       self, NULL);

  IDE_EXIT;
}

static void
ide_shortcut_manager_items_changed_cb (IdeShortcutManager *self,
                                       guint               position,
                                       guint               removed,
                                       guint               added,
                                       GListModel         *model)
{
  g_assert (IDE_IS_SHORTCUT_MANAGER (self));
  g_assert (G_IS_LIST_MODEL (model));

  g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);
}

static void
on_provider_added_cb (IdeExtensionSetAdapter *set,
                      PeasPluginInfo         *plugin_info,
                      GObject          *exten,
                      gpointer                user_data)
{
  IdeShortcutProvider *provider = (IdeShortcutProvider *)exten;
  IdeShortcutManager *self = user_data;
  g_autoptr(GListModel) model = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_SHORTCUT_PROVIDER (provider));

  if ((model = ide_shortcut_provider_list_shortcuts (provider)))
    {
      IDE_TRACE_MSG ("Adding shortcut model for %s with %d items",
                     peas_plugin_info_get_module_name (plugin_info),
                     g_list_model_get_n_items (model));
      g_object_set_data_full (G_OBJECT (provider),
                              "SHORTCUTS_MODEL",
                              g_object_ref (model),
                              g_object_unref);
      g_list_store_append (self->providers_models, model);
    }

  IDE_EXIT;
}

static void
on_provider_removed_cb (IdeExtensionSetAdapter *set,
                        PeasPluginInfo         *plugin_info,
                        GObject          *exten,
                        gpointer                user_data)
{
  IdeShortcutProvider *provider = (IdeShortcutProvider *)exten;
  IdeShortcutManager *self = user_data;
  GListModel *model;

  IDE_ENTRY;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_SHORTCUT_PROVIDER (provider));

  if (self->providers_models == NULL)
    IDE_EXIT;

  if ((model = g_object_get_data (G_OBJECT (provider), "SHORTCUTS_MODEL")))
    {
      guint n_items;

      g_assert (G_IS_LIST_MODEL (model));

      n_items = g_list_model_get_n_items (G_LIST_MODEL (self->providers_models));

      for (guint i = 0; i < n_items; i++)
        {
          g_autoptr(GListModel) item = g_list_model_get_item (G_LIST_MODEL (self->providers_models), i);

          if (item == model)
            {
              g_list_store_remove (self->providers_models, i);
              IDE_EXIT;
            }
        }
    }

  IDE_EXIT;
}

static void
ide_shortcut_manager_parent_set (IdeObject *object,
                                 IdeObject *parent)
{
  IdeShortcutManager *self = (IdeShortcutManager *)object;

  g_assert (IDE_IS_SHORTCUT_MANAGER (self));
  g_assert (!parent || IDE_IS_OBJECT (parent));

  if (parent == NULL)
    return;

  if (self->providers == NULL)
    {
      self->providers = ide_extension_set_adapter_new (IDE_OBJECT (self),
                                                       peas_engine_get_default (),
                                                       IDE_TYPE_SHORTCUT_PROVIDER,
                                                       NULL, NULL);
      g_signal_connect (self->providers,
                        "extension-added",
                        G_CALLBACK (on_provider_added_cb),
                        self);
      g_signal_connect (self->providers,
                        "extension-removed",
                        G_CALLBACK (on_provider_removed_cb),
                        self);
      ide_extension_set_adapter_foreach_by_priority (self->providers,
                                                     on_provider_added_cb,
                                                     self);
    }
}

static void
ide_shortcut_manager_destroy (IdeObject *object)
{
  IdeShortcutManager *self = (IdeShortcutManager *)object;

  g_clear_handle_id (&self->update_triggers_source, g_source_remove);

  g_clear_object (&self->observer);
  g_clear_object (&self->providers);
  g_clear_object (&self->providers_models);
  g_clear_object (&self->plugin_models);
  g_clear_object (&self->user_bundle);
  g_clear_object (&self->toplevel);
  g_clear_object (&self->flatten);

  IDE_OBJECT_CLASS (ide_shortcut_manager_parent_class)->destroy (object);
}

static void
ide_shortcut_manager_class_init (IdeShortcutManagerClass *klass)
{
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  i_object_class->destroy = ide_shortcut_manager_destroy;
  i_object_class->parent_set = ide_shortcut_manager_parent_set;

  g_type_ensure (IDE_TYPE_SHORTCUT_PROVIDER);
}

static void
ide_shortcut_manager_init (IdeShortcutManager *self)
{
  GtkFlattenListModel *flatten;

  if (plugin_models == NULL)
    plugin_models = g_list_store_new (G_TYPE_LIST_MODEL);

  self->toplevel = g_list_store_new (G_TYPE_LIST_MODEL);

  if (user_bundle == NULL)
    {
      g_autoptr(GFile) user_file = NULL;

      user_file = g_file_new_build_filename (g_get_user_config_dir (),
                                             "gnome-builder",
                                             "keybindings.json",
                                             NULL);
      user_bundle = ide_shortcut_bundle_new_for_user (user_file);
    }

  /* We monitor the user-bundle for changes so that we can
   * handle any sort of override by replacing the trigger
   * of the original shortcut.
   */
  g_signal_connect_object (user_bundle,
                           "items-changed",
                           G_CALLBACK (ide_shortcut_manager_user_items_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  /* Setup user shortcuts at highest priority */
  g_list_store_append (self->toplevel, user_bundle);

  /* Then add providers implemented by plugins */
  self->providers_models = g_list_store_new (G_TYPE_LIST_MODEL);
  flatten = gtk_flatten_list_model_new (g_object_ref (G_LIST_MODEL (self->providers_models)));
  g_list_store_append (self->toplevel, flatten);
  g_object_unref (flatten);

  /* Then add keybindings.json found within plugin resources */
  self->plugin_models = g_object_ref (plugin_models);
  flatten = gtk_flatten_list_model_new (g_object_ref (G_LIST_MODEL (self->plugin_models)));
  g_list_store_append (self->toplevel, flatten);
  g_object_unref (flatten);

  /* Then attach our internal plugins */
  g_list_store_append (self->toplevel, get_internal_shortcuts ());

  /* And finally flatten the whole thing into a shortcut list */
  self->flatten = gtk_flatten_list_model_new (g_object_ref (G_LIST_MODEL (self->toplevel)));
  g_signal_connect_object (self->flatten,
                           "items-changed",
                           G_CALLBACK (ide_shortcut_manager_items_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  /* Build a "compiled" shortcut map to make remappings easier */
  self->observer = ide_shortcut_observer_new (G_LIST_MODEL (self->flatten));

  /* Apply user-bundle overrides to plugin models/etc */
  ide_shortcut_manager_update_overrides (self);
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

void
ide_shortcut_manager_add_resources (const char *resource_path)
{
  g_autoptr(GFile) keybindings_json = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *keybindings_json_path = NULL;
  g_autoptr(IdeShortcutBundle) bundle = NULL;

  g_return_if_fail (resource_path != NULL);

  keybindings_json_path = g_build_filename (resource_path, "gtk", "keybindings.json", NULL);

  if (g_str_has_prefix (resource_path, "resource://"))
    keybindings_json = g_file_new_for_uri (keybindings_json_path);
  else
    keybindings_json = g_file_new_for_path (keybindings_json_path);

  if (!g_file_query_exists (keybindings_json, NULL))
    return;

  bundle = ide_shortcut_bundle_new ();

  if (!ide_shortcut_bundle_parse (bundle, keybindings_json, &error))
    {
      g_warning ("Failed to parse %s: %s", resource_path, error->message);
      return;
    }

  g_object_set_data_full (G_OBJECT (bundle),
                          "RESOURCE_PATH",
                          g_strdup (resource_path),
                          g_free);

  if (plugin_models == NULL)
    plugin_models = g_list_store_new (G_TYPE_LIST_MODEL);

  g_list_store_append (plugin_models, bundle);
}

void
ide_shortcut_manager_remove_resources (const char *resource_path)
{
  guint n_items;

  g_return_if_fail (resource_path != NULL);
  g_return_if_fail (plugin_models != NULL);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (plugin_models));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(IdeShortcutBundle) bundle = g_list_model_get_item (G_LIST_MODEL (plugin_models), i);

      if (g_strcmp0 (resource_path, g_object_get_data (G_OBJECT (bundle), "RESOURCE_PATH")) == 0)
        {
          g_list_store_remove (plugin_models, i);
          return;
        }
    }
}

IdeShortcutObserver *
ide_shortcut_manager_get_observer (IdeShortcutManager *self)
{
  g_return_val_if_fail (IDE_IS_SHORTCUT_MANAGER (self), NULL);

  return self->observer;
}

void
ide_shortcut_manager_reset_user (void)
{
  g_autoptr(GFile) file = g_file_new_build_filename (g_get_user_config_dir (),
                                                     "gnome-builder",
                                                     "keybindings.json",
                                                     NULL);
  g_file_delete (file, NULL, NULL);
}

IdeShortcutBundle *
ide_shortcut_manager_get_user_bundle (void)
{
  return user_bundle;
}
