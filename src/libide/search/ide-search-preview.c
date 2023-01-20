/* ide-search-preview.c
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-search-preview"

#include "config.h"

#include <adwaita.h>

#include <libide-gtk.h>

#include "ide-search-preview.h"

typedef struct
{
  char *title;
  char *subtitle;

  GtkOverlay     *overlay;
  GtkProgressBar *progress_bar;
} IdeSearchPreviewPrivate;

enum {
  PROP_0,
  PROP_CHILD,
  PROP_PROGRESS,
  PROP_SUBTITLE,
  PROP_TITLE,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (IdeSearchPreview, ide_search_preview, GTK_TYPE_WIDGET)

static GParamSpec *properties [N_PROPS];

static void
ide_search_preview_dispose (GObject *object)
{
  IdeSearchPreview *self = (IdeSearchPreview *)object;
  IdeSearchPreviewPrivate *priv = ide_search_preview_get_instance_private (self);

  g_clear_pointer (&priv->title, g_free);
  g_clear_pointer (&priv->subtitle, g_free);
  g_clear_pointer ((GtkWidget **)&priv->overlay, gtk_widget_unparent);

  G_OBJECT_CLASS (ide_search_preview_parent_class)->dispose (object);
}

static void
ide_search_preview_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  IdeSearchPreview *self = IDE_SEARCH_PREVIEW (object);

  switch (prop_id)
    {
    case PROP_CHILD:
      g_value_set_object (value, ide_search_preview_get_child (self));
      break;

    case PROP_PROGRESS:
      g_value_set_double (value, ide_search_preview_get_progress (self));
      break;

    case PROP_SUBTITLE:
      g_value_set_string (value, ide_search_preview_get_subtitle (self));
      break;

    case PROP_TITLE:
      g_value_set_string (value, ide_search_preview_get_title (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_search_preview_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  IdeSearchPreview *self = IDE_SEARCH_PREVIEW (object);

  switch (prop_id)
    {
    case PROP_CHILD:
      ide_search_preview_set_child (self, g_value_get_object (value));
      break;

    case PROP_PROGRESS:
      ide_search_preview_set_progress (self, g_value_get_double (value));
      break;

    case PROP_SUBTITLE:
      ide_search_preview_set_subtitle (self, g_value_get_string (value));
      break;

    case PROP_TITLE:
      ide_search_preview_set_title (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_search_preview_class_init (IdeSearchPreviewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ide_search_preview_dispose;
  object_class->get_property = ide_search_preview_get_property;
  object_class->set_property = ide_search_preview_set_property;

  properties [PROP_CHILD] =
    g_param_spec_object ("child", NULL, NULL,
                         GTK_TYPE_WIDGET,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_PROGRESS] =
    g_param_spec_double ("progress", NULL, NULL,
                         .0, 1., .0,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_SUBTITLE] =
    g_param_spec_string ("subtitle", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-search/ide-search-preview.ui");
  gtk_widget_class_bind_template_child_private (widget_class, IdeSearchPreview, overlay);
  gtk_widget_class_bind_template_child_private (widget_class, IdeSearchPreview, progress_bar);
}

static void
ide_search_preview_init (IdeSearchPreview *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

/**
 * ide_search_preview_get_child:
 * @self: a #IdeSearchPreview
 *
 * Gets the child widget, if any.
 *
 * Returns: (transfer none) (nullable): a #GtkWidget or %NULL
 */
GtkWidget *
ide_search_preview_get_child (IdeSearchPreview *self)
{
  IdeSearchPreviewPrivate *priv = ide_search_preview_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SEARCH_PREVIEW (self), NULL);

  return gtk_overlay_get_child (priv->overlay);
}

void
ide_search_preview_set_child (IdeSearchPreview *self,
                              GtkWidget        *child)
{
  IdeSearchPreviewPrivate *priv = ide_search_preview_get_instance_private (self);

  g_return_if_fail (IDE_IS_SEARCH_PREVIEW (self));

  if (child != ide_search_preview_get_child (self))
    {
      gtk_overlay_set_child (priv->overlay, child);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CHILD]);
    }
}

double
ide_search_preview_get_progress (IdeSearchPreview *self)
{
  IdeSearchPreviewPrivate *priv = ide_search_preview_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SEARCH_PREVIEW (self), .0);

  return gtk_progress_bar_get_fraction (priv->progress_bar);
}

void
ide_search_preview_set_progress (IdeSearchPreview *self,
                                 double            progress)
{
  IdeSearchPreviewPrivate *priv = ide_search_preview_get_instance_private (self);

  g_return_if_fail (IDE_IS_SEARCH_PREVIEW (self));

  if (progress != ide_search_preview_get_progress (self))
    {
      gtk_progress_bar_set_fraction (priv->progress_bar, progress);

      if (progress <= .0)
        gtk_widget_hide (GTK_WIDGET (priv->progress_bar));
      else if (progress < 1.)
        gtk_widget_show (GTK_WIDGET (priv->progress_bar));
      else
        ide_gtk_widget_hide_with_fade (GTK_WIDGET (priv->progress_bar));

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PROGRESS]);
    }
}

const char *
ide_search_preview_get_subtitle (IdeSearchPreview *self)
{
  IdeSearchPreviewPrivate *priv = ide_search_preview_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SEARCH_PREVIEW (self), NULL);

  return priv->subtitle;
}

void
ide_search_preview_set_subtitle (IdeSearchPreview *self,
                                 const char       *subtitle)
{
  IdeSearchPreviewPrivate *priv = ide_search_preview_get_instance_private (self);

  g_return_if_fail (IDE_IS_SEARCH_PREVIEW (self));

  if (g_set_str (&priv->subtitle, subtitle))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SUBTITLE]);
}

const char *
ide_search_preview_get_title (IdeSearchPreview *self)
{
  IdeSearchPreviewPrivate *priv = ide_search_preview_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SEARCH_PREVIEW (self), NULL);

  return priv->title;
}

void
ide_search_preview_set_title (IdeSearchPreview *self,
                              const char       *title)
{
  IdeSearchPreviewPrivate *priv = ide_search_preview_get_instance_private (self);

  g_return_if_fail (IDE_IS_SEARCH_PREVIEW (self));

  if (g_set_str (&priv->title, title))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TITLE]);
}

GtkWidget *
ide_search_preview_new (void)
{
  return g_object_new (IDE_TYPE_SEARCH_PREVIEW, NULL);
}
