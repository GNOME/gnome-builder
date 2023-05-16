/* ide-menu-manager.c
 *
 * Copyright (C) 2015-2023 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "ide-menu-manager"

#include "config.h"

#include <gtk/gtk.h>
#include <string.h>

#include "ide-menu-manager.h"

struct _IdeMenuManager
{
  GObject      parent_instance;
  guint        last_merge_id;
  GHashTable  *models;
  char       **cached_keys;
};

G_DEFINE_TYPE (IdeMenuManager, ide_menu_manager, G_TYPE_OBJECT)

#define IDE_MENU_ATTRIBUTE_BEFORE   "before"
#define IDE_MENU_ATTRIBUTE_AFTER    "after"
#define IDE_MENU_ATTRIBUTE_MERGE_ID "ide-merge-id"

/**
 * IdeMenuManager:
 *
 * The goal of #IdeMenuManager is to simplify the process of merging multiple
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
 * submenu links. This allows the #IdeMenuManager to be in full control of
 * the generated menus.
 *
 * ide_menu_manager_get_menu_by_id() will always return a #GMenu, however
 * that menu may contain no children until something has extended it later
 * on during the application process.
 */

static const gchar *
get_object_id (GObject *object)
{
  g_assert (G_IS_OBJECT (object));

  if (GTK_IS_BUILDABLE (object))
    return gtk_buildable_get_buildable_id (GTK_BUILDABLE (object));
  else
    return g_object_get_data (object, "gtk-builder-id");
}

static void
ide_menu_manager_dispose (GObject *object)
{
  IdeMenuManager *self = (IdeMenuManager *)object;

  g_clear_pointer (&self->models, g_hash_table_unref);
  g_clear_pointer (&self->cached_keys, g_strfreev);

  G_OBJECT_CLASS (ide_menu_manager_parent_class)->dispose (object);
}

static void
ide_menu_manager_class_init (IdeMenuManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_menu_manager_dispose;
}

static void
ide_menu_manager_init (IdeMenuManager *self)
{
  self->models = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
}

static gint
find_with_attribute_string (GMenuModel  *model,
                            const gchar *attribute,
                            const gchar *value)
{
  guint n_items;

  g_assert (G_IS_MENU_MODEL (model));
  g_assert (attribute != NULL);
  g_assert (value != NULL);

  n_items = g_menu_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autofree gchar *item_value = NULL;

      if (g_menu_model_get_item_attribute (model, i, attribute, "s", &item_value) &&
          (g_strcmp0 (value, item_value) == 0))
        return i;
    }

  return -1;
}

static gboolean
ide_menu_manager_menu_contains (IdeMenuManager *self,
                                GMenu          *menu,
                                GMenuItem      *item)
{
  const char *id;
  const char *label;
  const char *link_id;

  g_assert (IDE_IS_MENU_MANAGER (self));
  g_assert (G_IS_MENU (menu));
  g_assert (G_IS_MENU_ITEM (item));

  /* try to find match by item link */
  if (g_menu_item_get_attribute (item, "ide-link-id", "&s", &link_id) &&
      (find_with_attribute_string (G_MENU_MODEL (menu), "ide-link-id", link_id) >= 0))
    return TRUE;

  /* if this item has an "id" and that id is not found in the model,
   * then assume it is different even if another item with the same label
   * appears, as that could be an item that gets hidden.
   */
  if (g_menu_item_get_attribute (item, "id", "&s", &id) &&
      !ide_str_empty0 (id) &&
      (find_with_attribute_string (G_MENU_MODEL (menu), "id", id) < 0))
    return FALSE;

  /* try to find  match by item label */
  if (g_menu_item_get_attribute (item, G_MENU_ATTRIBUTE_LABEL, "&s", &label) &&
      (find_with_attribute_string (G_MENU_MODEL (menu), G_MENU_ATTRIBUTE_LABEL, label) >= 0))
    return TRUE;

  return FALSE;
}

static void
model_copy_attributes_to_item (GMenuModel *model,
                               gint        item_index,
                               GMenuItem  *item)
{
  g_autoptr(GMenuAttributeIter) iter = NULL;
  const gchar *attr_name;
  GVariant *attr_value;

  g_assert (G_IS_MENU_MODEL (model));
  g_assert (item_index >= 0);
  g_assert (G_IS_MENU_ITEM (item));

  if (!(iter = g_menu_model_iterate_item_attributes (model, item_index)))
    return;

  while (g_menu_attribute_iter_get_next (iter, &attr_name, &attr_value))
    {
      g_menu_item_set_attribute_value (item, attr_name, attr_value);
      g_variant_unref (attr_value);
    }
}

static void
model_copy_links_to_item (GMenuModel *model,
                          guint       position,
                          GMenuItem  *item)
{
  g_autoptr(GMenuLinkIter) link_iter = NULL;

  g_assert (G_IS_MENU_MODEL (model));
  g_assert (G_IS_MENU_ITEM (item));

  link_iter = g_menu_model_iterate_item_links (model, position);

  while (g_menu_link_iter_next (link_iter))
    {
      g_autoptr(GMenuModel) link_model = NULL;
      const gchar *link_name;

      link_name = g_menu_link_iter_get_name (link_iter);
      link_model = g_menu_link_iter_get_value (link_iter);

      g_menu_item_set_link (item, link_name, link_model);
    }
}

static void
menu_move_item_to (GMenu *menu,
                   guint  position,
                   guint  new_position)
{
  g_autoptr(GMenuItem) item = NULL;

  g_assert (G_IS_MENU (menu));

  item = g_menu_item_new (NULL, NULL);
  model_copy_attributes_to_item (G_MENU_MODEL (menu), position, item);
  model_copy_links_to_item (G_MENU_MODEL (menu), position, item);

  g_menu_remove (menu, position);
  g_menu_insert_item (menu, new_position, item);
}

static void
ide_menu_manager_resolve_constraints (GMenu *menu)
{
  GMenuModel *model = (GMenuModel *)menu;
  gint n_items;

  g_assert (G_IS_MENU (menu));

  n_items = (gint)g_menu_model_get_n_items (G_MENU_MODEL (menu));

  /*
   * We start iterating forwards. As we look at each row, we start
   * again from the end working backwards to see if we need to be
   * moved after that row.
   *
   * This way we know we see the furthest we might need to jump first.
   */

  for (gint i = 0; i < n_items; i++)
    {
      g_autofree gchar *i_after = NULL;

      g_menu_model_get_item_attribute (model, i, IDE_MENU_ATTRIBUTE_AFTER, "s", &i_after);
      if (i_after == NULL)
        continue;

      /* Work our way backwards from the end back to
       * our current position (but not overlapping).
       */
      for (gint j = n_items - 1; j > i; j--)
        {
          g_autofree gchar *j_id = NULL;
          g_autofree gchar *j_label = NULL;

          g_menu_model_get_item_attribute (model, j, "id", "s", &j_id);
          g_menu_model_get_item_attribute (model, j, "label", "s", &j_label);

          if (ide_str_equal0 (i_after, j_id) || ide_str_equal0 (i_after, j_label))
            {
              /* You might think we need to place the item *AFTER*
               * our position "j". But since we remove the row where
               * "i" currently is, we get the proper location.
               */
              menu_move_item_to (menu, i, j);
              i--;
              break;
            }
        }
    }

  /*
   * Now we need to apply the same thing but for the "before" links
   * in our model. To do this, we also want to ensure we find the
   * furthest jump first. So we start from the end and work our way
   * towards the front and for each of those nodes, start from the
   * front and work our way back.
   */

  for (gint i = n_items - 1; i >= 0; i--)
    {
      g_autofree gchar *i_before = NULL;

      g_menu_model_get_item_attribute (model, i, IDE_MENU_ATTRIBUTE_BEFORE, "s", &i_before);
      if (i_before == NULL)
        continue;

      /* Work our way from the front back towards our current position
       * that would cause our position to jump.
       */
      for (gint j = 0; j < i; j++)
        {
          g_autofree gchar *j_id = NULL;
          g_autofree gchar *j_label = NULL;

          g_menu_model_get_item_attribute (model, j, "id", "s", &j_id);
          g_menu_model_get_item_attribute (model, j, "label", "s", &j_label);

          if (ide_str_equal0 (i_before, j_id) || ide_str_equal0 (i_before, j_label))
            {
              /*
               * This item needs to be placed before this item we just found.
               * Since that is the furthest we could jump, just stop
               * afterwards.
               */
              menu_move_item_to (menu, i, j);
              i++;
              break;
            }
        }
    }
}

static void
ide_menu_manager_add_to_menu (IdeMenuManager *self,
                              GMenu          *menu,
                              GMenuItem      *item)
{
  g_assert (IDE_IS_MENU_MANAGER (self));
  g_assert (G_IS_MENU (menu));
  g_assert (G_IS_MENU_ITEM (item));

  /*
   * The proplem here is one that could end up being an infinite
   * loop if we tried to resolve all the position requirements
   * until no more position changes were required. So instead we
   * simplify the problem into an append, and two-passes as trying
   * to fix up the positions.
   */
  g_menu_append_item (menu, item);
  ide_menu_manager_resolve_constraints (menu);
  ide_menu_manager_resolve_constraints (menu);
}

static void
ide_menu_manager_merge_model (IdeMenuManager *self,
                              GMenu          *menu,
                              GMenuModel     *model,
                              guint           merge_id)
{
  guint n_items;

  g_assert (IDE_IS_MENU_MANAGER (self));
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
   * @menu, we add it taking into account %IDE_MENU_ATTRIBUTE_BEFORE and
   * %IDE_MENU_ATTRIBUTE_AFTER.
   */

  n_items = g_menu_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
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
      g_menu_item_set_attribute (item, IDE_MENU_ATTRIBUTE_MERGE_ID, "u", merge_id);

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
          g_menu_item_set_attribute (item, "ide-link-id", "s", link_id);

          g_menu_item_set_link (item, link_name, internal_menu);
        }

      /*
       * If the menu already has this item, that's fine. We will populate
       * the submenu/section links in followup merges of their GMenuModel.
       */
      if (ide_menu_manager_menu_contains (self, menu, item))
        continue;

      ide_menu_manager_add_to_menu (self, menu, item);
    }
}

static void
ide_menu_manager_merge_builder (IdeMenuManager *self,
                                GtkBuilder     *builder,
                                guint           merge_id)
{
  const GSList *iter;
  GSList *list;

  g_assert (IDE_IS_MENU_MANAGER (self));
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
        {
          g_hash_table_insert (self->models, g_strdup (id), g_menu_new ());
          g_clear_pointer (&self->cached_keys, g_strfreev);
        }
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

      ide_menu_manager_merge_model (self, menu, G_MENU_MODEL (object), merge_id);
    }

  g_slist_free (list);
}

IdeMenuManager *
ide_menu_manager_new (void)
{
  return g_object_new (IDE_TYPE_MENU_MANAGER, NULL);
}

guint
ide_menu_manager_add_filename (IdeMenuManager  *self,
                               const gchar     *filename,
                               GError         **error)
{
  GtkBuilder *builder;
  guint merge_id;

  g_return_val_if_fail (IDE_IS_MENU_MANAGER (self), 0);
  g_return_val_if_fail (filename != NULL, 0);

  builder = gtk_builder_new ();

  if (!gtk_builder_add_from_file (builder, filename, error))
    {
      g_object_unref (builder);
      return 0;
    }

  merge_id = ++self->last_merge_id;
  ide_menu_manager_merge_builder (self, builder, merge_id);
  g_object_unref (builder);

  return merge_id;
}

guint
ide_menu_manager_merge (IdeMenuManager *self,
                        const gchar    *menu_id,
                        GMenuModel     *menu_model)
{
  GMenu *menu;
  guint merge_id;

  g_return_val_if_fail (IDE_IS_MENU_MANAGER (self), 0);
  g_return_val_if_fail (menu_id != NULL, 0);
  g_return_val_if_fail (G_IS_MENU_MODEL (menu_model), 0);

  merge_id = ++self->last_merge_id;

  if (!(menu = g_hash_table_lookup (self->models, menu_id)))
    {
      GMenu *new_model = g_menu_new ();
      g_hash_table_insert (self->models, g_strdup (menu_id), new_model);
      g_clear_pointer (&self->cached_keys, g_strfreev);
      menu = new_model;
    }

  ide_menu_manager_merge_model (self, menu, menu_model, merge_id);

  return merge_id;
}

/**
 * ide_menu_manager_remove:
 * @self: a #IdeMenuManager
 * @merge_id: A previously registered merge id
 *
 * This removes items from menus that were added as part of a previous
 * menu merge. Use the value returned from ide_menu_manager_merge() as
 * the @merge_id.
 */
void
ide_menu_manager_remove (IdeMenuManager *self,
                         guint           merge_id)
{
  GHashTableIter iter;
  GMenu *menu;

  g_return_if_fail (IDE_IS_MENU_MANAGER (self));
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
          guint item_merge_id = 0;

          if (g_menu_model_get_item_attribute (G_MENU_MODEL (menu),
                                               i,
                                               IDE_MENU_ATTRIBUTE_MERGE_ID,
                                               "u", &item_merge_id))
            {
              if (item_merge_id == merge_id)
                g_menu_remove (menu, i);
            }
        }
    }

  g_clear_pointer (&self->cached_keys, g_strfreev);
}

/**
 * ide_menu_manager_get_menu_by_id:
 *
 * Returns: (transfer none): A #GMenu.
 */
GMenu *
ide_menu_manager_get_menu_by_id (IdeMenuManager *self,
                                 const gchar    *menu_id)
{
  GMenu *menu;

  g_return_val_if_fail (IDE_IS_MENU_MANAGER (self), NULL);
  g_return_val_if_fail (menu_id != NULL, NULL);

  menu = g_hash_table_lookup (self->models, menu_id);

  if (menu == NULL)
    {
      menu = g_menu_new ();
      g_hash_table_insert (self->models, g_strdup (menu_id), menu);
      g_clear_pointer (&self->cached_keys, g_strfreev);
    }

  return menu;
}

guint
ide_menu_manager_add_resource (IdeMenuManager  *self,
                               const gchar     *resource,
                               GError         **error)
{
  GtkBuilder *builder;
  guint merge_id;

  g_return_val_if_fail (IDE_IS_MENU_MANAGER (self), 0);
  g_return_val_if_fail (resource != NULL, 0);

  if (g_str_has_prefix (resource, "resource://"))
    resource += strlen ("resource://");

  builder = gtk_builder_new ();

  if (!gtk_builder_add_from_resource (builder, resource, error))
    {
      g_object_unref (builder);
      return 0;
    }

  merge_id = ++self->last_merge_id;
  ide_menu_manager_merge_builder (self, builder, merge_id);
  g_object_unref (builder);

  return merge_id;
}

const char * const *
ide_menu_manager_get_menu_ids (IdeMenuManager *self)
{
  g_return_val_if_fail (IDE_IS_MENU_MANAGER (self), NULL);

  if (self->cached_keys == NULL)
    {
      g_autofree gpointer *keys = g_hash_table_get_keys_as_array (self->models, NULL);
      self->cached_keys = g_strdupv ((char **)keys);
    }

  return (const char * const *)self->cached_keys;
}

void
ide_menu_manager_set_attribute_string (IdeMenuManager *self,
                                       GMenu          *menu,
                                       guint           position,
                                       const char     *attribute,
                                       const char     *value)
{
  g_autoptr(GMenuItem) item = NULL;

  g_return_if_fail (IDE_IS_MENU_MANAGER (self));
  g_return_if_fail (G_IS_MENU (menu));
  g_return_if_fail (attribute != NULL);

  item = g_menu_item_new (NULL, NULL);

  model_copy_attributes_to_item (G_MENU_MODEL (menu), position, item);
  model_copy_links_to_item (G_MENU_MODEL (menu), position, item);
  g_menu_item_set_attribute (item, attribute, "s", value);

  g_menu_remove (menu, position);
  g_menu_insert_item (menu, position, item);
}

/**
 * ide_menu_manager_find_item_by_id:
 * @self: a #IdeMenuManager
 * @id: the identifier of the menu item
 * @position: (out): the position within the resulting menu
 *
 * Locates a menu item that matches @id and sets the position within
 * the resulting #GMenu to @position.
 *
 * If no match is found, %NULL is returned.
 *
 * Returns: (transfer none) (nullable): a #GMenu if successful; otherwise
 *   %NULL and @position is unset.
 */
GMenu *
ide_menu_manager_find_item_by_id (IdeMenuManager *self,
                                  const char     *id,
                                  guint          *position)
{
  GHashTableIter iter;
  GMenu *menu = NULL;

  g_return_val_if_fail (IDE_IS_MENU_MANAGER (self), NULL);

  if (id == NULL)
    return NULL;

  g_hash_table_iter_init (&iter, self->models);

  while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&menu))
    {
      guint n_items = g_menu_model_get_n_items (G_MENU_MODEL (menu));

      for (guint i = 0; i < n_items; i++)
        {
          g_autofree char *item_id = NULL;

          if (g_menu_model_get_item_attribute (G_MENU_MODEL (menu), i, "id", "s", &item_id) &&
              ide_str_equal0 (id, item_id))
            {
              if (position != NULL)
                *position = i;
              return menu;
            }
        }
    }

  return NULL;
}
