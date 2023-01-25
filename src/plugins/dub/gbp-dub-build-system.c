/* gbp-dub-build-system.c
 *
 * Copyright 2022 Steven Oliver <oliver.steven@gmailcom>
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

#define G_LOG_DOMAIN "gbp-dub-build-system"
#define DUB "dub"

#include "config.h"

#include "gbp-dub-build-system.h"

struct _GbpDubBuildSystem
{
  IdeObject  parent_instance;
  GFile     *project_file;
};

enum {
  PROP_0,
  PROP_PROJECT_FILE,
  N_PROPS
};

static char *
gbp_dub_build_system_get_id (IdeBuildSystem *build_system)
{
  return g_strdup (DUB);
}

static char *
gbp_dub_build_system_get_display_name (IdeBuildSystem *build_system)
{
  return g_strdup ("dub");
}

static int
gbp_dub_build_system_get_priority (IdeBuildSystem *build_system)
{
  return -200;
}

static gboolean
gbp_dub_build_system_supports_language (IdeBuildSystem *build_system,
                                        const char     *language)
{
  return g_strv_contains (IDE_STRV_INIT ("d"), language);
}

static void
build_system_iface_init (IdeBuildSystemInterface *iface)
{
  iface->get_id = gbp_dub_build_system_get_id;
  iface->get_display_name = gbp_dub_build_system_get_display_name;
  iface->get_priority = gbp_dub_build_system_get_priority;
  iface->supports_language = gbp_dub_build_system_supports_language;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpDubBuildSystem, gbp_dub_build_system, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_SYSTEM, build_system_iface_init))

static GParamSpec *properties [N_PROPS];

static void
gbp_dub_build_system_destroy (IdeObject *object)
{
  GbpDubBuildSystem *self = (GbpDubBuildSystem *)object;

  g_clear_object (&self->project_file);

  IDE_OBJECT_CLASS (gbp_dub_build_system_parent_class)->destroy (object);
}

static void
gbp_dub_build_system_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GbpDubBuildSystem *self = GBP_DUB_BUILD_SYSTEM (object);

  switch (prop_id)
    {
    case PROP_PROJECT_FILE:
      g_value_set_object (value, self->project_file);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_dub_build_system_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GbpDubBuildSystem *self = GBP_DUB_BUILD_SYSTEM (object);

  switch (prop_id)
    {
    case PROP_PROJECT_FILE:
      g_set_object (&self->project_file, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_dub_build_system_class_init (GbpDubBuildSystemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  object_class->get_property = gbp_dub_build_system_get_property;
  object_class->set_property = gbp_dub_build_system_set_property;

  i_object_class->destroy = gbp_dub_build_system_destroy;

  properties [PROP_PROJECT_FILE] =
    g_param_spec_object ("project-file",
                         "Project File",
                         "The project file (dub.json)",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_dub_build_system_init (GbpDubBuildSystem *self)
{
}

char *
gbp_dub_build_system_get_project_dir (GbpDubBuildSystem *self)
{
  g_autoptr(GFile) workdir = NULL;
  g_autofree char *base = NULL;
  IdeContext *context;

  g_return_val_if_fail (GBP_IS_DUB_BUILD_SYSTEM (self), NULL);

  context = ide_object_get_context (IDE_OBJECT (self));
  workdir = ide_context_ref_workdir (context);

  if (self->project_file == NULL)
    return g_strdup (g_file_peek_path (workdir));

  base = g_file_get_basename (self->project_file);

  if (strcasecmp (base, "dub.json") == 0)
    {
      g_autoptr(GFile) parent = g_file_get_parent (self->project_file);
      return g_file_get_path (parent);
    }

  return g_file_get_path (self->project_file);
}

char *
gbp_dub_build_system_get_dub_json_path (GbpDubBuildSystem *self)
{
  g_autofree char *base = NULL;
  g_autoptr(GFile) child = NULL;

  g_return_val_if_fail (GBP_IS_DUB_BUILD_SYSTEM (self), NULL);

  base = g_file_get_basename (self->project_file);
  if (strcasecmp (base, "dub.json") == 0)
    return g_file_get_path (self->project_file);

  child = g_file_get_child (self->project_file, "dub.json");
  return g_file_get_path (child);
}

/**
 * gbp_dub_build_system_locate_dub:
 * @self: (nullable): a #GbpDubBuildSystem or %NULL
 * @pipeline: (nullable): an #IdePipeline or %NULL
 * @config: (nullable): an #IdeConfig or %NULL
 *
 * Currently, @self may be %NULL but is kept around so that in
 * the future we may have other fallbacks which could take use
 * of the build system.
 *
 * This function will first check for "DUB" in @config's environment
 * variables. If specified, that will be used.
 *
 * Then the config's runtime+sdk-extensions will be checked and if it
 * contains "dub" in the pipeline's $PATH, that will be used.
 *
 * Lastly, nothing was found, so "dub" will be used with the hope
 * that something, somewhere, will find it when executing.
 *
 * Returns: (transfer full) (not nullable): a path to a dub program
 *   or "dub" in the case that a specific path was not found.
 */
char *
gbp_dub_build_system_locate_dub (GbpDubBuildSystem *self,
                                 IdePipeline       *pipeline,
                                 IdeConfig         *config)
{
  const char *envvar;

  g_return_val_if_fail (!self || GBP_IS_DUB_BUILD_SYSTEM (self), NULL);
  g_return_val_if_fail (!pipeline || IDE_IS_PIPELINE (pipeline), NULL);
  g_return_val_if_fail (!config || IDE_IS_CONFIG (config), NULL);

  /* First check DUB=path override in IdeConfig */
  if (config != NULL)
    {
      if ((envvar = ide_config_getenv (config, "DUB")))
        return g_strdup (envvar);
    }

  /* Next see if the pipeline or one of it's extensions has Dub */
  if (pipeline != NULL)
    {
      if (ide_pipeline_contains_program_in_path (pipeline, DUB, NULL))
        return g_strdup (DUB);
    }

  /* Fallback to "dub" and hope for the best */
  return g_strdup (DUB);
}
