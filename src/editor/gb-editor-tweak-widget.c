/* gb-editor-tweak-widget.c
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

#define G_LOG_DOMAIN "editor-tweak"

#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>

#include "gb-editor-tweak-widget.h"
#include "gb-string.h"
#include "gb-widget.h"

struct _GbEditorTweakWidget
{
  GtkBin          parent_instance;

  GtkSearchEntry *entry;
  GtkListBox     *list_box;
};

G_DEFINE_TYPE (GbEditorTweakWidget, gb_editor_tweak_widget, GTK_TYPE_BIN)

static GQuark gLangQuark;

GtkWidget *
gb_editor_tweak_widget_new (void)
{
  return g_object_new (GB_TYPE_EDITOR_TWEAK_WIDGET, NULL);
}

static gboolean
gb_editor_tweak_widget_filter_func (GtkListBoxRow *row,
                                    gpointer       user_data)
{
  GtkSourceLanguage *language;
  GtkWidget *child;
  const gchar *needle = user_data;
  const gchar *lang_id;
  const gchar *lang_name;
  g_autofree gchar *lang_name_cf = NULL;

  g_return_val_if_fail (GTK_IS_LIST_BOX_ROW (row), FALSE);
  g_return_val_if_fail (needle, FALSE);

  child = gtk_bin_get_child (GTK_BIN (row));
  language = g_object_get_qdata (G_OBJECT (child), gLangQuark);
  lang_id = gtk_source_language_get_id (language);
  lang_name = gtk_source_language_get_name (language);
  lang_name_cf = g_utf8_casefold (lang_name, -1);

  if (strstr (lang_id, needle) || strstr (lang_name, needle) || strstr (lang_name_cf, needle))
    return TRUE;

  return FALSE;
}

static void
gb_editor_tweak_widget_entry_changed (GbEditorTweakWidget *self,
                                      GtkEntry            *entry)
{
  const gchar *text;
  gchar *text_cf;

  g_return_if_fail (GB_IS_EDITOR_TWEAK_WIDGET (self));
  g_return_if_fail (GTK_IS_ENTRY (entry));

  text = gtk_entry_get_text (entry);

  if (gb_str_empty0 (text))
    gtk_list_box_set_filter_func (self->list_box, NULL, NULL, NULL);
  else
    {
      text_cf = g_utf8_casefold (text, -1);
      gtk_list_box_set_filter_func (self->list_box, gb_editor_tweak_widget_filter_func,
                                    text_cf, g_free);
    }
}

static void
gb_editor_tweak_widget_row_activated (GbEditorTweakWidget *self,
                                      GtkListBoxRow       *row,
                                      GtkListBox          *list_box)
{
  GtkSourceLanguage *lang;
  const gchar *lang_id;
  GtkWidget *child;
  GVariant *param;

  g_return_if_fail (GB_IS_EDITOR_TWEAK_WIDGET (self));
  g_return_if_fail (GTK_IS_LIST_BOX_ROW (row));
  g_return_if_fail (GTK_IS_LIST_BOX (list_box));

  child = gtk_bin_get_child (GTK_BIN (row));
  lang = g_object_get_qdata (G_OBJECT (child), gLangQuark);

  if (lang)
    {
      lang_id = gtk_source_language_get_id (lang);
      param = g_variant_new_string (lang_id);
      gb_widget_activate_action (GTK_WIDGET (self), "view", "language", param);
    }
}

static void
gb_editor_tweak_widget_constructed (GObject *object)
{
  GbEditorTweakWidget *self = (GbEditorTweakWidget *)object;
  GtkSourceLanguageManager *manager;
  GtkSourceLanguage *lang;
  const gchar * const *lang_ids;
  guint i;

  g_return_if_fail (GB_IS_EDITOR_TWEAK_WIDGET (self));

  G_OBJECT_CLASS (gb_editor_tweak_widget_parent_class)->constructed (object);

  manager = gtk_source_language_manager_get_default ();
  lang_ids = gtk_source_language_manager_get_language_ids (manager);

  for (i = 0; lang_ids [i]; i++)
    {
      GtkWidget *row;

      lang = gtk_source_language_manager_get_language (manager, lang_ids [i]);
      row = g_object_new (GTK_TYPE_LABEL,
                          "label", gtk_source_language_get_name (lang),
                          "visible", TRUE,
                          "xalign", 0.0f,
                          "margin-end", 6,
                          "margin-start", 6,
                          "margin-top", 3,
                          "margin-bottom", 3,
                          NULL);
      g_object_set_qdata (G_OBJECT (row), gLangQuark, lang);
      gtk_list_box_insert (self->list_box, row, -1);
    }

  g_signal_connect_object (self->entry,
                           "changed",
                           G_CALLBACK (gb_editor_tweak_widget_entry_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->list_box,
                           "row-activated",
                           G_CALLBACK (gb_editor_tweak_widget_row_activated),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
gb_editor_tweak_widget_class_init (GbEditorTweakWidgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gb_editor_tweak_widget_constructed;

  GB_WIDGET_CLASS_TEMPLATE (klass, "gb-editor-tweak-widget.ui");
  GB_WIDGET_CLASS_BIND (klass, GbEditorTweakWidget, entry);
  GB_WIDGET_CLASS_BIND (klass, GbEditorTweakWidget, list_box);

  gLangQuark = g_quark_from_static_string ("GtkSourceLanguage");
}

static void
gb_editor_tweak_widget_init (GbEditorTweakWidget *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
