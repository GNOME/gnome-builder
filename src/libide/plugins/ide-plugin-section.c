/* ide-plugin-section.c
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

#define G_LOG_DOMAIN "ide-plugin-section"

#include "config.h"

#include <gtk/gtk.h>

#include "ide-plugin.h"
#include "ide-plugin-private.h"
#include "ide-plugin-section.h"
#include "ide-plugin-section-private.h"

struct _IdePluginSection
{
  GObject     parent_instance;
  const char *id;
  GListModel *plugins;
};

enum {
  PROP_0,
  PROP_ID,
  PROP_PLUGINS,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdePluginSection, ide_plugin_section, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
ide_plugin_section_dispose (GObject *object)
{
  IdePluginSection *self = (IdePluginSection *)object;

  g_clear_object (&self->plugins);

  G_OBJECT_CLASS (ide_plugin_section_parent_class)->dispose (object);
}

static void
ide_plugin_section_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  IdePluginSection *self = IDE_PLUGIN_SECTION (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_static_string (value, self->id);
      break;

    case PROP_PLUGINS:
      g_value_set_object (value, self->plugins);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_plugin_section_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  IdePluginSection *self = IDE_PLUGIN_SECTION (object);

  switch (prop_id)
    {
    case PROP_ID:
      self->id = g_intern_string (g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_plugin_section_class_init (IdePluginSectionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_plugin_section_dispose;
  object_class->get_property = ide_plugin_section_get_property;
  object_class->set_property = ide_plugin_section_set_property;

  properties [PROP_ID] =
    g_param_spec_string ("id", NULL, NULL, NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_PLUGINS] =
    g_param_spec_object ("plugins", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_plugin_section_init (IdePluginSection *self)
{
}

const char *
ide_plugin_section_get_id (IdePluginSection *self)
{
  g_return_val_if_fail (IDE_IS_PLUGIN_SECTION (self), NULL);

  return self->id;
}

/**
 * ide_plugin_section_get_plugins:
 * @self: a #IdePluginSection
 *
 * A #GListModel of #IdePlugin.
 *
 * Returns: (transfer none): a #GListModel
 */
GListModel *
ide_plugin_section_get_plugins (IdePluginSection *self)
{
  g_return_val_if_fail (IDE_IS_PLUGIN_SECTION (self), NULL);

  if (self->plugins == NULL)
    {
      GtkExpression *expression = gtk_property_expression_new (IDE_TYPE_PLUGIN, NULL, "section");
      GtkStringFilter *filter = gtk_string_filter_new (expression);
      GtkFilterListModel *model;

      gtk_string_filter_set_search (filter, self->id);
      gtk_string_filter_set_match_mode (filter, GTK_STRING_FILTER_MATCH_MODE_EXACT);
      model = gtk_filter_list_model_new (g_object_ref (_ide_plugin_get_all ()), GTK_FILTER (filter));

      self->plugins = G_LIST_MODEL (model);
    }

  return self->plugins;
}

/**
 * _ide_plugin_section_get_all:
 *
 * A #GListModel of #IdePluginSection.
 *
 * Returns: (transfer none): a #GListModel
 */
GListModel *
_ide_plugin_section_get_all (void)
{
  static GListStore *sections;
  static const char *section_ids[] = {
    "editing",
    "tooling",
    "projects",
    "history",
    "platforms",
    "integration",
    "other",
  };

  if (sections == NULL)
    {
      sections = g_list_store_new (IDE_TYPE_PLUGIN_SECTION);

      for (guint i = 0; i < G_N_ELEMENTS (section_ids); i++)
        {
          g_autoptr(IdePluginSection) section = NULL;

          section = g_object_new (IDE_TYPE_PLUGIN_SECTION,
                                  "id", section_ids[i],
                                  NULL);
          g_list_store_append (sections, section);
        }
    }

  return G_LIST_MODEL (sections);
}
