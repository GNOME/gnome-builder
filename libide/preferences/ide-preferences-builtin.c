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
#include <libgit2-glib/ggit.h>
#include <libpeas/peas.h>

#include "ide-preferences-builtin.h"
#include "ide-preferences-entry.h"

static void
ide_preferences_builtin_register_plugins (IdePreferences *preferences)
{
  PeasEngine *engine;
  const GList *list;
  guint i = 0;

  g_assert (IDE_IS_PREFERENCES (preferences));

  engine = peas_engine_get_default ();
  list = peas_engine_get_plugin_list (engine);

  ide_preferences_add_page (preferences, "plugins", _("Extensions"), 700);
  ide_preferences_add_list_group (preferences, "plugins", "builtin", _("Bundled Extensions"), 0);

  for (; list; list = list->next, i++)
    {
      g_autofree gchar *path = NULL;
      PeasPluginInfo *plugin_info = list->data;
      const gchar *desc;
      const gchar *name;

      if (!peas_plugin_info_is_builtin (plugin_info))
        continue;

      name = peas_plugin_info_get_name (plugin_info);
      desc = peas_plugin_info_get_description (plugin_info);

      path = g_strdup_printf ("/org/gnome/builder/extension-types/%s/",
                              peas_plugin_info_get_module_name (plugin_info));

      ide_preferences_add_switch (preferences, "plugins", "builtin", "org.gnome.builder.extension", "enabled", path, NULL, name, desc, NULL, i);
    }
}

static void
ide_preferences_builtin_register_appearance (IdePreferences *preferences)
{
  GtkSourceStyleSchemeManager *manager;
  const gchar * const *scheme_ids;
  gint i;

  ide_preferences_add_page (preferences, "appearance", _("Appearance"), 0);

  ide_preferences_add_list_group (preferences, "appearance", "basic", NULL, 0);
  ide_preferences_add_switch (preferences, "appearance", "basic", "org.gnome.builder", "night-mode", NULL, NULL, _("Dark Theme"), _("Whether Builder should use a dark theme"), _("dark theme"), 0);

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

      ide_preferences_add_radio (preferences, "appearance", "schemes", "org.gnome.builder.editor", "style-scheme-name", NULL, variant_str, title, NULL, title, i);
    }

  ide_preferences_add_list_group (preferences, "appearance", "background", NULL, 300);
  ide_preferences_add_switch (preferences, "appearance", "background", "org.gnome.builder.editor", "show-grid-lines", NULL, NULL, _("Grid Pattern"), _("Display a grid pattern underneath source code"), NULL, 0);
}

static void
ide_preferences_builtin_register_keyboard (IdePreferences *preferences)
{
  ide_preferences_add_page (preferences, "keyboard", _("Keyboard"), 400);

  ide_preferences_add_list_group (preferences, "keyboard", "mode", _("Emulation"), 0);
  ide_preferences_add_radio (preferences, "keyboard", "mode", "org.gnome.builder.editor", "keybindings", NULL, "\"default\"", _("Builder"), _("Default keybinding mode which mimics Gedit"), NULL, 0);
  ide_preferences_add_radio (preferences, "keyboard", "mode", "org.gnome.builder.editor", "keybindings", NULL, "\"emacs\"", _("Emacs"), _("Emulates the Emacs text editor"), NULL, 0);
  ide_preferences_add_radio (preferences, "keyboard", "mode", "org.gnome.builder.editor", "keybindings", NULL, "\"vim\"", _("Vim"), _("Emulates the Vim text editor"), NULL, 0);

  ide_preferences_add_list_group (preferences, "keyboard", "movements", _("Movement"), 100);
  ide_preferences_add_switch (preferences, "keyboard", "movements", "org.gnome.builder.editor", "smart-home-end", NULL, NULL, _("Smart Home and End"), _("Home moves to first non-whitespace character"), NULL, 0);
  ide_preferences_add_switch (preferences, "keyboard", "movements", "org.gnome.builder.editor", "smart-backspace", NULL, NULL, _("Smart Backspace"), _("Backspace will remove extra space to keep you aligned with your indentation"), NULL, 100);
}

static void
ide_preferences_builtin_register_editor (IdePreferences *preferences)
{
  ide_preferences_add_page (preferences, "editor", _("Editor"), 100);

  ide_preferences_add_list_group (preferences, "editor", "position", NULL, 0);
  ide_preferences_add_switch (preferences, "editor", "position", "org.gnome.builder.editor", "restore-insert-mark", NULL, NULL, _("Restore cursor position"), _("Restore cursor position when a file is reopened"), NULL, 0);
  ide_preferences_add_spin_button (preferences, "editor", "position", "org.gnome.builder.editor", "scroll-offset", NULL, _("Scroll Offset"), _("Minimum number of lines to keep above and below the cursor"), NULL, 10);

  ide_preferences_add_list_group (preferences, "editor", "line", _("Line Information"), 50);
  ide_preferences_add_switch (preferences, "editor", "line", "org.gnome.builder.editor", "show-line-numbers", NULL, NULL, _("Line numbers"), _("Show line number at beginning of each line"), NULL, 0);
  ide_preferences_add_switch (preferences, "editor", "line", "org.gnome.builder.editor", "show-line-changes", NULL, NULL, _("Line changes"), _("Show if a line was added or modified next to line number"), NULL, 1);

  ide_preferences_add_list_group (preferences, "editor", "highlight", _("Highlight"), 100);
  ide_preferences_add_switch (preferences, "editor", "highlight", "org.gnome.builder.editor", "highlight-current-line", NULL, NULL, _("Current line"), _("Make current line stand out with highlights"), NULL, 0);
  ide_preferences_add_switch (preferences, "editor", "highlight", "org.gnome.builder.editor", "highlight-matching-brackets", NULL, NULL, _("Matching brackets"), _("Highlight matching brackets based on cursor position"), NULL, 1);

  ide_preferences_add_list_group (preferences, "editor", "overview", _("Code Overview"), 100);
  ide_preferences_add_switch (preferences, "editor", "overview", "org.gnome.builder.editor", "show-map", NULL, NULL, _("Show overview map"), _("A zoomed out view to enhance navigating source code"), NULL, 0);
  ide_preferences_add_switch (preferences, "editor", "overview", "org.gnome.builder.editor", "auto-hide-map", NULL, NULL, _("Automatically hide overview map"), _("Automatically hide map when editor loses focus"), NULL, 1);

  ide_preferences_add_list_group (preferences, "editor", "draw-spaces", _("Whitespace Characters"), 400);
  ide_preferences_add_radio (preferences, "editor", "draw-spaces", "org.gnome.builder.editor", "draw-spaces", NULL, "\"space\"", _("Spaces"), NULL, NULL, 0);
  ide_preferences_add_radio (preferences, "editor", "draw-spaces", "org.gnome.builder.editor", "draw-spaces", NULL, "\"tab\"", _("Tabs"), NULL, NULL, 1);
  ide_preferences_add_radio (preferences, "editor", "draw-spaces", "org.gnome.builder.editor", "draw-spaces", NULL, "\"newline\"", _("New line and carriage return"), NULL, NULL, 2);
  ide_preferences_add_radio (preferences, "editor", "draw-spaces", "org.gnome.builder.editor", "draw-spaces", NULL, "\"nbsp\"", _("Non-breaking spaces"), NULL, NULL, 3);
  ide_preferences_add_radio (preferences, "editor", "draw-spaces", "org.gnome.builder.editor", "draw-spaces", NULL, "\"text\"", _("Spaces inside of text"), NULL, NULL, 4);
  ide_preferences_add_radio (preferences, "editor", "draw-spaces", "org.gnome.builder.editor", "draw-spaces", NULL, "\"trailing\"", _("Trailing Only"), NULL, NULL, 5);
  ide_preferences_add_radio (preferences, "editor", "draw-spaces", "org.gnome.builder.editor", "draw-spaces", NULL, "\"leading\"", _("Leading Only"), NULL, NULL, 6);
}

static void
ide_preferences_builtin_register_code_insight (IdePreferences *preferences)
{
  ide_preferences_add_page (preferences, "code-insight", _("Code Insight"), 300);

  ide_preferences_add_list_group (preferences, "code-insight", "semantic", NULL, 0);
  ide_preferences_add_switch (preferences, "code-insight", "semantic", "org.gnome.builder.code-insight", "semantic-highlighting", NULL, NULL, _("Semantic Highlighting"), _("Use code insignt to highlight additional information discovered in source file"), NULL, 0);

  ide_preferences_add_list_group (preferences, "code-insight", "completion", _("Completion"), 100);
  ide_preferences_add_switch (preferences, "code-insight", "completion", "org.gnome.builder.code-insight", "word-completion", NULL, NULL, _("Suggest words found in open files"), _("Suggests completions as you type based on words found in any open document"), NULL, 0);
  ide_preferences_add_switch (preferences, "code-insight", "completion", "org.gnome.builder.code-insight", "ctags-autocompletion", NULL, NULL, _("Suggest completions using Ctags"), _("Create and manages a Ctags database for completing class names, functions, and more"), NULL, 10);
  ide_preferences_add_switch (preferences, "code-insight", "completion", "org.gnome.builder.code-insight", "clang-autocompletion", NULL, NULL, _("Suggest completions using Clang (Experimental)"), _("Use Clang to suggest completions for C and C++ languages"), NULL, 20);
}

static void
ide_preferences_builtin_register_snippets (IdePreferences *preferences)
{
  ide_preferences_add_page (preferences, "snippets", _("Snippets"), 350);

  ide_preferences_add_list_group (preferences, "snippets", "completion", NULL, 0);
  ide_preferences_add_switch (preferences, "snippets", "completion", "org.gnome.builder.code-insight", "snippet-completion", NULL, NULL, _("Code snippets"), _("Use code fragments to increase typing efficiency"), NULL, 0);
}

static gchar *
read_config_string (GgitConfig   *orig_config,
                    const gchar  *key,
                    GError      **error)
{
  GgitConfig *config;
  const gchar *value;
  gchar *ret;

  g_assert (GGIT_IS_CONFIG (orig_config));
  g_assert (key != NULL);

  config = ggit_config_snapshot (orig_config, error);
  if (config == NULL)
    return NULL;

  value = ggit_config_get_string (config, key, error);

  ret = value ? g_strdup (value) : NULL;

  g_clear_object (&config);

  return ret;
}

static void
author_changed_cb (IdePreferencesEntry *entry,
                   const gchar         *text,
                   GgitConfig          *config)
{
  g_assert (IDE_IS_PREFERENCES_ENTRY (entry));
  g_assert (text != NULL);
  g_assert (GGIT_IS_CONFIG (config));

  ggit_config_set_string (config, "user.name", text, NULL);
}

static void
email_changed_cb (IdePreferencesEntry *entry,
                  const gchar         *text,
                  GgitConfig          *config)
{
  g_assert (IDE_IS_PREFERENCES_ENTRY (entry));
  g_assert (text != NULL);
  g_assert (GGIT_IS_CONFIG (config));

  ggit_config_set_string (config, "user.email", text, NULL);
}

static void
ide_preferences_builtin_register_vcs (IdePreferences *preferences)
{
  g_autofree gchar *author_text = NULL;
  g_autofree gchar *email_text = NULL;
  g_autoptr(GFile) global_file = NULL;
  GgitConfig *config;
  GtkWidget *author;
  GtkWidget *email;

  ide_preferences_add_page (preferences, "vcs", _("Version Control"), 600);

  if (!(global_file = ggit_config_find_global ()))
    {
      g_autofree gchar *path = NULL;

      path = g_build_filename (g_get_home_dir (), ".gitconfig", NULL);
      global_file = g_file_new_for_path (path);
    }

  config = ggit_config_new_from_file (global_file, NULL);
  g_object_set_data_full (G_OBJECT (preferences), "GGIT_CONFIG", config, g_object_unref);

  author_text = read_config_string (config, "user.name", NULL);
  author = g_object_new (IDE_TYPE_PREFERENCES_ENTRY,
                         "text", author_text,
                         "title", "Author",
                         "visible", TRUE,
                         NULL);
  g_signal_connect_object (author,
                           "changed",
                           G_CALLBACK (author_changed_cb),
                           config,
                           0);

  email_text = read_config_string (config, "user.email", NULL);
  email = g_object_new (IDE_TYPE_PREFERENCES_ENTRY,
                         "text", email_text,
                        "title", "Email",
                        "visible", TRUE,
                        NULL);
  g_signal_connect_object (email,
                           "changed",
                           G_CALLBACK (email_changed_cb),
                           config,
                           0);

  ide_preferences_add_list_group (preferences, "vcs", "attribution", _("Attribution"), 0);
  ide_preferences_add_custom (preferences, "vcs", "attribution", author, NULL, 0);
  ide_preferences_add_custom (preferences, "vcs", "attribution", email, NULL, 0);
}

static void
ide_preferences_builtin_register_languages (IdePreferences *preferences)
{
  GtkSourceLanguageManager *manager;
  const gchar * const *language_ids;
  gint i;

  ide_preferences_add_page (preferences, "languages", _("Programming Languages"), 200);

  manager = gtk_source_language_manager_get_default ();
  language_ids = gtk_source_language_manager_get_language_ids (manager);

  ide_preferences_add_list_group (preferences, "languages", "list", NULL, 0);

  for (i = 0; language_ids [i]; i++)
    {
      GtkSourceLanguage *language;
      const gchar *name;

      language = gtk_source_language_manager_get_language (manager, language_ids [i]);
      name = gtk_source_language_get_name (language);

      ide_preferences_add_custom (preferences, "languages", "list",
                                  g_object_new (GTK_TYPE_LABEL,
                                                "xalign", 0.0f,
                                                "visible", TRUE,
                                                "label", name,
                                                NULL),
                                  NULL, 0);
    }
}

void
_ide_preferences_builtin_register (IdePreferences *preferences)
{
  ide_preferences_builtin_register_appearance (preferences);
  ide_preferences_builtin_register_editor (preferences);
  ide_preferences_builtin_register_languages (preferences);
  ide_preferences_builtin_register_code_insight (preferences);
  ide_preferences_builtin_register_snippets (preferences);
  ide_preferences_builtin_register_keyboard (preferences);
  ide_preferences_builtin_register_vcs (preferences);
  ide_preferences_builtin_register_plugins (preferences);
}
