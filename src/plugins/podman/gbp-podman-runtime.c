/* gbp-podman-runtime.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-podman-runtime"

#include "config.h"

#include <glib/gi18n.h>

#include "gbp-podman-runtime.h"
#include "gbp-podman-subprocess-launcher.h"

struct _GbpPodmanRuntime
{
  IdeRuntime  parent_instance;
  JsonObject *object;
  guint       has_started : 1;
};

G_DEFINE_TYPE (GbpPodmanRuntime, gbp_podman_runtime, IDE_TYPE_RUNTIME)

static void
maybe_start (GbpPodmanRuntime *self)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  const gchar *id;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_PODMAN_RUNTIME (self));

  if (self->has_started)
    return;

  if (!(id = json_object_get_string_member (self->object, "ID")))
    return;

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDERR_SILENCE |
                                          G_SUBPROCESS_FLAGS_STDOUT_SILENCE);
  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
  ide_subprocess_launcher_push_argv (launcher, "podman");
  ide_subprocess_launcher_push_argv (launcher, "start");
  ide_subprocess_launcher_push_argv (launcher, id);

  if ((subprocess = ide_subprocess_launcher_spawn (launcher, NULL, NULL)))
    {
      ide_subprocess_wait_async (subprocess, NULL, NULL, NULL);
      self->has_started = TRUE;
    }
}

static IdeSubprocessLauncher *
gbp_podman_runtime_create_launcher (IdeRuntime  *runtime,
                                    GError     **error)
{
  GbpPodmanRuntime *self = (GbpPodmanRuntime *)runtime;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_PODMAN_RUNTIME (self));

  maybe_start (self);

  return g_object_new (GBP_TYPE_PODMAN_SUBPROCESS_LAUNCHER,
                       "id", json_object_get_string_member (self->object, "ID"),
                       "run-on-host", TRUE,
                       NULL);
}

static void
gbp_podman_runtime_destroy (IdeObject *object)
{
  GbpPodmanRuntime *self = (GbpPodmanRuntime *)object;

  g_clear_pointer (&self->object, json_object_unref);

  IDE_OBJECT_CLASS (gbp_podman_runtime_parent_class)->destroy (object);
}

static void
gbp_podman_runtime_class_init (GbpPodmanRuntimeClass *klass)
{
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);
  IdeRuntimeClass *runtime_class = IDE_RUNTIME_CLASS (klass);

  i_object_class->destroy = gbp_podman_runtime_destroy;

  runtime_class->create_launcher = gbp_podman_runtime_create_launcher;
}

static void
gbp_podman_runtime_init (GbpPodmanRuntime *self)
{
}

GbpPodmanRuntime *
gbp_podman_runtime_new (JsonObject *object)
{
  g_autofree gchar *full_id = NULL;
  g_autofree gchar *name = NULL;
  GbpPodmanRuntime *self;
  const gchar *id;
  const gchar *names;

  g_return_val_if_fail (object != NULL, NULL);

  id = json_object_get_string_member (object, "ID");
  names = json_object_get_string_member (object, "Names");
  full_id = g_strdup_printf ("podman:%s", id);
  name = g_strdup_printf ("%s %s", _("Podman"), names);

  g_return_val_if_fail (id != NULL, NULL);
  g_return_val_if_fail (names != NULL, NULL);

  self = g_object_new (GBP_TYPE_PODMAN_RUNTIME,
                       "id", full_id,
                       /* translators: this is a path to browse to the runtime, likely only "containers" should be translated */
                       "category", _("Containers/Podman"),
                       "display-name", names,
                       NULL);
  self->object = json_object_ref (object);

  return g_steal_pointer (&self);
}
