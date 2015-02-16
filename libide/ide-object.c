/* ide-object.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>

#include "ide-context.h"
#include "ide-object.h"

typedef struct
{
  IdeContext *context;
} IdeObjectPrivate;

typedef struct
{
  GTask *task;
  GList *objects;
  GList *iter;
  int    io_priority;
} InitAsyncState;

static void ide_object_new_async_try_next (InitAsyncState *state);

G_DEFINE_TYPE_WITH_PRIVATE (IdeObject, ide_object, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_CONTEXT,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

/**
 * ide_object_get_context:
 *
 * Fetches the #IdeObject:context property.
 *
 * Returns: (transfer none): An #IdeContext.
 */
IdeContext *
ide_object_get_context (IdeObject *object)
{
  IdeObjectPrivate *priv = ide_object_get_instance_private (object);

  g_return_val_if_fail (IDE_IS_OBJECT (object), NULL);

  return priv->context;
}

static void
ide_object_set_context (IdeObject  *object,
                        IdeContext *context)
{
  IdeObjectPrivate *priv = ide_object_get_instance_private (object);

  g_return_if_fail (IDE_IS_OBJECT (object));
  g_return_if_fail (IDE_IS_CONTEXT (context));

  ide_set_weak_pointer (&priv->context, context);
}

static void
ide_object_finalize (GObject *object)
{
  IdeObject *self = (IdeObject *)object;
  IdeObjectPrivate *priv = ide_object_get_instance_private (self);

  if (priv->context)
    {
      g_object_remove_weak_pointer (G_OBJECT (priv->context),
                                    (gpointer *)&priv->context);
      priv->context = NULL;
    }

  G_OBJECT_CLASS (ide_object_parent_class)->finalize (object);
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

  object_class->finalize = ide_object_finalize;
  object_class->get_property = ide_object_get_property;
  object_class->set_property = ide_object_set_property;

  gParamSpecs [PROP_CONTEXT] =
    g_param_spec_object ("context",
                         _("Context"),
                         _("The context that owns the object."),
                         IDE_TYPE_CONTEXT,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_CONTEXT,
                                   gParamSpecs [PROP_CONTEXT]);
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
                               _("No implementations of extension point."));
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
