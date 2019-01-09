/* gb-vim.c
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

G_BEGIN_DECLS

#define GB_VIM_ERROR (gb_vim_error_quark())

typedef enum
{
  GB_VIM_ERROR_NOT_IMPLEMENTED,
  GB_VIM_ERROR_NOT_FOUND,
  GB_VIM_ERROR_NOT_NUMBER,
  GB_VIM_ERROR_NUMBER_OUT_OF_RANGE,
  GB_VIM_ERROR_CANNOT_FIND_COLORSCHEME,
  GB_VIM_ERROR_UNKNOWN_OPTION,
  GB_VIM_ERROR_NOT_SOURCE_VIEW,
  GB_VIM_ERROR_NO_VIEW
} IdeVimError;

GQuark        gb_vim_error_quark (void);
gboolean      gb_vim_execute     (GtkWidget      *active_widget,
                                  const gchar    *line,
                                  GError        **error);
gchar       **gb_vim_complete    (GtkWidget      *active_widget,
                                  const gchar    *line);
const gchar **gb_vim_commands    (const gchar    *typed_text,
                                  const gchar  ***descriptions);

G_END_DECLS
