/* gb-command-result.h
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#define GB_TYPE_COMMAND_RESULT (gb_command_result_get_type())

G_DECLARE_FINAL_TYPE (GbCommandResult, gb_command_result, GB, COMMAND_RESULT, GObject)

GbCommandResult *gb_command_result_new              (void);
gboolean         gb_command_result_get_is_running   (GbCommandResult *result);
void             gb_command_result_set_is_running   (GbCommandResult *result,
                                                     gboolean         is_running);
gboolean         gb_command_result_get_is_error     (GbCommandResult *result);
void             gb_command_result_set_is_error     (GbCommandResult *result,
                                                     gboolean         is_error);
const gchar     *gb_command_result_get_command_text (GbCommandResult *result);
void             gb_command_result_set_command_text (GbCommandResult *result,
                                                     const gchar     *command_text);
const gchar     *gb_command_result_get_result_text  (GbCommandResult *result);
void             gb_command_result_set_result_text  (GbCommandResult *result,
                                                     const gchar     *result_text);

G_END_DECLS
