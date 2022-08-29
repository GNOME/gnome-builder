/* gbp-editorui-tweaks-addin.c
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

#define G_LOG_DOMAIN "gbp-editorui-tweaks-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-gui.h>

#include "gbp-editorui-preview.h"
#include "gbp-editorui-scheme-selector.h"
#include "gbp-editorui-tweaks-addin.h"

struct _GbpEditoruiTweaksAddin
{
  IdeTweaksAddin parent_instance;
};

G_DEFINE_FINAL_TYPE (GbpEditoruiTweaksAddin, gbp_editorui_tweaks_addin, IDE_TYPE_TWEAKS_ADDIN)

static GtkWidget *
editorui_create_style_scheme_preview (GbpEditoruiTweaksAddin *self,
                                      IdeTweaksWidget        *widget,
                                      IdeTweaksWidget        *instance)
{
  g_assert (GBP_IS_EDITORUI_TWEAKS_ADDIN (self));
  g_assert (IDE_IS_TWEAKS_WIDGET (widget));

  return g_object_new (GBP_TYPE_EDITORUI_PREVIEW,
                       "bottom-margin", 8,
                       "css-classes", IDE_STRV_INIT ("card"),
                       "cursor-visible", FALSE,
                       "left-margin", 12,
                       "monospace", TRUE,
                       "right-margin", 12,
                       "right-margin-position", 30,
                       "top-margin", 8,
                       NULL);
}

static GtkWidget *
editorui_create_style_scheme_selector (GbpEditoruiTweaksAddin *self,
                                       IdeTweaksWidget        *widget,
                                       IdeTweaksWidget        *instance)
{
  g_assert (GBP_IS_EDITORUI_TWEAKS_ADDIN (self));
  g_assert (IDE_IS_TWEAKS_WIDGET (widget));

  return g_object_new (GBP_TYPE_EDITORUI_SCHEME_SELECTOR,
                       "margin-top", 18,
                       NULL);
}

static void
reset_language_overrides_cb (GtkButton  *button,
                             IdeContext *context)
{
  GSettingsSchemaSource *source;
  g_autofree char *project_id = NULL;
  g_autofree char *schema_path = NULL;
  g_autoptr(GSettingsSchema) schema = NULL;
  g_autoptr(GSettings) settings = NULL;
  g_auto(GStrv) keys = NULL;
  const char *lang_id;

  g_assert (GTK_IS_BUTTON (button));
  g_assert (IDE_IS_CONTEXT (context));

  if (!(lang_id = g_object_get_data (G_OBJECT (button), "LANGUAGE")) ||
      !(project_id = ide_context_dup_project_id (context)) ||
      !(schema_path = ide_settings_resolve_schema_path ("org.gnome.builder.editor.language", project_id, lang_id)))
    return;

  source = g_settings_schema_source_get_default ();
  schema = g_settings_schema_source_lookup (source, "org.gnome.builder.editor.language", TRUE);
  keys = g_settings_schema_list_keys (schema);
  settings = g_settings_new_with_path ("org.gnome.builder.editor.language", schema_path);

  for (guint i = 0; keys[i]; i++)
    g_settings_reset (settings, keys[i]);
}

static GtkWidget *
create_language_reset_cb (GbpEditoruiTweaksAddin *self,
                          IdeTweaksWidget        *widget,
                          IdeTweaksWidget        *instance)
{
  g_autoptr(GObject) language = NULL;
  IdeTweaksBinding *binding;
  const char *lang_id;
  IdeContext *context;
  IdeTweaks *tweaks;
  GtkButton *button;

  g_assert (GBP_IS_EDITORUI_TWEAKS_ADDIN (self));
  g_assert (IDE_IS_TWEAKS_WIDGET (widget));
  g_assert (IDE_IS_TWEAKS_WIDGET (instance));

  if (!(binding = ide_tweaks_widget_get_binding (widget)) ||
      !(language = ide_tweaks_property_dup_object (IDE_TWEAKS_PROPERTY (binding))) ||
      !(lang_id = gtk_source_language_get_id (GTK_SOURCE_LANGUAGE (language))) ||
      !(tweaks = IDE_TWEAKS (ide_tweaks_item_get_root (IDE_TWEAKS_ITEM (widget)))) ||
      !(context = ide_tweaks_get_context (tweaks)))
    return NULL;

  button = g_object_new (GTK_TYPE_BUTTON,
                         "css-classes", IDE_STRV_INIT ("destructive-action"),
                         "label", _("Reset"),
                         "tooltip-text", _("Reverts language preferences to application defaults"),
                         "halign", GTK_ALIGN_END,
                         "width-request", 120,
                         NULL);
  g_object_set_data (G_OBJECT (button),
                     "LANGUAGE",
                     (gpointer)g_intern_string (lang_id));
  g_signal_connect_object (button,
                           "clicked",
                           G_CALLBACK (reset_language_overrides_cb),
                           context,
                           0);

  return GTK_WIDGET (button);
}

static int
compare_by_section (gconstpointer a,
                    gconstpointer b,
                    gpointer      user_data)
{
  GtkSourceLanguage *l_a = (GtkSourceLanguage *)a;
  GtkSourceLanguage *l_b = (GtkSourceLanguage *)b;

  return g_strcmp0 (gtk_source_language_get_section (l_a),
                    gtk_source_language_get_section (l_b));
}

static void
gbp_editorui_tweaks_addin_load (IdeTweaksAddin *addin,
                                IdeTweaks      *tweaks)
{
  GbpEditoruiTweaksAddin *self = (GbpEditoruiTweaksAddin *)addin;
  g_autoptr(GListStore) store = NULL;
  GtkSourceLanguageManager *lm;
  const char * const *ids;

  g_assert (GBP_IS_EDITORUI_TWEAKS_ADDIN (self));

  lm = gtk_source_language_manager_get_default ();
  ids = gtk_source_language_manager_get_language_ids (lm);
  store = g_list_store_new (GTK_SOURCE_TYPE_LANGUAGE);

  for (guint i = 0; ids[i]; i++)
    {
      GtkSourceLanguage *l = gtk_source_language_manager_get_language (lm, ids[i]);

      if (!gtk_source_language_get_hidden (l))
        g_list_store_append (store, l);
    }

  g_list_store_sort (store, compare_by_section, NULL);

  ide_tweaks_addin_set_resource_paths (IDE_TWEAKS_ADDIN (self),
                                       IDE_STRV_INIT ("/plugins/editorui/tweaks.ui",
                                                      "/plugins/editorui/tweaks-language.ui"));
  ide_tweaks_addin_bind_callback (IDE_TWEAKS_ADDIN (self), editorui_create_style_scheme_preview);
  ide_tweaks_addin_bind_callback (IDE_TWEAKS_ADDIN (self), editorui_create_style_scheme_selector);
  ide_tweaks_addin_bind_callback (IDE_TWEAKS_ADDIN (self), create_language_reset_cb);
  ide_tweaks_expose_object (tweaks, "GtkSourceLanguages", G_OBJECT (store));

  IDE_TWEAKS_ADDIN_CLASS (gbp_editorui_tweaks_addin_parent_class)->load (addin, tweaks);
}

static void
gbp_editorui_tweaks_addin_class_init (GbpEditoruiTweaksAddinClass *klass)
{
  IdeTweaksAddinClass *addin_class = IDE_TWEAKS_ADDIN_CLASS (klass);

  addin_class->load = gbp_editorui_tweaks_addin_load;
}

static void
gbp_editorui_tweaks_addin_init (GbpEditoruiTweaksAddin *self)
{
}
