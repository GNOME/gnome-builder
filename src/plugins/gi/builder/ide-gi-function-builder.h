/* ide-gi-function-builder.h
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

#ifndef IDE_GI_FUNCTION_BUILDER_H
#define IDE_GI_FUNCTION_BUILDER_H

#include <glib-object.h>

#include "../ide-gi-types.h"

#include "../ide-gi-helper.h"
#include "../ide-gi-parser.h"
#include "../ide-gi-parser-object.h"
#include "../ide-gi-parser-result.h"
#include "../ide-gi-pool.h"

G_BEGIN_DECLS

#define IDE_TYPE_GI_FUNCTION_BUILDER (ide_gi_function_builder_get_type())

G_DECLARE_FINAL_TYPE (IdeGiFunctionBuilder, ide_gi_function_builder, IDE, GI_FUNCTION_BUILDER, IdeGiParserObject)

IdeGiParserObject *ide_gi_function_builder_new (void);

G_END_DECLS

#endif /* IDE_GI_FUNCTION_BUILDER_H */

