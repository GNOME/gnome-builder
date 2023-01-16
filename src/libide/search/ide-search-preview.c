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

#include "ide-search-preview.h"

typedef struct
{
  char *title;
  char *subtitle;
} IdeSearchPreviewPrivate;

enum {
  PROP_0,
  PROP_SUBTITLE,
  PROP_TITLE,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (IdeSearchPreview, ide_search_preview, ADW_TYPE_BIN)

static GParamSpec *properties [N_PROPS];

static void
ide_search_preview_finalize (GObject *object)
{
  IdeSearchPreview *self = (IdeSearchPreview *)object;
  IdeSearchPreviewPrivate *priv = ide_search_preview_get_instance_private (self);

  g_clear_pointer (&priv->title, g_free);
  g_clear_pointer (&priv->subtitle, g_free);

  G_OBJECT_CLASS (ide_search_preview_parent_class)->finalize (object);
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

  object_class->finalize = ide_search_preview_finalize;
  object_class->get_property = ide_search_preview_get_property;
  object_class->set_property = ide_search_preview_set_property;

  properties[PROP_SUBTITLE] =
    g_param_spec_string ("sbutitle", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties[PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

}

static void
ide_search_preview_init (IdeSearchPreview *self)
{
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
