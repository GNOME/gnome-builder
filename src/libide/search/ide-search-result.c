/* ide-search-result.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-search-result"

#include "config.h"

#include <gtksourceview/gtksource.h>

#include "ide-search-result.h"

typedef struct
{
  char         *title;
  char         *subtitle;
  GdkPaintable *paintable;
  GIcon        *gicon;
  char         *accelerator;
  float         score;
  guint         priority;
  guint         use_underline : 1;
  guint         use_markup : 1;
} IdeSearchResultPrivate;

enum {
  PROP_0,
  PROP_ACCELERATOR,
  PROP_PAINTABLE,
  PROP_GICON,
  PROP_PRIORITY,
  PROP_SCORE,
  PROP_SUBTITLE,
  PROP_TITLE,
  PROP_USE_MARKUP,
  PROP_USE_UNDERLINE,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (IdeSearchResult, ide_search_result, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static gboolean
ide_search_result_real_matches (IdeSearchResult *self,
                                const char      *query)
{
  IdeSearchResultPrivate *priv = ide_search_result_get_instance_private (self);
  guint prio;

  if (priv->title != NULL && gtk_source_completion_fuzzy_match (priv->title, query, &prio))
    return TRUE;

  if (priv->subtitle != NULL && gtk_source_completion_fuzzy_match (priv->subtitle, query, &prio))
    return TRUE;

  return FALSE;
}

static void
ide_search_result_finalize (GObject *object)
{
  IdeSearchResult *self = (IdeSearchResult *)object;
  IdeSearchResultPrivate *priv = ide_search_result_get_instance_private (self);

  g_clear_object (&priv->gicon);
  g_clear_object (&priv->paintable);
  g_clear_pointer (&priv->title, g_free);
  g_clear_pointer (&priv->subtitle, g_free);
  g_clear_pointer (&priv->accelerator, g_free);

  G_OBJECT_CLASS (ide_search_result_parent_class)->finalize (object);
}

static void
ide_search_result_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  IdeSearchResult *self = IDE_SEARCH_RESULT (object);

  switch (prop_id)
    {
    case PROP_ACCELERATOR:
      g_value_set_string (value, ide_search_result_get_accelerator (self));
      break;

    case PROP_PAINTABLE:
      g_value_set_object (value, ide_search_result_get_paintable (self));
      break;

    case PROP_GICON:
      g_value_set_object (value, ide_search_result_get_gicon (self));
      break;

    case PROP_PRIORITY:
      g_value_set_int (value, ide_search_result_get_priority (self));
      break;

    case PROP_SCORE:
      g_value_set_float (value, ide_search_result_get_score (self));
      break;

    case PROP_SUBTITLE:
      g_value_set_string (value, ide_search_result_get_subtitle (self));
      break;

    case PROP_TITLE:
      g_value_set_string (value, ide_search_result_get_title (self));
      break;

    case PROP_USE_MARKUP:
      g_value_set_boolean (value, ide_search_result_get_use_markup (self));
      break;

    case PROP_USE_UNDERLINE:
      g_value_set_boolean (value, ide_search_result_get_use_underline (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_search_result_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  IdeSearchResult *self = IDE_SEARCH_RESULT (object);

  switch (prop_id)
    {
    case PROP_ACCELERATOR:
      ide_search_result_set_accelerator (self, g_value_get_string (value));
      break;

    case PROP_PAINTABLE:
      ide_search_result_set_paintable (self, g_value_get_object (value));
      break;

    case PROP_GICON:
      ide_search_result_set_gicon (self, g_value_get_object (value));
      break;

    case PROP_PRIORITY:
      ide_search_result_set_priority (self, g_value_get_int (value));
      break;

    case PROP_SCORE:
      ide_search_result_set_score (self, g_value_get_float (value));
      break;

    case PROP_SUBTITLE:
      ide_search_result_set_subtitle (self, g_value_get_string (value));
      break;

    case PROP_TITLE:
      ide_search_result_set_title (self, g_value_get_string (value));
      break;

    case PROP_USE_MARKUP:
      ide_search_result_set_use_markup (self, g_value_get_boolean (value));
      break;

    case PROP_USE_UNDERLINE:
      ide_search_result_set_use_underline (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_search_result_class_init (IdeSearchResultClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_search_result_finalize;
  object_class->get_property = ide_search_result_get_property;
  object_class->set_property = ide_search_result_set_property;

  klass->matches = ide_search_result_real_matches;

  properties [PROP_ACCELERATOR] =
    g_param_spec_string ("accelerator", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_PAINTABLE] =
    g_param_spec_object ("paintable",
                         "Paintable",
                         "The paintable for the row icon",
                         GDK_TYPE_PAINTABLE,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_GICON] =
    g_param_spec_object ("gicon",
                         "GIcon",
                         "The GIcon for the row icon",
                         G_TYPE_ICON,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_PRIORITY] =
    g_param_spec_int ("priority",
                      "Priority",
                      "The priority of search result group",
                      G_MININT,
                      G_MAXINT,
                      0,
                      (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_SCORE] =
    g_param_spec_float ("score",
                        "Score",
                        "The score of the result",
                        -G_MINFLOAT,
                        G_MAXFLOAT,
                        0.0f,
                        (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "The title of the search result",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_SUBTITLE] =
    g_param_spec_string ("subtitle",
                         "Subtitle",
                         "The subtitle of the search result",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_USE_MARKUP] =
    g_param_spec_boolean ("use-markup", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_USE_UNDERLINE] =
    g_param_spec_boolean ("use-underline", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_search_result_init (IdeSearchResult *self)
{
}

IdeSearchResult *
ide_search_result_new (void)
{
  return g_object_new (IDE_TYPE_SEARCH_RESULT, NULL);
}

int
ide_search_result_compare (gconstpointer a,
                           gconstpointer b)
{
  IdeSearchResult *ra = (IdeSearchResult *)a;
  IdeSearchResult *rb = (IdeSearchResult *)b;
  IdeSearchResultPrivate *priva = ide_search_result_get_instance_private (ra);
  IdeSearchResultPrivate *privb = ide_search_result_get_instance_private (rb);

  if (priva->priority < privb->priority)
    return -1;
  else if (priva->priority > privb->priority)
    return 1;

  if (priva->score > privb->score)
    return -1;
  else if (priva->score < privb->score)
    return 1;

  return 0;
}

float
ide_search_result_get_score (IdeSearchResult *self)
{
  IdeSearchResultPrivate *priv = ide_search_result_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SEARCH_RESULT (self), 0.0f);

  return priv->score;
}

void
ide_search_result_set_score (IdeSearchResult *self,
                             float            score)
{
  IdeSearchResultPrivate *priv = ide_search_result_get_instance_private (self);

  g_return_if_fail (IDE_IS_SEARCH_RESULT (self));

  if (priv->score != score)
    {
      priv->score = score;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SCORE]);
    }
}

int
ide_search_result_get_priority (IdeSearchResult *self)
{
  IdeSearchResultPrivate *priv = ide_search_result_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SEARCH_RESULT (self), 0.0);

  return priv->priority;
}

void
ide_search_result_set_priority (IdeSearchResult *self,
                                int              priority)
{
  IdeSearchResultPrivate *priv = ide_search_result_get_instance_private (self);

  g_return_if_fail (IDE_IS_SEARCH_RESULT (self));

  if (priv->priority != priority)
    {
      priv->priority = priority;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PRIORITY]);
    }
}

/**
 * ide_search_result_activate:
 * @self: a #IdeSearchResult
 * @last_focus: a #GtkWidget of the last focus
 *
 * Requests that @self activate. @last_focus is provided so that the search
 * result may activate #GAction or other context-specific actions.
 */
void
ide_search_result_activate (IdeSearchResult *self,
                            GtkWidget       *last_focus)
{
  g_return_if_fail (IDE_IS_SEARCH_RESULT (self));
  g_return_if_fail (GTK_IS_WIDGET (last_focus));

  if (IDE_SEARCH_RESULT_GET_CLASS (self)->activate)
    IDE_SEARCH_RESULT_GET_CLASS (self)->activate (self, last_focus);
}

/**
 * ide_search_result_get_paintable:
 * @self: a #IdeSearchResult
 *
 * Gets the paintable for the row, if any.
 *
 * Returns: (transfer none) (nullable): a #GdkPaintable or %NULL
 */
GdkPaintable *
ide_search_result_get_paintable (IdeSearchResult *self)
{
  IdeSearchResultPrivate *priv = ide_search_result_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SEARCH_RESULT (self), NULL);

  return priv->paintable;
}

void
ide_search_result_set_paintable (IdeSearchResult *self,
                                 GdkPaintable    *paintable)
{
  IdeSearchResultPrivate *priv = ide_search_result_get_instance_private (self);

  g_return_if_fail (IDE_IS_SEARCH_RESULT (self));
  g_return_if_fail (!paintable || GDK_IS_PAINTABLE (paintable));

  if (g_set_object (&priv->paintable, paintable))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PAINTABLE]);
}

const char *
ide_search_result_get_subtitle (IdeSearchResult *self)
{
  IdeSearchResultPrivate *priv = ide_search_result_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SEARCH_RESULT (self), NULL);

  return priv->subtitle;
}

void
ide_search_result_set_subtitle (IdeSearchResult *self,
                             const char      *subtitle)
{
  IdeSearchResultPrivate *priv = ide_search_result_get_instance_private (self);

  g_return_if_fail (IDE_IS_SEARCH_RESULT (self));

  if (g_set_str (&priv->subtitle, subtitle))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SUBTITLE]);
    }
}

const char *
ide_search_result_get_title (IdeSearchResult *self)
{
  IdeSearchResultPrivate *priv = ide_search_result_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SEARCH_RESULT (self), NULL);

  return priv->title;
}

void
ide_search_result_set_title (IdeSearchResult *self,
                             const char      *title)
{
  IdeSearchResultPrivate *priv = ide_search_result_get_instance_private (self);

  g_return_if_fail (IDE_IS_SEARCH_RESULT (self));

  if (g_set_str (&priv->title, title))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TITLE]);
    }
}

void
ide_search_result_set_gicon (IdeSearchResult *self,
                             GIcon           *gicon)
{
  IdeSearchResultPrivate *priv = ide_search_result_get_instance_private (self);

  g_return_if_fail (IDE_IS_SEARCH_RESULT (self));
  g_return_if_fail (!gicon || G_IS_ICON (gicon));

  if (g_set_object (&priv->gicon, gicon))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_GICON]);
}

/**
 * ide_search_result_get_gicon:
 * @self: a #IdeSearchResult
 *
 * Gets the #GIcon for the search result, if any.
 *
 * Returns: (transfer none) (nullable): a #GIcon or %NULL
 */
GIcon *
ide_search_result_get_gicon (IdeSearchResult *self)
{
  IdeSearchResultPrivate *priv = ide_search_result_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SEARCH_RESULT (self), NULL);

  return priv->gicon;
}

gboolean
ide_search_result_get_use_underline (IdeSearchResult *self)
{
  IdeSearchResultPrivate *priv = ide_search_result_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SEARCH_RESULT (self), FALSE);

  return priv->use_underline;
}

void
ide_search_result_set_use_underline (IdeSearchResult *self,
                                     gboolean         use_underline)
{
  IdeSearchResultPrivate *priv = ide_search_result_get_instance_private (self);

  g_return_if_fail (IDE_IS_SEARCH_RESULT (self));

  use_underline = !!use_underline;

  if (priv->use_underline != use_underline)
    {
      priv->use_underline = use_underline;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_USE_UNDERLINE]);
    }
}

gboolean
ide_search_result_get_use_markup (IdeSearchResult *self)
{
  IdeSearchResultPrivate *priv = ide_search_result_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SEARCH_RESULT (self), FALSE);

  return priv->use_markup;
}

void
ide_search_result_set_use_markup (IdeSearchResult *self,
                                     gboolean         use_markup)
{
  IdeSearchResultPrivate *priv = ide_search_result_get_instance_private (self);

  g_return_if_fail (IDE_IS_SEARCH_RESULT (self));

  use_markup = !!use_markup;

  if (priv->use_markup != use_markup)
    {
      priv->use_markup = use_markup;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_USE_MARKUP]);
    }
}

const char *
ide_search_result_get_accelerator (IdeSearchResult *self)
{
  IdeSearchResultPrivate *priv = ide_search_result_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SEARCH_RESULT (self), NULL);

  return priv->accelerator;
}

void
ide_search_result_set_accelerator (IdeSearchResult *self,
                                   const char      *accelerator)
{
  IdeSearchResultPrivate *priv = ide_search_result_get_instance_private (self);

  g_return_if_fail (IDE_IS_SEARCH_RESULT (self));

  if (g_set_str (&priv->accelerator, accelerator))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ACCELERATOR]);
}

/**
 * ide_search_result_load_preview:
 * @self: a #IdeSearchResult
 * @context: an #IdeContext
 *
 * Gets a preview widget for the search result, if any.
 *
 * Returns: (transfer full) (nullable): an #IdeSearchPreview, or %NULL
 */
IdeSearchPreview *
ide_search_result_load_preview (IdeSearchResult *self,
                                IdeContext      *context)
{
  g_return_val_if_fail (IDE_IS_SEARCH_RESULT (self), NULL);
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  if (IDE_SEARCH_RESULT_GET_CLASS (self)->load_preview)
    return IDE_SEARCH_RESULT_GET_CLASS (self)->load_preview (self, context);

  return NULL;
}
