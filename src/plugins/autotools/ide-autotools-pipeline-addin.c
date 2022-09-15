/* ide-autotools-pipeline-addin.c
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-autotools-pipeline-addin"

#include <glib/gi18n.h>

#include "ide-autotools-autogen-stage.h"
#include "ide-autotools-build-system.h"
#include "ide-autotools-make-stage.h"
#include "ide-autotools-makecache-stage.h"
#include "ide-autotools-pipeline-addin.h"

static gboolean
register_autoreconf_stage (IdeAutotoolsPipelineAddin  *self,
                           IdePipeline                *pipeline,
                           GError                    **error)
{
  g_autofree char *configure_path = NULL;
  g_autoptr(IdePipelineStage) stage = NULL;
  const char *srcdir;
  gboolean completed;
  guint stage_id;

  g_assert (IDE_IS_AUTOTOOLS_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  configure_path = ide_pipeline_build_srcdir_path (pipeline, "configure", NULL);
  completed = g_file_test (configure_path, G_FILE_TEST_IS_REGULAR);
  srcdir = ide_pipeline_get_srcdir (pipeline);

  stage = g_object_new (IDE_TYPE_AUTOTOOLS_AUTOGEN_STAGE,
                        "name", _("Bootstrapping build system"),
                        "completed", completed,
                        "srcdir", srcdir,
                        NULL);

  stage_id = ide_pipeline_attach (pipeline, IDE_PIPELINE_PHASE_AUTOGEN, 0, stage);

  ide_pipeline_addin_track (IDE_PIPELINE_ADDIN (self), stage_id);

  return TRUE;
}

static gint
compare_mtime (const char *path_a,
               const char *path_b)
{
  g_autoptr(GFile) file_a = g_file_new_for_path (path_a);
  g_autoptr(GFile) file_b = g_file_new_for_path (path_b);
  g_autoptr(GFileInfo) info_a = NULL;
  g_autoptr(GFileInfo) info_b = NULL;
  gint64 ret = 0;

  info_a = g_file_query_info (file_a,
                              G_FILE_ATTRIBUTE_TIME_MODIFIED,
                              G_FILE_QUERY_INFO_NONE,
                              NULL,
                              NULL);

  info_b = g_file_query_info (file_b,
                              G_FILE_ATTRIBUTE_TIME_MODIFIED,
                              G_FILE_QUERY_INFO_NONE,
                              NULL,
                              NULL);

  ret = (gint64)g_file_info_get_attribute_uint64 (info_a, G_FILE_ATTRIBUTE_TIME_MODIFIED) -
        (gint64)g_file_info_get_attribute_uint64 (info_b, G_FILE_ATTRIBUTE_TIME_MODIFIED);

  if (ret < 0)
    return -1;
  else if (ret > 0)
    return 1;
  return 0;
}

static void
check_configure_status (IdeAutotoolsPipelineAddin *self,
                        IdePipeline               *pipeline,
                        GPtrArray                 *targets,
                        GCancellable              *cancellable,
                        IdePipelineStage          *stage)
{
  g_autofree char *configure_ac = NULL;
  g_autofree char *configure = NULL;
  g_autofree char *config_status = NULL;
  g_autofree char *makefile = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_AUTOTOOLS_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_PIPELINE_STAGE (stage));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  configure = ide_pipeline_build_srcdir_path (pipeline, "configure", NULL);
  configure_ac = ide_pipeline_build_srcdir_path (pipeline, "configure.ac", NULL);
  config_status = ide_pipeline_build_builddir_path (pipeline, "config.status", NULL);
  makefile = ide_pipeline_build_builddir_path (pipeline, "Makefile", NULL);

  IDE_TRACE_MSG (" configure.ac is at %s", configure_ac);
  IDE_TRACE_MSG (" configure is at %s", configure);
  IDE_TRACE_MSG (" config.status is at %s", config_status);
  IDE_TRACE_MSG (" makefile is at %s", makefile);

  /*
   * First make sure some essential files exist. If not, we need to run the
   * configure process.
   *
   * TODO: This may take some tweaking if we ever try to reuse existing builds
   *       that were performed in-tree.
   */
  if (!g_file_test (configure_ac, G_FILE_TEST_IS_REGULAR) ||
      !g_file_test (configure, G_FILE_TEST_IS_REGULAR) ||
      !g_file_test (config_status, G_FILE_TEST_IS_REGULAR) ||
      !g_file_test (makefile, G_FILE_TEST_IS_REGULAR))
    {
      ide_pipeline_stage_set_completed (stage, FALSE);
      IDE_EXIT;
    }

  /*
   * Now make sure that config.status and Makefile are indeed newer than
   * our configure script.
   */
  if (compare_mtime (configure_ac, configure) < 0 &&
      compare_mtime (configure, config_status) < 0 &&
      compare_mtime (configure, makefile) < 0)
    {
      /*
       * TODO: It would be fancy if we could look at '^ac_cs_config=' to determine
       * if the configure args match what we expect. But this is a bit more
       * complicated than simply a string comparison.
       */
      ide_pipeline_stage_set_completed (stage, TRUE);
      IDE_EXIT;
    }

  ide_pipeline_stage_set_completed (stage, FALSE);

  IDE_EXIT;
}

static const char *
compiler_environment_from_language (const char *language)
{
  if (g_strcmp0 (language, IDE_TOOLCHAIN_LANGUAGE_C) == 0)
    return "CC";

  if (g_strcmp0 (language, IDE_TOOLCHAIN_LANGUAGE_CPLUSPLUS) == 0)
    return "CXX";

  if (g_strcmp0 (language, IDE_TOOLCHAIN_LANGUAGE_PYTHON) == 0)
    return "PYTHON";

  if (g_strcmp0 (language, IDE_TOOLCHAIN_LANGUAGE_FORTRAN) == 0)
    return "FC";

  if (g_strcmp0 (language, IDE_TOOLCHAIN_LANGUAGE_D) == 0)
    return "DC";

  if (g_strcmp0 (language, IDE_TOOLCHAIN_LANGUAGE_VALA) == 0)
    return "VALAC";

  return NULL;
}

static void
add_compiler_env_variables (gpointer key,
                            gpointer value,
                            gpointer user_data)
{
  IdeRunCommand *run_command = (IdeRunCommand *)user_data;
  const char *env;

  g_assert (IDE_IS_RUN_COMMAND (run_command));

  if ((env = compiler_environment_from_language (key)))
    ide_run_command_setenv (run_command, env, value);
}

static gboolean
register_configure_stage (IdeAutotoolsPipelineAddin  *self,
                          IdePipeline                *pipeline,
                          GError                    **error)
{
  g_autoptr(IdeRunCommand) run_command = NULL;
  g_autoptr(IdePipelineStage) stage = NULL;
  IdeConfig *configuration;
  IdeToolchain *toolchain;
  g_autofree char *configure_path = NULL;
  g_autofree char *host_arg = NULL;
  g_autoptr(IdeTriplet) triplet = NULL;
  g_autoptr(GStrvBuilder) argv = NULL;
  const char *config_opts;
  const char *prefix;
  g_auto(GStrv) strv = NULL;
  guint stage_id;

  g_assert (IDE_IS_AUTOTOOLS_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  argv = g_strv_builder_new ();
  run_command = ide_run_command_new ();

  /* /path/to/configure */
  configure_path = ide_pipeline_build_srcdir_path (pipeline, "configure", NULL);
  g_strv_builder_add (argv, configure_path);

  /* --host=triplet */
  configuration = ide_pipeline_get_config (pipeline);
  toolchain = ide_pipeline_get_toolchain (pipeline);
  triplet = ide_toolchain_get_host_triplet (toolchain);
  host_arg = g_strdup_printf ("--host=%s", ide_triplet_get_full_name (triplet));
  g_strv_builder_add (argv, host_arg);

  if (g_strcmp0 (ide_toolchain_get_id (toolchain), "default") != 0)
    {
      GHashTable *compilers = ide_toolchain_get_tools_for_id (toolchain,
                                                              IDE_TOOLCHAIN_TOOL_CC);
      const char *tool_path;

      g_hash_table_foreach (compilers, add_compiler_env_variables, run_command);

      tool_path = ide_toolchain_get_tool_for_language (toolchain,
                                                       IDE_TOOLCHAIN_LANGUAGE_ANY,
                                                       IDE_TOOLCHAIN_TOOL_AR);
      if (tool_path != NULL)
        ide_run_command_setenv (run_command, "AR", tool_path);

      tool_path = ide_toolchain_get_tool_for_language (toolchain,
                                                       IDE_TOOLCHAIN_LANGUAGE_ANY,
                                                       IDE_TOOLCHAIN_TOOL_STRIP);
      if (tool_path != NULL)
        ide_run_command_setenv (run_command, "STRIP", tool_path);

      tool_path = ide_toolchain_get_tool_for_language (toolchain,
                                                       IDE_TOOLCHAIN_LANGUAGE_ANY,
                                                       IDE_TOOLCHAIN_TOOL_PKG_CONFIG);
      if (tool_path != NULL)
        ide_run_command_setenv (run_command, "PKG_CONFIG", tool_path);
    }

  /*
   * Parse the configure options as defined in the build configuration and append
   * them to configure.
   */

  config_opts = ide_config_get_config_opts (configuration);
  prefix = ide_config_get_prefix (configuration);

  if (prefix != NULL)
    {
      g_autofree char *prefix_arg = g_strdup_printf ("--prefix=%s", prefix);
      g_strv_builder_add (argv, prefix_arg);
    }

  if (!ide_str_empty0 (config_opts))
    {
      g_auto(GStrv) parsed = NULL;
      gint argc = 0;

      if (!g_shell_parse_argv (config_opts, &argc, &parsed, error))
        return FALSE;

      g_strv_builder_addv (argv, (const char **)parsed);
    }

  strv = g_strv_builder_end (argv);
  ide_run_command_set_argv (run_command, (const char * const *)strv);

  stage = g_object_new (IDE_TYPE_PIPELINE_STAGE_COMMAND,
                        "name", _("Configuring project"),
                        "build-command", run_command,
                        NULL);

  /*
   * If the Makefile exists within the builddir, we will assume the
   * project has been initially configured correctly. Otherwise, every
   * time the user opens the project they have to go through a full
   * re-configure and build.
   *
   * Should the user need to perform an autogen, a manual rebuild is
   * easily achieved so this seems to be the sensible default.
   *
   * If we were to do this "correctly", we would look at config.status to
   * match the "ac_cs_config" variable to what we set. However, that is
   * influenced by environment variables, so its a bit non-trivial.
   */
  g_signal_connect_object (stage,
                           "query",
                           G_CALLBACK (check_configure_status),
                           self,
                           G_CONNECT_SWAPPED);

  stage_id = ide_pipeline_attach (pipeline, IDE_PIPELINE_PHASE_CONFIGURE, 0, stage);

  ide_pipeline_addin_track (IDE_PIPELINE_ADDIN (self), stage_id);

  return TRUE;
}

static gboolean
register_make_stage (IdeAutotoolsPipelineAddin  *self,
                     IdePipeline                *pipeline,
                     IdePipelinePhase            phase,
                     GError                    **error,
                     const char                 *target,
                     const char                 *clean_target)
{
  g_autoptr(IdePipelineStage) stage = NULL;
  IdeConfig *config;
  guint stage_id;
  gint parallel;

  g_assert (IDE_IS_AUTOTOOLS_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  config = ide_pipeline_get_config (pipeline);
  parallel = ide_config_get_parallelism (config);

  stage = g_object_new (IDE_TYPE_AUTOTOOLS_MAKE_STAGE,
                        "name", _("Building project"),
                        "clean-target", clean_target,
                        "parallel", parallel,
                        "target", target,
                        NULL);

  stage_id = ide_pipeline_attach (pipeline, phase, 0, stage);
  ide_pipeline_addin_track (IDE_PIPELINE_ADDIN (self), stage_id);

  return TRUE;
}

static gboolean
register_makecache_stage (IdeAutotoolsPipelineAddin  *self,
                          IdePipeline           *pipeline,
                          GError                    **error)
{
  g_autoptr(IdePipelineStage) stage = NULL;
  guint stage_id;

  g_assert (IDE_IS_AUTOTOOLS_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  if (NULL == (stage = ide_autotools_makecache_stage_new_for_pipeline (pipeline, error)))
    return FALSE;

  ide_pipeline_stage_set_name (stage, _("Caching build commands"));

  stage_id = ide_pipeline_attach (pipeline,
                                  IDE_PIPELINE_PHASE_CONFIGURE | IDE_PIPELINE_PHASE_AFTER,
                                  0,
                                  stage);
  ide_pipeline_addin_track (IDE_PIPELINE_ADDIN (self), stage_id);

  return TRUE;
}

static void
ide_autotools_pipeline_addin_load (IdePipelineAddin *addin,
                                   IdePipeline      *pipeline)
{
  IdeAutotoolsPipelineAddin *self = (IdeAutotoolsPipelineAddin *)addin;
  g_autoptr(GError) error = NULL;
  IdeBuildSystem *build_system;
  IdeContext *context;

  g_assert (IDE_IS_AUTOTOOLS_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  context = ide_object_get_context (IDE_OBJECT (addin));
  build_system = ide_build_system_from_context (context);

  if (!IDE_IS_AUTOTOOLS_BUILD_SYSTEM (build_system))
    return;

  if (!register_autoreconf_stage (self, pipeline, &error) ||
      !register_configure_stage (self, pipeline, &error) ||
      !register_makecache_stage (self, pipeline, &error) ||
      !register_make_stage (self, pipeline, IDE_PIPELINE_PHASE_BUILD, &error, "all", "clean") ||
      !register_make_stage (self, pipeline, IDE_PIPELINE_PHASE_INSTALL, &error, "install", NULL))
    {
      g_assert (error != NULL);
      g_warning ("Failed to create autotools launcher: %s", error->message);
      return;
    }
}

static void
addin_iface_init (IdePipelineAddinInterface *iface)
{
  iface->load = ide_autotools_pipeline_addin_load;
}

struct _IdeAutotoolsPipelineAddin { IdeObject parent; };

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeAutotoolsPipelineAddin, ide_autotools_pipeline_addin, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_PIPELINE_ADDIN, addin_iface_init))

static void
ide_autotools_pipeline_addin_class_init (IdeAutotoolsPipelineAddinClass *klass)
{
}

static void
ide_autotools_pipeline_addin_init (IdeAutotoolsPipelineAddin *self)
{
}
