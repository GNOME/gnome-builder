/* ide-plugin.c
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

#define G_LOG_DOMAIN "ide-plugin"

#include "config.h"

#include "ide-plugin.h"

struct _IdePlugin
{
  GObject parent_object;
  PeasPluginInfo *info;
};

enum {
  PROP_0,
  PROP_DESCRIPTION,
  PROP_INFO,
  PROP_NAME,
  PROP_SECTION,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdePlugin, ide_plugin, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
ide_plugin_dispose (GObject *object)
{
  IdePlugin *self = (IdePlugin *)object;

  if (self->info != NULL)
    {
      g_boxed_free (PEAS_TYPE_PLUGIN_INFO, self->info);
      self->info = NULL;
    }

  G_OBJECT_CLASS (ide_plugin_parent_class)->dispose (object);
}

static void
ide_plugin_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  IdePlugin *self = IDE_PLUGIN (object);

  switch (prop_id)
    {
    case PROP_INFO:
      g_value_set_boxed (value, self->info);
      break;

    case PROP_NAME:
      g_value_set_string (value, ide_plugin_get_name (self));
      break;

    case PROP_DESCRIPTION:
      g_value_set_string (value, ide_plugin_get_description (self));
      break;

    case PROP_SECTION:
      g_value_set_string (value, ide_plugin_get_section (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_plugin_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  IdePlugin *self = IDE_PLUGIN (object);

  switch (prop_id)
    {
    case PROP_INFO:
      self->info = g_value_dup_boxed (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_plugin_class_init (IdePluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_plugin_dispose;
  object_class->get_property = ide_plugin_get_property;
  object_class->set_property = ide_plugin_set_property;

  properties[PROP_INFO] =
    g_param_spec_boxed ("info", NULL, NULL,
                         PEAS_TYPE_PLUGIN_INFO,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties[PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_DESCRIPTION] =
    g_param_spec_string ("description", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_SECTION] =
    g_param_spec_string ("section", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_plugin_init (IdePlugin *self)
{
}

/**
 * ide_plugin_get_info:
 * @self: a #IdePlugin
 *
 * Get the underlying #PeasPluginInfo.
 *
 * Returns: (transfer none): a #PeasPluginInfo
 */
PeasPluginInfo *
ide_plugin_get_info (IdePlugin *self)
{
  g_return_val_if_fail (IDE_IS_PLUGIN (self), NULL);

  return self->info;
}

const char *
ide_plugin_get_name (IdePlugin *self)
{
  g_return_val_if_fail (IDE_IS_PLUGIN (self), NULL);

  return peas_plugin_info_get_name (self->info);
}

const char *
ide_plugin_get_description (IdePlugin *self)
{
  g_return_val_if_fail (IDE_IS_PLUGIN (self), NULL);

  return peas_plugin_info_get_description (self->info);
}

const char *
ide_plugin_get_section (IdePlugin *self)
{
  g_return_val_if_fail (IDE_IS_PLUGIN (self), NULL);

  return peas_plugin_info_get_external_data (self->info, "Section");
}
