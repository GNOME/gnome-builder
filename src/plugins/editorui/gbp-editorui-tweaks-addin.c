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
                                      IdeTweaksWidget        *widget)
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
                                       IdeTweaksWidget        *widget)
{
  g_assert (GBP_IS_EDITORUI_TWEAKS_ADDIN (self));
  g_assert (IDE_IS_TWEAKS_WIDGET (widget));

  return g_object_new (GBP_TYPE_EDITORUI_SCHEME_SELECTOR,
                       "margin-top", 18,
                       NULL);
}

static GtkWidget *
create_language_caption (IdeTweaks       *tweaks,
                         IdeTweaksWidget *widget)
{
  return g_object_new (GTK_TYPE_LABEL,
                       "css-classes", IDE_STRV_INIT ("caption", "dim-label"),
                       "label", _("Settings provided .editorconfig and modelines specified within files take precedence over those below."),
                       "xalign", .0f,
                       "wrap", TRUE,
                       NULL);
}

static GtkWidget *
create_spaces_style (IdeTweaksSettings *settings,
                     IdeTweaksWidget   *widget)
{
  static const struct {
    const char *nick;
    const char *title;
  } flags[] = {
    { "before-left-paren", N_("Space before opening parentheses") },
    { "before-left-bracket", N_("Space before opening brackets") },
    { "before-left-brace", N_("Space before opening braces") },
    { "before-left-angle", N_("Space before opening angles") },
    { "before-colon", N_("Prefer a space before colon") },
    { "before-comma", N_("Prefer a space before commas") },
    { "before-semicolon", N_("Prefer a space before semicolons") },
  };
  g_autoptr(GSimpleActionGroup) group = NULL;
  GtkListBox *list_box;

  list_box = g_object_new (GTK_TYPE_LIST_BOX,
                           "css-classes", IDE_STRV_INIT ("boxed-list"),
                           "selection-mode", GTK_SELECTION_NONE,
                           NULL);
  group = g_simple_action_group_new ();
  gtk_widget_insert_action_group (GTK_WIDGET (list_box),
                                  "spaces-style",
                                  G_ACTION_GROUP (group));

  for (guint i = 0; i < G_N_ELEMENTS (flags); i++)
    {
      g_autoptr(IdeSettingsFlagAction) action = NULL;
      const char *schema_id = ide_tweaks_settings_get_schema_id (settings);
      const char *schema_path = ide_tweaks_settings_get_schema_path (settings);
      g_autofree char *action_name = g_strdup_printf ("spaces-style.%s", flags[i].nick);
      GtkCheckButton *button = NULL;
      AdwActionRow *row;

      action = ide_settings_flag_action_new (schema_id, "spaces-style", schema_path, flags[i].nick);
      g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (action));

      button = g_object_new (GTK_TYPE_CHECK_BUTTON,
                             "action-name", action_name,
                             "can-target", FALSE,
                             "valign", GTK_ALIGN_CENTER,
                             NULL);
      gtk_widget_add_css_class (GTK_WIDGET (button), "checkimage");

      row = g_object_new (ADW_TYPE_ACTION_ROW,
                          "title", g_dgettext (GETTEXT_PACKAGE, flags[i].title),
                          "activatable-widget", button,
                          NULL);
      adw_action_row_add_suffix (row, GTK_WIDGET (button));
      gtk_list_box_append (list_box, GTK_WIDGET (row));
    }

  return GTK_WIDGET (list_box);
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

  ide_tweaks_addin_set_resource_paths (IDE_TWEAKS_ADDIN (self),
                                       IDE_STRV_INIT ("/plugins/editorui/tweaks.ui",
                                                      "/plugins/editorui/tweaks-language.ui"));
  ide_tweaks_addin_bind_callback (IDE_TWEAKS_ADDIN (self), editorui_create_style_scheme_preview);
  ide_tweaks_addin_bind_callback (IDE_TWEAKS_ADDIN (self), editorui_create_style_scheme_selector);
  ide_tweaks_addin_bind_callback (IDE_TWEAKS_ADDIN (self), create_language_caption);
  ide_tweaks_addin_bind_callback (IDE_TWEAKS_ADDIN (self), create_spaces_style);
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
