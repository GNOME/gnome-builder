/* tmpl-gi-private.h
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TMPL_GI_PRIVATE_H
#define TMPL_GI_PRIVATE_H

#include <girepository.h>

G_BEGIN_DECLS

#define TMPL_TYPE_TYPELIB (tmpl_typelib_get_type())

GType    tmpl_typelib_get_type         (void);
gboolean tmpl_gi_argument_from_g_value (const GValue  *value,
                                        GITypeInfo    *type_info,
                                        GIArgument    *arg,
                                        GError       **error);
gboolean tmpl_gi_argument_to_g_value   (GValue        *value,
                                        GITypeInfo    *type_info,
                                        GIArgument    *arg,
                                        GError       **error);

G_END_DECLS

#endif /* TMPL_GI_PRIVATE_H */
