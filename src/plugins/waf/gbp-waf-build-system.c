/* gbp-waf-build-system.c
 *
 * Copyright 2019 Alex Mitchell
 * Copyright 2016-2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-waf-build-system"

#include "config.h"

#include "gbp-waf-build-system.h"

struct _GbpWafBuildSystem
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
gbp_waf_build_system_get_id (IdeBuildSystem *build_system)
{
  return g_strdup ("waf");
}

static char *
gbp_waf_build_system_get_display_name (IdeBuildSystem *build_system)
{
  return g_strdup ("Waf");
}

static int
gbp_waf_build_system_get_priority (IdeBuildSystem *build_system)
{
  return 1000;
}

static void
build_system_iface_init (IdeBuildSystemInterface *iface)
{
  iface->get_id = gbp_waf_build_system_get_id;
  iface->get_display_name = gbp_waf_build_system_get_display_name;
  iface->get_priority = gbp_waf_build_system_get_priority;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpWafBuildSystem, gbp_waf_build_system, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_SYSTEM, build_system_iface_init))

static GParamSpec *properties [N_PROPS];

static void
gbp_waf_build_system_destroy (IdeObject *object)
{
  GbpWafBuildSystem *self = (GbpWafBuildSystem *)object;

  g_clear_object (&self->project_file);

  IDE_OBJECT_CLASS (gbp_waf_build_system_parent_class)->destroy (object);
}

static void
gbp_waf_build_system_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GbpWafBuildSystem *self = GBP_WAF_BUILD_SYSTEM (object);

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
gbp_waf_build_system_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GbpWafBuildSystem *self = GBP_WAF_BUILD_SYSTEM (object);

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
gbp_waf_build_system_class_init (GbpWafBuildSystemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  object_class->get_property = gbp_waf_build_system_get_property;
  object_class->set_property = gbp_waf_build_system_set_property;

  i_object_class->destroy = gbp_waf_build_system_destroy;

  properties [PROP_PROJECT_FILE] =
    g_param_spec_object ("project-file",
                         "Project File",
                         "The project file (Waf.toml)",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_waf_build_system_init (GbpWafBuildSystem *self)
{
}

gboolean
gbp_waf_build_system_wants_python2 (GbpWafBuildSystem  *self,
                                    GError            **error)
{
  g_autofree char *contents = NULL;
  IdeLineReader reader;
  char *line;
  gsize line_len;
  gsize len = 0;

  g_return_val_if_fail (GBP_IS_WAF_BUILD_SYSTEM (self), FALSE);
  g_return_val_if_fail (self->project_file != NULL, FALSE);

  if (!g_file_load_contents (self->project_file, NULL, &contents, &len, NULL, error))
    return TRUE;

  ide_line_reader_init (&reader, contents, len);
  if ((line = ide_line_reader_next (&reader, &line_len)))
    {
      line[line_len] = 0;

      if (strstr (line, "python3") != NULL)
        return FALSE;
    }

  return TRUE;
}

char *
gbp_waf_build_system_locate_waf (GbpWafBuildSystem *self)
{
  g_autoptr(GFile) parent = NULL;
  g_autoptr(GFile) waf = NULL;

  g_return_val_if_fail (GBP_IS_WAF_BUILD_SYSTEM (self), NULL);
  g_return_val_if_fail (G_IS_FILE (self->project_file), NULL);

  if (g_file_query_file_type (self->project_file, 0, NULL) == G_FILE_TYPE_DIRECTORY)
    parent = g_object_ref (self->project_file);
  else
    parent = g_file_get_parent (self->project_file);

  waf = g_file_get_child (parent, "waf");

  if (g_file_query_exists (waf, NULL))
    return g_file_get_path (waf);

  return g_strdup ("waf");
}
