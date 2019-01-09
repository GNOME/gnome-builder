/* gstyle-css-provider.c
 *
 * Copyright 2016 sebastien lafargue <slafargue@gnome.org>
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

#define G_LOG_DOMAIN "gstyle-css-provider"

#include "gstyle-css-provider.h"

struct _GstyleCssProvider
{
  GtkCssProvider parent_instance;
};

G_DEFINE_TYPE (GstyleCssProvider, gstyle_css_provider, GTK_TYPE_CSS_PROVIDER)

enum {
  PROP_0,
  N_PROPS
};

/* static GParamSpec *properties [N_PROPS]; */

static GstyleCssProvider *default_provider = NULL;

static void
parsing_error (GstyleCssProvider *self,
               GtkCssSection     *section,
               const GError      *error,
               GtkCssProvider    *provider)
{
  g_autofree gchar *uri = NULL;
  GFile *file;
  guint line = 0;
  guint line_offset = 0;

  g_assert (GSTYLE_IS_CSS_PROVIDER (self));
  g_assert (GTK_IS_CSS_PROVIDER (provider));
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
default_provider_weak_notify (gpointer           unused,
                              GstyleCssProvider *provider)
{
  g_assert (GTK_IS_CSS_PROVIDER (provider));

  g_warn_if_fail (g_atomic_pointer_compare_and_exchange (&default_provider, provider, NULL));
}

GstyleCssProvider *
gstyle_css_provider_init_default (GdkScreen *screen)
{
  g_return_val_if_fail (screen != NULL, NULL);

  if (default_provider == NULL)
    {
      default_provider = g_object_new (GSTYLE_TYPE_CSS_PROVIDER, NULL);
      g_object_weak_ref (G_OBJECT (default_provider), (GWeakNotify)default_provider_weak_notify, NULL);

      g_assert ( GSTYLE_IS_CSS_PROVIDER (default_provider));

      gtk_style_context_add_provider_for_screen (screen,
                                                 GTK_STYLE_PROVIDER (default_provider),
                                                 GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
      return default_provider;
    }

  g_assert ( GSTYLE_IS_CSS_PROVIDER (default_provider));
  return g_object_ref (default_provider);
}

GstyleCssProvider *
gstyle_css_provider_new (void)
{
  return g_object_new (GSTYLE_TYPE_CSS_PROVIDER, NULL);
}

static void
gstyle_css_provider_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  /* GstyleCssProvider *self = GSTYLE_CSS_PROVIDER (object); */

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gstyle_css_provider_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  /* GstyleCssProvider *self = GSTYLE_CSS_PROVIDER (object); */

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gstyle_css_provider_class_init (GstyleCssProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gstyle_css_provider_get_property;
  object_class->set_property = gstyle_css_provider_set_property;
}

static void
gstyle_css_provider_init (GstyleCssProvider *self)
{
  gtk_css_provider_load_from_resource (GTK_CSS_PROVIDER (self), "/org/gnome/libgstyle/theme/gstyle.css");
  g_signal_connect_swapped (self, "parsing-error", G_CALLBACK (parsing_error), self);
}
