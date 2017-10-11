/* gbp-sysprof-perspective.h
 *
 * Copyright © 2016 Christian Hergert <chergert@redhat.com>
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

#include <ide.h>
#include <sysprof-ui.h>

G_BEGIN_DECLS

#define GBP_TYPE_SYSPROF_PERSPECTIVE (gbp_sysprof_perspective_get_type())

G_DECLARE_FINAL_TYPE (GbpSysprofPerspective, gbp_sysprof_perspective, GBP, SYSPROF_PERSPECTIVE, GtkBin)

SpZoomManager   *gbp_sysprof_perspective_get_zoom_manager (GbpSysprofPerspective *self);
void             gbp_sysprof_perspective_set_profiler     (GbpSysprofPerspective *self,
                                                           SpProfiler            *profiler);
SpCaptureReader *gbp_sysprof_perspective_get_reader       (GbpSysprofPerspective *self);
void             gbp_sysprof_perspective_set_reader       (GbpSysprofPerspective *self,
                                                           SpCaptureReader       *reader);

G_END_DECLS
