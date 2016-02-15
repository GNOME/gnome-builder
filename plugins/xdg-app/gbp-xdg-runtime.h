/* gbp-xdg-runtime.h
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

#ifndef GBP_XDG_RUNTIME_H
#define GBP_XDG_RUNTIME_H

#include <ide.h>

G_BEGIN_DECLS

#define GBP_TYPE_XDG_RUNTIME (gbp_xdg_runtime_get_type())

G_DECLARE_FINAL_TYPE (GbpXdgRuntime, gbp_xdg_runtime, GBP, XDG_RUNTIME, IdeRuntime)

G_END_DECLS

#endif /* GBP_XDG_RUNTIME_H */
