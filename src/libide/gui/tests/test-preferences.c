#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>
#include <libide-gui.h>

#include "ide-gui-resources.h"

static void create_source_view_cb (const char                   *page,
                                   const IdePreferenceItemEntry *item,
                                   AdwPreferencesGroup          *pref_group,
                                   gpointer                      user_data);
static void create_schemes_cb (const char                   *page,
                               const IdePreferenceItemEntry *item,
                               AdwPreferencesGroup          *pref_group,
                               gpointer                      user_data);
static void select_font_cb (const char                   *page,
                            const IdePreferenceItemEntry *item,
                            AdwPreferencesGroup          *pref_group,
                            gpointer                      user_data);
static void create_style_cb (const char                   *page,
                             const IdePreferenceItemEntry *item,
                             AdwPreferencesGroup          *pref_group,
                             gpointer                      user_data);
static void toggle_cb (const char                   *page,
                       const IdePreferenceItemEntry *item,
                       AdwPreferencesGroup          *pref_group,
                       gpointer                      user_data);
static void spin_cb (const char                   *page,
                     const IdePreferenceItemEntry *item,
                     AdwPreferencesGroup          *pref_group,
                     gpointer                      user_data);

static const IdePreferencePageEntry pages[] = {
  { NULL, "visual", "appearance", "org.gnome.Builder-appearance-symbolic", 0, "Appearance" },
  { NULL, "visual", "editing", "org.gnome.Builder-editing-symbolic", 10, "Editing" },
  { NULL, "visual", "keyboard", "org.gnome.Builder-shortcuts-symbolic", 20, "Shortcuts" },
  { NULL, "code", "languages", "org.gnome.Builder-languages-symbolic", 100, "Languages" },
  { NULL, "code", "completion", "org.gnome.Builder-completion-symbolic", 110, "Completion" },
  { NULL, "code", "insight", "org.gnome.Builder-diagnostics-symbolic", 120, "Diagnostics" },
  { NULL, "projects", "projects", "org.gnome.Builder-projects-symbolic", 200, "Projects" },
  { NULL, "tools", "build", "org.gnome.Builder-build-symbolic", 300, "Build" },
  { NULL, "tools", "debug", "org.gnome.Builder-debugger-symbolic", 310, "Debugger" },
  { NULL, "tools", "commands", "org.gnome.Builder-command-symbolic", 320, "Commands" },
  { NULL, "tools", "sdks", "org.gnome.Builder-sdk-symbolic", 500, "SDKs" },
  { NULL, "plugins", "plugins", "org.gnome.Builder-plugins-symbolic", 600, "Plugins" },
};

static const IdePreferenceGroupEntry groups[] = {
  { "appearance", "style", 0, "Appearance" },
  { "appearance", "preview", 0, "Style" },
  { "appearance", "schemes", 10, NULL },
  { "appearance", "font", 20, NULL },
  { "appearance", "accessories", 20, NULL },

  { "languages/*", "general", 0, "General" },
  { "languages/*", "margins", 10, "Margins" },
  { "languages/*", "spacing", 20, "Spacing" },
  { "languages/*", "indentation", 30, "Indentation" },
};

static const IdePreferenceItemEntry items[] = {
  { "appearance", "style", "style", 0, create_style_cb },
  { "appearance", "preview", "sourceview", 0, create_source_view_cb },
  { "appearance", "schemes", "schemes", 0, create_schemes_cb },
  { "appearance", "font", "font", 0, select_font_cb },
};

static const IdePreferenceItemEntry lang_items[] = {
  { "languages/*", "general", "trim", 0, toggle_cb, "Trim Trailing Whitespace", "Upon saving, trailing whitepsace from modified lines will be trimmed." },
  { "languages/*", "general", "overwrite", 0, toggle_cb, "Overwrite Braces", "Overwrite closing braces" },
  { "languages/*", "general", "insert-matching", 0, toggle_cb, "Insert Matching Brace", "Insert matching character for [[(\"'" },
  { "languages/*", "general", "insert-trailing", 0, toggle_cb, "Insert Trailing Newline", "Ensure files end with a newline" },

  { "languages/*", "margins", "show-right-margin", 0, toggle_cb, "Show right margin", "Display a margin in the editor to indicate maximum desired width" },
  { "languages/*", "margins", "right-margin", 0, spin_cb, "Right margin position", "The position of the right margin in characters" },

  { "languages/*", "spacing", "before-parens", 0, toggle_cb, "Prefer a space before opening parentheses" },
  { "languages/*", "spacing", "before-brackets", 0, toggle_cb, "Prefer a space before opening brackets" },
  { "languages/*", "spacing", "before-braces", 0, toggle_cb, "Prefer a space before opening braces" },
  { "languages/*", "spacing", "before-angles", 0, toggle_cb, "Prefer a space before opening angles" },

  { "languages/*", "indentation", "tab-width", 0, spin_cb, "Tab width", "Width of a tab character in spaces" },
  { "languages/*", "indentation", "insert-spaces", 0, toggle_cb, "Insert spaces instead of tabs", "Prefer spaces over tabs" },
  { "languages/*", "indentation", "auto-indent", 0, toggle_cb, "Automatically Indent", "Format source code as you type" },
};

static int
compare_section (gconstpointer a,
                 gconstpointer b)
{
  const IdePreferencePageEntry *pagea = a;
  const IdePreferencePageEntry *pageb = b;

  return g_strcmp0 (pagea->section, pageb->section);
}

int
main (int argc,
      char *argv[])
{
  IdePreferencesWindow *window;
  GtkSourceLanguageManager *langs;
  const char * const *lang_ids;
  GMainLoop *main_loop;
  IdePreferencePageEntry *lpages;
  guint j = 0;

  gtk_init ();
  adw_init ();
  gtk_source_init ();

  g_resources_register (ide_gui_get_resource ());

  main_loop = g_main_loop_new (NULL, FALSE);
  window = IDE_PREFERENCES_WINDOW (ide_preferences_window_new (IDE_PREFERENCES_MODE_EMPTY, NULL));

  gtk_window_set_default_size (GTK_WINDOW (window), 1200, 900);

  ide_preferences_window_add_pages (window, pages, G_N_ELEMENTS (pages), NULL);
  ide_preferences_window_add_groups (window, groups, G_N_ELEMENTS (groups), NULL);
  ide_preferences_window_add_items (window, items, G_N_ELEMENTS (items), NULL, NULL);
  ide_preferences_window_add_items (window, lang_items, G_N_ELEMENTS (lang_items), NULL, NULL);

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
  g_free (lpages);

  g_signal_connect_swapped (window, "close-request", G_CALLBACK (g_main_loop_quit), main_loop);
  gtk_window_present (GTK_WINDOW (window));
  g_main_loop_run (main_loop);

  return 0;
}

static void
create_source_view_cb (const char                   *page,
                       const IdePreferenceItemEntry *item,
                       AdwPreferencesGroup          *pref_group,
                       gpointer                      user_data)
{
  GtkWidget *frame;
  GtkWidget *view;

  frame = g_object_new (GTK_TYPE_FRAME, NULL);
  view = g_object_new (GTK_SOURCE_TYPE_VIEW,
                       "show-line-numbers", TRUE,
                       "highlight-current-line", TRUE,
                       "hexpand", TRUE,
                       NULL);
  gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)), "\n\n\n\n", -1);
  gtk_frame_set_child (GTK_FRAME (frame), view);
  adw_preferences_group_add (pref_group, frame);
}

static void
create_schemes_cb (const char                   *page,
                   const IdePreferenceItemEntry *item,
                   AdwPreferencesGroup          *pref_group,
                   gpointer                      user_data)
{
  GtkSourceStyleSchemeManager *manager = gtk_source_style_scheme_manager_get_default ();
  const char * const *scheme_ids;
  GtkFlowBox *flow;

  flow = g_object_new (GTK_TYPE_FLOW_BOX,
                       "column-spacing", 6,
                       "row-spacing", 6,
                       NULL);

  scheme_ids = gtk_source_style_scheme_manager_get_scheme_ids (manager);

  for (guint i = 0; scheme_ids[i]; i++)
    {
      GtkSourceStyleScheme *scheme = gtk_source_style_scheme_manager_get_scheme (manager, scheme_ids[i]);
      GtkWidget *preview = gtk_source_style_scheme_preview_new (scheme);

      gtk_flow_box_append (flow, preview);
    }

  adw_preferences_group_add (pref_group, GTK_WIDGET (flow));
}

static void
select_font_cb (const char                   *page,
                const IdePreferenceItemEntry *item,
                AdwPreferencesGroup          *pref_group,
                gpointer                      user_data)
{
  AdwExpanderRow *row;
  AdwActionRow *font;

  row = g_object_new (ADW_TYPE_EXPANDER_ROW,
                      "title", _("Custom Font"),
                      "show-enable-switch", TRUE,
                      NULL);
  adw_preferences_group_add (pref_group, GTK_WIDGET (row));

  font = g_object_new (ADW_TYPE_ACTION_ROW,
                       "title", "Editor",
                       "subtitle", "Monospace 11",
                       NULL);
  adw_action_row_add_suffix (font, gtk_image_new_from_icon_name ("go-next-symbolic"));
  adw_expander_row_add_row (row, GTK_WIDGET (font));

  font = g_object_new (ADW_TYPE_ACTION_ROW,
                       "title", "Terminal",
                       "subtitle", "Monospace 11",
                       NULL);
  adw_action_row_add_suffix (font, gtk_image_new_from_icon_name ("go-next-symbolic"));
  adw_expander_row_add_row (row, GTK_WIDGET (font));
}

static void
create_style_cb (const char                   *page,
                 const IdePreferenceItemEntry *item,
                 AdwPreferencesGroup          *pref_group,
                 gpointer                      user_data)
{
  GtkBox *box;
  AdwPreferencesRow *row;

  box = g_object_new (GTK_TYPE_BOX,
                      "margin-top", 24,
                      "margin-end", 24,
                      "margin-start", 24,
                      "margin-bottom", 24,
                      "spacing", 24,
                      "homogeneous", TRUE,
                      "hexpand", TRUE,
                      NULL);
  row = g_object_new (ADW_TYPE_PREFERENCES_ROW,
                      "child", box,
                      NULL);

  gtk_box_append (box, gtk_button_new_with_label ("System"));
  gtk_box_append (box, gtk_button_new_with_label ("Light"));
  gtk_box_append (box, gtk_button_new_with_label ("Dark"));

  for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (box));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      gtk_widget_set_hexpand (child, TRUE);
      gtk_widget_set_size_request (child, -1, 96);
    }

  adw_preferences_group_add (pref_group, GTK_WIDGET (row));
}

static void
toggle_cb (const char                   *page,
           const IdePreferenceItemEntry *item,
           AdwPreferencesGroup          *pref_group,
           gpointer                      user_data)
{
  AdwActionRow *row;
  GtkSwitch *child;

  child = g_object_new (GTK_TYPE_SWITCH,
                        "active", TRUE,
                        "valign", GTK_ALIGN_CENTER,
                        NULL);
  row = g_object_new (ADW_TYPE_ACTION_ROW,
                      "title", item->title,
                      "subtitle", item->subtitle,
                      "activatable-widget", child,
                      NULL);
  adw_preferences_group_add (pref_group, GTK_WIDGET (row));
  adw_action_row_add_suffix (row, GTK_WIDGET (child));
}

static void
spin_cb (const char                   *page,
          const IdePreferenceItemEntry *item,
          AdwPreferencesGroup          *pref_group,
          gpointer                      user_data)
{
  AdwActionRow *row;
  GtkSwitch *child;

  child = g_object_new (GTK_TYPE_SPIN_BUTTON,
                        "valign", GTK_ALIGN_CENTER,
                        NULL);
  row = g_object_new (ADW_TYPE_ACTION_ROW,
                      "title", item->title,
                      "subtitle", item->subtitle,
                      "activatable-widget", child,
                      NULL);
  adw_preferences_group_add (pref_group, GTK_WIDGET (row));
  adw_action_row_add_suffix (row, GTK_WIDGET (child));
}
