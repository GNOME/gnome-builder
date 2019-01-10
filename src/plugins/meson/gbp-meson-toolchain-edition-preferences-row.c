/* gbp-meson-toolchain-edition-preferences-row.c
 *
 * Copyright 2018 Corentin Noël <corentin.noel@collabora.com>
 * Copyright 2018 Collabora Ltd.
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

#define G_LOG_DOMAIN "gbp-meson-toolchain-edition-preferences-row"

#include <glib/gi18n.h>

#include "gbp-meson-toolchain-edition-preferences-row.h"
#include "gbp-meson-tool-row.h"
#include "gbp-meson-utils.h"

struct _GbpMesonToolchainEditionPreferencesRow
{
  DzlPreferencesBin parent_instance;
  gchar *toolchain_path;
  GtkLabel *display_name;
  GtkEntry *name_entry;
  DzlFileChooserEntry *sysroot_entry;
  GtkComboBox *arch_combobox;
  GtkComboBox *tool_combobox;
  GtkComboBox *lang_combobox;
  DzlFileChooserEntry *path_entry;
  GtkListBox *tools_listbox;
  GtkButton *add_button;
  GtkButton *delete_button;
  GtkPopover *popover;
};

G_DEFINE_TYPE (GbpMesonToolchainEditionPreferencesRow, gbp_meson_toolchain_edition_preferences_row, DZL_TYPE_PREFERENCES_BIN)

enum {
  PROP_0,
  PROP_TOOLCHAIN_PATH,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
meson_toolchain_edition_preferences_row_name_changed (GbpMesonToolchainEditionPreferencesRow *self,
                                                      gpointer                                user_data)
{
  const gchar *entry_text;
  g_autofree gchar *user_folder_path = NULL;
  g_autofree gchar *possible_path = NULL;
  GtkStyleContext *style_context;

  g_assert (GBP_IS_MESON_TOOLCHAIN_EDITION_PREFERENCES_ROW (self));
  g_assert (GTK_IS_ENTRY (user_data));

  style_context = gtk_widget_get_style_context (GTK_WIDGET (user_data));
  entry_text = gtk_entry_get_text (GTK_ENTRY (user_data));
  user_folder_path = g_build_filename (g_get_user_data_dir (), "meson", "cross", NULL);
  possible_path = g_build_filename (user_folder_path, entry_text, NULL);
  if (g_strcmp0 (possible_path, self->toolchain_path) == 0)
    {
      if (gtk_style_context_has_class (style_context, GTK_STYLE_CLASS_ERROR))
        gtk_style_context_remove_class (style_context, GTK_STYLE_CLASS_ERROR);

      return;
    }

  if (!g_file_test (possible_path, G_FILE_TEST_EXISTS))
    {
      g_autoptr(GFile) source = g_file_new_for_path (self->toolchain_path);
      g_autoptr(GFile) destination = g_file_new_for_path (possible_path);
      g_autoptr (GError) error = NULL;

      if (!g_file_move (source, destination, G_FILE_COPY_NONE, NULL, NULL, NULL, &error))
        {
          gtk_style_context_add_class (style_context, GTK_STYLE_CLASS_ERROR);
          g_message ("Unable to rename file: %s", error->message);
          return;
        }

      if (gtk_style_context_has_class (style_context, GTK_STYLE_CLASS_ERROR))
        gtk_style_context_remove_class (style_context, GTK_STYLE_CLASS_ERROR);

      g_object_set (self, "toolchain-path", possible_path, NULL);
      gtk_label_set_label (self->display_name, entry_text);
    }
  else
    gtk_style_context_add_class (style_context, GTK_STYLE_CLASS_ERROR);

}

static void
meson_toolchain_edition_preferences_row_arch_changed (GbpMesonToolchainEditionPreferencesRow *self,
                                                      gpointer                                user_data)
{
  g_auto (GStrv) parts = NULL;
  g_autoptr(GKeyFile) keyfile = g_key_file_new ();
  g_autoptr(GError) error = NULL;
  const gchar *entry_text;

  g_assert (GBP_IS_MESON_TOOLCHAIN_EDITION_PREFERENCES_ROW (self));
  g_assert (GTK_IS_COMBO_BOX (user_data));

  entry_text = gtk_entry_get_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (self->arch_combobox))));
  parts = g_strsplit (entry_text, "-", 2);

  if (!g_key_file_load_from_file (keyfile, self->toolchain_path, G_KEY_FILE_KEEP_COMMENTS|G_KEY_FILE_KEEP_TRANSLATIONS, &error))
    {
      g_message ("Unable to load file \"%s\": %s", self->toolchain_path, error->message);
      return;
    }

  gbp_meson_key_file_set_string_quoted (keyfile, "host_machine", "cpu_family", parts[0]);
  gbp_meson_key_file_set_string_quoted (keyfile, "host_machine", "cpu", parts[0]);
  gbp_meson_key_file_set_string_quoted (keyfile, "host_machine", "system", parts[1]);
  if (!g_key_file_has_key (keyfile, "host_machine", "endian", NULL))
    gbp_meson_key_file_set_string_quoted (keyfile, "host_machine", "endian", "little");

  if (!g_key_file_save_to_file (keyfile, self->toolchain_path, &error))
    {
      g_message ("Unable to remove tool: %s", error->message);
      return;
    }
}

static void
meson_toolchain_edition_preferences_row_tool_changed (GbpMesonToolchainEditionPreferencesRow *self,
                                                      gpointer                                user_data)
{
  const gchar *active_id;
  gboolean lang_sensitive;

  g_assert (GBP_IS_MESON_TOOLCHAIN_EDITION_PREFERENCES_ROW (self));
  g_assert (GTK_IS_COMBO_BOX (user_data));

  active_id = gtk_combo_box_get_active_id (GTK_COMBO_BOX(user_data));
  lang_sensitive = g_strcmp0 (active_id, IDE_TOOLCHAIN_TOOL_CC) == 0;
  gtk_widget_set_sensitive (GTK_WIDGET(self->lang_combobox), lang_sensitive);
}

static void
meson_toolchain_edition_preferences_row_clicked (GbpMesonToolchainEditionPreferencesRow *self,
                                                 gpointer                                user_data)
{
  g_assert (GBP_IS_MESON_TOOLCHAIN_EDITION_PREFERENCES_ROW (self));

  gbp_meson_toolchain_edition_preferences_row_show_popup (self);
}

static void
meson_tool_deleted (GbpMesonToolchainEditionPreferencesRow *self,
                    gpointer                                user_data)
{
  GbpMesonToolRow *tool_row = (GbpMesonToolRow *)user_data;
  g_autoptr(GKeyFile) keyfile = g_key_file_new ();
  g_autoptr(GError) error = NULL;

  g_assert (GBP_IS_MESON_TOOLCHAIN_EDITION_PREFERENCES_ROW (self));
  g_assert (GBP_IS_MESON_TOOL_ROW (tool_row));

  if (!g_key_file_load_from_file (keyfile, self->toolchain_path, G_KEY_FILE_KEEP_COMMENTS|G_KEY_FILE_KEEP_TRANSLATIONS, &error))
    {
      g_message ("Unable to load file \"%s\": %s", self->toolchain_path, error->message);
      return;
    }

  if (!g_key_file_remove_key (keyfile, "binaries", gbp_meson_get_tool_binary_name (gbp_meson_tool_row_get_tool_id (tool_row)), &error))
    {
      g_message ("Unable to remove tool: %s", error->message);
      return;
    }

  if (!g_key_file_save_to_file (keyfile, self->toolchain_path, &error))
    {
      g_message ("Unable to remove tool: %s", error->message);
      return;
    }
}

static void
meson_toolchain_edition_preferences_row_add_tool (GbpMesonToolchainEditionPreferencesRow *self,
                                                  gpointer                                user_data)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) tool_file = NULL;
  g_autoptr(GKeyFile) keyfile = g_key_file_new ();
  g_autofree gchar *tool_path = NULL;
  const gchar *tool_id;
  const gchar *lang_id;
  GbpMesonToolRow *tool_row;

  g_assert (GBP_IS_MESON_TOOLCHAIN_EDITION_PREFERENCES_ROW (self));

  tool_id = gtk_combo_box_get_active_id (self->tool_combobox);
  lang_id = gtk_combo_box_get_active_id (self->lang_combobox);
  tool_file = dzl_file_chooser_entry_get_file (self->path_entry);
  tool_path = g_file_get_path (tool_file);

  if (!g_key_file_load_from_file (keyfile, self->toolchain_path, G_KEY_FILE_KEEP_COMMENTS|G_KEY_FILE_KEEP_TRANSLATIONS, &error))
    return;

  if (g_strcmp0 (tool_id, IDE_TOOLCHAIN_TOOL_CC) == 0)
    gbp_meson_key_file_set_string_quoted (keyfile, "binaries", lang_id, tool_path);
  else
    gbp_meson_key_file_set_string_quoted (keyfile, "binaries", gbp_meson_get_tool_binary_name (tool_id), tool_path);

  g_key_file_save_to_file (keyfile, self->toolchain_path, &error);

  tool_row = gbp_meson_tool_row_new (tool_id, tool_path, lang_id);
  gtk_container_add (GTK_CONTAINER (self->tools_listbox), GTK_WIDGET (tool_row));
  g_signal_connect_swapped (tool_row, "tool-removed", G_CALLBACK (meson_tool_deleted), self);
}

static void
meson_toolchain_edition_preferences_row_delete (GbpMesonToolchainEditionPreferencesRow *self,
                                                gpointer                                user_data)
{
  g_autoptr(GFile) file = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (GBP_IS_MESON_TOOLCHAIN_EDITION_PREFERENCES_ROW (self));

  file = g_file_new_for_path (self->toolchain_path);
  if (!g_file_delete (file, NULL, &error))
    {
      g_message ("Error removing \"%s\": %s", self->toolchain_path, error->message);
      return;
    }

  /* The row is wrapped into a GtkListBoxRow that won't be removed when child is destroyed */
  gtk_widget_destroy (gtk_widget_get_parent (GTK_WIDGET (self)));
}

gboolean
gbp_meson_toolchain_edition_preferences_row_load_file (GbpMesonToolchainEditionPreferencesRow  *self,
                                                       const gchar                             *file_path,
                                                       GError                                 **error)
{
  g_autofree gchar *arch = NULL;
  g_autofree gchar *system = NULL;
  g_autoptr(GKeyFile) keyfile = g_key_file_new ();
  g_autoptr(IdeTriplet) triplet = NULL;
  g_autoptr(GError) list_error = NULL;
  g_auto(GStrv) binaries = NULL;

  g_return_val_if_fail (GBP_IS_MESON_TOOLCHAIN_EDITION_PREFERENCES_ROW (self), FALSE);
  g_return_val_if_fail (file_path != NULL, FALSE);
  g_return_val_if_fail (error != NULL && *error == NULL, FALSE);

  g_object_set (G_OBJECT (self), "toolchain-path", file_path, NULL);
  if (!g_key_file_load_from_file (keyfile, self->toolchain_path, G_KEY_FILE_KEEP_COMMENTS|G_KEY_FILE_KEEP_TRANSLATIONS, error))
    return FALSE;

  arch = gbp_meson_key_file_get_string_quoted (keyfile, "host_machine", "cpu_family", error);
  if (arch == NULL)
    return FALSE;

  system = gbp_meson_key_file_get_string_quoted (keyfile, "host_machine", "system", error);
  if (system == NULL)
    return FALSE;

  triplet = ide_triplet_new_with_triplet (arch, system, NULL);
  gtk_entry_set_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (self->arch_combobox))), ide_triplet_get_full_name (triplet));

  binaries = g_key_file_get_keys (keyfile, "binaries", NULL, &list_error);
  if (binaries == NULL)
    return TRUE;

  for (int i = 0; binaries[i] != NULL; i++)
    {
      const gchar *lang = binaries[i];
      const gchar *tool_id;
      g_autoptr(GError) key_error = NULL;
      g_autofree gchar *exec_path = gbp_meson_key_file_get_string_quoted (keyfile, "binaries", lang, &key_error);
      GbpMesonToolRow *tool_row;

      tool_id = gbp_meson_get_tool_id_from_binary (lang);
      if (g_strcmp0 (tool_id, IDE_TOOLCHAIN_TOOL_CC) == 0)
        tool_row = gbp_meson_tool_row_new (tool_id, exec_path, gbp_meson_get_toolchain_language (lang));
      else
        tool_row = gbp_meson_tool_row_new (tool_id, exec_path, IDE_TOOLCHAIN_LANGUAGE_ANY);

      gtk_container_add (GTK_CONTAINER (self->tools_listbox), GTK_WIDGET (tool_row));
      g_signal_connect_swapped (tool_row, "tool-removed", G_CALLBACK (meson_tool_deleted), self);
    }

  return TRUE;
}

static void
gbp_meson_toolchain_edition_preferences_row_get_property (GObject    *object,
                                                          guint       prop_id,
                                                          GValue     *value,
                                                          GParamSpec *pspec)
{
  GbpMesonToolchainEditionPreferencesRow *self = GBP_MESON_TOOLCHAIN_EDITION_PREFERENCES_ROW (object);

  switch (prop_id)
    {
    case PROP_TOOLCHAIN_PATH:
      g_value_set_string (value, self->toolchain_path);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_meson_toolchain_edition_preferences_row_set_property (GObject      *object,
                                                          guint         prop_id,
                                                          const GValue *value,
                                                          GParamSpec   *pspec)
{
  GbpMesonToolchainEditionPreferencesRow *self = GBP_MESON_TOOLCHAIN_EDITION_PREFERENCES_ROW (object);
  g_autofree gchar *user_folder_path = NULL;
  g_autoptr(GFile) user_folder = NULL;
  g_autoptr(GFile) file = NULL;
  g_autofree gchar *row_name = NULL;

  switch (prop_id)
    {
    case PROP_TOOLCHAIN_PATH:
      if (self->toolchain_path != NULL)
        g_clear_pointer (&self->toolchain_path, g_free);

      self->toolchain_path = g_value_dup_string (value);

      user_folder_path = g_build_filename (g_get_user_data_dir (), "meson", "cross", NULL);
      user_folder = g_file_new_for_path (user_folder_path);
      file = g_file_new_for_path (self->toolchain_path);

      row_name = g_file_get_relative_path (user_folder, file);
      gtk_entry_set_text (self->name_entry, row_name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_meson_toolchain_edition_preferences_row_finalize (GObject *object)
{
  GbpMesonToolchainEditionPreferencesRow *self = (GbpMesonToolchainEditionPreferencesRow *) object;

  g_clear_pointer (&self->toolchain_path, g_free);

  G_OBJECT_CLASS (gbp_meson_toolchain_edition_preferences_row_parent_class)->finalize (object);
}

/**
 * gbp_meson_toolchain_edition_preferences_row_show_popup:
 * @self: a #GbpMesonToolchainEditionPreferencesRow
 *
 * Requests the configuration popover the be shown over the widget.
 *
 * Since: 3.32
 */
void
gbp_meson_toolchain_edition_preferences_row_show_popup (GbpMesonToolchainEditionPreferencesRow *self)
{
  g_return_if_fail (GBP_IS_MESON_TOOLCHAIN_EDITION_PREFERENCES_ROW (self));
  g_return_if_fail (GTK_IS_POPOVER (self->popover));

  gtk_popover_popup (self->popover);
  gtk_popover_set_modal (self->popover, TRUE);
}

static void
gbp_meson_toolchain_edition_preferences_row_constructed (GObject *object)
{
  GbpMesonToolchainEditionPreferencesRow *self = (GbpMesonToolchainEditionPreferencesRow *) object;
  GtkWidget *label;

  g_object_bind_property (self->name_entry, "text", self->display_name, "label", 0);

  g_signal_connect_object (self->name_entry,
                           "changed",
                           G_CALLBACK (meson_toolchain_edition_preferences_row_name_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->arch_combobox,
                           "changed",
                           G_CALLBACK (meson_toolchain_edition_preferences_row_arch_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->tool_combobox,
                           "changed",
                           G_CALLBACK (meson_toolchain_edition_preferences_row_tool_changed),
                           self,
                           G_CONNECT_SWAPPED);

  label = g_object_new (GTK_TYPE_LABEL,
                        "label", _("No Provided Tool"),
                        "visible", TRUE,
                        NULL);
  gtk_list_box_set_placeholder (self->tools_listbox, label);

  /*g_signal_connect_object (sysroot_manager,
                           "target-changed",
                           G_CALLBACK (toolchain_edition_preferences_row_target_changed),
                           self,
                           G_CONNECT_SWAPPED);*/
}

static void
gbp_meson_toolchain_edition_preferences_row_class_init (GbpMesonToolchainEditionPreferencesRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gbp_meson_toolchain_edition_preferences_row_finalize;
  object_class->get_property = gbp_meson_toolchain_edition_preferences_row_get_property;
  object_class->set_property = gbp_meson_toolchain_edition_preferences_row_set_property;
  object_class->constructed = gbp_meson_toolchain_edition_preferences_row_constructed;

  properties [PROP_TOOLCHAIN_PATH] =
    g_param_spec_string ("toolchain-path",
                         "Toolchain Path",
                         "The absolute path of the toolchain definition file.",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/meson/gbp-meson-toolchain-edition-preferences-row.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpMesonToolchainEditionPreferencesRow, display_name);
  gtk_widget_class_bind_template_child (widget_class, GbpMesonToolchainEditionPreferencesRow, popover);
  gtk_widget_class_bind_template_child (widget_class, GbpMesonToolchainEditionPreferencesRow, name_entry);
  gtk_widget_class_bind_template_child (widget_class, GbpMesonToolchainEditionPreferencesRow, tools_listbox);
  gtk_widget_class_bind_template_child (widget_class, GbpMesonToolchainEditionPreferencesRow, arch_combobox);
  gtk_widget_class_bind_template_child (widget_class, GbpMesonToolchainEditionPreferencesRow, tool_combobox);
  gtk_widget_class_bind_template_child (widget_class, GbpMesonToolchainEditionPreferencesRow, lang_combobox);
  gtk_widget_class_bind_template_child (widget_class, GbpMesonToolchainEditionPreferencesRow, add_button);
  gtk_widget_class_bind_template_child (widget_class, GbpMesonToolchainEditionPreferencesRow, delete_button);
  gtk_widget_class_bind_template_child (widget_class, GbpMesonToolchainEditionPreferencesRow, path_entry);
}

static void
gbp_meson_toolchain_edition_preferences_row_init (GbpMesonToolchainEditionPreferencesRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect (self, "preference-activated", G_CALLBACK (meson_toolchain_edition_preferences_row_clicked), NULL);
  g_signal_connect_swapped (self->add_button, "clicked", G_CALLBACK (meson_toolchain_edition_preferences_row_add_tool), self);
  g_signal_connect_swapped (self->delete_button, "clicked", G_CALLBACK (meson_toolchain_edition_preferences_row_delete), self);
}
