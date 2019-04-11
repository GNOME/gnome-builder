/* dspy-private.h
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "dspy-name.h"
#include "dspy-introspection-model.h"

G_BEGIN_DECLS

typedef enum
{
  DSPY_NODE_KIND_NODE = 1,
  DSPY_NODE_KIND_INTERFACES,
  DSPY_NODE_KIND_INTERFACE,
  DSPY_NODE_KIND_METHOD,
  DSPY_NODE_KIND_METHODS,
  DSPY_NODE_KIND_SIGNAL,
  DSPY_NODE_KIND_SIGNALS,
  DSPY_NODE_KIND_PROPERTY,
  DSPY_NODE_KIND_PROPERTIES,
  DSPY_NODE_KIND_ARG,
  DSPY_NODE_KIND_LAST
} DspyNodeKind;

typedef union  _DspyNode          DspyNode;
typedef struct _DspyArgInfo       DspyArgInfo;
typedef struct _DspyInterfaces    DspyInterfaces;
typedef struct _DspyInterfaceInfo DspyInterfaceInfo;
typedef struct _DspyMethodInfo    DspyMethodInfo;
typedef struct _DspyMethods       DspyMethods;
typedef struct _DspyNodeAny       DspyNodeAny;
typedef struct _DspyNodeInfo      DspyNodeInfo;
typedef struct _DspyProperties    DspyProperties;
typedef struct _DspyPropertyInfo  DspyPropertyInfo;
typedef struct _DspySignalInfo    DspySignalInfo;
typedef struct _DspySignals       DspySignals;

struct _DspyNodeAny
{
  DspyNodeKind  kind;
  DspyNode     *parent;
  GList         link;
};

struct _DspyNodeInfo
{
  DspyNodeKind     kind;
  DspyNode        *parent;
  GList            link;
  const gchar     *path;
  GQueue           nodes;
  DspyInterfaces  *interfaces;
};

struct _DspyInterfaceInfo
{
  DspyNodeKind    kind;
  DspyNode       *parent;
  GList           link;
  const gchar    *name;
  DspyProperties *properties;
  DspySignals    *signals;
  DspyMethods    *methods;
};

struct _DspyMethodInfo
{
  DspyNodeKind   kind;
  DspyNode      *parent;
  GList          link;
  const gchar   *name;
  GQueue         in_args;
  GQueue         out_args;
};

struct _DspySignalInfo
{
  DspyNodeKind   kind;
  DspyNode      *parent;
  GList          link;
  const gchar   *name;
  const gchar   *signature;
  GQueue         args;
};

struct _DspyPropertyInfo
{
  DspyNodeKind            kind;
  DspyNode               *parent;
  GList                   link;
  const gchar            *name;
  const gchar            *signature;
  GDBusPropertyInfoFlags  flags;
  gchar                  *value;
};

struct _DspyArgInfo
{
  DspyNodeKind  kind;
  DspyNode     *parent;
  GList         link;
  const gchar  *name;
  const gchar  *signature;
};

struct _DspyMethods
{
  DspyNodeKind  kind;
  DspyNode     *parent;
  GList         link;
  GQueue        methods;
};

struct _DspySignals
{
  DspyNodeKind  kind;
  DspyNode     *parent;
  GList         link;
  GQueue        signals;
};

struct _DspyProperties
{
  DspyNodeKind  kind;
  DspyNode     *parent;
  GList         link;
  GQueue        properties;
};

struct _DspyInterfaces
{
  DspyNodeKind  kind;
  DspyNode     *parent;
  GList         link;
  GQueue        interfaces;
};

union _DspyNode
{
  DspyNodeAny       any;
  DspyNodeInfo      node;
  DspyInterfaceInfo interface;
  DspyInterfaces    interfaces;
  DspyMethodInfo    method;
  DspyMethods       methods;
  DspySignalInfo    signal;
  DspySignals       signals;
  DspyPropertyInfo  property;
  DspyProperties    properties;
  DspyArgInfo       arg;
};

#define DSPY_IS_NODE(n) \
  (((DspyNode*)n)->any.kind > 0 && ((DspyNode*)n)->any.kind < DSPY_NODE_KIND_LAST)

DspyNodeInfo           *_dspy_node_parse              (const gchar              *xml,
                                                       GStringChunk             *chunks,
                                                       GError                  **error);
void                    _dspy_node_free               (gpointer                  data);
void                    _dspy_node_walk               (DspyNode                 *node,
                                                       GFunc                     func,
                                                       gpointer                  user_data);
gchar                  *_dspy_node_get_text           (DspyNode                 *node);
DspyNodeInfo           *_dspy_node_new_root           (void);
gboolean                _dspy_node_is_group           (DspyNode                 *node);
gint                    _dspy_node_info_compare       (const DspyNodeInfo       *a,
                                                       const DspyNodeInfo       *b);
const gchar            *_dspy_node_get_object_path    (DspyNode                 *node);
const gchar            *_dspy_node_get_interface      (DspyNode                 *node);
gint                    _dspy_interface_info_compare  (const DspyInterfaceInfo  *a,
                                                       const DspyInterfaceInfo  *b);
void                    _dspy_name_clear_pid          (DspyName                 *name);
void                    _dspy_name_refresh_pid        (DspyName                 *name,
                                                       GDBusConnection          *connection);
void                    _dspy_name_refresh_owner      (DspyName                 *name,
                                                       GDBusConnection          *connection);
void                    _dspy_name_set_owner          (DspyName                 *self,
                                                       const gchar              *owner);
void                    _dspy_name_set_activatable    (DspyName                 *name,
                                                       gboolean                  is_activatable);
DspyIntrospectionModel *_dspy_introspection_model_new (DspyName                 *name);
gchar                  *_dspy_signature_humanize      (const gchar              *signature);

G_END_DECLS
