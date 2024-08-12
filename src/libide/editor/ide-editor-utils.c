/* ide-editor-utils.c
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-editor-utils"

#include "config.h"

#include <glib/gi18n.h>

#include <string.h>
#include <math.h>

#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>

#include "ide-editor-utils.h"

static int
sort_by_name (gconstpointer a,
              gconstpointer b)
{
  return g_strcmp0 (gtk_source_encoding_get_name (a),
                    gtk_source_encoding_get_name (b));
}

/**
 * ide_editor_encoding_menu_new:
 * @action_name: the action to activate when selecting menu items
 *
 * Creates a new #GMenuModel with items which will activate using
 * their encoding charset for the action @action_name target.
 *
 * Returns: (transfer full): a #GMenuModel
 */
GMenuModel *
ide_editor_encoding_menu_new (const char *action_name)
{
  g_autoptr(GMenu) menu = NULL;
  GHashTable *submenus;
  GMenu *top_menu;
  GSList *all;

  g_return_val_if_fail (action_name, NULL);

  top_menu = g_menu_new ();

  submenus = g_hash_table_new (g_str_hash, g_str_equal);
  all = g_slist_sort (gtk_source_encoding_get_all (), sort_by_name);

  /* Always place UTF8 at the top in it's own section */
  {
    g_autoptr(GMenu) section = g_menu_new ();
    g_autoptr(GMenuItem) item = g_menu_item_new ("UTF-8", NULL);

    g_menu_item_set_action_and_target (item, action_name, "s", "UTF-8");
    g_menu_item_set_attribute (item, "role", "s", "check");
    g_menu_append_item (section, item);
    g_menu_append_section (top_menu, NULL, G_MENU_MODEL (section));
  }

  menu = g_menu_new ();
  g_menu_append_section (top_menu, NULL, G_MENU_MODEL (menu));

  for (const GSList *l = all; l; l = l->next)
    {
      GtkSourceEncoding *encoding = l->data;
      const char *name = gtk_source_encoding_get_name (encoding);
      const char *charset = gtk_source_encoding_get_charset (encoding);
      g_autofree char *title = g_strdup_printf ("%s (%s)", name, charset);
      g_autoptr(GMenuItem) item = g_menu_item_new (title, NULL);
      GMenu *submenu;

      if (name == NULL || charset == NULL)
        continue;

      if (!(submenu = g_hash_table_lookup (submenus, name)))
        {
          submenu = g_menu_new ();
          g_menu_append_submenu (menu, name, G_MENU_MODEL (submenu));
          g_hash_table_insert (submenus, (char *)name, submenu);
          g_object_unref (submenu);
        }

      g_menu_item_set_action_and_target (item, action_name, "s", gtk_source_encoding_get_charset (encoding));
      g_menu_item_set_attribute (item, "role", "s", "check");
      g_menu_append_item (submenu, item);
    }

  g_hash_table_unref (submenus);
  g_slist_free (all);

  return G_MENU_MODEL (top_menu);
}

/**
 * ide_editor_syntax_menu_new:
 * @action_name: the action to activate when selecting menu items
 *
 * Creates a new #GMenuModel with items which will activate using
 * their syntax id for the action @action_name target.
 *
 * Returns: (transfer full): a #GMenuModel
 */
GMenuModel *
ide_editor_syntax_menu_new (const char *action_name)
{
  GtkSourceLanguageManager *manager;
  const char * const *language_ids;
  g_autofree char **sections = NULL;
  GHashTable *submenus;
  g_autoptr(GMenu) top_section = NULL;
  GMenu *top_menu;
  guint len = 0;

  g_return_val_if_fail (action_name, NULL);

  manager = gtk_source_language_manager_get_default ();
  language_ids = gtk_source_language_manager_get_language_ids (manager);
  submenus = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);
  top_menu = g_menu_new ();
  top_section = g_menu_new ();

  g_menu_append_section (top_menu, _("Language"), G_MENU_MODEL (top_section));

  for (guint i = 0; language_ids[i]; i++)
    {
      const char *language_id = language_ids[i];
      GtkSourceLanguage *language = gtk_source_language_manager_get_language (manager, language_id);
      g_autoptr(GMenuItem) item = NULL;
      const char *name;
      const char *section;
      GMenu *submenu;

      if (gtk_source_language_get_hidden (language))
        continue;

      name = gtk_source_language_get_name (language);
      section = gtk_source_language_get_section (language);

      if (!(submenu = g_hash_table_lookup (submenus, section)))
        {
          submenu = g_menu_new ();
          g_hash_table_insert (submenus, (char *)section, submenu);
        }

      item = g_menu_item_new (name, NULL);
      g_menu_item_set_action_and_target (item, action_name, "s", language_id);
      g_menu_append_item (submenu, item);
    }

  sections = (char **)g_hash_table_get_keys_as_array (submenus, &len);
  ide_strv_sort ((char **)sections, len);

  for (guint i = 0; sections[i]; i++)
    {
      GMenu *submenu = g_hash_table_lookup (submenus, sections[i]);
      g_menu_append_submenu (top_section, sections[i], G_MENU_MODEL (submenu));
    }

  g_hash_table_unref (submenus);

  return G_MENU_MODEL (top_menu);
}
