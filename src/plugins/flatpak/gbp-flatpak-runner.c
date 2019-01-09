/* gbp-flatpak-runner.c
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

#define G_LOG_DOMAIN "gbp-flatpak-runner"

#include <errno.h>
#include <stdlib.h>
#include <glib/gi18n.h>
#include <unistd.h>

#include "gbp-flatpak-manifest.h"
#include "gbp-flatpak-runner.h"
#include "gbp-flatpak-util.h"

struct _GbpFlatpakRunner
{
  IdeRunner parent_instance;

  gchar *build_path;
  gchar *binary_path;
};

G_DEFINE_TYPE (GbpFlatpakRunner, gbp_flatpak_runner, IDE_TYPE_RUNNER)

static IdeSubprocessLauncher *
gbp_flatpak_runner_create_launcher (IdeRunner *runner)
{
  const gchar *cwd = ide_runner_get_cwd (runner);

  return g_object_new (IDE_TYPE_SUBPROCESS_LAUNCHER,
                       "flags", 0,
                       "cwd", cwd,
                       NULL);
}

static void
gbp_flatpak_runner_fixup_launcher (IdeRunner             *runner,
                                   IdeSubprocessLauncher *launcher)
{
  GbpFlatpakRunner *self = (GbpFlatpakRunner *)runner;
  g_autofree gchar *doc_portal = NULL;
  IdeConfigurationManager *config_manager;
  IdeConfiguration *config;
  IdeEnvironment *env;
  g_auto(GStrv) environ_ = NULL;
  const gchar *app_id;
  IdeContext *context;
  guint i = 0;

  g_assert (GBP_IS_FLATPAK_RUNNER (runner));
  g_assert (IDE_IS_SUBPROCESS_LAUNCHER (launcher));

  context = ide_object_get_context (IDE_OBJECT (self));
  config_manager = ide_configuration_manager_from_context (context);
  config = ide_configuration_manager_get_current (config_manager);
  app_id = ide_configuration_get_app_id (config);

  doc_portal = g_strdup_printf ("--bind-mount=/run/user/%u/doc=/run/user/%u/doc/by-app/%s",
                                getuid (), getuid (), app_id);

  ide_subprocess_launcher_insert_argv (launcher, i++, "flatpak");
  ide_subprocess_launcher_insert_argv (launcher, i++, "build");
  ide_subprocess_launcher_insert_argv (launcher, i++, "--with-appdir");
  ide_subprocess_launcher_insert_argv (launcher, i++, "--allow=devel");
  ide_subprocess_launcher_insert_argv (launcher, i++, doc_portal);

  if (GBP_IS_FLATPAK_MANIFEST (config))
    {
      const gchar * const *finish_args;

      /*
       * We cannot rely on flatpak-builder --run because it filters out
       * some finish-args that we really care about. (Primarily so that
       * it can preserve the integrity of the build results). So instead,
       * we just mimic what it would do for us and ensure we pass along
       * all the finish-args we know about.
       */

      finish_args = gbp_flatpak_manifest_get_finish_args (GBP_FLATPAK_MANIFEST (config));

      if (finish_args != NULL)
        {
          for (guint j = 0; finish_args[j] != NULL; j++)
            {
              const gchar *arg = finish_args[j];

              if (g_str_has_prefix (arg, "--allow") ||
                  g_str_has_prefix (arg, "--share") ||
                  g_str_has_prefix (arg, "--socket") ||
                  g_str_has_prefix (arg, "--filesystem") ||
                  g_str_has_prefix (arg, "--device") ||
                  g_str_has_prefix (arg, "--env") ||
                  g_str_has_prefix (arg, "--system-talk") ||
                  g_str_has_prefix (arg, "--own-name") ||
                  g_str_has_prefix (arg, "--talk-name"))
                ide_subprocess_launcher_insert_argv (launcher, i++, arg);
            }
        }
    }
  else
    {
      ide_subprocess_launcher_insert_argv (launcher, i++, "--share=ipc");
      ide_subprocess_launcher_insert_argv (launcher, i++, "--share=network");
      ide_subprocess_launcher_insert_argv (launcher, i++, "--socket=x11");
      ide_subprocess_launcher_insert_argv (launcher, i++, "--socket=wayland");
    }

  ide_subprocess_launcher_insert_argv (launcher, i++, "--talk-name=org.freedesktop.portal.*");

  /* Proxy environment stuff to the launcher */
  if ((env = ide_runner_get_environment (runner)) &&
      (environ_ = ide_environment_get_environ (env)))
    {
      for (guint j = 0; environ_[j]; j++)
        {
          g_autofree gchar *arg = g_strdup_printf ("--env=%s", environ_[j]);
          ide_subprocess_launcher_insert_argv (launcher, i++, arg);
        }
    }

  /* Disable G_MESSAGES_DEBUG as it could cause 'flatpak build' to spew info
   * and mess up systems that need a clean stdin/stdout/stderr.
   */
  ide_subprocess_launcher_setenv (launcher, "G_MESSAGES_DEBUG", NULL, TRUE);

  ide_subprocess_launcher_insert_argv (launcher, i++, self->build_path);
}

GbpFlatpakRunner *
gbp_flatpak_runner_new (IdeContext  *context,
                        const gchar *build_path,
                        const gchar *binary_path)
{
  GbpFlatpakRunner *self;

  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  self = g_object_new (GBP_TYPE_FLATPAK_RUNNER, NULL);

  if (binary_path != NULL)
    ide_runner_append_argv (IDE_RUNNER (self), binary_path);

  self->build_path = g_strdup (build_path);
  self->binary_path = g_strdup (binary_path);

  return self;
}

static void
gbp_flatpak_runner_finalize (GObject *object)
{
  GbpFlatpakRunner *self = (GbpFlatpakRunner *)object;

  g_clear_pointer (&self->build_path, g_free);
  g_clear_pointer (&self->binary_path, g_free);

  G_OBJECT_CLASS (gbp_flatpak_runner_parent_class)->finalize (object);
}

static void
gbp_flatpak_runner_class_init (GbpFlatpakRunnerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeRunnerClass *runner_class = IDE_RUNNER_CLASS (klass);

  object_class->finalize = gbp_flatpak_runner_finalize;

  runner_class->create_launcher = gbp_flatpak_runner_create_launcher;
  runner_class->fixup_launcher = gbp_flatpak_runner_fixup_launcher;
}

static void
gbp_flatpak_runner_init (GbpFlatpakRunner *self)
{
  ide_runner_set_run_on_host (IDE_RUNNER (self), TRUE);
  ide_runner_set_clear_env (IDE_RUNNER (self), FALSE);
}
