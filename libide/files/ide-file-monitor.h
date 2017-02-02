/* ide-file-monitor.h
 *
 * Copyright (C) 2017 Matthew Leeds <mleeds@redhat.com>
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

#ifndef IDE_FILE_MONITOR_H
#define IDE_FILE_MONITOR_H

#include <gtksourceview/gtksource.h>

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_FILE_MONITOR (ide_file_monitor_get_type())

G_DECLARE_FINAL_TYPE (IdeFileMonitor, ide_file_monitor, IDE, FILE_MONITOR, IdeObject)

G_END_DECLS

#endif /* IDE_FILE_MONITOR_H */
