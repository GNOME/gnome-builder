/* ide-gsettings-file-settings.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>

#include "ide-context.h"
#include "ide-file.h"
#include "ide-gsettings-file-settings.h"
#include "ide-language.h"

struct _IdeGsettingsFileSettings
{
  IdeFileSettings parent_instance;

  GSettings *settings;
};

static void async_initable_iface_init (GAsyncInitableIface *iface);

G_DEFINE_TYPE_EXTENDED (IdeGsettingsFileSettings,
                        ide_gsettings_file_settings,
                        IDE_TYPE_FILE_SETTINGS,
                        0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE,
                                               async_initable_iface_init))

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

  object_class->finalize = ide_gsettings_file_settings_finalize;
}

static void
ide_gsettings_file_settings_init (IdeGsettingsFileSettings *self)
{
}

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

static void
ide_gsettings_file_settings_init_async (GAsyncInitable      *initable,
                                        gint                 io_priority,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  IdeGsettingsFileSettings *self = (IdeGsettingsFileSettings *)initable;
  g_autoptr(GSettings) settings = NULL;
  g_autoptr(GTask) task = NULL;
  g_autofree gchar *path = NULL;
  IdeLanguage *language;
  IdeFile *file;
  const gchar *lang_id;

  g_return_if_fail (IDE_IS_GSETTINGS_FILE_SETTINGS (self));

  task = g_task_new (self, cancellable, callback, user_data);

  file = ide_file_settings_get_file (IDE_FILE_SETTINGS (self));

  if (!file)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_FOUND,
                               _("No file was provided"));
      return;
    }

  language = ide_file_get_language (file);

  if (!language)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               _("Failed to retrieve langauge for file."));
      return;
    }

  lang_id = ide_language_get_id (language);

  path = g_strdup_printf ("/org/gnome/builder/editor/language/%s/", lang_id);
  settings = g_settings_new_with_path ("org.gnome.builder.editor.language", path);

  self->settings = g_object_ref (settings);

  g_settings_bind (self->settings, "indent-width", self, "indent-width",
                   G_SETTINGS_BIND_GET);
  g_settings_bind (self->settings, "tab-width", self, "tab-width",
                   G_SETTINGS_BIND_GET);
  g_settings_bind_with_mapping (self->settings, "insert-spaces-instead-of-tabs",
                                self, "indent-style", G_SETTINGS_BIND_GET,
                                indent_style_get, NULL, NULL, NULL);
  g_settings_bind (self->settings, "right-margin-position",
                   self, "right-margin-position",
                   G_SETTINGS_BIND_GET);
  g_settings_bind (self->settings, "trim-trailing-whitespace",
                   self, "trim-trailing-whitespace",
                   G_SETTINGS_BIND_GET);

  g_task_return_boolean (task, TRUE);
}

static gboolean
ide_gsettings_file_settings_init_finish (GAsyncInitable  *initable,
                                         GAsyncResult    *result,
                                         GError         **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_GSETTINGS_FILE_SETTINGS (initable), FALSE);
  g_return_val_if_fail (G_IS_TASK (task), FALSE);

  return g_task_propagate_boolean (task, error);
}

static void
async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = ide_gsettings_file_settings_init_async;
  iface->init_finish = ide_gsettings_file_settings_init_finish;
}
