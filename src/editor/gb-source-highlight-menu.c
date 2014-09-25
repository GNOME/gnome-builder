/* gb-source-highlight-menu.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#include <gtksourceview/gtksource.h>

#include "gb-source-highlight-menu.h"

GMenuModel *
gb_source_highlight_menu_new (void)
{
  GtkSourceLanguageManager *manager;
  const gchar * const *lang_ids;
  GHashTable *groups;
  GHashTableIter iter;
  const gchar *key;
  GMenu *value;
  GMenu *top_menu;
  guint i;

  manager = gtk_source_language_manager_get_default ();
  lang_ids = gtk_source_language_manager_get_language_ids (manager);

  groups = g_hash_table_new (g_str_hash, g_str_equal);

  for (i = 0; lang_ids [i]; i++)
    {
      GtkSourceLanguage *lang;
      const gchar *section;
      const gchar *name;
      GMenuItem *item;
      GMenu *menu;
      gchar *detailed;

      lang = gtk_source_language_manager_get_language (manager, lang_ids [i]);

      section = gtk_source_language_get_section (lang);
      name = gtk_source_language_get_name (lang);
      menu = g_hash_table_lookup (groups, section);

      if (!menu)
        {
          menu = g_menu_new ();
          g_hash_table_insert (groups, (gchar *)section, menu);
        }

      detailed = g_strdup_printf ("editor.highlight-mode('%s')", lang_ids [i]);
      item = g_menu_item_new (name, detailed);
      g_menu_append_item (menu, item);
      g_free (detailed);
    }

  g_hash_table_iter_init (&iter, groups);

  top_menu = g_menu_new ();

  while (g_hash_table_iter_next (&iter, (gpointer *)&key, (gpointer *)&value))
    {
      GMenuItem *item;

      item = g_menu_item_new (key, NULL);
      g_menu_item_set_submenu (item, G_MENU_MODEL (value));
      g_menu_append_item (top_menu, item);
    }

  g_hash_table_unref (groups);

  return G_MENU_MODEL (top_menu);
}
