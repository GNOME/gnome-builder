/* ide-gi-function.h
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

#include <glib.h>
#include <glib-object.h>

#include "./../ide-gi-blob.h"
#include "./../ide-gi-types.h"
#include "./../ide-gi-namespace.h"
#include "./../ide-gi-namespace-private.h"

#include "ide-gi-base.h"
#include "ide-gi-parameter.h"

G_BEGIN_DECLS

struct _IdeGiFunction
{
  IDE_GI_BASE_FIELDS
};

void              ide_gi_function_free                (IdeGiBase       *base);
IdeGiBase        *ide_gi_function_new                 (IdeGiNamespace  *ns,
                                                       IdeGiBlobType    type,
                                                       gint32           offset);
void              ide_gi_function_dump                (IdeGiFunction   *self,
                                                       guint            depth);
IdeGiFunction    *ide_gi_function_ref                 (IdeGiFunction   *self);
void              ide_gi_function_unref               (IdeGiFunction   *self);

gboolean          ide_gi_function_is_setter           (IdeGiFunction   *self);
gboolean          ide_gi_function_is_getter           (IdeGiFunction   *self);
gboolean          ide_gi_function_is_throws           (IdeGiFunction   *self);

const gchar      *ide_gi_function_get_c_identifier    (IdeGiFunction   *self);
const gchar      *ide_gi_function_get_shadowed_by     (IdeGiFunction   *self);
const gchar      *ide_gi_function_get_shadows         (IdeGiFunction   *self);
const gchar      *ide_gi_function_get_moved_to        (IdeGiFunction   *self);
const gchar      *ide_gi_function_get_invoker         (IdeGiFunction   *self);

guint16           ide_gi_function_get_n_parameters    (IdeGiFunction   *self);
IdeGiParameter   *ide_gi_function_get_parameter       (IdeGiFunction   *self,
                                                       guint16          nth);
IdeGiParameter   *ide_gi_function_get_return_value    (IdeGiFunction   *self);

IdeGiParameter   *ide_gi_function_lookup_parameter    (IdeGiFunction    *self,
                                                        const gchar     *name);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeGiFunction, ide_gi_function_unref)

G_END_DECLS
