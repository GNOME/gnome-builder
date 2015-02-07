/* ide-context.c
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

#define G_LOG_DOMAIN "ide-context"

#include <glib/gi18n.h>

#include "ide-async-helper.h"
#include "ide-build-system.h"
#include "ide-context.h"
#include "ide-device-manager.h"
#include "ide-project.h"
#include "ide-service.h"
#include "ide-unsaved-files.h"
#include "ide-vcs.h"

typedef struct
{
  IdeBuildSystem   *build_system;
  IdeDeviceManager *device_manager;
  IdeProject       *project;
  GFile            *project_file;
  gchar            *root_build_dir;
  GHashTable       *services;
  IdeUnsavedFiles  *unsaved_files;
  IdeVcs           *vcs;
} IdeContextPrivate;

static void async_initable_init (GAsyncInitableIface *);

G_DEFINE_TYPE_EXTENDED (IdeContext, ide_context, G_TYPE_OBJECT, 0,
                        G_ADD_PRIVATE (IdeContext)
                        G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE,
                                               async_initable_init))

enum {
  PROP_0,
  PROP_BUILD_SYSTEM,
  PROP_DEVICE_MANAGER,
  PROP_PROJECT_FILE,
  PROP_ROOT_BUILD_DIR,
  PROP_VCS,
  PROP_UNSAVED_FILES,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

/**
 * ide_context_get_build_system:
 *
 * Fetches the "build-system" property of @context.
 *
 * Returns: (transfer none): An #IdeBuildSystem.
 */
IdeBuildSystem *
ide_context_get_build_system (IdeContext *context)
{
  IdeContextPrivate *priv = ide_context_get_instance_private (context);

  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  return priv->build_system;
}

/**
 * ide_context_get_device_manager:
 *
 * Retrieves the "device-manager" property. The device manager is responsible
 * for connecting and disconnecting to physical or virtual devices within
 * LibIDE.
 *
 * Returns: (transfer none): An #IdeDeviceManager.
 */
IdeDeviceManager *
ide_context_get_device_manager (IdeContext *context)
{
  IdeContextPrivate *priv = ide_context_get_instance_private (context);

  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  return priv->device_manager;
}

/**
 * ide_context_get_root_build_dir:
 *
 * Retrieves the "root-build-dir" for the context. This is the root directory
 * that will contain builds made for various devices.
 *
 * Returns: A string containing the "root-build-dir" property.
 */
const gchar *
ide_context_get_root_build_dir (IdeContext *context)
{
  IdeContextPrivate *priv = ide_context_get_instance_private (context);

  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  return priv->root_build_dir;
}

/**
 * ide_context_set_root_build_dir:
 * @root_build_dir: the path to the root build directory.
 *
 * Sets the "root-build-dir" property. This is the root directory that will
 * be used when building projects for projects that support building out of
 * tree.
 */
void
ide_context_set_root_build_dir (IdeContext  *context,
                                const gchar *root_build_dir)
{
  IdeContextPrivate *priv = ide_context_get_instance_private (context);

  g_return_if_fail (IDE_IS_CONTEXT (context));
  g_return_if_fail (root_build_dir);

  if (priv->root_build_dir != root_build_dir)
    {
      g_free (priv->root_build_dir);
      priv->root_build_dir = g_strdup (root_build_dir);
      g_object_notify_by_pspec (G_OBJECT (context),
                                gParamSpecs [PROP_ROOT_BUILD_DIR]);
    }
}

/**
 * ide_context_get_unsaved_files:
 *
 * Returns the unsaved files for the #IdeContext. These are the contents of
 * open buffers in the IDE.
 *
 * Returns: (transfer none): An #IdeUnsavedFiles.
 */
IdeUnsavedFiles *
ide_context_get_unsaved_files (IdeContext *context)
{
  IdeContextPrivate *priv = ide_context_get_instance_private (context);

  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  return priv->unsaved_files;
}

IdeVcs *
ide_context_get_vcs (IdeContext *context)
{
  IdeContextPrivate *priv = ide_context_get_instance_private (context);

  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  return priv->vcs;
}

static void
ide_context_new_cb (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  GAsyncInitable *initable = (GAsyncInitable *)object;
  GError *error = NULL;
  GTask *task = user_data;

  g_return_if_fail (G_IS_ASYNC_INITABLE (initable));
  g_return_if_fail (G_IS_TASK (task));

  object = g_async_initable_new_finish (initable, result, &error);

  if (!object)
    g_task_return_error (task, error);
  else
    g_task_return_pointer (task, object, g_object_unref);

  g_object_unref (task);
}

void
ide_context_new_async (GFile               *project_file,
                       GCancellable        *cancellable,
                       GAsyncReadyCallback  callback,
                       gpointer             user_data)
{
  GTask *task;

  g_return_if_fail (G_IS_FILE (project_file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (NULL, cancellable, callback, user_data);
  g_async_initable_new_async (IDE_TYPE_CONTEXT,
                              G_PRIORITY_DEFAULT,
                              cancellable,
                              ide_context_new_cb,
                              g_object_ref (task),
                              "project-file", project_file,
                              NULL);
  g_object_unref (task);
}

IdeContext *
ide_context_new_finish (GAsyncResult  *result,
                        GError       **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (G_IS_TASK (task), NULL);

  return g_task_propagate_pointer (task, error);
}

IdeProject *
ide_context_get_project (IdeContext *context)
{
  IdeContextPrivate *priv = ide_context_get_instance_private (context);

  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  return priv->project;
}

GFile *
ide_context_get_project_file (IdeContext *context)
{
  IdeContextPrivate *priv = ide_context_get_instance_private (context);

  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  return priv->project_file;
}

static void
ide_context_set_project_file (IdeContext *context,
                              GFile      *project_file)
{
  IdeContextPrivate *priv = ide_context_get_instance_private (context);

  g_return_if_fail (IDE_IS_CONTEXT (context));

  if (project_file != priv->project_file)
    {
      g_clear_object (&priv->project_file);
      if (project_file)
        priv->project_file = g_object_ref (project_file);
      g_object_notify_by_pspec (G_OBJECT (context),
                                gParamSpecs [PROP_PROJECT_FILE]);
    }
}

static gpointer
ide_context_create_service (IdeContext *context,
                            GType       service_type)
{
  IdeContextPrivate *priv = ide_context_get_instance_private (context);
  IdeService *service;

  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (g_type_is_a (service_type, IDE_TYPE_SERVICE), NULL);

  service = g_object_new (service_type,
                          "context", context,
                          NULL);

  ide_service_start (service);

  g_hash_table_insert (priv->services,
                       GINT_TO_POINTER (service_type),
                       service);

  return service;
}

/**
 * ide_context_get_service_typed:
 * @service_type: A #GType of the service desired.
 *
 * Retrieves a service matching @service_type. If no match was found, a type
 * implementing the requested service type will be returned. If no matching
 * service type could be found, then an instance of the service will be
 * created, started, and returned.
 *
 * Returns: (transfer none) (nullable): An #IdeService or %NULL.
 */
gpointer
ide_context_get_service_typed (IdeContext *context,
                               GType       service_type)
{
  IdeContextPrivate *priv = ide_context_get_instance_private (context);
  IdeService *service;
  GHashTableIter iter;
  gpointer key;
  gpointer value;

  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (g_type_is_a (service_type, IDE_TYPE_SERVICE), NULL);

  service = g_hash_table_lookup (priv->services,
                                 GINT_TO_POINTER (service_type));

  if (service)
    return service;

  g_hash_table_iter_init (&iter, priv->services);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      service = value;

      if (g_type_is_a (G_TYPE_FROM_INSTANCE (service), service_type))
        return service;
    }

  if (!service)
    service = ide_context_create_service (context, service_type);

  return service;
}

static void
ide_context_dispose (GObject *object)
{
  IdeContext *self = (IdeContext *)object;
  IdeContextPrivate *priv = ide_context_get_instance_private (self);
  GHashTableIter iter;
  gpointer key;
  gpointer value;

  g_return_if_fail (IDE_IS_CONTEXT (self));

  g_hash_table_iter_init (&iter, priv->services);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      IdeService *service = value;

      g_assert (IDE_IS_SERVICE (service));

      if (ide_service_get_running (service))
        ide_service_stop (service);
    }

  G_OBJECT_CLASS (ide_context_parent_class)->dispose (object);
}

static void
ide_context_finalize (GObject *object)
{
  IdeContext *self = (IdeContext *)object;
  IdeContextPrivate *priv = ide_context_get_instance_private (self);

  g_clear_pointer (&priv->services, g_hash_table_unref);
  g_clear_pointer (&priv->root_build_dir, g_free);

  g_clear_object (&priv->build_system);
  g_clear_object (&priv->device_manager);
  g_clear_object (&priv->project);
  g_clear_object (&priv->project_file);
  g_clear_object (&priv->unsaved_files);
  g_clear_object (&priv->vcs);

  G_OBJECT_CLASS (ide_context_parent_class)->finalize (object);
}

static void
ide_context_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  IdeContext *self = IDE_CONTEXT (object);

  switch (prop_id)
    {
    case PROP_BUILD_SYSTEM:
      g_value_set_object (value, ide_context_get_build_system (self));
      break;

    case PROP_DEVICE_MANAGER:
      g_value_set_object (value, ide_context_get_device_manager (self));
      break;

    case PROP_PROJECT_FILE:
      g_value_set_object (value, ide_context_get_project_file (self));
      break;

    case PROP_ROOT_BUILD_DIR:
      g_value_set_string (value, ide_context_get_root_build_dir (self));
      break;

    case PROP_UNSAVED_FILES:
      g_value_set_object (value, ide_context_get_unsaved_files (self));
      break;

    case PROP_VCS:
      g_value_set_object (value, ide_context_get_vcs (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_context_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  IdeContext *self = IDE_CONTEXT (object);

  switch (prop_id)
    {
    case PROP_PROJECT_FILE:
      ide_context_set_project_file (self, g_value_get_object (value));
      break;

    case PROP_ROOT_BUILD_DIR:
      ide_context_set_root_build_dir (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_context_class_init (IdeContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_context_dispose;
  object_class->finalize = ide_context_finalize;
  object_class->get_property = ide_context_get_property;
  object_class->set_property = ide_context_set_property;

  gParamSpecs [PROP_BUILD_SYSTEM] =
    g_param_spec_object ("build-system",
                         _("Build System"),
                         _("The build system used by the context."),
                         IDE_TYPE_BUILD_SYSTEM,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_BUILD_SYSTEM,
                                   gParamSpecs [PROP_BUILD_SYSTEM]);

  gParamSpecs [PROP_DEVICE_MANAGER] =
    g_param_spec_object ("device-manager",
                         _("Device Manager"),
                         _("The device manager for the context."),
                         IDE_TYPE_DEVICE_MANAGER,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_DEVICE_MANAGER,
                                   gParamSpecs [PROP_DEVICE_MANAGER]);

  gParamSpecs [PROP_PROJECT_FILE] =
    g_param_spec_object ("project-file",
                         _("Project File"),
                         _("The project file for the context."),
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_PROJECT_FILE,
                                   gParamSpecs [PROP_PROJECT_FILE]);

  gParamSpecs [PROP_ROOT_BUILD_DIR] =
    g_param_spec_string ("root-build-dir",
                         _("Root Build Dir"),
                         _("The root directory to perform builds within."),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_ROOT_BUILD_DIR,
                                   gParamSpecs [PROP_ROOT_BUILD_DIR]);

  gParamSpecs [PROP_UNSAVED_FILES] =
    g_param_spec_object ("unsaved-files",
                         _("Unsaved Files"),
                         _("The unsaved files in the context."),
                         IDE_TYPE_UNSAVED_FILES,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_UNSAVED_FILES,
                                   gParamSpecs [PROP_UNSAVED_FILES]);

  gParamSpecs [PROP_VCS] =
    g_param_spec_object ("vcs",
                         _("Vcs"),
                         _("The vcs for the context."),
                         IDE_TYPE_VCS,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_VCS,
                                   gParamSpecs [PROP_VCS]);
}

static void
ide_context_init (IdeContext *self)
{
  IdeContextPrivate *priv = ide_context_get_instance_private (self);

  priv->root_build_dir = g_build_filename (g_get_user_cache_dir (),
                                           ide_get_program_name (),
                                           "builds",
                                           NULL);

  priv->device_manager = g_object_new (IDE_TYPE_DEVICE_MANAGER,
                                       "context", self,
                                       NULL);

  priv->project = g_object_new (IDE_TYPE_PROJECT,
                                "context", self,
                                NULL);

  priv->services = g_hash_table_new_full (g_direct_hash,
                                          g_direct_equal,
                                          NULL,
                                          g_object_unref);

  priv->unsaved_files = g_object_new (IDE_TYPE_UNSAVED_FILES,
                                      "context", self,
                                      NULL);
}

static void
ide_context_init_project_name_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  IdeContextPrivate *priv;
  IdeContext *context;
  g_autoptr(gchar) name = NULL;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GFileInfo) file_info = NULL;
  GFile *file = (GFile *)object;

  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (G_IS_TASK (task));

  context = g_task_get_source_object (task);
  priv = ide_context_get_instance_private (context);

  file_info = g_file_query_info_finish (file, result, NULL);

  if (file_info &&
      (G_FILE_TYPE_DIRECTORY == g_file_info_get_file_type (file_info)))
    {
      g_autoptr(gchar) name;

      name = g_file_get_basename (file);
      ide_project_set_name (priv->project, name);
    }
  else
    {
      g_autoptr(GFile) parent;
      g_autoptr(gchar) name;

      parent = g_file_get_parent (file);
      name = g_file_get_basename (parent);

      ide_project_set_name (priv->project, name);
    }

  g_task_return_boolean (task, TRUE);
}

static void
ide_context_init_project_name (gpointer             source_object,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  IdeContext *context = source_object;
  IdeContextPrivate *priv = ide_context_get_instance_private (context);
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_CONTEXT (context));

  task = g_task_new (source_object, cancellable, callback, user_data);

  if (!ide_project_get_name (priv->project))
    g_file_query_info_async (priv->project_file,
                             G_FILE_ATTRIBUTE_STANDARD_TYPE,
                             G_FILE_QUERY_INFO_NONE,
                             G_PRIORITY_DEFAULT,
                             g_task_get_cancellable (task),
                             ide_context_init_project_name_cb,
                             g_object_ref (task));
  else
    g_task_return_boolean (task, TRUE);
}

static void
ide_context_init_vcs_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  IdeContextPrivate *priv;
  IdeContext *self;
  g_autoptr(GTask) task = user_data;
  g_autoptr(IdeVcs) vcs = NULL;
  GError *error = NULL;

  g_return_if_fail (G_IS_ASYNC_RESULT (result));
  g_return_if_fail (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  priv = ide_context_get_instance_private (self);

  if (!(vcs = ide_vcs_new_finish (result, &error)))
    {
      g_task_return_error (task, error);
      return;
    }

  priv->vcs = g_object_ref (vcs);

  g_task_return_boolean (task, TRUE);
}

static void
ide_context_init_vcs (gpointer             source_object,
                      GCancellable        *cancellable,
                      GAsyncReadyCallback  callback,
                      gpointer             user_data)
{
  IdeContext *context = source_object;
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_CONTEXT (context));

  task = g_task_new (source_object, cancellable, callback, user_data);

  ide_vcs_new_async (context,
                     G_PRIORITY_DEFAULT,
                     cancellable,
                     ide_context_init_vcs_cb,
                     g_object_ref (task));
}

static void
ide_context_init_build_system_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  g_autoptr(IdeBuildSystem) build_system = NULL;
  g_autoptr(GTask) task = user_data;
  IdeContextPrivate *priv;
  IdeContext *self;
  GError *error = NULL;

  self = g_task_get_source_object (task);
  priv = ide_context_get_instance_private (self);

  if (!(build_system = ide_build_system_new_finish (result, &error)))
    {
      g_task_return_error (task, error);
      return;
    }

  priv->build_system = g_object_ref (build_system);

  g_task_return_boolean (task, TRUE);
}

static void
ide_context_init_build_system (gpointer             source_object,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  IdeContext *self = source_object;
  IdeContextPrivate *priv = ide_context_get_instance_private (self);
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_CONTEXT (self));

  task = g_task_new (self, cancellable, callback, user_data);
  ide_build_system_new_async (self,
                              priv->project_file,
                              cancellable,
                              ide_context_init_build_system_cb,
                              g_object_ref (task));
}

static void
ide_context_init_unsaved_files_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  IdeUnsavedFiles *unsaved_files = (IdeUnsavedFiles *)object;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;

  g_return_if_fail (IDE_IS_UNSAVED_FILES (unsaved_files));

  if (!ide_unsaved_files_restore_finish (unsaved_files, result, &error))
    {
      g_task_return_error (task, error);
      return;
    }

  g_task_return_boolean (task, TRUE);
}

static void
ide_context_init_unsaved_files (gpointer             source_object,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  IdeContext *self = source_object;
  IdeContextPrivate *priv = ide_context_get_instance_private (self);
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_CONTEXT (self));

  task = g_task_new (source_object, cancellable, callback, user_data);
  ide_unsaved_files_restore_async (priv->unsaved_files,
                                   cancellable,
                                   ide_context_init_unsaved_files_cb,
                                   g_object_ref (task));
}

static void
ide_context_init_async (GAsyncInitable      *initable,
                        int                  io_priority,
                        GCancellable        *cancellable,
                        GAsyncReadyCallback  callback,
                        gpointer             user_data)
{
  IdeContext *context = (IdeContext *)initable;
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (G_IS_ASYNC_INITABLE (context));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  ide_async_helper_run (context,
                        cancellable,
                        callback,
                        user_data,
                        ide_context_init_build_system,
                        ide_context_init_vcs,
                        ide_context_init_project_name,
                        ide_context_init_unsaved_files,
                        NULL);
}

static gboolean
ide_context_init_finish (GAsyncInitable  *initable,
                         GAsyncResult    *result,
                         GError         **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_CONTEXT (initable), FALSE);

  return g_task_propagate_boolean (task, error);
}

static void
async_initable_init (GAsyncInitableIface *iface)
{
  iface->init_async = ide_context_init_async;
  iface->init_finish = ide_context_init_finish;
}
