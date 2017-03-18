/* jsonrpc-message.c
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "jsonrpc-message"

#include <string.h>

#include "jsonrpc-message.h"

#if 0
# define ENTRY     do { g_print (" ENTRY: %s(): %d\n", G_STRFUNC, __LINE__); } while (0)
# define RETURN(r) do { g_print ("RETURN: %s(): %d\n", G_STRFUNC, __LINE__); return r; } while (0)
# define EXIT      do { g_print ("  EXIT: %s(): %d\n", G_STRFUNC, __LINE__); return; } while (0)
#else
# define ENTRY     do { } while (0)
# define RETURN(r) do { return r; } while (0)
# define EXIT      do { return; } while (0)
#endif

#define COMPARE_MAGIC(_any,_magic) \
  (strncmp ((_any)->magic.bytes, \
            _JSONRPC_MESSAGE_##_magic##_MAGIC, \
            sizeof ((_any)->magic.bytes)) == 0)

#define IS_PUT_STRING(_any)  COMPARE_MAGIC(_any, PUT_STRING)
#define IS_PUT_INT32(_any)   COMPARE_MAGIC(_any, PUT_INT32)
#define IS_PUT_INT64(_any)   COMPARE_MAGIC(_any, PUT_INT64)
#define IS_PUT_BOOLEAN(_any) COMPARE_MAGIC(_any, PUT_BOOLEAN)
#define IS_PUT_DOUBLE(_any)  COMPARE_MAGIC(_any, PUT_DOUBLE)

#define IS_GET_STRING(_any)  COMPARE_MAGIC(_any, GET_STRING)
#define IS_GET_INT32(_any)   COMPARE_MAGIC(_any, GET_INT32)
#define IS_GET_INT64(_any)   COMPARE_MAGIC(_any, GET_INT64)
#define IS_GET_BOOLEAN(_any) COMPARE_MAGIC(_any, GET_BOOLEAN)
#define IS_GET_DOUBLE(_any)  COMPARE_MAGIC(_any, GET_DOUBLE)
#define IS_GET_ITER(_any)    COMPARE_MAGIC(_any, GET_ITER)
#define IS_GET_DICT(_any)    COMPARE_MAGIC(_any, GET_DICT)
#define IS_GET_VARIANT(_any) COMPARE_MAGIC(_any, GET_VARIANT)

static void     jsonrpc_message_build_object   (GVariantBuilder *builder,
                                                gpointer         param,
                                                va_list          args);
static void     jsonrpc_message_build_array    (GVariantBuilder *builder,
                                                gpointer         param,
                                                va_list          args);
static gboolean jsonrpc_message_parse_object   (GVariantDict    *dict,
                                                gpointer         param,
                                                va_list          args);
static gboolean jsonrpc_message_parse_array_va (GVariantIter    *iter,
                                                gpointer         param,
                                                va_list          args);

static void
jsonrpc_message_build_object (GVariantBuilder *builder,
                              gpointer         param,
                              va_list          args)
{
  JsonrpcMessageAny *keyptr = param;
  JsonrpcMessageAny *valptr;
  const char *key;

  ENTRY;

  /*
   * First we need to get the key for this item. If we have reached
   * the '}', then we either have an empty {} or got here via
   * recursion to read the next key/val pair.
   */

  if (!keyptr || keyptr->magic.bytes[0] == '}')
    EXIT;

  g_variant_builder_open (builder, G_VARIANT_TYPE ("{sv}"));

  /*
   * Either this is a string wrapped in JSONRPC_MESSAGE_PUT_STRING() or
   * we assume it is a raw key name like "foo".
   */
  if (IS_PUT_STRING (keyptr))
    key = ((JsonrpcMessagePutString *)keyptr)->val;
  else
    key = (const char *)keyptr;

  g_variant_builder_add (builder, "s", key);

  /*
   * Now try to read the value for the key/val pair.
   */
  valptr = param = va_arg (args, gpointer);

  g_variant_builder_open (builder, G_VARIANT_TYPE ("v"));

  switch (valptr->magic.bytes[0])
    {
    case '{':
      g_variant_builder_open (builder, G_VARIANT_TYPE ("a{sv}"));
      param = va_arg (args, gpointer);
      jsonrpc_message_build_object (builder, param, args);
      g_variant_builder_close (builder);
      break;

    case '[':
      g_variant_builder_open (builder, G_VARIANT_TYPE ("av"));
      param = va_arg (args, gpointer);
      jsonrpc_message_build_array (builder, param, args);
      g_variant_builder_close (builder);
      break;

    case ']':
    case '}':
      g_return_if_reached ();
      break;

    default:
      if (IS_PUT_STRING (valptr))
        g_variant_builder_add (builder, "s", ((JsonrpcMessagePutString *)valptr)->val);
      else if (IS_PUT_INT32 (valptr))
        g_variant_builder_add (builder, "i", ((JsonrpcMessagePutInt32 *)valptr)->val);
      else if (IS_PUT_INT64 (valptr))
        g_variant_builder_add (builder, "x", ((JsonrpcMessagePutInt64 *)valptr)->val);
      else if (IS_PUT_BOOLEAN (valptr))
        g_variant_builder_add (builder, "b", ((JsonrpcMessagePutBoolean *)valptr)->val);
      else if (IS_PUT_DOUBLE (valptr))
        g_variant_builder_add (builder, "d", ((JsonrpcMessagePutDouble *)valptr)->val);
      else
        g_variant_builder_add (builder, "s", (const char *)valptr);
      break;
    }

  g_variant_builder_close (builder);
  g_variant_builder_close (builder);

  /*
   * Try to build the next field in the object if there is one.
   */
  param = va_arg (args, gpointer);
  jsonrpc_message_build_object (builder, param, args);

  EXIT;
}

static void
jsonrpc_message_build_array (GVariantBuilder *builder,
                             gpointer         param,
                             va_list          args)
{
  JsonrpcMessageAny *valptr = param;

  ENTRY;

  /* If we have the end of the array, we're done */
  if (valptr->magic.bytes[0] == ']')
    EXIT;

  g_variant_builder_open (builder, G_VARIANT_TYPE ("v"));

  switch (valptr->magic.bytes[0])
    {
    case '{':
      g_variant_builder_open (builder, G_VARIANT_TYPE ("a{sv}"));
      param = va_arg (args, gpointer);
      jsonrpc_message_build_object (builder, param, args);
      g_variant_builder_close (builder);
      break;

    case '[':
      g_variant_builder_open (builder, G_VARIANT_TYPE ("av"));
      param = va_arg (args, gpointer);
      jsonrpc_message_build_array (builder, param, args);
      g_variant_builder_close (builder);
      break;

    case ']':
    case '}':
      g_return_if_reached ();
      break;

    default:
      if (IS_PUT_STRING (valptr))
        g_variant_builder_add (builder, "s", ((JsonrpcMessagePutString *)valptr)->val);
      else if (IS_PUT_INT32 (valptr))
        g_variant_builder_add (builder, "i", ((JsonrpcMessagePutInt32 *)valptr)->val);
      else if (IS_PUT_INT64 (valptr))
        g_variant_builder_add (builder, "x", ((JsonrpcMessagePutInt64 *)valptr)->val);
      else if (IS_PUT_BOOLEAN (valptr))
        g_variant_builder_add (builder, "b", ((JsonrpcMessagePutBoolean *)valptr)->val);
      else if (IS_PUT_DOUBLE (valptr))
        g_variant_builder_add (builder, "d", ((JsonrpcMessagePutDouble *)valptr)->val);
      else
        g_variant_builder_add (builder, "s", (const char *)valptr);
      break;
    }

  g_variant_builder_close (builder);

  param = va_arg (args, gpointer);
  jsonrpc_message_build_array (builder, param, args);

  EXIT;
}

static GVariant *
jsonrpc_message_new_valist (gpointer first_param,
                            va_list  args)
{
  GVariantBuilder builder;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));

  if (first_param != NULL)
    jsonrpc_message_build_object (&builder, first_param, args);

  return g_variant_ref_sink (g_variant_builder_end (&builder));
}

GVariant *
jsonrpc_message_new (gpointer first_param,
                     ...)
{
  GVariant *ret;
  va_list args;

  g_return_val_if_fail (first_param != NULL, NULL);

  va_start (args, first_param);
  ret = jsonrpc_message_new_valist (first_param, args);
  va_end (args);

  return ret;
}

static gboolean
jsonrpc_message_parse_object (GVariantDict *dict,
                              gpointer      param,
                              va_list       args)
{
  JsonrpcMessageAny *valptr;
  const char *key = param;
  gboolean ret = FALSE;

  ENTRY;

  g_assert (dict != NULL);

  if (key == NULL || key[0] == '}')
    RETURN (TRUE);

  valptr = va_arg (args, gpointer);

  if (valptr == NULL)
    g_error ("got unexpected NULL for key %s", key);

  if (valptr->magic.bytes[0] == '{' || IS_GET_DICT (valptr))
    {
      g_autoptr(GVariant) value = NULL;

      if (NULL != (value = g_variant_dict_lookup_value (dict, key, G_VARIANT_TYPE ("a{sv}"))))
        {
          g_autoptr(GVariantDict) subdict = NULL;

          subdict = g_variant_dict_new (value);

          g_assert (subdict != NULL);

          if (IS_GET_DICT (valptr))
            ret = !!(*((JsonrpcMessageGetDict *)valptr)->dictptr = g_steal_pointer (&subdict));
          else
            {
              param = va_arg (args, gpointer);
              ret = jsonrpc_message_parse_object (subdict, param, args);
            }
        }
    }
  else if (valptr->magic.bytes[0] == '[' || IS_GET_ITER (valptr))
    {
      g_autoptr(GVariantIter) subiter = NULL;

      if (g_variant_dict_lookup (dict, key, "av", &subiter))
        {
          if (IS_GET_ITER (valptr))
            ret = !!(*((JsonrpcMessageGetIter *)valptr)->iterptr = g_steal_pointer (&subiter));
          else
            {
              param = va_arg (args, gpointer);
              ret = jsonrpc_message_parse_array_va (subiter, param, args);
            }
        }
    }
  else if (IS_GET_VARIANT (valptr))
    ret = !!(*((JsonrpcMessageGetVariant *)valptr)->variantptr = g_variant_dict_lookup_value (dict, key, NULL));
  else if (IS_GET_STRING (valptr))
    ret = g_variant_dict_lookup (dict, key, "&s", ((JsonrpcMessageGetString *)valptr)->valptr);
  else if (IS_GET_INT32 (valptr))
    ret = g_variant_dict_lookup (dict, key, "i", ((JsonrpcMessageGetInt32 *)valptr)->valptr);
  else if (IS_GET_INT64 (valptr))
    ret = g_variant_dict_lookup (dict, key, "x", ((JsonrpcMessageGetInt64 *)valptr)->valptr);
  else if (IS_GET_BOOLEAN (valptr))
    ret = g_variant_dict_lookup (dict, key, "b", ((JsonrpcMessageGetBoolean *)valptr)->valptr);
  else if (IS_GET_DOUBLE (valptr))
    ret = g_variant_dict_lookup (dict, key, "d", ((JsonrpcMessageGetDouble *)valptr)->valptr);
  else
    {
      /* Assume the string is a raw string, so compare for equality */
      const gchar *valstr = NULL;
      ret = g_variant_dict_lookup (dict, key, "&s", &valstr) && g_strcmp0 (valstr, (const char *)valptr) == 0;
    }

  if (!ret || !param)
    RETURN (ret);

  /* If we succeeded, try to read the next field */
  param = va_arg (args, gpointer);
  ret = jsonrpc_message_parse_object (dict, param, args);

  RETURN (ret);
}

static gboolean
jsonrpc_message_parse_array_va (GVariantIter *iter,
                                gpointer      param,
                                va_list       args)
{
  JsonrpcMessageAny *valptr = param;
  g_autoptr(GVariant) value = NULL;
  gboolean ret = FALSE;

  ENTRY;

  g_assert (iter != NULL);

  if (valptr == NULL || valptr->magic.bytes[0] == ']')
    RETURN (TRUE);

  if (!g_variant_iter_next (iter, "v", &value))
    RETURN (FALSE);

  if (valptr->magic.bytes[0] == '{' || IS_GET_DICT (valptr))
    {
      if (g_variant_is_of_type (value, G_VARIANT_TYPE ("a{sv}")))
        {
          g_autoptr(GVariantDict) subdict = NULL;

          subdict = g_variant_dict_new (value);

          if (IS_GET_ITER (valptr))
            ret = !!(*((JsonrpcMessageGetDict *)valptr)->dictptr = g_steal_pointer (&subdict));
          else
            {
              param = va_arg (args, gpointer);
              ret = jsonrpc_message_parse_object (subdict, param, args);
            }
        }
    }
  else if (valptr->magic.bytes[0] == '[' || IS_GET_ITER (valptr))
    {
      if (g_variant_is_of_type (value, G_VARIANT_TYPE ("av")))
        {
          g_autoptr(GVariantIter) subiter = NULL;

          subiter = g_variant_iter_new (value);

          if (IS_GET_ITER (valptr))
            ret = !!(*((JsonrpcMessageGetIter *)valptr)->iterptr = g_steal_pointer (&subiter));
          else
            {
              param = va_arg (args, gpointer);
              ret = jsonrpc_message_parse_array_va (subiter, param, args);
            }
        }
    }
  else if (IS_GET_VARIANT (valptr))
    {
      ret = !!(*((JsonrpcMessageGetVariant *)valptr)->variantptr = g_steal_pointer (&value));
    }
  else if (IS_GET_STRING (valptr))
    {
      if ((ret = g_variant_is_of_type (value, G_VARIANT_TYPE_STRING)))
        *((JsonrpcMessageGetString *)valptr)->valptr = g_variant_get_string (value, NULL);
    }
  else if (IS_GET_INT32 (valptr))
    {
      if ((ret = g_variant_is_of_type (value, G_VARIANT_TYPE_INT32)))
        *((JsonrpcMessageGetInt32 *)valptr)->valptr = g_variant_get_int32 (value);
    }
  else if (IS_GET_INT64 (valptr))
    {
      if ((ret = g_variant_is_of_type (value, G_VARIANT_TYPE_INT64)))
        *((JsonrpcMessageGetInt64 *)valptr)->valptr = g_variant_get_int64 (value);
    }
  else if (IS_GET_BOOLEAN (valptr))
    {
      if ((ret = g_variant_is_of_type (value, G_VARIANT_TYPE_BOOLEAN)))
        *((JsonrpcMessageGetBoolean *)valptr)->valptr = g_variant_get_boolean (value);
    }
  else if (IS_GET_DOUBLE (valptr))
    {
      if ((ret = g_variant_is_of_type (value, G_VARIANT_TYPE_DOUBLE)))
        *((JsonrpcMessageGetDouble *)valptr)->valptr = g_variant_get_double (value);
    }
  else
    {
      /* Assume the string is a raw string, so compare for equality */
      ret = g_variant_is_of_type (value, G_VARIANT_TYPE_STRING) &&
            (0 == g_strcmp0 (g_variant_get_string (value, NULL), (const gchar *)valptr));
    }

  if (!ret || !param)
    RETURN (ret);

  /* If we succeeded, try to read the next element */
  param = va_arg (args, gpointer);
  ret = jsonrpc_message_parse_array_va (iter, param, args);

  RETURN (ret);
}

gboolean
jsonrpc_message_parse_array (GVariantIter *iter, ...)
{
  va_list args;
  gpointer param;
  gboolean ret = FALSE;

  g_return_val_if_fail (iter != NULL, FALSE);

  va_start (args, iter);
  param = va_arg (args, gpointer);
  if (param)
    ret = jsonrpc_message_parse_array_va (iter, param, args);
  va_end (args);

  return ret;
}

static gboolean
jsonrpc_message_parse_valist (GVariant *message,
                              va_list   args)
{
  gpointer param;
  gboolean ret = FALSE;

  g_assert (message != NULL);
  g_assert (g_variant_is_of_type (message, G_VARIANT_TYPE ("a{sv}")));

  param = va_arg (args, gpointer);

  if (param != NULL)
    {
      GVariantDict dict;

      g_variant_dict_init (&dict, message);
      ret = jsonrpc_message_parse_object (&dict, param, args);
      g_variant_dict_clear (&dict);
    }

  return ret;
}

gboolean
jsonrpc_message_parse (GVariant *message, ...)
{
  gboolean ret;
  va_list args;

  g_return_val_if_fail (message != NULL, FALSE);
  g_return_val_if_fail (g_variant_is_of_type (message, G_VARIANT_TYPE ("a{sv}")), FALSE);

  va_start (args, message);
  ret = jsonrpc_message_parse_valist (message, args);
  va_end (args);

  return ret;
}
