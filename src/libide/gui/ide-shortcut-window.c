/* ide-shortcut-window.c
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

#define G_LOG_DOMAIN "ide-shortcut-window"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-gtk.h>

#include "ide-application-private.h"
#include "ide-shortcut-bundle-private.h"
#include "ide-shortcut-info.h"
#include "ide-shortcut-window-private.h"

typedef struct _ShortcutInfo
{
  GList link;
  char *accel;
  char *action;
  char *icon_name;
  char *subtitle;
  GVariant *target;
  char *title;
  const char *group;
  const char *page;
  const char *id;
} ShortcutInfo;

typedef struct _GroupInfo
{
  GList link;
  GQueue shortcuts;
  const char *title;
} GroupInfo;

typedef struct _PageInfo
{
  GList link;
  GQueue groups;
  const char *title;
} PageInfo;

static void
shortcut_info_free (ShortcutInfo *si)
{
  g_assert (si->link.data == si);
  g_assert (si->link.prev == NULL);
  g_assert (si->link.next == NULL);

  si->link.data = NULL;
  si->page = NULL;
  si->group = NULL;

  g_clear_pointer (&si->accel, g_free);
  g_clear_pointer (&si->action, g_free);
  g_clear_pointer (&si->icon_name, g_free);
  g_clear_pointer (&si->subtitle, g_free);
  g_clear_pointer (&si->target, g_variant_unref);
  g_clear_pointer (&si->title, g_free);

  g_slice_free (ShortcutInfo, si);
}

static void
group_info_free (GroupInfo *gi)
{
  g_assert (gi->link.data == gi);
  g_assert (gi->link.prev == NULL);
  g_assert (gi->link.next == NULL);

  while (gi->shortcuts.length)
    {
      ShortcutInfo *si = g_queue_peek_head (&gi->shortcuts);
      g_queue_unlink (&gi->shortcuts, &si->link);
      shortcut_info_free (si);
    }

  gi->link.data = NULL;
  gi->title = NULL;

  g_slice_free (GroupInfo, gi);
}

static void
page_info_free (PageInfo *pi)
{
  g_assert (pi->link.data == pi);
  g_assert (pi->link.prev == NULL);
  g_assert (pi->link.next == NULL);

  while (pi->groups.length)
    {
      GroupInfo *gi = g_queue_peek_head (&pi->groups);
      g_queue_unlink (&pi->groups, &gi->link);
      group_info_free (gi);
    }

  pi->link.data = NULL;
  pi->title = NULL;

  g_slice_free (PageInfo, pi);
}

static char *
find_accel_for_action (GHashTable *accel_map,
                       const char *action)
{
  g_autofree char *alt = NULL;
  const char *split;
  const char *accel;

  g_assert (accel_map != NULL);

  if (action == NULL)
    return NULL;

  if ((accel = g_hash_table_lookup (accel_map, action)))
    return g_strdup (accel);

  if ((split = strstr (action, "::")))
    alt = g_strdup (split + 2);
  else if ((split = strchr (action, '(')) && split[1] != 0)
    alt = g_strndup (split+1, strlen (split)-2);
  else
    return NULL;

  return g_strdup (g_hash_table_lookup (accel_map, alt));
}

static void
populate_from_menu_model (GQueue     *queue,
                          GHashTable *accel_map,
                          const char *page,
                          const char *group,
                          GMenuModel *menu)
{
  guint n_items;

  g_assert (queue != NULL);
  g_assert (accel_map != NULL);
  g_assert (G_IS_MENU_MODEL (menu));

  n_items = g_menu_model_get_n_items (menu);

  for (guint i = 0; i < n_items; i++)
    {
      g_autofree char *id = NULL;
      g_autofree char *action = NULL;
      g_autofree char *icon_name = NULL;
      g_autofree char *item_group = NULL;
      g_autofree char *item_page = NULL;
      g_autofree char *accel = NULL;
      g_autofree char *subtitle = NULL;
      g_autofree char *title = NULL;
      g_autoptr(GVariant) target = NULL;
      ShortcutInfo *si;

      if (!g_menu_model_get_item_attribute (menu, i, "action", "s", &action))
        continue;

      if (!(accel = find_accel_for_action (accel_map, action)) &&
          !g_menu_model_get_item_attribute (menu, i, "accel", "s", &accel))
        {
          /* Do nothing, we want to still know about it even if we
           * don't have an accel so we can get page/group info from
           * various utilities.
           */
        }

      if (!g_menu_model_get_item_attribute (menu, i, "label", "s", &title))
        continue;

      target = g_menu_model_get_item_attribute_value (menu, i, "target", NULL);

      g_menu_model_get_item_attribute (menu, i, "id", "s", &id);
      g_menu_model_get_item_attribute (menu, i, "description", "s", &subtitle);
      g_menu_model_get_item_attribute (menu, i, "verb-icon", "s", &icon_name);
      g_menu_model_get_item_attribute (menu, i, "page", "s", &item_page);
      g_menu_model_get_item_attribute (menu, i, "group", "s", &item_group);

      si = g_slice_new0 (ShortcutInfo);
      si->link.data = si;
      si->id = g_intern_string (id);
      si->accel = g_steal_pointer (&accel);
      si->icon_name = g_steal_pointer (&icon_name);
      si->subtitle = g_steal_pointer (&subtitle);
      si->title = g_steal_pointer (&title);
      si->page = item_page ? g_intern_string (item_page) : page;
      si->group = item_group ? g_intern_string (item_group) : group;
      si->action = g_steal_pointer (&action);
      si->target = g_steal_pointer (&target);

      g_queue_push_head_link (queue, &si->link);
    }
}

static void
populate_page_and_group (GHashTable *page_map,
                         GHashTable *group_map,
                         GMenuModel *menu)
{
  guint n_items;

  g_assert (page_map != NULL);
  g_assert (group_map != NULL);
  g_assert (G_IS_MENU_MODEL (menu));

  n_items = g_menu_model_get_n_items (menu);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(GMenuLinkIter) iter = NULL;
      g_autoptr(GMenuModel) other = NULL;
      g_autofree char *page = NULL;
      g_autofree char *group = NULL;
      const char *link = NULL;

      if (!g_menu_model_get_item_attribute (menu, i, "page", "s", &page) ||
          !g_menu_model_get_item_attribute (menu, i, "group", "s", &group))
        continue;

      if (!(iter = g_menu_model_iterate_item_links (menu, i)))
        continue;

      while (g_menu_link_iter_get_next (iter, &link, &other))
        {
          g_hash_table_insert (page_map, other, (char *)g_intern_string (page));
          g_hash_table_insert (group_map, other, (char *)g_intern_string (group));
        }
    }
}

static PageInfo *
find_page (GQueue     *pages,
           const char *page)
{
  PageInfo *pi;

  if (page == NULL)
    page = g_intern_string (_("Other"));

  for (const GList *iter = pages->head; iter; iter = iter->next)
    {
      pi = iter->data;

      if (pi->title == page)
        return pi;
    }

  pi = g_slice_new0 (PageInfo);
  pi->link.data = pi;
  pi->title = page;

  g_queue_push_head_link (pages, &pi->link);

  return pi;
}

static GroupInfo *
find_group (GQueue     *groups,
            const char *group)
{
  GroupInfo *gi;

  if (group == NULL)
    group = g_intern_string (_("Other"));

  for (const GList *iter = groups->head; iter; iter = iter->next)
    {
      gi = iter->data;

      if (gi->title == group)
        return gi;
    }

  gi = g_slice_new0 (GroupInfo);
  gi->link.data = gi;
  gi->title = group;

  g_queue_push_head_link (groups, &gi->link);

  return gi;
}

static int
sort_pages_func (const PageInfo *a,
                 const PageInfo *b)
{
  return g_utf8_collate (a->title, b->title);
}

static int
sort_groups_func (const GroupInfo *a,
                  const GroupInfo *b)
{
  return g_utf8_collate (a->title, b->title);
}

static void
remove_underline_and_ellipsis (char *str)
{
  guint i = 0;
  guint j = 0;

  while (str[j])
    {
      if (str[j] == '_')
        {
          j++;
          continue;
        }

      if (str[j] == '.' && str[j+1] == '.' && str[j+2] == '.' && str[j+3] == 0)
        break;

      if (g_str_equal (&str[j], "…"))
        break;

      str[i++] = str[j++];
    }

  str[i] = 0;
}

static void
populate_info (GQueue     *pages,
               GHashTable *accel_map)
{
  IdeMenuManager *menu_manager = IDE_APPLICATION_DEFAULT->menu_manager;
  const char * const *menu_ids = ide_menu_manager_get_menu_ids (menu_manager);
  g_autoptr(GHashTable) page_map = g_hash_table_new (NULL, NULL);
  g_autoptr(GHashTable) group_map = g_hash_table_new (NULL, NULL);
  GQueue queue = G_QUEUE_INIT;

  /* Find all of the "links" to sections/subpages/etc and stash
   * any attributes denoting what the page/group should be so
   * that they can be inherited by items.
   */
  for (guint i = 0; menu_ids[i]; i++)
    {
      GMenu *menu;

      if (!(menu = ide_menu_manager_get_menu_by_id (menu_manager, menu_ids[i])))
        continue;

      populate_page_and_group (page_map, group_map, G_MENU_MODEL (menu));
    }

  /* Now populate items using the mined information */
  for (guint i = 0; menu_ids[i]; i++)
    {
      const char *page;
      const char *group;
      GMenu *menu;

      if (!(menu = ide_menu_manager_get_menu_by_id (menu_manager, menu_ids[i])))
        continue;

      page = g_hash_table_lookup (page_map, menu);
      group = g_hash_table_lookup (group_map, menu);

      populate_from_menu_model (&queue, accel_map, page, group, G_MENU_MODEL (menu));
    }

  /* Build our page tree for the shortcuts */
  while (queue.length)
    {
      ShortcutInfo *si = g_queue_peek_head (&queue);
      PageInfo *page = find_page (pages, si->page);
      GroupInfo *group = find_group (&page->groups, si->group);

      g_queue_unlink (&queue, &si->link);
      g_queue_push_head_link (&group->shortcuts, &si->link);
    }

  g_queue_sort (pages, (GCompareDataFunc)sort_pages_func, NULL);
}

GtkWidget *
ide_shortcut_window_new (GListModel *shortcuts)
{
  g_autoptr(GHashTable) accel_map = NULL;
  g_autoptr(GtkBuilder) builder = NULL;
  g_autoptr(GString) xml = NULL;
  GtkWidget *window = NULL;
  GQueue pages = G_QUEUE_INIT;
  guint n_items;

  IDE_ENTRY;

  g_return_val_if_fail (G_IS_LIST_MODEL (shortcuts), NULL);
  g_return_val_if_fail (g_type_is_a (g_list_model_get_item_type (shortcuts), GTK_TYPE_SHORTCUT), NULL);

  accel_map = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  n_items = g_list_model_get_n_items (shortcuts);

  /* First build a hashmap of action names to shortcut triggers */
  for (guint i = n_items; i > 0; i--)
    {
      g_autoptr(GtkShortcut) shortcut = g_list_model_get_item (shortcuts, i-1);
      GtkShortcutAction *action = gtk_shortcut_get_action (shortcut);

      if (GTK_IS_NAMED_ACTION (action))
        {
          GtkShortcutTrigger *trigger = gtk_shortcut_get_trigger (shortcut);
          g_autofree char *accel = gtk_shortcut_trigger_to_string (trigger);
          const char *name = gtk_named_action_get_action_name (GTK_NAMED_ACTION (action));

          if (!ide_str_equal0 (accel, "never"))
            g_hash_table_insert (accel_map, g_strdup (name), g_steal_pointer (&accel));
        }
    }

  populate_info (&pages, accel_map);

  /* Generate XML for the shortcuts window */
  xml = g_string_new ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
  g_string_append (xml, "<interface>\n");
  g_string_append (xml, "  <object class=\"AdwShortcutsDialog\" id=\"shortcuts_dialog\">\n");
  for (const GList *piter = pages.head; piter; piter = piter->next)
    {
      PageInfo *pi = piter->data;

      if (g_str_equal (pi->title, "ignore"))
        continue;

      g_queue_sort (&pi->groups, (GCompareDataFunc)sort_groups_func, NULL);

      for (const GList *giter = pi->groups.head; giter; giter = giter->next)
        {
          GroupInfo *gi = giter->data;
          g_autofree char *group_title = g_markup_escape_text (gi->title, -1);
          gboolean has_at_least_one = FALSE;

          if (g_str_equal (gi->title, "ignore"))
            continue;

          for (const GList *siter = gi->shortcuts.head; siter; siter = siter->next)
            {
              const ShortcutInfo *si = siter->data;

              if (si->accel != NULL)
                {
                  has_at_least_one = TRUE;
                  break;
                }
            }

          if (!has_at_least_one)
            continue;

          g_string_append        (xml, "    <child>\n");
          g_string_append        (xml, "      <object class=\"AdwShortcutsSection\">\n");
          g_string_append_printf (xml, "        <property name=\"title\">%s</property>\n", group_title);

          for (const GList *siter = gi->shortcuts.head; siter; siter = siter->next)
            {
              ShortcutInfo *si = siter->data;
              g_autofree char *accel = NULL;
              g_autofree char *shortcut_title = NULL;

              if (si->accel == NULL)
                continue;

              accel = g_markup_escape_text (si->accel, -1);
              shortcut_title = g_markup_escape_text (si->title, -1);

              remove_underline_and_ellipsis (shortcut_title);

              g_string_append        (xml, "        <child>\n");
              g_string_append        (xml, "          <object class=\"AdwShortcutsItem\">\n");
              g_string_append_printf (xml, "            <property name=\"accelerator\">%s</property>\n", accel);
              g_string_append_printf (xml, "            <property name=\"title\">%s</property>\n", shortcut_title);
              if (si->subtitle)
                g_string_append_printf (xml, "            <property name=\"subtitle\">%s</property>\n", si->subtitle);
              g_string_append        (xml, "          </object>\n");
              g_string_append        (xml, "        </child>\n");
            }
          g_string_append        (xml, "      </object>\n");
          g_string_append        (xml, "    </child>\n");
        }
    }
  g_string_append        (xml, "  </object>\n");
  g_string_append        (xml, "</interface>\n");

  while (pages.length)
    {
      PageInfo *pi = g_queue_peek_head (&pages);
      g_queue_unlink (&pages, &pi->link);
      page_info_free (pi);
    }

  if (!(builder = gtk_builder_new_from_string (xml->str, -1)))
    IDE_RETURN (NULL);

  if (!(window = GTK_WIDGET (gtk_builder_get_object (builder, "shortcuts_dialog"))))
    IDE_RETURN (NULL);

  g_object_set_data_full (G_OBJECT (window),
                          "GTK_BUILDER",
                          g_steal_pointer (&builder),
                          g_object_unref);

  IDE_RETURN (window);
}

struct _IdeShortcutInfo
{
  const char *id;
  const char *page;
  const char *group;
  const char *title;
  const char *subtitle;
  const char *accel;
  const char *icon_name;
  const char *action_name;
  GVariant *action_target;
};

/**
 * ide_shortcut_info_foreach:
 * @shortcuts: a #GListModel of #GtkShortcut
 * @func: (scope call): a callback for each shortcut info
 * @func_data: closure data for @func
 *
 * Calls @func for every shortcut info. Accelerators come from
 * @shortcuts by matching action and target.
 */
void
ide_shortcut_info_foreach (GListModel          *shortcuts,
                           IdeShortcutInfoFunc  func,
                           gpointer             func_data)
{
  g_autoptr(GHashTable) accel_map = NULL;
  GQueue pages = G_QUEUE_INIT;
  IdeShortcutInfo info = {0};

  g_return_if_fail (func != NULL);
  g_return_if_fail (!shortcuts || G_IS_LIST_MODEL (shortcuts));

  accel_map = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  if (shortcuts != NULL)
    {
      guint n_items = g_list_model_get_n_items (shortcuts);

      /* First build a hashmap of action names to shortcut triggers */
      for (guint i = n_items; i > 0; i--)
        {
          g_autoptr(GtkShortcut) shortcut = g_list_model_get_item (shortcuts, i-1);
          GtkShortcutTrigger *trigger = gtk_shortcut_get_trigger (shortcut);
          GtkShortcutAction *action = gtk_shortcut_get_action (shortcut);
          IdeShortcut *state;

          if (GTK_IS_NAMED_ACTION (action))
            {
              g_autofree char *accel = gtk_shortcut_trigger_to_string (trigger);
              const char *name = gtk_named_action_get_action_name (GTK_NAMED_ACTION (action));

              if (!ide_str_equal0 (accel, "never"))
                g_hash_table_insert (accel_map, g_strdup (name), g_steal_pointer (&accel));
            }
          else if ((state = g_object_get_data (G_OBJECT (shortcut), "IDE_SHORTCUT")) &&
                   GTK_IS_NAMED_ACTION (state->action))
            {
              g_autofree char *accel = gtk_shortcut_trigger_to_string (trigger);
              const char *name = gtk_named_action_get_action_name (GTK_NAMED_ACTION (state->action));

              if (!ide_str_equal0 (accel, "never"))
                g_hash_table_insert (accel_map,
                                     g_strdup (name),
                                     g_steal_pointer (&accel));
            }
        }
    }

  populate_info (&pages, accel_map);

  for (const GList *piter = pages.head; piter; piter = piter->next)
    {
      PageInfo *pi = piter->data;
      g_autofree char *page_title = g_markup_escape_text (pi->title, -1);

      info.page = page_title;

      for (const GList *giter = pi->groups.head; giter; giter = giter->next)
        {
          GroupInfo *gi = giter->data;
          g_autofree char *group_title = g_markup_escape_text (gi->title, -1);

          info.group = group_title;

          for (const GList *siter = gi->shortcuts.head; siter; siter = siter->next)
            {
              ShortcutInfo *si = siter->data;
              g_autofree char *shortcut_title = g_markup_escape_text (si->title, -1);

              remove_underline_and_ellipsis (shortcut_title);

              info.id = si->id;
              info.title = shortcut_title;
              info.subtitle = si->subtitle;
              info.accel = si->accel;
              info.action_name = si->action;
              info.action_target = si->target;
              info.icon_name = si->icon_name;

              func (&info, func_data);
            }
        }
    }

  while (pages.length)
    {
      PageInfo *pi = g_queue_peek_head (&pages);
      g_queue_unlink (&pages, &pi->link);
      page_info_free (pi);
    }
}

const char *
ide_shortcut_info_get_action_name (const IdeShortcutInfo *self)
{
  return self->action_name;
}

/**
 * ide_shortcut_info_get_action_target:
 * @self: a #IdeShortcutInfo
 *
 * Returns: (transfer none) (nullable): a #GVariant or %NULL
 */
GVariant *
ide_shortcut_info_get_action_target (const IdeShortcutInfo *self)
{
  return self->action_target;
}

const char *
ide_shortcut_info_get_page (const IdeShortcutInfo *self)
{
  return self->page;
}

const char *
ide_shortcut_info_get_group (const IdeShortcutInfo *self)
{
  return self->group;
}

const char *
ide_shortcut_info_get_title (const IdeShortcutInfo *self)
{
  return self->title;
}

const char *
ide_shortcut_info_get_subtitle (const IdeShortcutInfo *self)
{
  return self->subtitle;
}

const char *
ide_shortcut_info_get_accelerator (const IdeShortcutInfo *self)
{
  return self->accel;
}

const char *
ide_shortcut_info_get_icon_name (const IdeShortcutInfo *self)
{
  return self->icon_name;
}

const char *
ide_shortcut_info_get_id (const IdeShortcutInfo *self)
{
  return self->id;
}
