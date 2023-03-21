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

#include <glib/gi18n.h>

#include "ide-plugin.h"
#include "ide-plugin-private.h"

struct _IdePlugin
{
  GObject parent_object;
  PeasPluginInfo *info;
};

enum {
  PROP_0,
  PROP_AUTHORS,
  PROP_CATEGORY,
  PROP_CATEGORY_ID,
  PROP_COPYRIGHT,
  PROP_DESCRIPTION,
  PROP_ID,
  PROP_INFO,
  PROP_NAME,
  PROP_SECTION,
  PROP_VERSION,
  PROP_WEBSITE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdePlugin, ide_plugin, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];
static GHashTable *sections;

static void
ide_plugin_dispose (GObject *object)
{
  IdePlugin *self = (IdePlugin *)object;

  g_clear_object (&self->info);

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
    case PROP_COPYRIGHT:
      g_value_set_string (value, peas_plugin_info_get_copyright (self->info));
      break;

    case PROP_WEBSITE:
      {
        const char *str = peas_plugin_info_get_website (self->info);
        g_value_set_string (value, str ? str : "https://gitlab.gnome.org/GNOME/gnome-builder");
      }
      break;

    case PROP_VERSION:
      {
        const char *str = peas_plugin_info_get_version (self->info);
        g_value_set_string (value, str ? str : PACKAGE_VERSION);
      }
      break;

    case PROP_AUTHORS:
      g_value_take_string (value,
                           g_strjoinv (", ", (char **)peas_plugin_info_get_authors (self->info)));
      break;

    case PROP_ID:
      g_value_set_string (value, ide_plugin_get_id (self));
      break;

    case PROP_INFO:
      g_value_set_boxed (value, self->info);
      break;

    case PROP_NAME:
      g_value_set_string (value, ide_plugin_get_name (self));
      break;

    case PROP_CATEGORY:
      g_value_set_string (value, ide_plugin_get_category (self));
      break;

    case PROP_CATEGORY_ID:
      g_value_set_string (value, ide_plugin_get_category_id (self));
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
      self->info = g_value_dup_object (value);
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

  properties[PROP_AUTHORS] =
    g_param_spec_string ("authors", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_COPYRIGHT] =
    g_param_spec_string ("copyright", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_WEBSITE] =
    g_param_spec_string ("website", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_VERSION] =
    g_param_spec_string ("version", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_INFO] =
    g_param_spec_object ("info", NULL, NULL,
                         PEAS_TYPE_PLUGIN_INFO,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties[PROP_ID] =
    g_param_spec_string ("id", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_CATEGORY] =
    g_param_spec_string ("category", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_CATEGORY_ID] =
    g_param_spec_string ("category-id", NULL, NULL,
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

static void
ide_plugin_init_sections (void)
{
#define ADD_SECTION(category_id, section) \
  g_hash_table_insert (sections, (char *)category_id, (char *)section)

  if G_UNLIKELY (sections == NULL)
    {
      sections = g_hash_table_new (g_str_hash, g_str_equal);
      ADD_SECTION ("vcs", "history");
      ADD_SECTION ("sdks", "platforms");
      ADD_SECTION ("lsps", "tooling");
      ADD_SECTION ("devices", "platforms");
      ADD_SECTION ("diagnostics", "tooling");
      ADD_SECTION ("buildsystems", "projects");
      ADD_SECTION ("compilers", "tooling");
      ADD_SECTION ("debuggers", "projects");
      ADD_SECTION ("templates", "projects");
      ADD_SECTION ("editing", "editing");
      ADD_SECTION ("keybindings", "integration");
      ADD_SECTION ("search", "history");
      ADD_SECTION ("web", "integration");
      ADD_SECTION ("language", "tooling");
      ADD_SECTION ("desktop", "integration");
      ADD_SECTION ("other", "other");
    }

#undef ADD_SECTION
}

const char *
ide_plugin_get_section (IdePlugin *self)
{
  const char *category_id;

  g_return_val_if_fail (IDE_IS_PLUGIN (self), NULL);

  ide_plugin_init_sections ();

  if (!(category_id = peas_plugin_info_get_external_data (self->info, "Category")))
    category_id = "other";

  return g_hash_table_lookup (sections, category_id);
}

const char *
ide_plugin_get_category_id (IdePlugin *self)
{
  const char *category_id;

  g_return_val_if_fail (IDE_IS_PLUGIN (self), NULL);

  if (!(category_id = peas_plugin_info_get_external_data (self->info, "Category")))
    category_id = "other";

  return category_id;
}

const char *
ide_plugin_get_id (IdePlugin *self)
{
  g_return_val_if_fail (IDE_IS_PLUGIN (self), NULL);

  return peas_plugin_info_get_module_name (self->info);
}

const char *
ide_plugin_get_category (IdePlugin *self)
{
  static GHashTable *titles;
  const char *category_id;

  g_return_val_if_fail (IDE_IS_PLUGIN (self), NULL);

  if G_UNLIKELY (titles == NULL)
    {
      titles = g_hash_table_new (g_str_hash, g_str_equal);

#define ADD_TITLE(category, name) \
      g_hash_table_insert (titles, (char *)category, (char *)name)
      ADD_TITLE ("vcs", _("Version Control"));
      ADD_TITLE ("sdks", _("SDKs"));
      ADD_TITLE ("lsps", _("Language Servers"));
      ADD_TITLE ("devices", _("Devices & Simulators"));
      ADD_TITLE ("diagnostics", _("Diagnostics"));
      ADD_TITLE ("buildsystems", _("Build Systems"));
      ADD_TITLE ("compilers", _("Compilers"));
      ADD_TITLE ("debuggers", _("Debuggers"));
      ADD_TITLE ("templates", _("Templates"));
      ADD_TITLE ("editing", _("Editing & Formatting"));
      ADD_TITLE ("keybindings", _("Keyboard Shortcuts"));
      ADD_TITLE ("search", _("Search"));
      ADD_TITLE ("web", _("Web"));
      ADD_TITLE ("language", _("Language Enablement"));
      ADD_TITLE ("desktop", _("Desktop Integration"));
      ADD_TITLE ("other", _("Additional"));
#undef ADD_TITLE
    }

  category_id = ide_plugin_get_category_id (self);

  return g_hash_table_lookup (titles, category_id);
}

static void
plugin_list_changed_cb (PeasEngine *engine,
                        GParamSpec *pspec,
                        GListStore *store)
{
  guint n_items;

  g_assert (PEAS_IS_ENGINE (engine));
  g_assert (G_IS_LIST_STORE (store));

  g_list_store_remove_all (store);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (engine));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(PeasPluginInfo) plugin_info = g_list_model_get_item (G_LIST_MODEL (engine), i);
      g_autoptr(IdePlugin) plugin = NULL;

      if (peas_plugin_info_is_hidden (plugin_info))
        continue;

      plugin = g_object_new (IDE_TYPE_PLUGIN,
                             "info", plugin_info,
                             NULL);
      g_list_store_append (store, plugin);
    }
}

GListModel *
_ide_plugin_get_all (void)
{
  static GListStore *store;

  if (store == NULL)
    {
      PeasEngine *engine = peas_engine_get_default ();

      store = g_list_store_new (IDE_TYPE_PLUGIN);
      g_signal_connect_object (engine,
                               "notify::plugin-list",
                               G_CALLBACK (plugin_list_changed_cb),
                               store,
                               0);
      plugin_list_changed_cb (engine, NULL, store);
    }

  return G_LIST_MODEL (store);
}
