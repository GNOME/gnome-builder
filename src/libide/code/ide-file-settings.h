/* ide-file-settings.h
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

#pragma once

#if !defined (IDE_CODE_INSIDE) && !defined (IDE_CODE_COMPILATION)
# error "Only <libide-code.h> can be included directly."
#endif

#include <libide-core.h>
#include <gtksourceview/gtksource.h>

#include "ide-code-types.h"
#include "ide-indent-style.h"
#include "ide-spaces-style.h"

G_BEGIN_DECLS

#define IDE_TYPE_FILE_SETTINGS            (ide_file_settings_get_type())
#define IDE_FILE_SETTINGS_EXTENSION_POINT "org.gnome.libide.extensions.file-settings"

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeFileSettings, ide_file_settings, IDE, FILE_SETTINGS, IdeObject)

struct _IdeFileSettingsClass
{
  IdeObjectClass parent;

  /*< private >*/
  gpointer _reserved[8];
};

IDE_AVAILABLE_IN_ALL
IdeFileSettings *ide_file_settings_new          (IdeObject       *parent,
                                                 GFile           *file,
                                                 const gchar     *language);
IDE_AVAILABLE_IN_ALL
GFile           *ide_file_settings_get_file     (IdeFileSettings *self);
IDE_AVAILABLE_IN_ALL
const gchar     *ide_file_settings_get_language (IdeFileSettings *self);
IDE_AVAILABLE_IN_ALL
gboolean         ide_file_settings_get_settled  (IdeFileSettings *self);

#define IDE_FILE_SETTINGS_PROPERTY(_1, name, _2, ret_type, _3, _4, _5, _6) \
  _IDE_EXTERN ret_type ide_file_settings_get_##name (IdeFileSettings *self);
# include "ide-file-settings.defs"
#undef IDE_FILE_SETTINGS_PROPERTY

#define IDE_FILE_SETTINGS_PROPERTY(_1, name, _2, ret_type, _3, _4, _5, _6) \
  _IDE_EXTERN void ide_file_settings_set_##name (IdeFileSettings *self, \
                                                 ret_type         name);
# include "ide-file-settings.defs"
#undef IDE_FILE_SETTINGS_PROPERTY

#define IDE_FILE_SETTINGS_PROPERTY(_1, name, _2, _3, _4, _5, _6, _7) \
  _IDE_EXTERN gboolean ide_file_settings_get_##name##_set (IdeFileSettings *self);
# include "ide-file-settings.defs"
#undef IDE_FILE_SETTINGS_PROPERTY

#define IDE_FILE_SETTINGS_PROPERTY(_1, name, _2, _3, _4, _5, _6, _7) \
  _IDE_EXTERN void ide_file_settings_set_##name##_set (IdeFileSettings *self, \
                                                       gboolean         name##_set);
# include "ide-file-settings.defs"
#undef IDE_FILE_SETTINGS_PROPERTY

G_END_DECLS
