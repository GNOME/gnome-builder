/* ide-path-element.h
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_PATH_ELEMENT (ide_path_element_get_type())

IDE_AVAILABLE_IN_3_34
G_DECLARE_DERIVABLE_TYPE (IdePathElement, ide_path_element, IDE, PATH_ELEMENT, GObject)

struct _IdePathElementClass
{
  GObjectClass parent_class;

  /*< private >*/
  gpointer _reserved[8];
};

IDE_AVAILABLE_IN_3_34
IdePathElement *ide_path_element_new       (const gchar    *id,
                                            const gchar    *title);
IDE_AVAILABLE_IN_3_34
const gchar    *ide_path_element_get_id    (IdePathElement *self);
IDE_AVAILABLE_IN_3_34
const gchar    *ide_path_element_get_title (IdePathElement *self);
IDE_AVAILABLE_IN_3_34
gboolean        ide_path_element_equal     (IdePathElement *self,
                                            IdePathElement *other);

G_END_DECLS
