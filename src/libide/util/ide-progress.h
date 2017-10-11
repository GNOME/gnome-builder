/* ide-progress.h
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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

#define IDE_TYPE_PROGRESS (ide_progress_get_type())

G_DECLARE_FINAL_TYPE (IdeProgress, ide_progress, IDE, PROGRESS, GObject)

IdeProgress *ide_progress_new                       (void);
gdouble      ide_progress_get_fraction              (IdeProgress *self);
gchar       *ide_progress_get_message               (IdeProgress *self);
void         ide_progress_set_fraction              (IdeProgress *self,
                                                     gdouble      fraction);
void         ide_progress_set_message               (IdeProgress *self,
                                                     const gchar *message);
void         ide_progress_flatpak_progress_callback (const char  *status,
                                                     guint        progress,
                                                     gboolean     estimating,
                                                     gpointer     user_data);
void         ide_progress_file_progress_callback    (goffset      current_num_bytes,
                                                     goffset      total_num_bytes,
                                                     gpointer     user_data);

G_END_DECLS
