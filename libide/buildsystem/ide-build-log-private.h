/* ide-build-log-private.h
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#ifndef IDE_BUILD_LOG_PRIVATE_H
#define IDE_BUILD_LOG_PRIVATE_H

#include <gio/gio.h>

#include "ide-build-log.h"

G_BEGIN_DECLS

#define IDE_TYPE_BUILD_LOG (ide_build_log_get_type())

G_DECLARE_FINAL_TYPE (IdeBuildLog, ide_build_log, IDE, BUILD_LOG, GObject)

IdeBuildLog *ide_build_log_new              (void);
void         ide_build_log_observer         (IdeBuildLogStream    stream,
                                             const gchar         *message,
                                             gssize               message_len,
                                             gpointer             user_data);
guint        ide_build_log_add_observer     (IdeBuildLog         *self,
                                             IdeBuildLogObserver  observer,
                                             gpointer             observer_data,
                                             GDestroyNotify       observer_data_destroy);
gboolean     ide_build_log_remove_observer  (IdeBuildLog         *self,
                                             guint                observer_id);


G_END_DECLS

#endif /* IDE_BUILD_LOG_PRIVATE_H */
