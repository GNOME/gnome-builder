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

#define LANG_PATH "/org/gnome/builder/editor/language/*"

struct _GbpEditoruiPreferencesAddin
{
  GObject parent_instance;
};

static const IdePreferenceGroupEntry groups[] = {
  { "appearance", "preview",      10, N_("Style") },
  { "appearance", "schemes",      20, NULL },
  { "appearance", "font",         30, NULL },
  { "appearance", "effects",      40, NULL },
  { "appearance", "lines",        50, NULL },
  { "appearance", "brackets",     60, NULL },
  { "appearance", "accessories", 100, NULL },

  { "keyboard",   "movement",      10, N_("Movements") },
};

static const IdePreferenceGroupEntry lang_groups[] = {
  { "languages/*", "general",      0, N_("General") },
  { "languages/*", "margins",     10, N_("Margins") },
  { "languages/*", "spacing",     20, N_("Spacing") },
  { "languages/*", "indentation", 30, N_("Indentation") },
};

static const IdePreferenceItemEntry items[] = {
  { "appearance", "font", "font-name", 0, ide_preferences_window_font,
    N_("Editor Font"),
    N_("The font used within the source code editor"),
    "org.gnome.builder.editor", NULL, "font-name" },

  { "appearance", "effects", "show-grid-lines", 10, ide_preferences_window_toggle,
    N_("Show Grid Pattern"),
    N_("Display a grid pattern underneath source code"),
    "org.gnome.builder.editor", NULL, "show-grid-lines" },

  { "appearance", "effects", "show-map", 10, ide_preferences_window_toggle,
    N_("Show Overview Map"),
    N_("Use an overview map instead of a scrollbar"),
    "org.gnome.builder.editor", NULL, "show-map" },

  { "appearance", "lines", "show-line-numbers", 0, ide_preferences_window_toggle,
    N_("Show Line Numbers"),
    N_("Display line numbers next to each line of source code"),
    "org.gnome.builder.editor", NULL, "show-line-numbers" },

  { "appearance", "lines", "line-height", 0, ide_preferences_window_spin,
    N_("Line Height"),
    N_("Adjust line-height of the configured font"),
    "org.gnome.builder.editor", NULL, "line-height" },

  { "appearance", "lines", "highlight-current-line", 20, ide_preferences_window_toggle,
    N_("Highlight Current Line"),
    N_("Make current line stand out with highlights"),
    "org.gnome.builder.editor", NULL, "highlight-current-line" },

  { "appearance", "brackets", "highlight-matching-brackets", 30, ide_preferences_window_toggle,
    N_("Highlight Matching Brackets"),
    N_("Use cursor position to highlight matching brackets, braces, parenthesis, and more"),
    "org.gnome.builder.editor", NULL, "highlight-matching-brackets" },

  { "editing", "completion", "interactive-completion", 10, ide_preferences_window_toggle,
    N_("Suggest Completions While Typing"),
    N_("Automatically suggest completions while typing within the file"),
    "org.gnome.builder.editor", NULL, "interactive-completion" },

  { "editing", "completion", "select-first-completion", 20, ide_preferences_window_toggle,
    N_("Select First Completion"),
    N_("Automatically select the first completion when displayed"),
    "org.gnome.builder.editor", NULL, "select-first-completion" },

  { "editing", "completion", "enable-snippets", 30, ide_preferences_window_toggle,
    N_("Expand Snippets"),
    N_("Use “Tab” to expand configured snippets in the editor"),
    "org.gnome.builder.editor", NULL, "enable-snippets" },

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
    "org.gnome.builder.editor.language", LANG_PATH, "trim-trailing-whitespace" },

  { "languages/*", "general", "overwrite", 0, ide_preferences_window_toggle,
    N_("Overwrite Braces"),
    N_("Overwrite closing braces"),
    "org.gnome.builder.editor.language", LANG_PATH, "overwrite-braces" },

  { "languages/*", "general", "insert-matching", 0, ide_preferences_window_toggle,
    N_("Insert Matching Brace"),
    N_("Insert matching character for [[(\"'"),
    "org.gnome.builder.editor.language", LANG_PATH, "insert-matching-brace" },

  { "languages/*", "general", "insert-trailing", 0, ide_preferences_window_toggle,
    N_("Insert Trailing Newline"),
    N_("Ensure files end with a newline"),
    "org.gnome.builder.editor.language", LANG_PATH, "insert-trailing-newline" },

  { "languages/*", "margins", "show-right-margin", 0, ide_preferences_window_toggle,
    N_("Show right margin"),
    N_("Display a margin in the editor to indicate maximum desired width"),
    "org.gnome.builder.editor.language", LANG_PATH, "show-right-margin" },

#if 0
  { "languages/*", "spacing", "before-parens", 0, ide_preferences_window_toggle, "Prefer a space before opening parentheses" },
  { "languages/*", "spacing", "before-brackets", 0, ide_preferences_window_toggle, "Prefer a space before opening brackets" },
  { "languages/*", "spacing", "before-braces", 0, ide_preferences_window_toggle, "Prefer a space before opening braces" },
  { "languages/*", "spacing", "before-angles", 0, ide_preferences_window_toggle, "Prefer a space before opening angles" },
#endif

  { "languages/*", "indentation", "insert-spaces", 0, ide_preferences_window_toggle,
    N_("Insert spaces instead of tabs"),
    N_("Prefer spaces over tabs"),
    "org.gnome.builder.editor.language", LANG_PATH, "insert-spaces-instead-of-tabs" },

  { "languages/*", "indentation", "auto-indent", 0, ide_preferences_window_toggle,
    N_("Automatically Indent"),
    N_("Format source code as you type"),
    "org.gnome.builder.editor.language", LANG_PATH, "auto-indent" },
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

static gboolean
can_install_scheme (GtkSourceStyleSchemeManager *manager,
                    const char * const          *scheme_ids,
                    GFile                       *file)
{
  g_autofree char *uri = NULL;
  const char *path;

  g_assert (GTK_SOURCE_IS_STYLE_SCHEME_MANAGER (manager));
  g_assert (G_IS_FILE (file));

  uri = g_file_get_uri (file);

  /* Don't allow resources, which would be weird anyway */
  if (g_str_has_prefix (uri, "resource://"))
    return FALSE;

  /* Make sure it's in the form of name.xml as we will require
   * that elsewhere anyway.
   */
  if (!g_str_has_suffix (uri, ".xml"))
    return FALSE;

  /* Not a native file, so likely not already installed */
  if (!g_file_is_native (file))
    return TRUE;

  path = g_file_peek_path (file);
  scheme_ids = gtk_source_style_scheme_manager_get_scheme_ids (manager);
  for (guint i = 0; scheme_ids[i] != NULL; i++)
    {
      GtkSourceStyleScheme *scheme = gtk_source_style_scheme_manager_get_scheme (manager, scheme_ids[i]);
      const char *filename = gtk_source_style_scheme_get_filename (scheme);

      /* If we have already loaded this scheme, then ignore it */
      if (g_strcmp0 (filename, path) == 0)
        return FALSE;
    }

  return TRUE;
}

static gboolean
drop_scheme_cb (GtkDropTarget *drop_target,
                const GValue  *value,
                double         x,
                double         y,
                gpointer       user_data)
{
  g_assert (GTK_IS_DROP_TARGET (drop_target));

  if (G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST))
    {
      GSList *list = g_value_get_boxed (value);
      g_autoptr(GPtrArray) to_install = NULL;
      GtkSourceStyleSchemeManager *manager;
      const char * const *scheme_ids;

      if (list == NULL)
        return FALSE;

      manager = gtk_source_style_scheme_manager_get_default ();
      scheme_ids = gtk_source_style_scheme_manager_get_scheme_ids (manager);
      to_install = g_ptr_array_new_with_free_func (g_object_unref);

      for (const GSList *iter = list; iter; iter = iter->next)
        {
          GFile *file = iter->data;

          if (can_install_scheme (manager, scheme_ids, file))
            g_ptr_array_add (to_install, g_object_ref (file));
        }

      if (to_install->len == 0)
        return FALSE;

      /* TODO: We need to reload the preferences */
      ide_application_install_schemes_async (IDE_APPLICATION_DEFAULT,
                                             (GFile **)(gpointer)to_install->pdata,
                                             to_install->len,
                                             NULL, NULL, NULL);

      return TRUE;
    }

  return FALSE;
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
  GtkDropTarget *drop_target;
  GtkFlowBox *flowbox;
  GtkWidget *preview;

  g_assert (IDE_IS_PREFERENCES_WINDOW (window));
  g_assert (entry != NULL);
  g_assert (ADW_IS_PREFERENCES_GROUP (group));

  preview = gbp_editorui_preview_new ();
  gtk_widget_add_css_class (GTK_WIDGET (preview), "card");
  gtk_widget_set_margin_bottom (preview, 12);
  adw_preferences_group_add (group, preview);

  manager = gtk_source_style_scheme_manager_get_default ();
  scheme_ids = gtk_source_style_scheme_manager_get_scheme_ids (manager);

  flowbox = g_object_new (GTK_TYPE_FLOW_BOX,
                          "activate-on-single-click", TRUE,
                          "column-spacing", 12,
                          "row-spacing", 12,
                          "margin-top", 6,
                          "max-children-per-line", 4,
                          NULL);
  gtk_widget_add_css_class (GTK_WIDGET (flowbox), "style-schemes");

  /* Setup DnD for schemes to be dropped onto the section */
  drop_target = gtk_drop_target_new (GDK_TYPE_FILE_LIST, GDK_ACTION_COPY);
  g_signal_connect (drop_target,
                    "drop",
                    G_CALLBACK (drop_scheme_cb),
                    NULL);
  gtk_widget_add_controller (GTK_WIDGET (flowbox), GTK_EVENT_CONTROLLER (drop_target));

  for (guint i = 0; scheme_ids[i]; i++)
    {
      GtkSourceStyleScheme *scheme = gtk_source_style_scheme_manager_get_scheme (manager, scheme_ids[i]);
      GtkSourceStyleSchemePreview *selector;

      selector = g_object_new (GTK_SOURCE_TYPE_STYLE_SCHEME_PREVIEW,
                               "action-name", "app.style-scheme-name",
                               "scheme", scheme,
                               NULL);
      gtk_actionable_set_action_target (GTK_ACTIONABLE (selector), "s", scheme_ids[i]);
      gtk_flow_box_append (flowbox, GTK_WIDGET (selector));
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
gbp_editorui_preferences_addin_add_languages (IdePreferencesWindow *window,
                                              const char           *lang_path)
{
  GtkSourceLanguageManager *langs;
  const char * const *lang_ids;
  IdePreferencePageEntry *lpages;
  IdePreferenceItemEntry _items[G_N_ELEMENTS (lang_items)];
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

  memcpy (_items, lang_items, sizeof _items);
  for (guint i = 0; i < G_N_ELEMENTS (_items); i++)
    _items[i].path = lang_path;

  ide_preferences_window_add_pages (window, lpages, j, NULL);
  ide_preferences_window_add_groups (window, lang_groups, G_N_ELEMENTS (lang_groups), NULL);
  ide_preferences_window_add_items (window, _items, G_N_ELEMENTS (_items), window, NULL);

  g_free (lpages);
}


static void
gbp_editorui_preferences_addin_load (IdePreferencesAddin  *addin,
                                     IdePreferencesWindow *window,
                                     IdeContext           *context)
{
  GbpEditoruiPreferencesAddin *self = (GbpEditoruiPreferencesAddin *)addin;
  IdePreferencesMode mode;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_EDITORUI_PREFERENCES_ADDIN (self));
  g_assert (IDE_IS_PREFERENCES_WINDOW (window));
  g_assert (!context || IDE_IS_CONTEXT (context));

  mode = ide_preferences_window_get_mode (window);

  if (mode == IDE_PREFERENCES_MODE_APPLICATION)
    {
      ide_preferences_window_add_groups (window, groups, G_N_ELEMENTS (groups), NULL);
      ide_preferences_window_add_items (window, items, G_N_ELEMENTS (items), window, NULL);
      ide_preferences_window_add_item (window, "appearance", "preview", "scheme", 0,
                                       ide_preferences_builtin_add_schemes, window, NULL);
      gbp_editorui_preferences_addin_add_languages (window, LANG_PATH);
    }
  else if (mode == IDE_PREFERENCES_MODE_PROJECT && IDE_IS_CONTEXT (context))
    {
      g_autofree char *project_id = ide_context_dup_project_id (context);
      g_autofree char *project_lang_path = g_strdup_printf ("/org/gnome/builder/projects/%s/language/*", project_id);

      gbp_editorui_preferences_addin_add_languages (window, project_lang_path);
    }

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
