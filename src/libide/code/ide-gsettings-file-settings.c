/* ide-gsettings-file-settings.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-gsettings-file-settings"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-core.h>

#include "ide-code-enums.h"
#include "ide-gsettings-file-settings.h"
#include "ide-language-defaults.h"

struct _IdeGsettingsFileSettings
{
  IdeFileSettings  parent_instance;
  IdeSettings     *language_settings;
};

typedef struct
{
  const gchar             *key;
  const gchar             *property;
  GSettingsBindGetMapping  get_mapping;
} SettingsMapping;

G_DEFINE_FINAL_TYPE (IdeGsettingsFileSettings, ide_gsettings_file_settings, IDE_TYPE_FILE_SETTINGS)

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

static gboolean
spaces_style_get (GValue   *value,
                  GVariant *variant,
                  gpointer  user_data)
{
  g_autofree const gchar **strv = g_variant_get_strv (variant, NULL);
  GFlagsClass *klass, *unref_class = NULL;
  guint flags = 0;

  if (!(klass = g_type_class_peek (IDE_TYPE_SPACES_STYLE)))
    klass = unref_class = g_type_class_ref (IDE_TYPE_SPACES_STYLE);

  for (guint i = 0; strv[i] != NULL; i++)
    {
      GFlagsValue *val = g_flags_get_value_by_nick (klass, strv[i]);

      if (val == NULL)
        {
          g_warning ("No such nick %s", strv[i]);
          continue;
        }

      flags |= val->value;
    }

  g_value_set_flags (value, flags);

  if (unref_class != NULL)
    g_type_class_unref (unref_class);

  return TRUE;
}

static SettingsMapping language_mappings [] = {
  { "auto-indent",                   "auto-indent",              NULL             },
  { "indent-width",                  "indent-width",             NULL             },
  { "insert-spaces-instead-of-tabs", "indent-style",             indent_style_get },
  { "right-margin-position",         "right-margin-position",    NULL             },
  { "show-right-margin",             "show-right-margin",        NULL             },
  { "tab-width",                     "tab-width",                NULL             },
  { "trim-trailing-whitespace",      "trim-trailing-whitespace", NULL             },
  { "insert-matching-brace",         "insert-matching-brace",    NULL             },
  { "insert-trailing-newline",       "insert-trailing-newline",  NULL             },
  { "overwrite-braces",              "overwrite-braces",         NULL             },
  { "spaces-style",                  "spaces-style",             spaces_style_get },
};

static void
ide_gsettings_file_settings_apply (IdeGsettingsFileSettings *self)
{
  g_autofree char *project_id = NULL;
  const char *lang_id;
  IdeContext *context;

  g_assert (IDE_IS_GSETTINGS_FILE_SETTINGS (self));

  g_clear_object (&self->language_settings);

  if (!(lang_id = ide_file_settings_get_language (IDE_FILE_SETTINGS (self))))
    lang_id = "plain-text";

  g_assert (lang_id != NULL);

  context = ide_object_get_context (IDE_OBJECT (self));
  project_id = ide_context_dup_project_id (context);
  self->language_settings = ide_settings_new_relocatable_with_suffix (project_id,
                                                                      "org.gnome.builder.editor.language",
                                                                      lang_id);

  for (guint i = 0; i < G_N_ELEMENTS (language_mappings); i++)
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
ide_gsettings_file_settings_parent_set (IdeObject *object,
                                        IdeObject *parent)
{
  IdeGsettingsFileSettings *self = (IdeGsettingsFileSettings *)object;

  IDE_ENTRY;

  g_assert (IDE_IS_GSETTINGS_FILE_SETTINGS (self));
  g_assert (!parent || IDE_IS_OBJECT (parent));

  if (parent != NULL)
    ide_gsettings_file_settings_apply (self);

  IDE_EXIT;
}

static void
ide_gsettings_file_settings_destroy (IdeObject *object)
{
  IdeGsettingsFileSettings *self = (IdeGsettingsFileSettings *)object;

  g_clear_object (&self->language_settings);

  IDE_OBJECT_CLASS (ide_gsettings_file_settings_parent_class)->destroy (object);
}

static void
ide_gsettings_file_settings_class_init (IdeGsettingsFileSettingsClass *klass)
{
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  i_object_class->parent_set = ide_gsettings_file_settings_parent_set;
  i_object_class->destroy = ide_gsettings_file_settings_destroy;
}

static void
ide_gsettings_file_settings_init (IdeGsettingsFileSettings *self)
{
}
