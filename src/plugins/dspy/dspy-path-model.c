/* dspy-path-model.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "dspy-path-model"

#include "config.h"

#include <errno.h>
#include <glib/gi18n.h>
#include <stdlib.h>

#include "dspy-path-model.h"

struct _DspyPathModel
{
  GtkTreeStore     store;
  GCancellable    *cancellable;
  GDBusConnection *connection;
  DspyName        *name;
};

G_DEFINE_TYPE (DspyPathModel, dspy_path_model, GTK_TYPE_TREE_STORE)

static GHashTable *simple_types;

static void dspy_path_model_introspect (DspyPathModel *self,
                                        const gchar   *path);

static gboolean
arg_name_is_generated (const gchar *str)
{
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

static void
add_paren (GString *str,
           gchar    paren)
{
  g_string_append_printf (str, "<span fgalpha='49000'>%c</span>", paren);
}

static gint
compare_iface (gconstpointer a,
               gconstpointer b)
{
  const GDBusInterfaceInfo * const *info1 = a;
  const GDBusInterfaceInfo * const *info2 = b;

  return g_strcmp0 ((*info1)->name, (*info2)->name);
}

static gint
compare_method (gconstpointer a,
                gconstpointer b)
{
  const GDBusMethodInfo * const *info1 = a;
  const GDBusMethodInfo * const *info2 = b;

  return g_strcmp0 ((*info1)->name, (*info2)->name);
}

static gint
compare_property (gconstpointer a,
                  gconstpointer b)
{
  const GDBusPropertyInfo * const *info1 = a;
  const GDBusPropertyInfo * const *info2 = b;

  return g_strcmp0 ((*info1)->name, (*info2)->name);
}

static gint
compare_signal (gconstpointer a,
                gconstpointer b)
{
  const GDBusSignalInfo * const *info1 = a;
  const GDBusSignalInfo * const *info2 = b;

  return g_strcmp0 ((*info1)->name, (*info2)->name);
}

static void
dspy_path_model_finalize (GObject *object)
{
  DspyPathModel *self = (DspyPathModel *)object;

  g_clear_object (&self->cancellable);
  g_clear_object (&self->connection);
  g_clear_object (&self->name);

  G_OBJECT_CLASS (dspy_path_model_parent_class)->finalize (object);
}

static void
add_signature (GString     *str,
               const gchar *signature)
{
  const gchar *tmp;

  /* TODO: decode signature into human text */

  if ((tmp = g_hash_table_lookup (simple_types, signature)))
    signature = tmp;

  g_string_append_printf (str, "<span weight='bold' fgalpha='40000'>%s</span>", signature);
}

static gchar *
prop_to_string (GDBusPropertyInfo *prop)
{
  GString *str = g_string_new (NULL);

  g_string_append (str, prop->name);

  g_string_append_c (str, ' ');
  add_signature (str, prop->signature);

  g_string_append_c (str, ' ');
  g_string_append (str, "<span size='smaller' fgalpha='32767'>(");
  if (prop->flags == (G_DBUS_PROPERTY_INFO_FLAGS_READABLE | G_DBUS_PROPERTY_INFO_FLAGS_WRITABLE))
    g_string_append (str, _("read/write"));
  else if (prop->flags == G_DBUS_PROPERTY_INFO_FLAGS_READABLE)
    g_string_append (str, _("read-only"));
  else if (prop->flags == G_DBUS_PROPERTY_INFO_FLAGS_WRITABLE)
    g_string_append (str, _("write-only"));
  g_string_append (str, ")</span>");

  return g_string_free (str, FALSE);
}

static void
add_arg_name (GString     *str,
              const gchar *name)
{
  g_string_append_printf (str, "<span fgalpha='32767'>%s</span>", name);
}

static gchar *
method_to_string (GDBusMethodInfo *method)
{
  GString *str = g_string_new (NULL);

  g_string_append (str, method->name);

  g_string_append_c (str, ' ');
  add_paren (str, '(');

  for (guint i = 0; method->in_args[i] != NULL; i++)
    {
      GDBusArgInfo *arg = method->in_args[i];

      if (i > 0)
        g_string_append (str, ", ");

      add_signature (str, arg->signature);

      if (!arg_name_is_generated (arg->name))
        {
          g_string_append_c (str, ' ');
          add_arg_name (str, arg->name);
        }
    }

  add_paren (str, ')');
  g_string_append (str, " ↦ ");
  add_paren (str, '(');

  for (guint i = 0; method->out_args[i] != NULL; i++)
    {
      GDBusArgInfo *arg = method->out_args[i];

      if (i > 0)
        g_string_append (str, ", ");

      add_signature (str, arg->signature);

      if (!arg_name_is_generated (arg->name))
        {
          g_string_append_c (str, ' ');
          add_arg_name (str, arg->name);
        }
    }

  add_paren (str, ')');

  return g_string_free (str, FALSE);
}

static gchar *
signal_to_string (GDBusSignalInfo *sig)
{
  GString *str = g_string_new (NULL);

  g_string_append (str, sig->name);
  g_string_append (str, " (");

  for (guint i = 0; sig->args[i] != NULL; i++)
    {
      GDBusArgInfo *arg = sig->args[i];

      if (i > 0)
        g_string_append (str, ", ");

      add_signature (str, arg->signature);

      if (!arg_name_is_generated (arg->name))
        {
          g_string_append_c (str, ' ');
          add_arg_name (str, arg->name);
        }
    }

  g_string_append_c (str, ')');

  return g_string_free (str, FALSE);
}

static void
dspy_path_model_dispose (GObject *object)
{
  DspyPathModel *self = (DspyPathModel *)object;

  g_cancellable_cancel (self->cancellable);

  G_OBJECT_CLASS (dspy_path_model_parent_class)->dispose (object);
}

static void
dspy_path_model_class_init (DspyPathModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = dspy_path_model_dispose;
  object_class->finalize = dspy_path_model_finalize;

  simple_types = g_hash_table_new (g_str_hash, g_str_equal);

#define INSERT(k,v) \
  g_hash_table_insert (simple_types, (gchar *)k, (gchar *)v)
  INSERT ("n",     "int16");
  INSERT ("q",     "uint16");
  INSERT ("i",     "int32");
  INSERT ("u",     "uint32");
  INSERT ("x",     "int64");
  INSERT ("t",     "uint64");
  INSERT ("s",     "string");
  INSERT ("b",     "boolean");
  INSERT ("y",     "byte");
  INSERT ("o",     "Object Path");
  INSERT ("g",     "Signature");
  INSERT ("d",     "double");
  INSERT ("v",     "Variant");
  INSERT ("h",     "File Descriptor");
  INSERT ("as",    "string[]");
  INSERT ("a{sv}", "Vardict");
  INSERT ("ay",    "Byte Array");
#undef INSERT
}

static void
dspy_path_model_init (DspyPathModel *self)
{
  GType types[] = { G_TYPE_STRING };

  gtk_tree_store_set_column_types (GTK_TREE_STORE (self), G_N_ELEMENTS (types), types);

  self->cancellable = g_cancellable_new ();
}

static void
dspy_path_model_introspect_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  GDBusConnection *connection = (GDBusConnection *)object;
  g_autoptr(GDBusNodeInfo) node_info = NULL;
  g_autoptr(GVariant) ret = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = user_data;
  DspyPathModel *self;
  const gchar *path;
  const gchar *xml;

  g_assert (G_IS_DBUS_CONNECTION (connection));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!(ret = g_dbus_connection_call_finish (connection, result, &error)))
    {
      /* XXX: We might not be authorized, we should propagate that to user */
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  self = g_task_get_source_object (task);
  path = g_task_get_task_data (task);
  xml = NULL;

  g_variant_get (ret, "(&s)", &xml);

  if (!(node_info = g_dbus_node_info_new_for_xml (xml, &error)))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  /* First, queue up all the paths to load while we keep ourselves
   * busy filling out this part of the tree.
   */
  for (guint i = 0; node_info->nodes[i] != NULL; i++)
    {
      g_autofree gchar *subpath = NULL;

      subpath = g_strdup_printf ("%s/%s",
                                 g_str_equal (path, "/") ? "" : path,
                                 node_info->nodes[i]->path);
      dspy_path_model_introspect (self, subpath);
    }

  if (node_info->interfaces[0] != NULL)
    {
      GtkTreeIter iter;
      GtkTreeIter ifaceiter;
      GtkTreeIter groupiter;

      qsort (node_info->interfaces,
             g_strv_length ((gchar **)node_info->interfaces),
             sizeof (gpointer),
             compare_iface);

      gtk_tree_store_append (GTK_TREE_STORE (self), &iter, NULL);
      gtk_tree_store_set (GTK_TREE_STORE (self), &iter,
                          0, path,
                          -1);

      gtk_tree_store_append (GTK_TREE_STORE (self), &groupiter, &iter);
      gtk_tree_store_set (GTK_TREE_STORE (self), &groupiter,
                          0, _("<b>Interfaces</b>"),
                          -1);

      for (guint i = 0; node_info->interfaces[i] != NULL; i++)
        {
          GDBusInterfaceInfo *iface = node_info->interfaces[i];

          gtk_tree_store_append (GTK_TREE_STORE (self), &ifaceiter, &groupiter);
          gtk_tree_store_set (GTK_TREE_STORE (self), &ifaceiter,
                              0, iface->name,
                              -1);

          if (iface->properties[0] != NULL)
            {
              GtkTreeIter propsiter;

              gtk_tree_store_append (GTK_TREE_STORE (self), &propsiter, &ifaceiter);
              gtk_tree_store_set (GTK_TREE_STORE (self), &propsiter,
                                  0, _("<b>Properties</b>"),
                                  -1);

              qsort (iface->properties,
                     g_strv_length ((gchar **)iface->properties),
                     sizeof (gpointer),
                     compare_property);

              for (guint j = 0; iface->properties[j] != NULL; j++)
                {
                  GDBusPropertyInfo *prop = iface->properties[j];
                  g_autofree gchar *propstr = prop_to_string (prop);
                  GtkTreeIter propiter;

                  gtk_tree_store_append (GTK_TREE_STORE (self), &propiter, &propsiter);
                  gtk_tree_store_set (GTK_TREE_STORE (self), &propiter,
                                      0, propstr,
                                      -1);
                }
            }

          if (iface->signals[0] != NULL)
            {
              GtkTreeIter signalsiter;

              gtk_tree_store_append (GTK_TREE_STORE (self), &signalsiter, &ifaceiter);
              gtk_tree_store_set (GTK_TREE_STORE (self), &signalsiter,
                                  0, _("<b>Signals</b>"),
                                  -1);

              qsort (iface->signals,
                     g_strv_length ((gchar **)iface->signals),
                     sizeof (gpointer),
                     compare_signal);

              for (guint j = 0; iface->signals[j] != NULL; j++)
                {
                  GDBusSignalInfo *sig = iface->signals[j];
                  g_autofree gchar *signalstr = signal_to_string (sig);
                  GtkTreeIter signaliter;

                  gtk_tree_store_append (GTK_TREE_STORE (self), &signaliter, &signalsiter);
                  gtk_tree_store_set (GTK_TREE_STORE (self), &signaliter,
                                      0, signalstr,
                                      -1);
                }
            }

          if (iface->methods[0] != NULL)
            {
              GtkTreeIter methodsiter;

              gtk_tree_store_append (GTK_TREE_STORE (self), &methodsiter, &ifaceiter);
              gtk_tree_store_set (GTK_TREE_STORE (self), &methodsiter,
                                  0, _("<b>Methods</b>"),
                                  -1);

              qsort (iface->methods,
                     g_strv_length ((gchar **)iface->methods),
                     sizeof (gpointer),
                     compare_method);

              for (guint j = 0; iface->methods[j] != NULL; j++)
                {
                  GDBusMethodInfo *method = iface->methods[j];
                  g_autofree gchar *methodstr = method_to_string (method);
                  GtkTreeIter methoditer;

                  gtk_tree_store_append (GTK_TREE_STORE (self), &methoditer, &methodsiter);
                  gtk_tree_store_set (GTK_TREE_STORE (self), &methoditer,
                                      0, methodstr,
                                      -1);
                }
            }
        }
    }

  g_task_return_boolean (task, TRUE);
}

static void
dspy_path_model_introspect (DspyPathModel *self,
                            const gchar   *object_path)
{
  g_autoptr(GTask) task = NULL;

  g_assert (DSPY_IS_PATH_MODEL (self));
  g_assert (G_IS_DBUS_CONNECTION (self->connection));
  g_assert (DSPY_IS_NAME (self->name));
  g_assert (object_path != NULL);

  g_debug ("Introspecting DBus XML of peer %s path %s",
           dspy_name_get_owner (self->name),
           object_path);

  task = g_task_new (self, self->cancellable, NULL, NULL);
  g_task_set_task_data (task, g_strdup (object_path), g_free);

  g_dbus_connection_call (self->connection,
                          dspy_name_get_owner (self->name),
                          object_path,
                          "org.freedesktop.DBus.Introspectable",
                          "Introspect",
                          NULL, /* params */
                          G_VARIANT_TYPE ("(s)"),
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          self->cancellable,
                          dspy_path_model_introspect_cb,
                          g_steal_pointer (&task));
}

DspyPathModel *
dspy_path_model_new (GDBusConnection *connection,
                     DspyName        *name)
{
  DspyPathModel *self;

  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), NULL);
  g_return_val_if_fail (DSPY_IS_NAME (name), NULL);

  self = g_object_new (DSPY_TYPE_PATH_MODEL, NULL);
  self->connection = g_object_ref (connection);
  self->name = g_object_ref (name);

  dspy_path_model_introspect (self, "/");

  return g_steal_pointer (&self);
}
