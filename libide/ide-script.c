/* ide-script.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include <glib/gi18n.h>

#include "ide-script.h"

typedef struct
{
  GFile *file;
} IdeScriptPrivate;

static void async_initable_iface_init (GAsyncInitableIface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (IdeScript, ide_script, IDE_TYPE_OBJECT,
                                  G_ADD_PRIVATE (IdeScript)
                                  G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE,
                                                         async_initable_iface_init))

enum {
  PROP_0,
  PROP_FILE,
  LAST_PROP
};

enum {
  LOAD,
  UNLOAD,
  LAST_SIGNAL
};

static GParamSpec *gParamSpecs [LAST_PROP];
static guint       gSignals [LAST_SIGNAL];

/**
 * ide_script_get_file:
 *
 * Returns a #GFile pointing to the location of the script on disk.
 *
 * Returns: (transfer none): A #GFile
 */
GFile *
ide_script_get_file (IdeScript *self)
{
  IdeScriptPrivate *priv = ide_script_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SCRIPT (self), NULL);

  return priv->file;
}

static void
ide_script_set_file (IdeScript *self,
                     GFile     *file)
{
  IdeScriptPrivate *priv = ide_script_get_instance_private (self);

  g_return_if_fail (IDE_IS_SCRIPT (self));
  g_return_if_fail (G_IS_FILE (file));

  if (g_set_object (&priv->file, file))
    g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_FILE]);
}

void
ide_script_load (IdeScript *self)
{
  g_return_if_fail (IDE_IS_SCRIPT (self));

  g_signal_emit (self, gSignals [LOAD], 0);
}

void
ide_script_unload (IdeScript *self)
{
  g_return_if_fail (IDE_IS_SCRIPT (self));

  g_signal_emit (self, gSignals [UNLOAD], 0);
}

static void
ide_script_finalize (GObject *object)
{
  IdeScript *self = (IdeScript *)object;
  IdeScriptPrivate *priv = ide_script_get_instance_private (self);

  g_clear_object (&priv->file);

  G_OBJECT_CLASS (ide_script_parent_class)->finalize (object);
}

static void
ide_script_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  IdeScript *self = IDE_SCRIPT(object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, ide_script_get_file (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
ide_script_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  IdeScript *self = IDE_SCRIPT(object);

  switch (prop_id)
    {
    case PROP_FILE:
      ide_script_set_file (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
ide_script_class_init (IdeScriptClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_script_finalize;
  object_class->get_property = ide_script_get_property;
  object_class->set_property = ide_script_set_property;

  gParamSpecs [PROP_FILE] =
    g_param_spec_object ("file",
                         _("File"),
                         _("The file containing the script."),
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_FILE,
                                   gParamSpecs [PROP_FILE]);

  gSignals [LOAD] =
    g_signal_new ("load",
                  IDE_TYPE_SCRIPT,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeScriptClass, load),
                  NULL,
                  NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE,
                  0);

  gSignals [UNLOAD] =
    g_signal_new ("unload",
                  IDE_TYPE_SCRIPT,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeScriptClass, unload),
                  NULL,
                  NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE,
                  0);
}

static void
ide_script_init (IdeScript *self)
{
}

static void
ide_script_init_async (GAsyncInitable      *initable,
                       gint                 io_priority,
                       GCancellable        *cancellable,
                       GAsyncReadyCallback  callback,
                       gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (G_IS_ASYNC_INITABLE (initable));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (initable, cancellable, callback, user_data);
  g_task_return_new_error (task,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           _("%s has not implemented GAsyncInitable."),
                           g_type_name (G_TYPE_FROM_INSTANCE (initable)));
}

static gboolean
ide_script_init_finish (GAsyncInitable  *initable,
                        GAsyncResult    *result,
                        GError         **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (G_IS_TASK (task), FALSE);

  return g_task_propagate_boolean (task, error);
}

static void
async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = ide_script_init_async;
  iface->init_finish = ide_script_init_finish;
}
