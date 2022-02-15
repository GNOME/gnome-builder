/* rust-analyzer-pipeline-addin.c
 *
 * Copyright 2021 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "rust-analyzer-pipeline-addin"

#include "config.h"

#include <libide-gui.h>
#include <libide-core.h>
#include <libide-io.h>
#include <libide-terminal.h>

#include "rust-analyzer-pipeline-addin.h"

#if 0
# define DEV_MODE
#endif

struct _RustAnalyzerPipelineAddin
{
  IdeObject        parent_instance;
  IdeNotification *notif;
  IdePipeline     *pipeline;
  gchar           *path;
  gchar           *cargo_home;
  guint            run_on_host : 1;
};

static void pipeline_addin_iface_init (IdePipelineAddinInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (RustAnalyzerPipelineAddin, rust_analyzer_pipeline_addin, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_PIPELINE_ADDIN, pipeline_addin_iface_init))

static void
rust_analyzer_pipeline_addin_class_init (RustAnalyzerPipelineAddinClass *klass)
{
}

static void
rust_analyzer_pipeline_addin_init (RustAnalyzerPipelineAddin *self)
{
}

static gboolean
is_meson_project (RustAnalyzerPipelineAddin *self)
{
  IdeBuildSystem *build_system;
  IdeContext *context;

  g_assert (RUST_IS_ANALYZER_PIPELINE_ADDIN (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  build_system = ide_build_system_from_context (context);

  return strstr (G_OBJECT_TYPE_NAME (build_system), "Meson") != NULL;
}

static GFile *
find_cargo_toml_from_file (GFile *file)
{
  if (file != NULL)
    {
      g_autoptr(GFile) parent = g_file_get_parent (file);
      g_autofree gchar *name = g_file_get_basename (file);

      if (parent != NULL)
        {
          g_autoptr(GFile) cargo_toml = g_file_get_child (parent, "Cargo.toml");

          if (g_strcmp0 (name, "Cargo.toml") == 0)
            return g_steal_pointer (&parent);

          if (g_file_query_exists (cargo_toml, NULL))
            return g_steal_pointer (&cargo_toml);

          return find_cargo_toml_from_file (parent);
        }
    }

  return NULL;
}

static void
rust_analyzer_pipeline_addin_discover_workdir_cb (GtkWidget *widget,
                                                  gpointer   user_data)
{
  IdePage *page = (IdePage *)widget;
  GFile **workdir = user_data;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GFile) cargo_toml = NULL;

  g_assert (IDE_IS_PAGE (page));
  g_assert (workdir != NULL);
  g_assert (*workdir == NULL || G_IS_FILE (*workdir));

  if (!(file = ide_page_get_file_or_directory (page)) || !g_file_is_native (file))
    return;

  if ((cargo_toml = find_cargo_toml_from_file (file)))
    {
      g_autoptr(GFile) parent = g_file_get_parent (cargo_toml);

      if (parent == NULL)
        return;

      if (*workdir == NULL || g_file_has_prefix (*workdir, parent))
        g_set_object (workdir, parent);
    }
}

void
rust_analyzer_pipeline_addin_discover_workdir (RustAnalyzerPipelineAddin  *self,
                                               gchar                     **src_workdir,
                                               gchar                     **build_workdir)
{
  g_autoptr(IdeContext) context = NULL;
  g_autofree gchar *relative = NULL;
  g_autofree gchar *ret = NULL;
  g_autoptr(GFile) project_workdir = NULL;
  g_autoptr(GFile) workdir = NULL;
  g_autoptr(GFile) cargo_toml = NULL;
  IdeWorkbench *workbench;

  IDE_ENTRY;

  g_assert (RUST_IS_ANALYZER_PIPELINE_ADDIN (self));

  if (!(context = ide_object_ref_context (IDE_OBJECT (self))) ||
      !(workbench = ide_workbench_from_context (context)) ||
      !(project_workdir = ide_context_ref_workdir (context)))
    {
      *src_workdir = NULL;
      *build_workdir = NULL;
      IDE_EXIT;
    }

  /* Use project root as workdir if it contains Cargo.toml, otherwise
   * try to look at open files and locate a workdir from the topmost
   * directory containing a Cargo.toml.
   */
  cargo_toml = g_file_get_child (project_workdir, "Cargo.toml");
  if (!g_file_query_exists (cargo_toml, NULL))
    ide_workbench_foreach_page (workbench,
                                rust_analyzer_pipeline_addin_discover_workdir_cb,
                                &workdir);
  if (workdir == NULL)
    workdir = g_object_ref (project_workdir);

  /* Now that we found what would be the workdir from the source
   * tree, we want to translate that into the build tree so that
   * we increase the chance that rust-analyzer will reuse artifacts
   * from building the actual project.
   *
   * For example, it places a bunch of data in target/, but we don't
   * want to polute the source tree with that, we want it to end up
   * in the $builddir/target where meson/cargo would put it while
   * actually building the project.
   */
  if (g_file_equal (project_workdir, workdir))
    ret = g_strdup (ide_pipeline_get_builddir (self->pipeline));
  else if ((relative = g_file_get_relative_path (project_workdir, workdir)))
    ret = ide_pipeline_build_builddir_path (self->pipeline, relative, NULL);
  else
    ret = g_file_get_path (workdir);

  *src_workdir = g_file_get_path (workdir);
  *build_workdir = g_steal_pointer (&ret);

  IDE_TRACE_MSG ("rust-analyzer workdir=%s builddir=%s", *src_workdir, *build_workdir);

  IDE_EXIT;
}

IdeSubprocessLauncher *
rust_analyzer_pipeline_addin_create_launcher (RustAnalyzerPipelineAddin *self)
{
  GSubprocessFlags flags = G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autofree gchar *src_workdir = NULL;
  g_autofree gchar *build_workdir = NULL;
  g_autofree gchar *cargo_target_dir = NULL;
  g_autofree gchar *cargo_home = NULL;

  IDE_ENTRY;

  g_return_val_if_fail (RUST_IS_ANALYZER_PIPELINE_ADDIN (self), NULL);
  g_return_val_if_fail (self->cargo_home == NULL || self->path != NULL, NULL);

  if (self->path == NULL)
    IDE_RETURN (NULL);

  rust_analyzer_pipeline_addin_discover_workdir (self, &src_workdir, &build_workdir);
  if (src_workdir == NULL || build_workdir == NULL)
    IDE_RETURN (NULL);

#ifdef DEV_MODE
  flags &= ~G_SUBPROCESS_FLAGS_STDERR_SILENCE;
#endif

  if (self->run_on_host)
    {
      const char *user_shell = ide_get_user_shell ();

      g_debug ("Using rust-analyzer from host");

      launcher = ide_subprocess_launcher_new (flags);
      ide_subprocess_launcher_set_run_on_host (launcher, self->run_on_host);
      ide_subprocess_launcher_set_clear_env (launcher, TRUE);

      if (self->cargo_home != NULL)
        ide_subprocess_launcher_setenv (launcher, "CARGO_HOME", self->cargo_home, TRUE);

      /* Try to use the user's shell to increase chances we get the right
       * $PATH for the user session.
       */
      if (ide_shell_supports_dash_c (user_shell) &&
          ide_shell_supports_dash_login (user_shell))
        {
          ide_subprocess_launcher_push_argv (launcher, user_shell);
          ide_subprocess_launcher_push_argv (launcher, "--login");
          ide_subprocess_launcher_push_argv (launcher, "-c");
          ide_subprocess_launcher_push_argv (launcher, self->path);
        }
      else
        {
          ide_subprocess_launcher_push_argv (launcher, self->path);
        }
    }
  else
    {
      g_debug ("Using rust-analyzer from runtime");

      if (!(launcher = ide_pipeline_create_launcher (self->pipeline, NULL)))
        return NULL;

      /* Unset CARGO_HOME if it's set by the runtime */
      ide_subprocess_launcher_set_flags (launcher, flags);
      ide_subprocess_launcher_set_clear_env (launcher, TRUE);
      ide_subprocess_launcher_push_argv (launcher, self->path);
    }

  /* In Builder meson projects that use Cargo, we use target/cargo-home as
   * a convention within the builddir. This is just convention, but it's the
   * one thing we got right now to work off os.
   */
  if (is_meson_project (self))
    {
      cargo_target_dir = g_build_filename (build_workdir, "target", NULL);
      cargo_home = g_build_filename (build_workdir, "cargo-home", NULL);
    }
  else
    {
      cargo_target_dir = g_strdup (build_workdir);
      cargo_home = g_strdup (self->cargo_home);
    }

  if (cargo_home != NULL)
    ide_subprocess_launcher_setenv (launcher, "CARGO_HOME", cargo_home, FALSE);
  ide_subprocess_launcher_setenv (launcher, "CARGO_TARGET_DIR", cargo_target_dir, FALSE);

#ifdef DEV_MODE
  ide_subprocess_launcher_setenv (launcher, "RA_LOG", "rust_analyzer=info", TRUE);
#endif

  ide_subprocess_launcher_set_cwd (launcher, src_workdir);

  return g_steal_pointer (&launcher);
}

static void
set_path (RustAnalyzerPipelineAddin *self,
          const gchar               *path,
          const gchar               *cargo_home,
          gboolean                   run_on_host)
{
  if (g_strcmp0 (path, self->path) != 0)
    {
      g_free (self->path);
      self->path = g_strdup (path);
    }

  if (g_strcmp0 (cargo_home, self->cargo_home) != 0)
    {
      g_free (self->cargo_home);
      self->cargo_home = g_strdup (cargo_home);
    }

  self->run_on_host = !!run_on_host;
}

static void
rust_analyzer_pipeline_addin_load (IdePipelineAddin *addin,
                                   IdePipeline      *pipeline)
{
  RustAnalyzerPipelineAddin *self = (RustAnalyzerPipelineAddin *)addin;
  IdeRuntimeManager *runtime_manager;
  IdeBuildSystem *buildsystem = NULL;
  IdeContext *context = NULL;
  IdeRuntime *host;
  g_autoptr(GFile) cargo_home = NULL;
  g_autoptr(GFile) file = NULL;
  g_autofree char *local_path = NULL;

  IDE_ENTRY;

  g_assert (RUST_IS_ANALYZER_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  context = ide_object_get_context (IDE_OBJECT (pipeline));
  buildsystem = ide_build_system_from_context (context);

  if (!ide_build_system_supports_language (buildsystem, "rust"))
    IDE_EXIT;

  self->pipeline = pipeline;

  if (ide_pipeline_contains_program_in_path (pipeline, "rust-analyzer", NULL))
    {
      set_path (self, "rust-analyzer", NULL, FALSE);
      IDE_EXIT;
    }

  cargo_home = g_file_new_build_filename (g_get_home_dir (), ".cargo", NULL);
  file = g_file_get_child (cargo_home, "bin" G_DIR_SEPARATOR_S "rust-analyzer");

  if (g_file_query_exists (file, NULL))
    {
      set_path (self, g_file_peek_path (file), g_file_peek_path (cargo_home), TRUE);
      IDE_EXIT;
    }

  /* Try ~/.local/bin/ where rust-analyzer suggests installation */
  local_path = g_build_filename (g_get_home_dir (), ".local", "bin", "rust-analyzer", NULL);
  if (g_file_test (local_path, G_FILE_TEST_IS_EXECUTABLE))
    {
      set_path (self, local_path, NULL, TRUE);
      IDE_EXIT;
    }

  /* Check on host, hoping to inherit PATH */
  runtime_manager = ide_runtime_manager_from_context (context);
  host = ide_runtime_manager_get_runtime (runtime_manager, "host");
  if (ide_runtime_contains_program_in_path (host, "rust-analyzer", NULL))
    {
      set_path (self, "rust-analyzer", NULL, TRUE);
      IDE_EXIT;
    }

  self->notif = ide_notification_new ();
  ide_notification_set_title (self->notif, "Rust-analyzer is missing");
  ide_notification_set_body (self->notif, "Install rust-analyzer in your PATH, or use the Rust flatpak extension in your manifest.");
  ide_notification_set_urgent (self->notif, TRUE);
  ide_notification_attach (self->notif, IDE_OBJECT (pipeline));

  set_path (self, NULL, NULL, FALSE);

  IDE_EXIT;
}

static void
rust_analyzer_pipeline_addin_unload (IdePipelineAddin *addin,
                                     IdePipeline      *pipeline)
{
  RustAnalyzerPipelineAddin *self = (RustAnalyzerPipelineAddin *)addin;

  IDE_ENTRY;

  g_assert (RUST_IS_ANALYZER_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  if (self->notif)
    {
      ide_notification_withdraw (self->notif);
      g_clear_object (&self->notif);
    }

  IDE_EXIT;
}

static void
pipeline_addin_iface_init (IdePipelineAddinInterface *iface)
{
  iface->load = rust_analyzer_pipeline_addin_load;
  iface->unload = rust_analyzer_pipeline_addin_unload;
}

