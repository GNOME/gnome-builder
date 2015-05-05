/* ide-gsettings-file-settings.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include "ide-context.h"
#include "ide-file.h"
#include "ide-gsettings-file-settings.h"
#include "ide-language.h"
#include "ide-language-defaults.h"

struct _IdeGsettingsFileSettings
{
  IdeFileSettings  parent_instance;
  GSettings       *settings;
};

typedef struct
{
  const gchar             *source_property;
  const gchar             *target_property;
  GSettingsBindGetMapping  mapping;
} SettingsMapping;

G_DEFINE_TYPE (IdeGsettingsFileSettings, ide_gsettings_file_settings, IDE_TYPE_FILE_SETTINGS)

static gboolean indent_style_get (GValue   *value,
                                  GVariant *variant,
                                  gpointer  user_data);

static GSettings *gEditorSettings;
static SettingsMapping gMappings[] = {
  { "indent-width", "indent-width" },
  { "insert-spaces-instead-of-tabs", "indent-style", indent_style_get },
  { "right-margin-position", "right-margin-position" },
  { "show-right-margin", "show-right-margin" },
  { "tab-width", "tab-width" },
  { "trim-trailing-whitespace", "trim-trailing-whitespace" },
};

static gboolean
indent_style_get (GValue   *value,
                  GVariant *variant,
                  gpointer  user_data)
{
  if (g_variant_get_boolean (variant))
    g_value_set_enum (value, IDE_INDENT_STYLE_SPACES);
  else
    g_value_set_enum (value, IDE_INDENT_STYLE_TABS);
  return TRUE;
}

static const gchar *
get_mapped_name (const gchar *name)
{
  gsize i;

  g_assert (name != NULL);

  for (i = 0; gMappings [i].source_property; i++)
    {
      if (ide_str_equal0 (name, gMappings [i].source_property))
        return gMappings [i].target_property;
    }

  g_assert_not_reached ();

  return NULL;
}

static void
ide_gsettings_file_settings_changed (IdeGsettingsFileSettings *self,
                                     const gchar              *key,
                                     GSettings                *settings)
{
  g_autoptr(GVariant) value = NULL;
  g_autofree gchar *set_name = NULL;
  const gchar *mapped;

  g_assert (IDE_IS_GSETTINGS_FILE_SETTINGS (self));
  g_assert (key != NULL);
  g_assert (G_IS_SETTINGS (settings));

  mapped = get_mapped_name (key);
  if (mapped == NULL)
    return;

  set_name = g_strdup_printf ("%s-set", mapped);
  value = g_settings_get_user_value (settings, key);
  g_object_set (self, set_name, !!value, NULL);
}

static void
ide_gsettings_file_settings_bind (IdeGsettingsFileSettings *self,
                                  GSettings                *settings,
                                  const gchar              *source_name,
                                  const gchar              *target_name,
                                  GSettingsBindGetMapping   get_mapping)
{
  g_autofree gchar *set_name = NULL;
  g_autofree gchar *changed_name = NULL;
  g_autoptr(GVariant) value = NULL;

  g_assert (IDE_IS_GSETTINGS_FILE_SETTINGS (self));
  g_assert (G_IS_SETTINGS (settings));
  g_assert (source_name != NULL);

  g_settings_bind_with_mapping (settings, source_name, self, target_name, G_SETTINGS_BIND_GET,
                                get_mapping, NULL, NULL, NULL);

  value = g_settings_get_user_value (settings, source_name);
  set_name = g_strdup_printf ("%s-set", target_name);
  g_object_set (self, set_name, !!value, NULL);

  changed_name = g_strdup_printf ("changed::%s", source_name);

  g_signal_connect_object (settings,
                           changed_name,
                           G_CALLBACK (ide_gsettings_file_settings_changed),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
ide_gsettings_file_settings_connect (IdeGsettingsFileSettings *self,
                                     GSettings                *settings)
{
  gsize i;

  g_assert (IDE_IS_GSETTINGS_FILE_SETTINGS (self));
  g_assert (G_IS_SETTINGS (settings));

  for (i = 0; gMappings [i].source_property != NULL; i++)
    {
      ide_gsettings_file_settings_bind (self,
                                        settings,
                                        gMappings [i].source_property,
                                        gMappings [i].target_property,
                                        gMappings [i].mapping);
    }
}

static void
ide_gsettings_file_settings_constructed (GObject *object)
{
  IdeGsettingsFileSettings *self = (IdeGsettingsFileSettings *)object;
  g_autoptr(GSettings) settings = NULL;
  g_autofree gchar *path = NULL;
  const gchar *lang_id;
  IdeLanguage *language;
  IdeFile *file;

  G_OBJECT_CLASS (ide_gsettings_file_settings_parent_class)->constructed (object);

  file = ide_file_settings_get_file (IDE_FILE_SETTINGS (self));
  language = ide_file_get_language (file);
  lang_id = ide_language_get_id (language);

  path = g_strdup_printf ("/org/gnome/builder/editor/language/%s/", lang_id);
  settings = g_settings_new_with_path ("org.gnome.builder.editor.language", path);

  ide_gsettings_file_settings_connect (self, settings);
}

static void
ide_gsettings_file_settings_finalize (GObject *object)
{
  IdeGsettingsFileSettings *self = (IdeGsettingsFileSettings *)object;

  g_clear_object (&self->settings);

  G_OBJECT_CLASS (ide_gsettings_file_settings_parent_class)->finalize (object);
}

static void
ide_gsettings_file_settings_class_init (IdeGsettingsFileSettingsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_gsettings_file_settings_constructed;
  object_class->finalize = ide_gsettings_file_settings_finalize;

  gEditorSettings = g_settings_new ("org.gnome.builder.editor");
}

static void
ide_gsettings_file_settings_init (IdeGsettingsFileSettings *self)
{
}
