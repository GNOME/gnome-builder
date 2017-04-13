/* ide-build-system.c
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

#define G_LOG_DOMAIN "ide-build-system"

#include "ide-context.h"
#include "ide-debug.h"
#include "ide-object.h"

#include "buildsystem/ide-build-system.h"
#include "buildsystem/ide-configuration.h"
#include "files/ide-file.h"
#include "projects/ide-project.h"

G_DEFINE_INTERFACE (IdeBuildSystem, ide_build_system, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_CONTEXT,
  PROP_PROJECT_FILE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

gint
ide_build_system_get_priority (IdeBuildSystem *self)
{
  IdeBuildSystemInterface *iface;

  g_return_val_if_fail (IDE_IS_BUILD_SYSTEM (self), 0);

  iface = IDE_BUILD_SYSTEM_GET_IFACE (self);

  if (iface->get_priority != NULL)
    return iface->get_priority (self);

  return 0;
}

static void
ide_build_system_real_get_build_flags_async (IdeBuildSystem      *self,
                                             IdeFile             *file,
                                             GCancellable        *cancellable,
                                             GAsyncReadyCallback  callback,
                                             gpointer             user_data)
{
  g_task_report_new_error (self,
                           callback,
                           user_data,
                           ide_build_system_real_get_build_flags_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Fetching build flags is not supported");
}

static gchar **
ide_build_system_real_get_build_flags_finish (IdeBuildSystem  *self,
                                              GAsyncResult    *result,
                                              GError         **error)
{
  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
ide_build_system_real_get_build_targets_async (IdeBuildSystem      *self,
                                               GCancellable        *cancellable,
                                               GAsyncReadyCallback  callback,
                                               gpointer             user_data)
{
  g_task_report_new_error (self,
                           callback,
                           user_data,
                           ide_build_system_real_get_build_targets_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Fetching build targets is not supported");
}

static GPtrArray *
ide_build_system_real_get_build_targets_finish (IdeBuildSystem  *self,
                                                GAsyncResult    *result,
                                                GError         **error)
{
  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
ide_build_system_default_init (IdeBuildSystemInterface *iface)
{
  iface->get_build_flags_async = ide_build_system_real_get_build_flags_async;
  iface->get_build_flags_finish = ide_build_system_real_get_build_flags_finish;
  iface->get_build_targets_finish = ide_build_system_real_get_build_targets_finish;
  iface->get_build_targets_async = ide_build_system_real_get_build_targets_async;
  iface->get_build_targets_finish = ide_build_system_real_get_build_targets_finish;

  properties [PROP_PROJECT_FILE] =
    g_param_spec_object ("project-file",
                         "Project File",
                         "The project file.",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  g_object_interface_install_property (iface, properties [PROP_PROJECT_FILE]);

  properties [PROP_CONTEXT] =
    g_param_spec_object ("context",
                         "Context",
                         "Context",
                         IDE_TYPE_CONTEXT,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  g_object_interface_install_property (iface, properties [PROP_CONTEXT]);
}

static gint
sort_priority (gconstpointer a,
               gconstpointer b,
               gpointer      data)
{
  IdeBuildSystem **as = (IdeBuildSystem **)a;
  IdeBuildSystem **bs = (IdeBuildSystem **)b;
  g_autofree gchar *id_a = ide_build_system_get_id (*as);
  g_autofree gchar *id_b = ide_build_system_get_id (*bs);
  const gchar *build_system_hint = data;

  if (build_system_hint != NULL)
    {
      if (g_strcmp0 (build_system_hint, id_a) == 0)
        return -1;
      else if (g_strcmp0 (build_system_hint, id_b) == 0)
        return 1;
    }

  return ide_build_system_get_priority (*as) - ide_build_system_get_priority (*bs);
}

/**
 * ide_build_system_new_async:
 * @context: #IdeBuildSystem
 * @project_file: A #GFile containing the directory or project file.
 * @build_system_hint: A hint for the build system to use
 * @cancellable: (allow-none): A #GCancellable
 * @callback: A callback to execute upon completion
 * @user_data: User data for @callback.
 *
 * Asynchronously creates a new #IdeBuildSystem instance using the registered
 * #GIOExtensionPoint system. Each extension point will be tried asynchronously
 * by priority until one has been found that supports @project_file.
 *
 * If no build system could be found, then ide_build_system_new_finish() will
 * return %NULL.
 */
void
ide_build_system_new_async (IdeContext          *context,
                            GFile               *project_file,
                            const gchar         *build_system_hint,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  g_return_if_fail (IDE_IS_CONTEXT (context));
  g_return_if_fail (G_IS_FILE (project_file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_TRACE_MSG ("Creating build system with hint \"%s\"", build_system_hint ?: "");

  ide_object_new_for_extension_async (IDE_TYPE_BUILD_SYSTEM,
                                      sort_priority, (gpointer)build_system_hint,
                                      G_PRIORITY_DEFAULT,
                                      cancellable,
                                      callback,
                                      user_data,
                                      "context", context,
                                      "project-file", project_file,
                                      NULL);
}

/**
 * ide_build_system_new_finish:
 *
 * Complete an asynchronous call to ide_build_system_new_async().
 *
 * Returns: (transfer full): An #IdeBuildSystem if successful; otherwise
 *   %NULL and @error is set.
 */
IdeBuildSystem *
ide_build_system_new_finish (GAsyncResult  *result,
                             GError       **error)
{
  IdeObject *ret;

  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  ret = ide_object_new_finish (result, error);

  return ret ? IDE_BUILD_SYSTEM (ret) : NULL;
}

void
ide_build_system_get_build_flags_async (IdeBuildSystem      *self,
                                        IdeFile             *file,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_BUILD_SYSTEM (self));
  g_return_if_fail (IDE_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_BUILD_SYSTEM_GET_IFACE (self)->get_build_flags_async (self, file, cancellable, callback, user_data);

  IDE_EXIT;
}

/**
 * ide_build_system_get_build_flags_finish:
 *
 * Returns: (transfer full):
 */
gchar **
ide_build_system_get_build_flags_finish (IdeBuildSystem  *self,
                                         GAsyncResult    *result,
                                         GError         **error)
{
  gchar **ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_BUILD_SYSTEM (self), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  ret = IDE_BUILD_SYSTEM_GET_IFACE (self)->get_build_flags_finish (self, result, error);

  IDE_RETURN (ret);
}

void
ide_build_system_get_build_targets_async (IdeBuildSystem      *self,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_BUILD_SYSTEM (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_BUILD_SYSTEM_GET_IFACE (self)->get_build_targets_async (self, cancellable, callback, user_data);

  IDE_EXIT;
}

/**
 * ide_build_system_get_build_targets_finish:
 *
 * Returns: (transfer container) (element-type Ide.BuildTarget): An array
 *   of #IdeBuildTarget or %NULL and @error is set.
 */
GPtrArray *
ide_build_system_get_build_targets_finish (IdeBuildSystem  *self,
                                           GAsyncResult    *result,
                                           GError         **error)
{
  GPtrArray *ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_BUILD_SYSTEM (self), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  ret = IDE_BUILD_SYSTEM_GET_IFACE (self)->get_build_targets_finish (self, result, error);

  IDE_RETURN (ret);
}

gchar *
ide_build_system_get_builddir (IdeBuildSystem   *self,
                               IdeConfiguration *configuration)
{
  gchar *ret = NULL;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_BUILD_SYSTEM (self), NULL);
  g_return_val_if_fail (IDE_IS_CONFIGURATION (configuration), NULL);

  if (IDE_BUILD_SYSTEM_GET_IFACE (self)->get_builddir)
    ret = IDE_BUILD_SYSTEM_GET_IFACE (self)->get_builddir (self, configuration);

  if (ret == NULL)
    {
      g_autofree gchar *name = NULL;
      const gchar *project_id;
      const gchar *config_id;
      const gchar *device_id;
      const gchar *runtime_id;
      IdeContext *context;
      IdeProject *project;

      context = ide_object_get_context (IDE_OBJECT (self));
      project = ide_context_get_project (context);
      project_id = ide_project_get_id (project);
      config_id = ide_configuration_get_id (configuration);
      device_id = ide_configuration_get_device_id (configuration);
      runtime_id = ide_configuration_get_runtime_id (configuration);

      name = g_strdelimit (g_strdup_printf ("%s-%s-%s", config_id, device_id, runtime_id),
                           "@:/", '-');

      ret = g_build_filename (g_get_user_cache_dir (),
                              "gnome-builder",
                              "builds",
                              project_id,
                              name,
                              NULL);
    }

  IDE_RETURN (ret);
}

gchar *
ide_build_system_get_id (IdeBuildSystem *self)
{
  g_return_val_if_fail (IDE_IS_BUILD_SYSTEM (self), NULL);

  if (IDE_BUILD_SYSTEM_GET_IFACE (self)->get_id)
    return IDE_BUILD_SYSTEM_GET_IFACE (self)->get_id (self);

  return g_strdup (G_OBJECT_TYPE_NAME (self));
}

gchar *
ide_build_system_get_display_name (IdeBuildSystem *self)
{
  g_return_val_if_fail (IDE_IS_BUILD_SYSTEM (self), NULL);

  if (IDE_BUILD_SYSTEM_GET_IFACE (self)->get_display_name)
    return IDE_BUILD_SYSTEM_GET_IFACE (self)->get_display_name (self);

  return ide_build_system_get_id (self);
}
