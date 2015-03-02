/* ide-autotools-build-system.c
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
#include <gio/gio.h>

#include "ide-autotools-build-system.h"
#include "ide-autotools-builder.h"
#include "ide-context.h"
#include "ide-device.h"
#include "ide-device-manager.h"
#include "ide-file.h"
#include "ide-makecache.h"

typedef struct
{
  IdeMakecache *makecache;
  gchar        *tarball_name;
  GPtrArray    *makecache_tasks;

  guint         makecache_in_progress : 1;
} IdeAutotoolsBuildSystemPrivate;

static void async_initable_iface_init (GAsyncInitableIface *iface);

G_DEFINE_TYPE_EXTENDED (IdeAutotoolsBuildSystem,
                        ide_autotools_build_system,
                        IDE_TYPE_BUILD_SYSTEM,
                        0,
                        G_ADD_PRIVATE (IdeAutotoolsBuildSystem)
                        G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE,
                                               async_initable_iface_init))

enum {
  PROP_0,
  PROP_TARBALL_NAME,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

const gchar *
ide_autotools_build_system_get_tarball_name (IdeAutotoolsBuildSystem *system)
{
  IdeAutotoolsBuildSystemPrivate *priv = ide_autotools_build_system_get_instance_private (system);

  g_return_val_if_fail (IDE_IS_AUTOTOOLS_BUILD_SYSTEM (system), NULL);

  return priv->tarball_name;
}

static IdeBuilder *
ide_autotools_build_system_get_builder (IdeBuildSystem  *system,
                                        GKeyFile        *config,
                                        IdeDevice       *device,
                                        GError         **error)
{
  IdeBuilder *ret;
  IdeContext *context;

  g_return_val_if_fail (IDE_IS_AUTOTOOLS_BUILD_SYSTEM (system), NULL);
  g_return_val_if_fail (config, NULL);
  g_return_val_if_fail (IDE_IS_DEVICE (device), NULL);

  context = ide_object_get_context (IDE_OBJECT (system));

  ret = g_object_new (IDE_TYPE_AUTOTOOLS_BUILDER,
                      "context", context,
                      "config", config,
                      "device", device,
                      NULL);

  return ret;
}

static void
discover_query_info_cb (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  g_autoptr(GFileInfo) file_info = NULL;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GFile) child = NULL;
  GFile *file = (GFile *)object;
  GError *error = NULL;
  GFileType file_type;

  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (G_IS_TASK (task));

  file_info = g_file_query_info_finish (file, result, &error);

  if (!file_info)
    {
      g_task_return_error (task, error);
      return;
    }

  file_type = g_file_info_get_file_type (file_info);

  if (file_type == G_FILE_TYPE_REGULAR)
    {
      const gchar *name;

      name = g_file_info_get_name (file_info);

      if ((g_strcmp0 (name, "configure.ac") == 0) ||
          (g_strcmp0 (name, "configure.in") == 0))
        {
          g_task_return_pointer (task, g_object_ref (file), g_object_unref);
          return;
        }
    }
  else if (file_type != G_FILE_TYPE_DIRECTORY)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               _("Not an autotools project file."));
      return;
    }

  child = g_file_get_child (file, "configure.ac");
  g_file_query_info_async (child,
                           (G_FILE_ATTRIBUTE_STANDARD_TYPE","
                            G_FILE_ATTRIBUTE_STANDARD_NAME),
                           G_FILE_QUERY_INFO_NONE,
                           G_PRIORITY_DEFAULT,
                           g_task_get_cancellable (task),
                           discover_query_info_cb,
                           g_object_ref (task));
}

static void
ide_autotools_build_system_discover_file_async (IdeAutotoolsBuildSystem *system,
                                                GFile                   *file,
                                                GCancellable            *cancellable,
                                                GAsyncReadyCallback      callback,
                                                gpointer                 user_data)
{
  g_autofree gchar *name = NULL;
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_AUTOTOOLS_BUILD_SYSTEM (system));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (system, cancellable, callback, user_data);
  name = g_file_get_basename (file);

  if (!name)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_FILENAME,
                               _("Invalid file provided to discover."));
      return;
    }

  if (g_str_equal (name, "configure.ac") || g_str_equal (name, "configure.in"))
    {
      g_task_return_pointer (task, g_object_ref (file), g_object_unref);
      return;
    }

  g_file_query_info_async (file,
                           (G_FILE_ATTRIBUTE_STANDARD_TYPE","
                            G_FILE_ATTRIBUTE_STANDARD_NAME),
                           G_FILE_QUERY_INFO_NONE,
                           G_PRIORITY_DEFAULT,
                           cancellable,
                           discover_query_info_cb,
                           g_object_ref (task));
}

static GFile *
ide_autotools_build_system_discover_file_finish (IdeAutotoolsBuildSystem  *system,
                                                 GAsyncResult             *result,
                                                 GError                  **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_AUTOTOOLS_BUILD_SYSTEM (system), NULL);
  g_return_val_if_fail (G_IS_TASK (task), NULL);

  return g_task_propagate_pointer (task, error);
}

static void
ide_autotools_build_system__bootstrap_cb (GObject      *object,
                                          GAsyncResult *result,
                                          gpointer      user_data)
{
  IdeAutotoolsBuilder *builder = (IdeAutotoolsBuilder *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GFile) build_directory = NULL;
  g_autoptr(GFile) makefile = NULL;
  GError *error = NULL;

  g_assert (IDE_IS_AUTOTOOLS_BUILDER (builder));
  g_assert (G_IS_TASK (task));

  if (!ide_autotools_builder_bootstrap_finish (builder, result, &error))
    {
      g_task_return_error (task, error);
      return;
    }

  build_directory = ide_autotools_builder_get_build_directory (builder);
  makefile = g_file_get_child (build_directory, "Makefile");

  g_task_return_pointer (task, g_object_ref (makefile), g_object_unref);
}

static void
ide_autotools_build_system_get_local_makefile_async (IdeAutotoolsBuildSystem *self,
                                                     GCancellable            *cancellable,
                                                     GAsyncReadyCallback      callback,
                                                     gpointer                 user_data)
{
  IdeContext *context;
  IdeDeviceManager *device_manager;
  IdeDevice *device;
  g_autoptr(GTask) task = NULL;
  g_autoptr(IdeBuilder) builder = NULL;
  g_autoptr(GKeyFile) config = NULL;
  g_autoptr(GFile) build_directory = NULL;
  g_autoptr(GFile) makefile = NULL;
  GError *error = NULL;

  g_return_if_fail (IDE_IS_AUTOTOOLS_BUILD_SYSTEM (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  context = ide_object_get_context (IDE_OBJECT (self));
  device_manager = ide_context_get_device_manager (context);
  device = ide_device_manager_get_device (device_manager, "local");
  config = g_key_file_new ();
  builder = ide_autotools_build_system_get_builder (IDE_BUILD_SYSTEM (self), config, device, &error);

  if (!builder)
    {
      g_task_return_error (task, error);
      return;
    }

  /*
   * If we haven't yet bootstrapped the project, let's go ahead and do that now.
   */
  if (ide_autotools_builder_get_needs_bootstrap (IDE_AUTOTOOLS_BUILDER (builder)))
    {
      ide_autotools_builder_bootstrap_async (IDE_AUTOTOOLS_BUILDER (builder),
                                             cancellable,
                                             ide_autotools_build_system__bootstrap_cb,
                                             g_object_ref (task));
      return;
    }

  build_directory = ide_autotools_builder_get_build_directory (IDE_AUTOTOOLS_BUILDER (builder));
  makefile = g_file_get_child (build_directory, "Makefile");

  g_task_return_pointer (task, g_object_ref (makefile), g_object_unref);
}

static GFile *
ide_autotools_build_system_get_local_makefile_finish (IdeAutotoolsBuildSystem  *self,
                                                      GAsyncResult             *result,
                                                      GError                  **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_AUTOTOOLS_BUILD_SYSTEM (self), NULL);

  return g_task_propagate_pointer (task, error);
}

static void
ide_autotools_build_system__makecache_new_cb (GObject      *object,
                                              GAsyncResult *result,
                                              gpointer      user_data)
{
  IdeAutotoolsBuildSystemPrivate *priv;
  IdeAutotoolsBuildSystem *self;
  g_autoptr(IdeMakecache) makecache = NULL;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GPtrArray) tasks = NULL;
  GError *error = NULL;

  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  priv = ide_autotools_build_system_get_instance_private (self);

  makecache = ide_makecache_new_for_makefile_finish (result, &error);

  if (!makecache)
    {
      g_task_return_error (task, error);
      return;
    }

  priv->makecache = g_object_ref (makecache);

  /*
   * Complete all of the pending tasks in flight.
   */

  tasks = priv->makecache_tasks;
  priv->makecache_tasks = g_ptr_array_new ();
  priv->makecache_in_progress = FALSE;

  g_task_return_pointer (task, g_object_ref (makecache), g_object_unref);

  while (tasks->len)
    {
      GTask *other_task;
      gsize i = tasks->len - 1;

      other_task = g_ptr_array_index (tasks, i);
      g_task_return_pointer (other_task, g_object_ref (makecache), g_object_unref);
      g_ptr_array_remove_index (tasks, i);
      g_object_unref (other_task);
    }
}

static void
ide_autotools_build_system__local_makefile_cb (GObject      *object,
                                               GAsyncResult *result,
                                               gpointer      user_data)
{
  IdeAutotoolsBuildSystem *self = (IdeAutotoolsBuildSystem *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GFile) makefile = NULL;
  IdeContext *context;
  GError *error = NULL;

  g_assert (IDE_IS_AUTOTOOLS_BUILD_SYSTEM (self));
  g_assert (G_IS_TASK (task));

  makefile = ide_autotools_build_system_get_local_makefile_finish (self, result, &error);

  if (!makefile)
    {
      g_task_return_error (task, error);
      return;
    }

  context = ide_object_get_context (IDE_OBJECT (self));
  ide_makecache_new_for_makefile_async (context,
                                        makefile,
                                        g_task_get_cancellable (task),
                                        ide_autotools_build_system__makecache_new_cb,
                                        g_object_ref (task));
}

static void
ide_autotools_build_system_get_makecache_async (IdeAutotoolsBuildSystem *self,
                                                GCancellable            *cancellable,
                                                GAsyncReadyCallback      callback,
                                                gpointer                 user_data)
{
  IdeAutotoolsBuildSystemPrivate *priv = ide_autotools_build_system_get_instance_private (self);
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_AUTOTOOLS_BUILD_SYSTEM (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  /*
   * If we already have the makecache loaded, we can just return that.
   */
  if (priv->makecache)
    {
      g_task_return_pointer (task, g_object_ref (priv->makecache), g_object_unref);
      return;
    }

  /*
   * If we have a makecache operation in progress, we need to queue the task to be completed
   * when that operation completes.
   */
  if (priv->makecache_in_progress)
    {
      g_ptr_array_add (priv->makecache_tasks, g_object_ref (task));
      return;
    }

  /*
   * Nothing else is creating the makecache, so let's go ahead and create it now.
   */
  priv->makecache_in_progress = TRUE;
  ide_autotools_build_system_get_local_makefile_async (self,
                                                       cancellable,
                                                       ide_autotools_build_system__local_makefile_cb,
                                                       g_object_ref (task));
}

static IdeMakecache *
ide_autotools_build_system_get_makecache_finish (IdeAutotoolsBuildSystem  *self,
                                                 GAsyncResult             *result,
                                                 GError                  **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_AUTOTOOLS_BUILD_SYSTEM (self), NULL);
  g_return_val_if_fail (G_IS_TASK (task), NULL);

  return g_task_propagate_pointer (task, error);
}

static void
ide_autotools_build_system__get_file_flags_cb (GObject      *object,
                                               GAsyncResult *result,
                                               gpointer      user_data)
{
  IdeMakecache *makecache = (IdeMakecache *)object;
  g_autoptr(GTask) task = user_data;
  gchar **flags;
  GError *error = NULL;

  g_assert (IDE_IS_MAKECACHE (makecache));
  g_assert (G_IS_TASK (task));

  flags = ide_makecache_get_file_flags_finish (makecache, result, &error);

  if (!flags)
    {
      g_task_return_error (task, error);
      return;
    }

  g_task_return_pointer (task, flags, (GDestroyNotify)g_strfreev);
}

static void
ide_autotools_build_system__makecache_cb (GObject      *object,
                                          GAsyncResult *result,
                                          gpointer      user_data)
{
  IdeAutotoolsBuildSystem *self = (IdeAutotoolsBuildSystem *)object;
  g_autoptr(IdeMakecache) makecache = NULL;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;
  GFile *file;

  g_assert (IDE_IS_AUTOTOOLS_BUILD_SYSTEM (self));
  g_assert (G_IS_TASK (task));

  makecache = ide_autotools_build_system_get_makecache_finish (self, result, &error);

  if (!makecache)
    {
      g_task_return_error (task, error);
      return;
    }

  file = g_task_get_task_data (task);
  g_assert (G_IS_FILE (file));

  ide_makecache_get_file_flags_async (makecache,
                                      file,
                                      g_task_get_cancellable (task),
                                      ide_autotools_build_system__get_file_flags_cb,
                                      g_object_ref (task));
}

static void
ide_autotools_build_system_get_build_flags_async (IdeBuildSystem      *build_system,
                                                  IdeFile             *file,
                                                  GCancellable        *cancellable,
                                                  GAsyncReadyCallback  callback,
                                                  gpointer             user_data)
{
  IdeAutotoolsBuildSystem *self = (IdeAutotoolsBuildSystem *)build_system;
  g_autoptr(GTask) task = NULL;
  GFile *gfile;

  g_assert (IDE_IS_AUTOTOOLS_BUILD_SYSTEM (self));
  g_assert (IDE_IS_FILE (file));

  gfile = ide_file_get_file (file);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (task, g_object_ref (gfile), g_object_unref);

  ide_autotools_build_system_get_makecache_async (self,
                                                  cancellable,
                                                  ide_autotools_build_system__makecache_cb,
                                                  g_object_ref (task));
}

static gchar **
ide_autotools_build_system_get_build_flags_finish (IdeBuildSystem  *build_system,
                                                   GAsyncResult    *result,
                                                   GError         **error)
{
  GTask *task = (GTask *)result;

  g_assert (IDE_IS_AUTOTOOLS_BUILD_SYSTEM (build_system));
  g_assert (G_IS_TASK (task));

  return g_task_propagate_pointer (task, error);
}

static void
ide_autotools_build_system_finalize (GObject *object)
{
  IdeAutotoolsBuildSystemPrivate *priv;
  IdeAutotoolsBuildSystem *system = (IdeAutotoolsBuildSystem *)object;

  priv = ide_autotools_build_system_get_instance_private (system);

  g_clear_pointer (&priv->tarball_name, g_free);
  g_clear_pointer (&priv->makecache_tasks, g_ptr_array_unref);

  G_OBJECT_CLASS (ide_autotools_build_system_parent_class)->finalize (object);
}

static void
ide_autotools_build_system_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  IdeAutotoolsBuildSystem *self = IDE_AUTOTOOLS_BUILD_SYSTEM (object);

  switch (prop_id)
    {
    case PROP_TARBALL_NAME:
      g_value_set_string (value,
                          ide_autotools_build_system_get_tarball_name (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_autotools_build_system_class_init (IdeAutotoolsBuildSystemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeBuildSystemClass *build_system_class = IDE_BUILD_SYSTEM_CLASS (klass);

  object_class->finalize = ide_autotools_build_system_finalize;
  object_class->get_property = ide_autotools_build_system_get_property;

  build_system_class->get_builder = ide_autotools_build_system_get_builder;
  build_system_class->get_build_flags_async = ide_autotools_build_system_get_build_flags_async;
  build_system_class->get_build_flags_finish = ide_autotools_build_system_get_build_flags_finish;

  gParamSpecs [PROP_TARBALL_NAME] =
    g_param_spec_string ("tarball-name",
                         _("Tarball Name"),
                         _("The name of the project tarball."),
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_TARBALL_NAME,
                                   gParamSpecs [PROP_TARBALL_NAME]);
}

static void
ide_autotools_build_system_init (IdeAutotoolsBuildSystem *self)
{
  IdeAutotoolsBuildSystemPrivate *priv = ide_autotools_build_system_get_instance_private (self);

  priv->makecache_tasks = g_ptr_array_new ();
}

static void
ide_autotools_build_system_parse_async (IdeAutotoolsBuildSystem *system,
                                        GFile                   *project_file,
                                        GCancellable            *cancellable,
                                        GAsyncReadyCallback      callback,
                                        gpointer                 user_data)
{
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_AUTOTOOLS_BUILD_SYSTEM (system));
  g_return_if_fail (G_IS_FILE (project_file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (system, cancellable, callback, user_data);
  g_task_return_boolean (task, TRUE);
}

static gboolean
ide_autotools_build_system_parse_finish (IdeAutotoolsBuildSystem  *system,
                                         GAsyncResult             *result,
                                         GError                  **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_AUTOTOOLS_BUILD_SYSTEM (system), FALSE);
  g_return_val_if_fail (G_IS_TASK (task), FALSE);

  return g_task_propagate_boolean (task, error);
}

static void
parse_cb (GObject      *object,
          GAsyncResult *result,
          gpointer      user_data)
{
  IdeAutotoolsBuildSystem *self = (IdeAutotoolsBuildSystem *)object;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;

  g_return_if_fail (IDE_IS_AUTOTOOLS_BUILD_SYSTEM (self));
  g_return_if_fail (G_IS_TASK (task));

  if (!ide_autotools_build_system_parse_finish (self, result, &error))
    {
      g_task_return_error (task, error);
      return;
    }

  g_task_return_boolean (task, TRUE);
}

static void
discover_file_cb (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  IdeAutotoolsBuildSystem *self;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GFile) file = NULL;
  GError *error = NULL;

  g_return_if_fail (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  file = ide_autotools_build_system_discover_file_finish (self, result, &error);

  if (!file)
    {
      g_task_return_error (task, error);
      return;
    }

  ide_autotools_build_system_parse_async (self,
                                          file,
                                          g_task_get_cancellable (task),
                                          parse_cb,
                                          g_object_ref (task));
}

static void
ide_autotools_build_system_init_async (GAsyncInitable      *initable,
                                       gint                 io_priority,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  IdeAutotoolsBuildSystem *system = (IdeAutotoolsBuildSystem *)initable;
  g_autoptr(GTask) task = NULL;
  IdeContext *context;
  GFile *project_file;

  g_return_if_fail (IDE_IS_AUTOTOOLS_BUILD_SYSTEM (system));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (initable, cancellable, callback, user_data);
  context = ide_object_get_context (IDE_OBJECT (system));
  project_file = ide_context_get_project_file (context);

  ide_autotools_build_system_discover_file_async (system,
                                                  project_file,
                                                  cancellable,
                                                  discover_file_cb,
                                                  g_object_ref (task));
}

static gboolean
ide_autotools_build_system_init_finish (GAsyncInitable  *initable,
                                        GAsyncResult    *result,
                                        GError         **error)
{
  IdeAutotoolsBuildSystem *system = (IdeAutotoolsBuildSystem *)initable;
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_AUTOTOOLS_BUILD_SYSTEM (system), FALSE);
  g_return_val_if_fail (G_IS_TASK (task), FALSE);

  return g_task_propagate_boolean (task, error);
}

static void
async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = ide_autotools_build_system_init_async;
  iface->init_finish = ide_autotools_build_system_init_finish;
}
