/* ide-sysroot-preferences-row.c
 *
 * Copyright (C) 2018 Corentin Noël <corentin.noel@collabora.com>
 * Copyright (C) 2018 Collabora Ltd.
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

#include "ide-sysroot-preferences-row.h"
#include "ide-sysroot-manager.h"

struct _IdeSysrootPreferencesRow
{
  DzlPreferencesBin parent_instance;
  gchar *sysroot_id;
  GtkLabel *display_name;
  GtkEntry *name_entry;
  GtkEntry *sysroot_entry;
  GtkEntry *pkg_config_entry;
  GtkButton *delete_button;
  GtkWidget *popover;
};

G_DEFINE_TYPE (IdeSysrootPreferencesRow, ide_sysroot_preferences_row, DZL_TYPE_PREFERENCES_BIN)

enum {
  PROP_0,
  PROP_SYSROOT_ID,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static void
sysroot_preferences_row_name_changed (IdeSysrootPreferencesRow *self, gpointer user_data)
{
  IdeSysrootManager *sysroot_manager = NULL;

  g_assert (IDE_IS_SYSROOT_PREFERENCES_ROW (self));
  g_assert (GTK_IS_ENTRY (user_data));

  sysroot_manager = ide_sysroot_manager_get_default ();
  ide_sysroot_manager_set_target_name (sysroot_manager, self->sysroot_id, gtk_entry_get_text (GTK_ENTRY (user_data)));
}

static void
sysroot_preferences_row_sysroot_changed (IdeSysrootPreferencesRow *self, gpointer user_data)
{
  IdeSysrootManager *sysroot_manager = NULL;

  g_assert (IDE_IS_SYSROOT_PREFERENCES_ROW (self));
  g_assert (GTK_IS_ENTRY (user_data));

  sysroot_manager = ide_sysroot_manager_get_default ();
  ide_sysroot_manager_set_target_path (sysroot_manager, self->sysroot_id, gtk_entry_get_text (GTK_ENTRY (user_data)));
}

static void
sysroot_preferences_row_pkg_config_changed (IdeSysrootPreferencesRow *self, gpointer user_data)
{
  IdeSysrootManager *sysroot_manager = NULL;

  g_assert (IDE_IS_SYSROOT_PREFERENCES_ROW (self));
  g_assert (GTK_IS_ENTRY (user_data));

  sysroot_manager = ide_sysroot_manager_get_default ();
  ide_sysroot_manager_set_target_pkg_config_path (sysroot_manager, self->sysroot_id, gtk_entry_get_text (GTK_ENTRY (user_data)));
}

static void
sysroot_preferences_row_clicked (IdeSysrootPreferencesRow *self, gpointer user_data)
{
  ide_sysroot_preferences_row_show_popup (self);
}

static void
sysroot_preferences_delete (IdeSysrootPreferencesRow *self, gpointer user_data)
{
  IdeSysrootManager *sysroot_manager = NULL;

  g_assert (IDE_IS_SYSROOT_PREFERENCES_ROW (self));

  sysroot_manager = ide_sysroot_manager_get_default ();
  ide_sysroot_manager_remove_target (sysroot_manager, self->sysroot_id);

  // The row is wrapped into a GtkListBoxRow that won't be removed when child is destroyed
  gtk_widget_destroy (gtk_widget_get_parent (GTK_WIDGET (self)));
}

static void
ide_sysroot_preferences_row_get_property (GObject    *object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
  IdeSysrootPreferencesRow *self = IDE_SYSROOT_PREFERENCES_ROW (object);

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
ide_sysroot_preferences_row_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  IdeSysrootPreferencesRow *self = IDE_SYSROOT_PREFERENCES_ROW (object);

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
ide_sysroot_preferences_row_finalize (GObject *object)
{
  IdeSysrootPreferencesRow *self = IDE_SYSROOT_PREFERENCES_ROW (object);

  g_clear_pointer (&self->sysroot_id, g_free);

  G_OBJECT_CLASS (ide_sysroot_preferences_row_parent_class)->finalize (object);
}

void
ide_sysroot_preferences_row_show_popup (IdeSysrootPreferencesRow *self)
{
  gtk_popover_popup (GTK_POPOVER (self->popover));
  gtk_popover_set_modal (GTK_POPOVER (self->popover), TRUE);
}

static GObject *
ide_sysroot_preferences_row_constructor (GType type,
                                         guint n_construct_properties,
                                         GObjectConstructParam * construct_properties)
{
  IdeSysrootManager *sysroot_manager = NULL;
  gchar *value;
  GObject * obj = G_OBJECT_CLASS (ide_sysroot_preferences_row_parent_class)->constructor (type, n_construct_properties, construct_properties);
  IdeSysrootPreferencesRow *self = IDE_SYSROOT_PREFERENCES_ROW (obj);

  sysroot_manager = ide_sysroot_manager_get_default ();
  gtk_entry_set_text (self->name_entry, ide_sysroot_manager_get_target_name (sysroot_manager, self->sysroot_id));
  gtk_entry_set_text (self->sysroot_entry, ide_sysroot_manager_get_target_path (sysroot_manager, self->sysroot_id));
  value = ide_sysroot_manager_get_target_pkg_config_path (sysroot_manager, self->sysroot_id);
  if (value != NULL)
    gtk_entry_set_text (self->pkg_config_entry, value);

  g_signal_connect_object (self->name_entry,
                           "changed",
                           G_CALLBACK (sysroot_preferences_row_name_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->sysroot_entry,
                           "changed",
                           G_CALLBACK (sysroot_preferences_row_sysroot_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->pkg_config_entry,
                           "changed",
                           G_CALLBACK (sysroot_preferences_row_pkg_config_changed),
                           self,
                           G_CONNECT_SWAPPED);
  return obj;
}

static void
ide_sysroot_preferences_row_class_init (IdeSysrootPreferencesRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ide_sysroot_preferences_row_finalize;
  object_class->get_property = ide_sysroot_preferences_row_get_property;
  object_class->set_property = ide_sysroot_preferences_row_set_property;
  object_class->constructor = ide_sysroot_preferences_row_constructor;

  properties [PROP_SYSROOT_ID] =
    g_param_spec_string ("sysroot-id",
                         "Sysroot ID",
                         "Internal id of the sysroot",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/sysroot-plugin/ide-sysroot-preferences-row.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeSysrootPreferencesRow, display_name);
  gtk_widget_class_bind_template_child (widget_class, IdeSysrootPreferencesRow, popover);
  gtk_widget_class_bind_template_child (widget_class, IdeSysrootPreferencesRow, name_entry);
  gtk_widget_class_bind_template_child (widget_class, IdeSysrootPreferencesRow, sysroot_entry);
  gtk_widget_class_bind_template_child (widget_class, IdeSysrootPreferencesRow, pkg_config_entry);
  gtk_widget_class_bind_template_child (widget_class, IdeSysrootPreferencesRow, delete_button);
}

static void
ide_sysroot_preferences_row_init (IdeSysrootPreferencesRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect (self, "preference-activated", G_CALLBACK(sysroot_preferences_row_clicked), NULL);
  g_signal_connect_swapped (self->delete_button, "clicked", G_CALLBACK(sysroot_preferences_delete), self);
  g_object_bind_property (self->name_entry, "text", self->display_name, "label", 0);
}
