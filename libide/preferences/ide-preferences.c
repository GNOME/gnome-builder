/* ide-preferences.c
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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

#include "ide-preferences.h"

G_DEFINE_INTERFACE (IdePreferences, ide_preferences, G_TYPE_OBJECT)

static void
ide_preferences_default_init (IdePreferencesInterface *iface)
{
}

void
ide_preferences_add_page (IdePreferences *self,
                          const gchar    *page_name,
                          const gchar    *title,
                          gint            priority)
{
  g_return_if_fail (IDE_IS_PREFERENCES (self));
  g_return_if_fail (page_name != NULL);
  g_return_if_fail (title != NULL);

  IDE_PREFERENCES_GET_IFACE (self)->add_page (self, page_name, title, priority);
}

void
ide_preferences_add_group (IdePreferences *self,
                           const gchar    *page_name,
                           const gchar    *group_name,
                           const gchar    *title,
                           gint            priority)
{
  g_return_if_fail (IDE_IS_PREFERENCES (self));
  g_return_if_fail (page_name != NULL);
  g_return_if_fail (group_name != NULL);

  IDE_PREFERENCES_GET_IFACE (self)->add_group (self, page_name, group_name, title, priority);
}

guint
ide_preferences_add_switch (IdePreferences *self,
                            const gchar    *page_name,
                            const gchar    *group_name,
                            const gchar    *schema_id,
                            const gchar    *key,
                            const gchar    *path,
                            const gchar    *variant_string,
                            const gchar    *title,
                            const gchar    *subtitle,
                            const gchar    *keywords,
                            gint            priority)
{
  g_return_val_if_fail (IDE_IS_PREFERENCES (self), 0);
  g_return_val_if_fail (page_name != NULL, 0);
  g_return_val_if_fail (group_name != NULL, 0);
  g_return_val_if_fail (schema_id != NULL, 0);
  g_return_val_if_fail (key != NULL, 0);
  g_return_val_if_fail (title != NULL, 0);

  return IDE_PREFERENCES_GET_IFACE (self)->add_switch (self, page_name, group_name, schema_id, key, path, variant_string, title, subtitle, keywords, priority);
}

guint
ide_preferences_add_spin_button (IdePreferences *self,
                                 const gchar    *page_name,
                                 const gchar    *group_name,
                                 const gchar    *schema_id,
                                 const gchar    *key,
                                 const gchar    *title,
                                 const gchar    *subtitle,
                                 const gchar    *keywords,
                                 gint            priority)
{
  g_return_val_if_fail (IDE_IS_PREFERENCES (self), 0);
  g_return_val_if_fail (page_name != NULL, 0);
  g_return_val_if_fail (group_name != NULL, 0);
  g_return_val_if_fail (schema_id != NULL, 0);
  g_return_val_if_fail (key != NULL, 0);
  g_return_val_if_fail (title != NULL, 0);

  return IDE_PREFERENCES_GET_IFACE (self)->add_spin_button (self, page_name, group_name, schema_id, key, title, subtitle, keywords, priority);
}

guint
ide_preferences_add_custom (IdePreferences *self,
                            const gchar    *page_name,
                            const gchar    *group_name,
                            GtkWidget      *widget,
                            const gchar    *keywords,
                            gint            priority)
{
  g_return_val_if_fail (IDE_IS_PREFERENCES (self), 0);
  g_return_val_if_fail (page_name != NULL, 0);
  g_return_val_if_fail (group_name != NULL, 0);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  return IDE_PREFERENCES_GET_IFACE (self)->add_custom (self, page_name, group_name, widget, keywords, priority);
}

void
ide_preferences_add_list_group  (IdePreferences *self,
                                 const gchar    *page_name,
                                 const gchar    *group_name,
                                 const gchar    *title,
                                 gint            priority)
{
  g_return_if_fail (IDE_IS_PREFERENCES (self));
  g_return_if_fail (page_name != NULL);
  g_return_if_fail (group_name != NULL);

  return IDE_PREFERENCES_GET_IFACE (self)->add_list_group  (self, page_name, group_name, title, priority);
}

guint ide_preferences_add_radio (IdePreferences *self,
                                 const gchar    *page_name,
                                 const gchar    *group_name,
                                 const gchar    *schema_id,
                                 const gchar    *key,
                                 const gchar    *path,
                                 const gchar    *variant_string,
                                 const gchar    *title,
                                 const gchar    *subtitle,
                                 const gchar    *keywords,
                                 gint            priority)
{
  g_return_val_if_fail (IDE_IS_PREFERENCES (self), 0);
  g_return_val_if_fail (page_name != NULL, 0);
  g_return_val_if_fail (group_name != NULL, 0);
  g_return_val_if_fail (schema_id != NULL, 0);
  g_return_val_if_fail (key != NULL, 0);
  g_return_val_if_fail (title != NULL, 0);

  return IDE_PREFERENCES_GET_IFACE (self)->add_radio (self, page_name, group_name, schema_id, key, path, variant_string, title, subtitle, keywords, priority);
}

guint
ide_preferences_add_font_button (IdePreferences *self,
                                 const gchar    *page_name,
                                 const gchar    *group_name,
                                 const gchar    *schema_id,
                                 const gchar    *key,
                                 const gchar    *title,
                                 const gchar    *keywords,
                                 gint            priority)
{
  g_return_val_if_fail (IDE_IS_PREFERENCES (self), 0);
  g_return_val_if_fail (page_name != NULL, 0);
  g_return_val_if_fail (group_name != NULL, 0);
  g_return_val_if_fail (schema_id != NULL, 0);
  g_return_val_if_fail (key != NULL, 0);
  g_return_val_if_fail (title != NULL, 0);

  return IDE_PREFERENCES_GET_IFACE (self)->add_font_button (self, page_name, group_name, schema_id, key, title, keywords, priority);
}
