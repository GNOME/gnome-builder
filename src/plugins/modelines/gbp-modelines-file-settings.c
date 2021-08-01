/* gbp-modelines-file-settings.c
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

#define G_LOG_DOMAIN "gbp-modelines-file-settings"

#include "config.h"

#include <libide-code.h>
#include <glib/gi18n.h>

#include "gbp-modelines-file-settings.h"
#include "modeline-parser.h"

struct _GbpModelinesFileSettings
{
  IdeFileSettings parent_instance;
};

G_DEFINE_FINAL_TYPE (GbpModelinesFileSettings, gbp_modelines_file_settings, IDE_TYPE_FILE_SETTINGS)

static gboolean
buffer_file_matches (GbpModelinesFileSettings *self,
                     IdeBuffer                *buffer)
{
  GFile *our_file;
  GFile *buffer_file;

  g_assert (GBP_IS_MODELINES_FILE_SETTINGS (self));
  g_assert (IDE_IS_BUFFER (buffer));

  buffer_file = ide_buffer_get_file (buffer);
  our_file = ide_file_settings_get_file (IDE_FILE_SETTINGS (self));

  return g_file_equal (buffer_file, our_file);
}

static void
buffer_loaded_cb (GbpModelinesFileSettings *self,
                  IdeBuffer                *buffer,
                  IdeBufferManager         *buffer_manager)
{
  g_assert (GBP_IS_MODELINES_FILE_SETTINGS (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));

  if (buffer_file_matches (self, buffer))
    modeline_parser_apply_modeline (GTK_TEXT_BUFFER (buffer), IDE_FILE_SETTINGS (self));
}

static void
buffer_saved_cb (GbpModelinesFileSettings *self,
                 IdeBuffer                *buffer,
                 IdeBufferManager         *buffer_manager)
{
  g_assert (GBP_IS_MODELINES_FILE_SETTINGS (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));

  if (buffer_file_matches (self, buffer))
    modeline_parser_apply_modeline (GTK_TEXT_BUFFER (buffer), IDE_FILE_SETTINGS (self));
}

static void
gbp_modelines_file_settings_parent_set (IdeObject *object,
                                        IdeObject *parent)
{
  GbpModelinesFileSettings *self = (GbpModelinesFileSettings *)object;
  IdeBufferManager *buffer_manager;
  IdeContext *context;

  g_assert (IDE_IS_OBJECT (object));
  g_assert (!parent || IDE_IS_OBJECT (parent));

  if (parent == NULL)
    return;

  context = ide_object_get_context (IDE_OBJECT (self));
  buffer_manager = ide_buffer_manager_from_context (context);

  g_signal_connect_object (buffer_manager,
                           "buffer-loaded",
                           G_CALLBACK (buffer_loaded_cb),
                           self,
                           G_CONNECT_SWAPPED | G_CONNECT_AFTER);

  g_signal_connect_object (buffer_manager,
                           "buffer-saved",
                           G_CALLBACK (buffer_saved_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
gbp_modelines_file_settings_class_init (GbpModelinesFileSettingsClass *klass)
{
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  i_object_class->parent_set = gbp_modelines_file_settings_parent_set;
}

static void
gbp_modelines_file_settings_init (GbpModelinesFileSettings *self)
{
}
