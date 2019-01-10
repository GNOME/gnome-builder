/* gbp-trim-spaces-buffer-addin.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-trim-spaces-buffer-addin"

#include "config.h"

#include <libide-code.h>

#include "ide-buffer-private.h"

#include "gbp-trim-spaces-buffer-addin.h"

struct _GbpTrimSpacesBufferAddin
{
  GObject parent_instance;
};

static void
gbp_trim_spaces_buffer_addin_save_file (IdeBufferAddin *addin,
                                        IdeBuffer      *buffer,
                                        GFile          *file)
{
  IdeFileSettings *file_settings;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_TRIM_SPACES_BUFFER_ADDIN (addin));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_FILE (file));

  if (!(file_settings = ide_buffer_get_file_settings (buffer)) ||
      !ide_file_settings_get_trim_trailing_whitespace (file_settings))
    return;

  /*
   * If file-settings dictate that we should trim trailing whitespace, trim it
   * from the modified lines in the IdeBuffer. This is performed automatically
   * based on line state within ide_buffer_trim_trailing_whitespace().
   */
  ide_buffer_trim_trailing_whitespace (buffer);
}

static void
buffer_addin_iface_init (IdeBufferAddinInterface *iface)
{
  iface->save_file = gbp_trim_spaces_buffer_addin_save_file;
}

G_DEFINE_TYPE_WITH_CODE (GbpTrimSpacesBufferAddin, gbp_trim_spaces_buffer_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_BUFFER_ADDIN, buffer_addin_iface_init))

static void
gbp_trim_spaces_buffer_addin_class_init (GbpTrimSpacesBufferAddinClass *klass)
{
}

static void
gbp_trim_spaces_buffer_addin_init (GbpTrimSpacesBufferAddin *self)
{
}
