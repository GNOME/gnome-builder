/* ide-gsettings-file-settings.c
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-gsettings-file-settings"

#include <dazzle.h>
#include <glib/gi18n.h>

#include "ide-context.h"
#include "ide-debug.h"

#include "files/ide-file.h"
#include "gsettings/ide-gsettings-file-settings.h"
#include "gsettings/ide-language-defaults.h"
#include "util/ide-settings.h"

struct _IdeGsettingsFileSettings
{
  IdeFileSettings  parent_instance;

  IdeSettings     *language_settings;
  DzlSignalGroup  *signal_group;
};

typedef struct
{
  const gchar             *key;
  const gchar             *property;
  GSettingsBindGetMapping  get_mapping;
} SettingsMapping;

G_DEFINE_TYPE (IdeGsettingsFileSettings, ide_gsettings_file_settings, IDE_TYPE_FILE_SETTINGS)

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

static SettingsMapping language_mappings [] = {
  { "indent-width",                  "indent-width",             NULL             },
  { "insert-spaces-instead-of-tabs", "indent-style",             indent_style_get },
  { "right-margin-position",         "right-margin-position",    NULL             },
  { "show-right-margin",             "show-right-margin",        NULL             },
  { "tab-width",                     "tab-width",                NULL             },
  { "trim-trailing-whitespace",      "trim-trailing-whitespace", NULL             },
  { "insert-matching-brace",         "insert-matching-brace",    NULL             },
  { "overwrite-braces",              "overwrite-braces",         NULL             },
};

static void
file_notify_language_cb (IdeGsettingsFileSettings *self,
                         GParamSpec               *pspec,
                         IdeFile                  *file)
{
  g_autofree gchar *relative_path = NULL;
  GtkSourceLanguage *language;
  const gchar *lang_id;
  IdeContext *context;
  gsize i;

  g_assert (IDE_IS_GSETTINGS_FILE_SETTINGS (self));
  g_assert (IDE_IS_FILE (file));

  g_clear_object (&self->language_settings);

  language = ide_file_get_language (file);

  if (language == NULL)
    lang_id = "plain-text";
  else
    lang_id = gtk_source_language_get_id (language);

  g_assert (lang_id != NULL);

  context = ide_object_get_context (IDE_OBJECT (self));
  relative_path = g_strdup_printf ("/editor/language/%s/", lang_id);
  self->language_settings = ide_context_get_settings (context,
                                                      "org.gnome.builder.editor.language",
                                                      relative_path);

  for (i = 0; i < G_N_ELEMENTS (language_mappings); i++)
    {
      SettingsMapping *mapping = &language_mappings [i];

      ide_settings_bind_with_mapping (self->language_settings,
                                      mapping->key,
                                      self,
                                      mapping->property,
                                      G_SETTINGS_BIND_GET,
                                      mapping->get_mapping,
                                      NULL,
                                      NULL,
                                      NULL);
    }
}

static void
ide_gsettings_file_settings_constructed (GObject *object)
{
  IdeGsettingsFileSettings *self = (IdeGsettingsFileSettings *)object;
  IdeFile *file;

  IDE_ENTRY;

  G_OBJECT_CLASS (ide_gsettings_file_settings_parent_class)->constructed (object);

  file = ide_file_settings_get_file (IDE_FILE_SETTINGS (self));
  if (file == NULL)
    IDE_EXIT;

  dzl_signal_group_set_target (self->signal_group, file);
  file_notify_language_cb (self, NULL, file);

  IDE_EXIT;
}

static void
ide_gsettings_file_settings_dispose (GObject *object)
{
  IdeGsettingsFileSettings *self = (IdeGsettingsFileSettings *)object;

  g_clear_object (&self->signal_group);
  g_clear_object (&self->language_settings);

  G_OBJECT_CLASS (ide_gsettings_file_settings_parent_class)->dispose (object);
}

static void
ide_gsettings_file_settings_class_init (IdeGsettingsFileSettingsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_gsettings_file_settings_constructed;
  object_class->dispose = ide_gsettings_file_settings_dispose;
}

static void
ide_gsettings_file_settings_init (IdeGsettingsFileSettings *self)
{
  self->signal_group = dzl_signal_group_new (IDE_TYPE_FILE);
  dzl_signal_group_connect_object (self->signal_group,
                                   "notify::language",
                                   G_CALLBACK (file_notify_language_cb),
                                   self,
                                   G_CONNECT_SWAPPED);
}
