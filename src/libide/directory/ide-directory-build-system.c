/* ide-directory-build-system.c
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

#define G_LOG_DOMAIN "directory-build-system"

#include <glib/gi18n.h>

#include "ide-context.h"

#include "directory/ide-directory-build-system.h"
#include "projects/ide-project-file.h"
#include "projects/ide-project-item.h"
#include "projects/ide-project.h"

struct _IdeDirectoryBuildSystem
{
  IdeObject  parent_instance;
  GFile     *project_file;
};

static void async_initiable_init (GAsyncInitableIface     *iface);
static void build_system_init    (IdeBuildSystemInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdeDirectoryBuildSystem,
                        ide_directory_build_system,
                        IDE_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, async_initiable_init)
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_SYSTEM, build_system_init))

enum {
  PROP_0,
  PROP_PROJECT_FILE,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

IdeDirectoryBuildSystem *
ide_directory_build_system_new (void)
{
  return g_object_new (IDE_TYPE_DIRECTORY_BUILD_SYSTEM, NULL);
}

static void
ide_directory_build_system_finalize (GObject *object)
{
  IdeDirectoryBuildSystem *self = (IdeDirectoryBuildSystem *)object;

  g_clear_object (&self->project_file);

  G_OBJECT_CLASS (ide_directory_build_system_parent_class)->finalize (object);
}

static void
ide_directory_build_system_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  IdeDirectoryBuildSystem *self = IDE_DIRECTORY_BUILD_SYSTEM (object);

  switch (prop_id)
    {
    case PROP_PROJECT_FILE:
      g_value_set_object (value, self->project_file);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_directory_build_system_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  IdeDirectoryBuildSystem *self = IDE_DIRECTORY_BUILD_SYSTEM (object);

  switch (prop_id)
    {
    case PROP_PROJECT_FILE:
      g_clear_object (&self->project_file);
      self->project_file = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_directory_build_system_class_init (IdeDirectoryBuildSystemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_directory_build_system_finalize;
  object_class->get_property = ide_directory_build_system_get_property;
  object_class->set_property = ide_directory_build_system_set_property;

  properties [PROP_PROJECT_FILE] =
    g_param_spec_object ("project-file",
                         "Project File",
                         "The path of the project file.",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
ide_directory_build_system_init (IdeDirectoryBuildSystem *self)
{
}

static void
ide_directory_build_system_init_async (GAsyncInitable      *initable,
                                       int                  io_priority,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  IdeDirectoryBuildSystem *system = (IdeDirectoryBuildSystem *)initable;
  GTask *task;

  g_return_if_fail (IDE_IS_DIRECTORY_BUILD_SYSTEM (system));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (system, cancellable, callback, user_data);
  g_task_return_boolean (task, TRUE);
  g_object_unref (task);
}

static gboolean
ide_directory_build_system_init_finish (GAsyncInitable  *initable,
                                        GAsyncResult    *result,
                                        GError         **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (G_IS_TASK (task), FALSE);

  return g_task_propagate_boolean (task, error);
}

static void
async_initiable_init (GAsyncInitableIface *iface)
{
  iface->init_async = ide_directory_build_system_init_async;
  iface->init_finish = ide_directory_build_system_init_finish;
}

static gint
ide_directory_build_system_get_priority (IdeBuildSystem *build_system)
{
  return 1000000;
}

static gchar *
ide_directory_build_system_get_id (IdeBuildSystem *build_system)
{
  return g_strdup ("directory");
}

static gchar *
ide_directory_build_system_get_display_name (IdeBuildSystem *build_system)
{
  return g_strdup ("Directory");
}

static void
build_system_init (IdeBuildSystemInterface *iface)
{
  iface->get_priority = ide_directory_build_system_get_priority;
  iface->get_id = ide_directory_build_system_get_id;
  iface->get_display_name = ide_directory_build_system_get_display_name;
}
