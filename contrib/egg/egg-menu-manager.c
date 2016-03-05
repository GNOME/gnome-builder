/* egg-menu-manager.c
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "egg-menu-manager.h"

struct _EggMenuManager
{
  GObject     parent_instance;

  guint       last_merge_id;
  GHashTable *models;
};

G_DEFINE_TYPE (EggMenuManager, egg_menu_manager, G_TYPE_OBJECT)

#define EGG_MENU_ATTRIBUTE_BEFORE   "before"
#define EGG_MENU_ATTRIBUTE_AFTER    "after"
#define EGG_MENU_ATTRIBUTE_MERGE_ID "egg-merge-id"

/**
 * SECTION:egg-menu-manager
 * @short_description: UI merging for menus
 *
 * The goal of #EggMenuManager is to simplify the process of merging multiple
 * GtkBuilder .ui files containing menus into a single representation of the
 * application menus. Additionally, it provides the ability to "unmerge"
 * previously merged menus.
 *
 * This allows for an application to have plugins which seemlessly extends
 * the core application menus.
 *
 * Implementation notes:
 *
 * To make this work, we don't use the GMenu instances created by a GtkBuilder
 * instance. Instead, we create the menus ourself and recreate section and
 * submenu links. This allows the #EggMenuManager to be in full control of
 * the generated menus.
 *
 * egg_menu_manager_get_menu_by_id() will always return a #GMenu, however
 * that menu may contain no children until something has extended it later
 * on during the application process.
 */

static const gchar *
get_object_id (GObject *object)
{
  g_assert (G_IS_OBJECT (object));

  if (GTK_IS_BUILDABLE (object))
    return gtk_buildable_get_name (GTK_BUILDABLE (object));
  else
    return g_object_get_data (object, "gtk-builder-name");
}

static void
egg_menu_manager_finalize (GObject *object)
{
  EggMenuManager *self = (EggMenuManager *)object;

  g_clear_pointer (&self->models, g_hash_table_unref);

  G_OBJECT_CLASS (egg_menu_manager_parent_class)->finalize (object);
}

static void
egg_menu_manager_class_init (EggMenuManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = egg_menu_manager_finalize;
}

static void
egg_menu_manager_init (EggMenuManager *self)
{
  self->models = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
}

static gint
find_with_attribute_string (GMenuModel  *model,
                            const gchar *attribute,
                            const gchar *value)
{
  guint n_items;
  gint i;

  g_assert (G_IS_MENU_MODEL (model));
  g_assert (attribute != NULL);
  g_assert (value != NULL);

  n_items = g_menu_model_get_n_items (model);

  for (i = 0; i < n_items; i++)
    {
      g_autofree gchar *item_value = NULL;

      if (g_menu_model_get_item_attribute (model, i, attribute, "s", &item_value) &&
          (g_strcmp0 (value, item_value) == 0))
        return i;
    }

  return -1;
}

static gboolean
egg_menu_manager_menu_contains (EggMenuManager *self,
                                GMenu          *menu,
                                GMenuItem      *item)
{
  const gchar *link_id;
  const gchar *label;

  g_assert (EGG_IS_MENU_MANAGER (self));
  g_assert (G_IS_MENU (menu));
  g_assert (G_IS_MENU_ITEM (item));

  /* try to find  match by item label */
  if (g_menu_item_get_attribute (item, G_MENU_ATTRIBUTE_LABEL, "&s", &label) &&
      (find_with_attribute_string (G_MENU_MODEL (menu), G_MENU_ATTRIBUTE_LABEL, label) >= 0))
    return TRUE;

  /* try to find match by item link */
  if (g_menu_item_get_attribute (item, "egg-link-id", "&s", &link_id) &&
      (find_with_attribute_string (G_MENU_MODEL (menu), "egg-link-id", link_id) >= 0))
    return TRUE;

  return FALSE;
}

static void
model_copy_attributes_to_item (GMenuModel *model,
                               gint        item_index,
                               GMenuItem  *item)
{
  GMenuAttributeIter *iter;
  const gchar *attr_name;
  GVariant *attr_value;

  g_assert (G_IS_MENU_MODEL (model));
  g_assert (item_index >= 0);
  g_assert (G_IS_MENU_ITEM (item));

  iter = g_menu_model_iterate_item_attributes (model, item_index);
  while (g_menu_attribute_iter_get_next (iter, &attr_name, &attr_value))
    g_menu_item_set_attribute_value (item, attr_name, attr_value);
  g_object_unref (iter);
}

static gint
find_position_for_item (GMenuModel *model,
                        GMenuItem  *item)
{
  const gchar *after;
  const gchar *before;
  gint before_pos = -1;
  gint after_pos = -1;

  g_assert (G_IS_MENU_MODEL (model));
  g_assert (G_IS_MENU_ITEM (item));

  if (!g_menu_item_get_attribute (item, EGG_MENU_ATTRIBUTE_AFTER, "&s", &after))
    after = NULL;

  if (!g_menu_item_get_attribute (item, EGG_MENU_ATTRIBUTE_BEFORE, "&s", &before))
    before = NULL;

  if (after != NULL)
    after_pos = find_with_attribute_string (model, G_MENU_ATTRIBUTE_LABEL, after);

  if (before != NULL)
    before_pos = find_with_attribute_string (model, G_MENU_ATTRIBUTE_LABEL, before);

  /*
   * TODO: Perform resort after all items have been inserted.
   *
   *       We cannot resolve all positions incrementally as we add them since
   *       plugin ordering cannot be guaranteed (and some relative positions
   *       may not yet be available). If relations were symmetrical, that
   *       wouldn't be a problem.
   */

  if (before_pos >= 0)
    return MAX (0, before_pos - 1);

  if (after_pos >= 0)
    return after_pos + 1;

  return -1;
}

static void
egg_menu_manager_add_to_menu (EggMenuManager *self,
                              GMenu          *menu,
                              GMenuItem      *item)
{
  gint position;

  g_assert (EGG_IS_MENU_MANAGER (self));
  g_assert (G_IS_MENU (menu));
  g_assert (G_IS_MENU_ITEM (item));

  position = find_position_for_item (G_MENU_MODEL (menu), item);
  g_menu_insert_item (menu, position, item);
}

static void
egg_menu_manager_merge_model (EggMenuManager *self,
                              GMenu          *menu,
                              GMenuModel     *model,
                              guint           merge_id)
{
  guint n_items;
  gint i;

  g_assert (EGG_IS_MENU_MANAGER (self));
  g_assert (G_IS_MENU (menu));
  g_assert (G_IS_MENU_MODEL (model));
  g_assert (merge_id > 0);

  /*
   * NOTES:
   *
   * Instead of using g_menu_item_new_from_model(), we create our own item
   * and resolve section/submenu links. This allows us to be in full control
   * of all of the menu items created.
   *
   * We move through each item in @model. If that item does not exist within
   * @menu, we add it taking into account %EGG_MENU_ATTRIBUTE_BEFORE and
   * %EGG_MENU_ATTRIBUTE_AFTER.
   */

  n_items = g_menu_model_get_n_items (model);

  for (i = 0; i < n_items; i++)
    {
      g_autoptr(GMenuItem) item = NULL;
      g_autoptr(GMenuLinkIter) link_iter = NULL;

      item = g_menu_item_new (NULL, NULL);

      /*
       * Copy attributes from the model. This includes, label, action,
       * target, before, after, etc. Also set our merge-id so that we
       * can remove the item when we are unmerged.
       */
      model_copy_attributes_to_item (model, i, item);
      g_menu_item_set_attribute (item, EGG_MENU_ATTRIBUTE_MERGE_ID, "u", merge_id);

      /*
       * If this is a link, resolve it from our already created GMenu.
       * The menu might be empty now, but it will get filled in on a
       * followup pass for that model.
       */
      link_iter = g_menu_model_iterate_item_links (model, i);
      while (g_menu_link_iter_next (link_iter))
        {
          g_autoptr(GMenuModel) link_model = NULL;
          const gchar *link_name;
          const gchar *link_id;
          GMenuModel *internal_menu;

          link_name = g_menu_link_iter_get_name (link_iter);
          link_model = g_menu_link_iter_get_value (link_iter);

          g_assert (link_name != NULL);
          g_assert (G_IS_MENU_MODEL (link_model));

          link_id = get_object_id (G_OBJECT (link_model));

          if (link_id == NULL)
            {
              g_warning ("Link of type \"%s\" missing \"id=\". "
                         "Merging will not be possible.",
                         link_name);
              continue;
            }

          internal_menu = g_hash_table_lookup (self->models, link_id);

          if (internal_menu == NULL)
            {
              g_warning ("linked menu %s has not been created", link_id);
              continue;
            }

          /*
           * Save the internal link reference-id to do merging of items
           * later on. We need to know if an item matches when we might
           * not have a "label" to work from.
           */
          g_menu_item_set_attribute (item, "egg-link-id", "s", link_id);

          g_menu_item_set_link (item, link_name, internal_menu);
        }

      /*
       * If the menu already has this item, that's fine. We will populate
       * the submenu/section links in followup merges of their GMenuModel.
       */
      if (egg_menu_manager_menu_contains (self, menu, item))
        continue;

      egg_menu_manager_add_to_menu (self, menu, item);
    }
}

static void
egg_menu_manager_merge (EggMenuManager *self,
                        GtkBuilder     *builder,
                        guint           merge_id)
{
  const GSList *iter;
  GSList *list;

  g_assert (EGG_IS_MENU_MANAGER (self));
  g_assert (GTK_IS_BUILDER (builder));
  g_assert (merge_id > 0);

  /*
   * NOTES:
   *
   * We cannot re-use any of the created GMenu from the builder as we need
   * control over all the created GMenu. Primarily because manipulating
   * existing GMenu is such a PITA. So instead, we create our own GMenu and
   * resolve links manually.
   *
   * Since GtkBuilder requires that all menus have an "id" element, we can
   * resolve the menu->id fairly easily. First we create our own GMenu
   * instances so that we can always resolve them during the creation process.
   * Then we can go through and manually resolve links as we create items.
   *
   * We don't need to recursively create the menus since we will come across
   * additional GMenu instances while iterating the available objects from the
   * GtkBuilder. This does require 2 iterations of the objects, but that is
   * not an issue.
   */

  list = gtk_builder_get_objects (builder);

  /*
   * For every menu with an id, check to see if we already created our
   * instance of that menu. If not, create it now so we can resolve them
   * while building the menu links.
   */
  for (iter = list; iter != NULL; iter = iter->next)
    {
      GObject *object = iter->data;
      const gchar *id;
      GMenu *menu;

      if (!G_IS_MENU (object))
        continue;

      if (!(id = get_object_id (object)))
        {
          g_warning ("menu without identifier, implausible");
          continue;
        }

      if (!(menu = g_hash_table_lookup (self->models, id)))
        g_hash_table_insert (self->models, g_strdup (id), g_menu_new ());
    }

  /*
   * Now build each menu we discovered in the GtkBuilder. We do not need to
   * build them recursively since we will pass the linked menus as we make
   * forward progress on the GtkBuilder created objects.
   */

  for (iter = list; iter != NULL; iter = iter->next)
    {
      GObject *object = iter->data;
      const gchar *id;
      GMenu *menu;

      if (!G_IS_MENU_MODEL (object))
        continue;

      if (!(id = get_object_id (object)))
        continue;

      menu = g_hash_table_lookup (self->models, id);

      g_assert (G_IS_MENU (menu));

      egg_menu_manager_merge_model (self, menu, G_MENU_MODEL (object), merge_id);
    }

  g_slist_free (list);
}

EggMenuManager *
egg_menu_manager_new (void)
{
  return g_object_new (EGG_TYPE_MENU_MANAGER, NULL);
}

guint
egg_menu_manager_add_filename (EggMenuManager  *self,
                               const gchar     *filename,
                               GError         **error)
{
  GtkBuilder *builder;
  guint merge_id;

  g_return_val_if_fail (EGG_IS_MENU_MANAGER (self), 0);
  g_return_val_if_fail (filename != NULL, 0);

  builder = gtk_builder_new ();

  if (!gtk_builder_add_from_file (builder, filename, error))
    {
      g_object_unref (builder);
      return 0;
    }

  merge_id = ++self->last_merge_id;
  egg_menu_manager_merge (self, builder, merge_id);
  g_object_unref (builder);

  return merge_id;
}

void
egg_menu_manager_remove (EggMenuManager *self,
                         guint           merge_id)
{
  GHashTableIter iter;
  GMenu *menu;

  g_return_if_fail (EGG_IS_MENU_MANAGER (self));
  g_return_if_fail (merge_id != 0);

  g_hash_table_iter_init (&iter, self->models);

  while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&menu))
    {
      gint n_items;
      gint i;

      g_assert (G_IS_MENU (menu));

      n_items = g_menu_model_get_n_items (G_MENU_MODEL (menu));

      /* Iterate backward so we have a stable loop variable. */
      for (i = n_items - 1; i >= 0; i--)
        {
          guint item_merge_id;

          if (g_menu_model_get_item_attribute (G_MENU_MODEL (menu),
                                               i,
                                               EGG_MENU_ATTRIBUTE_MERGE_ID,
                                               "u", &item_merge_id))
            if (item_merge_id == merge_id)
              g_menu_remove (menu, i);
        }
    }
}

/**
 * egg_menu_manager_get_menu_by_id:
 *
 * Returns: (transfer none): A #GMenu.
 */
GMenu *
egg_menu_manager_get_menu_by_id (EggMenuManager *self,
                                 const gchar    *menu_id)
{
  GMenu *menu;

  g_return_val_if_fail (EGG_IS_MENU_MANAGER (self), NULL);
  g_return_val_if_fail (menu_id != NULL, NULL);

  menu = g_hash_table_lookup (self->models, menu_id);

  if (menu == NULL)
    {
      menu = g_menu_new ();
      g_hash_table_insert (self->models, g_strdup (menu_id), menu);
    }

  return menu;
}

guint
egg_menu_manager_add_resource (EggMenuManager  *self,
                               const gchar     *resource,
                               GError         **error)
{
  GtkBuilder *builder;
  guint merge_id;

  g_return_val_if_fail (EGG_IS_MENU_MANAGER (self), 0);
  g_return_val_if_fail (resource != NULL, 0);

  builder = gtk_builder_new ();

  if (!gtk_builder_add_from_resource (builder, resource, error))
    {
      g_object_unref (builder);
      return 0;
    }

  merge_id = ++self->last_merge_id;
  egg_menu_manager_merge (self, builder, merge_id);
  g_object_unref (builder);

  return merge_id;
}
