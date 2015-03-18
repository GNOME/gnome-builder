/* gb-keybindings.c
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

#define G_LOG_DOMAIN "gb-keybindings"

#include <glib/gi18n.h>
#include <ide.h>

#include "gb-keybindings.h"

struct _GbKeybindings
{
  GObject         parent_instance;

  GtkApplication *application;
  GtkCssProvider *css_provider;
  gchar          *mode;
  guint           constructed : 1;
};

enum
{
  PROP_0,
  PROP_APPLICATION,
  PROP_MODE,
  LAST_PROP
};

G_DEFINE_TYPE (GbKeybindings, gb_keybindings, G_TYPE_OBJECT)

static GParamSpec *gParamSpecs [LAST_PROP];

GbKeybindings *
gb_keybindings_new (GtkApplication *application,
                    const gchar    *mode)
{
  g_return_val_if_fail (GTK_IS_APPLICATION (application), NULL);

  return g_object_new (GB_TYPE_KEYBINDINGS,
                       "application", application,
                       "mode", mode,
                       NULL);
}

static void
gb_keybindings_reload (GbKeybindings *self)
{
  const gchar *mode;
  g_autofree gchar *path = NULL;
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (GB_IS_KEYBINDINGS (self));

  mode = self->mode ? self->mode : "default";
  IDE_TRACE_MSG ("Loading %s keybindings", mode);
  path = g_strdup_printf ("/org/gnome/builder/keybindings/%s.css", mode);
  bytes = g_resources_lookup_data (path, G_RESOURCE_LOOKUP_FLAGS_NONE, &error);

  if (error == NULL)
    gtk_css_provider_load_from_data (self->css_provider,
                                     g_bytes_get_data (bytes, NULL),
                                     g_bytes_get_size (bytes),
                                     &error);

  if (error)
    g_warning ("%s", error->message);

  IDE_EXIT;
}

const gchar *
gb_keybindings_get_mode (GbKeybindings *self)
{
  g_return_val_if_fail (GB_IS_KEYBINDINGS (self), NULL);

  return self->mode;
}

void
gb_keybindings_set_mode (GbKeybindings *self,
                         const gchar   *mode)
{
  g_return_if_fail (GB_IS_KEYBINDINGS (self));

  if (mode != self->mode)
    {
      g_free (self->mode);
      self->mode = g_strdup (mode);
      if (self->constructed)
        gb_keybindings_reload (self);
      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_MODE]);
    }
}

GtkApplication *
gb_keybindings_get_application (GbKeybindings *self)
{
  g_return_val_if_fail (GB_IS_KEYBINDINGS (self), NULL);

  return self->application;
}

static void
gb_keybindings_set_application (GbKeybindings  *self,
                                GtkApplication *application)
{
  g_assert (GB_IS_KEYBINDINGS (self));
  g_assert (!application || GTK_IS_APPLICATION (application));

  if (application != self->application)
    {
      if (self->application)
        {
          /* remove keybindings */
          g_clear_object (&self->application);
        }

      if (application)
        {
          /* connect keybindings */
          self->application = g_object_ref (application);
        }
    }
}

static void
gb_keybindings_parsing_error (GtkCssProvider *css_provider,
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
gb_keybindings_constructed (GObject *object)
{
  GbKeybindings *self = (GbKeybindings *)object;
  GdkScreen *screen;

  IDE_ENTRY;

  G_OBJECT_CLASS (gb_keybindings_parent_class)->constructed (object);

  screen = gdk_screen_get_default ();
  gtk_style_context_add_provider_for_screen (screen, GTK_STYLE_PROVIDER (self->css_provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  self->constructed = TRUE;

  gb_keybindings_reload (self);

  IDE_EXIT;
}

static void
gb_keybindings_finalize (GObject *object)
{
  GbKeybindings *self = (GbKeybindings *)object;

  IDE_ENTRY;

  g_clear_object (&self->application);
  g_clear_object (&self->css_provider);
  g_clear_pointer (&self->mode, g_free);

  G_OBJECT_CLASS (gb_keybindings_parent_class)->finalize (object);

  IDE_EXIT;
}

static void
gb_keybindings_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GbKeybindings *self = GB_KEYBINDINGS (object);

  switch (prop_id)
    {
    case PROP_APPLICATION:
      g_value_set_object (value, gb_keybindings_get_application (self));
      break;

    case PROP_MODE:
      g_value_set_string (value, gb_keybindings_get_mode (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_keybindings_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GbKeybindings *self = GB_KEYBINDINGS (object);

  switch (prop_id)
    {
    case PROP_APPLICATION:
      gb_keybindings_set_application (self, g_value_get_object (value));
      break;

    case PROP_MODE:
      gb_keybindings_set_mode (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_keybindings_class_init (GbKeybindingsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gb_keybindings_constructed;
  object_class->finalize = gb_keybindings_finalize;
  object_class->get_property = gb_keybindings_get_property;
  object_class->set_property = gb_keybindings_set_property;

  gParamSpecs [PROP_APPLICATION] =
    g_param_spec_object ("application",
                         _("Application"),
                         _("The application to register keybindings for."),
                         GTK_TYPE_APPLICATION,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_APPLICATION, gParamSpecs [PROP_APPLICATION]);

  gParamSpecs [PROP_MODE] =
    g_param_spec_string ("mode",
                         _("Mode"),
                         _("The name of the keybindings mode."),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_MODE, gParamSpecs [PROP_MODE]);
}

static void
gb_keybindings_init (GbKeybindings *self)
{
  self->css_provider = gtk_css_provider_new ();

  g_signal_connect (self->css_provider,
                    "parsing-error",
                    G_CALLBACK (gb_keybindings_parsing_error),
                    NULL);
}
