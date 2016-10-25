/* jcon.h
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
 * Copyright      2016 Christian Hergert <chergert@redhat.com>
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

#ifndef JSONRPC_JCON_H
#define JSONRPC_JCON_H

#include <json-glib/json-glib.h>

G_BEGIN_DECLS

#define JCON_ENSURE_DECLARE(fun, type) \
  static inline type jcon_ensure_##fun (type _t) { return _t; }

#define JCON_ENSURE(fun, val) \
  jcon_ensure_##fun (val)

#define JCON_ENSURE_STORAGE(fun, val) \
  jcon_ensure_##fun (&(val))

JCON_ENSURE_DECLARE (const_char_ptr, const char *)
JCON_ENSURE_DECLARE (const_char_ptr_ptr, const char **)
JCON_ENSURE_DECLARE (double, double)
JCON_ENSURE_DECLARE (double_ptr, double *)
JCON_ENSURE_DECLARE (array_ptr, JsonArray *)
JCON_ENSURE_DECLARE (array_ptr_ptr, JsonArray **)
JCON_ENSURE_DECLARE (object_ptr, JsonObject *)
JCON_ENSURE_DECLARE (object_ptr_ptr, JsonObject **)
JCON_ENSURE_DECLARE (node_ptr, JsonNode *)
JCON_ENSURE_DECLARE (node_ptr_ptr, JsonNode **)
JCON_ENSURE_DECLARE (int, gint)
JCON_ENSURE_DECLARE (int_ptr, gint *)
JCON_ENSURE_DECLARE (boolean, gboolean)
JCON_ENSURE_DECLARE (boolean_ptr, gboolean *)

#define JCON_STRING(_val) \
  JCON_MAGIC, JCON_TYPE_STRING, JCON_ENSURE (const_char_ptr, (_val))
#define JCON_DOUBLE(_val) \
  JCON_MAGIC, JCON_TYPE_DOUBLE, JCON_ENSURE (double, (_val))
#define JCON_OBJECT(_val) \
  JCON_MAGIC, JCON_TYPE_OBJECT, JCON_ENSURE (object_ptr, (_val))
#define JCON_ARRAY(_val) \
  JCON_MAGIC, JCON_TYPE_ARRAY, JCON_ENSURE (array_ptr, (_val))
#define JCON_NODE(_val) \
  JCON_MAGIC, JCON_TYPE_NODE, JCON_ENSURE (node_ptr, (_val))
#define JCON_BOOLEAN(_val) \
  JCON_MAGIC, JCON_TYPE_BOOLEAN, JCON_ENSURE (boolean, (_val))
#define JCON_NULL JCON_MAGIC, JCON_TYPE_NULL
#define JCON_INT(_val) \
  JCON_MAGIC, JCON_TYPE_INT, JCON_ENSURE (int, (_val))

#define JCONE_STRING(_val) JCONE_MAGIC, JCON_TYPE_STRING, \
  JCON_ENSURE_STORAGE (const_char_ptr_ptr, (_val))
#define JCONE_DOUBLE(_val) JCONE_MAGIC, JCON_TYPE_DOUBLE, \
  JCON_ENSURE_STORAGE (double_ptr, (_val))
#define JCONE_OBJECT(_val) JCONE_MAGIC, JCON_TYPE_OBJECT, \
  JCON_ENSURE_STORAGE (object_ptr_ptr, (_val))
#define JCONE_ARRAY(_val) JCONE_MAGIC, JCON_TYPE_ARRAY, \
  JCON_ENSURE_STORAGE (array_ptr_ptr, (_val))
#define JCONE_NODE(_val) JCONE_MAGIC, JCON_TYPE_NODE, \
  JCON_ENSURE_STORAGE (node_ptr_ptr, (_val))
#define JCONE_BOOLEAN(_val) JCONE_MAGIC, JCON_TYPE_BOOLEAN, \
  JCON_ENSURE_STORAGE (bool_ptr, (_val))
#define JCONE_NULL JCONE_MAGIC, JCON_TYPE_NULL
#define JCONE_INT(_val) JCONE_MAGIC, JCON_TYPE_INT, \
  JCON_ENSURE_STORAGE (int_ptr, (_val))

typedef enum
{
  JCON_TYPE_STRING,
  JCON_TYPE_DOUBLE,
  JCON_TYPE_OBJECT,
  JCON_TYPE_ARRAY,
  JCON_TYPE_NODE,
  JCON_TYPE_BOOLEAN,
  JCON_TYPE_NULL,
  JCON_TYPE_INT,
  JCON_TYPE_ARRAY_START,
  JCON_TYPE_ARRAY_END,
  JCON_TYPE_OBJECT_START,
  JCON_TYPE_OBJECT_END,
  JCON_TYPE_END,
  JCON_TYPE_RAW,
} JconType;

#define JCON_MAGIC jcon_magic()
const char *jcon_magic  (void) G_GNUC_CONST;

#define JCONE_MAGIC jcone_magic()
const char *jcone_magic (void) G_GNUC_CONST;

#define JCON_NEW(...) jcon_new (NULL, __VA_ARGS__, NULL)
JsonNode *jcon_new (gpointer  unused, ...) G_GNUC_NULL_TERMINATED;

#define JCON_EXTRACT(_node, ...) jcon_extract ((_node), __VA_ARGS__, NULL)
gboolean jcon_extract (JsonNode *node, ...) G_GNUC_NULL_TERMINATED;

G_END_DECLS

#endif /* JSONRPC_JCON_H */
