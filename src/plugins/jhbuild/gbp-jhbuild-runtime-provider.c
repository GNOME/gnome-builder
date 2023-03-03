/* gbp-jhbuild-runtime-provider.c
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

#define G_LOG_DOMAIN "gbp-jhbuild-runtime-provider"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-foundry.h>
#include <libide-threading.h>

#include "gbp-jhbuild-runtime.h"
#include "gbp-jhbuild-runtime-provider.h"

struct _GbpJhbuildRuntimeProvider
{
  IdeRuntimeProvider parent_instance;
};

static char *
get_jhbuild_path (void)
{
  g_autofree char *local_path = g_build_filename (g_get_home_dir (), ".local", "bin", "jhbuild", NULL);
  const char *maybe_path[] = { local_path, "jhbuild" };

  for (guint i = 0; i < G_N_ELEMENTS (maybe_path); i++)
    {
      g_autoptr(IdeSubprocessLauncher) launcher = NULL;
      g_autoptr(IdeSubprocess) subprocess = NULL;
      const char *path = maybe_path[i];

      launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_SILENCE | G_SUBPROCESS_FLAGS_STDERR_SILENCE);
      ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
      ide_subprocess_launcher_set_clear_env (launcher, FALSE);
      ide_subprocess_launcher_push_argv (launcher, "which");
      ide_subprocess_launcher_push_argv (launcher, path);

      if (!(subprocess = ide_subprocess_launcher_spawn (launcher, NULL, NULL)))
        continue;

      if (ide_subprocess_wait_check (subprocess, NULL, NULL))
        return g_strdup (path);
    }

  return NULL;
}

static char *
get_jhbuild_prefix (const char *jhbuild_bin)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autofree char *stdout_buf = NULL;

  g_assert (jhbuild_bin != NULL);

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE);
  ide_subprocess_launcher_push_args (launcher, IDE_STRV_INIT (jhbuild_bin, "run", "sh", "-c", "echo $JHBUILD_PREFIX"));
  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);

  if (!(subprocess = ide_subprocess_launcher_spawn (launcher, NULL, NULL)))
    return NULL;

  if (!ide_subprocess_communicate_utf8 (subprocess, NULL, NULL, &stdout_buf, NULL, NULL))
    return NULL;

  if (stdout_buf != NULL)
    g_strstrip (stdout_buf);

  return g_steal_pointer (&stdout_buf);
}

static DexFuture *
gbp_jhbuild_runtime_provider_load (IdeRuntimeProvider *provider)
{
  g_autofree char *jhbuild_bin = NULL;
  g_autofree char *jhbuild_prefix = NULL;
  g_autoptr(IdeRuntime) runtime = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_JHBUILD_RUNTIME_PROVIDER (provider));

  if (!(jhbuild_bin = get_jhbuild_path ()))
    {
      g_debug ("jhbuild not found within path, ignoring");
      goto finished;
    }

  if (!(jhbuild_prefix = get_jhbuild_prefix (jhbuild_bin)))
    {
      g_debug ("jhbuild installation not complete, ignoring");
      goto finished;
    }

  runtime = g_object_new (GBP_TYPE_JHBUILD_RUNTIME,
                          "id", "jhbuild",
                          "category", _("Host System"),
                          "display-name", "JHBuild",
                          "executable-path", jhbuild_bin,
                          "install-prefix", jhbuild_prefix,
                          NULL);
  ide_runtime_provider_add (provider, runtime);

finished:
  IDE_RETURN (dex_future_new_for_boolean (TRUE));
}

static gboolean
gbp_jhbuild_runtime_provider_provides (IdeRuntimeProvider *provider,
                                       const char         *runtime_id)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_JHBUILD_RUNTIME_PROVIDER (provider));
  g_assert (runtime_id != NULL);

  return ide_str_equal0 (runtime_id, "jhbuild");
}

G_DEFINE_FINAL_TYPE (GbpJhbuildRuntimeProvider, gbp_jhbuild_runtime_provider, IDE_TYPE_RUNTIME_PROVIDER)

static void
gbp_jhbuild_runtime_provider_class_init (GbpJhbuildRuntimeProviderClass *klass)
{
  IdeRuntimeProviderClass *runtime_provider_class = IDE_RUNTIME_PROVIDER_CLASS (klass);

  runtime_provider_class->load = gbp_jhbuild_runtime_provider_load;
  runtime_provider_class->provides = gbp_jhbuild_runtime_provider_provides;
}

static void
gbp_jhbuild_runtime_provider_init (GbpJhbuildRuntimeProvider *self)
{
}
