/* ide-preferences-builtin.c
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

#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>
#include <libpeas/peas.h>

#include "ide-preferences-builtin.h"

static void
ide_preferences_builtin_register_plugins (IdePreferences *preferences)
{
  PeasEngine *engine;
  const GList *list;
  guint i = 0;

  g_assert (IDE_IS_PREFERENCES (preferences));

  engine = peas_engine_get_default ();
  list = peas_engine_get_plugin_list (engine);

  ide_preferences_add_page (preferences, "plugins", _("Plugins"), 700);
  ide_preferences_add_list_group (preferences, "plugins", "plugins", _("Installed Plugins"), 0);

  for (; list; list = list->next, i++)
    {
      PeasPluginInfo *plugin_info = list->data;
      const gchar *desc;
      const gchar *name;

      name = peas_plugin_info_get_name (plugin_info);
      desc = peas_plugin_info_get_description (plugin_info);

      /* TODO: support custom path (and use right schema/key) */
      ide_preferences_add_switch (preferences, "plugins", "plugins", "org.gnome.builder.editor", "show-line-numbers", NULL, name, desc, NULL, i);
    }
}

static void
ide_preferences_builtin_register_appearance (IdePreferences *preferences)
{
  GtkSourceStyleSchemeManager *manager;
  const gchar * const *scheme_ids;
  gint i;

  ide_preferences_add_page (preferences, "appearance", _("Appearance"), 0);

  ide_preferences_add_group (preferences, "appearance", "basic", NULL, 0);
  ide_preferences_add_switch (preferences, "appearance", "basic", "org.gnome.builder", "night-mode", NULL, _("Dark Theme"), _("Whether Builder should use a dark theme"), _("dark theme"), 0);
  ide_preferences_add_switch (preferences, "appearance", "basic", "org.gnome.builder", "animations", NULL, _("Animations"), _("Whether animations should be used when appropriate"), _("animations"), 100);

  ide_preferences_add_list_group (preferences, "appearance", "font", _("Font"), 100);
  ide_preferences_add_font_button (preferences, "appearance", "font", "org.gnome.builder.editor", "font-name", _("Editor"), _("editor font monospace"), 0);

  ide_preferences_add_list_group (preferences, "appearance", "schemes", _("Themes"), 200);

  manager = gtk_source_style_scheme_manager_get_default ();
  scheme_ids = gtk_source_style_scheme_manager_get_scheme_ids (manager);

  for (i = 0; scheme_ids [i]; i++)
    {
      g_autofree gchar *variant_str = NULL;
      GtkSourceStyleScheme *scheme;
      const gchar *title;

      variant_str = g_strdup_printf ("\"%s\"", scheme_ids [i]);
      scheme = gtk_source_style_scheme_manager_get_scheme (manager, scheme_ids [i]);
      title = gtk_source_style_scheme_get_name (scheme);

      ide_preferences_add_radio (preferences, "appearance", "schemes", "org.gnome.builder.editor", "style-scheme-name", variant_str, title, NULL, title, i);
    }

  ide_preferences_add_list_group (preferences, "appearance", "draw-spaces", _("Whitespace Characters"), 300);
  ide_preferences_add_switch (preferences, "appearance", "draw-spaces", "org.gnome.builder.editor", "draw-spaces", "\"space\"", _("Spaces"), NULL, NULL, 0);
  ide_preferences_add_switch (preferences, "appearance", "draw-spaces", "org.gnome.builder.editor", "draw-spaces", "\"tab\"", _("Tabs"), NULL, NULL, 1);
  ide_preferences_add_switch (preferences, "appearance", "draw-spaces", "org.gnome.builder.editor", "draw-spaces", "\"newline\"", _("New line and carriage return"), NULL, NULL, 2);
  ide_preferences_add_switch (preferences, "appearance", "draw-spaces", "org.gnome.builder.editor", "draw-spaces", "\"nbsp\"", _("Non-breaking spaces"), NULL, NULL, 3);
  ide_preferences_add_switch (preferences, "appearance", "draw-spaces", "org.gnome.builder.editor", "draw-spaces", "\"text\"", _("Spaces inside of text"), NULL, NULL, 4);
  ide_preferences_add_switch (preferences, "appearance", "draw-spaces", "org.gnome.builder.editor", "draw-spaces", "\"trailing\"", _("Trailing Only"), NULL, NULL, 5);
  ide_preferences_add_switch (preferences, "appearance", "draw-spaces", "org.gnome.builder.editor", "draw-spaces", "\"leading\"", _("Leading Only"), NULL, NULL, 6);
}

void
_ide_preferences_builtin_register (IdePreferences *preferences)
{
  ide_preferences_builtin_register_appearance (preferences);

  ide_preferences_add_page (preferences, "editor", _("Editor"), 100);
  ide_preferences_add_page (preferences, "languages", _("Programming Languages"), 200);
  ide_preferences_add_page (preferences, "code-insight", _("Code Insight"), 300);
  ide_preferences_add_page (preferences, "keyboard", _("Keyboard"), 400);
  ide_preferences_add_page (preferences, "vcs", _("Version Control"), 600);

  ide_preferences_builtin_register_plugins (preferences);
}
