/* egg-centering-bin.h
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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

#ifndef EGG_CENTERING_BIN_H
#define EGG_CENTERING_BIN_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EGG_TYPE_CENTERING_BIN (egg_centering_bin_get_type())

G_DECLARE_DERIVABLE_TYPE (EggCenteringBin, egg_centering_bin, EGG, CENTERING_BIN, GtkBin)

struct _EggCenteringBinClass
{
  GtkBinClass parent;
};

GtkWidget *egg_centering_bin_new (void);

G_END_DECLS

#endif /* EGG_CENTERING_BIN_H */
