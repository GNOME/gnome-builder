/* ide-log.h
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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

#include <glib.h>

#include "ide-version-macros.h"

G_BEGIN_DECLS

IDE_AVAILABLE_IN_ALL
void ide_log_init               (gboolean     stdout_,
                                 const gchar *filename);
IDE_AVAILABLE_IN_ALL
void ide_log_increase_verbosity (void);
IDE_AVAILABLE_IN_ALL
gint ide_log_get_verbosity      (void);
IDE_AVAILABLE_IN_ALL
void ide_log_set_verbosity      (gint         level);
IDE_AVAILABLE_IN_ALL
void ide_log_shutdown           (void);

G_END_DECLS
