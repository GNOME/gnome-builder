/* ide-mingw-device-provider.c
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

#include <glib/gi18n.h>

#include "ide-context.h"
#include "ide-mingw-device.h"
#include "ide-mingw-device-provider.h"

struct _IdeMingwDeviceProvider
{
  IdeDeviceProvider parent_instance;
};

G_DEFINE_TYPE (IdeMingwDeviceProvider, ide_mingw_device_provider, IDE_TYPE_DEVICE_PROVIDER)

static void
ide_mingw_device_provider_discover_worker (GTask        *task,
                                           gpointer      source_object,
                                           gpointer      task_data,
                                           GCancellable *cancellable)
{
  IdeMingwDeviceProvider *self = source_object;
  GPtrArray *devices;
  IdeContext *context;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_MINGW_DEVICE_PROVIDER (self));

  devices = g_ptr_array_new_with_free_func (g_object_unref);

  context = ide_object_get_context (IDE_OBJECT (self));

  g_assert (IDE_IS_CONTEXT (context));

  /*
   * FIXME:
   *
   * I'm almost certain this is not the proper way to check for mingw support.
   * Someone that knows how this works, please fix this up!
   */

  if (g_file_test ("/usr/bin/x86_64-w64-mingw32-gcc", G_FILE_TEST_EXISTS))
    {
      IdeDevice *device;

      /* add 64-bit mingw device */
      device = ide_mingw_device_new (context,
                                     _("MinGW (64-bit)"),
                                     "local-x86_64-w64-mingw32",
                                     "x86_64-w64-mingw32");
      g_ptr_array_add (devices, device);
    }

  if (g_file_test ("/usr/bin/i686-w64-mingw32-gcc", G_FILE_TEST_EXISTS))
    {
      IdeDevice *device;

      /* add 32-bit mingw device */
      device = ide_mingw_device_new (context,
                                     _("MinGW (32-bit)"),
                                     "local-i686-w64-mingw32",
                                     "i686-w64-mingw32");
      g_ptr_array_add (devices, device);
    }

  g_task_return_pointer (task, devices, (GDestroyNotify)g_ptr_array_unref);

  ide_object_release (IDE_OBJECT (self));
}

static void
load_cb (GObject      *object,
         GAsyncResult *result,
         gpointer      user_data)
{
  IdeMingwDeviceProvider *self = (IdeMingwDeviceProvider *)object;
  GTask *task = (GTask *)result;
  g_autoptr(GPtrArray) devices = NULL;
  gsize i;

  g_assert (IDE_IS_MINGW_DEVICE_PROVIDER (self));
  g_assert (G_IS_TASK (task));

  devices = g_task_propagate_pointer (task, NULL);

  if (devices)
    {
      for (i = 0; i < devices->len; i++)
        {
          IdeDevice *device;

          device = g_ptr_array_index (devices, i);
          ide_device_provider_device_added (IDE_DEVICE_PROVIDER (self), device);
        }
    }
}

static void
ide_mingw_device_provider_constructed (GObject *object)
{
  IdeMingwDeviceProvider *self = (IdeMingwDeviceProvider *)object;
  g_autoptr(GTask) task = NULL;

  g_assert (IDE_IS_MINGW_DEVICE_PROVIDER (self));

  ide_object_hold (IDE_OBJECT (self));
  task = g_task_new (self, NULL, load_cb, NULL);
  g_task_run_in_thread (task, ide_mingw_device_provider_discover_worker);
}

static void
ide_mingw_device_provider_class_init (IdeMingwDeviceProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_mingw_device_provider_constructed;
}

static void
ide_mingw_device_provider_init (IdeMingwDeviceProvider *self)
{
}
