/* ide-autotools-build-system.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-autotools-build-system"

#include "config.h"

#include <gio/gio.h>
#include <gtksourceview/gtksource.h>
#include <string.h>

#include <libide-foundry.h>
#include <libide-vcs.h>

#include "ide-autotools-build-system.h"
#include "ide-autotools-makecache-stage.h"
#include "ide-makecache.h"

struct _IdeAutotoolsBuildSystem
{
  IdeObject  parent_instance;

  GFile     *project_file;
  gchar     *tarball_name;
};

static void async_initable_iface_init (GAsyncInitableIface *iface);
static void build_system_iface_init (IdeBuildSystemInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeAutotoolsBuildSystem,
                         ide_autotools_build_system,
                         IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, async_initable_iface_init)
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_SYSTEM, build_system_iface_init))

enum {
  PROP_0,
  PROP_PROJECT_FILE,
  PROP_TARBALL_NAME,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

const gchar *
ide_autotools_build_system_get_tarball_name (IdeAutotoolsBuildSystem *self)
{
  g_return_val_if_fail (IDE_IS_AUTOTOOLS_BUILD_SYSTEM (self), NULL);

  return self->tarball_name;
}

static gboolean
is_configure (GFile *file)
{
  g_autofree gchar *name = NULL;

  g_assert (G_IS_FILE (file));

  name = g_file_get_basename (file);
  return ide_str_equal0 (name, "configure.ac") ||
         ide_str_equal0 (name, "configure.in");
}

static gboolean
check_for_ac_init (GFile        *file,
                   GCancellable *cancellable)
{
  g_autofree gchar *contents = NULL;
  gsize len = 0;

  g_assert (G_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (g_file_load_contents (file, cancellable, &contents, &len, NULL, NULL))
    return strstr (contents, "AC_INIT") != NULL;

  return FALSE;
}

static void
ide_autotools_build_system_discover_file_worker (IdeTask      *task,
                                                 gpointer      source_object,
                                                 gpointer      task_data,
                                                 GCancellable *cancellable)
{
  g_autoptr(GFile) configure_ac = NULL;
  g_autoptr(GFile) configure_in = NULL;
  GFile *file = task_data;

  IDE_ENTRY;

  g_assert (IDE_IS_TASK (task));
  g_assert (G_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  /*
   * Previously, we used to walk up the tree looking for configure.ac. But
   * that causes more problems than it solves as it means that we cannot
   * handle test projects inside the Builder project (and other projects
   * may have the same issue for sub-projects).
   */

  if (is_configure (file) && g_file_query_exists (file, cancellable))
    {
      ide_task_return_pointer (task, g_object_ref (file), g_object_unref);
      IDE_EXIT;
    }

  /*
   * So this file is not the configure file, if it's not a directory,
   * we'll ignore this request and assume this isn't an autotools project.
   */
  if (g_file_query_file_type (file, 0, cancellable) != G_FILE_TYPE_DIRECTORY)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_FOUND,
                                 "Failed to locate configure.ac");
      IDE_EXIT;
    }

  configure_ac = g_file_get_child (file, "configure.ac");
  if (check_for_ac_init (configure_ac, cancellable))
    {
      ide_task_return_pointer (task, g_steal_pointer (&configure_ac), g_object_unref);
      IDE_EXIT;
    }

  configure_in = g_file_get_child (file, "configure.in");
  if (check_for_ac_init (configure_in, cancellable))
    {
      ide_task_return_pointer (task, g_steal_pointer (&configure_in), g_object_unref);
      IDE_EXIT;
    }

  ide_task_return_new_error (task,
                             G_IO_ERROR,
                             G_IO_ERROR_NOT_FOUND,
                             "Failed to locate configure.ac");

  IDE_EXIT;
}

static void
ide_autotools_build_system_discover_file_async (IdeAutotoolsBuildSystem *system,
                                                GFile                   *file,
                                                GCancellable            *cancellable,
                                                GAsyncReadyCallback      callback,
                                                gpointer                 user_data)
{
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_AUTOTOOLS_BUILD_SYSTEM (system));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (system, cancellable, callback, user_data);
  ide_task_set_task_data (task, g_object_ref (file), g_object_unref);
  ide_task_run_in_thread (task, ide_autotools_build_system_discover_file_worker);

  IDE_EXIT;
}

static GFile *
ide_autotools_build_system_discover_file_finish (IdeAutotoolsBuildSystem  *system,
                                                 GAsyncResult             *result,
                                                 GError                  **error)
{
  IdeTask *task = (IdeTask *)result;

  g_return_val_if_fail (IDE_IS_AUTOTOOLS_BUILD_SYSTEM (system), NULL);
  g_return_val_if_fail (IDE_IS_TASK (task), NULL);

  return ide_task_propagate_pointer (task, error);
}

static void
invalidate_makecache_stage (gpointer data,
                            gpointer user_data)
{
  IdePipelineStage *stage = data;

  if (IDE_IS_AUTOTOOLS_MAKECACHE_STAGE (stage))
    ide_pipeline_stage_set_completed (stage, FALSE);
}

static void
evict_makecache (IdeContext *context)
{
  IdeBuildManager *build_manager = ide_build_manager_from_context (context);
  IdePipeline *pipeline = ide_build_manager_get_pipeline (build_manager);

  ide_pipeline_foreach_stage (pipeline, invalidate_makecache_stage, NULL);
}

static gboolean
looks_like_makefile (IdeBuffer *buffer)
{
  GtkSourceLanguage *language;
  const gchar *path;
  GFile *file;

  g_assert (IDE_IS_BUFFER (buffer));

  file = ide_buffer_get_file (buffer);
  path = g_file_peek_path (file);

  if (path != NULL)
    {
      if (g_str_has_suffix (path, "Makefile.am") || g_str_has_suffix (path, ".mk"))
        return TRUE;
    }

  language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (buffer));

  if (language != NULL)
    {
      const gchar *lang_id;

      lang_id = gtk_source_language_get_id (language);

      if (ide_str_equal0 (lang_id, "automake") || ide_str_equal0 (lang_id, "makefile"))
        return TRUE;
    }

  return FALSE;
}

static void
ide_autotools_build_system__buffer_saved_cb (IdeAutotoolsBuildSystem *self,
                                             IdeBuffer               *buffer,
                                             IdeBufferManager        *buffer_manager)
{
  g_assert (IDE_IS_AUTOTOOLS_BUILD_SYSTEM (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));

  if (looks_like_makefile (buffer))
    evict_makecache (ide_object_get_context (IDE_OBJECT (self)));
}

static void
ide_autotools_build_system__vcs_changed_cb (IdeAutotoolsBuildSystem *self,
                                            IdeVcs                  *vcs)
{
  IDE_ENTRY;

  g_assert (IDE_IS_AUTOTOOLS_BUILD_SYSTEM (self));
  g_assert (IDE_IS_VCS (vcs));

  IDE_TRACE_MSG ("VCS has changed, evicting cached makecaches");

  evict_makecache (ide_object_get_context (IDE_OBJECT (self)));

  IDE_EXIT;
}

static void
ide_autotools_build_system_parent_set (IdeObject *object,
                                       IdeObject *parent)
{
  IdeAutotoolsBuildSystem *self = (IdeAutotoolsBuildSystem *)object;
  IdeBufferManager *buffer_manager;
  IdeContext *context;
  IdeVcs *vcs;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_AUTOTOOLS_BUILD_SYSTEM (self));
  g_assert (!parent || IDE_IS_OBJECT (parent));

  if (parent == NULL)
    return;

  context = ide_object_get_context (IDE_OBJECT (self));
  g_assert (IDE_IS_CONTEXT (context));

  buffer_manager = ide_buffer_manager_from_context (context);
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));

  vcs = ide_vcs_from_context (context);

  g_signal_connect_object (vcs,
                           "changed",
                           G_CALLBACK (ide_autotools_build_system__vcs_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  /*
   * FIXME:
   *
   * We could setup and try to track all of the makefiles in the system
   * with inotify watches. That would require that 1) we can tell if a file
   * is an automake file (or a dependent included file), and 2) lots of
   * inotify watches.
   *
   * What is cheap, easy, and can be done right now is to just watch for save
   * events on files that look like makefiles, and invalidate the makecache.
   */
  g_signal_connect_object (buffer_manager,
                           "buffer-saved",
                           G_CALLBACK (ide_autotools_build_system__buffer_saved_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

static gint
ide_autotools_build_system_get_priority (IdeBuildSystem *system)
{
  return 0;
}

static void
find_makecache_stage (gpointer data,
                      gpointer user_data)
{
  IdeMakecache **makecache = user_data;
  IdePipelineStage *stage = data;

  if (*makecache != NULL)
    return;

  if (IDE_IS_AUTOTOOLS_MAKECACHE_STAGE (stage))
    *makecache = ide_autotools_makecache_stage_get_makecache (IDE_AUTOTOOLS_MAKECACHE_STAGE (stage));
}

static void
ide_autotools_build_system_get_file_flags_cb (GObject      *object,
                                              GAsyncResult *result,
                                              gpointer      user_data)
{
  IdeMakecache *makecache = (IdeMakecache *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_auto(GStrv) flags = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAKECACHE (makecache));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  flags = ide_makecache_get_file_flags_finish (makecache, result, &error);

  if (flags == NULL)
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task, g_steal_pointer (&flags), g_strfreev);

  IDE_EXIT;
}

static void
ide_autotools_build_system_get_build_flags_execute_cb (GObject      *object,
                                                       GAsyncResult *result,
                                                       gpointer      user_data)
{
  IdeBuildManager *build_manager = (IdeBuildManager *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeMakecache *makecache = NULL;
  IdePipeline *pipeline;
  GCancellable *cancellable;
  GFile *file;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_MANAGER (build_manager));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  file = ide_task_get_task_data (task);
  cancellable = ide_task_get_cancellable (task);

  g_assert (G_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (!ide_build_manager_build_finish (build_manager, result, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  pipeline = ide_build_manager_get_pipeline (build_manager);

  /*
   * Locate our makecache by finding the makecache stage (which should have
   * successfully executed by now) and get makecache object. Then we can
   * locate the build flags for the file (which makecache will translate
   * into the appropriate build target).
   */

  ide_pipeline_foreach_stage (pipeline, find_makecache_stage, &makecache);

  if (makecache != NULL)
    {
      ide_makecache_get_file_flags_async (makecache,
                                          file,
                                          cancellable,
                                          ide_autotools_build_system_get_file_flags_cb,
                                          g_steal_pointer (&task));
      IDE_EXIT;
    }

  /*
   * We failed to locate anything, so just return an empty array of
   * of flags.
   */

  ide_task_return_pointer (task, g_new0 (gchar *, 1), g_free);

  IDE_EXIT;
}

static void
ide_autotools_build_system_get_build_flags_async (IdeBuildSystem      *build_system,
                                                  GFile               *file,
                                                  GCancellable        *cancellable,
                                                  GAsyncReadyCallback  callback,
                                                  gpointer             user_data)
{
  IdeAutotoolsBuildSystem *self = (IdeAutotoolsBuildSystem *)build_system;
  IdeBuildManager *build_manager;
  IdeContext *context;
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_AUTOTOOLS_BUILD_SYSTEM (self));
  g_assert (G_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_autotools_build_system_get_build_flags_async);
  ide_task_set_task_data (task, g_object_ref (file), g_object_unref);

  /*
   * To get the build flags for the file, we first need to get the makecache
   * for the current build pipeline. That requires advancing the pipeline to
   * at least the CONFIGURE stage so that our CONFIGURE|AFTER step has executed
   * to generate the Makecache file in $builddir. With that, we can load a new
   * IdeMakecache (if necessary) and scan the file for build flags.
   */

  context = ide_object_get_context (IDE_OBJECT (self));
  build_manager = ide_build_manager_from_context (context);

  ide_build_manager_build_async (build_manager,
                                   IDE_PIPELINE_PHASE_CONFIGURE,
                                   NULL,
                                   cancellable,
                                   ide_autotools_build_system_get_build_flags_execute_cb,
                                   g_steal_pointer (&task));

  IDE_EXIT;
}

static gchar **
ide_autotools_build_system_get_build_flags_finish (IdeBuildSystem  *build_system,
                                                   GAsyncResult    *result,
                                                   GError         **error)
{
  g_assert (IDE_IS_AUTOTOOLS_BUILD_SYSTEM (build_system));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static gchar *
ide_autotools_build_system_get_builddir (IdeBuildSystem   *build_system,
                                         IdePipeline *pipeline)
{
  IdeAutotoolsBuildSystem *self = (IdeAutotoolsBuildSystem *)build_system;
  g_autoptr(GFile) makefile = NULL;
  IdeContext *context;
  IdeVcs *vcs;
  GFile *workdir;
  IdeConfig *config;

  g_assert (IDE_IS_AUTOTOOLS_BUILD_SYSTEM (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  /*
   * If there is a Makefile in the build directory, then the project has been
   * configured in-tree, and we must override the builddir to perform in-tree
   * builds.
   */

  context = ide_object_get_context (IDE_OBJECT (self));
  vcs = ide_vcs_from_context (context);
  workdir = ide_vcs_get_workdir (vcs);
  config = ide_pipeline_get_config (pipeline);

  if (!g_file_is_native (workdir))
    return NULL;

  makefile = g_file_get_child (workdir, "Makefile");

  if (g_file_query_exists (makefile, NULL) || ide_config_get_locality (config) == IDE_BUILD_LOCALITY_IN_TREE)
    return g_file_get_path (workdir);

  return NULL;
}

static gchar *
ide_autotools_build_system_get_id (IdeBuildSystem *build_system)
{
  return g_strdup ("autotools");
}

static gchar *
ide_autotools_build_system_get_display_name (IdeBuildSystem *build_system)
{
  return g_strdup ("Autotools");
}

static void
ide_autotools_build_system_destroy (IdeObject *object)
{
  IdeAutotoolsBuildSystem *self = (IdeAutotoolsBuildSystem *)object;

  g_clear_pointer (&self->tarball_name, g_free);
  g_clear_object (&self->project_file);

  IDE_OBJECT_CLASS (ide_autotools_build_system_parent_class)->destroy (object);
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
    case PROP_PROJECT_FILE:
      g_value_set_object (value, self->project_file);
      break;

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
  IdeAutotoolsBuildSystem *self = IDE_AUTOTOOLS_BUILD_SYSTEM (object);

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
build_system_iface_init (IdeBuildSystemInterface *iface)
{
  iface->get_priority = ide_autotools_build_system_get_priority;
  iface->get_build_flags_async = ide_autotools_build_system_get_build_flags_async;
  iface->get_build_flags_finish = ide_autotools_build_system_get_build_flags_finish;
  iface->get_builddir = ide_autotools_build_system_get_builddir;
  iface->get_id = ide_autotools_build_system_get_id;
  iface->get_display_name = ide_autotools_build_system_get_display_name;
}

static void
ide_autotools_build_system_class_init (IdeAutotoolsBuildSystemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  object_class->get_property = ide_autotools_build_system_get_property;
  object_class->set_property = ide_autotools_build_system_set_property;

  i_object_class->parent_set = ide_autotools_build_system_parent_set;
  i_object_class->destroy = ide_autotools_build_system_destroy;

  properties [PROP_TARBALL_NAME] =
    g_param_spec_string ("tarball-name",
                         "Tarball Name",
                         "The name of the project tarball.",
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_PROJECT_FILE] =
    g_param_spec_object ("project-file",
                         "Project File",
                         "The path of the project file.",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
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
  g_autoptr(IdeTask) task = NULL;

  g_return_if_fail (IDE_IS_AUTOTOOLS_BUILD_SYSTEM (system));
  g_return_if_fail (G_IS_FILE (project_file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (system, cancellable, callback, user_data);
  ide_task_return_boolean (task, TRUE);
}

static gboolean
ide_autotools_build_system_parse_finish (IdeAutotoolsBuildSystem  *system,
                                         GAsyncResult             *result,
                                         GError                  **error)
{
  IdeTask *task = (IdeTask *)result;

  g_return_val_if_fail (IDE_IS_AUTOTOOLS_BUILD_SYSTEM (system), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (task), FALSE);

  return ide_task_propagate_boolean (task, error);
}

static void
parse_cb (GObject      *object,
          GAsyncResult *result,
          gpointer      user_data)
{
  IdeAutotoolsBuildSystem *self = (IdeAutotoolsBuildSystem *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_return_if_fail (IDE_IS_AUTOTOOLS_BUILD_SYSTEM (self));
  g_return_if_fail (IDE_IS_TASK (task));

  if (!ide_autotools_build_system_parse_finish (self, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);
}

static void
discover_file_cb (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  IdeAutotoolsBuildSystem *self;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  file = ide_autotools_build_system_discover_file_finish (self, result, &error);

  if (error != NULL)
    {
      g_debug ("Not an autotools build system: %s", error->message);
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  g_object_set (self, "project-file", file, NULL);

  ide_autotools_build_system_parse_async (self,
                                          file,
                                          ide_task_get_cancellable (task),
                                          parse_cb,
                                          g_object_ref (task));

  IDE_EXIT;
}

static void
ide_autotools_build_system_init_async (GAsyncInitable      *initable,
                                       gint                 io_priority,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  IdeAutotoolsBuildSystem *self = (IdeAutotoolsBuildSystem *)initable;
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_AUTOTOOLS_BUILD_SYSTEM (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_autotools_build_system_init_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  ide_autotools_build_system_discover_file_async (self,
                                                  self->project_file,
                                                  cancellable,
                                                  discover_file_cb,
                                                  g_steal_pointer (&task));

  IDE_EXIT;
}

static gboolean
ide_autotools_build_system_init_finish (GAsyncInitable  *initable,
                                        GAsyncResult    *result,
                                        GError         **error)
{
  IdeTask *task = (IdeTask *)result;

  g_return_val_if_fail (IDE_IS_AUTOTOOLS_BUILD_SYSTEM (initable), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (task), FALSE);

  return ide_task_propagate_boolean (task, error);
}

static void
async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = ide_autotools_build_system_init_async;
  iface->init_finish = ide_autotools_build_system_init_finish;
}
