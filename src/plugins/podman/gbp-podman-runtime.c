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
  gchar      *id;
  GMutex      mutex;
  guint       has_started : 1;
};

G_DEFINE_FINAL_TYPE (GbpPodmanRuntime, gbp_podman_runtime, IDE_TYPE_RUNTIME)

static void
maybe_start (GbpPodmanRuntime *self)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;

  g_assert (GBP_IS_PODMAN_RUNTIME (self));
  g_assert (self->id != NULL);

  if (self->has_started)
    return;

  g_mutex_lock (&self->mutex);

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDERR_SILENCE |
                                          G_SUBPROCESS_FLAGS_STDOUT_SILENCE);
  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
  ide_subprocess_launcher_push_argv (launcher, "podman");
  ide_subprocess_launcher_push_argv (launcher, "start");
  ide_subprocess_launcher_push_argv (launcher, self->id);

  if ((subprocess = ide_subprocess_launcher_spawn (launcher, NULL, NULL)))
    {
      ide_subprocess_wait_async (subprocess, NULL, NULL, NULL);
      self->has_started = TRUE;
    }

  g_mutex_unlock (&self->mutex);
}

static IdeSubprocessLauncher *
gbp_podman_runtime_create_launcher (IdeRuntime  *runtime,
                                    GError     **error)
{
  GbpPodmanRuntime *self = (GbpPodmanRuntime *)runtime;
  IdeSubprocessLauncher *launcher;

  g_assert (GBP_IS_PODMAN_RUNTIME (self));
  g_assert (self->id != NULL);

  maybe_start (self);

  launcher = g_object_new (GBP_TYPE_PODMAN_SUBPROCESS_LAUNCHER,
                           "id", self->id,
                           NULL);

  return launcher;
}

static void
gbp_podman_runtime_destroy (IdeObject *object)
{
  GbpPodmanRuntime *self = (GbpPodmanRuntime *)object;

  g_clear_pointer (&self->object, json_object_unref);
  g_clear_pointer (&self->id, g_free);

  IDE_OBJECT_CLASS (gbp_podman_runtime_parent_class)->destroy (object);
}

static void
gbp_podman_runtime_finalize (GObject *object)
{
  GbpPodmanRuntime *self = (GbpPodmanRuntime *)object;

  g_clear_pointer (&self->id, g_free);
  g_mutex_clear (&self->mutex);

  G_OBJECT_CLASS (gbp_podman_runtime_parent_class)->finalize (object);
}

static void
gbp_podman_runtime_class_init (GbpPodmanRuntimeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);
  IdeRuntimeClass *runtime_class = IDE_RUNTIME_CLASS (klass);

  object_class->finalize = gbp_podman_runtime_finalize;

  i_object_class->destroy = gbp_podman_runtime_destroy;

  runtime_class->create_launcher = gbp_podman_runtime_create_launcher;
  runtime_class->translate_file = NULL;
}

static void
gbp_podman_runtime_init (GbpPodmanRuntime *self)
{
  g_mutex_init (&self->mutex);
}

GbpPodmanRuntime *
gbp_podman_runtime_new (JsonObject *object)
{
  g_autofree gchar *full_id = NULL;
  g_autofree gchar *name = NULL;
  GbpPodmanRuntime *self;
  const gchar *id;
  const gchar *names;
  JsonArray *names_arr;
  JsonNode *names_node;
  JsonNode *labels_node;
  gboolean is_toolbox = FALSE;
  const gchar *category;

  g_return_val_if_fail (object != NULL, NULL);

  if (json_object_has_member (object, "ID"))
    id = json_object_get_string_member (object, "ID");
  else
    id = json_object_get_string_member (object, "Id");

  names_node = json_object_get_member (object, "Names");
  if (JSON_NODE_HOLDS_ARRAY (names_node))
    {
      names_arr = json_node_get_array (names_node);
      names = json_array_get_string_element (names_arr, 0);
    }
  else
    {
      names = json_node_get_string (names_node);
    }

  if (json_object_has_member (object, "Labels") &&
      (labels_node = json_object_get_member (object, "Labels")) &&
      JSON_NODE_HOLDS_OBJECT (labels_node))
    {
      JsonObject *labels = json_node_get_object (labels_node);

      /* Check if this is a toolbox container */
      if (json_object_has_member (labels, "com.github.debarshiray.toolbox") ||
          json_object_has_member (labels, "com.github.containers.toolbox"))
        is_toolbox = TRUE;
    }

  full_id = g_strdup_printf ("podman:%s", id);

  if (is_toolbox)
    {
      name = g_strdup_printf ("Toolbox %s", names);
      /* translators: this is a path to browse to the runtime, likely only "containers" should be translated */
      category = _("Containers/Toolbox");
    }
  else
    {
      name = g_strdup_printf ("Podman %s", names);
      /* translators: this is a path to browse to the runtime, likely only "containers" should be translated */
      category = _("Containers/Podman");
    }

  g_return_val_if_fail (id != NULL, NULL);
  g_return_val_if_fail (names != NULL, NULL);

  self = g_object_new (GBP_TYPE_PODMAN_RUNTIME,
                       "id", full_id,
                       "category", category,
                       "display-name", names,
                       NULL);
  self->object = json_object_ref (object);
  self->id = g_strdup (id);

  return g_steal_pointer (&self);
}
