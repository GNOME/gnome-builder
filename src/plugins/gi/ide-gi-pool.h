/* ide-gi-pool.h
 *
 * Copyright (C) 2017 Sebastien Lafargue <slafargue@gnome.org>
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

#include <glib-object.h>

#include "ide-gi-types.h"

#include "ide-gi-parser.h"
#include "ide-gi-parser-object.h"

G_BEGIN_DECLS

const gchar          *_ide_gi_pool_get_element_type_string     (IdeGiElementType  type);

const gchar          *ide_gi_pool_get_unhandled_element        (IdeGiPool        *self);
IdeGiParserObject    *ide_gi_pool_get_object                   (IdeGiPool        *self,
                                                                IdeGiElementType  type);
IdeGiParserObject    *ide_gi_pool_get_current_parser_object    (IdeGiPool        *self);
IdeGiParserObject    *ide_gi_pool_get_parent_parser_object     (IdeGiPool        *self);
IdeGiPool            *ide_gi_pool_new                          (gboolean          reuse);
gboolean              ide_gi_pool_release_object               (IdeGiPool        *self);
void                  ide_gi_pool_set_unhandled_element        (IdeGiPool        *self,
                                                                const gchar      *element);

G_END_DECLS
