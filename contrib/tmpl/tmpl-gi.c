/* tmpl-gi.c
 *
 * Copyright PyGObject authors
 *           Christian Hergert <chergert@redhat.com>
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

#include "tmpl-error.h"
#include "tmpl-gi-private.h"

G_DEFINE_POINTER_TYPE (GisTypelib, tmpl_typelib)

#define return_type_mismatch(value, type)                          \
  G_STMT_START {                                                   \
      g_set_error (error,                                          \
                   TMPL_ERROR,                                     \
                   TMPL_ERROR_TYPE_MISMATCH,                       \
                   "Expected %s, got %s",                          \
                   g_type_name (type), G_VALUE_TYPE_NAME (value)); \
      return FALSE;                                                \
  } G_STMT_END

#define return_if_not_type(value, type)  \
  G_STMT_START {                         \
    if (!G_VALUE_HOLDS (value, type))    \
      return_type_mismatch(value, type); \
  } G_STMT_END

gboolean
tmpl_gi_argument_from_g_value (const GValue  *value,
                               GITypeInfo    *type_info,
                               GIArgument    *arg,
                               GError       **error)
{
  GITypeTag type_tag = g_type_info_get_tag (type_info);

  /* For the long handling: long can be equivalent to
   * int32 or int64, depending on the architecture, but
   * gi doesn't tell us (and same for ulong)
   */

  if (G_VALUE_TYPE (value) == G_TYPE_INVALID)
    {
      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_TYPE_MISMATCH,
                   "uninitialized value");
      return FALSE;
    }

  switch (type_tag)
    {
    case GI_TYPE_TAG_BOOLEAN:
      return_if_not_type (value, G_TYPE_BOOLEAN);
      arg->v_boolean = g_value_get_boolean (value);
      return TRUE;

    case GI_TYPE_TAG_INT8:
      return_if_not_type (value, G_TYPE_CHAR);
      arg->v_int8 = g_value_get_schar (value);
      return TRUE;

    case GI_TYPE_TAG_INT16:
    case GI_TYPE_TAG_INT32:
      if (G_VALUE_HOLDS (value, G_TYPE_LONG))
        arg->v_int = g_value_get_long (value);
      else if (G_VALUE_HOLDS (value, G_TYPE_INT))
        arg->v_int = g_value_get_int (value);
      else
        return_type_mismatch (value, G_TYPE_INT);
      return TRUE;

    case GI_TYPE_TAG_INT64:
      if (G_VALUE_HOLDS (value, G_TYPE_LONG))
        arg->v_int64 = g_value_get_long (value);
      else if (G_VALUE_HOLDS (value, G_TYPE_INT64))
        arg->v_int64 = g_value_get_int64 (value);
      else
        return_type_mismatch (value, G_TYPE_INT64);
      return TRUE;

    case GI_TYPE_TAG_UINT8:
      if (G_VALUE_HOLDS (value, G_TYPE_UCHAR))
        arg->v_uint8 = g_value_get_uchar (value);
      else
        return_type_mismatch (value, G_TYPE_UCHAR);
      return TRUE;

    case GI_TYPE_TAG_UINT16:
    case GI_TYPE_TAG_UINT32:
      if (G_VALUE_HOLDS (value, G_TYPE_ULONG))
        arg->v_uint = g_value_get_ulong (value);
      else if (G_VALUE_HOLDS (value, G_TYPE_UINT))
        arg->v_uint = g_value_get_uint (value);
      else
        return_type_mismatch (value, G_TYPE_UINT);
      return TRUE;

    case GI_TYPE_TAG_UINT64:
      if (G_VALUE_HOLDS (value, G_TYPE_ULONG))
        arg->v_uint64 = g_value_get_ulong (value);
      else if (G_VALUE_HOLDS (value, G_TYPE_UINT64))
        arg->v_uint64 = g_value_get_uint64 (value);
      else
        return_type_mismatch (value, G_TYPE_UINT64);
      return TRUE;

    case GI_TYPE_TAG_UNICHAR:
      if (G_VALUE_HOLDS (value, G_TYPE_CHAR))
        arg->v_uint32 = g_value_get_schar (value);
      else
        return_type_mismatch (value, G_TYPE_CHAR);
      return TRUE;

    case GI_TYPE_TAG_FLOAT:
      if (G_VALUE_HOLDS (value, G_TYPE_FLOAT))
        arg->v_float = g_value_get_float (value);
      else
        return_type_mismatch (value, G_TYPE_FLOAT);
      return TRUE;

    case GI_TYPE_TAG_DOUBLE:
      if (G_VALUE_HOLDS (value, G_TYPE_DOUBLE))
        arg->v_double = g_value_get_double (value);
      else
        return_type_mismatch (value, G_TYPE_DOUBLE);
      return TRUE;

    case GI_TYPE_TAG_GTYPE:
      if (G_VALUE_HOLDS (value, G_TYPE_GTYPE))
        arg->v_long = g_value_get_gtype (value);
      else
        return_type_mismatch (value, G_TYPE_GTYPE);
      return TRUE;

    case GI_TYPE_TAG_UTF8:
    case GI_TYPE_TAG_FILENAME:
      /* Callers are responsible for ensuring the GValue stays alive
       * long enough for the string to be copied. */
      if (G_VALUE_HOLDS (value, G_TYPE_STRING))
        arg->v_string = (char *)g_value_get_string (value);
      else
        return_type_mismatch (value, G_TYPE_STRING);
      return TRUE;

    case GI_TYPE_TAG_GLIST:
    case GI_TYPE_TAG_GSLIST:
    case GI_TYPE_TAG_ARRAY:
    case GI_TYPE_TAG_GHASH:
      if (G_VALUE_HOLDS_BOXED (value))
        arg->v_pointer = g_value_get_boxed (value);
      else if (G_VALUE_HOLDS (value, G_TYPE_POINTER))
        /* e. g. GSettings::change-event */
        arg->v_pointer = g_value_get_pointer (value);
      else
        return_type_mismatch (value, G_TYPE_POINTER);
      return TRUE;

    case GI_TYPE_TAG_INTERFACE:
      {
        GIBaseInfo *info;
        GIInfoType info_type;

        info = g_type_info_get_interface (type_info);
        info_type = g_base_info_get_type (info);

        g_base_info_unref (info);

        switch (info_type)
          {
          case GI_INFO_TYPE_FLAGS:
            if (G_VALUE_HOLDS (value, G_TYPE_FLAGS))
              arg->v_uint = g_value_get_flags (value);
            else
              return_type_mismatch (value, G_TYPE_FLAGS);
            return TRUE;

          case GI_INFO_TYPE_ENUM:
            arg->v_int = g_value_get_enum (value);
            return TRUE;

          case GI_INFO_TYPE_INTERFACE:
          case GI_INFO_TYPE_OBJECT:
            if (G_VALUE_HOLDS_PARAM (value))
              arg->v_pointer = g_value_get_param (value);
            else
              arg->v_pointer = g_value_get_object (value);
            return TRUE;

          case GI_INFO_TYPE_BOXED:
          case GI_INFO_TYPE_STRUCT:
          case GI_INFO_TYPE_UNION:
            if (G_VALUE_HOLDS (value, G_TYPE_BOXED))
              arg->v_pointer = g_value_get_boxed (value);
            else if (G_VALUE_HOLDS (value, G_TYPE_VARIANT))
              arg->v_pointer = g_value_get_variant (value);
            else if (G_VALUE_HOLDS (value, G_TYPE_POINTER))
              arg->v_pointer = g_value_get_pointer (value);
            else
              {
                g_set_error (error,
                             TMPL_ERROR,
                             TMPL_ERROR_NOT_IMPLEMENTED,
                             "Converting GValue's of type '%s' is not implemented.",
                             g_type_name (G_VALUE_TYPE (value)));
                return FALSE;
              }
            return TRUE;

          case GI_INFO_TYPE_INVALID:
          case GI_INFO_TYPE_INVALID_0:
          case GI_INFO_TYPE_FUNCTION:
          case GI_INFO_TYPE_CONSTANT:
          case GI_INFO_TYPE_CALLBACK:
          case GI_INFO_TYPE_VALUE:
          case GI_INFO_TYPE_SIGNAL:
          case GI_INFO_TYPE_VFUNC:
          case GI_INFO_TYPE_PROPERTY:
          case GI_INFO_TYPE_FIELD:
          case GI_INFO_TYPE_ARG:
          case GI_INFO_TYPE_TYPE:
          case GI_INFO_TYPE_UNRESOLVED:
          default:
            g_set_error (error,
                         TMPL_ERROR,
                         TMPL_ERROR_NOT_IMPLEMENTED,
                         "Converting GValue's of type '%s' is not implemented.",
                         g_info_type_to_string (info_type));
            return FALSE;
          }

        g_assert_not_reached ();
      }

    case GI_TYPE_TAG_ERROR:
      if (G_VALUE_HOLDS (value, G_TYPE_ERROR))
        arg->v_pointer = g_value_get_boxed (value);
      else
        return_type_mismatch (value, G_TYPE_ERROR);
      return TRUE;

    case GI_TYPE_TAG_VOID:
      if (G_VALUE_HOLDS (value, G_TYPE_POINTER))
        arg->v_pointer = g_value_get_pointer (value);
      else
        return_type_mismatch (value, G_TYPE_POINTER);
      return TRUE;

    default:
      break;
    }

  g_set_error (error,
               TMPL_ERROR,
               TMPL_ERROR_NOT_IMPLEMENTED,
               "Unknown marshaling error.");

  return FALSE;
}

gboolean
tmpl_gi_argument_to_g_value (GValue      *value,
                             GITypeInfo  *type_info,
                             GIArgument  *arg,
                             GError     **error)
{
  GITypeTag tag;

  g_assert (value != NULL);
  g_assert (type_info != NULL);
  g_assert (arg != NULL);

  tag = g_type_info_get_tag (type_info);

  if (tag == GI_TYPE_TAG_INTERFACE)
    {
      g_warning ("TODO: proper return marshaling");
    }

  switch (tag)
    {
    case GI_TYPE_TAG_VOID:
      /* No type info */
      return TRUE;

    case GI_TYPE_TAG_BOOLEAN:
      g_value_init (value, G_TYPE_BOOLEAN);
      g_value_set_boolean (value, arg->v_boolean);
      return TRUE;

    case GI_TYPE_TAG_INT8:
      g_value_init (value, G_TYPE_INT);
      g_value_set_int (value, arg->v_int8);
      return TRUE;

    case GI_TYPE_TAG_INT16:
      g_value_init (value, G_TYPE_INT);
      g_value_set_int (value, arg->v_int16);
      return TRUE;

    case GI_TYPE_TAG_INT32:
      g_value_init (value, G_TYPE_INT);
      g_value_set_int (value, arg->v_int32);
      return TRUE;

    case GI_TYPE_TAG_INT64:
      g_value_init (value, G_TYPE_INT64);
      g_value_set_int64 (value, arg->v_int64);
      return TRUE;

    case GI_TYPE_TAG_UINT8:
      g_value_init (value, G_TYPE_UINT);
      g_value_set_uint (value, arg->v_uint8);
      return TRUE;

    case GI_TYPE_TAG_UINT16:
      g_value_init (value, G_TYPE_UINT);
      g_value_set_uint (value, arg->v_uint16);
      return TRUE;

    case GI_TYPE_TAG_UINT32:
      g_value_init (value, G_TYPE_UINT);
      g_value_set_uint (value, arg->v_uint32);
      return TRUE;

    case GI_TYPE_TAG_UINT64:
      g_value_init (value, G_TYPE_UINT64);
      g_value_set_uint64 (value, arg->v_uint64);
      return TRUE;

    case GI_TYPE_TAG_FLOAT:
      g_value_init (value, G_TYPE_FLOAT);
      g_value_set_float (value, arg->v_float);
      return TRUE;

    case GI_TYPE_TAG_DOUBLE:
      g_value_init (value, G_TYPE_DOUBLE);
      g_value_set_float (value, arg->v_double);
      return TRUE;

    case GI_TYPE_TAG_GTYPE:
      g_value_init (value, G_TYPE_GTYPE);
      g_value_set_gtype (value, arg->v_long);
      return TRUE;

    case GI_TYPE_TAG_UTF8:
    case GI_TYPE_TAG_FILENAME:
      g_value_init (value, G_TYPE_STRING);
      g_value_set_string (value, arg->v_string);
      return TRUE;

    case GI_TYPE_TAG_ARRAY:
    case GI_TYPE_TAG_INTERFACE:
    case GI_TYPE_TAG_GLIST:
    case GI_TYPE_TAG_GSLIST:
    case GI_TYPE_TAG_GHASH:
    case GI_TYPE_TAG_ERROR:
      break;

    case GI_TYPE_TAG_UNICHAR:
      {
        gchar str[8];

        str [g_unichar_to_utf8 (arg->v_int32, str)] = '\0';
        g_value_init (value, G_TYPE_STRING);
        g_value_set_string (value, str);

        return TRUE;
      }

    default:
      break;
    }

  g_set_error (error,
               TMPL_ERROR,
               TMPL_ERROR_TYPE_MISMATCH,
               "Failed to decode value from GObject Introspection");

  return FALSE;
}
