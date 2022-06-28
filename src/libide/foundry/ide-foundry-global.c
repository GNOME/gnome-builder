/* ide-foundry-global.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-foundry-global"

#include "config.h"

#include <errno.h>
#include <glib/gstdio.h>
#ifdef __linux__
# include <sys/mman.h>
#endif
#include <unistd.h>

#include "ide-build-manager.h"
#include "ide-foundry-global.h"
#include "ide-pipeline.h"
#include "ide-run-context.h"
#include "ide-runtime-manager.h"
#include "ide-runtime.h"

static IdeSubprocessLauncher *
create_host_launcher (void)
{
  g_autoptr(IdeRunContext) run_context = ide_run_context_new ();

  /* To be like the build pipeline, we do not add the "minimal"
   * environment as that would give display access which the
   * build pipeline generally does not have.
   */
  ide_run_context_push_host (run_context);
  return ide_run_context_end (run_context, NULL);
}

/**
 * ide_foundry_get_launcher_for_context:
 * @context: an #IdeContext
 * @program_name: the basename of the program
 * @bundled_program_path: (nullable): the path to a bundled version of the program
 * @error: a location for a #GError, or %NULL
 *
 * A helper to get a launcher for @program in the proper environment.
 *
 * If available within the build environment, that will be used.
 * Otherwise, either the host system or a bundled version of the
 * program may be used as a fallback.
 *
 * If the program could not be located, %NULL is returned and
 * @error is set.
 *
 * Returns: (transfer full): a #IdeSubprocessLauncher or %NULL and
 *   @error is set.
 */
IdeSubprocessLauncher *
ide_foundry_get_launcher_for_context (IdeContext  *context,
                                      const char  *program_name,
                                      const char  *bundled_program_path,
                                      GError     **error)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  IdeRuntimeManager *runtime_manager;
  g_autoptr(GFile) workdir = NULL;
  g_autofree char *program_path = NULL;
  IdePipeline *pipeline = NULL;
  IdeRuntime *host = NULL;
  const char *srcdir = NULL;
  const char *local_program_path;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (program_name != NULL, NULL);

  workdir = ide_context_ref_workdir (context);
  srcdir = g_file_peek_path (workdir);
  local_program_path = program_name;

  if (ide_context_has_project (context))
    {
      IdeBuildManager *build_manager = ide_build_manager_from_context (context);

      pipeline = ide_build_manager_get_pipeline (build_manager);
      runtime_manager = ide_runtime_manager_from_context (context);
      host = ide_runtime_manager_get_runtime (runtime_manager, "host");
    }

  if (pipeline != NULL)
    srcdir = ide_pipeline_get_srcdir (pipeline);

  if (local_program_path != NULL)
    {
      g_autofree char *local_program = g_build_filename (srcdir, local_program_path, NULL);
      if (g_file_test (local_program, G_FILE_TEST_IS_EXECUTABLE))
        program_path = g_steal_pointer (&local_program);
    }

  if (pipeline != NULL &&
      ((program_path != NULL && ide_pipeline_contains_program_in_path (pipeline, program_path, NULL)) ||
      ide_pipeline_contains_program_in_path (pipeline, program_name, NULL)) &&
      (launcher = ide_pipeline_create_launcher (pipeline, NULL)))
    IDE_GOTO (setup_launcher);

  if (host != NULL)
    {
      /* Now try on the host using the "host" runtime which can do
       * a better job of discovering the program on the host and
       * take into account if the user has something modifying the
       * shell like .bashrc.
       */
      if (program_path != NULL ||
          ide_runtime_contains_program_in_path (host, program_name, NULL))
        {
          launcher = create_host_launcher ();
          IDE_GOTO (setup_launcher);
        }
    }
  else if (program_path != NULL)
    {
      launcher = create_host_launcher ();
      IDE_GOTO (setup_launcher);
    }

  if (bundled_program_path != NULL && ide_is_flatpak ())
    program_path = g_strdup (bundled_program_path);

  /* See if Builder itself has bundled the program */
  if (program_path || g_find_program_in_path (program_name))
    {
      launcher = ide_subprocess_launcher_new (0);
      IDE_GOTO (setup_launcher);
    }

  g_set_error (error,
               G_IO_ERROR,
               G_IO_ERROR_NOT_FOUND,
               "Failed to locate program \"%s\"",
               program_name);

  IDE_RETURN (NULL);

setup_launcher:
  ide_subprocess_launcher_push_argv (launcher, program_path ? program_path : program_name);
  ide_subprocess_launcher_set_cwd (launcher, srcdir);
  ide_subprocess_launcher_set_flags (launcher,
                                     (G_SUBPROCESS_FLAGS_STDIN_PIPE |
                                      G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                      G_SUBPROCESS_FLAGS_STDERR_PIPE));

  IDE_RETURN (g_steal_pointer (&launcher));
}

static gboolean
write_all_bytes (int           fd,
                 const guint8 *buf,
                 gsize         len)
{
  gsize to_write = len;

  g_assert (fd > -1);
  g_assert (buf != NULL || len == 0);

  if (buf == NULL || len == 0)
    return TRUE;

  while (to_write > 0)
    {
      gssize ret;

      errno = 0;
      ret = write (fd, buf, to_write);

      if (ret < 0)
        {
          if (errno == EINTR)
            continue;
          return FALSE;
        }

      g_assert (ret <= to_write);

      to_write -= ret;
      buf += ret;
    }

  return TRUE;
}

/**
 * ide_foundry_bytes_to_memfd:
 * @bytes: (nullable): a #GBytes or %NULL
 * @name: the name for the memfd or tempfile
 *
 * Writes all of @bytes to a new memfd or tempfile and returns
 * the file-descriptor. -1 is returned upon error.
 *
 * if @bytes is %NULL, then a memfd/tempfile that is empty will
 * be returned if successful.
 *
 * Returns: -1 upon failure, otherwise a file-descriptor
 */
int
ide_foundry_bytes_to_memfd (GBytes     *bytes,
                            const char *name)
{
  const guint8 *data = NULL;
  gsize len = 0;
  int fd = -1;

  g_return_val_if_fail (name != NULL, -1);

#ifdef __linux__
  fd = memfd_create (name, 0);
#endif

  if (fd == -1)
    {
      g_autofree char *template = g_strdup_printf ("%s.XXXXXX", name);

      fd = g_mkstemp (template);

      if (fd < 0)
        return -1;

      g_unlink (template);
    }

  g_assert (fd >= 0);

  if (bytes != NULL)
    data = g_bytes_get_data (bytes, &len);

  if (!write_all_bytes (fd, data, len))
    {
      close (fd);
      return -1;
    }

  /* Make sure we are set at the start */
  lseek (fd, SEEK_SET, 0);

  return fd;
}

int
ide_foundry_file_to_memfd (GFile      *file,
                           const char *name)
{
  g_autoptr(GBytes) bytes = NULL;

  g_return_val_if_fail (G_IS_FILE (file), -1);

  if (!(bytes = g_file_load_bytes (file, NULL, NULL, NULL)))
    return -1;

  return ide_foundry_bytes_to_memfd (bytes, name);
}
