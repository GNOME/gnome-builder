/* dspy-node.c
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

#define G_LOG_DOMAIN "dspy-node"

#include <errno.h>
#include <glib/gi18n.h>

#include "dspy-private.h"

#define LPAREN  "<span fgalpha='30000'>(</span>"
#define RPAREN  "<span fgalpha='30000'>)</span>"
#define ARROW   "<span fgalpha='20000'>↦</span>"
#define BOLD(s) "<span weight='bold'>" s "</span>"
#define DIM(s)  "<span fgalpha='40000'>" s "</span>"

/*
 * This file contains an alternate GDBusNodeInfo hierarchy that we can use
 * for a couple benefits over GDBusNodeInfo.
 *
 * First, it provides parent pointers so that we can navigate the structure
 * like a tree. This is very useful when used as a GtkTreeModel.
 *
 * Second, we can use a GStringChunk and reduce a lot of duplicate strings.
 */

static gpointer
dspy_node_new (DspyNodeKind  kind,
               DspyNode     *parent)
{
  DspyNode *node;

  g_assert (kind > 0);
  g_assert (kind < DSPY_NODE_KIND_LAST);

  node = g_slice_new0 (DspyNode);
  node->any.kind = kind;
  node->any.parent = parent;
  node->any.link.data = node;

  g_assert (DSPY_IS_NODE (node));

  return g_steal_pointer (&node);
}

static void
push_tail (GQueue   *queue,
           gpointer  node)
{
  DspyNodeAny *any = node;

  g_assert (DSPY_IS_NODE (any));

  g_queue_push_tail_link (queue, &any->link);
}

static void
clear_full (GQueue *queue)
{
  g_queue_foreach (queue, (GFunc) _dspy_node_free, NULL);
  queue->length = 0;
  queue->head = NULL;
  queue->tail = NULL;
}

static DspyArgInfo *
_dspy_arg_info_new (DspyNode     *parent,
                    GDBusArgInfo *info,
                    GStringChunk *chunks)
{
  DspyArgInfo *ret;

  g_assert (!parent || DSPY_IS_NODE (parent));
  g_assert (info != NULL);
  g_assert (chunks != NULL);

  ret = dspy_node_new (DSPY_NODE_KIND_ARG, parent);
  ret->name = g_string_chunk_insert_const (chunks, info->name);
  ret->signature = g_string_chunk_insert_const (chunks, info->signature);

  return ret;
}

static DspyMethodInfo *
_dspy_method_info_new (DspyNode        *parent,
                       GDBusMethodInfo *info,
                       GStringChunk    *chunks)
{
  DspyMethodInfo *ret;

  g_assert (!parent || DSPY_IS_NODE (parent));
  g_assert (info != NULL);
  g_assert (chunks != NULL);

  ret = dspy_node_new (DSPY_NODE_KIND_METHOD, parent);
  ret->name = g_string_chunk_insert_const (chunks, info->name);

  for (guint i = 0; info->in_args[i] != NULL; i++)
    push_tail (&ret->in_args,
               _dspy_arg_info_new ((DspyNode *)ret, info->in_args[i], chunks));

  for (guint i = 0; info->out_args[i] != NULL; i++)
    push_tail (&ret->out_args,
               _dspy_arg_info_new ((DspyNode *)ret, info->out_args[i], chunks));

  return ret;
}

static DspySignalInfo *
_dspy_signal_info_new (DspyNode        *parent,
                       GDBusSignalInfo *info,
                       GStringChunk    *chunks)
{
  DspySignalInfo *ret;

  g_assert (!parent || DSPY_IS_NODE (parent));
  g_assert (info != NULL);
  g_assert (chunks != NULL);

  ret = dspy_node_new (DSPY_NODE_KIND_SIGNAL, parent);
  ret->name = g_string_chunk_insert_const (chunks, info->name);

  for (guint i = 0; info->args[i] != NULL; i++)
    push_tail (&ret->args,
               _dspy_arg_info_new ((DspyNode *)ret, info->args[i], chunks));

  return ret;
}

static DspyPropertyInfo *
_dspy_property_info_new (DspyNode          *parent,
                         GDBusPropertyInfo *info,
                         GStringChunk      *chunks)
{
  DspyPropertyInfo *ret;

  g_assert (!parent || DSPY_IS_NODE (parent));
  g_assert (info != NULL);
  g_assert (chunks != NULL);

  ret = dspy_node_new (DSPY_NODE_KIND_PROPERTY, parent);
  ret->name = g_string_chunk_insert_const (chunks, info->name);
  ret->signature = g_string_chunk_insert_const (chunks, info->signature);
  ret->flags = info->flags;

  return ret;
}

static DspyInterfaceInfo *
_dspy_interface_info_new (DspyNode           *parent,
                          GDBusInterfaceInfo *info,
                          GStringChunk       *chunks)
{
  DspyInterfaceInfo *ret;

  g_assert (!parent || DSPY_IS_NODE (parent));
  g_assert (info != NULL);
  g_assert (chunks != NULL);

  ret = dspy_node_new (DSPY_NODE_KIND_INTERFACE, parent);
  ret->name = g_string_chunk_insert_const (chunks, info->name);
  ret->properties = dspy_node_new (DSPY_NODE_KIND_PROPERTIES, (DspyNode *)ret);
  ret->signals = dspy_node_new (DSPY_NODE_KIND_SIGNALS, (DspyNode *)ret);
  ret->methods = dspy_node_new (DSPY_NODE_KIND_METHODS, (DspyNode *)ret);

  for (guint i = 0; info->signals[i] != NULL; i++)
    push_tail (&ret->signals->signals,
               _dspy_signal_info_new ((DspyNode *)ret->signals, info->signals[i], chunks));

  for (guint i = 0; info->methods[i] != NULL; i++)
    push_tail (&ret->methods->methods,
               _dspy_method_info_new ((DspyNode *)ret->methods, info->methods[i], chunks));

  for (guint i = 0; info->properties[i] != NULL; i++)
    push_tail (&ret->properties->properties,
               _dspy_property_info_new ((DspyNode *)ret->properties,
                                        info->properties[i],
                                        chunks));

  return ret;
}

static DspyNodeInfo *
_dspy_node_info_new (DspyNode      *parent,
                     GDBusNodeInfo *info,
                     GStringChunk  *chunks)
{
  DspyNodeInfo *ret;

  g_assert (!parent || DSPY_IS_NODE (parent));
  g_assert (info != NULL);
  g_assert (chunks != NULL);

  ret = dspy_node_new (DSPY_NODE_KIND_NODE, parent);
  ret->interfaces = dspy_node_new (DSPY_NODE_KIND_INTERFACES, (DspyNode *)ret);
  ret->path = info->path ? g_string_chunk_insert_const (chunks, info->path) : NULL;

  for (guint i = 0; info->nodes[i] != NULL; i++)
    push_tail (&ret->nodes,
               _dspy_node_info_new ((DspyNode *)ret, info->nodes[i], chunks));

  if (info->interfaces[0])
    {
      for (guint i = 0; info->interfaces[i] != NULL; i++)
        push_tail (&ret->interfaces->interfaces,
                   _dspy_interface_info_new ((DspyNode *)ret->interfaces,
                                             info->interfaces[i],
                                             chunks));
    }

  return ret;
}

DspyNodeInfo *
_dspy_node_parse (const gchar   *xml,
                  GStringChunk  *chunks,
                  GError       **error)
{
  g_autoptr(GDBusNodeInfo) info = NULL;

  g_assert (xml != NULL);
  g_assert (chunks != NULL);

  if ((info = g_dbus_node_info_new_for_xml (xml, error)))
    return _dspy_node_info_new (NULL, info, chunks);

  return NULL;
}

void
_dspy_node_free (gpointer data)
{
  DspyNode *node = data;

  g_assert (!node || DSPY_IS_NODE (node));

  if (node == NULL)
    return;

  node->any.parent = NULL;

  switch (node->any.kind)
    {
    case DSPY_NODE_KIND_ARG:
      break;

    case DSPY_NODE_KIND_NODE:
      _dspy_node_free ((DspyNode *)node->node.interfaces);
      clear_full (&node->node.nodes);
      break;

    case DSPY_NODE_KIND_INTERFACE:
      _dspy_node_free ((DspyNode *)node->interface.properties);
      _dspy_node_free ((DspyNode *)node->interface.signals);
      _dspy_node_free ((DspyNode *)node->interface.methods);
      break;

    case DSPY_NODE_KIND_INTERFACES:
      clear_full (&node->interfaces.interfaces);
      break;

    case DSPY_NODE_KIND_METHODS:
      clear_full (&node->methods.methods);
      break;

    case DSPY_NODE_KIND_METHOD:
      clear_full (&node->method.in_args);
      clear_full (&node->method.out_args);
      break;

    case DSPY_NODE_KIND_PROPERTIES:
      clear_full (&node->properties.properties);
      break;

    case DSPY_NODE_KIND_PROPERTY:
      g_clear_pointer (&node->property.value, g_free);
      break;

    case DSPY_NODE_KIND_SIGNALS:
      clear_full (&node->signals.signals);
      break;

    case DSPY_NODE_KIND_SIGNAL:
      clear_full (&node->signal.args);
      break;

    case DSPY_NODE_KIND_LAST:
    default:
      g_assert_not_reached ();
    }

  node->any.kind = 0;
  node->any.parent = NULL;
  node->any.link.prev = NULL;
  node->any.link.next = NULL;
  node->any.link.data = NULL;

  g_slice_free (DspyNode, node);
}

gint
_dspy_node_info_compare (const DspyNodeInfo  *a,
                         const DspyNodeInfo  *b)
{
  return g_strcmp0 (a->path, b->path);
}

gint
_dspy_interface_info_compare (const DspyInterfaceInfo *a,
                              const DspyInterfaceInfo *b)
{
  return g_strcmp0 (a->name, b->name);
}

DspyNodeInfo *
_dspy_node_new_root (void)
{
  return dspy_node_new (DSPY_NODE_KIND_NODE, NULL);
}

void
_dspy_node_walk (DspyNode *node,
                 GFunc     func,
                 gpointer  user_data)
{
  g_assert (DSPY_IS_NODE (node));
  g_assert (func != NULL);

  func (node, user_data);

  switch (node->any.kind)
    {
    case DSPY_NODE_KIND_ARG:
      break;

    case DSPY_NODE_KIND_NODE:
      if (node->node.interfaces != NULL)
        _dspy_node_walk ((DspyNode *)node->node.interfaces, func, user_data);
      for (const GList *iter = node->node.nodes.head; iter; iter = iter->next)
        _dspy_node_walk (iter->data, func, user_data);
      break;

    case DSPY_NODE_KIND_INTERFACE:
      _dspy_node_walk ((DspyNode *)node->interface.properties, func, user_data);
      _dspy_node_walk ((DspyNode *)node->interface.signals, func, user_data);
      _dspy_node_walk ((DspyNode *)node->interface.methods, func, user_data);
      break;

    case DSPY_NODE_KIND_INTERFACES:
      for (const GList *iter = node->interfaces.interfaces.head; iter; iter = iter->next)
        _dspy_node_walk (iter->data, func, user_data);
      break;

    case DSPY_NODE_KIND_METHODS:
      for (const GList *iter = node->methods.methods.head; iter; iter = iter->next)
        _dspy_node_walk (iter->data, func, user_data);
      break;

    case DSPY_NODE_KIND_METHOD:
    case DSPY_NODE_KIND_PROPERTY:
    case DSPY_NODE_KIND_SIGNAL:
      break;

    case DSPY_NODE_KIND_PROPERTIES:
      for (const GList *iter = node->properties.properties.head; iter; iter = iter->next)
        _dspy_node_walk (iter->data, func, user_data);
      break;

    case DSPY_NODE_KIND_SIGNALS:
      for (const GList *iter = node->signals.signals.head; iter; iter = iter->next)
        _dspy_node_walk (iter->data, func, user_data);
      break;

    case DSPY_NODE_KIND_LAST:
    default:
      g_assert_not_reached ();
    }
}

static gchar *
_dspy_property_info_to_string (DspyPropertyInfo *info)
{
  g_autofree gchar *sig = NULL;
  const gchar *rw;

  g_assert (DSPY_IS_NODE (info));
  g_assert (info->kind == DSPY_NODE_KIND_PROPERTY);

  sig = _dspy_signature_humanize (info->signature);

  if (info->flags == (G_DBUS_PROPERTY_INFO_FLAGS_READABLE | G_DBUS_PROPERTY_INFO_FLAGS_WRITABLE))
    rw = _("read/write");
  else if (info->flags  & G_DBUS_PROPERTY_INFO_FLAGS_WRITABLE)
    rw = _("write-only");
  else if (info->flags  & G_DBUS_PROPERTY_INFO_FLAGS_READABLE)
    rw = _("read-only");
  else
    rw = "";

  return g_strdup_printf ("%s "ARROW" "BOLD(DIM("%s"))" "LPAREN DIM("%s") RPAREN,
                          info->name, sig, rw);
}

static gboolean
arg_name_is_generated (const gchar *str)
{
  if (str == NULL)
    return TRUE;

  if (g_str_has_prefix (str, "arg_"))
    {
      gchar *endptr = NULL;
      gint64 val;

      str += strlen ("arg_");
      errno = 0;
      val = g_ascii_strtoll (str, &endptr, 10);

      if (val >= 0 && errno == 0 && *endptr == 0)
        return TRUE;
    }

  return FALSE;
}

static gchar *
_dspy_method_info_to_string (DspyMethodInfo *info)
{
  GString *str;

  g_assert (DSPY_IS_NODE (info));
  g_assert (info->kind == DSPY_NODE_KIND_METHOD);

  str = g_string_new (info->name);
  g_string_append (str, " "LPAREN);

  for (const GList *iter = info->in_args.head; iter; iter = iter->next)
    {
      DspyArgInfo *arg = iter->data;
      g_autofree gchar *sig = _dspy_signature_humanize (arg->signature);

      if (iter->prev != NULL)
        g_string_append (str, ", ");
      g_string_append_printf (str, BOLD(DIM("%s")), sig);
      if (!arg_name_is_generated (arg->name))
        g_string_append_printf (str, DIM(" %s"), arg->name);
    }

  g_string_append (str, RPAREN" "ARROW" "LPAREN);

  for (const GList *iter = info->out_args.head; iter; iter = iter->next)
    {
      DspyArgInfo *arg = iter->data;
      g_autofree gchar *sig = _dspy_signature_humanize (arg->signature);

      if (iter->prev != NULL)
        g_string_append (str, ", ");
      g_string_append_printf (str, BOLD(DIM("%s")), sig);
      if (!arg_name_is_generated (arg->name))
        g_string_append_printf (str, DIM(" %s"), arg->name);
    }

  g_string_append (str, RPAREN);

  return g_string_free (str, FALSE);
}

static gchar *
_dspy_signal_info_to_string (DspySignalInfo *info)
{
  GString *str;

  g_assert (DSPY_IS_NODE (info));
  g_assert (info->kind == DSPY_NODE_KIND_SIGNAL);

  str = g_string_new (info->name);
  g_string_append (str, " "LPAREN);

  for (const GList *iter = info->args.head; iter; iter = iter->next)
    {
      DspyArgInfo *arg = iter->data;
      g_autofree gchar *sig = _dspy_signature_humanize (arg->signature);

      if (iter->prev != NULL)
        g_string_append (str, ", ");
      g_string_append_printf (str, BOLD(DIM("%s")), sig);
      if (!arg_name_is_generated (arg->name))
        g_string_append_printf (str, DIM(" %s"), arg->name);
    }

  g_string_append (str, RPAREN);

  return g_string_free (str, FALSE);
}

gchar *
_dspy_node_get_text (DspyNode *node)
{
  switch (node->any.kind)
    {
    case DSPY_NODE_KIND_ARG:
      return g_strdup (node->arg.name);

    case DSPY_NODE_KIND_NODE:
      return g_strdup (node->node.path);

    case DSPY_NODE_KIND_INTERFACE:
      return g_strdup (node->interface.name);

    case DSPY_NODE_KIND_INTERFACES:
      return g_strdup (_("Interfaces"));

    case DSPY_NODE_KIND_METHODS:
      return g_strdup (_("Methods"));

    case DSPY_NODE_KIND_METHOD:
      return _dspy_method_info_to_string (&node->method);

    case DSPY_NODE_KIND_PROPERTIES:
      return g_strdup (_("Properties"));

    case DSPY_NODE_KIND_PROPERTY:
        {
          g_autofree gchar *str = _dspy_property_info_to_string (&node->property);

          if (node->property.value != NULL)
            {
              g_autofree gchar *escaped = g_markup_escape_text (node->property.value, -1);
              return g_strdup_printf ("%s = %s", str, escaped);
            }

          return g_steal_pointer (&str);
        }

    case DSPY_NODE_KIND_SIGNALS:
      return g_strdup (_("Signals"));

    case DSPY_NODE_KIND_SIGNAL:
      return _dspy_signal_info_to_string (&node->signal);

    case DSPY_NODE_KIND_LAST:
    default:
      g_return_val_if_reached (NULL);
    }
}

gboolean
_dspy_node_is_group (DspyNode *node)
{
  g_assert (node != NULL);
  g_assert (DSPY_IS_NODE (node));

  return node->any.kind == DSPY_NODE_KIND_INTERFACES ||
         node->any.kind == DSPY_NODE_KIND_PROPERTIES ||
         node->any.kind == DSPY_NODE_KIND_SIGNALS ||
         node->any.kind == DSPY_NODE_KIND_METHODS;
}

const gchar *
_dspy_node_get_object_path (DspyNode *node)
{
  if (node == NULL)
    return NULL;

  if (node->any.kind == DSPY_NODE_KIND_NODE)
    return node->node.path;

  return _dspy_node_get_object_path (node->any.parent);
}

const gchar *
_dspy_node_get_interface (DspyNode *node)
{
  if (node == NULL)
    return NULL;

  if (node->any.kind == DSPY_NODE_KIND_INTERFACE)
    return node->interface.name;

  return _dspy_node_get_interface (node->any.parent);
}
