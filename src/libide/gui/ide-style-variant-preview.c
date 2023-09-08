/* ide-style-variant-preview.c
 *
 * Copyright 2022 Christian Hergert <unknown@domain.org>
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

#define G_LOG_DOMAIN "ide-style-variant-preview"

#include "config.h"

#include "ide-style-variant-preview-private.h"

struct _IdeStyleVariantPreview
{
  GtkWidget       parent_instance;
  AdwColorScheme  color_scheme;
  GtkPicture     *wallpaper;
};

G_DEFINE_FINAL_TYPE (IdeStyleVariantPreview, ide_style_variant_preview, GTK_TYPE_WIDGET)

enum {
  PROP_0,
  PROP_COLOR_SCHEME,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

GtkWidget *
ide_style_variant_preview_new (AdwColorScheme color_scheme)
{
  return g_object_new (IDE_TYPE_STYLE_VARIANT_PREVIEW,
                       "color-scheme", color_scheme,
                       NULL);
}

static void
ide_style_variant_preview_set_color_scheme (IdeStyleVariantPreview *self,
                                            AdwColorScheme          color_scheme)
{
  const char *wallpaper;

  g_assert (IDE_IS_STYLE_VARIANT_PREVIEW (self));

  self->color_scheme = color_scheme;

  switch (color_scheme)
    {
    case ADW_COLOR_SCHEME_PREFER_LIGHT:
    case ADW_COLOR_SCHEME_FORCE_LIGHT:
      wallpaper = "/org/gnome/libide-gui/images/preview-light.svg";
      break;

    case ADW_COLOR_SCHEME_PREFER_DARK:
    case ADW_COLOR_SCHEME_FORCE_DARK:
      wallpaper = "/org/gnome/libide-gui/images/preview-dark.svg";
      break;

    case ADW_COLOR_SCHEME_DEFAULT:
    default:
      wallpaper = "/org/gnome/libide-gui/images/preview-system.svg";
      break;
    }

  gtk_picture_set_resource (self->wallpaper, wallpaper);
}

static void
ide_style_variant_preview_measure (GtkWidget      *widget,
                                   GtkOrientation  orientation,
                                   int             for_size,
                                   int            *minimum,
                                   int            *natural,
                                   int            *minimum_baseline,
                                   int            *natural_baseline)
{
  GTK_WIDGET_CLASS (ide_style_variant_preview_parent_class)->measure (widget, orientation, for_size, minimum, natural, minimum_baseline, natural_baseline);

  /* Work around GtkPicture wierdness */
  if (orientation == GTK_ORIENTATION_VERTICAL)
    {
      *natural = *minimum;
      *natural_baseline = *minimum_baseline;
    }
}

static void
ide_style_variant_preview_dispose (GObject *object)
{
  IdeStyleVariantPreview *self = (IdeStyleVariantPreview *)object;
  GtkWidget *child;

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self))))
    gtk_widget_unparent (child);

  G_OBJECT_CLASS (ide_style_variant_preview_parent_class)->dispose (object);
}

static void
ide_style_variant_preview_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  IdeStyleVariantPreview *self = IDE_STYLE_VARIANT_PREVIEW (object);

  switch (prop_id)
    {
    case PROP_COLOR_SCHEME:
      g_value_set_enum (value, self->color_scheme);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_style_variant_preview_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  IdeStyleVariantPreview *self = IDE_STYLE_VARIANT_PREVIEW (object);

  switch (prop_id)
    {
    case PROP_COLOR_SCHEME:
      ide_style_variant_preview_set_color_scheme (self, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_style_variant_preview_class_init (IdeStyleVariantPreviewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ide_style_variant_preview_dispose;
  object_class->get_property = ide_style_variant_preview_get_property;
  object_class->set_property = ide_style_variant_preview_set_property;

  widget_class->measure = ide_style_variant_preview_measure;

  properties [PROP_COLOR_SCHEME] =
    g_param_spec_enum ("color-scheme",
                       "Color Scheme",
                       "Color Scheme",
                       ADW_TYPE_COLOR_SCHEME,
                       ADW_COLOR_SCHEME_DEFAULT,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "stylevariantpreview");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-gui/ui/ide-style-variant-preview.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeStyleVariantPreview, wallpaper);
}

static void
ide_style_variant_preview_init (IdeStyleVariantPreview *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
