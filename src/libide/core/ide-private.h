/* ide-private.h
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#include <glib.h>

G_BEGIN_DECLS

typedef struct
{
  void (*load)     (void);
  void (*unload)   (void);
  void (*function) (const gchar    *func,
                    gint64          begin_time_usec,
                    gint64          end_time_usec);
  void (*log)      (GLogLevelFlags  log_level,
                    const gchar    *domain,
                    const gchar    *message);
} IdeTraceVTable;

void                 _ide_trace_init     (IdeTraceVTable *vtable);
void                 _ide_trace_log      (GLogLevelFlags  log_level,
                                          const gchar    *domain,
                                          const gchar    *message);
void                 _ide_trace_shutdown (void);
const gchar * const *_ide_host_environ   (void);

G_END_DECLS
