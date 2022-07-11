/* ide-range.h
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define IDE_TYPE_RANGE (ide_range_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeRange, ide_range, IDE, RANGE, GObject)

struct _IdeRangeClass
{
  GObjectClass parent_class;

  /*< private >*/
  gpointer _reserved[16];
};

IDE_AVAILABLE_IN_ALL
IdeRange    *ide_range_new_from_variant (GVariant    *variant);
IDE_AVAILABLE_IN_ALL
IdeRange    *ide_range_new              (IdeLocation *begin,
                                         IdeLocation *end);
IDE_AVAILABLE_IN_ALL
IdeLocation *ide_range_get_begin        (IdeRange    *self);
IDE_AVAILABLE_IN_ALL
IdeLocation *ide_range_get_end          (IdeRange    *self);
IDE_AVAILABLE_IN_ALL
GVariant    *ide_range_to_variant       (IdeRange    *self);

G_END_DECLS
