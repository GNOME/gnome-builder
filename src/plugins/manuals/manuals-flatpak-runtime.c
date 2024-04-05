/*
 * manuals-flatpak-runtime.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#include "manuals-flatpak-runtime.h"

struct _ManualsFlatpakRuntime
{
  GObject parent_instance;
  char *name;
  char *arch;
  char *branch;
  char *deploy_dir;
};

enum {
  PROP_0,
  PROP_NAME,
  PROP_ARCH,
  PROP_BRANCH,
  PROP_DEPLOY_DIR,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (ManualsFlatpakRuntime, manuals_flatpak_runtime, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
manuals_flatpak_runtime_finalize (GObject *object)
{
  ManualsFlatpakRuntime *self = (ManualsFlatpakRuntime *)object;

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->arch, g_free);
  g_clear_pointer (&self->branch, g_free);
  g_clear_pointer (&self->deploy_dir, g_free);

  G_OBJECT_CLASS (manuals_flatpak_runtime_parent_class)->finalize (object);
}

static void
manuals_flatpak_runtime_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  ManualsFlatpakRuntime *self = MANUALS_FLATPAK_RUNTIME (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, manuals_flatpak_runtime_get_name (self));
      break;

    case PROP_ARCH:
      g_value_set_string (value, manuals_flatpak_runtime_get_arch (self));
      break;

    case PROP_BRANCH:
      g_value_set_string (value, manuals_flatpak_runtime_get_branch (self));
      break;

    case PROP_DEPLOY_DIR:
      g_value_set_string (value, manuals_flatpak_runtime_get_deploy_dir (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
manuals_flatpak_runtime_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  ManualsFlatpakRuntime *self = MANUALS_FLATPAK_RUNTIME (object);

  switch (prop_id)
    {
    case PROP_NAME:
      self->name = g_value_dup_string (value);
      break;

    case PROP_ARCH:
      self->arch = g_value_dup_string (value);
      break;

    case PROP_BRANCH:
      self->branch = g_value_dup_string (value);
      break;

    case PROP_DEPLOY_DIR:
      self->deploy_dir = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
manuals_flatpak_runtime_class_init (ManualsFlatpakRuntimeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = manuals_flatpak_runtime_finalize;
  object_class->get_property = manuals_flatpak_runtime_get_property;
  object_class->set_property = manuals_flatpak_runtime_set_property;

  properties[PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_ARCH] =
    g_param_spec_string ("arch", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_BRANCH] =
    g_param_spec_string ("branch", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_DEPLOY_DIR] =
    g_param_spec_string ("deploy-dir", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
manuals_flatpak_runtime_init (ManualsFlatpakRuntime *self)
{
}

const char *
manuals_flatpak_runtime_get_name (ManualsFlatpakRuntime *self)
{
  g_return_val_if_fail (MANUALS_IS_FLATPAK_RUNTIME (self), NULL);

  return self->name;
}

const char *
manuals_flatpak_runtime_get_arch (ManualsFlatpakRuntime *self)
{
  g_return_val_if_fail (MANUALS_IS_FLATPAK_RUNTIME (self), NULL);

  return self->arch;
}

const char *
manuals_flatpak_runtime_get_branch (ManualsFlatpakRuntime *self)
{
  g_return_val_if_fail (MANUALS_IS_FLATPAK_RUNTIME (self), NULL);

  return self->branch;
}

const char *
manuals_flatpak_runtime_get_deploy_dir (ManualsFlatpakRuntime *self)
{
  g_return_val_if_fail (MANUALS_IS_FLATPAK_RUNTIME (self), NULL);

  return self->deploy_dir;
}
