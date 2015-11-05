/* ide-css-provider.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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
 */

#define G_LOG_DOMAIN "ide-css-provider"

#include <glib/gi18n.h>
#include <ide.h>

#include "ide-css-provider.h"

struct _IdeCssProvider
{
  GtkCssProvider parent_instance;

  GtkSettings *settings;
};

G_DEFINE_TYPE (IdeCssProvider, ide_css_provider, GTK_TYPE_CSS_PROVIDER)

GtkCssProvider *
ide_css_provider_new (void)
{
  return g_object_new (IDE_TYPE_CSS_PROVIDER, NULL);
}

static void
ide_css_provider_update (IdeCssProvider *self)
{
  g_autofree gchar *theme_name = NULL;
  g_autofree gchar *resource_path = NULL;
  gboolean prefer_dark_theme = FALSE;
  gsize len = 0;
  guint32 flags = 0;

  IDE_ENTRY;

  g_assert (IDE_IS_CSS_PROVIDER (self));
  g_assert (GTK_IS_SETTINGS (self->settings));

  g_object_get (self->settings,
                "gtk-theme-name", &theme_name,
                "gtk-application-prefer-dark-theme", &prefer_dark_theme,
                NULL);

  resource_path = g_strdup_printf ("/org/gnome/builder/theme/%s%s.css",
                                   theme_name,
                                   prefer_dark_theme ? "-dark" : "");

  if (!g_resources_get_info (resource_path, G_RESOURCE_LOOKUP_FLAGS_NONE, &len, &flags, NULL))
    {
      g_free (resource_path);
      resource_path = g_strdup ("/org/gnome/builder/theme/shared.css");
    }

  gtk_css_provider_load_from_resource (GTK_CSS_PROVIDER (self), resource_path);

  IDE_EXIT;
}

static void
ide_css_provider__settings_notify_gtk_theme_name (IdeCssProvider *self,
                                                 GParamSpec    *pspec,
                                                 GtkSettings   *settings)
{
  g_assert (IDE_IS_CSS_PROVIDER (self));

  ide_css_provider_update (self);
}

static void
ide_css_provider__settings_notify_gtk_application_prefer_dark_theme (IdeCssProvider *self,
                                                                    GParamSpec    *pspec,
                                                                    GtkSettings   *settings)
{
  g_assert (IDE_IS_CSS_PROVIDER (self));

  ide_css_provider_update (self);
}

static void
ide_css_provider_parsing_error (GtkCssProvider *provider,
                               GtkCssSection  *section,
                               const GError   *error)
{
  g_autofree gchar *uri = NULL;
  GFile *file;
  guint line = 0;
  guint line_offset = 0;

  g_assert (IDE_IS_CSS_PROVIDER (provider));
  g_assert (error != NULL);

  if (section != NULL)
    {
      file = gtk_css_section_get_file (section);
      uri = g_file_get_uri (file);
      line = gtk_css_section_get_start_line (section);
      line_offset = gtk_css_section_get_start_position (section);
      g_warning ("Parsing Error: %s @ %u:%u: %s", uri, line, line_offset, error->message);
    }
  else
    {
      g_warning ("%s", error->message);
    }
}

static void
ide_css_provider_constructed (GObject *object)
{
  IdeCssProvider *self = (IdeCssProvider *)object;

  G_OBJECT_CLASS (ide_css_provider_parent_class)->constructed (object);

  self->settings = g_object_ref (gtk_settings_get_default ());

  g_signal_connect_object (self->settings,
                           "notify::gtk-theme-name",
                           G_CALLBACK (ide_css_provider__settings_notify_gtk_theme_name),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->settings,
                           "notify::gtk-application-prefer-dark-theme",
                           G_CALLBACK (ide_css_provider__settings_notify_gtk_application_prefer_dark_theme),
                           self,
                           G_CONNECT_SWAPPED);

  ide_css_provider_update (self);
}

static void
ide_css_provider_finalize (GObject *object)
{
  IdeCssProvider *self = (IdeCssProvider *)object;

  g_clear_object (&self->settings);

  G_OBJECT_CLASS (ide_css_provider_parent_class)->finalize (object);
}

static void
ide_css_provider_class_init (IdeCssProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkCssProviderClass *provider_class = GTK_CSS_PROVIDER_CLASS (klass);

  object_class->finalize = ide_css_provider_finalize;
  object_class->constructed = ide_css_provider_constructed;

  provider_class->parsing_error = ide_css_provider_parsing_error;
}

static void
ide_css_provider_init (IdeCssProvider *self)
{
}
