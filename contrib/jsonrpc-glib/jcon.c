/* jcon.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

/* Copyright 2009-2013 MongoDB, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define G_LOG_DOMAIN "jcon"

#include <stdarg.h>
#include <string.h>

#include "jcon.h"

#define STACK_DEPTH 50

typedef union
{
  const gchar *v_string;
  gdouble      v_double;
  JsonObject  *v_object;
  JsonArray   *v_array;
  JsonNode    *v_node;
  gboolean     v_boolean;
  gint         v_int;
} JconAppend;

typedef union
{
  const gchar  *v_key;
  const gchar **v_string;
  gdouble      *v_double;
  JsonObject  **v_object;
  JsonArray   **v_array;
  JsonNode    **v_node;
  gboolean     *v_boolean;
  gint         *v_int;
} JconExtract;

const char *
jcon_magic  (void)
{
  return "THIS_IS_JCON_MAGIC";
}

const char *
jcone_magic (void)
{
  return "THIS_IS_JCONE_MAGIC";
}

static JsonNode *
jcon_append_to_node (JconType    type,
                     JconAppend *val)
{
  JsonNode *node;

  switch (type)
    {
    case JCON_TYPE_STRING:
      node = json_node_new (JSON_NODE_VALUE);
      json_node_set_string (node, val->v_string);
      return node;

    case JCON_TYPE_DOUBLE:
      node = json_node_new (JSON_NODE_VALUE);
      json_node_set_double (node, val->v_double);
      return node;

    case JCON_TYPE_BOOLEAN:
      node = json_node_new (JSON_NODE_VALUE);
      json_node_set_boolean (node, val->v_boolean);
      return node;

    case JCON_TYPE_NULL:
      return json_node_new (JSON_NODE_NULL);

    case JCON_TYPE_INT:
      node = json_node_new (JSON_NODE_VALUE);
      json_node_set_int (node, val->v_int);
      return node;

    case JCON_TYPE_NODE:
      return json_node_copy (val->v_node);

    case JCON_TYPE_ARRAY:
      node = json_node_new (JSON_NODE_ARRAY);
      json_node_set_array (node, val->v_array);
      return node;

    case JCON_TYPE_OBJECT:
      node = json_node_new (JSON_NODE_OBJECT);
      json_node_set_object (node, val->v_object);
      return node;

    case JCON_TYPE_OBJECT_START:
    case JCON_TYPE_OBJECT_END:
    case JCON_TYPE_ARRAY_START:
    case JCON_TYPE_ARRAY_END:
    case JCON_TYPE_END:
    case JCON_TYPE_RAW:
    default:
      return NULL;
    }
}

static JconType
jcon_append_tokenize (va_list    *ap,
                      JconAppend *u)
{
  const gchar *mark;
  JconType type;

  g_assert (ap != NULL);
  g_assert (u != NULL);

  /* Consumes ap, storing output values into u and returning the type of the
   * captured token.
   *
   * The basic workflow goes like this:
   *
   * 1. Look at the current arg.  It will be a gchar *
   *    a. If it's a NULL, we're done processing.
   *    b. If it's JCON_MAGIC (a symbol with storage in this module)
   *       I. The next token is the type
   *       II. The type specifies how many args to eat and their types
   *    c. Otherwise it's either recursion related or a raw string
   *       I. If the first byte is '{', '}', '[', or ']' pass back an
   *          appropriate recursion token
   *       II. If not, just call it a v_string token and pass that back
   */

  memset (u, 0, sizeof *u);

  mark = va_arg (*ap, const gchar *);

  g_assert (mark != JCONE_MAGIC);

  if (mark == NULL)
    {
      type = JCON_TYPE_END;
    }
  else if (mark == JCON_MAGIC)
    {
      type = va_arg (*ap, JconType);

      switch (type)
        {
        case JCON_TYPE_STRING:
          u->v_string = va_arg (*ap, const gchar *);
          break;

        case JCON_TYPE_DOUBLE:
          u->v_double = va_arg (*ap, double);
          break;

        case JCON_TYPE_NODE:
          u->v_node = va_arg (*ap, JsonNode *);
          break;

        case JCON_TYPE_OBJECT:
          u->v_object = va_arg (*ap, JsonObject *);
          break;

        case JCON_TYPE_ARRAY:
          u->v_array = va_arg (*ap, JsonArray *);
          break;

        case JCON_TYPE_BOOLEAN:
          u->v_boolean = va_arg (*ap, gboolean);
          break;

        case JCON_TYPE_NULL:
          break;

        case JCON_TYPE_INT:
          u->v_int = va_arg (*ap, gint);
          break;

        case JCON_TYPE_ARRAY_START:
        case JCON_TYPE_ARRAY_END:
        case JCON_TYPE_OBJECT_START:
        case JCON_TYPE_OBJECT_END:
        case JCON_TYPE_END:
        case JCON_TYPE_RAW:
        default:
          g_assert_not_reached ();
          break;
      }
    }
  else
    {
      switch (mark[0])
        {
        case '{':
          type = JCON_TYPE_OBJECT_START;
          break;

        case '}':
          type = JCON_TYPE_OBJECT_END;
          break;

        case '[':
          type = JCON_TYPE_ARRAY_START;
          break;

        case ']':
          type = JCON_TYPE_ARRAY_END;
          break;

        default:
          type = JCON_TYPE_STRING;
          u->v_string = mark;
          break;
        }
    }

  return type;
}

static void
jcon_append_va_list (JsonNode *node,
                     va_list  *args)
{
  g_assert (JSON_NODE_HOLDS_OBJECT (node));
  g_assert (args != NULL);

  for (;;)
    {
      const gchar *key = NULL;
      JconAppend val = { 0 };
      JconType type;

      g_assert (node != NULL);

      if (!JSON_NODE_HOLDS_ARRAY (node))
        {
          type = jcon_append_tokenize (args, &val);

          if (type == JCON_TYPE_END)
            return;

          if (type == JCON_TYPE_OBJECT_END)
            {
              node = json_node_get_parent (node);
              continue;
            }

          if (type != JCON_TYPE_STRING)
            g_error ("string keys are required for objects");

          key = val.v_string;
        }

      type = jcon_append_tokenize (args, &val);

      if (type == JCON_TYPE_END)
        g_error ("implausable time to reach end token");

      if (type == JCON_TYPE_OBJECT_START)
        {
          JsonNode *child_node = json_node_new (JSON_NODE_OBJECT);
          JsonObject *child_object = json_object_new ();

          json_node_take_object (child_node, child_object);

          if (JSON_NODE_HOLDS_ARRAY (node))
            json_array_add_element (json_node_get_array (node), child_node);
          else
            json_object_set_member (json_node_get_object (node), key, child_node);

          json_node_set_parent (child_node, node);

          node = child_node;

          continue;
        }
      else if (type == JCON_TYPE_ARRAY_START)
        {
          JsonNode *child_node = json_node_new (JSON_NODE_ARRAY);
          JsonArray *child_array = json_array_new ();

          json_node_take_array (child_node, child_array);

          if (JSON_NODE_HOLDS_ARRAY (node))
            json_array_add_element (json_node_get_array (node), child_node);
          else
            json_object_set_member (json_node_get_object (node), key, child_node);

          json_node_set_parent (child_node, node);

          node = child_node;

          continue;
        }
      else if (type == JCON_TYPE_OBJECT_END || type == JCON_TYPE_ARRAY_END)
        {
          node = json_node_get_parent (node);
          continue;
        }

      if (JSON_NODE_HOLDS_ARRAY (node))
        json_array_add_element (json_node_get_array (node), jcon_append_to_node (type, &val));
      else
        json_object_set_member (json_node_get_object (node), key, jcon_append_to_node (type, &val));
    }
}

JsonNode *
jcon_new (gpointer unused,
          ...)
{
  g_autoptr(JsonNode) node = NULL;
  va_list args;

  g_return_val_if_fail (unused == NULL, NULL);

  node = json_node_new (JSON_NODE_OBJECT);
  json_node_take_object (node, json_object_new ());

  va_start (args, unused);
  jcon_append_va_list (node, &args);
  va_end (args);

  return g_steal_pointer (&node);
}

static gboolean
jcon_extract_tokenize (va_list     *args,
                       JconExtract *val,
                       JconType    *type)
{
  const char *mark;

  g_assert (args != NULL);
  g_assert (val != NULL);
  g_assert (type != NULL);

  memset (val, 0, sizeof *val);
  *type = -1;

  mark = va_arg (*args, const char *);

  if (mark == NULL)
    {
      *type = JCON_TYPE_END;
      return TRUE;
    }

  if (mark == JCONE_MAGIC)
    {
      *type = va_arg (*args, JconType);

      switch (*type)
        {
        case JCON_TYPE_STRING:
          val->v_string = va_arg (*args, const gchar **);
          return TRUE;

        case JCON_TYPE_DOUBLE:
          val->v_double = va_arg (*args, gdouble *);
          return TRUE;

        case JCON_TYPE_NODE:
          val->v_node = va_arg (*args, JsonNode **);
          return TRUE;

        case JCON_TYPE_OBJECT:
          val->v_object = va_arg (*args, JsonObject **);
          return TRUE;

        case JCON_TYPE_ARRAY:
          val->v_array = va_arg (*args, JsonArray **);
          return TRUE;

        case JCON_TYPE_BOOLEAN:
          val->v_boolean = va_arg (*args, gboolean *);
          return TRUE;

        case JCON_TYPE_NULL:
          return TRUE;

        case JCON_TYPE_INT:
          val->v_int = va_arg (*args, gint *);
          return TRUE;

        case JCON_TYPE_ARRAY_START:
        case JCON_TYPE_ARRAY_END:
        case JCON_TYPE_OBJECT_START:
        case JCON_TYPE_OBJECT_END:
        case JCON_TYPE_END:
        case JCON_TYPE_RAW:
        default:
          return FALSE;
        }
    }

  switch (mark[0])
    {
    case '{':
      *type = JCON_TYPE_OBJECT_START;
      return TRUE;

    case '}':
      *type = JCON_TYPE_OBJECT_END;
      return TRUE;

    case '[':
      *type = JCON_TYPE_ARRAY_START;
      return TRUE;

    case ']':
      *type = JCON_TYPE_ARRAY_END;
      return TRUE;

    default:
      break;
    }

  *type = JCON_TYPE_RAW;
  val->v_key = mark;

  return TRUE;
}

static void
jcon_extract_from_node (JsonNode    *node,
                        JconType     type,
                        JconExtract *val)
{
  g_assert (node != NULL);
  g_assert (val != NULL);

  switch (type)
    {
    case JCON_TYPE_STRING:
      if (JSON_NODE_HOLDS_VALUE (node))
        *val->v_string = json_node_get_string (node);
      else
        *val->v_string = NULL;
      break;

    case JCON_TYPE_DOUBLE:
      if (JSON_NODE_HOLDS_VALUE (node))
        *val->v_double = json_node_get_double (node);
      else
        *val->v_double = 0.0;
      break;

    case JCON_TYPE_NODE:
      *val->v_node = node;
      break;

    case JCON_TYPE_OBJECT:
      if (JSON_NODE_HOLDS_OBJECT (node))
        *val->v_object = json_node_get_object (node);
      else
        *val->v_object = NULL;
      break;

    case JCON_TYPE_ARRAY:
      if (JSON_NODE_HOLDS_ARRAY (node))
        *val->v_array = json_node_get_array (node);
      else
        *val->v_array = NULL;
      break;

    case JCON_TYPE_BOOLEAN:
      if (JSON_NODE_HOLDS_VALUE (node))
        *val->v_boolean = json_node_get_boolean (node);
      else
        *val->v_boolean = FALSE;
      break;

    case JCON_TYPE_NULL:
      break;

    case JCON_TYPE_INT:
      if (JSON_NODE_HOLDS_VALUE (node))
        *val->v_int = json_node_get_int (node);
      else
        *val->v_int = 0;
      break;

    case JCON_TYPE_ARRAY_START:
    case JCON_TYPE_ARRAY_END:
    case JCON_TYPE_OBJECT_START:
    case JCON_TYPE_OBJECT_END:
    case JCON_TYPE_END:
    case JCON_TYPE_RAW:
    default:
      g_assert_not_reached ();
      break;
    }
}

static JsonNode *
get_stack_node (JsonNode    *node,
                const gchar *key,
                guint        idx)
{
  g_assert (node != NULL);

  if (JSON_NODE_HOLDS_ARRAY (node))
    {
      JsonArray *array = json_node_get_array (node);
      if (array == NULL)
        return NULL;
      if (idx >= json_array_get_length (array))
        return NULL;
      return json_array_get_element (array, idx);
    }

  if (JSON_NODE_HOLDS_OBJECT (node))
    {
      JsonObject *object = json_node_get_object (node);
      if (object == NULL)
        return NULL;
      if (!json_object_has_member (object, key))
        return NULL;
      return json_object_get_member (object, key);
    }

  return NULL;
}

static gboolean
jcon_extract_va_list (JsonNode *node,
                      va_list  *args)
{
  const gchar *keys[STACK_DEPTH];
  guint indexes[STACK_DEPTH];
  gint sp = 0;

  g_assert (node != NULL);
  g_assert (args != NULL);

#define POP_STACK() sp--
#define PUSH_STACK() \
  G_STMT_START { \
    sp++; \
    keys[sp] = NULL; \
    indexes[sp] = 0; \
  } G_STMT_END

  keys[0] = NULL;
  indexes[0] = 0;

  while (node != NULL && sp >= 0 && sp < STACK_DEPTH)
    {
      JconExtract val = { 0 };
      JconType type;

      if (JSON_NODE_HOLDS_OBJECT (node))
        {
          JsonObject *object;

          if (!jcon_extract_tokenize (args, &val, &type))
            return FALSE;

          if (type == JCON_TYPE_END)
            return TRUE;

          if (type == JCON_TYPE_OBJECT_END)
            {
              node = json_node_get_parent (node);
              POP_STACK();
              continue;
            }

          if (type != JCON_TYPE_RAW)
            return FALSE;

          keys[sp] = val.v_key;
          object = json_node_get_object (node);

          if (object == NULL || !json_object_has_member (object, keys[sp]))
            return FALSE;
        }

      if (!jcon_extract_tokenize (args, &val, &type))
        return FALSE;

      /* We can have an END here if the root was an array */
      if (sp == 0 && type == JCON_TYPE_END && JSON_NODE_HOLDS_ARRAY (node))
        return TRUE;

      if (JSON_NODE_HOLDS_OBJECT (node))
        {
          if (type == JCON_TYPE_OBJECT_END || type == JCON_TYPE_ARRAY_END || type == JCON_TYPE_END)
            return FALSE;
        }

      if (type == JCON_TYPE_ARRAY_END)
        {
          if (!JSON_NODE_HOLDS_ARRAY (node))
            return FALSE;
          node = json_node_get_parent (node);
          POP_STACK();
          continue;
        }

      if (type == JCON_TYPE_ARRAY_START)
        {
          JsonNode *target = get_stack_node (node, keys[sp], indexes[sp]);
          if (target == NULL || !JSON_NODE_HOLDS_ARRAY (target))
            return FALSE;
          g_assert (node == json_node_get_parent (target));
          node = target;
          PUSH_STACK();
          continue;
        }

      if (type == JCON_TYPE_OBJECT_START)
        {
          JsonNode *target = get_stack_node (node, keys[sp], indexes[sp]);
          if (target == NULL || !JSON_NODE_HOLDS_OBJECT (target))
            return FALSE;
          g_assert (node == json_node_get_parent (target));
          node = target;
          PUSH_STACK();
          continue;
        }

      jcon_extract_from_node (get_stack_node (node, keys[sp], indexes[sp]), type, &val);

      if (JSON_NODE_HOLDS_ARRAY (node))
        indexes[sp]++;
    }

  return FALSE;
}

gboolean
jcon_extract (JsonNode *node,
              ...)
{
  gboolean ret;
  va_list args;

  va_start (args, node);
  ret = jcon_extract_va_list (node, &args);
  va_end (args);

  return ret;
}
