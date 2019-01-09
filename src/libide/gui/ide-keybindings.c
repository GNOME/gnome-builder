/* ide-keybindings.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-keybindings"

#include "config.h"

#include <dazzle.h>
#include <glib/gi18n.h>
#include <libpeas/peas.h>
#include <libide-core.h>

#include "ide-keybindings.h"

struct _IdeKeybindings
{
  GObject         parent_instance;

  GtkCssProvider *css_provider;
  gchar          *mode;
  GHashTable     *plugin_providers;

  guint           constructed : 1;
};

enum
{
  PROP_0,
  PROP_MODE,
  LAST_PROP
};

G_DEFINE_TYPE (IdeKeybindings, ide_keybindings, G_TYPE_OBJECT)

static GParamSpec *properties [LAST_PROP];

IdeKeybindings *
ide_keybindings_new (const gchar *mode)
{
  return g_object_new (IDE_TYPE_KEYBINDINGS,
                       "mode", mode,
                       NULL);
}

static void
ide_keybindings_load_plugin (IdeKeybindings *self,
                             PeasPluginInfo *plugin_info,
                             PeasEngine     *engine)
{
  g_autofree gchar *path = NULL;
  const gchar *module_name;
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GtkCssProvider) provider = NULL;

  g_assert (IDE_IS_KEYBINDINGS (self));
  g_assert (plugin_info != NULL);
  g_assert (PEAS_IS_ENGINE (engine));

  if (!self->mode || !self->plugin_providers)
    return;

  module_name = peas_plugin_info_get_module_name (plugin_info);
  path = g_strdup_printf ("/plugins/%s/keybindings/%s.css", module_name, self->mode);
  bytes = g_resources_lookup_data (path, 0, NULL);
  if (bytes == NULL)
    return;

  IDE_TRACE_MSG ("Loading %s keybindings for \"%s\" plugin", self->mode, module_name);

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider, path);
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                             GTK_STYLE_PROVIDER (provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);
  g_hash_table_insert (self->plugin_providers,
                       g_strdup (module_name),
                       g_steal_pointer (&provider));
}

static void
ide_keybindings_unload_plugin (IdeKeybindings *self,
                               PeasPluginInfo *plugin_info,
                               PeasEngine     *engine)
{
  GtkStyleProvider *provider;
  const gchar *module_name;

  g_assert (IDE_IS_KEYBINDINGS (self));
  g_assert (plugin_info != NULL);
  g_assert (PEAS_IS_ENGINE (engine));

  if (self->plugin_providers == NULL)
    return;

  module_name = peas_plugin_info_get_module_name (plugin_info);
  provider = g_hash_table_lookup (self->plugin_providers, module_name);
  if (provider == NULL)
    return;

  gtk_style_context_remove_provider_for_screen (gdk_screen_get_default (), provider);
  g_hash_table_remove (self->plugin_providers, module_name);
}

static void
ide_keybindings_reload (IdeKeybindings *self)
{
  GdkScreen *screen;
  PeasEngine *engine;
  const GList *list;

  IDE_ENTRY;

  g_assert (IDE_IS_KEYBINDINGS (self));

  {
    g_autofree gchar *path = NULL;
    g_autoptr(GBytes) bytes = NULL;
    g_autoptr(GError) error = NULL;

    if (self->mode == NULL)
      self->mode = g_strdup ("default");

    IDE_TRACE_MSG ("Loading %s keybindings", self->mode);
    path = g_strdup_printf ("/org/gnome/builder/keybindings/%s.css", self->mode);
    bytes = g_resources_lookup_data (path, G_RESOURCE_LOOKUP_FLAGS_NONE, &error);

    if (bytes == NULL)
      {
        g_clear_pointer (&path, g_free);
        path = g_strdup_printf ("/plugins/%s/keybindings/%s.css", self->mode, self->mode);
        bytes = g_resources_lookup_data (path, G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
        if (bytes != NULL)
          g_clear_error (&error);
      }

    if (error == NULL)
      {
        /*
         * We use -1 for the length so that the CSS provider knows that the
         * string is \0 terminated. This is guaranteed to us by GResources so
         * that interned data can be used as C strings.
         */
        gtk_css_provider_load_from_data (self->css_provider,
                                         g_bytes_get_data (bytes, NULL),
                                         -1,
                                         &error);
      }

    if (error)
      g_warning ("%s", error->message);
  }

  engine = peas_engine_get_default ();
  screen = gdk_screen_get_default ();

  if (self->plugin_providers != NULL)
    {
      GHashTableIter iter;
      GtkStyleProvider *provider;

      g_hash_table_iter_init (&iter, self->plugin_providers);
      while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&provider))
        gtk_style_context_remove_provider_for_screen (screen, provider);

      g_clear_pointer (&self->plugin_providers, g_hash_table_unref);
    }

  self->plugin_providers = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

  list = peas_engine_get_plugin_list (engine);

  for (; list != NULL; list = list->next)
    {
      PeasPluginInfo *plugin_info = list->data;

      if (!peas_plugin_info_is_loaded (plugin_info))
        continue;

      ide_keybindings_load_plugin (self, plugin_info, engine);
    }

  IDE_EXIT;
}

const gchar *
ide_keybindings_get_mode (IdeKeybindings *self)
{
  g_return_val_if_fail (IDE_IS_KEYBINDINGS (self), NULL);

  return self->mode;
}

void
ide_keybindings_set_mode (IdeKeybindings *self,
                          const gchar    *mode)
{
  g_return_if_fail (IDE_IS_KEYBINDINGS (self));

  if (!dzl_str_equal0 (self->mode, mode))
    {
      g_free (self->mode);
      self->mode = g_strdup (mode);

      if (self->constructed)
        ide_keybindings_reload (self);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MODE]);
    }
}

static void
ide_keybindings_parsing_error (GtkCssProvider *css_provider,
                               GtkCssSection  *section,
                               GError         *error,
                               gpointer        user_data)
{
  g_autofree gchar *filename = NULL;
  GFile *file;
  guint start_line;
  guint end_line;

  file = gtk_css_section_get_file (section);
  filename = g_file_get_uri (file);
  start_line = gtk_css_section_get_start_line (section);
  end_line = gtk_css_section_get_end_line (section);

  g_warning ("CSS parsing error in %s between lines %u and %u", filename, start_line, end_line);
}

static void
ide_keybindings_constructed (GObject *object)
{
  IdeKeybindings *self = (IdeKeybindings *)object;
  PeasEngine *engine;
  GdkScreen *screen;

  IDE_ENTRY;

  self->constructed = TRUE;

  G_OBJECT_CLASS (ide_keybindings_parent_class)->constructed (object);

  screen = gdk_screen_get_default ();
  engine = peas_engine_get_default ();

  g_signal_connect_object (engine,
                           "load-plugin",
                           G_CALLBACK (ide_keybindings_load_plugin),
                           self,
                           G_CONNECT_AFTER | G_CONNECT_SWAPPED);

  g_signal_connect_object (engine,
                           "unload-plugin",
                           G_CALLBACK (ide_keybindings_unload_plugin),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_style_context_add_provider_for_screen (screen, GTK_STYLE_PROVIDER (self->css_provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  ide_keybindings_reload (self);

  IDE_EXIT;
}

static void
ide_keybindings_finalize (GObject *object)
{
  IdeKeybindings *self = (IdeKeybindings *)object;

  IDE_ENTRY;

  g_clear_object (&self->css_provider);
  g_clear_pointer (&self->mode, g_free);
  g_clear_pointer (&self->plugin_providers, g_hash_table_unref);

  G_OBJECT_CLASS (ide_keybindings_parent_class)->finalize (object);

  IDE_EXIT;
}

static void
ide_keybindings_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeKeybindings *self = IDE_KEYBINDINGS (object);

  switch (prop_id)
    {
    case PROP_MODE:
      g_value_set_string (value, ide_keybindings_get_mode (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_keybindings_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  IdeKeybindings *self = IDE_KEYBINDINGS (object);

  switch (prop_id)
    {
    case PROP_MODE:
      ide_keybindings_set_mode (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_keybindings_class_init (IdeKeybindingsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_keybindings_constructed;
  object_class->finalize = ide_keybindings_finalize;
  object_class->get_property = ide_keybindings_get_property;
  object_class->set_property = ide_keybindings_set_property;

  properties [PROP_MODE] =
    g_param_spec_string ("mode",
                         "Mode",
                         "The name of the keybindings mode.",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
ide_keybindings_init (IdeKeybindings *self)
{
  self->css_provider = gtk_css_provider_new ();

  g_signal_connect (self->css_provider,
                    "parsing-error",
                    G_CALLBACK (ide_keybindings_parsing_error),
                    NULL);
}
