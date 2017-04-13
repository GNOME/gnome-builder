/* egg-elastic-bin.h
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef EGG_ELASTIC_BIN_H
#define EGG_ELASTIC_BIN_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EGG_TYPE_ELASTIC_BIN (egg_elastic_bin_get_type())

G_DECLARE_DERIVABLE_TYPE (EggElasticBin, egg_elastic_bin, EGG, ELASTIC_BIN, GtkBin)

struct _EggElasticBinClass
{
  GtkBinClass parent_class;

  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
};

GtkWidget *egg_elastic_bin_new (void);

G_END_DECLS

#endif /* EGG_ELASTIC_BIN_H */
