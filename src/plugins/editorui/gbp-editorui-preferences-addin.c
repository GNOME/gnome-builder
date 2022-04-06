/* gbp-editorui-preferences-addin.c
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

#define G_LOG_DOMAIN "gbp-editorui-preferences-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-sourceview.h>

#include "gbp-editorui-preferences-addin.h"
#include "gbp-editorui-preview.h"

struct _GbpEditoruiPreferencesAddin
{
  GObject parent_instance;
};

static const IdePreferenceGroupEntry groups[] = {
  { "appearance", "preview",     10, N_("Style") },
  { "appearance", "schemes",     20, NULL },
  { "appearance", "font",        30, NULL },
  { "appearance", "effects",     40, NULL },
  { "appearance", "accessories", 50, NULL },

  { "languages/*", "general",      0, N_("General") },
  { "languages/*", "margins",     10, N_("Margins") },
  { "languages/*", "spacing",     20, N_("Spacing") },
  { "languages/*", "indentation", 30, N_("Indentation") },

  { "completion", "general",       0, N_("General") },

  { "keyboard",   "movement",      10, N_("Movements") },
};

static const IdePreferenceItemEntry items[] = {
  { "appearance", "effects", "show-grid-lines", 10, ide_preferences_window_toggle,
    N_("Show Grid Pattern"),
    N_("Display a grid pattern underneath source code"),
    "org.gnome.builder.editor", NULL, "show-grid-lines" },

  { "appearance", "effects", "show-map", 10, ide_preferences_window_toggle,
    N_("Show Overview Map"),
    N_("Use an overview map instead of a scrollbar"),
    "org.gnome.builder.editor", NULL, "show-map" },

  { "appearance", "accessories", "show-line-numbers", 0, ide_preferences_window_toggle,
    N_("Show Line Numbers"),
    N_("Display line numbers next to each line of source code"),
    "org.gnome.builder.editor", NULL, "show-line-numbers" },

  { "appearance", "accessories", "highlight-current-line", 20, ide_preferences_window_toggle,
    N_("Highlight Current Line"),
    N_("Make current line stand out with highlights"),
    "org.gnome.builder.editor", NULL, "highlight-current-line" },

  { "appearance", "accessories", "highlight-matching-brackets", 30, ide_preferences_window_toggle,
    N_("Highlight Matching Brackets"),
    N_("Use cursor position to highlight matching brackets, braces, parenthesis, and more"),
    "org.gnome.builder.editor", NULL, "highlight-matching-brackets" },

  { "completion", "general", "interactive", 10, ide_preferences_window_toggle,
    N_("Suggest Completions While Typing"),
    N_("Automatically suggest completions while typing within the file"),
    "org.gnome.builder.editor", NULL, "interactive-completion" },

  { "keyboard", "movement", "smart-home-end", 0, ide_preferences_window_toggle,
    N_("Smart Home and End"),
    N_("Home moves to first non-whitespace character"),
    "org.gnome.builder.editor", NULL, "smart-home-end" },

  { "keyboard", "movement", "smart-backspace", 0, ide_preferences_window_toggle,
    N_("Smart Backspace"),
    N_("Backspace will remove extra space to keep you aligned with your indentation"),
    "org.gnome.builder.editor", NULL, "smart-backspace" },
};

static const IdePreferenceItemEntry lang_items[] = {
  { "languages/*", "general", "trim", 0, ide_preferences_window_toggle,
    N_("Trim Trailing Whitespace"),
    N_("Upon saving, trailing whitepsace from modified lines will be trimmed"),
    "org.gnome.builder.editor.language", "/*", "trim-trailing-whitespace" },

  { "languages/*", "general", "overwrite", 0, ide_preferences_window_toggle,
    N_("Overwrite Braces"),
    N_("Overwrite closing braces"),
    "org.gnome.builder.editor.language", "/*", "overwrite-braces" },

  { "languages/*", "general", "insert-matching", 0, ide_preferences_window_toggle,
    N_("Insert Matching Brace"),
    N_("Insert matching character for [[(\"'"),
    "org.gnome.builder.editor.language", "/*", "insert-matching-brace" },

  { "languages/*", "general", "insert-trailing", 0, ide_preferences_window_toggle,
    N_("Insert Trailing Newline"),
    N_("Ensure files end with a newline"),
    "org.gnome.builder.editor.language", "/*", "insert-trailing-newline" },

  { "languages/*", "margins", "show-right-margin", 0, ide_preferences_window_toggle,
    N_("Show right margin"),
    N_("Display a margin in the editor to indicate maximum desired width"),
    "org.gnome.builder.editor.language", "/*", "show-right-margin" },

#if 0
  { "languages/*", "spacing", "before-parens", 0, ide_preferences_window_toggle, "Prefer a space before opening parentheses" },
  { "languages/*", "spacing", "before-brackets", 0, ide_preferences_window_toggle, "Prefer a space before opening brackets" },
  { "languages/*", "spacing", "before-braces", 0, ide_preferences_window_toggle, "Prefer a space before opening braces" },
  { "languages/*", "spacing", "before-angles", 0, ide_preferences_window_toggle, "Prefer a space before opening angles" },
#endif

  { "languages/*", "indentation", "insert-spaces", 0, ide_preferences_window_toggle,
    N_("Insert spaces instead of tabs"),
    N_("Prefer spaces over tabs"),
    "org.gnome.builder.editor.language", "/*", "insert-spaces-instead-of-tabs" },

  { "languages/*", "indentation", "auto-indent", 0, ide_preferences_window_toggle,
    N_("Automatically Indent"),
    N_("Format source code as you type"),
    "org.gnome.builder.editor.language", "/*", "auto-indent" },
};


static int
compare_section (gconstpointer a,
                 gconstpointer b)
{
  const IdePreferencePageEntry *pagea = a;
  const IdePreferencePageEntry *pageb = b;

  return g_strcmp0 (pagea->section, pageb->section);
}

static void
notify_style_scheme_cb (IdeApplication *app,
                        GParamSpec     *pspec,
                        GtkFlowBox     *flowbox)
{
  const char *style_scheme;
  gboolean dark;

  g_assert (IDE_IS_APPLICATION (app));
  g_assert (GTK_IS_FLOW_BOX (flowbox));

  style_scheme = ide_application_get_style_scheme (app);
  dark = ide_application_get_dark (app);

  for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (flowbox));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      GtkSourceStyleSchemePreview *preview = GTK_SOURCE_STYLE_SCHEME_PREVIEW (gtk_flow_box_child_get_child (GTK_FLOW_BOX_CHILD (child)));
      GtkSourceStyleScheme *scheme = gtk_source_style_scheme_preview_get_scheme (preview);
      gboolean visible = dark == ide_source_style_scheme_is_dark (scheme);
      gboolean selected = g_strcmp0 (style_scheme, gtk_source_style_scheme_get_id (scheme)) == 0;

      gtk_source_style_scheme_preview_set_selected (preview, selected);
      gtk_widget_set_visible (child, visible);
    }
}

static void
ide_preferences_builtin_add_schemes (const char                   *page_name,
                                     const IdePreferenceItemEntry *entry,
                                     AdwPreferencesGroup          *group,
                                     gpointer                      user_data)
{
  IdePreferencesWindow *window = user_data;
  GtkSourceStyleSchemeManager *manager;
  const char * const *scheme_ids;
  GtkFlowBox *flowbox;
  GtkWidget *preview;
  GtkFrame *frame;

  g_assert (IDE_IS_PREFERENCES_WINDOW (window));
  g_assert (entry != NULL);
  g_assert (ADW_IS_PREFERENCES_GROUP (group));

  preview = gbp_editorui_preview_new ();
  frame = g_object_new (GTK_TYPE_FRAME,
                        "child", preview,
                        "margin-bottom", 12,
                        NULL);
  adw_preferences_group_add (group, GTK_WIDGET (frame));

  manager = gtk_source_style_scheme_manager_get_default ();
  scheme_ids = gtk_source_style_scheme_manager_get_scheme_ids (manager);

  flowbox = g_object_new (GTK_TYPE_FLOW_BOX,
                          "activate-on-single-click", TRUE,
                          "column-spacing", 6,
                          "row-spacing", 6,
                          "max-children-per-line", 4,
                          NULL);
  gtk_widget_add_css_class (GTK_WIDGET (flowbox), "style-schemes");

  for (guint i = 0; scheme_ids[i]; i++)
    {
      GtkSourceStyleScheme *scheme = gtk_source_style_scheme_manager_get_scheme (manager, scheme_ids[i]);
      GtkWidget *selector = gtk_source_style_scheme_preview_new (scheme);

      gtk_actionable_set_action_name (GTK_ACTIONABLE (selector), "app.style-scheme-name");
      gtk_actionable_set_action_target (GTK_ACTIONABLE (selector), "s", scheme_ids[i]);

      gtk_flow_box_append (flowbox, selector);
    }

  g_signal_connect_object (IDE_APPLICATION_DEFAULT,
                           "notify::style-scheme",
                           G_CALLBACK (notify_style_scheme_cb),
                           flowbox,
                           0);
  notify_style_scheme_cb (IDE_APPLICATION_DEFAULT, NULL, flowbox);

  adw_preferences_group_add (group, GTK_WIDGET (flowbox));
}

static void
gbp_editorui_preferences_addin_add_languages (IdePreferencesWindow *window)
{
  GtkSourceLanguageManager *langs;
  const char * const *lang_ids;
  IdePreferencePageEntry *lpages;
  guint j = 0;

  g_assert (IDE_IS_PREFERENCES_WINDOW (window));

  langs = gtk_source_language_manager_get_default ();
  lang_ids = gtk_source_language_manager_get_language_ids (langs);
  lpages = g_new0 (IdePreferencePageEntry, g_strv_length ((char **)lang_ids));

  for (guint i = 0; lang_ids[i]; i++)
    {
      GtkSourceLanguage *l = gtk_source_language_manager_get_language (langs, lang_ids[i]);
      IdePreferencePageEntry *page;
      char name[256];

      if (gtk_source_language_get_hidden (l))
        continue;

      page = &lpages[j++];

      g_snprintf (name, sizeof name, "languages/%s", lang_ids[i]);

      page->parent = "languages";
      page->section = gtk_source_language_get_section (l);
      page->name = g_intern_string (name);
      page->icon_name = NULL;
      page->title = gtk_source_language_get_name (l);
    }

  qsort (lpages, j, sizeof *lpages, compare_section);
  for (guint i = 0; i < j; i++)
    lpages[i].priority = i;

  ide_preferences_window_add_pages (window, lpages, j, NULL);
  ide_preferences_window_add_items (window, lang_items, G_N_ELEMENTS (lang_items), window, NULL);

  g_free (lpages);
}


static void
gbp_editorui_preferences_addin_load (IdePreferencesAddin  *addin,
                                     IdePreferencesWindow *window)
{
  GbpEditoruiPreferencesAddin *self = (GbpEditoruiPreferencesAddin *)addin;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_EDITORUI_PREFERENCES_ADDIN (self));
  g_assert (IDE_IS_PREFERENCES_WINDOW (window));

  ide_preferences_window_add_groups (window, groups, G_N_ELEMENTS (groups), NULL);
  ide_preferences_window_add_items (window, items, G_N_ELEMENTS (items), window, NULL);
  ide_preferences_window_add_item (window, "appearance", "preview", "scheme", 0,
                                   ide_preferences_builtin_add_schemes, window, NULL);
  gbp_editorui_preferences_addin_add_languages (window);

  IDE_EXIT;
}

static void
preferences_addin_init (IdePreferencesAddinInterface *iface)
{
  iface->load = gbp_editorui_preferences_addin_load;
}

G_DEFINE_TYPE_WITH_CODE (GbpEditoruiPreferencesAddin, gbp_editorui_preferences_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_PREFERENCES_ADDIN, preferences_addin_init))

static void
gbp_editorui_preferences_addin_class_init (GbpEditoruiPreferencesAddinClass *klass)
{
}

static void
gbp_editorui_preferences_addin_init (GbpEditoruiPreferencesAddin *self)
{
}
