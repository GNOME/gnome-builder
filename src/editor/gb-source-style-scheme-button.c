/* gb-source-style-scheme-button.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>

#include "gb-source-style-scheme-button.h"
#include "gb-source-style-scheme-widget.h"

struct _GbSourceStyleSchemeButtonPrivate
{
  gchar    *style_scheme_name;

  GtkLabel *label;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbSourceStyleSchemeButton,
                            gb_source_style_scheme_button,
                            GTK_TYPE_TOGGLE_BUTTON)

enum {
  PROP_0,
  PROP_STYLE_SCHEME,
  PROP_STYLE_SCHEME_NAME,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

GtkWidget *
gb_source_style_scheme_button_new (void)
{
  return g_object_new (GB_TYPE_SOURCE_STYLE_SCHEME_BUTTON, NULL);
}

const gchar *
gb_source_style_scheme_button_get_style_scheme_name (GbSourceStyleSchemeButton *button)
{
  g_return_val_if_fail (GB_IS_SOURCE_STYLE_SCHEME_BUTTON (button), NULL);

  return button->priv->style_scheme_name;
}

static void
gb_source_style_scheme_button_update_label (GbSourceStyleSchemeButton *button)
{
  GtkSourceStyleScheme *scheme;
  const gchar *label;

  g_return_if_fail (GB_IS_SOURCE_STYLE_SCHEME_BUTTON (button));

  scheme = gb_source_style_scheme_button_get_style_scheme (button);
  if (!scheme)
    g_return_if_reached ();

  label = gtk_source_style_scheme_get_name (scheme);
  gtk_label_set_label (button->priv->label, label);
}

void
gb_source_style_scheme_button_set_style_scheme_name (GbSourceStyleSchemeButton *button,
                                                     const gchar               *style_scheme_name)
{
  g_return_if_fail (GB_IS_SOURCE_STYLE_SCHEME_BUTTON (button));

  if (!style_scheme_name)
    style_scheme_name = "tango";

  if (style_scheme_name != button->priv->style_scheme_name)
    {
      g_free (button->priv->style_scheme_name);
      button->priv->style_scheme_name = g_strdup (style_scheme_name);
      gb_source_style_scheme_button_update_label (button);
      g_object_notify_by_pspec (G_OBJECT (button),
                                gParamSpecs [PROP_STYLE_SCHEME_NAME]);
      g_object_notify_by_pspec (G_OBJECT (button),
                                gParamSpecs [PROP_STYLE_SCHEME]);
    }
}

/**
 * gb_source_style_scheme_button_get_style_scheme:
 *
 * Returns: (transfer none): A #GtkSourceStyleScheme.
 */
GtkSourceStyleScheme *
gb_source_style_scheme_button_get_style_scheme (GbSourceStyleSchemeButton *button)
{
  GtkSourceStyleSchemeManager *manager;

  g_return_val_if_fail (GB_IS_SOURCE_STYLE_SCHEME_BUTTON (button), NULL);

  manager = gtk_source_style_scheme_manager_get_default ();

  return gtk_source_style_scheme_manager_get_scheme (manager,
                                                     button->priv->style_scheme_name);
}

static void
gb_source_style_scheme_button_toggled (GtkToggleButton *button)
{
  GbSourceStyleSchemeButton *self = (GbSourceStyleSchemeButton *)button;
  GtkDialog *dialog;
  GtkWidget *toplevel;
  GbSourceStyleSchemeWidget *chooser;
  GtkWidget *content_area;

  g_return_if_fail (GB_IS_SOURCE_STYLE_SCHEME_BUTTON (button));

  if (!gtk_toggle_button_get_active (button))
    return;

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (button));
  dialog = g_object_new (GTK_TYPE_DIALOG,
                         "transient-for", toplevel,
                         "title", _("Select a Style"),
                         "use-header-bar", TRUE,
                         NULL);
  gtk_dialog_add_buttons (dialog,
                          _("Cancel"), GTK_RESPONSE_CANCEL,
                          _("Select"), GTK_RESPONSE_OK,
                          NULL);
  gtk_dialog_set_default_response (dialog, GTK_RESPONSE_OK);

  chooser = g_object_new (GB_TYPE_SOURCE_STYLE_SCHEME_WIDGET,
                          "height-request", 325,
                          "style-scheme-name", self->priv->style_scheme_name,
                          "visible", TRUE,
                          "width-request", 450,
                          NULL);

  content_area = gtk_dialog_get_content_area (dialog);
  gtk_container_add (GTK_CONTAINER (content_area), GTK_WIDGET (chooser));

  if (gtk_dialog_run (dialog) == GTK_RESPONSE_OK)
    {
      gchar *name;

      g_object_get (chooser, "style-scheme-name", &name, NULL);
      gb_source_style_scheme_button_set_style_scheme_name (self, name);
      g_free (name);
    }

  gtk_widget_destroy (GTK_WIDGET (dialog));

  gtk_toggle_button_set_active (button, FALSE);
}

static void
gb_source_style_scheme_button_finalize (GObject *object)
{
  GbSourceStyleSchemeButtonPrivate *priv = GB_SOURCE_STYLE_SCHEME_BUTTON (object)->priv;

  g_clear_pointer (&priv->style_scheme_name, g_free);

  G_OBJECT_CLASS (gb_source_style_scheme_button_parent_class)->finalize (object);
}

static void
gb_source_style_scheme_button_get_property (GObject    *object,
                                            guint       prop_id,
                                            GValue     *value,
                                            GParamSpec *pspec)
{
  GbSourceStyleSchemeButton *self = GB_SOURCE_STYLE_SCHEME_BUTTON (object);

  switch (prop_id)
    {
    case PROP_STYLE_SCHEME:
      g_value_set_object (value,
                          gb_source_style_scheme_button_get_style_scheme (self));
      break;

    case PROP_STYLE_SCHEME_NAME:
      g_value_set_string (value, self->priv->style_scheme_name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_source_style_scheme_button_set_property (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec)
{
  GbSourceStyleSchemeButton *self = GB_SOURCE_STYLE_SCHEME_BUTTON (object);

  switch (prop_id)
    {
    case PROP_STYLE_SCHEME_NAME:
      gb_source_style_scheme_button_set_style_scheme_name (self,
                                                           g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_source_style_scheme_button_class_init (GbSourceStyleSchemeButtonClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkToggleButtonClass *toggle_class = GTK_TOGGLE_BUTTON_CLASS (klass);

  object_class->finalize = gb_source_style_scheme_button_finalize;
  object_class->get_property = gb_source_style_scheme_button_get_property;
  object_class->set_property = gb_source_style_scheme_button_set_property;

  toggle_class->toggled = gb_source_style_scheme_button_toggled;

  gParamSpecs [PROP_STYLE_SCHEME_NAME] =
    g_param_spec_string ("style-scheme-name",
                         _("Style Scheme Name"),
                         _("THe name of the GtkSourceStyleScheme."),
                         "tango",
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_STYLE_SCHEME_NAME,
                                   gParamSpecs [PROP_STYLE_SCHEME_NAME]);

  gParamSpecs [PROP_STYLE_SCHEME] =
    g_param_spec_object ("style-scheme",
                         _("Style Scheme"),
                         _("The style scheme as an object."),
                         GTK_SOURCE_TYPE_STYLE_SCHEME,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_STYLE_SCHEME,
                                   gParamSpecs [PROP_STYLE_SCHEME]);
}

static void
gb_source_style_scheme_button_init (GbSourceStyleSchemeButton *self)
{
  self->priv = gb_source_style_scheme_button_get_instance_private (self);

  self->priv->label = g_object_new (GTK_TYPE_LABEL,
                                    "halign", GTK_ALIGN_CENTER,
                                    "valign", GTK_ALIGN_BASELINE,
                                    "visible", TRUE,
                                    NULL);
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->priv->label));

  self->priv->style_scheme_name = g_strdup ("tango");
  gb_source_style_scheme_button_update_label (self);
}
