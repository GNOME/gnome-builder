/* gbp-create-project-template-icon.c
 *
 * Copyright 2016 Akshaya Kakkilaya <akshaya.kakkilaya@gmail.com>
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

#include "gbp-create-project-template-icon.h"

struct _GbpCreateProjectTemplateIcon
{
  GtkBin              parent;

  GtkImage           *template_icon;
  GtkLabel           *template_name;

  IdeProjectTemplate *template;
};

enum {
  PROP_0,
  PROP_TEMPLATE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

G_DEFINE_TYPE (GbpCreateProjectTemplateIcon, gbp_create_project_template_icon, GTK_TYPE_BIN)

static void
gbp_create_project_template_icon_get_property (GObject    *object,
                                               guint       prop_id,
                                               GValue     *value,
                                               GParamSpec *pspec)
{
  GbpCreateProjectTemplateIcon *self = GBP_CREATE_PROJECT_TEMPLATE_ICON (object);

  switch (prop_id)
    {
    case PROP_TEMPLATE:
      g_value_set_object (value, self->template);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_create_project_template_icon_set_property (GObject      *object,
                                               guint         prop_id,
                                               const GValue *value,
                                               GParamSpec   *pspec)
{
  GbpCreateProjectTemplateIcon *self = GBP_CREATE_PROJECT_TEMPLATE_ICON (object);
  g_autofree gchar *icon_name = NULL;
  g_autofree gchar *name = NULL;
  g_autofree gchar *description = NULL;

  switch (prop_id)
    {
    case PROP_TEMPLATE:
      self->template = g_value_dup_object (value);

      icon_name = ide_project_template_get_icon_name (self->template);
      name = ide_project_template_get_name (self->template);
      description = ide_project_template_get_description (self->template);

      g_object_set (self->template_icon,
                    "icon-name", icon_name,
                    NULL);
      gtk_label_set_text (self->template_name, name);
      if (!ide_str_empty0 (description))
        gtk_widget_set_tooltip_text (GTK_WIDGET (self), description);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_create_project_template_icon_destroy (GtkWidget *widget)
{
  GbpCreateProjectTemplateIcon *self = (GbpCreateProjectTemplateIcon *)widget;

  g_clear_object (&self->template);

  GTK_WIDGET_CLASS (gbp_create_project_template_icon_parent_class)->destroy (widget);
}

static void
gbp_create_project_template_icon_class_init (GbpCreateProjectTemplateIconClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = gbp_create_project_template_icon_set_property;
  object_class->get_property = gbp_create_project_template_icon_get_property;

  widget_class->destroy = gbp_create_project_template_icon_destroy;

  properties [PROP_TEMPLATE] =
    g_param_spec_object ("template",
                         "Template",
                         "Template",
                         IDE_TYPE_PROJECT_TEMPLATE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/plugins/create-project/gbp-create-project-template-icon.ui");
  gtk_widget_class_set_css_name (widget_class, "createprojecttemplateicon");
  gtk_widget_class_bind_template_child (widget_class, GbpCreateProjectTemplateIcon, template_icon);
  gtk_widget_class_bind_template_child (widget_class, GbpCreateProjectTemplateIcon, template_name);
}

static void
gbp_create_project_template_icon_init (GbpCreateProjectTemplateIcon *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

/**
 * gbp_create_project_template_icon_get_template:
 * @self: a #GbpCreateProjectTemplateIcon
 *
 * Gets the template for the item.
 *
 * Returns: (transfer none): an #IdeProjectTemplate
 *
 * Since: 3.32
 */
IdeProjectTemplate *
gbp_create_project_template_icon_get_template (GbpCreateProjectTemplateIcon *self)
{
  g_return_val_if_fail (GBP_IS_CREATE_PROJECT_TEMPLATE_ICON (self), NULL);

  return self->template;
}
