/* ide-formatter-options.h
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define IDE_TYPE_FORMATTER_OPTIONS (ide_formatter_options_get_type())

G_DECLARE_FINAL_TYPE (IdeFormatterOptions, ide_formatter_options, IDE, FORMATTER_OPTIONS, GObject)

IdeFormatterOptions *ide_formatter_options_new               (void);
guint                ide_formatter_options_get_tab_width     (IdeFormatterOptions *self);
void                 ide_formatter_options_set_tab_width     (IdeFormatterOptions *self,
                                                              guint                tab_width);
gboolean             ide_formatter_options_get_insert_spaces (IdeFormatterOptions *self);
void                 ide_formatter_options_set_insert_spaces (IdeFormatterOptions *self,
                                                              gboolean             insert_spaces);

G_END_DECLS
