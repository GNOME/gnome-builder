/* ide-formatter-options.h
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

G_BEGIN_DECLS

#define IDE_TYPE_FORMATTER_OPTIONS (ide_formatter_options_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeFormatterOptions, ide_formatter_options, IDE, FORMATTER_OPTIONS, GObject)

IDE_AVAILABLE_IN_ALL
IdeFormatterOptions *ide_formatter_options_new               (void);
IDE_AVAILABLE_IN_ALL
guint                ide_formatter_options_get_tab_width     (IdeFormatterOptions *self);
IDE_AVAILABLE_IN_ALL
void                 ide_formatter_options_set_tab_width     (IdeFormatterOptions *self,
                                                              guint                tab_width);
IDE_AVAILABLE_IN_ALL
gboolean             ide_formatter_options_get_insert_spaces (IdeFormatterOptions *self);
IDE_AVAILABLE_IN_ALL
void                 ide_formatter_options_set_insert_spaces (IdeFormatterOptions *self,
                                                              gboolean             insert_spaces);

G_END_DECLS
