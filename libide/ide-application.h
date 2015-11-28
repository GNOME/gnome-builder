/* ide-application.h
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

#ifndef IDE_APPLICATION_H
#define IDE_APPLICATION_H

#include <gtk/gtk.h>

#include "ide-recent-projects.h"

G_BEGIN_DECLS

#define IDE_TYPE_APPLICATION    (ide_application_get_type())
#define IDE_APPLICATION_DEFAULT (IDE_APPLICATION (g_application_get_default()))

G_DECLARE_FINAL_TYPE (IdeApplication, ide_application, IDE, APPLICATION, GtkApplication)

typedef enum
{
  IDE_APPLICATION_MODE_PRIMARY,
  IDE_APPLICATION_MODE_WORKER,
  IDE_APPLICATION_MODE_TOOL,
} IdeApplicationMode;

IdeApplicationMode  ide_application_get_mode             (IdeApplication       *self);
IdeApplication     *ide_application_new                  (void);
GDateTime          *ide_application_get_started_at       (IdeApplication       *self);
IdeRecentProjects  *ide_application_get_recent_projects  (IdeApplication       *self);
void                ide_application_show_projects_window (IdeApplication       *self);
const gchar        *ide_application_get_keybindings_mode (IdeApplication       *self);
void                ide_application_get_worker_async     (IdeApplication       *self,
                                                          const gchar          *plugin_name,
                                                          GCancellable         *cancellable,
                                                          GAsyncReadyCallback   callback,
                                                          gpointer              user_data);
GDBusProxy         *ide_application_get_worker_finish    (IdeApplication       *self,
                                                          GAsyncResult         *result,
                                                          GError              **error);

G_END_DECLS

#endif /* IDE_APPLICATION_H */
