/* ide-location.h
 *
 * Copyright 2014-2019 Christian Hergert <chergert@redhat.com>
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

#if !defined (IDE_CODE_INSIDE) && !defined (IDE_CODE_COMPILATION)
# error "Only <libide-code.h> can be included directly."
#endif

#include <libide-core.h>

#include "ide-code-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_LOCATION (ide_location_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeLocation, ide_location, IDE, LOCATION, GObject)

struct _IdeLocationClass
{
  GObjectClass parent_class;

  /*< private >*/
  gpointer _reserved[16];
};

IDE_AVAILABLE_IN_ALL
IdeLocation *ide_location_new_from_variant (GVariant    *variant);
IDE_AVAILABLE_IN_ALL
IdeLocation *ide_location_new              (GFile       *file,
                                            gint         line,
                                            gint         line_offset);
IDE_AVAILABLE_IN_ALL
IdeLocation *ide_location_new_with_offset  (GFile       *file,
                                            gint         line,
                                            gint         line_offset,
                                            gint         offset);
IDE_AVAILABLE_IN_ALL
IdeLocation *ide_location_dup              (IdeLocation *self);
IDE_AVAILABLE_IN_ALL
gint         ide_location_get_line         (IdeLocation *self);
IDE_AVAILABLE_IN_ALL
gint         ide_location_get_line_offset  (IdeLocation *self);
IDE_AVAILABLE_IN_ALL
gint         ide_location_get_offset       (IdeLocation *self);
IDE_AVAILABLE_IN_ALL
GFile       *ide_location_get_file         (IdeLocation *self);
IDE_AVAILABLE_IN_ALL
GVariant    *ide_location_to_variant       (IdeLocation *self);
IDE_AVAILABLE_IN_ALL
gboolean     ide_location_compare          (IdeLocation *a,
                                            IdeLocation *b);
IDE_AVAILABLE_IN_ALL
guint        ide_location_hash             (IdeLocation *self);
IDE_AVAILABLE_IN_ALL
gboolean     ide_location_equal            (IdeLocation *a,
                                            IdeLocation *b);
IDE_AVAILABLE_IN_ALL
char        *ide_location_dup_title        (IdeLocation *self);

G_END_DECLS
