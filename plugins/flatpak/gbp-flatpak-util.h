/* gbp-flatpak-util.h
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

#ifndef GBP_FLATPAK_UTIL_H
#define GBP_FLATPAK_UTIL_H

#include <ide.h>

G_BEGIN_DECLS

gboolean  gbp_flatpak_is_ignored      (const gchar      *name);
gchar    *gbp_flatpak_get_repo_dir    (IdeConfiguration *configuration);
gchar    *gbp_flatpak_get_staging_dir (IdeConfiguration *configuration);

G_END_DECLS

#endif /* GBP_FLATPAK_UTIL_H */
