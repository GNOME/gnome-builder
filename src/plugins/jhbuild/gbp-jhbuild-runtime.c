/* gbp-jhbuild-runtime.c
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

#define G_LOG_DOMAIN "gbp-jhbuild-runtime"

#include "config.h"

#include "gbp-jhbuild-runtime.h"

struct _GbpJhbuildRuntime
{
  IdeRuntime runtime;
  char *executable_path;
  char *install_prefix;
};

enum {
  PROP_0,
  PROP_EXECUTABLE_PATH,
  PROP_INSTALL_PREFIX,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpJhbuildRuntime, gbp_jhbuild_runtime, IDE_TYPE_RUNTIME)

static GParamSpec *properties [N_PROPS];

static IdeSubprocessLauncher *
gbp_jhbuild_runtime_create_launcher (IdeRuntime  *runtime,
                                     GError     **error)
{
  GbpJhbuildRuntime *self = (GbpJhbuildRuntime *)runtime;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;

  g_assert (GBP_IS_JHBUILD_RUNTIME (self));

  launcher = IDE_RUNTIME_CLASS (gbp_jhbuild_runtime_parent_class)->create_launcher (runtime, error);

  if (launcher != NULL)
    {
      ide_subprocess_launcher_push_args (launcher, IDE_STRV_INIT (self->executable_path, "run"));
      ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
      ide_subprocess_launcher_set_clear_env (launcher, FALSE);
    }

  return g_steal_pointer (&launcher);
}

static IdeRunner *
gbp_jhbuild_runtime_create_runner (IdeRuntime     *runtime,
                                   IdeBuildTarget *build_target)
{
  GbpJhbuildRuntime *self = (GbpJhbuildRuntime *)runtime;
  g_autoptr(IdeRunner) runner = NULL;

  g_assert (GBP_IS_JHBUILD_RUNTIME (self));
  g_assert (IDE_IS_BUILD_TARGET (build_target));

  runner = IDE_RUNTIME_CLASS (gbp_jhbuild_runtime_parent_class)->create_runner (runtime, build_target);

  if (runner != NULL)
    ide_runner_set_run_on_host (runner, TRUE);

  return g_steal_pointer (&runner);
}

static gboolean
gbp_jhbuild_runtime_contains_program_in_path (IdeRuntime   *runtime,
                                              const char   *program,
                                              GCancellable *cancellable)
{
  GbpJhbuildRuntime *self = (GbpJhbuildRuntime *)runtime;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;

  g_assert (GBP_IS_JHBUILD_RUNTIME (self));
  g_assert (program != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (!(launcher = ide_runtime_create_launcher (runtime, NULL)))
    return FALSE;

  ide_subprocess_launcher_set_flags (launcher,
                                     (G_SUBPROCESS_FLAGS_STDOUT_SILENCE |
                                      G_SUBPROCESS_FLAGS_STDERR_SILENCE));
  ide_subprocess_launcher_push_args (launcher, IDE_STRV_INIT ("which", program));

  if (!(subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, NULL)))
    return FALSE;

  return ide_subprocess_wait_check (subprocess, cancellable, NULL);
}

static void
gbp_jhbuild_runtime_prepare_configuration (IdeRuntime *runtime,
                                           IdeConfig  *config)
{
  GbpJhbuildRuntime *self = (GbpJhbuildRuntime *)runtime;

  g_assert (GBP_IS_JHBUILD_RUNTIME (self));
  g_assert (IDE_IS_CONFIG (config));

  g_object_set (config,
                "prefix", self->install_prefix,
                "prefix-set", FALSE,
                NULL);
}

static void
gbp_jhbuild_runtime_finalize (GObject *object)
{
  GbpJhbuildRuntime *self = (GbpJhbuildRuntime *)object;

  g_clear_pointer (&self->executable_path, g_free);
  g_clear_pointer (&self->install_prefix, g_free);

  G_OBJECT_CLASS (gbp_jhbuild_runtime_parent_class)->finalize (object);
}

static void
gbp_jhbuild_runtime_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GbpJhbuildRuntime *self = GBP_JHBUILD_RUNTIME (object);

  switch (prop_id)
    {
    case PROP_EXECUTABLE_PATH:
      g_value_set_string (value, self->executable_path);
      break;

    case PROP_INSTALL_PREFIX:
      g_value_set_string (value, self->install_prefix);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_jhbuild_runtime_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GbpJhbuildRuntime *self = GBP_JHBUILD_RUNTIME (object);

  switch (prop_id)
    {
    case PROP_EXECUTABLE_PATH:
      self->executable_path = g_value_dup_string (value);
      break;

    case PROP_INSTALL_PREFIX:
      self->install_prefix = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_jhbuild_runtime_class_init (GbpJhbuildRuntimeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeRuntimeClass *runtime_class = IDE_RUNTIME_CLASS (klass);

  object_class->finalize = gbp_jhbuild_runtime_finalize;
  object_class->get_property = gbp_jhbuild_runtime_get_property;
  object_class->set_property = gbp_jhbuild_runtime_set_property;

  runtime_class->contains_program_in_path = gbp_jhbuild_runtime_contains_program_in_path;
  runtime_class->create_launcher = gbp_jhbuild_runtime_create_launcher;
  runtime_class->create_runner = gbp_jhbuild_runtime_create_runner;
  runtime_class->prepare_configuration = gbp_jhbuild_runtime_prepare_configuration;

  properties [PROP_EXECUTABLE_PATH] =
    g_param_spec_string ("executable-path", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_INSTALL_PREFIX] =
    g_param_spec_string ("install-prefix", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_jhbuild_runtime_init (GbpJhbuildRuntime *self)
{
}
