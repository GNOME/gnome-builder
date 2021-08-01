/* ide-pkcon-transfer.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-pkcon-transfer"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-threading.h>

#include "ide-pkcon-transfer.h"

struct _IdePkconTransfer
{
  IdeTransfer   parent;
  gchar       **packages;
  gchar        *status;
};

enum {
  PROP_0,
  PROP_PACKAGES,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdePkconTransfer, ide_pkcon_transfer, IDE_TYPE_TRANSFER)

static GParamSpec *properties [N_PROPS];

static void
ide_pkcon_transfer_update_title (IdePkconTransfer *self)
{
  g_autofree gchar *title = NULL;
  guint count;

  g_assert (IDE_IS_PKCON_TRANSFER (self));

  count = g_strv_length (self->packages);
  title = g_strdup_printf (ngettext ("Installing %u package", "Installing %u packages", count), count);
  ide_transfer_set_title (IDE_TRANSFER (self), title);
}

static void
ide_pkcon_transfer_wait_check_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_subprocess_wait_check_finish (subprocess, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);
}

static void
ide_pkcon_transfer_read_line_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  GDataInputStream *stream = (GDataInputStream *)object;
  g_autoptr(IdePkconTransfer) self = user_data;
  g_autofree gchar *line = NULL;
  g_auto(GStrv) parts = NULL;
  gsize len;

  g_assert (G_IS_DATA_INPUT_STREAM (stream));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_PKCON_TRANSFER (self));

  if (!(line = g_data_input_stream_read_line_finish_utf8 (stream, result, &len, NULL)))
    return;

  parts = g_strsplit (line, ":", 2);

  if (parts[0]) g_strstrip (parts[0]);
  if (parts[1]) g_strstrip (parts[1]);

  if (g_strcmp0 (parts[0], "Status") == 0)
    ide_transfer_set_status (IDE_TRANSFER (self), parts[1]);
  else if (g_strcmp0 (parts[0], "Percentage") == 0 && parts[1])
    ide_transfer_set_progress (IDE_TRANSFER (self), g_strtod (parts[1], NULL) / 100.0);

  g_data_input_stream_read_line_async (stream,
                                       G_PRIORITY_DEFAULT,
                                       NULL,
                                       ide_pkcon_transfer_read_line_cb,
                                       g_object_ref (self));
}

static void
ide_pkcon_transfer_execute_async (IdeTransfer         *transfer,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  IdePkconTransfer *self = (IdePkconTransfer *)transfer;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(GDataInputStream) data_stream = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;
  GInputStream *stdout_stream;

  IDE_ENTRY;

  g_assert (IDE_IS_TRANSFER (transfer));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_pkcon_transfer_execute_async);

  if (self->packages == NULL || !self->packages[0])
    {
      ide_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);
  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
  ide_subprocess_launcher_push_argv (launcher, "pkcon");
  ide_subprocess_launcher_push_argv (launcher, "install");
  ide_subprocess_launcher_push_argv (launcher, "-y");
  ide_subprocess_launcher_push_argv (launcher, "-p");

  for (guint i = 0; self->packages[i]; i++)
    ide_subprocess_launcher_push_argv (launcher, self->packages[i]);

  subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, &error);

  if (subprocess == NULL)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  stdout_stream = ide_subprocess_get_stdout_pipe (subprocess);
  data_stream = g_data_input_stream_new (stdout_stream);

  g_data_input_stream_read_line_async (data_stream,
                                       G_PRIORITY_DEFAULT,
                                       cancellable,
                                       ide_pkcon_transfer_read_line_cb,
                                       g_object_ref (self));

  ide_subprocess_wait_check_async (subprocess,
                                   cancellable,
                                   ide_pkcon_transfer_wait_check_cb,
                                   g_steal_pointer (&task));

  IDE_EXIT;
}

static gboolean
ide_pkcon_transfer_execute_finish (IdeTransfer   *transfer,
                                   GAsyncResult  *result,
                                   GError       **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_TRANSFER (transfer));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_pkcon_transfer_finalize (GObject *object)
{
  IdePkconTransfer *self = (IdePkconTransfer *)object;

  g_clear_pointer (&self->packages, g_strfreev);
  g_clear_pointer (&self->status, g_free);

  G_OBJECT_CLASS (ide_pkcon_transfer_parent_class)->finalize (object);
}

static void
ide_pkcon_transfer_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  IdePkconTransfer *self = IDE_PKCON_TRANSFER (object);

  switch (prop_id)
    {
    case PROP_PACKAGES:
      g_value_set_boxed (value, self->packages);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_pkcon_transfer_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  IdePkconTransfer *self = IDE_PKCON_TRANSFER (object);

  switch (prop_id)
    {
    case PROP_PACKAGES:
      self->packages = g_value_dup_boxed (value);
      ide_pkcon_transfer_update_title (self);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_pkcon_transfer_class_init (IdePkconTransferClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeTransferClass *transfer_class = IDE_TRANSFER_CLASS (klass);

  object_class->finalize = ide_pkcon_transfer_finalize;
  object_class->get_property = ide_pkcon_transfer_get_property;
  object_class->set_property = ide_pkcon_transfer_set_property;

  transfer_class->execute_async = ide_pkcon_transfer_execute_async;
  transfer_class->execute_finish = ide_pkcon_transfer_execute_finish;

  properties [PROP_PACKAGES] =
    g_param_spec_boxed ("packages",
                        "Packages",
                        "The package names to be installed",
                        G_TYPE_STRV,
                        (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_pkcon_transfer_init (IdePkconTransfer *self)
{
  ide_transfer_set_icon_name (IDE_TRANSFER (self), "system-software-install-symbolic");
}

IdePkconTransfer *
ide_pkcon_transfer_new (const gchar * const *packages)
{
  return g_object_new (IDE_TYPE_PKCON_TRANSFER,
                       "packages", packages,
                       NULL);
}
