/* ide-object-box.h
 *
 * Copyright 2018 Christian Hergert <unknown@domain.org>
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

#if !defined (IDE_CORE_INSIDE) && !defined (IDE_CORE_COMPILATION)
# error "Only <libide-core.h> can be included directly."
#endif

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_OBJECT_BOX (ide_object_box_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeObjectBox, ide_object_box, IDE, OBJECT_BOX, IdeObject)

IDE_AVAILABLE_IN_ALL
IdeObjectBox *ide_object_box_new         (GObject      *object);
IDE_AVAILABLE_IN_ALL
gpointer      ide_object_box_ref_object  (IdeObjectBox *self);
IDE_AVAILABLE_IN_ALL
IdeObjectBox *ide_object_box_from_object (GObject      *object);
IDE_AVAILABLE_IN_ALL
gboolean      ide_object_box_contains    (IdeObjectBox *self,
                                          gpointer      instance);

G_END_DECLS
