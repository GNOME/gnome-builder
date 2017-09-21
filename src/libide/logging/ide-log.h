/* ide-log.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#ifndef IDE_LOG_H
#define IDE_LOG_H

#include <glib.h>

G_BEGIN_DECLS

void ide_log_init               (gboolean     stdout_,
                                 const gchar *filename);
void ide_log_increase_verbosity (void);
gint ide_log_get_verbosity      (void);
void ide_log_set_verbosity      (gint         level);
void ide_log_shutdown           (void);

G_END_DECLS

#endif /* IDE_LOG_H */
