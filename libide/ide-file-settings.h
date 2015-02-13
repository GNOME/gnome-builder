/* ide-file-settings.h
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

#ifndef IDE_FILE_SETTINGS_H
#define IDE_FILE_SETTINGS_H

#include <gtksourceview/gtksource.h>

#include "ide-object.h"
#include "ide-indent-style.h"

G_BEGIN_DECLS

#define IDE_TYPE_FILE_SETTINGS (ide_file_settings_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeFileSettings, ide_file_settings,
                          IDE, FILE_SETTINGS, IdeObject)

struct _IdeFileSettingsClass
{
  IdeObjectClass parent;
};

const gchar          *ide_file_settings_get_encoding                 (IdeFileSettings      *self);
IdeIndentStyle        ide_file_settings_get_indent_style             (IdeFileSettings      *self);
guint                 ide_file_settings_get_indent_width             (IdeFileSettings      *self);
gboolean              ide_file_settings_get_insert_trailing_newline  (IdeFileSettings      *self);
GtkSourceNewlineType  ide_file_settings_get_newline_type             (IdeFileSettings      *self);
guint                 ide_file_settings_get_tab_width                (IdeFileSettings      *self);
gboolean              ide_file_settings_get_trim_trailing_whitespace (IdeFileSettings      *self);
void                  ide_file_settings_set_encoding                 (IdeFileSettings      *self,
                                                                      const gchar          *encoding);
void                  ide_file_settings_set_indent_style             (IdeFileSettings      *self,
                                                                      IdeIndentStyle        indent_style);
void                  ide_file_settings_set_indent_width             (IdeFileSettings      *self,
                                                                      guint                 indent_width);
void                  ide_file_settings_set_insert_trailing_newline  (IdeFileSettings      *self,
                                                                      gboolean              insert_trailing_newline);
void                  ide_file_settings_set_newline_type             (IdeFileSettings      *self,
                                                                      GtkSourceNewlineType  newline_type);
void                  ide_file_settings_set_tab_width                (IdeFileSettings      *self,
                                                                      guint                 tab_width);
void                  ide_file_settings_set_trim_trailing_whitespace (IdeFileSettings      *self,
                                                                      gboolean              trim_trailing_whitespace);

G_END_DECLS

#endif /* IDE_FILE_SETTINGS_H */
