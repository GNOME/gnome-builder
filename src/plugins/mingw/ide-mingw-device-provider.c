/* ide-mingw-device-provider.c
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-mingw-device-provider"

#include <glib/gi18n.h>

#include "ide-mingw-device.h"
#include "ide-mingw-device-provider.h"

struct _IdeMingwDeviceProvider
{
  IdeDeviceProvider parent_instance;
};

G_DEFINE_TYPE (IdeMingwDeviceProvider, ide_mingw_device_provider, IDE_TYPE_DEVICE_PROVIDER)

static void
ide_mingw_device_provider_load_worker (GTask        *task,
                                       gpointer      source_object,
                                       gpointer      task_data,
                                       GCancellable *cancellable)
{
  IdeMingwDeviceProvider *self = source_object;
  g_autoptr(GPtrArray) devices = NULL;
  g_autofree gchar *x32 = NULL;
  g_autofree gchar *x64 = NULL;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_MINGW_DEVICE_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  context = ide_object_get_context (IDE_OBJECT (self));
  devices = g_ptr_array_new_with_free_func (g_object_unref);

  if (NULL != (x64 = g_find_program_in_path ("x86_64-w64-mingw32-gcc")))
    g_ptr_array_add (devices,
                     ide_mingw_device_new (context,
                                           _("MinGW 64-bit"),
                                           "local-x86_64-w64-mingw32",
                                           "x86_64-w64-mingw32"));

  if (NULL != (x32 = g_find_program_in_path ("i686-w64-mingw32-gcc")))
    g_ptr_array_add (devices,
                     ide_mingw_device_new (context,
                                           _("MinGW 32-bit"),
                                           "local-i686-w64-mingw32",
                                           "i686-w64-mingw32"));

  g_task_return_pointer (task,
                         g_steal_pointer (&devices),
                         (GDestroyNotify)g_ptr_array_unref);

  IDE_EXIT;
}

static void
ide_mingw_device_provider_load_async (IdeDeviceProvider   *provider,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  IdeMingwDeviceProvider *self = (IdeMingwDeviceProvider *)provider;
  g_autoptr(GTask) task = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MINGW_DEVICE_PROVIDER (self));
  g_assert (IDE_IS_DEVICE_PROVIDER (provider));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_mingw_device_provider_load_async);
  ide_context_hold_for_object (ide_object_get_context (IDE_OBJECT (self)), task);
  g_task_run_in_thread (task, ide_mingw_device_provider_load_worker);

  IDE_EXIT;
}

static gboolean
ide_mingw_device_provider_load_finish (IdeDeviceProvider  *provider,
                                       GAsyncResult       *result,
                                       GError            **error)
{
  g_autoptr(GPtrArray) devices = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MINGW_DEVICE_PROVIDER (provider));
  g_assert (G_IS_TASK (result));

  if (NULL != (devices = g_task_propagate_pointer (G_TASK (result), error)))
    {
      for (guint i = 0; i < devices->len; i++)
        {
          IdeDevice *device = g_ptr_array_index (devices, i);

          g_assert (IDE_IS_DEVICE (device));

          ide_device_provider_emit_device_added (provider, device);
        }
    }

  IDE_RETURN (TRUE);
}

static void
ide_mingw_device_provider_class_init (IdeMingwDeviceProviderClass *klass)
{
  IdeDeviceProviderClass *provider_class = IDE_DEVICE_PROVIDER_CLASS (klass);

  provider_class->load_async = ide_mingw_device_provider_load_async;
  provider_class->load_finish = ide_mingw_device_provider_load_finish;
}

static void
ide_mingw_device_provider_init (IdeMingwDeviceProvider *self)
{
}
