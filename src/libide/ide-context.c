/* ide-context.c
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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

#include "config.h"

#include <glib/gi18n.h>
#include <libpeas/peas.h>

#include "ide-context.h"
#include "ide-debug.h"
#include "ide-global.h"
#include "ide-pausable.h"
#include "ide-service.h"

#include "application/ide-application.h"
#include "buffers/ide-buffer-manager.h"
#include "buffers/ide-buffer.h"
#include "buffers/ide-unsaved-file.h"
#include "buffers/ide-unsaved-files.h"
#include "buildsystem/ide-build-log.h"
#include "buildsystem/ide-build-log-private.h"
#include "buildsystem/ide-build-manager.h"
#include "buildsystem/ide-build-system.h"
#include "buildsystem/ide-build-system-discovery.h"
#include "config/ide-configuration-manager.h"
#include "diagnostics/ide-diagnostics-manager.h"
#include "debugger/ide-debug-manager.h"
#include "devices/ide-device-manager.h"
#include "doap/ide-doap.h"
#include "documentation/ide-documentation.h"
#include "plugins/ide-extension-util.h"
#include "projects/ide-project-item.h"
#include "projects/ide-project.h"
#include "projects/ide-recent-projects.h"
#include "runner/ide-run-manager.h"
#include "runtimes/ide-runtime-manager.h"
#include "search/ide-search-engine.h"
#include "search/ide-search-provider.h"
#include "snippets/ide-source-snippets-manager.h"
#include "testing/ide-test-manager.h"
#include "toolchain/ide-toolchain-manager.h"
#include "transfers/ide-transfer-manager.h"
#include "vcs/ide-vcs.h"
#include "vcs/ide-vcs-monitor.h"
#include "workbench/ide-workbench.h"
#include "threading/ide-task.h"
#include "util/ide-async-helper.h"
#include "util/ide-glib.h"
#include "util/ide-line-reader.h"
#include "util/ide-settings.h"

/**
 * SECTION:ide-context
 * @title: IdeContext
 * @short_description: Encapsulates all processing related to a project
 *
 * The #IdeContext encapsulates all processing related to a project. This
 * includes everything from project management, version control, debugging,
 * building, running the project, and more.
 *
 * ## Subsystems
 *
 * The context is broken into a series of subsystems which can be accessed
 * via accessors on the #IdeContext. Some subsystems include
 * #IdeBufferManager, #IdeBuildManager, #IdeBuildSystem,
 * #IdeConfigurationManager, #IdeDiagnosticsManager, #IdeDebugManager,
 * #IdeDeviceManager, #IdeRuntimeManager, #IdeRunManager, #IdeSearchEngine,
 * #IdeSourceSnippetsManager, #IdeTestManager, #IdeProject, and #IdeVcs.
 *
 * ## Services
 *
 * If you need a long running service that has it's life-time synchronized to
 * the lifetime of the #IdeContext, you may want to use #IdeService. It allows
 * a simple addin interface to provide long-running services to your plugin.
 *
 * ## Unloading the project context
 *
 * The context can be unloaded with ide_context_unload_async(), which should
 * generally only be called by the #IdeWorkbench when closing the project. If
 * you want to prevent unloading of the context during an operation, use the
 * ide_context_hold() and ide_context_release() functions to prevent
 * pre-mature unloading.
 *
 * Since: 3.18
 */

#define RESTORE_FILES_MAX_FILES 20

struct _IdeContext
{
  GObject                   parent_instance;

  IdeBufferManager         *buffer_manager;
  IdeBuildManager          *build_manager;
  IdeBuildSystem           *build_system;
  gchar                    *build_system_hint;
  IdeConfigurationManager  *configuration_manager;
  IdeDebugManager          *debug_manager;
  IdeDiagnosticsManager    *diagnostics_manager;
  IdeDeviceManager         *device_manager;
  IdeDoap                  *doap;
  IdeDocumentation         *documentation;
  GListStore               *pausables;
  IdeVcsMonitor            *monitor;
  GtkRecentManager         *recent_manager;
  IdeRunManager            *run_manager;
  IdeRuntimeManager        *runtime_manager;
  IdeToolchainManager      *toolchain_manager;
  IdeSearchEngine          *search_engine;
  IdeSourceSnippetsManager *snippets_manager;
  IdeTestManager           *test_manager;
  IdeProject               *project;
  GFile                    *project_file;
  GFile                    *downloads_dir;
  GFile                    *home_dir;
  gchar                    *recent_projects_path;
  PeasExtensionSet         *services;
  GHashTable               *services_by_gtype;
  IdeUnsavedFiles          *unsaved_files;
  IdeVcs                   *vcs;

  IdeBuildLog              *log;
  guint                     log_id;

  GMutex                    unload_mutex;
  gint                      hold_count;

  IdeTask                  *delayed_unload_task;

  guint                     restored : 1;
  guint                     restoring : 1;
  guint                     unloading : 1;
};

static void async_initable_init (GAsyncInitableIface *);

G_DEFINE_TYPE_EXTENDED (IdeContext, ide_context, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, async_initable_init))

DZL_DEFINE_COUNTER (instances, "Context", "N contexts", "Number of contexts")

enum {
  PROP_0,
  PROP_BUFFER_MANAGER,
  PROP_BUILD_SYSTEM,
  PROP_CONFIGURATION_MANAGER,
  PROP_DEVICE_MANAGER,
  PROP_DOCUMENTATION,
  PROP_PROJECT_FILE,
  PROP_PROJECT,
  PROP_RUNTIME_MANAGER,
  PROP_TOOLCHAIN_MANAGER,
  PROP_SEARCH_ENGINE,
  PROP_SNIPPETS_MANAGER,
  PROP_VCS,
  PROP_UNSAVED_FILES,
  LAST_PROP
};

enum {
  LOADED,
  LOG,
  LAST_SIGNAL
};

static GParamSpec *properties [LAST_PROP];
static guint signals [LAST_SIGNAL];

static void
ide_context_log_observer (IdeBuildLogStream  log_stream,
                          const gchar       *message,
                          gssize             message_len,
                          gpointer           user_data)
{
  IdeContext *self = user_data;
  GLogLevelFlags flags;
  IdeLineReader reader;
  const gchar *str;
  gsize len;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CONTEXT (self));
  g_assert (message_len >= 0);
  g_assert (message[message_len] == 0);

  flags = log_stream == IDE_BUILD_LOG_STDOUT ? G_LOG_LEVEL_MESSAGE
                                             : G_LOG_LEVEL_WARNING;

  ide_line_reader_init (&reader, (gchar *)message, message_len);

  while (NULL != (str = ide_line_reader_next (&reader, &len)))
    {
      g_autofree gchar *copy = NULL;

      /* Most of the time, we'll only have a single line, so we
       * don't need to copy the string to get a single line emitted
       * to the ::log signal.
       */

      if G_UNLIKELY (str[len] != '\0')
        str = copy = g_strndup (str, len);

      g_signal_emit (self, signals [LOG], 0, flags, str);
    }
}

/**
 * ide_context_get_recent_manager:
 *
 * Gets the IdeContext:recent-manager property. The recent manager is a GtkRecentManager instance
 * that should be used for the workbench.
 *
 * Returns: (transfer none): a #GtkRecentManager.
 */
GtkRecentManager *
ide_context_get_recent_manager (IdeContext *self)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  return self->recent_manager;
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
 * ide_context_get_build_manager:
 *
 * Returns: (transfer none): An #IdeBuildManager.
 */
IdeBuildManager *
ide_context_get_build_manager (IdeContext *self)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  return self->build_manager;
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
 * ide_context_get_documentation:
 * @self: An #IdeContext.
 *
 * Returns the #IdeDocumentation for the source view if there is one.
 *
 * Returns: (transfer none) (nullable): an #IdeDocumentation or %NULL.
 */
IdeDocumentation *
ide_context_get_documentation (IdeContext *self)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  return self->documentation;
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
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GObject) ret = NULL;

  g_return_if_fail (G_IS_ASYNC_INITABLE (initable));
  g_return_if_fail (IDE_IS_TASK (task));

  ret = g_async_initable_new_finish (initable, result, &error);

  if (ret == NULL)
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task, g_steal_pointer (&ret), g_object_unref);
}

void
ide_context_new_async (GFile               *project_file,
                       GCancellable        *cancellable,
                       GAsyncReadyCallback  callback,
                       gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_return_if_fail (G_IS_FILE (project_file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (NULL, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_context_new_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  g_async_initable_new_async (IDE_TYPE_CONTEXT,
                              G_PRIORITY_DEFAULT,
                              cancellable,
                              ide_context_new_cb,
                              g_steal_pointer (&task),
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
  IdeTask *task = (IdeTask *)result;
  IdeContext *ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_TASK (task), NULL);

  ret = ide_task_propagate_pointer (task, error);

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
 * Returns: (transfer none): a #GFile.
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
 * @service_type: a #GType of the service desired.
 *
 * Retrieves a service matching @service_type. If no match was found, a type
 * implementing the requested service type will be returned. If no matching
 * service type could be found, then an instance of the service will be
 * created, started, and returned.
 *
 * Returns: (type Ide.Service) (transfer none) (nullable): An #IdeService or %NULL.
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
      if (G_TYPE_CHECK_INSTANCE_TYPE (value, service_type))
        return value;
    }

  if (G_TYPE_IS_INSTANTIATABLE (service_type))
    {
      service = g_object_new (service_type, "context", self, NULL);
      g_hash_table_insert (self->services_by_gtype, GSIZE_TO_POINTER (service_type), service);
    }

  return service;
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
ide_context_real_log (IdeContext     *self,
                      GLogLevelFlags  log_level,
                      const gchar    *str)
{
  g_log ("Ide", log_level, "%s", str);
}

static void
ide_context_dispose (GObject *object)
{
  IdeContext *self = (IdeContext *)object;

  IDE_ENTRY;

  if (self->log_id != 0)
    {
      guint log_id = self->log_id;
      self->log_id = 0;
      ide_build_log_remove_observer (self->log, log_id);
    }

  g_list_store_remove_all (self->pausables);
  g_object_run_dispose (G_OBJECT (self->monitor));

  G_OBJECT_CLASS (ide_context_parent_class)->dispose (object);

  IDE_EXIT;
}

static void
ide_context_finalize (GObject *object)
{
  IdeContext *self = (IdeContext *)object;

  IDE_ENTRY;

  g_clear_object (&self->log);
  g_clear_object (&self->services);
  g_clear_object (&self->pausables);
  g_clear_object (&self->monitor);
  g_clear_object (&self->downloads_dir);
  g_clear_object (&self->home_dir);

  g_clear_pointer (&self->build_system_hint, g_free);
  g_clear_pointer (&self->services_by_gtype, g_hash_table_unref);
  g_clear_pointer (&self->recent_projects_path, g_free);

  g_clear_object (&self->build_system);
  g_clear_object (&self->configuration_manager);
  g_clear_object (&self->debug_manager);
  g_clear_object (&self->device_manager);
  g_clear_object (&self->doap);
  g_clear_object (&self->project);
  g_clear_object (&self->project_file);
  g_clear_object (&self->recent_manager);
  g_clear_object (&self->runtime_manager);
  g_clear_object (&self->toolchain_manager);
  g_clear_object (&self->test_manager);
  g_clear_object (&self->unsaved_files);
  g_clear_object (&self->vcs);

  g_mutex_clear (&self->unload_mutex);

  G_OBJECT_CLASS (ide_context_parent_class)->finalize (object);

  DZL_COUNTER_DEC (instances);

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

    case PROP_DOCUMENTATION:
      g_value_set_object (value, ide_context_get_documentation (self));
      break;

    case PROP_PROJECT:
      g_value_set_object (value, ide_context_get_project (self));
      break;

    case PROP_PROJECT_FILE:
      g_value_set_object (value, ide_context_get_project_file (self));
      break;

    case PROP_RUNTIME_MANAGER:
      g_value_set_object (value, ide_context_get_runtime_manager (self));
      break;

    case PROP_TOOLCHAIN_MANAGER:
      g_value_set_object (value, ide_context_get_toolchain_manager (self));
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

  properties [PROP_DOCUMENTATION] =
    g_param_spec_object ("documentation",
                         "Documentation",
                         "The documentation for the context.",
                         IDE_TYPE_DOCUMENTATION,
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

  properties [PROP_RUNTIME_MANAGER] =
    g_param_spec_object ("runtime-manager",
                         "Runtime Manager",
                         "Runtime Manager",
                         IDE_TYPE_RUNTIME_MANAGER,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_TOOLCHAIN_MANAGER] =
    g_param_spec_object ("toolchain-manager",
                         "Toolchain Manager",
                         "Toolchain Manager",
                         IDE_TYPE_TOOLCHAIN_MANAGER,
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

  /**
   * IdeContext::log:
   * @self: an #IdeContext
   * @log_level: the #GLogLevelFlags
   * @message: the log message
   *
   * The "log" signal is emitted when ide_context_warning()
   * or other log messages are sent.
   *
   * Since: 3.28
   */
  signals [LOG] =
    g_signal_new_class_handler ("log",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (ide_context_real_log),
                                NULL, NULL, NULL,
                                G_TYPE_NONE,
                                2,
                                G_TYPE_UINT,
                                G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);
}

static void
ide_context_init (IdeContext *self)
{
  const gchar *downloads_dir;

  IDE_ENTRY;

  DZL_COUNTER_INC (instances);

  g_mutex_init (&self->unload_mutex);

  /* Create log for internal context-based logging */
  self->log = ide_build_log_new ();
  self->log_id = ide_build_log_add_observer (self->log,
                                             ide_context_log_observer,
                                             self, NULL);

  /* Cache some paths for future lookups */
  downloads_dir = g_get_user_special_dir (G_USER_DIRECTORY_DOWNLOAD);
  if (downloads_dir != NULL)
    self->downloads_dir = g_file_new_for_path (downloads_dir);
  self->home_dir = g_file_new_for_path (g_get_home_dir ());

  self->pausables = g_list_store_new (IDE_TYPE_PAUSABLE);

  self->recent_manager = g_object_ref (gtk_recent_manager_get_default ());

  self->recent_projects_path = g_build_filename (g_get_user_data_dir (),
                                                 ide_get_program_name (),
                                                 IDE_RECENT_PROJECTS_BOOKMARK_FILENAME,
                                                 NULL);

  self->buffer_manager = g_object_new (IDE_TYPE_BUFFER_MANAGER,
                                       "context", self,
                                       NULL);

  self->build_manager = g_object_new (IDE_TYPE_BUILD_MANAGER,
                                      "context", self,
                                      NULL);

  self->debug_manager = g_object_new (IDE_TYPE_DEBUG_MANAGER,
                                      "context", self,
                                      NULL);

  self->diagnostics_manager = g_object_new (IDE_TYPE_DIAGNOSTICS_MANAGER,
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

  self->run_manager = g_object_new (IDE_TYPE_RUN_MANAGER,
                                    "context", self,
                                    NULL);

  self->runtime_manager = g_object_new (IDE_TYPE_RUNTIME_MANAGER,
                                        "context", self,
                                        NULL);

  self->toolchain_manager = g_object_new (IDE_TYPE_TOOLCHAIN_MANAGER,
                                          "context", self,
                                          NULL);

  self->test_manager = g_object_new (IDE_TYPE_TEST_MANAGER,
                                     "context", self,
                                     NULL);

  self->unsaved_files = g_object_new (IDE_TYPE_UNSAVED_FILES,
                                      "context", self,
                                      NULL);

  self->snippets_manager = g_object_new (IDE_TYPE_SOURCE_SNIPPETS_MANAGER, NULL);

  IDE_EXIT;
}

static void
ide_context_load_doap_worker (IdeTask      *task,
                              gpointer      source_object,
                              gpointer      task_data,
                              GCancellable *cancellable)
{
  IdeContext *self = source_object;
  g_autofree gchar *name = NULL;
  g_autoptr(GFile) directory = NULL;
  g_autoptr(GFileEnumerator) enumerator = NULL;

  g_assert (IDE_IS_TASK (task));
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

          if (!dzl_str_empty0 (filename) && g_str_has_suffix (filename, ".doap"))
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

  ide_task_return_boolean (task, TRUE);
}

static void
ide_context_init_project_name (gpointer             source_object,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  IdeContext *self = source_object;
  g_autoptr(IdeTask) task = NULL;

  g_return_if_fail (IDE_IS_CONTEXT (self));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_run_in_thread (task, ide_context_load_doap_worker);
}

static void
ide_context_init_vcs_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  IdeContext *self;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(IdeVcs) vcs = NULL;
  g_autoptr(GError) error = NULL;

  g_return_if_fail (G_IS_ASYNC_RESULT (result));
  g_return_if_fail (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);

  if (!(vcs = ide_vcs_new_finish (result, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  self->vcs = g_object_ref (vcs);

  ide_task_return_boolean (task, TRUE);
}

static void
ide_context_init_vcs (gpointer             source_object,
                      GCancellable        *cancellable,
                      GAsyncReadyCallback  callback,
                      gpointer             user_data)
{
  IdeContext *context = source_object;
  g_autoptr(IdeTask) task = NULL;

  g_return_if_fail (IDE_IS_CONTEXT (context));

  task = ide_task_new (source_object, cancellable, callback, user_data);

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
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GFile) project_file = NULL;
  g_autoptr(GError) error = NULL;
  IdeContext *self;

  IDE_ENTRY;

  g_assert (IDE_IS_TASK (task));
  g_assert (G_IS_ASYNC_RESULT (result));

  self = ide_task_get_source_object (task);

  g_assert (IDE_IS_CONTEXT (self));

  if (NULL == (build_system = ide_build_system_new_finish (result, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  self->build_system = g_object_ref (build_system);

  /* allow the build system to override the project file */
  g_object_get (self->build_system,
                "project-file", &project_file,
                NULL);
  if (project_file != NULL)
    ide_context_set_project_file (self, project_file);

  ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_context_init_build_system (gpointer             source_object,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  IdeContext *self = source_object;
  g_autoptr(IdeTask) task = NULL;

  g_return_if_fail (IDE_IS_CONTEXT (self));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_context_init_build_system);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  ide_build_system_new_async (self,
                              self->project_file,
                              self->build_system_hint,
                              cancellable,
                              ide_context_init_build_system_cb,
                              g_steal_pointer (&task));
}

static void
ide_context_init_runtimes (gpointer             source_object,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  IdeContext *self = source_object;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;

  g_return_if_fail (IDE_IS_CONTEXT (self));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_context_init_runtimes);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  if (!g_initable_init (G_INITABLE (self->runtime_manager), cancellable, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);
}

static void
ide_context_init_toolchain_manager_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  GAsyncInitable *initable = (GAsyncInitable *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_ASYNC_INITABLE (initable));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!g_async_initable_init_finish (initable, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);
}

static void
ide_context_init_toolchain_manager (gpointer             source_object,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  IdeContext *self = source_object;
  g_autoptr(IdeTask) task = NULL;

  g_assert (IDE_IS_CONTEXT (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  g_async_initable_init_async (G_ASYNC_INITABLE (self->toolchain_manager),
                               G_PRIORITY_DEFAULT,
                               cancellable,
                               ide_context_init_toolchain_manager_cb,
                               g_object_ref (task));
}

static void
ide_context_reap_unsaved_files_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  IdeUnsavedFiles *unsaved_files = (IdeUnsavedFiles *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_UNSAVED_FILES (unsaved_files));

  if (!ide_unsaved_files_reap_finish (unsaved_files, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);
}

static void
ide_context_reap_unsaved_files (gpointer             source_object,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  IdeContext *self = source_object;
  g_autoptr(IdeTask) task = NULL;

  g_return_if_fail (IDE_IS_CONTEXT (self));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_context_reap_unsaved_files);
  ide_unsaved_files_reap_async (self->unsaved_files,
                                cancellable,
                                ide_context_reap_unsaved_files_cb,
				g_steal_pointer (&task));
}

static void
ide_context_init_unsaved_files_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  IdeUnsavedFiles *unsaved_files = (IdeUnsavedFiles *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_UNSAVED_FILES (unsaved_files));

  if (!ide_unsaved_files_restore_finish (unsaved_files, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);
}

static void
ide_context_init_unsaved_files (gpointer             source_object,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  IdeContext *self = source_object;
  g_autoptr(IdeTask) task = NULL;

  g_return_if_fail (IDE_IS_CONTEXT (self));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_context_init_unsaved_files);
  ide_unsaved_files_restore_async (self->unsaved_files,
                                   cancellable,
                                   ide_context_init_unsaved_files_cb,
				   g_steal_pointer (&task));
}

static void
ide_context_init_snippets_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  IdeSourceSnippetsManager *manager = (IdeSourceSnippetsManager *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_return_if_fail (IDE_IS_SOURCE_SNIPPETS_MANAGER (manager));

  if (!ide_source_snippets_manager_load_finish (manager, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);
}

static void
ide_context_init_snippets (gpointer             source_object,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  IdeContext *self = source_object;
  g_autoptr(IdeTask) task = NULL;

  g_return_if_fail (IDE_IS_CONTEXT (self));

  task = ide_task_new (self, cancellable, callback, user_data);

  ide_source_snippets_manager_load_async (self->snippets_manager,
                                          cancellable,
                                          ide_context_init_snippets_cb,
                                          g_object_ref (task));
}

static void
ide_context_init_tests (gpointer             source_object,
                        GCancellable        *cancellable,
                        GAsyncReadyCallback  callback,
                        gpointer             user_data)
{
  IdeContext *self = source_object;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;

  g_return_if_fail (IDE_IS_CONTEXT (self));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_source_tag (task, ide_context_init_tests);

  if (!g_initable_init (G_INITABLE (self->test_manager), cancellable, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);
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

  if (!g_hash_table_contains (self->services_by_gtype, GSIZE_TO_POINTER (G_OBJECT_TYPE (exten))))
    {
      g_hash_table_insert (self->services_by_gtype,
                           GSIZE_TO_POINTER (G_OBJECT_TYPE (exten)),
                           exten);
      ide_service_start (IDE_SERVICE (exten));
    }
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
  g_autoptr(IdeTask) task = NULL;

  g_return_if_fail (IDE_IS_CONTEXT (self));

  task = ide_task_new (self, cancellable, callback, user_data);

  self->services_by_gtype = g_hash_table_new (NULL, NULL);
  self->services = ide_extension_set_new (peas_engine_get_default (),
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

  ide_task_return_boolean (task, TRUE);
}

static gboolean
directory_is_ignored (IdeContext *self,
                      GFile      *file)
{
  g_autofree gchar *relative_path = NULL;
  GFileType file_type;

  g_assert (IDE_IS_CONTEXT (self));
  g_assert (G_IS_FILE (file));

  relative_path = g_file_get_relative_path (self->home_dir, file);
  file_type = g_file_query_file_type (file,
                                      G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                      NULL);

  if (!g_file_has_prefix (file, self->home_dir))
    return TRUE;

  if (self->downloads_dir != NULL &&
      (g_file_equal (file, self->downloads_dir) ||
       g_file_has_prefix (file, self->downloads_dir)))
    return TRUE;

  /* realtive_path should be valid here because we are within the home_prefix. */
  g_assert (relative_path != NULL);

  /*
   * Ignore dot directories, except .local.
   * We've had too many bug reports with people creating things
   * like gnome-shell extensions in their .local directory.
   */
  if (relative_path[0] == '.' &&
      !g_str_has_prefix (relative_path, ".local"G_DIR_SEPARATOR_S))
    return TRUE;

  if (file_type != G_FILE_TYPE_DIRECTORY)
    {
      g_autoptr(GFile) parent = g_file_get_parent (file);

      if (g_file_equal (self->home_dir, parent))
        return TRUE;
    }

  return FALSE;
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
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *uri = NULL;
  g_autofree gchar *app_exec = NULL;
  g_autofree gchar *dir = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_CONTEXT (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_context_init_add_recent);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  if (directory_is_ignored (self, self->project_file))
    {
      ide_task_return_boolean (task, TRUE);
      return;
    }

  projects_file = g_bookmark_file_new ();
  g_bookmark_file_load_from_file (projects_file,
                                  self->recent_projects_path,
                                  &error);

  /*
   * If there was an error loading the file and the error is not "File does not
   * exist" then stop saving operation
   */
  if (error != NULL &&
      !g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
    {
      ide_context_warning (self,
                           "Unable to open recent projects \"%s\" file: %s",
                           self->recent_projects_path, error->message);
      ide_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

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

  {
    IdeBuildSystem *build_system;
    g_autofree gchar *build_system_name = NULL;
    g_autofree gchar *build_system_group = NULL;

    build_system = ide_context_get_build_system (self);
    build_system_name = ide_build_system_get_display_name (build_system);
    build_system_group = g_strdup_printf ("%s%s", IDE_RECENT_PROJECTS_BUILD_SYSTEM_GROUP_PREFIX, build_system_name);
    g_bookmark_file_add_group (projects_file, uri, build_system_group);
  }

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

  ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_context_init_search_engine (gpointer             source_object,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  IdeContext *self = source_object;

  g_assert (IDE_IS_CONTEXT (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  self->search_engine = g_object_new (IDE_TYPE_SEARCH_ENGINE,
                                      "context", self,
                                      NULL);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_return_boolean (task, TRUE);
}

static void
ide_context_init_documentation (gpointer             source_object,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  IdeContext *self = source_object;

  g_assert (IDE_IS_CONTEXT (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  self->documentation = g_object_new (IDE_TYPE_DOCUMENTATION,
                                     "context", self,
                                      NULL);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_return_boolean (task, TRUE);
}

static void
ide_context_init_configuration_manager_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  GAsyncInitable *initable = (GAsyncInitable *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_ASYNC_INITABLE (initable));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!g_async_initable_init_finish (initable, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);
}

static void
ide_context_init_configuration_manager (gpointer             source_object,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  IdeContext *self = source_object;
  g_autoptr(IdeTask) task = NULL;

  g_assert (IDE_IS_CONTEXT (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  g_async_initable_init_async (G_ASYNC_INITABLE (self->configuration_manager),
                               G_PRIORITY_DEFAULT,
                               cancellable,
                               ide_context_init_configuration_manager_cb,
                               g_object_ref (task));
}

static void
ide_context_init_diagnostics_manager (gpointer             source_object,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = NULL;
  IdeContext *self = source_object;

  g_assert (IDE_IS_CONTEXT (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);

  if (!g_initable_init (G_INITABLE (self->diagnostics_manager), cancellable, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);
}

static void
ide_context_init_build_manager (gpointer             source_object,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = NULL;
  IdeContext *self = source_object;

  g_assert (IDE_IS_CONTEXT (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);

  if (!g_initable_init (G_INITABLE (self->build_manager), cancellable, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);
}

static void
ide_context_init_run_manager (gpointer             source_object,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = NULL;
  IdeContext *self = source_object;

  g_assert (IDE_IS_CONTEXT (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);

  if (!g_initable_init (G_INITABLE (self->run_manager), cancellable, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);
}

static void
ide_context_init_loaded (gpointer             source_object,
                         GCancellable        *cancellable,
                         GAsyncReadyCallback  callback,
                         gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  IdeContext *self = source_object;

  g_assert (IDE_IS_CONTEXT (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_signal_emit (self, signals [LOADED], 0);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_return_boolean (task, TRUE);
}

static void
ide_context_init_early_discover_cb (PeasExtensionSet *set,
                                    PeasPluginInfo   *plugin_info,
                                    PeasExtension    *exten,
                                    gpointer          user_data)
{
  IdeBuildSystemDiscovery *discovery = (IdeBuildSystemDiscovery *)exten;
  g_autofree gchar *ret = NULL;
  struct {
    GFile *project_file;
    gchar *hint;
    gint   priority;
  } *state = user_data;
  gint priority = 0;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_BUILD_SYSTEM_DISCOVERY (exten));
  g_assert (state != NULL);

  ret = ide_build_system_discovery_discover (discovery, state->project_file, NULL, &priority, NULL);

  if (ret != NULL && (priority < state->priority || state->hint == NULL))
    {
      g_free (state->hint);
      state->hint = g_steal_pointer (&ret);
      state->priority = priority;
    }
}

static void
ide_context_init_early_discovery_worker (IdeTask      *task,
                                         gpointer      source_object,
                                         gpointer      task_data,
                                         GCancellable *cancellable)
{
  IdeContext *self = source_object;
  g_autoptr(PeasExtensionSet) addins = NULL;
  g_autoptr(GFile) parent = NULL;
  GFile *project_file = task_data;
  struct {
    GFile *project_file;
    gchar *hint;
    gint   priority;
  } state;

  g_assert (IDE_IS_TASK (task));
  g_assert (IDE_IS_CONTEXT (self));
  g_assert (G_IS_FILE (project_file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  /*
   * Before we load our build system by working through the extension
   * points, try to discover if there is a specific build system we
   * should be using.
   */

  /*
   * If the project file is not a directory, we want the parent so that the the
   * discovery layer can potentially change which build system should be loaded.
   */
  if (g_file_query_file_type (project_file, 0, cancellable) != G_FILE_TYPE_DIRECTORY)
    {
      parent = g_file_get_parent (project_file);
      project_file = parent;
    }

  state.project_file = project_file;
  state.hint = NULL;
  state.priority = G_MAXINT;

  /*
   * Ask our plugins to try to discover what build system to use before loading
   * build system extension points. This allows things like flatpak configuration
   * files to influence what build system to load.
   */
  addins = peas_extension_set_new (peas_engine_get_default (),
                                   IDE_TYPE_BUILD_SYSTEM_DISCOVERY,
                                   NULL);

  peas_extension_set_foreach (addins, ide_context_init_early_discover_cb, &state);

  /*
   * If we discovered the build system to use based on the hint, we need to stash
   * that info so the ide_context_init_build_system() function can use it to narrow
   * the build systems to try.
   */
  if (state.hint != NULL)
    {
      IDE_TRACE_MSG ("Discovered that %s is the build system to load", state.hint);

      self->build_system_hint = g_steal_pointer (&state.hint);

      /*
       * We might need to take the parent as the new project file, so
       * that the build system can load the proper file.
       */
      g_set_object (&self->project_file, project_file);
    }

  ide_task_return_boolean (task, TRUE);
}

static void
ide_context_init_early_discovery (gpointer             source_object,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  IdeContext *self = source_object;
  g_autoptr(IdeTask) task = NULL;

  g_assert (IDE_IS_CONTEXT (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_context_init_early_discovery);
  ide_task_set_task_data (task, g_object_ref (self->project_file), g_object_unref);
  ide_task_run_in_thread (task, ide_context_init_early_discovery_worker);
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
                        ide_context_init_early_discovery,
                        ide_context_init_build_system,
                        ide_context_init_vcs,
                        ide_context_init_services,
                        ide_context_init_project_name,
                        ide_context_init_snippets,
                        ide_context_reap_unsaved_files,
                        ide_context_init_unsaved_files,
                        ide_context_init_add_recent,
                        ide_context_init_search_engine,
                        ide_context_init_documentation,
                        ide_context_init_runtimes,
                        ide_context_init_toolchain_manager,
                        ide_context_init_configuration_manager,
                        ide_context_init_build_manager,
                        ide_context_init_run_manager,
                        ide_context_init_diagnostics_manager,
                        ide_context_init_tests,
                        ide_context_init_loaded,
                        NULL);
}

static gboolean
ide_context_init_finish (GAsyncInitable  *initable,
                         GAsyncResult    *result,
                         GError         **error)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (initable), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
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
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  gint in_progress;

  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));
  g_assert (IDE_IS_TASK (task));

  in_progress = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (task), "IN_PROGRESS"));
  g_assert (in_progress > 0);
  in_progress--;
  g_object_set_data (G_OBJECT (task), "IN_PROGRESS", GINT_TO_POINTER (in_progress));

  if (!ide_buffer_manager_save_file_finish (buffer_manager, result, &error))
    g_warning ("%s", error->message);

  if (in_progress == 0)
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_context_unload_buffer_manager (gpointer             source_object,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  IdeContext *self = source_object;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GPtrArray) buffers = NULL;
  gsize i;
  guint skipped = 0;

  IDE_ENTRY;

  g_assert (IDE_IS_CONTEXT (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  buffers = ide_buffer_manager_get_buffers (self->buffer_manager);

  IDE_PTR_ARRAY_SET_FREE_FUNC (buffers, g_object_unref);

  task = ide_task_new (self, cancellable, callback, user_data);

  if (buffers->len == 0)
    {
      ide_task_return_boolean (task, TRUE);
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
        ide_task_return_boolean (task, TRUE);
    }

  IDE_EXIT;
}

static void
ide_context_unload__configuration_manager_save_cb (GObject      *object,
                                                   GAsyncResult *result,
                                                   gpointer      user_data)
{
  IdeConfigurationManager *manager = (IdeConfigurationManager *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_CONFIGURATION_MANAGER (manager));
  g_assert (IDE_IS_TASK (task));

  /* unfortunate if this happens, but not much we can do */
  if (!ide_configuration_manager_save_finish (manager, result, &error))
    g_warning ("%s", error->message);

  ide_task_return_boolean (task, TRUE);
}

static void
ide_context_unload_configuration_manager (gpointer             source_object,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data)
{
  IdeContext *self = source_object;
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_CONTEXT (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (IDE_IS_CONFIGURATION_MANAGER (self->configuration_manager));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_context_unload_configuration_manager);

  ide_configuration_manager_save_async (self->configuration_manager,
                                        cancellable,
                                        ide_context_unload__configuration_manager_save_cb,
                                        g_steal_pointer (&task));

  IDE_EXIT;
}

static void
ide_context_unload__unsaved_files_save_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  IdeUnsavedFiles *unsaved_files = (IdeUnsavedFiles *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_UNSAVED_FILES (unsaved_files));
  g_assert (IDE_IS_TASK (task));

  /* nice to know, but not critical to rest of shutdown */
  if (!ide_unsaved_files_save_finish (unsaved_files, result, &error))
    g_warning ("%s", error->message);

  ide_task_return_boolean (task, TRUE);
}

static void
ide_context_unload_unsaved_files (gpointer             source_object,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  IdeContext *self = source_object;
  g_autoptr(IdeTask) task = NULL;

  g_assert (IDE_IS_CONTEXT (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);

  ide_unsaved_files_save_async (self->unsaved_files,
                                cancellable,
                                ide_context_unload__unsaved_files_save_cb,
                                g_object_ref (task));
}

static void
ide_context_unload_services (gpointer             source_object,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  IdeContext *self = source_object;
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_CONTEXT (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_clear_object (&self->services);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_context_unload_cb (GObject      *object,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  IdeContext *self = (IdeContext *)object;
  IdeTask *unload_task = (IdeTask *)result;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_CONTEXT (self));
  g_assert (IDE_IS_TASK (unload_task));
  g_assert (IDE_IS_TASK (task));

  g_clear_object (&self->device_manager);
  g_clear_object (&self->runtime_manager);
  g_clear_object (&self->toolchain_manager);

  if (!ide_task_propagate_boolean (unload_task, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);
}

static void
ide_context_do_unload_locked (IdeContext *self)
{
  g_autoptr(IdeTask) task = NULL;

  g_assert (IDE_IS_CONTEXT (self));
  g_assert (self->delayed_unload_task != NULL);

  task = self->delayed_unload_task;
  self->delayed_unload_task = NULL;

  ide_async_helper_run (self,
                        ide_task_get_cancellable (task),
                        ide_context_unload_cb,
                        g_object_ref (task),
                        ide_context_unload_configuration_manager,
                        ide_context_unload_buffer_manager,
                        ide_context_unload_unsaved_files,
                        ide_context_unload_services,
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
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_CONTEXT (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  self->unloading = TRUE;

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_context_unload_async);

  g_mutex_lock (&self->unload_mutex);

  if (self->delayed_unload_task != NULL)
    {
      ide_task_return_new_error (task,
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
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_CONTEXT (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static gboolean restore_in_idle (gpointer user_data);

static void
ide_context_restore__load_file_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  IdeBufferManager *buffer_manager = (IdeBufferManager *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(IdeBuffer) buffer = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));
  g_assert (IDE_IS_TASK (task));

  if (!(buffer = ide_buffer_manager_load_file_finish (buffer_manager, result, &error)))
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
  g_autoptr(IdeTask) task = user_data;
  IdeUnsavedFile *uf;
  IdeContext *self;
  GPtrArray *ar;
  GFile *file;

  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  g_assert (IDE_IS_CONTEXT (self));

  ar = ide_task_get_task_data (task);

  if (ar == NULL || ar->len == 0)
    {
      self->restoring = FALSE;
      ide_task_return_boolean (task, TRUE);
      return G_SOURCE_REMOVE;
    }

  g_assert (ar != NULL);
  g_assert (ar->len > 0);

  uf = g_ptr_array_index (ar, ar->len - 1);
  file = ide_unsaved_file_get_file (uf);
  ifile = ide_file_new (self, file);
  g_ptr_array_remove_index (ar, ar->len - 1);

  ide_buffer_manager_load_file_async (self->buffer_manager,
                                      ifile,
                                      FALSE,
                                      IDE_WORKBENCH_OPEN_FLAGS_BACKGROUND,
                                      NULL,
                                      ide_task_get_cancellable (task),
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
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GPtrArray) ar = NULL;

  g_return_if_fail (IDE_IS_CONTEXT (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);

  if (self->restored)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 _("Context has already been restored."));
      return;
    }

  self->restored = TRUE;

  ar = ide_unsaved_files_to_array (self->unsaved_files);
  IDE_PTR_ARRAY_SET_FREE_FUNC (ar, ide_unsaved_file_unref);

  if (ar->len == 0)
    {
      ide_task_return_boolean (task, TRUE);
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
      ide_task_return_boolean (task, TRUE);
      return;
    }

  self->restoring = TRUE;

  ide_task_set_task_data (task, g_ptr_array_ref (ar), (GDestroyNotify)g_ptr_array_unref);

  g_idle_add (restore_in_idle, g_object_ref (task));
}

gboolean
ide_context_restore_finish (IdeContext    *self,
                            GAsyncResult  *result,
                            GError       **error)
{
  IdeTask *task = (IdeTask *)result;

  g_return_val_if_fail (IDE_IS_CONTEXT (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (task), FALSE);

  return ide_task_propagate_boolean (task, error);
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
 * @instance: (type GObject.Object): a #GObject instance
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
 * ide_context_get_toolchain_manager:
 * @self: An #IdeContext
 *
 * Gets the #IdeToolchainManager for the LibIDE context.
 *
 * The toolchain manager provies access to #IdeToolchain instances via the
 * #GListModel interface. These can provide support for building projects
 * using different specified toolchains.
 *
 * Returns: (transfer none): An #IdeToolchainManager.
 */
IdeToolchainManager *
ide_context_get_toolchain_manager (IdeContext *self)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  return self->toolchain_manager;
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

/**
 * ide_context_emit_log:
 * @self: a #IdeContext
 * @log_level: a #GLogLevelFlags
 * @message: the log message
 * @message_len: the length of @message, not including a %NULL byte, or -1
 *   to indicate the message is %NULL terminated.
 *
 * Emits the #IdeContext::log signal, possibly after sending the message to
 * the main loop.
 *
 * Thread-safety: you may call this from any thread that holds a reference to
 *   the #IdeContext object.
 */
void
ide_context_emit_log (IdeContext     *self,
                      GLogLevelFlags  log_level,
                      const gchar    *message,
                      gssize          message_len)
{
  g_return_if_fail (IDE_IS_CONTEXT (self));

  /*
   * This may be called from a thread, so we proxy the message
   * to the main thread using IdeBuildLog.
   */

  if (self->log != NULL)
    ide_build_log_observer (log_level & (G_LOG_LEVEL_WARNING | G_LOG_LEVEL_ERROR) ?
                            IDE_BUILD_LOG_STDERR : IDE_BUILD_LOG_STDOUT,
                            message,
                            message_len,
                            self->log);
}

/**
 * ide_context_message:
 * @self: a #IdeContext
 * @format: a printf style format
 * @...: parameters for @format
 *
 * Emits a log message for the context, which is useful so that
 * messages may be displayed to the user in the workbench window.
 *
 * Thread-safety: you may call this from any thread, so long as the thread
 *   owns a reference to the context.
 *
 * Since: 3.28
 */
void
ide_context_message (IdeContext  *self,
                     const gchar *format,
                     ...)
{
  g_return_if_fail (IDE_IS_CONTEXT (self));
  g_return_if_fail (format != NULL);

  /*
   * This may be called from a thread, so we proxy the message
   * to the main thread using IdeBuildLog.
   */

  if (self->log != NULL)
    {
      g_autofree gchar *str = NULL;
      va_list args;

      va_start (args, format);
      str = g_strdup_vprintf (format, args);
      va_end (args);

      ide_context_emit_log (self, G_LOG_LEVEL_MESSAGE, str, -1);
    }
}

/**
 * ide_context_warning:
 * @self: a #IdeContext
 * @format: a printf style format
 * @...: parameters for @format
 *
 * Emits a log message for the context, which is useful so that error
 * messages may be displayed to the user in the workbench window.
 *
 * Thread-safety: you may call this from any thread, so long as the thread
 *   owns a reference to the context.
 */
void
ide_context_warning (IdeContext  *self,
                     const gchar *format,
                     ...)
{
  g_return_if_fail (IDE_IS_CONTEXT (self));
  g_return_if_fail (format != NULL);

  /*
   * This may be called from a thread, so we proxy the message
   * to the main thread using IdeBuildLog.
   */

  if (self->log != NULL)
    {
      g_autofree gchar *str = NULL;
      va_list args;

      va_start (args, format);
      str = g_strdup_vprintf (format, args);
      va_end (args);

      ide_context_emit_log (self, G_LOG_LEVEL_WARNING, str, -1);
    }
}

/**
 * ide_context_get_run_manager:
 *
 * Gets the #IdeRunManager for the context. This manager object simplifies
 * the process of running an #IdeBuildTarget from the build system. Primarily,
 * it enforces that only a single target may be run at a time, since that is
 * what the UI will expect.
 *
 * Returns: (transfer none): An #IdeRunManager.
 */
IdeRunManager *
ide_context_get_run_manager (IdeContext *self)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  return self->run_manager;
}

/**
 * ide_context_get_diagnostics_manager:
 *
 * Gets the #IdeDiagnosticsManager for the context.
 *
 * Returns: (transfer none): An #IdeDiagnosticsManager.
 */
IdeDiagnosticsManager *
ide_context_get_diagnostics_manager (IdeContext *self)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  return self->diagnostics_manager;
}

/**
 * ide_context_get_debug_manager:
 * @self: An #IdeContext
 *
 * Gets the debug manager for the context.
 *
 * Returns: (transfer none): An #IdeDebugManager
 */
IdeDebugManager *
ide_context_get_debug_manager (IdeContext *self)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  return self->debug_manager;
}

/**
 * ide_context_get_test_manager:
 * @self: An #IdeTestManager
 *
 * Gets the test manager for the #IdeContext.
 *
 * Returns: (transfer none): An #IdeTestManager
 *
 * Since: 3.28
 */
IdeTestManager *
ide_context_get_test_manager (IdeContext *self)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  return self->test_manager;
}

/**
 * ide_context_add_pausable:
 * @self: an #IdeContext
 * @pausable: an #IdePausable
 *
 * Adds a pausable which can be used to associate pausable actions with the
 * context. Various UI in Builder may use this to present pausable actions to
 * the user.
 *
 * Since: 3.26
 */
void
ide_context_add_pausable (IdeContext  *self,
                          IdePausable *pausable)
{
  g_return_if_fail (IDE_IS_CONTEXT (self));
  g_return_if_fail (IDE_IS_PAUSABLE (pausable));

  g_list_store_append (self->pausables, pausable);
}

/**
 * ide_context_remove_pausable:
 * @self: an #IdeContext
 * @pausable: an #IdePausable
 *
 * Remove a previously registered #IdePausable.
 *
 * Since: 3.26
 */
void
ide_context_remove_pausable (IdeContext  *self,
                             IdePausable *pausable)
{
  guint n_items;

  g_return_if_fail (IDE_IS_CONTEXT (self));
  g_return_if_fail (IDE_IS_PAUSABLE (pausable));

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->pausables));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(IdePausable) item = NULL;

      item = g_list_model_get_item (G_LIST_MODEL (self->pausables), i);

      if (item == pausable)
        {
          g_list_store_remove (self->pausables, i);
          break;
        }
    }
}

GListModel *
_ide_context_get_pausables (IdeContext *self)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  return G_LIST_MODEL (self->pausables);
}

/**
 * ide_context_cache_file:
 * @self: a #IdeContext
 * @first_part: The first part of the path
 *
 * Like ide_context_cache_filename() but returns a #GFile.
 *
 * Returns: (transfer full): a #GFile for the cache file
 *
 * Since: 3.28
 */
GFile *
ide_context_cache_file (IdeContext  *self,
                        const gchar *first_part,
                        ...)
{
  g_autoptr(GPtrArray) ar = NULL;
  g_autofree gchar *path = NULL;
  const gchar *project_id;
  const gchar *part = first_part;
  va_list args;

  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);
  g_return_val_if_fail (IDE_IS_PROJECT (self->project), NULL);
  g_return_val_if_fail (first_part != NULL, NULL);

  project_id = ide_project_get_id (self->project);
  g_return_val_if_fail (project_id != NULL, NULL);

  ar = g_ptr_array_new ();
  g_ptr_array_add (ar, (gchar *)g_get_user_cache_dir ());
  g_ptr_array_add (ar, (gchar *)ide_get_program_name ());
  g_ptr_array_add (ar, (gchar *)"projects");
  g_ptr_array_add (ar, (gchar *)project_id);

  va_start (args, first_part);
  do
    {
      g_ptr_array_add (ar, (gchar *)part);
      part = va_arg (args, const gchar *);
    }
  while (part != NULL);
  va_end (args);

  g_ptr_array_add (ar, NULL);

  path = g_build_filenamev ((gchar **)ar->pdata);

  return g_file_new_for_path (path);
}

/**
 * ide_context_cache_filename:
 * @self: a #IdeContext
 * @first_part: the first part of the filename
 *
 * Creates a new filename that will be located in the projects cache directory.
 * This makes it convenient to remove files when a project is deleted as all
 * cache files will share a unified parent directory.
 *
 * The file will be located in a directory similar to
 * ~/.cache/gnome-builder/project_name. This may change based on the value
 * of g_get_user_cache_dir().
 *
 * Returns: (transfer full): A new string containing the cache filename
 *
 * Since: 3.28
 */
gchar *
ide_context_cache_filename (IdeContext  *self,
                            const gchar *first_part,
                            ...)
{
  g_autoptr(GPtrArray) ar = NULL;
  const gchar *part = first_part;
  const gchar *project_id;
  va_list args;

  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);
  g_return_val_if_fail (IDE_IS_PROJECT (self->project), NULL);
  g_return_val_if_fail (first_part != NULL, NULL);

  project_id = ide_project_get_id (self->project);
  g_return_val_if_fail (project_id != NULL, NULL);

  ar = g_ptr_array_new ();
  g_ptr_array_add (ar, (gchar *)g_get_user_cache_dir ());
  g_ptr_array_add (ar, (gchar *)ide_get_program_name ());
  g_ptr_array_add (ar, (gchar *)"projects");
  g_ptr_array_add (ar, (gchar *)project_id);

  va_start (args, first_part);
  do
    {
      g_ptr_array_add (ar, (gchar *)part);
      part = va_arg (args, const gchar *);
    }
  while (part != NULL);
  va_end (args);

  g_ptr_array_add (ar, NULL);

  return g_build_filenamev ((gchar **)ar->pdata);
}

/**
 * ide_context_get_monitor:
 * @self: a #IdeContext
 *
 * Gets a #DzlRecursiveFileMonitor that monitors the project directory
 * recursively. You can use this to track changes across the project
 * tree without creating your own #GFileMonitor.
 *
 * Returns: (transfer none): a #IdeVcsMonitor to monitor the project tree.
 *
 * Since: 3.28
 */
IdeVcsMonitor *
ide_context_get_monitor (IdeContext *self)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  if (self->monitor == NULL)
    {
      g_autoptr(GFile) root = NULL;
      GFileType file_type;

      file_type = g_file_query_file_type (self->project_file, 0, NULL);

      if (file_type == G_FILE_TYPE_DIRECTORY)
        root = g_object_ref (self->project_file);
      else
        root = g_file_get_parent (self->project_file);

      self->monitor = g_object_new (IDE_TYPE_VCS_MONITOR,
                                    "context", self,
                                    "root", root,
                                    NULL);
    }

  return self->monitor;
}

/**
 * ide_context_build_filename:
 * @self: a #IdeContext
 * @first_part: first path part
 *
 * Creates a new path that starts from the working directory of the
 * loaded project.
 *
 * Returns: (transfer full): a string containing the new path
 *
 * Since: 3.28
 */
gchar *
ide_context_build_filename (IdeContext  *self,
                            const gchar *first_part,
                            ...)
{
  g_autoptr(GPtrArray) ar = NULL;
  g_autofree gchar *base = NULL;
  const gchar *part = first_part;
  GFile *workdir;
  va_list args;

  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);
  g_return_val_if_fail (first_part != NULL, NULL);

  workdir = ide_vcs_get_working_directory (self->vcs);
  base = g_file_get_path (workdir);

  ar = g_ptr_array_new ();
  g_ptr_array_add (ar, base);

  va_start (args, first_part);
  do
    {
      g_ptr_array_add (ar, (gchar *)part);
      part = va_arg (args, const gchar *);
    }
  while (part != NULL);
  va_end (args);

  g_ptr_array_add (ar, NULL);

  return g_build_filenamev ((gchar **)ar->pdata);
}

/**
 * ide_context_get_project_settings:
 * @self: a #IdeContext
 *
 * Gets an org.gnome.builder.project #GSettings.
 *
 * This creates a new #GSettings instance for the project.
 *
 * Returns: (transfer full): a #GSettings
 */
GSettings *
ide_context_get_project_settings (IdeContext *self)
{
  g_autofree gchar *path = NULL;
  const gchar *project_id;
  IdeProject *project;

  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  project = ide_context_get_project (self);
  project_id = ide_project_get_id (project);
  path = g_strdup_printf ("/org/gnome/builder/projects/%s/", project_id);

  return g_settings_new_with_path ("org.gnome.builder.project", path);
}

/**
 * ide_context_is_unloading:
 * @self: a #IdeContext
 *
 * Checks if ide_context_unload_async() has been called.
 *
 * You might use this to avoid starting any new work once the context has
 * started the shutdown sequence.
 *
 * Returns: %TRUE if ide_context_unload_async() has been called.
 *
 * Since: 3.28
 */
gboolean
ide_context_is_unloading (IdeContext *self)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (self), FALSE);

  return self->unloading;
}
