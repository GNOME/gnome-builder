/* gbp-sysroot-preferences-row.c
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

#define G_LOG_DOMAIN "gbp-sysroot-preferences-row"

#include "gbp-sysroot-preferences-row.h"
#include "gbp-sysroot-manager.h"

struct _GbpSysrootPreferencesRow
{
  DzlPreferencesBin parent_instance;
  gchar *sysroot_id;
  GtkLabel *display_name;
  GtkEntry *name_entry;
  DzlFileChooserEntry *sysroot_entry;
  GtkEntry *pkg_config_entry;
  GtkComboBox *arch_combobox;
  GtkButton *delete_button;
  GtkWidget *popover;
};

G_DEFINE_TYPE (GbpSysrootPreferencesRow, gbp_sysroot_preferences_row, DZL_TYPE_PREFERENCES_BIN)

enum {
  PROP_0,
  PROP_SYSROOT_ID,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
sysroot_preferences_row_name_changed (GbpSysrootPreferencesRow *self,
                                      gpointer                  user_data)
{
  GbpSysrootManager *sysroot_manager = NULL;

  g_assert (GBP_IS_SYSROOT_PREFERENCES_ROW (self));
  g_assert (GTK_IS_ENTRY (user_data));

  sysroot_manager = gbp_sysroot_manager_get_default ();
  gbp_sysroot_manager_set_target_name (sysroot_manager,
                                       self->sysroot_id,
                                       gtk_entry_get_text (GTK_ENTRY (user_data)));
}

static void
sysroot_preferences_row_sysroot_changed (GbpSysrootPreferencesRow *self,
                                         GParamSpec               *pspec,
                                         gpointer                  user_data)
{
  GbpSysrootManager *sysroot_manager = NULL;
  g_autofree gchar *sysroot_path = NULL;
  GFile *file;

  g_assert (GBP_IS_SYSROOT_PREFERENCES_ROW (self));
  g_assert (DZL_IS_FILE_CHOOSER_ENTRY (user_data));

  sysroot_manager = gbp_sysroot_manager_get_default ();
  file = dzl_file_chooser_entry_get_file (DZL_FILE_CHOOSER_ENTRY (user_data));
  sysroot_path = g_file_get_path (file);
  gbp_sysroot_manager_set_target_path (sysroot_manager,
                                       self->sysroot_id,
                                       sysroot_path);
}

static void
sysroot_preferences_row_arch_changed (GbpSysrootPreferencesRow *self,
                                      gpointer                  user_data)
{
  GbpSysrootManager *sysroot_manager = NULL;

  g_assert (GBP_IS_SYSROOT_PREFERENCES_ROW (self));
  g_assert (GTK_IS_COMBO_BOX (user_data));

  sysroot_manager = gbp_sysroot_manager_get_default ();
  gbp_sysroot_manager_set_target_arch (sysroot_manager,
                                       self->sysroot_id,
                                       gtk_combo_box_get_active_id (GTK_COMBO_BOX (user_data)));
}

static void
sysroot_preferences_row_pkg_config_changed (GbpSysrootPreferencesRow *self,
                                            gpointer                  user_data)
{
  GbpSysrootManager *sysroot_manager = NULL;

  g_assert (GBP_IS_SYSROOT_PREFERENCES_ROW (self));
  g_assert (GTK_IS_ENTRY (user_data));

  sysroot_manager = gbp_sysroot_manager_get_default ();
  gbp_sysroot_manager_set_target_pkg_config_path (sysroot_manager,
                                                  self->sysroot_id,
                                                  gtk_entry_get_text (GTK_ENTRY (user_data)));
}

static void
sysroot_preferences_row_target_changed (GbpSysrootPreferencesRow                *self,
                                        const gchar                             *target,
                                        GbpSysrootManagerTargetModificationType  mod_type,
                                        gpointer                                 user_data)
{
  GbpSysrootManager *sysroot_manager = NULL;
  g_autofree gchar *value = NULL;

  g_assert (GBP_IS_SYSROOT_PREFERENCES_ROW (self));
  g_assert (GBP_IS_SYSROOT_MANAGER (user_data));

  if (mod_type != GBP_SYSROOT_MANAGER_TARGET_CHANGED)
    return;

  if (g_strcmp0 (target, self->sysroot_id) != 0)
    return;

  sysroot_manager = GBP_SYSROOT_MANAGER (user_data);
  value = gbp_sysroot_manager_get_target_pkg_config_path (sysroot_manager,
                                                          self->sysroot_id);
  if (value != NULL)
    gtk_entry_set_text (self->pkg_config_entry, value);
}

static void
sysroot_preferences_row_clicked (GbpSysrootPreferencesRow *self,
                                 gpointer                  user_data)
{
  g_assert (GBP_IS_SYSROOT_PREFERENCES_ROW (self));

  gbp_sysroot_preferences_row_show_popup (self);
}

static void
sysroot_preferences_delete (GbpSysrootPreferencesRow *self,
                            gpointer                  user_data)
{
  GbpSysrootManager *sysroot_manager = NULL;

  g_assert (GBP_IS_SYSROOT_PREFERENCES_ROW (self));

  sysroot_manager = gbp_sysroot_manager_get_default ();
  gbp_sysroot_manager_remove_target (sysroot_manager, self->sysroot_id);

  /* The row is wrapped into a GtkListBoxRow that won't be removed when child is destroyed */
  gtk_widget_destroy (gtk_widget_get_parent (GTK_WIDGET (self)));
}

static void
gbp_sysroot_preferences_row_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  GbpSysrootPreferencesRow *self = GBP_SYSROOT_PREFERENCES_ROW (object);

  switch (prop_id)
    {
    case PROP_SYSROOT_ID:
      g_value_set_string (value, self->sysroot_id);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_sysroot_preferences_row_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  GbpSysrootPreferencesRow *self = GBP_SYSROOT_PREFERENCES_ROW (object);

  switch (prop_id)
    {
    case PROP_SYSROOT_ID:
      self->sysroot_id = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_sysroot_preferences_row_finalize (GObject *object)
{
  GbpSysrootPreferencesRow *self = (GbpSysrootPreferencesRow *) object;

  g_clear_pointer (&self->sysroot_id, g_free);

  G_OBJECT_CLASS (gbp_sysroot_preferences_row_parent_class)->finalize (object);
}

/**
 * gbp_sysroot_preferences_row_show_popup:
 * @self: a #GbpSysrootPreferencesRow
 *
 * Requests the configuration popover the be shown over the widget.
 */
void
gbp_sysroot_preferences_row_show_popup (GbpSysrootPreferencesRow *self)
{
  g_return_if_fail (GBP_IS_SYSROOT_PREFERENCES_ROW (self));
  g_return_if_fail (GTK_IS_POPOVER (self->popover));

  gtk_popover_popup (GTK_POPOVER (self->popover));
  gtk_popover_set_modal (GTK_POPOVER (self->popover), TRUE);
}

static void
gbp_sysroot_preferences_row_constructed (GObject *object)
{
  GbpSysrootPreferencesRow *self = (GbpSysrootPreferencesRow *)object;
  GbpSysrootManager *sysroot_manager;
  g_autoptr(GFile) file = NULL;
  g_autofree gchar *value = NULL;
  g_autofree gchar *path = NULL;
  g_autofree gchar *name = NULL;

  g_assert (GBP_IS_SYSROOT_PREFERENCES_ROW (self));

  sysroot_manager = gbp_sysroot_manager_get_default ();

  name = gbp_sysroot_manager_get_target_name (sysroot_manager, self->sysroot_id);
  path = gbp_sysroot_manager_get_target_path (sysroot_manager, self->sysroot_id);

  gtk_entry_set_text (self->name_entry, name ?: "");
  gtk_combo_box_set_active_id (self->arch_combobox, gbp_sysroot_manager_get_target_arch (sysroot_manager, self->sysroot_id));

  if (path)
    {
      file = g_file_new_for_path (path);
      dzl_file_chooser_entry_set_file (self->sysroot_entry, file);
    }

  value = gbp_sysroot_manager_get_target_pkg_config_path (sysroot_manager, self->sysroot_id);
  gtk_entry_set_text (self->pkg_config_entry, value ?: "");

  g_signal_connect_object (self->name_entry,
                           "changed",
                           G_CALLBACK (sysroot_preferences_row_name_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->arch_combobox,
                           "changed",
                           G_CALLBACK (sysroot_preferences_row_arch_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->sysroot_entry,
                           "notify::file",
                           G_CALLBACK (sysroot_preferences_row_sysroot_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->pkg_config_entry,
                           "changed",
                           G_CALLBACK (sysroot_preferences_row_pkg_config_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (sysroot_manager,
                           "target-changed",
                           G_CALLBACK (sysroot_preferences_row_target_changed),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
gbp_sysroot_preferences_row_class_init (GbpSysrootPreferencesRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gbp_sysroot_preferences_row_finalize;
  object_class->get_property = gbp_sysroot_preferences_row_get_property;
  object_class->set_property = gbp_sysroot_preferences_row_set_property;
  object_class->constructed = gbp_sysroot_preferences_row_constructed;

  properties [PROP_SYSROOT_ID] =
    g_param_spec_string ("sysroot-id",
                         "Sysroot ID",
                         "Internal id of the sysroot",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/sysroot/gbp-sysroot-preferences-row.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpSysrootPreferencesRow, display_name);
  gtk_widget_class_bind_template_child (widget_class, GbpSysrootPreferencesRow, popover);
  gtk_widget_class_bind_template_child (widget_class, GbpSysrootPreferencesRow, name_entry);
  gtk_widget_class_bind_template_child (widget_class, GbpSysrootPreferencesRow, arch_combobox);
  gtk_widget_class_bind_template_child (widget_class, GbpSysrootPreferencesRow, sysroot_entry);
  gtk_widget_class_bind_template_child (widget_class, GbpSysrootPreferencesRow, pkg_config_entry);
  gtk_widget_class_bind_template_child (widget_class, GbpSysrootPreferencesRow, delete_button);
}

static void
gbp_sysroot_preferences_row_init (GbpSysrootPreferencesRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect (self, "preference-activated", G_CALLBACK (sysroot_preferences_row_clicked), NULL);
  g_signal_connect_swapped (self->delete_button, "clicked", G_CALLBACK (sysroot_preferences_delete), self);
  g_object_bind_property (self->name_entry, "text", self->display_name, "label", 0);
}
