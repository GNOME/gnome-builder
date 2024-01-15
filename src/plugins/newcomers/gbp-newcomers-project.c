/* gbp-newcomers-project.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-newcomers-project"

#include "config.h"

#include "gbp-newcomers-project.h"

struct _GbpNewcomersProject
{
  IdeGreeterRow    parent_instance;

  IdeProjectInfo  *project_info;

  GtkLabel        *label;
  GtkImage        *icon;
  GtkBox          *tags_box;
};

enum {
  PROP_0,
  PROP_DESCRIPTION,
  PROP_ICON_NAME,
  PROP_LANGUAGES,
  PROP_NAME,
  PROP_URI,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpNewcomersProject, gbp_newcomers_project, IDE_TYPE_GREETER_ROW)

static GParamSpec *properties [N_PROPS];

static void
gbp_newcomers_project_constructed (GObject *object)
{
  GbpNewcomersProject *self = (GbpNewcomersProject *)object;

  G_OBJECT_CLASS (gbp_newcomers_project_parent_class)->constructed (object);

  ide_greeter_row_set_project_info (IDE_GREETER_ROW (self), self->project_info);
}

static void
gbp_newcomers_project_dispose (GObject *object)
{
  GbpNewcomersProject *self = (GbpNewcomersProject *)object;

  g_clear_object (&self->project_info);

  G_OBJECT_CLASS (gbp_newcomers_project_parent_class)->dispose (object);
}

static void
gbp_newcomers_project_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GbpNewcomersProject *self = GBP_NEWCOMERS_PROJECT (object);

  switch (prop_id)
    {
    case PROP_URI:
      g_value_set_string (value, gbp_newcomers_project_get_uri (self));
      break;

    case PROP_NAME:
      g_value_set_string (value, gbp_newcomers_project_get_name (self));
      break;

    case PROP_DESCRIPTION:
      g_value_set_string (value, ide_project_info_get_description (self->project_info));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_newcomers_project_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GbpNewcomersProject *self = GBP_NEWCOMERS_PROJECT (object);

  switch (prop_id)
    {
    case PROP_URI:
      ide_project_info_set_vcs_uri (self->project_info, g_value_get_string (value));
      break;

    case PROP_LANGUAGES:
      ide_project_info_set_languages (self->project_info, g_value_get_boxed (value));
      break;

    case PROP_DESCRIPTION:
      ide_project_info_set_description (self->project_info, g_value_get_string (value));
      break;

    case PROP_NAME:
      ide_project_info_set_name (self->project_info, g_value_get_string (value));
      break;

    case PROP_ICON_NAME:
      ide_project_info_set_icon_name (self->project_info, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_newcomers_project_class_init (GbpNewcomersProjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gbp_newcomers_project_constructed;
  object_class->dispose = gbp_newcomers_project_dispose;
  object_class->get_property = gbp_newcomers_project_get_property;
  object_class->set_property = gbp_newcomers_project_set_property;

  properties [PROP_ICON_NAME] =
    g_param_spec_string ("icon-name",
                         "Icon Name",
                         "The icon to load",
                         NULL,
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "The name of the newcomer project",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_DESCRIPTION] =
    g_param_spec_string ("description",
                         "Description",
                         "The description of the newcomer project",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_LANGUAGES] =
    g_param_spec_boxed ("languages",
                        "Languages",
                        "The programming languages of the newcomer project",
                        G_TYPE_STRV,
                        (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_URI] =
    g_param_spec_string ("uri",
                         "Uri",
                         "The URL of the project's source code repository",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

}

static void
gbp_newcomers_project_init (GbpNewcomersProject *self)
{
  self->project_info = ide_project_info_new ();
}

const char *
gbp_newcomers_project_get_name (GbpNewcomersProject *self)
{
  g_return_val_if_fail (GBP_IS_NEWCOMERS_PROJECT (self), NULL);

  return ide_project_info_get_name (self->project_info);
}

const char *
gbp_newcomers_project_get_uri (GbpNewcomersProject *self)
{
  g_return_val_if_fail (GBP_IS_NEWCOMERS_PROJECT (self), NULL);

  return ide_project_info_get_vcs_uri (self->project_info);
}

const char * const *
gbp_newcomers_project_get_languages (GbpNewcomersProject *self)
{
  g_return_val_if_fail (GBP_IS_NEWCOMERS_PROJECT (self), NULL);

  return ide_project_info_get_languages (self->project_info);
}
