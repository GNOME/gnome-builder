/* dspy-introspection-model.c
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

#define G_LOG_DOMAIN "dspy-introspection-model"

#include "config.h"

#include <glib/gi18n.h>
#include <string.h>

#include "dspy-introspection-model.h"
#include "dspy-private.h"

#if 0
# define LOG_DEBUG(str) g_printerr ("%s\n", str);
#else
# define LOG_DEBUG(str)
#endif

struct _DspyIntrospectionModel
{
  GObject       parent_instance;

  GCancellable *cancellable;
  DspyName     *name;
  DspyNodeInfo *root;

  /* Synchronize chunks access in threaded workers */
  GMutex        chunks_mutex;
  GStringChunk *chunks;
};

typedef struct
{
  GTask           *task;
  GDBusConnection *connection;
  gchar           *path;
} Introspect;

static void
introspect_free (Introspect *state)
{
  g_clear_object (&state->task);
  g_clear_object (&state->connection);
  g_clear_pointer (&state->path, g_free);
  g_slice_free (Introspect, state);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (Introspect, introspect_free)

static void dspy_introspection_model_introspect (GTask           *task,
                                                 GDBusConnection *connection,
                                                 const gchar     *path);

static void
parse_xml_worker (GTask        *task,
                  gpointer      source_object,
                  gpointer      task_data,
                  GCancellable *cancellable)
{
  DspyIntrospectionModel *self = source_object;
  GBytes *bytes = task_data;
  g_autoptr(GError) error = NULL;
  DspyNodeInfo *info;
  const gchar *xml;

  g_assert (G_IS_TASK (task));
  g_assert (DSPY_IS_INTROSPECTION_MODEL (source_object));
  g_assert (bytes != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  xml = (const gchar *)g_bytes_get_data (bytes, NULL);

  g_mutex_lock (&self->chunks_mutex);
  info = _dspy_node_parse (xml, self->chunks, &error);
  g_mutex_unlock (&self->chunks_mutex);

  if (info != NULL)
    g_task_return_pointer (task, info, (GDestroyNotify) _dspy_node_free);
  else
    g_task_return_error (task, g_steal_pointer (&error));
}

static void
parse_xml_async (DspyIntrospectionModel *self,
                 GBytes                 *bytes,
                 GCancellable           *cancellable,
                 GAsyncReadyCallback     callback,
                 gpointer                user_data)
{
  g_autoptr(GTask) task = NULL;

  g_assert (DSPY_IS_INTROSPECTION_MODEL (self));
  g_assert (bytes != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, parse_xml_async);
  g_task_set_task_data (task, g_bytes_ref (bytes), (GDestroyNotify) g_bytes_unref);
  g_task_run_in_thread (task, parse_xml_worker);
}

static DspyNodeInfo *
parse_xml_finish (DspyIntrospectionModel  *self,
                  GAsyncResult            *result,
                  GError                 **error)
{
  g_assert (DSPY_IS_INTROSPECTION_MODEL (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
emit_row_inserted_for_tree_cb (gpointer item,
                               gpointer user_data)
{
  g_autoptr(GtkTreePath) path = NULL;
  DspyIntrospectionModel *self = user_data;
  GtkTreeIter iter = { .user_data = item, };

  path = gtk_tree_model_get_path (GTK_TREE_MODEL (self), &iter);
  gtk_tree_model_row_inserted (GTK_TREE_MODEL (self), path, &iter);
}

static void
emit_row_inserted_for_tree (DspyIntrospectionModel *self,
                            DspyNode               *tree)
{
  g_assert (DSPY_IS_INTROSPECTION_MODEL (self));
  g_assert (tree != NULL);

  _dspy_node_walk (tree, emit_row_inserted_for_tree_cb, self);
}

static void
dspy_introspection_model_init_parse_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  DspyIntrospectionModel *self = (DspyIntrospectionModel *)object;
  g_autoptr(Introspect) state = user_data;
  g_autoptr(GError) error = NULL;
  DspyNodeInfo *info = NULL;
  GCancellable *cancellable;
  gint *n_active;

  g_assert (DSPY_IS_INTROSPECTION_MODEL (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (state != NULL);
  g_assert (G_IS_TASK (state->task));
  g_assert (state->path != NULL);

  self = g_task_get_source_object (state->task);
  n_active = g_task_get_task_data (state->task);
  cancellable = g_task_get_cancellable (state->task);

  g_assert (self != NULL);
  g_assert (DSPY_IS_INTROSPECTION_MODEL (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (n_active != NULL);
  g_assert (*n_active > 0);

  if ((info = parse_xml_finish (self, result, &error)))
    {
      g_assert (DSPY_IS_NODE (info));
      g_assert (info->kind == DSPY_NODE_KIND_NODE);

      /* First, queue a bunch of sub-path reads based on any discovered
       * nodes from querying this specific node.
       */
      for (const GList *iter = info->nodes.head; iter; iter = iter->next)
        {
          DspyNodeInfo *child = iter->data;
          g_autofree gchar *child_path = NULL;

          g_assert (child != NULL);
          g_assert (DSPY_IS_NODE (child));
          g_assert (child->kind == DSPY_NODE_KIND_NODE);

          child_path = g_build_path ("/", state->path, child->path, NULL);
          dspy_introspection_model_introspect (state->task, state->connection, child_path);
        }

      /* Now add this node to our root if it contains any intefaces. */
      if (info->interfaces->interfaces.length > 0)
        {
          g_autofree gchar *abs_path = g_build_path ("/", state->path, info->path, NULL);

          g_mutex_lock (&self->chunks_mutex);
          info->path = g_string_chunk_insert_const (self->chunks, abs_path);
          g_mutex_unlock (&self->chunks_mutex);

          g_queue_push_tail_link (&self->root->nodes, &info->link);
          info->parent = (DspyNode *)self->root;

          emit_row_inserted_for_tree (self, (DspyNode *)info);

          /* Stolen */
          info = NULL;
        }

      g_clear_pointer (&info, _dspy_node_free);
    }

  if (--(*n_active) == 0)
    g_task_return_boolean (state->task, TRUE);
}

static void
dspy_introspection_model_init_introspect_cb (GObject      *object,
                                             GAsyncResult *result,
                                             gpointer      user_data)
{
  GDBusConnection *bus = (GDBusConnection *)object;
  DspyIntrospectionModel *self;
  g_autoptr(Introspect) state = user_data;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GError) error = NULL;
  GCancellable *cancellable;
  gint *n_active;

  g_assert (G_IS_DBUS_CONNECTION (bus));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (state != NULL);
  g_assert (G_IS_TASK (state->task));
  g_assert (state->path != NULL);

  self = g_task_get_source_object (state->task);
  n_active = g_task_get_task_data (state->task);
  cancellable = g_task_get_cancellable (state->task);

  g_assert (self != NULL);
  g_assert (DSPY_IS_INTROSPECTION_MODEL (self));
  g_assert (n_active != NULL);
  g_assert (*n_active > 0);

  if ((reply = g_dbus_connection_call_finish (bus, result, &error)))
    {
      g_autoptr(GBytes) bytes = NULL;
      const gchar *str = NULL;

      /* Get the XML contents, and wrap it in a new GBytes that will
       * reference the original GVariant to avoid a copy as this might
       * contain a large amount of text.
       */
      g_variant_get (reply, "(&s)", &str);

      if (str[0] != 0)
        {
          bytes = g_bytes_new_with_free_func (str,
                                              strlen (str),
                                              (GDestroyNotify) g_variant_unref,
                                              g_variant_ref (reply));
          parse_xml_async (self,
                           bytes,
                           cancellable,
                           dspy_introspection_model_init_parse_cb,
                           g_steal_pointer (&state));
          return;
        }
    }
  else
    {
      DspyConnection *connection = dspy_name_get_connection (self->name);

      dspy_connection_add_error (connection, error);
    }

  if (--(*n_active) == 0)
    g_task_return_boolean (state->task, TRUE);
}

static gboolean
has_node_with_path (DspyIntrospectionModel *self,
                    const gchar            *path)
{
  g_assert (DSPY_IS_INTROSPECTION_MODEL (self));
  g_assert (path != NULL);

  for (const GList *iter = self->root->nodes.head; iter; iter = iter->next)
    {
      const DspyNode *node = iter->data;

      g_assert (node != NULL);
      g_assert (DSPY_IS_NODE (node));
      g_assert (node->any.kind == DSPY_NODE_KIND_NODE);

      if (g_strcmp0 (path, node->node.path) == 0)
        return TRUE;
    }

  return FALSE;
}

static void
dspy_introspection_model_introspect (GTask           *task,
                                     GDBusConnection *connection,
                                     const gchar     *path)
{
  DspyIntrospectionModel *self;
  Introspect *state;
  gint *n_active;

  g_assert (G_IS_TASK (task));
  g_assert (G_IS_DBUS_CONNECTION (connection));
  g_assert (path != NULL);

  self = g_task_get_source_object (task);
  n_active = g_task_get_task_data (task);

  g_assert (G_IS_TASK (task));
  g_assert (n_active != NULL);

  /* If we already have this path, then ignore the suplimental query */
  if (has_node_with_path (self, path))
    return;

  (*n_active)++;

  state = g_slice_new0 (Introspect);
  state->task = g_object_ref (task);
  state->connection = g_object_ref (connection);
  state->path = g_strdup (path);

  g_dbus_connection_call (connection,
                          dspy_name_get_owner (self->name),
                          path,
                          "org.freedesktop.DBus.Introspectable",
                          "Introspect",
                          NULL, /* Params */
                          G_VARIANT_TYPE ("(s)"),
                          G_DBUS_CALL_FLAGS_ALLOW_INTERACTIVE_AUTHORIZATION,
                          -1,
                          self->cancellable,
                          dspy_introspection_model_init_introspect_cb,
                          state);
}

static void
dspy_introspection_model_init_async (GAsyncInitable      *initiable,
                                     gint                 io_priority,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  DspyIntrospectionModel *self = (DspyIntrospectionModel *)initiable;
  GDBusConnection *bus = NULL;
  DspyConnection *connection = NULL;
  g_autoptr(GTask) task = NULL;

  g_assert (DSPY_IS_INTROSPECTION_MODEL (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, dspy_introspection_model_init_async);
  g_task_set_task_data (task, g_new0 (gint, 1), g_free);
  g_task_set_priority (task, io_priority);

  if (self->name == NULL ||
      !(connection = dspy_name_get_connection (self->name)) ||
      !(bus = dspy_connection_get_connection (connection)))
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_INITIALIZED,
                               "%s has not been intialized with a name",
                               G_OBJECT_TYPE_NAME (self));
      return;
    }

  dspy_introspection_model_introspect (task, bus, "/");
}

static gboolean
dspy_introspection_model_init_finish (GAsyncInitable  *initable,
                                      GAsyncResult    *result,
                                      GError         **error)
{
  g_assert (DSPY_IS_INTROSPECTION_MODEL (initable));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = dspy_introspection_model_init_async;
  iface->init_finish = dspy_introspection_model_init_finish;
}

static gboolean
dspy_introspection_model_iter_children (GtkTreeModel *model,
                                        GtkTreeIter  *iter,
                                        GtkTreeIter  *parent)
{
  LOG_DEBUG (G_STRFUNC);

  g_assert (DSPY_IS_INTROSPECTION_MODEL (model));
  g_assert (iter != NULL);

  return gtk_tree_model_iter_nth_child (model, iter, parent, 0);
}

static gboolean
dspy_introspection_model_iter_next (GtkTreeModel *model,
                                    GtkTreeIter  *iter)
{
  DspyIntrospectionModel *self = (DspyIntrospectionModel *)model;
  DspyNode *node;

  LOG_DEBUG (G_STRFUNC);

  g_assert (DSPY_IS_INTROSPECTION_MODEL (self));
  g_assert (iter != NULL);

  node = iter->user_data;

  g_assert (node != NULL);
  g_assert (node->any.kind > 0);
  g_assert (node->any.kind < DSPY_NODE_KIND_LAST);

  switch (node->any.kind)
    {
    case DSPY_NODE_KIND_NODE:
    case DSPY_NODE_KIND_METHOD:
    case DSPY_NODE_KIND_SIGNAL:
    case DSPY_NODE_KIND_PROPERTY:
    case DSPY_NODE_KIND_INTERFACE:
      if (node->any.link.next != NULL)
        {
          iter->user_data = node->any.link.next->data;
          return TRUE;
        }
      else
        {
          node->any.link.next = NULL;
          return FALSE;
        }

    case DSPY_NODE_KIND_PROPERTIES:
      iter->user_data = node->any.parent->interface.signals;
      return TRUE;

    case DSPY_NODE_KIND_SIGNALS:
      iter->user_data = node->any.parent->interface.methods;
      return TRUE;

    case DSPY_NODE_KIND_INTERFACES:
    case DSPY_NODE_KIND_METHODS:
    case DSPY_NODE_KIND_ARG:
    case DSPY_NODE_KIND_LAST:
    default:
      return FALSE;
    }
}

static gint
dspy_introspection_model_get_n_columns (GtkTreeModel *model)
{
  return 1;
}

static GtkTreePath *
dspy_introspection_model_get_path (GtkTreeModel *model,
                                   GtkTreeIter  *iter)
{
  DspyIntrospectionModel *self = (DspyIntrospectionModel *)model;
  GtkTreePath *path;
  DspyNode *node;

  LOG_DEBUG (G_STRFUNC);

  g_assert (DSPY_IS_INTROSPECTION_MODEL (self));
  g_assert (iter != NULL);

  node = iter->user_data;

  g_assert (node != NULL);
  g_assert (node->any.parent != NULL);

  path = gtk_tree_path_new_first ();

  g_assert (gtk_tree_path_get_depth (path) == 1);

  for (; node->any.parent != NULL; node = node->any.parent)
    {
      gint pos = 0;

      for (const GList *list = &node->any.link; list->prev; list = list->prev)
        pos++;

      gtk_tree_path_prepend_index (path, pos);
    }

  gtk_tree_path_up (path);

  return g_steal_pointer (&path);
}

static gboolean
dspy_introspection_model_iter_parent (GtkTreeModel *model,
                                      GtkTreeIter  *iter,
                                      GtkTreeIter  *child)
{
  DspyNode *node;

  LOG_DEBUG (G_STRFUNC);

  g_assert (DSPY_IS_INTROSPECTION_MODEL (model));
  g_assert (iter != NULL);
  g_assert (child != NULL);

  memset (iter, 0, sizeof *iter);

  node = child->user_data;

  g_assert (node != NULL);
  g_assert (DSPY_IS_NODE (node));
  g_assert (node->any.parent != NULL);

  /* Ignore root, we don't have a visual node for that */
  if (node->any.parent->any.parent != NULL)
    iter->user_data = node->node.parent;

  return iter->user_data != NULL;
}

static GType
dspy_introspection_model_get_column_type (GtkTreeModel *model,
                                          gint          column)
{
  if (column == 0)
    return G_TYPE_STRING;

  return G_TYPE_INVALID;
}

static gboolean
dspy_introspection_model_iter_has_child (GtkTreeModel *model,
                                         GtkTreeIter  *iter)
{
  GtkTreeIter child;

  LOG_DEBUG (G_STRFUNC);

  g_assert (DSPY_IS_INTROSPECTION_MODEL (model));
  g_assert (iter != NULL);

  return gtk_tree_model_iter_nth_child (model, &child, iter, 0);
}

static GtkTreeModelFlags
dspy_introspection_model_get_flags (GtkTreeModel *model)
{
  return 0;
}

static void
dspy_introspection_model_get_value (GtkTreeModel *model,
                                    GtkTreeIter  *iter,
                                    gint          column,
                                    GValue       *value)
{
  LOG_DEBUG (G_STRFUNC);

  if (column == 0)
    {
      DspyNode *node = iter->user_data;
      g_autofree gchar *str = NULL;

      g_assert (node != NULL);
      g_assert (DSPY_IS_NODE (node));

      g_value_init (value, G_TYPE_STRING);

      str = _dspy_node_get_text (node);

      if (_dspy_node_is_group (node))
        {
          if (gtk_tree_model_iter_has_child (model, iter))
            g_value_take_string (value, g_strdup_printf ("<b>%s</b>", str));
          else
            g_value_take_string (value, g_strdup_printf ("<span fgalpha='25000' weight='bold'>%s</span>", str));
        }
      else
        g_value_take_string (value, g_steal_pointer (&str));
    }
}

static gboolean
dspy_introspection_model_get_iter (GtkTreeModel *model,
                                   GtkTreeIter  *iter,
                                   GtkTreePath  *tree_path)
{
  DspyIntrospectionModel *self = (DspyIntrospectionModel *)model;
  DspyNode *cur;
  gint *indices;
  gint depth;

  LOG_DEBUG (G_STRFUNC);

  g_assert (DSPY_IS_INTROSPECTION_MODEL (self));
  g_assert (iter != NULL);
  g_assert (tree_path != NULL);

  memset (iter, 0, sizeof *iter);

  cur = (DspyNode *)self->root;
  indices = gtk_tree_path_get_indices_with_depth (tree_path, &depth);

  for (guint i = 0; cur != NULL && i < depth; i++)
    {
      gint pos = indices[i];

      if (cur->any.parent == NULL)
        cur = g_queue_peek_nth (&cur->node.nodes, pos);
      else if (cur->any.kind == DSPY_NODE_KIND_NODE)
        cur = (DspyNode *)cur->node.interfaces;
      else if (cur->any.kind == DSPY_NODE_KIND_INTERFACES)
        cur = g_queue_peek_nth (&cur->interfaces.interfaces, pos);
      else if (cur->any.kind == DSPY_NODE_KIND_INTERFACE)
        {
          if (pos == 0)
            cur = (DspyNode *)cur->interface.properties;
          else if (pos == 1)
            cur = (DspyNode *)cur->interface.signals;
          else if (pos == 2)
            cur = (DspyNode *)cur->interface.methods;
          else
            cur = NULL;
        }
      else if (cur->any.kind == DSPY_NODE_KIND_PROPERTIES)
        cur = g_queue_peek_nth (&cur->properties.properties, pos);
      else if (cur->any.kind == DSPY_NODE_KIND_SIGNALS)
        cur = g_queue_peek_nth (&cur->signals.signals, pos);
      else if (cur->any.kind == DSPY_NODE_KIND_METHODS)
        cur = g_queue_peek_nth (&cur->methods.methods, pos);
      else
        cur = NULL;
    }

  if (cur != NULL)
    {
      iter->user_data = cur;
      return TRUE;
    }

  return FALSE;
}

static gint
dspy_introspection_model_iter_n_children (GtkTreeModel *model,
                                          GtkTreeIter  *iter)
{
  DspyIntrospectionModel *self = (DspyIntrospectionModel *)model;
  DspyNode *node;

  LOG_DEBUG (G_STRFUNC);

  g_assert (DSPY_IS_INTROSPECTION_MODEL (self));
  g_assert (iter != NULL);

  node = iter ? iter->user_data : self->root;

  if (node->any.kind == DSPY_NODE_KIND_NODE)
    {
      /* Root item is the list of paths */
      if (node->any.parent == NULL)
        return node->node.nodes.length;
      else
        return 1;
    }

  if (node->any.kind == DSPY_NODE_KIND_INTERFACES)
    return node->interfaces.interfaces.length;

  if (node->any.kind == DSPY_NODE_KIND_INTERFACE)
    return 3;

  if (node->any.kind == DSPY_NODE_KIND_METHODS)
    return node->methods.methods.length;

  if (node->any.kind == DSPY_NODE_KIND_SIGNALS)
    return node->signals.signals.length;

  if (node->any.kind == DSPY_NODE_KIND_PROPERTIES)
    return node->properties.properties.length;

  return 0;
}

static gboolean
dspy_introspection_model_iter_nth_child (GtkTreeModel *model,
                                         GtkTreeIter  *iter,
                                         GtkTreeIter  *parent,
                                         gint          nth)
{
  DspyIntrospectionModel *self = (DspyIntrospectionModel *)model;
  DspyNode *cur;

  LOG_DEBUG (G_STRFUNC);

  g_assert (DSPY_IS_INTROSPECTION_MODEL (self));
  g_assert (iter != NULL);
  g_assert (nth >= 0);

  cur = parent ? parent->user_data : self->root;

  g_assert (DSPY_IS_NODE (cur));

  switch (cur->any.kind)
    {
    case DSPY_NODE_KIND_NODE:
      if (cur->any.parent == NULL)
        iter->user_data = g_queue_peek_nth (&cur->node.nodes, nth);
      else
        iter->user_data = cur->node.interfaces;
      break;

    case DSPY_NODE_KIND_METHODS:
      iter->user_data = g_queue_peek_nth (&cur->methods.methods, nth);
      break;

    case DSPY_NODE_KIND_SIGNALS:
      iter->user_data = g_queue_peek_nth (&cur->signals.signals, nth);
      break;

    case DSPY_NODE_KIND_PROPERTIES:
      iter->user_data = g_queue_peek_nth (&cur->properties.properties, nth);
      break;

    case DSPY_NODE_KIND_INTERFACES:
      iter->user_data = g_queue_peek_nth (&cur->interfaces.interfaces, nth);
      break;

    case DSPY_NODE_KIND_INTERFACE:
      if (nth == 0)
        iter->user_data = cur->interface.properties;
      else if (nth == 1)
        iter->user_data = cur->interface.signals;
      else if (nth == 2)
        iter->user_data = cur->interface.methods;
      break;

    case DSPY_NODE_KIND_ARG:
    case DSPY_NODE_KIND_METHOD:
    case DSPY_NODE_KIND_SIGNAL:
    case DSPY_NODE_KIND_PROPERTY:
    case DSPY_NODE_KIND_LAST:
    default:
      return FALSE;
    }

  return iter->user_data != NULL;
}

static void
tree_model_iface_init (GtkTreeModelIface *iface)
{
  iface->get_column_type = dspy_introspection_model_get_column_type;
  iface->get_iter = dspy_introspection_model_get_iter;
  iface->get_flags = dspy_introspection_model_get_flags;
  iface->get_n_columns = dspy_introspection_model_get_n_columns;
  iface->get_path = dspy_introspection_model_get_path;
  iface->get_value = dspy_introspection_model_get_value;
  iface->iter_children = dspy_introspection_model_iter_children;
  iface->iter_has_child = dspy_introspection_model_iter_has_child;
  iface->iter_n_children = dspy_introspection_model_iter_n_children;
  iface->iter_nth_child = dspy_introspection_model_iter_nth_child;
  iface->iter_next = dspy_introspection_model_iter_next;
  iface->iter_parent = dspy_introspection_model_iter_parent;
}

G_DEFINE_TYPE_WITH_CODE (DspyIntrospectionModel, dspy_introspection_model, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, async_initable_iface_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL, tree_model_iface_init))

enum {
  PROP_0,
  PROP_NAME,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

DspyIntrospectionModel *
_dspy_introspection_model_new (DspyName *name)
{
  g_return_val_if_fail (DSPY_IS_NAME (name), NULL);

  return g_object_new (DSPY_TYPE_INTROSPECTION_MODEL,
                       "name", name,
                       NULL);
}

static void
dspy_introspection_model_finalize (GObject *object)
{
  DspyIntrospectionModel *self = (DspyIntrospectionModel *)object;

  g_clear_object (&self->cancellable);
  g_clear_object (&self->name);
  g_clear_pointer (&self->chunks, g_string_chunk_free);
  g_clear_pointer (&self->root, _dspy_node_free);
  g_mutex_clear (&self->chunks_mutex);

  G_OBJECT_CLASS (dspy_introspection_model_parent_class)->finalize (object);
}

static void
dspy_introspection_model_dispose (GObject *object)
{
  DspyIntrospectionModel *self = (DspyIntrospectionModel *)object;

  g_cancellable_cancel (self->cancellable);

  G_OBJECT_CLASS (dspy_introspection_model_parent_class)->dispose (object);
}

static void
dspy_introspection_model_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  DspyIntrospectionModel *self = DSPY_INTROSPECTION_MODEL (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_object (value, dspy_introspection_model_get_name (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
dspy_introspection_model_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  DspyIntrospectionModel *self = DSPY_INTROSPECTION_MODEL (object);

  switch (prop_id)
    {
    case PROP_NAME:
      self->name = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
dspy_introspection_model_class_init (DspyIntrospectionModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = dspy_introspection_model_dispose;
  object_class->finalize = dspy_introspection_model_finalize;
  object_class->get_property = dspy_introspection_model_get_property;
  object_class->set_property = dspy_introspection_model_set_property;

  properties [PROP_NAME] =
    g_param_spec_object ("name",
                         "Name",
                         "The DspyName to introspect",
                         DSPY_TYPE_NAME,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
dspy_introspection_model_init (DspyIntrospectionModel *self)
{
  self->cancellable = g_cancellable_new ();
  self->chunks = g_string_chunk_new (4096L * 4);
  self->root = _dspy_node_new_root ();
  g_mutex_init (&self->chunks_mutex);
}

/**
 * dspy_introspection_model_get_name:
 *
 * Gets the #DspyName that is being introspected.
 *
 * Returns: (transfer none): a #DspyName
 */
DspyName *
dspy_introspection_model_get_name (DspyIntrospectionModel *self)
{
  g_return_val_if_fail (DSPY_IS_INTROSPECTION_MODEL (self), NULL);

  return self->name;
}
