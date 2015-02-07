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
#include "ide-context.h"
#include "ide-device.h"

#include "autotools/ide-autotools-builder.h"

typedef struct
{
  gchar *tarball_name;
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
  g_autoptr(gchar) name = NULL;
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
ide_autotools_build_system_finalize (GObject *object)
{
  IdeAutotoolsBuildSystemPrivate *priv;
  IdeAutotoolsBuildSystem *system = (IdeAutotoolsBuildSystem *)object;

  priv = ide_autotools_build_system_get_instance_private (system);

  g_clear_pointer (&priv->tarball_name, g_free);

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
ide_autotools_build_system_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  //IdeAutotoolsBuildSystem *self = IDE_AUTOTOOLS_BUILD_SYSTEM (object);

  switch (prop_id)
    {
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
  object_class->set_property = ide_autotools_build_system_set_property;

  build_system_class->get_builder =   ide_autotools_build_system_get_builder;

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
