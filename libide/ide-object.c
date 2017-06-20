/* ide-object.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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
 */

#define G_LOG_DOMAIN "ide-object"

#include <glib/gi18n.h>
#include <libpeas/peas.h>

#include "ide-context.h"
#include "ide-debug.h"
#include "ide-object.h"

typedef struct
{
  IdeContext *context;
  guint       is_destroyed : 1;
} IdeObjectPrivate;

typedef struct
{
  GTask *task;            /* back pointer */
  GList *objects;         /* list of objects to try */
  GList *iter;            /* current iter of objects */
  gchar *extension_point; /* name of extension point */
  int    io_priority;
} InitAsyncState;

typedef struct
{
  GPtrArray *plugins;
  GType      plugin_type;
  gint       position;
  gint       io_priority;
} InitExtensionAsyncState;

static void ide_object_new_async_try_next (InitAsyncState *state);
static void ide_object_new_for_extension_async_try_next (GTask *task);

G_DEFINE_TYPE_WITH_PRIVATE (IdeObject, ide_object, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_CONTEXT,
  LAST_PROP
};

enum {
  DESTROY,
  LAST_SIGNAL
};

static GParamSpec *properties [LAST_PROP];
static guint signals [LAST_SIGNAL];

static void
ide_object_destroy (IdeObject *self)
{
  IdeObjectPrivate *priv = ide_object_get_instance_private (self);

  g_assert (IDE_IS_OBJECT (self));

  if (!priv->is_destroyed)
    {
      priv->is_destroyed = TRUE;
      g_signal_emit (self, signals [DESTROY], 0);
    }
}

static void
ide_object_release_context (gpointer  data,
                            GObject  *where_the_object_was)
{
  IdeObject *self = data;
  IdeObjectPrivate *priv = ide_object_get_instance_private (self);

  g_assert (IDE_IS_OBJECT (self));

  priv->context = NULL;

  ide_object_destroy (self);
}

static IdeContext *
ide_object_real_get_context (IdeObject *self)
{
  IdeObjectPrivate *priv = ide_object_get_instance_private (self);

  return priv->context;
}

/**
 * ide_object_get_context:
 *
 * Fetches the #IdeObject:context property.
 *
 * Returns: (transfer none): An #IdeContext.
 */
IdeContext *
ide_object_get_context (IdeObject *self)
{
  g_return_val_if_fail (IDE_IS_OBJECT (self), NULL);

  return IDE_OBJECT_GET_CLASS (self)->get_context (self);
}

void
ide_object_set_context (IdeObject  *self,
                        IdeContext *context)
{
  g_return_if_fail (IDE_IS_OBJECT (self));
  g_return_if_fail (!context || IDE_IS_CONTEXT (context));

  IDE_OBJECT_GET_CLASS (self)->set_context (self, context);
}

static void
ide_object_real_set_context (IdeObject  *self,
                             IdeContext *context)
{
  IdeObjectPrivate *priv = ide_object_get_instance_private (self);

  g_assert (IDE_IS_OBJECT (self));
  g_assert (!context || IDE_IS_CONTEXT (context));

  if (context != priv->context)
    {
      if (priv->context != NULL)
        {
          g_object_weak_unref (G_OBJECT (priv->context),
                               ide_object_release_context,
                               self);
          priv->context = NULL;
        }

      if (context != NULL)
        {
          priv->context = context;
          g_object_weak_ref (G_OBJECT (priv->context),
                             ide_object_release_context,
                             self);
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CONTEXT]);
    }
}

static void
ide_object_dispose (GObject *object)
{
  IdeObject *self = (IdeObject *)object;
  IdeObjectPrivate *priv = ide_object_get_instance_private (self);

  G_OBJECT_CLASS (ide_object_parent_class)->dispose (object);

  IDE_TRACE_MSG ("%s (%p)",
                 g_type_name (G_TYPE_FROM_INSTANCE (object)),
                 object);

  if (priv->context != NULL)
    ide_object_set_context (self, NULL);

  if (!priv->is_destroyed)
    ide_object_destroy (self);
}

static void
ide_object_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  IdeObject *self = IDE_OBJECT (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, ide_object_get_context (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_object_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  IdeObject *self = IDE_OBJECT (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      ide_object_set_context (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_object_class_init (IdeObjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_object_dispose;
  object_class->get_property = ide_object_get_property;
  object_class->set_property = ide_object_set_property;

  klass->get_context = ide_object_real_get_context;
  klass->set_context = ide_object_real_set_context;

  properties [PROP_CONTEXT] =
    g_param_spec_object ("context",
                         "Context",
                         "The context that owns the object.",
                         IDE_TYPE_CONTEXT,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  signals [DESTROY] =
    g_signal_new ("destroy",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeObjectClass, destroy),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);
}

static void
ide_object_init (IdeObject *self)
{
}

static void
init_async_state_free (gpointer data)
{
  InitAsyncState *state = data;

  if (state)
    {
      g_free (state->extension_point);
      g_list_foreach (state->objects, (GFunc)g_object_unref, NULL);
      g_list_free (state->objects);
      g_slice_free (InitAsyncState, state);
    }
}

static void
ide_object_init_async_cb (GObject      *source_object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  InitAsyncState *state = user_data;
  IdeObject *object = (IdeObject *)source_object;
  GError *error = NULL;

  g_return_if_fail (!object || IDE_IS_OBJECT (object));
  g_return_if_fail (state);

  if (!g_async_initable_init_finish (G_ASYNC_INITABLE (object), result, &error))
    {
      ide_object_new_async_try_next (state);
      return;
    }

  g_task_return_pointer (state->task, g_object_ref (object), g_object_unref);
  g_object_unref (state->task);
}

static void
ide_object_new_async_try_next (InitAsyncState *state)
{
  IdeObject *object;

  g_return_if_fail (state);

  if (!state->iter)
    {
      g_task_return_new_error (state->task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               _("No implementations of extension point “%s”."),
                               state->extension_point);
      g_object_unref (state->task);
      return;
    }

  object = state->iter->data;
  state->iter = state->iter->next;

  g_async_initable_init_async (G_ASYNC_INITABLE (object),
                               state->io_priority,
                               g_task_get_cancellable (state->task),
                               ide_object_init_async_cb,
                               state);
}

static void
extension_async_state_free (gpointer data)
{
  InitExtensionAsyncState *state = data;

  g_ptr_array_unref (state->plugins);
  g_slice_free (InitExtensionAsyncState, state);
}

static void
extensions_foreach_cb (PeasExtensionSet *set,
                       PeasPluginInfo   *plugin_info,
                       PeasExtension    *exten,
                       gpointer          user_data)
{
  InitExtensionAsyncState *state = user_data;

  g_assert (state != NULL);
  g_assert (state->plugins != NULL);

  if (!G_IS_ASYNC_INITABLE (exten))
    {
      g_warning ("\"%s\" does not implement GAsyncInitable. Ignoring extension.",
                 G_OBJECT_TYPE_NAME (exten));
      return;
    }

  g_ptr_array_add (state->plugins, g_object_ref (exten));
}

static void
extension_init_cb (GObject      *object,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  g_autoptr(GTask) task = user_data;
  GAsyncInitable *initable = (GAsyncInitable *)object;
  g_autoptr(GError) error = NULL;
  InitExtensionAsyncState *state;

  IDE_ENTRY;

  g_assert (G_IS_TASK (task));
  g_assert (G_IS_ASYNC_INITABLE (initable));

  state = g_task_get_task_data (task);

  if (!g_async_initable_init_finish (initable, result, &error))
    {
      if (error == NULL)
        error = g_error_new (G_IO_ERROR,
                             G_IO_ERROR_FAILED,
                             "Unknown error while initializing %s",
                             G_OBJECT_TYPE_NAME (initable));

      IDE_TRACE_MSG ("extension for %s failed to initialize: %s",
                     G_OBJECT_TYPE_NAME (initable), error->message);

      if ((guint)state->position == state->plugins->len)
        {
          g_task_return_error (task, g_steal_pointer (&error));
          IDE_EXIT;
        }

      ide_object_new_for_extension_async_try_next (task);
      IDE_EXIT;
    }

  IDE_TRACE_MSG ("initialization of %s was successful", G_OBJECT_TYPE_NAME (initable));

  g_task_return_pointer (task, g_object_ref (initable), g_object_unref);

  IDE_EXIT;
}

static void
ide_object_new_for_extension_async_try_next (GTask *task)
{
  InitExtensionAsyncState *state;
  GAsyncInitable *initable;

  IDE_ENTRY;

  g_assert (G_IS_TASK (task));

  state = g_task_get_task_data (task);

  if ((guint)state->position == state->plugins->len)
    {
      IDE_TRACE_MSG ("No more %s extensions to try", g_type_name (state->plugin_type));
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               _("Failed to locate %s plugin."),
                               g_type_name (state->plugin_type));
      IDE_EXIT;
    }

  initable = g_ptr_array_index (state->plugins, state->position++);

  IDE_TRACE_MSG ("Initializing object of type %s", G_OBJECT_TYPE_NAME (initable));

  g_async_initable_init_async (initable,
                               state->io_priority,
                               g_task_get_cancellable (task),
                               extension_init_cb,
                               g_object_ref (task));

  IDE_EXIT;
}

/**
 * ide_object_new_for_extension_async:
 * @sort_priority_func: (scope call) (allow-none): A #GCompareDataFunc or %NULL.
 *
 */
void
ide_object_new_for_extension_async (GType                 interface_gtype,
                                    GCompareDataFunc      sort_priority_func,
                                    gpointer              sort_priority_data,
                                    int                   io_priority,
                                    GCancellable         *cancellable,
                                    GAsyncReadyCallback   callback,
                                    gpointer              user_data,
                                    const gchar          *first_property,
                                    ...)
{
  PeasEngine *engine;
  PeasExtensionSet *set;
  g_autoptr(GTask) task = NULL;
  InitExtensionAsyncState *state;
  va_list args;

  g_return_if_fail (G_TYPE_IS_INTERFACE (interface_gtype));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  engine = peas_engine_get_default ();

  va_start (args, first_property);
  set = peas_extension_set_new_valist (engine, interface_gtype, first_property, args);
  va_end (args);

  task = g_task_new (NULL, cancellable, callback, user_data);

  state = g_slice_new0 (InitExtensionAsyncState);
  state->plugins = g_ptr_array_new_with_free_func (g_object_unref);
  state->position = 0;
  state->io_priority = io_priority;
  state->plugin_type = interface_gtype;

  peas_extension_set_foreach (set, extensions_foreach_cb, state);

  if (sort_priority_func != NULL)
    g_ptr_array_sort_with_data (state->plugins, sort_priority_func, sort_priority_data);

#ifdef IDE_ENABLE_TRACE
  for (guint i = 0; i < state->plugins->len; i++)
    {
      gpointer instance = g_ptr_array_index (state->plugins, i);
      IDE_TRACE_MSG (" Plugin[%u] = %s", i, G_OBJECT_TYPE_NAME (instance));
    }
#endif

  g_task_set_task_data (task, state, extension_async_state_free);

  ide_object_new_for_extension_async_try_next (task);

  g_clear_object (&set);
}

void
ide_object_new_async (const gchar          *extension_point,
                      int                   io_priority,
                      GCancellable         *cancellable,
                      GAsyncReadyCallback   callback,
                      gpointer              user_data,
                      const gchar          *first_property,
                      ...)
{
  GIOExtensionPoint *point;
  InitAsyncState *state;
  const GList *extensions;
  const GList *iter;
  va_list args;

  g_return_if_fail (extension_point);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  point = g_io_extension_point_lookup (extension_point);

  if (!point)
    {
      g_task_report_new_error (NULL, callback, user_data, ide_object_new_async,
                               G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                               _("No such extension point."));
      return;
    }

  extensions = g_io_extension_point_get_extensions (point);

  if (!extensions)
    {
      g_task_report_new_error (NULL, callback, user_data, ide_object_new_async,
                               G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                               _("No implementations of extension point."));
      return;
    }

  state = g_slice_new0 (InitAsyncState);
  state->extension_point = g_strdup (extension_point);
  state->io_priority = io_priority;
  state->task = g_task_new (NULL, cancellable, callback, user_data);
  g_task_set_task_data (state->task, state, init_async_state_free);

  for (iter = extensions; iter; iter = iter->next)
    {
      GIOExtension *extension = iter->data;
      GObject *object;
      GType type_id;

      type_id = g_io_extension_get_type (extension);

      if (!g_type_is_a (type_id, G_TYPE_ASYNC_INITABLE))
        continue;

      va_start (args, first_property);
      object = g_object_new_valist (type_id, first_property, args);
      va_end (args);

      state->objects = g_list_append (state->objects, object);

      if (!state->iter)
        state->iter = state->objects;
    }

  ide_object_new_async_try_next (state);
}

IdeObject *
ide_object_new_finish  (GAsyncResult  *result,
                        GError       **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (G_IS_TASK (task), NULL);

  return g_task_propagate_pointer (task, error);
}

/**
 * ide_object_hold:
 * @self: the #IdeObject
 *
 * This function will acquire a reference to the IdeContext that the object
 * is a part of. This is useful if you are going to be doing a long running
 * task (such as something in a thread) and want to ensure the context cannot
 * be unloaded during your operation.
 *
 * You should call ide_object_release() an equivalent number of times to
 * ensure the context may be freed afterwards.
 *
 * You should check the return value of this function to ensure that the
 * context is not already in shutdown.
 *
 * Returns: %TRUE if a hold was successfully created.
 */
gboolean
ide_object_hold (IdeObject *self)
{
  IdeObjectPrivate *priv = ide_object_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_OBJECT (self), FALSE);

  if (priv->context != NULL)
    {
      ide_context_hold (priv->context);
      return TRUE;
    }

  return FALSE;
}

/**
 * ide_object_release:
 * @self: the #IdeObject.
 *
 * Releases a successful hold on the context previously created with ide_object_hold().
 */
void
ide_object_release (IdeObject *self)
{
  IdeObjectPrivate *priv = ide_object_get_instance_private (self);

  g_return_if_fail (IDE_IS_OBJECT (self));

  if (priv->context == NULL)
    {
      IDE_BUG ("libide", "Called after context was released.");
      return;
    }

  ide_context_release (priv->context);
}

static gboolean
ide_object_notify_in_main_cb (gpointer data)
{
  struct {
    GObject    *object;
    GParamSpec *pspec;
  } *notify = data;

  g_assert (notify != NULL);
  g_assert (G_IS_OBJECT (notify->object));
  g_assert (notify->pspec != NULL);

  g_object_notify_by_pspec (notify->object, notify->pspec);

  g_object_unref (notify->object);
  g_param_spec_unref (notify->pspec);
  g_slice_free1 (sizeof *notify, notify);

  return G_SOURCE_REMOVE;
}

void
ide_object_notify_in_main (gpointer    instance,
                           GParamSpec *pspec)
{
  struct {
    GObject    *object;
    GParamSpec *pspec;
  } *notify;

  g_return_if_fail (G_IS_OBJECT (instance));
  g_return_if_fail (pspec != NULL);

  /*
   * Short circuit if we can notify immediately without the round trip
   * to the main loop.
   */
  if G_LIKELY (g_main_context_get_thread_default () == g_main_context_default ())
    {
      g_object_notify_by_pspec (instance, pspec);
      return;
    }

  notify = g_slice_alloc0 (sizeof *notify);
  notify->object = g_object_ref (instance);
  notify->pspec = g_param_spec_ref (pspec);

  g_timeout_add (0, ide_object_notify_in_main_cb, notify);
}
