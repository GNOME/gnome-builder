/* gb-preferences-page-language.c
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

#include <glib/gi18n.h>

#include <gtksourceview/gtksource.h>
#include <string.h>

#include "gb-editor-settings-widget.h"
#include "gb-preferences-page-language.h"
#include "gb-string.h"

struct _GbPreferencesPageLanguagePrivate
{
  GtkStack       *stack;
  GtkListBox     *language_list_box;
  GtkSearchEntry *search_entry;
  GtkBox         *language_selection;
  GtkWidget      *language_settings;
  GtkBox         *language_settings_box;
  GtkButton      *back_button;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbPreferencesPageLanguage,
                            gb_preferences_page_language,
                            GB_TYPE_PREFERENCES_PAGE)

GtkWidget *
make_language_row (GtkSourceLanguage *language)
{
  GtkListBoxRow *row;
  GtkBox *box;
  GtkLabel *label;
  const gchar *name;

  box = g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_HORIZONTAL,
                      "spacing", 6,
                      "visible", TRUE,
                      NULL);

  name = gtk_source_language_get_name (language);

  label = g_object_new (GTK_TYPE_LABEL,
                        "hexpand", TRUE,
                        "visible", TRUE,
                        "margin-top", 6,
                        "margin-bottom", 6,
                        "margin-start", 6,
                        "margin-end", 6,
                        "xalign", 0.0f,
                        "label", name,
                        NULL);
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (label));

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      "visible", TRUE,
                      NULL);
  gtk_container_add (GTK_CONTAINER (row), GTK_WIDGET (box));

  g_object_set_data (G_OBJECT (row), "GTK_SOURCE_LANGUAGE", language);

  return GTK_WIDGET (row);
}

static void
item_header_func (GtkListBoxRow *row,
                  GtkListBoxRow *before,
                  gpointer       user_data)
{
  g_return_if_fail (GTK_IS_LIST_BOX_ROW (row));

  if (before)
    {
      GtkWidget *sep;

      sep = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_list_box_row_set_header (row, sep);
      gtk_widget_show (sep);
    }
}

static gboolean
item_filter_func (GtkListBoxRow *row,
                  gpointer       user_data)
{
  GtkEntry *entry = user_data;
  GtkSourceLanguage *lang;
  const gchar *text;

  g_return_val_if_fail (GTK_IS_LIST_BOX_ROW (row), FALSE);
  g_return_val_if_fail (GTK_IS_ENTRY (entry), FALSE);

  text = gtk_entry_get_text (entry);

  if (gb_str_empty0 (text))
    return TRUE;

  lang = g_object_get_data (G_OBJECT (row), "GTK_SOURCE_LANGUAGE");
  g_assert (lang);

  if (strstr (gtk_source_language_get_id (lang), text) ||
      strstr (gtk_source_language_get_name (lang), text))
    return TRUE;

  return FALSE;
}

static void
search_entry_changed (GtkEntry   *entry,
                      GtkListBox *list_box)
{
  g_return_if_fail (GTK_IS_LIST_BOX (list_box));

  gtk_list_box_invalidate_filter (list_box);
}

static void
row_selected (GtkListBox                *list_box,
              GtkListBoxRow             *row,
              GbPreferencesPageLanguage *page)
{
  GtkSourceLanguage *lang;
  GbEditorSettingsWidget *widget;
  const gchar *lang_id;

  g_assert (GTK_IS_LIST_BOX (list_box));
  g_assert (!row || GTK_IS_LIST_BOX_ROW (row));
  g_assert (GB_IS_PREFERENCES_PAGE_LANGUAGE (page));

  if (!row)
    return;

  lang = g_object_get_data (G_OBJECT (row), "GTK_SOURCE_LANGUAGE");
  if (!lang)
    return;

  lang_id = gtk_source_language_get_id (lang);

  widget = g_object_new (GB_TYPE_EDITOR_SETTINGS_WIDGET,
                         "border-width", 12,
                         "language", lang_id,
                         "visible", TRUE,
                         NULL);
  gtk_container_add (GTK_CONTAINER (page->priv->language_settings_box), GTK_WIDGET (widget));
  gtk_stack_set_visible_child (page->priv->stack,GTK_WIDGET (page->priv->language_settings));
  gb_preferences_page_set_title (GB_PREFERENCES_PAGE (page), gtk_source_language_get_name (lang));
}

static void
stack_notify_visible_child (GbPreferencesPageLanguage *page,
                            GParamSpec                *pspec,
                            GtkStack                  *stack)
{
  GtkWidget *visible_child;

  g_assert (GB_IS_PREFERENCES_PAGE_LANGUAGE (page));
  g_assert (GTK_IS_STACK (stack));

  visible_child = gtk_stack_get_visible_child (stack);
  if (visible_child == GTK_WIDGET (page->priv->language_selection))
    {
      GList *children;
      GList *iter;

      children = gtk_container_get_children (GTK_CONTAINER (page->priv->language_settings_box));
      for(iter = children; iter != NULL; iter = g_list_next (iter))
        gtk_widget_destroy (GTK_WIDGET (iter->data));
      g_list_free (children);

      gtk_list_box_unselect_all (page->priv->language_list_box);
      gtk_widget_hide (GTK_WIDGET (page->priv->back_button));
      gb_preferences_page_reset_title (GB_PREFERENCES_PAGE (page));
    }
  else if (visible_child == GTK_WIDGET (page->priv->language_settings))
    {
      gtk_widget_show (GTK_WIDGET (page->priv->back_button));
    }
}

static void
back_button_clicked_cb (GbPreferencesPageLanguage *page,
                        GtkButton                 *back_button)
{
  g_assert (GB_IS_PREFERENCES_PAGE_LANGUAGE (page));
  g_assert (GTK_IS_BUTTON (back_button));

  gtk_stack_set_visible_child (page->priv->stack, GTK_WIDGET (page->priv->language_selection));
}

static void
gb_preferences_page_language_constructed (GObject *object)
{
  GtkSourceLanguageManager *manager;
  GbPreferencesPageLanguage *page = (GbPreferencesPageLanguage *)object;
  const gchar * const *lang_ids;
  guint i;

  gtk_list_box_set_header_func (page->priv->language_list_box,
                                item_header_func, NULL, NULL);
  gtk_list_box_set_filter_func (page->priv->language_list_box,
                                item_filter_func, page->priv->search_entry,
                                NULL);

  g_signal_connect (page->priv->search_entry,
                    "changed",
                    G_CALLBACK (search_entry_changed),
                    page->priv->language_list_box);

  g_signal_connect (page->priv->language_list_box,
                    "row-selected",
                    G_CALLBACK (row_selected),
                    page);

  g_signal_connect_object (page->priv->back_button,
                          "clicked",
                           G_CALLBACK (back_button_clicked_cb),
                           page,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (page->priv->stack,
                           "notify::visible-child",
                           G_CALLBACK (stack_notify_visible_child),
                           page,
                           G_CONNECT_SWAPPED);



  manager = gtk_source_language_manager_get_default ();
  lang_ids = gtk_source_language_manager_get_language_ids (manager);

  for (i = 0; lang_ids [i]; i++)
    {
      GtkSourceLanguage *lang;
      GtkWidget *widget;
      gchar *keywords;

      if (g_str_equal (lang_ids [i], "def"))
        continue;

      lang = gtk_source_language_manager_get_language (manager, lang_ids [i]);
      widget = make_language_row (lang);

      keywords = g_strdup_printf ("%s %s %s",
                                  gtk_source_language_get_id (lang),
                                  gtk_source_language_get_name (lang),
                                  gtk_source_language_get_section (lang));
      gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (object),
                                                   keywords, widget, NULL);
      g_free (keywords);

      gtk_container_add (GTK_CONTAINER (page->priv->language_list_box), widget);
    }

  G_OBJECT_CLASS (gb_preferences_page_language_parent_class)->constructed (object);
}

static void
gb_preferences_page_language_finalize (GObject *object)
{
  G_OBJECT_CLASS (gb_preferences_page_language_parent_class)->finalize (object);
}

static void
gb_preferences_page_language_class_init (GbPreferencesPageLanguageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gb_preferences_page_language_constructed;
  object_class->finalize = gb_preferences_page_language_finalize;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/builder/ui/gb-preferences-page-language.ui");
  gtk_widget_class_bind_template_child_private (widget_class,
                                                GbPreferencesPageLanguage,
                                                stack);
  gtk_widget_class_bind_template_child_private (widget_class,
                                                GbPreferencesPageLanguage,
                                                language_list_box);
  gtk_widget_class_bind_template_child_private (widget_class,
                                                GbPreferencesPageLanguage,
                                                search_entry);
  gtk_widget_class_bind_template_child_private (widget_class,
                                                GbPreferencesPageLanguage,
                                                language_selection);
  gtk_widget_class_bind_template_child_private (widget_class,
                                                GbPreferencesPageLanguage,
                                                language_settings);
  gtk_widget_class_bind_template_child_private (widget_class,
                                                GbPreferencesPageLanguage,
                                                language_settings_box);
  gtk_widget_class_bind_template_child_private (widget_class,
                                                GbPreferencesPageLanguage,
                                                back_button);

  g_type_ensure (GB_TYPE_EDITOR_SETTINGS_WIDGET);
}

static void
gb_preferences_page_language_init (GbPreferencesPageLanguage *self)
{
  self->priv = gb_preferences_page_language_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));
}
