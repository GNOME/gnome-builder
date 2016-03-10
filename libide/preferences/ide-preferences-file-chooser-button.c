/* ide-preferences-file-chooser-button.c
 *
 * Copyright (C) 2016 Akshaya Kakkilaya <akshaya.kakkilaya@gmail.com>
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

#include "ide-macros.h"
#include "ide-preferences-file-chooser-button.h"

struct _IdePreferencesFileChooserButton
{
  IdePreferencesBin    parent_instance;

  gchar                *key;
  GSettings            *settings;

  GtkFileChooserButton *widget;
  GtkLabel             *title;
  GtkLabel             *subtitle;
};

G_DEFINE_TYPE (IdePreferencesFileChooserButton, ide_preferences_file_chooser_button, IDE_TYPE_PREFERENCES_BIN)

enum {
  PROP_0,
  PROP_ACTION,
  PROP_KEY,
  PROP_SUBTITLE,
  PROP_TITLE,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static void
ide_preferences_file_chooser_button_save_folder (IdePreferencesFileChooserButton *self,
                                                 GtkFileChooserButton            *widget)
{
  g_autoptr(GFile) home = NULL;
  g_autoptr(GFile) folder = NULL;
  g_autofree gchar *path = NULL;

  g_assert (IDE_IS_PREFERENCES_FILE_CHOOSER_BUTTON (self));
  g_assert (GTK_IS_FILE_CHOOSER_BUTTON (widget));

  home = g_file_new_for_path (g_get_home_dir ());
  folder = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (widget));
  path = g_file_get_relative_path (home, folder);

  g_settings_set_string (self->settings, self->key, path);

}

static void
ide_preferences_file_chooser_button_connect (IdePreferencesBin *bin,
                                             GSettings         *settings)
{
  IdePreferencesFileChooserButton *self = (IdePreferencesFileChooserButton *)bin;
  g_autofree gchar *folder = NULL;
  g_autofree gchar *path = NULL;

  g_assert (IDE_IS_PREFERENCES_FILE_CHOOSER_BUTTON (self));
  g_assert (G_IS_SETTINGS (settings));

  self->settings = g_object_ref (settings);

  folder = g_settings_get_string (settings, self->key);

  if (!ide_str_empty0 (folder))
    {
      path = g_build_filename (g_get_home_dir (), folder, NULL);
      gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (self->widget), path);
    }

  g_signal_connect_object (self->widget,
                           "file-set",
                           G_CALLBACK (ide_preferences_file_chooser_button_save_folder),
                           self,
                           G_CONNECT_SWAPPED);
}

static gboolean
ide_preferences_file_chooser_button_matches (IdePreferencesBin *bin,
                                             IdePatternSpec    *spec)
{
  IdePreferencesFileChooserButton *self = (IdePreferencesFileChooserButton *)bin;
  const gchar *tmp;

  g_assert (IDE_IS_PREFERENCES_FILE_CHOOSER_BUTTON (self));
  g_assert (spec != NULL);

  tmp = gtk_label_get_label (self->title);
  if (tmp && ide_pattern_spec_match (spec, tmp))
    return TRUE;

  tmp = gtk_label_get_label (self->subtitle);
  if (tmp && ide_pattern_spec_match (spec, tmp))
    return TRUE;

  if (self->key && ide_pattern_spec_match (spec, self->key))
    return TRUE;

  return FALSE;
}

static void
ide_preferences_file_chooser_button_finalize (GObject *object)
{
  IdePreferencesFileChooserButton *self = (IdePreferencesFileChooserButton *)object;

  g_clear_pointer (&self->key, g_free);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (ide_preferences_file_chooser_button_parent_class)->finalize (object);
}

static void
ide_preferences_file_chooser_button_get_property (GObject    *object,
                                                  guint       prop_id,
                                                  GValue     *value,
                                                  GParamSpec *pspec)
{
  IdePreferencesFileChooserButton *self = IDE_PREFERENCES_FILE_CHOOSER_BUTTON (object);

  switch (prop_id)
    {
    case PROP_ACTION:
      g_value_set_enum (value, gtk_file_chooser_get_action (GTK_FILE_CHOOSER (self->widget)));
      break;

    case PROP_KEY:
      g_value_set_string (value, self->key);
      break;

    case PROP_TITLE:
      g_value_set_string (value, gtk_label_get_label (self->title));
      break;

    case PROP_SUBTITLE:
      g_value_set_string (value, gtk_label_get_label (self->title));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_preferences_file_chooser_button_set_property (GObject      *object,
                                                  guint         prop_id,
                                                  const GValue *value,
                                                  GParamSpec   *pspec)
{
  IdePreferencesFileChooserButton *self = IDE_PREFERENCES_FILE_CHOOSER_BUTTON (object);

  switch (prop_id)
    {
    case PROP_ACTION:
      gtk_file_chooser_set_action (GTK_FILE_CHOOSER (self->widget), g_value_get_enum (value));
      break;

    case PROP_KEY:
      self->key = g_value_dup_string (value);
      break;

    case PROP_TITLE:
      gtk_label_set_label (self->title, g_value_get_string (value));
      break;

    case PROP_SUBTITLE:
      gtk_label_set_label (self->subtitle, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_preferences_file_chooser_button_class_init (IdePreferencesFileChooserButtonClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  IdePreferencesBinClass *bin_class = IDE_PREFERENCES_BIN_CLASS (klass);

  object_class->finalize = ide_preferences_file_chooser_button_finalize;
  object_class->get_property = ide_preferences_file_chooser_button_get_property;
  object_class->set_property = ide_preferences_file_chooser_button_set_property;

  bin_class->connect = ide_preferences_file_chooser_button_connect;
  bin_class->matches = ide_preferences_file_chooser_button_matches;

  properties [PROP_ACTION] =
    g_param_spec_enum ("action",
                       "Action",
                       "Action",
                       GTK_TYPE_FILE_CHOOSER_ACTION,
                       GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                       (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_KEY] =
    g_param_spec_string ("key",
                         "Key",
                         "Key",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "Title",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_SUBTITLE] =
    g_param_spec_string ("subtitle",
                         "Subtitle",
                         "Subtitle",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-preferences-file-chooser-button.ui");
  gtk_widget_class_bind_template_child (widget_class, IdePreferencesFileChooserButton, widget);
  gtk_widget_class_bind_template_child (widget_class, IdePreferencesFileChooserButton, title);
  gtk_widget_class_bind_template_child (widget_class, IdePreferencesFileChooserButton, subtitle);
}

static void
ide_preferences_file_chooser_button_init (IdePreferencesFileChooserButton *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
