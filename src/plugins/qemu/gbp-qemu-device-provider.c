/* gbp-qemu-device-provider.c
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-qemu-device-provider"

#include <glib/gi18n.h>
#include <string.h>

#include "gbp-qemu-device-provider.h"

struct _GbpQemuDeviceProvider
{
  IdeDeviceProvider parent_instance;
};

G_DEFINE_TYPE (GbpQemuDeviceProvider, gbp_qemu_device_provider, IDE_TYPE_DEVICE_PROVIDER)

static const struct {
  const gchar *filename;
  const gchar *arch;
  const gchar *suffix;
} machines[] = {
  /* translators: format is "CPU emulation". Only translate "emulation" */
  { "qemu-aarch64", "aarch64", N_("Aarch64 Emulation") },
  { "qemu-arm",     "arm",     N_("Arm Emulation") },
};

#ifdef __linux__
static void
gbp_qemu_device_provider_load_worker (GTask        *task,
                                      gpointer      source_object,
                                      gpointer      task_data,
                                      GCancellable *cancellable)
{
  g_autofree gchar *mounts = NULL;
  g_autofree gchar *status = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GPtrArray) devices = NULL;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (G_IS_TASK (task));
  g_assert (GBP_IS_QEMU_DEVICE_PROVIDER (source_object));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  devices = g_ptr_array_new_with_free_func (g_object_unref);

  /* The first thing we need to do is ensure that binfmt is available
   * in /proc/mounts so that the system knows about binfmt hooks.
   */

  if (!ide_g_host_file_get_contents ("/proc/mounts", &mounts, NULL, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  /* @mounts is guaranteed to have a \0 suffix */
  if (strstr (mounts, "binfmt") == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "binfmt is missing from /proc/mounts");
      IDE_EXIT;
    }

  if (!ide_g_host_file_get_contents  ("/proc/sys/fs/binfmt_misc/status", &status, NULL, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  /* @status is guaranteed to have a \0 suffix */
  if (!g_str_equal (g_strstrip (status), "enabled"))
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "binfmt hooks are not currently enabled");
      IDE_EXIT;
    }

  context = ide_object_get_context (source_object);

  /* Now locate which of the machines are registered. Qemu has a huge
   * list of these, so we only check for ones we think are likely to
   * be used. If you want support for more, let us know.
   */
  for (guint i = 0; i < G_N_ELEMENTS (machines); i++)
    {
      g_autofree gchar *path = NULL;
      g_autofree gchar *contents = NULL;
      gsize len;

      path = g_build_filename ("/proc/sys/fs/binfmt_misc", machines[i].filename, NULL);

      /* First line of file should be "enabled\n" */
      if (ide_g_host_file_get_contents (path, &contents, &len, NULL) &&
          strncmp (contents, "enabled\n", 8) == 0)
        {
          g_autoptr(IdeLocalDevice) device = NULL;
          g_autofree gchar *display_name = NULL;

          IDE_TRACE_MSG ("Discovered QEMU device \"%s\"\n", machines[i].arch);

          /* translators: first %s is replaced with hostname, second %s with the CPU architecture */
          display_name = g_strdup_printf (_("My Computer (%s) %s"),
                                          g_get_host_name (),
                                          machines[i].suffix);

          device = g_object_new (IDE_TYPE_LOCAL_DEVICE,
                                 "id", machines[i].filename,
                                 "arch", machines[i].arch,
                                 "context", context,
                                 "display-name", display_name,
                                 NULL);
          g_ptr_array_add (devices, g_steal_pointer (&device));
        }
    }

  g_task_return_pointer (task,
                         g_steal_pointer (&devices),
                         (GDestroyNotify)g_ptr_array_unref);

  IDE_EXIT;
}
#endif

static void
gbp_qemu_device_provider_load_async (IdeDeviceProvider   *provider,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_QEMU_DEVICE_PROVIDER (provider));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (provider, cancellable, callback, user_data);
  g_task_set_source_tag (task, gbp_qemu_device_provider_load_async);

#ifdef __linux__
  g_task_run_in_thread (task, gbp_qemu_device_provider_load_worker);
#else
  g_task_return_new_error (task,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Qemu device hooks are only supported on Linux");
#endif

  IDE_EXIT;
}

static gboolean
gbp_qemu_device_provider_load_finish (IdeDeviceProvider  *provider,
                                      GAsyncResult       *result,
                                      GError            **error)
{
  g_autoptr(GPtrArray) devices = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_DEVICE_PROVIDER (provider));
  g_assert (G_IS_ASYNC_RESULT (result));

  if ((devices = g_task_propagate_pointer (G_TASK (result), error)))
    {
      for (guint i = 0; i < devices->len; i++)
        {
          IdeDevice *device = g_ptr_array_index (devices, i);

          ide_device_provider_emit_device_added (provider, device);
        }
    }

  IDE_RETURN (!!devices);
}

static void
gbp_qemu_device_provider_finalize (GObject *object)
{
  G_OBJECT_CLASS (gbp_qemu_device_provider_parent_class)->finalize (object);
}

static void
gbp_qemu_device_provider_class_init (GbpQemuDeviceProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeDeviceProviderClass *provider_class = IDE_DEVICE_PROVIDER_CLASS (klass);

  object_class->finalize = gbp_qemu_device_provider_finalize;

  provider_class->load_async = gbp_qemu_device_provider_load_async;
  provider_class->load_finish = gbp_qemu_device_provider_load_finish;
}

static void
gbp_qemu_device_provider_init (GbpQemuDeviceProvider *self)
{
}
