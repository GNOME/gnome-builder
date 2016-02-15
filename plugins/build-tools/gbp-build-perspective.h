/* gbp-build-perspective.h
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

#ifndef GBP_BUILD_PERSPECTIVE_H
#define GBP_BUILD_PERSPECTIVE_H

#include <ide.h>

G_BEGIN_DECLS

#define GBP_TYPE_BUILD_PERSPECTIVE (gbp_build_perspective_get_type())

G_DECLARE_FINAL_TYPE (GbpBuildPerspective, gbp_build_perspective, GBP, BUILD_PERSPECTIVE, GtkBin)

IdeConfiguration *gbp_build_perspective_get_configuration (GbpBuildPerspective *self);
void              gbp_build_perspective_set_configuration (GbpBuildPerspective *self,
                                                           IdeConfiguration    *configuration);

G_END_DECLS

#endif /* GBP_BUILD_PERSPECTIVE_H */
