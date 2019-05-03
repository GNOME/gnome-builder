/* gbp-vagrant-runtime.c
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

#define G_LOG_DOMAIN "gbp-vagrant-runtime"

#include "config.h"

#include "gbp-vagrant-runtime.h"
#include "gbp-vagrant-subprocess-launcher.h"

struct _GbpVagrantRuntime
{
  IdeRuntime parent_instance;
  gchar *vagrant_id;
  gchar *provider;
  gchar *state;
};

enum {
  PROP_0,
  PROP_PROVIDER,
  PROP_STATE,
  PROP_VAGRANT_ID,
  N_PROPS
};

G_DEFINE_TYPE (GbpVagrantRuntime, gbp_vagrant_runtime, IDE_TYPE_RUNTIME)

static GParamSpec *properties [N_PROPS];

static IdeSubprocessLauncher *
gbp_vagrant_runtime_create_launcher (IdeRuntime  *runtime,
                                     GError     **error)
{
  GbpVagrantRuntime *self = (GbpVagrantRuntime *)runtime;
  IdeSubprocessLauncher *launcher;
  g_autoptr(GFile) workdir = NULL;
  IdeContext *context;

  g_assert (GBP_IS_VAGRANT_RUNTIME (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  workdir = ide_context_ref_workdir (context);

  launcher = gbp_vagrant_subprocess_launcher_new (g_file_peek_path (workdir));

  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);

  ide_subprocess_launcher_push_argv (launcher, "vagrant");
  ide_subprocess_launcher_push_argv (launcher, "ssh");
  ide_subprocess_launcher_push_argv (launcher, self->vagrant_id);
  ide_subprocess_launcher_push_argv (launcher, GBP_VAGRANT_SUBPROCESS_LAUNCHER_C_OPT);

  return g_steal_pointer (&launcher);
}

static void
gbp_vagrant_runtime_finalize (GObject *object)
{
  GbpVagrantRuntime *self = (GbpVagrantRuntime *)object;

  g_clear_pointer (&self->provider, g_free);
  g_clear_pointer (&self->state, g_free);
  g_clear_pointer (&self->vagrant_id, g_free);

  G_OBJECT_CLASS (gbp_vagrant_runtime_parent_class)->finalize (object);
}

static void
gbp_vagrant_runtime_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GbpVagrantRuntime *self = GBP_VAGRANT_RUNTIME (object);

  switch (prop_id)
    {
    case PROP_PROVIDER:
      g_value_set_string (value, gbp_vagrant_runtime_get_provider (self));
      break;

    case PROP_STATE:
      g_value_set_string (value, gbp_vagrant_runtime_get_state (self));
      break;

    case PROP_VAGRANT_ID:
      g_value_set_string (value, gbp_vagrant_runtime_get_vagrant_id (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_vagrant_runtime_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GbpVagrantRuntime *self = GBP_VAGRANT_RUNTIME (object);

  switch (prop_id)
    {
    case PROP_PROVIDER:
      gbp_vagrant_runtime_set_provider (self, g_value_get_string (value));
      break;

    case PROP_STATE:
      gbp_vagrant_runtime_set_state (self, g_value_get_string (value));
      break;

    case PROP_VAGRANT_ID:
      self->vagrant_id = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_vagrant_runtime_class_init (GbpVagrantRuntimeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeRuntimeClass *runtime_class = IDE_RUNTIME_CLASS (klass);

  object_class->finalize = gbp_vagrant_runtime_finalize;
  object_class->get_property = gbp_vagrant_runtime_get_property;
  object_class->set_property = gbp_vagrant_runtime_set_property;

  runtime_class->create_launcher = gbp_vagrant_runtime_create_launcher;

  properties [PROP_PROVIDER] =
    g_param_spec_string ("provider",
                         "Provider",
                         "Provider",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_STATE] =
    g_param_spec_string ("state",
                         "State",
                         "State",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_VAGRANT_ID] =
    g_param_spec_string ("vagrant-id",
                         "Vagrant Id",
                         "Vagrant Id",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_vagrant_runtime_init (GbpVagrantRuntime *self)
{
}

const gchar *
gbp_vagrant_runtime_get_vagrant_id (GbpVagrantRuntime *self)
{
  g_return_val_if_fail (GBP_IS_VAGRANT_RUNTIME (self), NULL);

  return self->vagrant_id;
}

const gchar *
gbp_vagrant_runtime_get_provider (GbpVagrantRuntime *self)
{
  g_return_val_if_fail (GBP_IS_VAGRANT_RUNTIME (self), NULL);

  return self->provider;
}

void
gbp_vagrant_runtime_set_provider (GbpVagrantRuntime *self,
                                  const gchar       *provider)
{
  g_return_if_fail (GBP_IS_VAGRANT_RUNTIME (self));

  if (!ide_str_equal0 (provider, self->provider))
    {
      g_free (self->provider);
      self->provider = g_strdup (provider);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PROVIDER]);
    }
}

const gchar *
gbp_vagrant_runtime_get_state (GbpVagrantRuntime *self)
{
  g_return_val_if_fail (GBP_IS_VAGRANT_RUNTIME (self), NULL);

  return self->state;
}

void
gbp_vagrant_runtime_set_state (GbpVagrantRuntime *self,
                               const gchar       *state)
{
  g_return_if_fail (GBP_IS_VAGRANT_RUNTIME (self));

  if (!ide_str_equal0 (state, self->state))
    {
      g_free (self->state);
      self->state = g_strdup (state);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_STATE]);
    }
}
