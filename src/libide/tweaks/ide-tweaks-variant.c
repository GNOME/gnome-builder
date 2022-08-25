/* ide-tweaks-variant.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-tweaks-variant"

#include "config.h"

#include "ide-tweaks-variant.h"

GType
_ide_tweaks_variant_type_to_gtype (const GVariantType *variant_type)
{
  if (variant_type == NULL)
    return G_TYPE_INVALID;

#define MAP_VARIANT_TYPE_TO_GTYPE(vtype, gtype)     \
  G_STMT_START {                                    \
    if (g_variant_type_equal (variant_type, vtype)) \
      return gtype;                                 \
  } G_STMT_END

  MAP_VARIANT_TYPE_TO_GTYPE (G_VARIANT_TYPE_BOOLEAN, G_TYPE_BOOLEAN);
  MAP_VARIANT_TYPE_TO_GTYPE (G_VARIANT_TYPE_BYTE, G_TYPE_UCHAR);
  MAP_VARIANT_TYPE_TO_GTYPE (G_VARIANT_TYPE_DOUBLE, G_TYPE_DOUBLE);
  MAP_VARIANT_TYPE_TO_GTYPE (G_VARIANT_TYPE_INT32, G_TYPE_INT);
  MAP_VARIANT_TYPE_TO_GTYPE (G_VARIANT_TYPE_INT64, G_TYPE_INT64);
  MAP_VARIANT_TYPE_TO_GTYPE (G_VARIANT_TYPE_STRING, G_TYPE_STRING);
  MAP_VARIANT_TYPE_TO_GTYPE (G_VARIANT_TYPE_STRING_ARRAY, G_TYPE_STRV);
  MAP_VARIANT_TYPE_TO_GTYPE (G_VARIANT_TYPE_UINT32, G_TYPE_UINT);
  MAP_VARIANT_TYPE_TO_GTYPE (G_VARIANT_TYPE_UINT64, G_TYPE_UINT64);
  MAP_VARIANT_TYPE_TO_GTYPE (G_VARIANT_TYPE_VARIANT, G_TYPE_VARIANT);

#undef MAP_VARIANT_TYPE_TO_GTYPE

  return G_TYPE_INVALID;
}

const GVariantType *
_ide_tweaks_gtype_to_variant_type (GType type)
{
  if (type == G_TYPE_INVALID)
    return NULL;

#define MAP_GTYPE_TO_VARIANT_TYPE(vtype, gtype) \
  G_STMT_START {                                \
    if (g_type_is_a (type, gtype))              \
      return vtype;                             \
  } G_STMT_END

  MAP_GTYPE_TO_VARIANT_TYPE (G_VARIANT_TYPE_BOOLEAN, G_TYPE_BOOLEAN);
  MAP_GTYPE_TO_VARIANT_TYPE (G_VARIANT_TYPE_BYTE, G_TYPE_UCHAR);
  MAP_GTYPE_TO_VARIANT_TYPE (G_VARIANT_TYPE_DOUBLE, G_TYPE_DOUBLE);
  MAP_GTYPE_TO_VARIANT_TYPE (G_VARIANT_TYPE_INT32, G_TYPE_INT);
  MAP_GTYPE_TO_VARIANT_TYPE (G_VARIANT_TYPE_INT64, G_TYPE_INT64);
  MAP_GTYPE_TO_VARIANT_TYPE (G_VARIANT_TYPE_STRING, G_TYPE_STRING);
  MAP_GTYPE_TO_VARIANT_TYPE (G_VARIANT_TYPE_STRING_ARRAY, G_TYPE_STRV);
  MAP_GTYPE_TO_VARIANT_TYPE (G_VARIANT_TYPE_UINT32, G_TYPE_UINT);
  MAP_GTYPE_TO_VARIANT_TYPE (G_VARIANT_TYPE_UINT64, G_TYPE_UINT64);
  MAP_GTYPE_TO_VARIANT_TYPE (G_VARIANT_TYPE_VARIANT, G_TYPE_VARIANT);

#undef MAP_GTYPE_TO_VARIANT_TYPE

  return NULL;
}
