/* ide-modelines-file-settings.c
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

#define G_LOG_DOMAIN "ide-modelines-file-settings"

#include <glib/gi18n.h>

#include "ide-context.h"

#include "buffers/ide-buffer-manager.h"
#include "buffers/ide-buffer.h"
#include "modelines/ide-modelines-file-settings.h"
#include "modelines/modeline-parser.h"

struct _IdeModelinesFileSettings
{
  IdeFileSettings parent_instance;
};

G_DEFINE_TYPE (IdeModelinesFileSettings, ide_modelines_file_settings, IDE_TYPE_FILE_SETTINGS)

static void
buffer_loaded_cb (IdeModelinesFileSettings *self,
                  IdeBuffer                *buffer,
                  IdeBufferManager         *buffer_manager)
{
  IdeFile *our_file;
  IdeFile *buffer_file;

  g_assert (IDE_IS_MODELINES_FILE_SETTINGS (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));

  if ((buffer_file = ide_buffer_get_file (buffer)) &&
      (our_file = ide_file_settings_get_file (IDE_FILE_SETTINGS (self))) &&
      ide_file_equal (buffer_file, our_file))
    {
      modeline_parser_apply_modeline (GTK_TEXT_BUFFER (buffer), IDE_FILE_SETTINGS (self));
    }
}

static void
buffer_saved_cb (IdeModelinesFileSettings *self,
                 IdeBuffer                *buffer,
                 IdeBufferManager         *buffer_manager)
{
  IdeFile *our_file;
  IdeFile *buffer_file;

  g_assert (IDE_IS_MODELINES_FILE_SETTINGS (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));

  if ((buffer_file = ide_buffer_get_file (buffer)) &&
      (our_file = ide_file_settings_get_file (IDE_FILE_SETTINGS (self))) &&
      ide_file_equal (buffer_file, our_file))
    {
      modeline_parser_apply_modeline (GTK_TEXT_BUFFER (buffer), IDE_FILE_SETTINGS (self));
    }
}

static void
ide_modelines_file_settings_constructed (GObject *object)
{
  IdeModelinesFileSettings *self = (IdeModelinesFileSettings *)object;
  IdeBufferManager *buffer_manager;
  IdeContext *context;

  G_OBJECT_CLASS (ide_modelines_file_settings_parent_class)->constructed (object);

  context = ide_object_get_context (IDE_OBJECT (self));
  buffer_manager = ide_context_get_buffer_manager (context);

  g_signal_connect_object (buffer_manager,
                           "buffer-loaded",
                           G_CALLBACK (buffer_loaded_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (buffer_manager,
                           "buffer-saved",
                           G_CALLBACK (buffer_saved_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
ide_modelines_file_settings_class_init (IdeModelinesFileSettingsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_modelines_file_settings_constructed;
}

static void
ide_modelines_file_settings_init (IdeModelinesFileSettings *self)
{
}
