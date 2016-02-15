/* ide-context.c
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

#define G_LOG_DOMAIN "ide-context"

#include <glib/gi18n.h>
#include <libpeas/peas.h>

#include "ide-async-helper.h"
#include "ide-back-forward-list.h"
#include "ide-back-forward-list-private.h"
#include "ide-buffer-manager.h"
#include "ide-buffer.h"
#include "ide-build-system.h"
#include "ide-configuration-manager.h"
#include "ide-context.h"
#include "ide-debug.h"
#include "ide-device-manager.h"
#include "ide-global.h"
#include "ide-internal.h"
#include "ide-project.h"
#include "ide-project-item.h"
#include "ide-project-files.h"
#include "ide-runtime-manager.h"
#include "ide-script-manager.h"
#include "ide-search-engine.h"
#include "ide-search-provider.h"
#include "ide-service.h"
#include "ide-settings.h"
#include "ide-source-snippets-manager.h"
#include "ide-unsaved-file.h"
#include "ide-unsaved-files.h"
#include "ide-vcs.h"
#include "ide-recent-projects.h"

#include "doap/ide-doap.h"

#define RESTORE_FILES_MAX_FILES 20

struct _IdeContext
{
  GObject                   parent_instance;

  IdeBackForwardList       *back_forward_list;
  IdeBufferManager         *buffer_manager;
  IdeBuildSystem           *build_system;
  IdeConfigurationManager  *configuration_manager;
  IdeDeviceManager         *device_manager;
  IdeDoap                  *doap;
  GtkRecentManager         *recent_manager;
  IdeRuntimeManager        *runtime_manager;
  IdeScriptManager         *script_manager;
  IdeSearchEngine          *search_engine;
  IdeSourceSnippetsManager *snippets_manager;
  IdeProject               *project;
  GFile                    *project_file;
  gchar                    *root_build_dir;
  gchar                    *recent_projects_path;
  PeasExtensionSet         *services;
  GHashTable               *services_by_gtype;
  IdeUnsavedFiles          *unsaved_files;
  IdeVcs                   *vcs;

  guint                     restored : 1;
  guint                     restoring : 1;

  GMutex                    unload_mutex;
  gint                      hold_count;
  GTask                    *delayed_unload_task;
};

static void async_initable_init (GAsyncInitableIface *);

G_DEFINE_TYPE_EXTENDED (IdeContext, ide_context, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE,
                                               async_initable_init))

enum {
  PROP_0,
  PROP_BACK_FORWARD_LIST,
  PROP_BUFFER_MANAGER,
  PROP_BUILD_SYSTEM,
  PROP_CONFIGURATION_MANAGER,
  PROP_DEVICE_MANAGER,
  PROP_PROJECT_FILE,
  PROP_PROJECT,
  PROP_ROOT_BUILD_DIR,
  PROP_RUNTIME_MANAGER,
  PROP_SCRIPT_MANAGER,
  PROP_SEARCH_ENGINE,
  PROP_SNIPPETS_MANAGER,
  PROP_VCS,
  PROP_UNSAVED_FILES,
  LAST_PROP
};

enum {
  LOADED,
  LAST_SIGNAL
};

static GParamSpec *properties [LAST_PROP];
static guint signals [LAST_SIGNAL];

/**
 * ide_context_get_recent_manager:
 *
 * Gets the IdeContext:recent-manager property. The recent manager is a GtkRecentManager instance
 * that should be used for the workbench.
 *
 * Returns: (transfer none): A #GtkRecentManager.
 */
GtkRecentManager *
ide_context_get_recent_manager (IdeContext *self)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  return self->recent_manager;
}

/**
 * ide_context_get_back_forward_list:
 *
 * Retrieves the global back forward list for the #IdeContext.
 *
 * Consumers of this should branch the #IdeBackForwardList and merge them
 * when there document stack is closed.
 *
 * See ide_back_forward_list_branch() and ide_back_forward_list_merge() for
 * more information.
 *
 * Returns: (transfer none): An #IdeBackForwardList.
 */
IdeBackForwardList *
ide_context_get_back_forward_list (IdeContext *self)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  return self->back_forward_list;
}

/**
 * ide_context_get_buffer_manager:
 *
 * Gets the #IdeContext:buffer-manager property. The buffer manager is responsible for loading
 * and saving buffers (files) within the #IdeContext. It provides a convenient place for scripts
 * to hook into the load and save process.
 *
 * Returns: (transfer none): An #IdeBufferManager.
 */
IdeBufferManager *
ide_context_get_buffer_manager (IdeContext *self)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  return self->buffer_manager;
}

/**
 * ide_context_get_build_system:
 *
 * Fetches the "build-system" property of @context.
 *
 * Returns: (transfer none): An #IdeBuildSystem.
 */
IdeBuildSystem *
ide_context_get_build_system (IdeContext *self)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  return self->build_system;
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
ide_context_get_device_manager (IdeContext *self)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  return self->device_manager;
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
ide_context_get_root_build_dir (IdeContext *self)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  return self->root_build_dir;
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
ide_context_set_root_build_dir (IdeContext  *self,
                                const gchar *root_build_dir)
{
  g_return_if_fail (IDE_IS_CONTEXT (self));
  g_return_if_fail (root_build_dir);

  if (self->root_build_dir != root_build_dir)
    {
      g_free (self->root_build_dir);
      self->root_build_dir = g_strdup (root_build_dir);
      g_object_notify_by_pspec (G_OBJECT (self),
                                properties [PROP_ROOT_BUILD_DIR]);
    }
}

/**
 * ide_context_get_snippets_manager:
 *
 * Gets the #IdeContext:snippets-manager property.
 *
 * Returns: (transfer none): An #IdeSourceSnippetsManager.
 */
IdeSourceSnippetsManager *
ide_context_get_snippets_manager (IdeContext *self)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  return self->snippets_manager;
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
ide_context_get_unsaved_files (IdeContext *self)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  return self->unsaved_files;
}

/**
 * ide_context_get_vcs:
 *
 * Retrieves the #IdeVcs used to load the project. If no version control system
 * could be found, this will return an #IdeDirectoryVcs.
 *
 * Returns: (transfer none): An #IdeVcs.
 */
IdeVcs *
ide_context_get_vcs (IdeContext *self)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  return self->vcs;
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
  g_autoptr(GTask) task = NULL;

  IDE_ENTRY;

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

  IDE_EXIT;
}

/**
 * ide_context_new_finish:
 *
 * Returns: (transfer full): An #IdeContext or %NULL upon failure and
 *   @error is set.
 */
IdeContext *
ide_context_new_finish (GAsyncResult  *result,
                        GError       **error)
{
  GTask *task = (GTask *)result;
  IdeContext *ret;

  IDE_ENTRY;

  g_return_val_if_fail (G_IS_TASK (task), NULL);

  ret = g_task_propagate_pointer (task, error);

  IDE_RETURN (ret);
}

/**
 * ide_context_get_project:
 *
 * Retrieves the #IdeProject for the context.
 *
 * Returns: (transfer none): An #IdeContext.
 */
IdeProject *
ide_context_get_project (IdeContext *self)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  return self->project;
}

/**
 * ide_context_get_project_file:
 *
 * Retrieves a #GFile containing the project file that was used to load
 * the context.
 *
 * Returns: (transfer none): A #GFile.
 */
GFile *
ide_context_get_project_file (IdeContext *self)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  return self->project_file;
}

static void
ide_context_set_project_file (IdeContext *self,
                              GFile      *project_file)
{
  g_return_if_fail (IDE_IS_CONTEXT (self));

  if (g_set_object (&self->project_file, project_file))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PROJECT_FILE]);
}

/**
 * ide_context_get_script_manager:
 *
 * Retrieves the script manager for the context.
 *
 * Returns: (transfer none): An #IdeScriptManager.
 */
IdeScriptManager *
ide_context_get_script_manager (IdeContext *self)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  return self->script_manager;
}

/**
 * ide_context_get_search_engine:
 *
 * Retrieves the search engine for the context.
 *
 * Returns: (transfer none): An #IdeSearchEngine.
 */
IdeSearchEngine *
ide_context_get_search_engine (IdeContext *self)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  return self->search_engine;
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
ide_context_get_service_typed (IdeContext *self,
                               GType       service_type)
{
  IdeService *service;
  GHashTableIter iter;
  gpointer key;
  gpointer value;

  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);
  g_return_val_if_fail (g_type_is_a (service_type, IDE_TYPE_SERVICE), NULL);

  service = g_hash_table_lookup (self->services_by_gtype, GSIZE_TO_POINTER (service_type));
  if (service != NULL)
    return service;

  g_hash_table_iter_init (&iter, self->services_by_gtype);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      if (g_type_is_a (service_type, GPOINTER_TO_SIZE (key)))
        return value;
    }

  return NULL;
}

static GFile *
get_back_forward_list_file (IdeContext *self)
{
  const gchar *project_name;
  g_autofree gchar *name = NULL;
  g_autofree gchar *path = NULL;
  GFile *file;

  g_assert (IDE_IS_CONTEXT (self));

  project_name = ide_project_get_name (self->project);
  name = g_strdup_printf ("%s.back-forward-list", project_name);
  path = g_build_filename (g_get_user_data_dir (),
                           "gnome-builder",
                           g_strdelimit (name, " \t\n", '_'),
                           NULL);
  file = g_file_new_for_path (path);

  return file;
}

static void
ide_context_service_notify_loaded (PeasExtensionSet *set,
                                   PeasPluginInfo   *plugin_info,
                                   PeasExtension    *exten,
                                   gpointer          user_data)
{
  g_assert (IDE_IS_SERVICE (exten));

  _ide_service_emit_context_loaded (IDE_SERVICE (exten));
}

static void
ide_context_loaded (IdeContext *self)
{
  g_assert (IDE_IS_CONTEXT (self));

  peas_extension_set_foreach (self->services,
                              ide_context_service_notify_loaded,
                              self);
}

static void
ide_context_dispose (GObject *object)
{
  IDE_ENTRY;

  /*
   * TODO: Shutdown services.
   */

  G_OBJECT_CLASS (ide_context_parent_class)->dispose (object);

  IDE_EXIT;
}

static void
ide_context_finalize (GObject *object)
{
  IdeContext *self = (IdeContext *)object;

  IDE_ENTRY;

  g_clear_pointer (&self->services, g_hash_table_unref);
  g_clear_pointer (&self->root_build_dir, g_free);
  g_clear_pointer (&self->recent_projects_path, g_free);

  g_clear_object (&self->build_system);
  g_clear_object (&self->configuration_manager);
  g_clear_object (&self->device_manager);
  g_clear_object (&self->doap);
  g_clear_object (&self->project);
  g_clear_object (&self->project_file);
  g_clear_object (&self->recent_manager);
  g_clear_object (&self->runtime_manager);
  g_clear_object (&self->unsaved_files);
  g_clear_object (&self->vcs);

  g_mutex_clear (&self->unload_mutex);

  G_OBJECT_CLASS (ide_context_parent_class)->finalize (object);

  _ide_battery_monitor_shutdown ();

  IDE_EXIT;
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
    case PROP_BACK_FORWARD_LIST:
      g_value_set_object (value, ide_context_get_back_forward_list (self));
      break;

    case PROP_BUFFER_MANAGER:
      g_value_set_object (value, ide_context_get_buffer_manager (self));
      break;

    case PROP_BUILD_SYSTEM:
      g_value_set_object (value, ide_context_get_build_system (self));
      break;

    case PROP_CONFIGURATION_MANAGER:
      g_value_set_object (value, ide_context_get_configuration_manager (self));
      break;

    case PROP_DEVICE_MANAGER:
      g_value_set_object (value, ide_context_get_device_manager (self));
      break;

    case PROP_PROJECT:
      g_value_set_object (value, ide_context_get_project (self));
      break;

    case PROP_PROJECT_FILE:
      g_value_set_object (value, ide_context_get_project_file (self));
      break;

    case PROP_ROOT_BUILD_DIR:
      g_value_set_string (value, ide_context_get_root_build_dir (self));
      break;

    case PROP_RUNTIME_MANAGER:
      g_value_set_object (value, ide_context_get_runtime_manager (self));
      break;

    case PROP_SCRIPT_MANAGER:
      g_value_set_object (value, ide_context_get_script_manager (self));
      break;

    case PROP_SEARCH_ENGINE:
      g_value_set_object (value, ide_context_get_search_engine (self));
      break;

    case PROP_SNIPPETS_MANAGER:
      g_value_set_object (value, ide_context_get_snippets_manager (self));
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

  properties [PROP_BACK_FORWARD_LIST] =
    g_param_spec_object ("back-forward-list",
                         "Back Forward List",
                         "Back/forward navigation history for the context.",
                         IDE_TYPE_BACK_FORWARD_LIST,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_BUFFER_MANAGER] =
    g_param_spec_object ("buffer-manager",
                         "Buffer Manager",
                         "The buffer manager for the context.",
                         IDE_TYPE_BUFFER_MANAGER,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_BUILD_SYSTEM] =
    g_param_spec_object ("build-system",
                         "Build System",
                         "The build system used by the context.",
                         IDE_TYPE_BUILD_SYSTEM,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_CONFIGURATION_MANAGER] =
    g_param_spec_object ("configuration-manager",
                         "Configuration Manager",
                         "The configuration manager for the context",
                         IDE_TYPE_CONFIGURATION_MANAGER,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_DEVICE_MANAGER] =
    g_param_spec_object ("device-manager",
                         "Device Manager",
                         "The device manager for the context.",
                         IDE_TYPE_DEVICE_MANAGER,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_PROJECT] =
    g_param_spec_object ("project",
                         "Project",
                         "The project for the context.",
                         IDE_TYPE_PROJECT,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_PROJECT_FILE] =
    g_param_spec_object ("project-file",
                         "Project File",
                         "The project file for the context.",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_ROOT_BUILD_DIR] =
    g_param_spec_string ("root-build-dir",
                         "Root Build Directory",
                         "The root directory to perform builds within.",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_RUNTIME_MANAGER] =
    g_param_spec_object ("runtime-manager",
                         "Runtime Manager",
                         "Runtime Manager",
                         IDE_TYPE_RUNTIME_MANAGER,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SCRIPT_MANAGER] =
    g_param_spec_object ("script-manager",
                         "Script Manager",
                         "The script manager for the context.",
                         IDE_TYPE_SCRIPT_MANAGER,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SEARCH_ENGINE] =
    g_param_spec_object ("search-engine",
                         "Search Engine",
                         "The search engine for the context.",
                         IDE_TYPE_SEARCH_ENGINE,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SNIPPETS_MANAGER] =
    g_param_spec_object ("snippets-manager",
                         "Snippets Manager",
                         "The snippets manager for the context.",
                         IDE_TYPE_SOURCE_SNIPPETS_MANAGER,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_UNSAVED_FILES] =
    g_param_spec_object ("unsaved-files",
                         "Unsaved Files",
                         "The unsaved files in the context.",
                         IDE_TYPE_UNSAVED_FILES,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_VCS] =
    g_param_spec_object ("vcs",
                         "VCS",
                         "The VCS for the context.",
                         IDE_TYPE_VCS,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  /**
   * IdeContext::loaded:
   *
   * This signal is emitted when loading of the context has completed.
   * Plugins and services might want to get notified of this to perform
   * work that requires subsystems that may not be loaded during context
   * startup.
   */
  signals [LOADED] =
    g_signal_new_class_handler ("loaded",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (ide_context_loaded),
                                NULL, NULL, NULL,
                                G_TYPE_NONE, 0);
}

static void
ide_context_init (IdeContext *self)
{
  g_autofree gchar *scriptsdir = NULL;

  IDE_ENTRY;

  g_mutex_init (&self->unload_mutex);

  self->recent_manager = g_object_ref (gtk_recent_manager_get_default ());

  self->root_build_dir = g_build_filename (g_get_user_cache_dir (),
                                           ide_get_program_name (),
                                           "builds",
                                           NULL);

  self->recent_projects_path = g_build_filename (g_get_user_data_dir (),
                                                 ide_get_program_name (),
                                                 IDE_RECENT_PROJECTS_BOOKMARK_FILENAME,
                                                 NULL);

  self->back_forward_list = g_object_new (IDE_TYPE_BACK_FORWARD_LIST,
                                          "context", self,
                                          NULL);

  self->buffer_manager = g_object_new (IDE_TYPE_BUFFER_MANAGER,
                                       "context", self,
                                       NULL);

  self->device_manager = g_object_new (IDE_TYPE_DEVICE_MANAGER,
                                       "context", self,
                                       NULL);

  self->configuration_manager = g_object_new (IDE_TYPE_CONFIGURATION_MANAGER,
                                              "context", self,
                                              NULL);

  self->project = g_object_new (IDE_TYPE_PROJECT,
                                "context", self,
                                NULL);

  self->runtime_manager = g_object_new (IDE_TYPE_RUNTIME_MANAGER,
                                        "context", self,
                                        NULL);

  self->unsaved_files = g_object_new (IDE_TYPE_UNSAVED_FILES,
                                      "context", self,
                                      NULL);

  self->snippets_manager = g_object_new (IDE_TYPE_SOURCE_SNIPPETS_MANAGER, NULL);

  scriptsdir = g_build_filename (g_get_user_config_dir (),
                                 ide_get_program_name (),
                                 "scripts",
                                 NULL);
  self->script_manager = g_object_new (IDE_TYPE_SCRIPT_MANAGER,
                                       "context", self,
                                       "scripts-directory", scriptsdir,
                                       NULL);

  IDE_EXIT;
}

static void
ide_context_load_doap_worker (GTask        *task,
                              gpointer      source_object,
                              gpointer      task_data,
                              GCancellable *cancellable)
{
  IdeContext *self = source_object;
  g_autofree gchar *name = NULL;
  g_autoptr(GFile) directory = NULL;
  g_autoptr(GFileEnumerator) enumerator = NULL;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_CONTEXT (self));

  if (g_file_query_file_type (self->project_file, 0, cancellable) == G_FILE_TYPE_DIRECTORY)
    directory = g_object_ref (self->project_file);
  else
    directory = g_file_get_parent (self->project_file);

  name = g_file_get_basename (directory);

  enumerator = g_file_enumerate_children (directory,
                                          G_FILE_ATTRIBUTE_STANDARD_NAME,
                                          G_FILE_QUERY_INFO_NONE,
                                          cancellable,
                                          NULL);

  if (enumerator != NULL)
    {
      gpointer infoptr;

      while ((infoptr = g_file_enumerator_next_file (enumerator, cancellable, NULL)))
        {
          g_autoptr(GFileInfo) file_info = infoptr;
          const gchar *filename;

          filename = g_file_info_get_name (file_info);

          if (!ide_str_empty0 (filename) && g_str_has_suffix (filename, ".doap"))
            {
              g_autoptr(GFile) file = NULL;
              g_autoptr(IdeDoap) doap = NULL;

              file = g_file_get_child (directory, filename);
              doap = ide_doap_new ();

              if (ide_doap_load_from_file (doap, file, cancellable, NULL))
                {
                  const gchar *doap_name;

                  if ((doap_name = ide_doap_get_name (doap)))
                    {
                      g_free (name);
                      name = g_strdup (doap_name);
                    }

                  self->doap = g_object_ref (doap);

                  break;
                }
            }
        }
    }

  _ide_project_set_name (self->project, name);

  g_task_return_boolean (task, TRUE);
}

static void
ide_context_init_project_name (gpointer             source_object,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  IdeContext *self = source_object;
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_CONTEXT (self));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_run_in_thread (task, ide_context_load_doap_worker);
}

static void
ide_context_init_vcs_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  IdeContext *self;
  g_autoptr(GTask) task = user_data;
  g_autoptr(IdeVcs) vcs = NULL;
  GError *error = NULL;

  g_return_if_fail (G_IS_ASYNC_RESULT (result));
  g_return_if_fail (G_IS_TASK (task));

  self = g_task_get_source_object (task);

  if (!(vcs = ide_vcs_new_finish (result, &error)))
    {
      g_task_return_error (task, error);
      return;
    }

  self->vcs = g_object_ref (vcs);

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
  IdeContext *self;
  g_autoptr(GFile) project_file = NULL;
  GError *error = NULL;

  self = g_task_get_source_object (task);

  if (!(build_system = ide_build_system_new_finish (result, &error)))
    {
      g_task_return_error (task, error);
      return;
    }

  self->build_system = g_object_ref (build_system);

  /* allow the build system to override the project file */
  g_object_get (self->build_system,
                "project-file", &project_file,
                NULL);
  if (project_file != NULL)
    ide_context_set_project_file (self, project_file);

  g_task_return_boolean (task, TRUE);
}

static void
ide_context_init_build_system (gpointer             source_object,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  IdeContext *self = source_object;
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_CONTEXT (self));

  task = g_task_new (self, cancellable, callback, user_data);
  ide_build_system_new_async (self,
                              self->project_file,
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

  g_assert (IDE_IS_UNSAVED_FILES (unsaved_files));

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
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_CONTEXT (self));

  task = g_task_new (self, cancellable, callback, user_data);
  ide_unsaved_files_restore_async (self->unsaved_files,
                                   cancellable,
                                   ide_context_init_unsaved_files_cb,
                                   g_object_ref (task));
}

static void
ide_context_init_scripts_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  IdeScriptManager *manager = (IdeScriptManager *)object;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;

  g_assert (IDE_IS_SCRIPT_MANAGER (manager));
  g_assert (G_IS_TASK (task));

  if (!ide_script_manager_load_finish (manager, result, &error))
    {
      g_task_return_error (task, error);
      return;
    }

  g_task_return_boolean (task, TRUE);
}

static void
ide_context_init_scripts (gpointer             source_object,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
  IdeContext *self = source_object;
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_CONTEXT (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  ide_script_manager_load_async (self->script_manager,
                                 cancellable,
                                 ide_context_init_scripts_cb,
                                 g_object_ref (task));
}

static void
ide_context_init_snippets_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  IdeSourceSnippetsManager *manager = (IdeSourceSnippetsManager *)object;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;

  g_return_if_fail (IDE_IS_SOURCE_SNIPPETS_MANAGER (manager));

  if (!ide_source_snippets_manager_load_finish (manager, result, &error))
    {
      g_task_return_error (task, error);
      return;
    }

  g_task_return_boolean (task, TRUE);
}

static void
ide_context_init_snippets (gpointer             source_object,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  IdeContext *self = source_object;
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_CONTEXT (self));

  task = g_task_new (self, cancellable, callback, user_data);

  ide_source_snippets_manager_load_async (self->snippets_manager,
                                          cancellable,
                                          ide_context_init_snippets_cb,
                                          g_object_ref (task));
}

static void
ide_context__back_forward_list_load_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  IdeBackForwardList *back_forward_list = (IdeBackForwardList *)object;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;

  g_assert (IDE_IS_BACK_FORWARD_LIST (back_forward_list));
  g_assert (G_IS_TASK (task));

  /*
   * Failing to load the back-forward list is non-fatal. We'll fix it during
   * our next write to the file.
   */
  if (!_ide_back_forward_list_load_finish (back_forward_list, result, &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        g_warning ("%s", error->message);
      g_clear_error (&error);
    }

  g_task_return_boolean (task, TRUE);
}

static void
ide_context_init_back_forward_list (gpointer             source_object,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  IdeContext *self = source_object;
  g_autoptr(GTask) task = NULL;
  g_autoptr(GFile) file = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_CONTEXT (self));

  task = g_task_new (self, cancellable, callback, user_data);

  file = get_back_forward_list_file (self);
  _ide_back_forward_list_load_async (self->back_forward_list,
                                     file,
                                     cancellable,
                                     ide_context__back_forward_list_load_cb,
                                     g_object_ref (task));

  IDE_EXIT;
}

static void
ide_context_service_added (PeasExtensionSet *set,
                           PeasPluginInfo   *info,
                           PeasExtension    *exten,
                           gpointer          user_data)
{
  IdeContext *self = user_data;

  g_assert (IDE_IS_CONTEXT (self));
  g_assert (IDE_IS_SERVICE (exten));


  g_hash_table_insert (self->services_by_gtype,
                       GSIZE_TO_POINTER (G_OBJECT_TYPE (exten)),
                       exten);

  ide_service_start (IDE_SERVICE (exten));
}

static void
ide_context_service_removed (PeasExtensionSet *set,
                             PeasPluginInfo   *info,
                             PeasExtension    *exten,
                             gpointer          user_data)
{
  IdeContext *self = user_data;

  g_assert (IDE_IS_CONTEXT (self));
  g_assert (IDE_IS_SERVICE (exten));

  ide_service_stop (IDE_SERVICE (exten));

  g_hash_table_remove (self->services_by_gtype,
                       GSIZE_TO_POINTER (G_OBJECT_TYPE (exten)));
}

static void
ide_context_init_services (gpointer             source_object,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  IdeContext *self = source_object;
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_CONTEXT (self));

  task = g_task_new (self, cancellable, callback, user_data);

  self->services_by_gtype = g_hash_table_new (NULL, NULL);
  self->services = peas_extension_set_new (peas_engine_get_default (),
                                           IDE_TYPE_SERVICE,
                                           "context", self,
                                           NULL);

  g_signal_connect_object (self->services,
                           "extension-added",
                           G_CALLBACK (ide_context_service_added),
                           self,
                           0);

  g_signal_connect_object (self->services,
                           "extension-removed",
                           G_CALLBACK (ide_context_service_removed),
                           self,
                           0);

  peas_extension_set_foreach (self->services,
                              (PeasExtensionSetForeachFunc)ide_context_service_added,
                              self);

  g_task_return_boolean (task, TRUE);
}

static void
ide_context_init_add_recent (gpointer             source_object,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  IdeContext *self = source_object;
  g_autoptr(GBookmarkFile) projects_file = NULL;
  g_autoptr(GPtrArray) groups = NULL;
  g_autoptr(GTask) task = NULL;
  g_autofree gchar *uri = NULL;
  g_autofree gchar *app_exec = NULL;
  g_autofree gchar *dir = NULL;
  GError *error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_CONTEXT (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  projects_file = g_bookmark_file_new ();
  g_bookmark_file_load_from_file (projects_file, self->recent_projects_path, &error);

  /*
   * If there was an error loading the file and the error is not "File does not exist"
   * then stop saving operation
   */
  if ((error != NULL) && !g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
    {
      g_warning ("Unable to open recent projects %s file: %s",
                 self->recent_projects_path, error->message);
      g_task_return_boolean (task, TRUE);
      g_clear_error (&error);
      IDE_EXIT;
    }

  g_clear_error (&error);

  uri = g_file_get_uri (self->project_file);
  app_exec = g_strdup_printf ("%s -p %%p", ide_get_program_name ());

  g_bookmark_file_set_title (projects_file, uri, ide_project_get_name (self->project));
  g_bookmark_file_set_mime_type (projects_file, uri, "application/x-builder-project");
  g_bookmark_file_add_application (projects_file, uri, ide_get_program_name (), app_exec);
  g_bookmark_file_set_is_private (projects_file, uri, FALSE);

  /* attach project description to recent info */
  if (self->doap != NULL)
    g_bookmark_file_set_description (projects_file, uri, ide_doap_get_shortdesc (self->doap));

  /* attach discovered languages to recent info */
  groups = g_ptr_array_new_with_free_func (g_free);
  g_ptr_array_add (groups, g_strdup (IDE_RECENT_PROJECTS_GROUP));
  if (self->doap != NULL)
    {
      gchar **languages;
      gsize i;

      if ((languages = ide_doap_get_languages (self->doap)))
        {
          for (i = 0; languages [i]; i++)
            g_ptr_array_add (groups,
                             g_strdup_printf ("%s%s",
                                              IDE_RECENT_PROJECTS_LANGUAGE_GROUP_PREFIX,
                                              languages [i]));
        }
    }

  g_bookmark_file_set_groups (projects_file, uri, (const gchar **)groups->pdata, groups->len);

  IDE_TRACE_MSG ("Registering %s as recent project.", uri);

  /* ensure the containing directory exists */
  dir = g_path_get_dirname (self->recent_projects_path);
  g_mkdir_with_parents (dir, 0750);

  if (!g_bookmark_file_to_file (projects_file, self->recent_projects_path, &error))
    {
       g_warning ("Unable to save recent projects %s file: %s",
                  self->recent_projects_path, error->message);
       g_clear_error (&error);
    }

  g_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_context_init_search_engine (gpointer             source_object,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  IdeContext *self = source_object;

  g_assert (IDE_IS_CONTEXT (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  self->search_engine = g_object_new (IDE_TYPE_SEARCH_ENGINE,
                                      "context", self,
                                      NULL);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_return_boolean (task, TRUE);
}

static void
ide_context_init_configuration_manager_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  GAsyncInitable *initable = (GAsyncInitable *)object;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;

  g_assert (G_IS_ASYNC_INITABLE (initable));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!g_async_initable_init_finish (initable, result, &error))
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, TRUE);
}

static void
ide_context_init_configuration_manager (gpointer             source_object,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  IdeContext *self = source_object;
  g_autoptr(GTask) task = NULL;

  g_assert (IDE_IS_CONTEXT (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_async_initable_init_async (G_ASYNC_INITABLE (self->configuration_manager),
                               G_PRIORITY_DEFAULT,
                               cancellable,
                               ide_context_init_configuration_manager_cb,
                               g_object_ref (task));
}

static void
ide_context_init_loaded (gpointer             source_object,
                         GCancellable        *cancellable,
                         GAsyncReadyCallback  callback,
                         gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  IdeContext *self = source_object;

  g_assert (IDE_IS_CONTEXT (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_signal_emit (self, signals [LOADED], 0);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_return_boolean (task, TRUE);
}

static void
ide_context_init_async (GAsyncInitable      *initable,
                        int                  io_priority,
                        GCancellable        *cancellable,
                        GAsyncReadyCallback  callback,
                        gpointer             user_data)
{
  IdeContext *context = (IdeContext *)initable;

  g_return_if_fail (G_IS_ASYNC_INITABLE (context));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  ide_async_helper_run (context,
                        cancellable,
                        callback,
                        user_data,
                        ide_context_init_build_system,
                        ide_context_init_vcs,
                        ide_context_init_services,
                        ide_context_init_project_name,
                        ide_context_init_back_forward_list,
                        ide_context_init_snippets,
                        ide_context_init_scripts,
                        ide_context_init_unsaved_files,
                        ide_context_init_add_recent,
                        ide_context_init_search_engine,
                        ide_context_init_configuration_manager,
                        ide_context_init_loaded,
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

static void
ide_context_unload__buffer_manager_save_file_cb (GObject      *object,
                                                 GAsyncResult *result,
                                                 gpointer      user_data)
{
  IdeBufferManager *buffer_manager = (IdeBufferManager *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  gint in_progress;

  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));
  g_assert (G_IS_TASK (task));

  in_progress = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (task), "IN_PROGRESS"));
  g_assert (in_progress > 0);
  in_progress--;
  g_object_set_data (G_OBJECT (task), "IN_PROGRESS", GINT_TO_POINTER (in_progress));

  if (!ide_buffer_manager_save_file_finish (buffer_manager, result, &error))
    g_warning ("%s", error->message);

  if (in_progress == 0)
    g_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_context_unload_buffer_manager (gpointer             source_object,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  IdeContext *self = source_object;
  g_autoptr(GTask) task = NULL;
  g_autoptr(GPtrArray) buffers = NULL;
  gsize i;
  guint skipped = 0;

  IDE_ENTRY;

  g_assert (IDE_IS_CONTEXT (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  buffers = ide_buffer_manager_get_buffers (self->buffer_manager);

  task = g_task_new (self, cancellable, callback, user_data);

  if (buffers->len == 0)
    {
      g_task_return_boolean (task, TRUE);
      return;
    }

  g_object_set_data (G_OBJECT (task), "IN_PROGRESS", GINT_TO_POINTER (buffers->len));

  for (i = 0; i < buffers->len; i++)
    {
      IdeBuffer *buffer;
      IdeFile *file;

      buffer = g_ptr_array_index (buffers, i);
      file = ide_buffer_get_file (buffer);

      if (!gtk_text_buffer_get_modified (GTK_TEXT_BUFFER (buffer)))
        {
          skipped++;
          continue;
        }

      ide_buffer_manager_save_file_async (self->buffer_manager,
                                          buffer,
                                          file,
                                          NULL,
                                          cancellable,
                                          ide_context_unload__buffer_manager_save_file_cb,
                                          g_object_ref (task));
    }

  if (skipped > 0)
    {
      guint count;

      count = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (task), "IN_PROGRESS"));
      count -= skipped;
      g_object_set_data (G_OBJECT (task), "IN_PROGRESS", GINT_TO_POINTER (count));

      if (count == 0)
        g_task_return_boolean (task, TRUE);
    }

  IDE_EXIT;
}

static void
ide_context_unload__configuration_manager_save_cb (GObject      *object,
                                                   GAsyncResult *result,
                                                   gpointer      user_data)
{
  IdeConfigurationManager *manager = (IdeConfigurationManager *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_CONFIGURATION_MANAGER (manager));
  g_assert (G_IS_TASK (task));

  /* unfortunate if this happens, but not much we can do */
  if (!ide_configuration_manager_save_finish (manager, result, &error))
    g_warning ("%s", error->message);

  g_task_return_boolean (task, TRUE);
}

static void
ide_context_unload_configuration_manager (gpointer             source_object,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data)
{
  IdeContext *self = source_object;
  g_autoptr(GTask) task = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_CONTEXT (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (IDE_IS_CONFIGURATION_MANAGER (self->configuration_manager));

  task = g_task_new (self, cancellable, callback, user_data);

  ide_configuration_manager_save_async (self->configuration_manager,
                                        cancellable,
                                        ide_context_unload__configuration_manager_save_cb,
                                        g_object_ref (task));

  IDE_EXIT;
}

static void
ide_context_unload__back_forward_list_save_cb (GObject      *object,
                                               GAsyncResult *result,
                                               gpointer      user_data)
{
  IdeBackForwardList *back_forward_list = (IdeBackForwardList *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_BACK_FORWARD_LIST (back_forward_list));
  g_assert (G_IS_TASK (task));

  /* nice to know, but not critical to save process */
  if (!_ide_back_forward_list_save_finish (back_forward_list, result, &error))
    g_warning ("%s", error->message);

  g_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_context_unload_back_forward_list (gpointer             source_object,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  IdeContext *self = source_object;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GTask) task = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_CONTEXT (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  file = get_back_forward_list_file (self);
  _ide_back_forward_list_save_async (self->back_forward_list,
                                     file,
                                     cancellable,
                                     ide_context_unload__back_forward_list_save_cb,
                                     g_object_ref (task));

  IDE_EXIT;
}

static void
ide_context_unload__unsaved_files_save_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  IdeUnsavedFiles *unsaved_files = (IdeUnsavedFiles *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_UNSAVED_FILES (unsaved_files));
  g_assert (G_IS_TASK (task));

  /* nice to know, but not critical to rest of shutdown */
  if (!ide_unsaved_files_save_finish (unsaved_files, result, &error))
    g_warning ("%s", error->message);

  g_task_return_boolean (task, TRUE);
}

static void
ide_context_unload_unsaved_files (gpointer             source_object,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  IdeContext *self = source_object;
  g_autoptr(GTask) task = NULL;

  g_assert (IDE_IS_CONTEXT (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  ide_unsaved_files_save_async (self->unsaved_files,
                                cancellable,
                                ide_context_unload__unsaved_files_save_cb,
                                g_object_ref (task));
}

static void
ide_context_unload_cb (GObject      *object,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  GTask *unload_task = (GTask *)result;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;

  g_assert (IDE_IS_CONTEXT (object));
  g_assert (G_IS_TASK (task));

  if (!g_task_propagate_boolean (unload_task, &error))
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, TRUE);
}

static void
ide_context_do_unload_locked (IdeContext *self)
{
  g_autoptr(GTask) task = NULL;

  g_assert (IDE_IS_CONTEXT (self));
  g_assert (self->delayed_unload_task != NULL);

  task = self->delayed_unload_task;
  self->delayed_unload_task = NULL;

  g_clear_object (&self->device_manager);
  g_clear_object (&self->runtime_manager);

  ide_async_helper_run (self,
                        g_task_get_cancellable (task),
                        ide_context_unload_cb,
                        g_object_ref (task),
                        ide_context_unload_configuration_manager,
                        ide_context_unload_back_forward_list,
                        ide_context_unload_buffer_manager,
                        ide_context_unload_unsaved_files,
                        NULL);
}

/**
 * ide_context_unload_async:
 *
 * This function attempts to unload various components in the #IdeContext. This
 * should be called before you dispose the context. Unsaved buffers will be
 * persisted to the drafts directory.  More operations may be added in the
 * future.
 *
 * If there is a hold on the #IdeContext, created by ide_context_hold(), then
 * the unload request will be delayed until the appropriate number of calls to
 * ide_context_release() have been called.
 */
void
ide_context_unload_async (IdeContext          *self,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_CONTEXT (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  g_mutex_lock (&self->unload_mutex);

  if (self->delayed_unload_task != NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_PENDING,
                               _("An unload request is already pending"));
      IDE_GOTO (failure);
    }

  self->delayed_unload_task = g_object_ref (task);

  if (self->hold_count == 0)
    ide_context_do_unload_locked (self);

failure:
  g_mutex_unlock (&self->unload_mutex);

  IDE_EXIT;
}

gboolean
ide_context_unload_finish (IdeContext    *self,
                           GAsyncResult  *result,
                           GError       **error)
{
  GTask *task = (GTask *)result;
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_CONTEXT (self), FALSE);

  ret = g_task_propagate_boolean (task, error);

  IDE_RETURN (ret);
}

static gboolean restore_in_idle (gpointer user_data);

static void
ide_context_restore__load_file_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  IdeBufferManager *buffer_manager = (IdeBufferManager *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));
  g_assert (G_IS_TASK (task));

  if (!ide_buffer_manager_load_file_finish (buffer_manager, result, &error))
    {
      g_warning ("%s", error->message);
      /* TODO: add error into grouped error */
    }

  g_idle_add (restore_in_idle, g_object_ref (task));
}

static gboolean
restore_in_idle (gpointer user_data)
{
  g_autoptr(IdeFile) ifile = NULL;
  g_autoptr(GTask) task = user_data;
  IdeUnsavedFile *uf;
  IdeContext *self;
  GPtrArray *ar;
  GFile *file;

  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  ar = g_task_get_task_data (task);

  if (ar == NULL || ar->len == 0)
    {
      self->restoring = FALSE;
      g_task_return_boolean (task, TRUE);
      return G_SOURCE_REMOVE;
    }

  g_assert (ar != NULL);
  g_assert (ar->len > 0);

  uf = g_ptr_array_index (ar, ar->len - 1);
  file = ide_unsaved_file_get_file (uf);
  ifile = ide_project_get_project_file (self->project, file);
  g_ptr_array_remove_index (ar, ar->len - 1);

  ide_buffer_manager_load_file_async (self->buffer_manager,
                                      ifile,
                                      FALSE,
                                      NULL,
                                      g_task_get_cancellable (task),
                                      ide_context_restore__load_file_cb,
                                      g_object_ref (task));

  return G_SOURCE_REMOVE;
}

void
ide_context_restore_async (IdeContext          *self,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  g_autoptr(GPtrArray) ar = NULL;

  g_return_if_fail (IDE_IS_CONTEXT (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  if (self->restored)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               _("Context has already been restored."));
      return;
    }

  self->restored = TRUE;

  ar = ide_unsaved_files_to_array (self->unsaved_files);

  if (ar->len == 0)
    {
      g_task_return_boolean (task, TRUE);
      return;
    }

  if (ar->len > RESTORE_FILES_MAX_FILES)
    {
      /*
       * To protect from some insanity, ignore attempts to restore files if
       * they are over RESTORE_FILES_MAX_FILES. Just prune and go back to
       * normal.  This should help in situations where hadn't pruned the
       * unsaved files list.
       */
      ide_unsaved_files_clear (self->unsaved_files);
      g_task_return_boolean (task, TRUE);
      return;
    }

  self->restoring = TRUE;

  g_task_set_task_data (task, g_ptr_array_ref (ar), (GDestroyNotify)g_ptr_array_unref);

  g_idle_add (restore_in_idle, g_object_ref (task));
}

gboolean
ide_context_restore_finish (IdeContext    *self,
                            GAsyncResult  *result,
                            GError       **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_CONTEXT (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (task), FALSE);

  return g_task_propagate_boolean (task, error);
}

gboolean
_ide_context_is_restoring (IdeContext *self)
{
  return self->restoring;
}

/**
 * ide_context_get_settings:
 *
 * Gets an #IdeSettings representing the given #GSettingsSchema.
 *
 * relative_path will be used to apply multiple layers of settings. Project settings will be
 * applied to first, followed by global settings.
 *
 * Returns: (transfer full): An #IdeSettings.
 */
IdeSettings *
ide_context_get_settings (IdeContext  *self,
                          const gchar *schema_id,
                          const gchar *relative_path)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);
  g_return_val_if_fail (schema_id != NULL, NULL);

  return  _ide_settings_new (self, schema_id, relative_path, FALSE);
}

/**
 * ide_context_hold:
 * @self: the #IdeContext
 *
 * Puts a hold on the #IdeContext, preventing the context from being unloaded
 * until a call to ide_context_release().
 *
 * If ide_context_unload_async() is called while a hold is in progress, the
 * unload will be delayed until ide_context_release() has been called the
 * same number of times as ide_context_hold().
 */
void
ide_context_hold (IdeContext *self)
{
  g_return_if_fail (IDE_IS_CONTEXT (self));
  g_return_if_fail (self->hold_count >= 0);

  g_object_ref (self);

  g_mutex_lock (&self->unload_mutex);
  self->hold_count++;
  g_mutex_unlock (&self->unload_mutex);
}

/**
 * ide_context_hold_for_object:
 * @self: An #IdeContext
 * @instance: (type GObject.Object): A #GObject instance
 *
 * Adds a hold on @self for the lifetime of @instance.
 */
void
ide_context_hold_for_object (IdeContext *self,
                             gpointer    instance)
{
  g_return_if_fail (IDE_IS_CONTEXT (self));
  g_return_if_fail (G_IS_OBJECT (instance));

  ide_context_hold (self);
  g_object_set_data_full (instance, "IDE_CONTEXT", self, (GDestroyNotify)ide_context_release);
}

/**
 * ide_context_release:
 * @self: the #IdeContext
 *
 * Releases a hold on the context previously created with ide_context_hold().
 *
 * If a pending unload of the context has been requested, it will be dispatched
 * once the hold count reaches zero.
 */
void
ide_context_release (IdeContext *self)
{
  g_return_if_fail (IDE_IS_CONTEXT (self));
  g_return_if_fail (self->hold_count > 0);

  g_mutex_lock (&self->unload_mutex);
  if ((--self->hold_count == 0) && (self->delayed_unload_task != NULL))
    ide_context_do_unload_locked (self);
  g_mutex_unlock (&self->unload_mutex);

  g_object_unref (self);
}

/**
 * ide_context_get_runtime_manager:
 * @self: An #IdeContext
 *
 * Gets the #IdeRuntimeManager for the LibIDE context.
 *
 * The runtime manager provies access to #IdeRuntime instances via the
 * #GListModel interface. These can provide support for building projects
 * in various runtimes such as xdg-app.
 *
 * Returns: (transfer none): An #IdeRuntimeManager.
 */
IdeRuntimeManager *
ide_context_get_runtime_manager (IdeContext *self)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  return self->runtime_manager;
}

/**
 * ide_context_get_configuration_manager:
 * @self: An #IdeContext
 *
 * Gets the #IdeConfigurationManager for the context.
 *
 * The configuration manager is responsible for loading and saving
 * configurations. Configurations consist of information about how to
 * perform a particular build. Such information includes the target
 * #IdeDevice, the #IdeRuntime to use, and various other build options.
 *
 * Returns: (transfer none): An #IdeConfigurationManager.
 */
IdeConfigurationManager *
ide_context_get_configuration_manager (IdeContext *self)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  return self->configuration_manager;
}

void
ide_context_warning (IdeContext  *self,
                     const gchar *format,
                     ...)
{
  va_list args;

  g_return_if_fail (IDE_IS_CONTEXT (self));
  g_return_if_fail (format != NULL);

  va_start (args, format);
  /*
   * TODO: Track logging information so that we can display warnings
   *       to the user in the workbench.
   */
  g_logv ("Ide", G_LOG_LEVEL_WARNING, format, args);
  va_end (args);
}
