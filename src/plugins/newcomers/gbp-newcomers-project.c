/* gbp-newcomers-project.c
 *
 * Copyright © 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-newcomers-project"

#include <dazzle.h>

#include "gbp-newcomers-project.h"

struct _GbpNewcomersProject
{
  GtkFlowBoxChild  parent_instance;

  gchar           *uri;

  GtkLabel        *label;
  GtkImage        *icon;
  GtkBox          *tags_box;
};

enum {
  PROP_0,
  PROP_ICON_NAME,
  PROP_LANGUAGES,
  PROP_NAME,
  PROP_URI,
  N_PROPS
};

G_DEFINE_TYPE (GbpNewcomersProject, gbp_newcomers_project, GTK_TYPE_FLOW_BOX_CHILD)

static GParamSpec *properties [N_PROPS];

static void
gbp_newcomers_project_set_languages (GbpNewcomersProject *self,
                                     const gchar * const *languages)
{
  g_assert (GBP_IS_NEWCOMERS_PROJECT (self));

  if (languages == NULL)
    return;

  for (guint i = 0; languages[i] != NULL; i++)
    {
      GtkWidget *tag;

      tag = dzl_pill_box_new (languages[i]);
      gtk_container_add_with_properties (GTK_CONTAINER (self->tags_box), tag,
                                         "pack-type", GTK_PACK_END,
                                         NULL);
      gtk_widget_show (tag);
    }
}

static void
gbp_newcomers_project_destroy (GtkWidget *widget)
{
  GbpNewcomersProject *self = GBP_NEWCOMERS_PROJECT (widget);

  g_clear_pointer (&self->uri, g_free);

  GTK_WIDGET_CLASS (gbp_newcomers_project_parent_class)->destroy (widget);
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
      self->uri = g_value_dup_string (value);
      break;

    case PROP_LANGUAGES:
      gbp_newcomers_project_set_languages (self, g_value_get_boxed (value));
      break;

    case PROP_NAME:
      gtk_label_set_label (self->label, g_value_get_string (value));
      break;

    case PROP_ICON_NAME:
      g_object_set (self->icon, "icon-name", g_value_get_string (value), NULL);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_newcomers_project_class_init (GbpNewcomersProjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = gbp_newcomers_project_get_property;
  object_class->set_property = gbp_newcomers_project_set_property;

  widget_class->destroy = gbp_newcomers_project_destroy;

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

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/builder/plugins/newcomers-plugin/gbp-newcomers-project.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpNewcomersProject, label);
  gtk_widget_class_bind_template_child (widget_class, GbpNewcomersProject, icon);
  gtk_widget_class_bind_template_child (widget_class, GbpNewcomersProject, tags_box);
}

static void
gbp_newcomers_project_init (GbpNewcomersProject *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

const gchar *
gbp_newcomers_project_get_name (GbpNewcomersProject *self)
{
  g_return_val_if_fail (GBP_IS_NEWCOMERS_PROJECT (self), NULL);

  return self->label ? gtk_label_get_label (self->label) : NULL;
}

const gchar *
gbp_newcomers_project_get_uri (GbpNewcomersProject *self)
{
  g_return_val_if_fail (GBP_IS_NEWCOMERS_PROJECT (self), NULL);

  return self->uri;
}
