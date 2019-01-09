/* ide-gtask-private.h
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

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

void ide_g_task_return_boolean_from_main (GTask          *task,
                                          gboolean        value);
void ide_g_task_return_int_from_main     (GTask          *task,
                                          gint            value);
void ide_g_task_return_pointer_from_main (GTask          *task,
                                          gpointer        value,
                                          GDestroyNotify  notify);
void ide_g_task_return_error_from_main   (GTask          *task,
                                          GError         *error);

G_END_DECLS
