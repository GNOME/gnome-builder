/* ide-search-result.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include "ide-context.h"
#include "ide-search-result.h"

typedef struct
{
  gchar  *title;
  gchar  *subtitle;
  gfloat  score;
} IdeSearchResultPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeSearchResult, ide_search_result, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_SCORE,
  PROP_SUBTITLE,
  PROP_TITLE,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

IdeSearchResult *
ide_search_result_new (IdeContext  *context,
                       const gchar *title,
                       const gchar *subtitle,
                       gfloat       score)
{
  IdeSearchResult *self;

  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  self = g_object_new (IDE_TYPE_SEARCH_RESULT,
                       "context", context,
                       "title", title,
                       "subtitle", subtitle,
                       "score", score,
                       NULL);

  return self;
}

const gchar *
ide_search_result_get_title (IdeSearchResult *self)
{
  IdeSearchResultPrivate *priv = ide_search_result_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SEARCH_RESULT (self), NULL);

  return priv->title;
}

const gchar *
ide_search_result_get_subtitle (IdeSearchResult *self)
{
  IdeSearchResultPrivate *priv = ide_search_result_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SEARCH_RESULT (self), NULL);

  return priv->subtitle;
}

gfloat
ide_search_result_get_score (IdeSearchResult *self)
{
  IdeSearchResultPrivate *priv = ide_search_result_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SEARCH_RESULT (self), 0.0);

  return priv->score;
}

static void
ide_search_result_set_title (IdeSearchResult *self,
                             const gchar     *title)
{
  IdeSearchResultPrivate *priv = ide_search_result_get_instance_private (self);

  g_return_if_fail (IDE_IS_SEARCH_RESULT (self));

  if (priv->title != title)
    {
      g_free (priv->title);
      priv->title = g_strdup (title);
    }
}

static void
ide_search_result_set_subtitle (IdeSearchResult *self,
                                const gchar     *subtitle)
{
  IdeSearchResultPrivate *priv = ide_search_result_get_instance_private (self);

  g_return_if_fail (IDE_IS_SEARCH_RESULT (self));

  if (priv->subtitle != subtitle)
    {
      g_free (priv->subtitle);
      priv->subtitle = g_strdup (subtitle);
    }
}

static void
ide_search_result_set_score (IdeSearchResult *self,
                             gfloat           score)
{
  IdeSearchResultPrivate *priv = ide_search_result_get_instance_private (self);

  g_return_if_fail (IDE_IS_SEARCH_RESULT (self));
  g_return_if_fail (score >= 0.0);
  g_return_if_fail (score <= 1.0);

  priv->score = score;
}

gint
ide_search_result_compare (const IdeSearchResult *a,
                           const IdeSearchResult *b)
{
  gfloat fa;
  gfloat fb;

  g_return_val_if_fail (IDE_IS_SEARCH_RESULT ((IdeSearchResult*)a), 0);
  g_return_val_if_fail (IDE_IS_SEARCH_RESULT ((IdeSearchResult*)b), 0);

  fa = ide_search_result_get_score ((IdeSearchResult *)a);
  fb = ide_search_result_get_score ((IdeSearchResult *)b);

  if (fa < fb)
    return -1;
  else if (fa > fb)
    return 1;
  else
    return 0;
}

static void
ide_search_result_finalize (GObject *object)
{
  IdeSearchResult *self = (IdeSearchResult *)object;
  IdeSearchResultPrivate *priv = ide_search_result_get_instance_private (self);

  g_clear_pointer (&priv->title, g_free);
  g_clear_pointer (&priv->subtitle, g_free);

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
    case PROP_TITLE:
      g_value_set_string (value, ide_search_result_get_title (self));
      break;

    case PROP_SUBTITLE:
      g_value_set_string (value, ide_search_result_get_subtitle (self));
      break;

    case PROP_SCORE:
      g_value_set_float (value, ide_search_result_get_score (self));
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
    case PROP_TITLE:
      ide_search_result_set_title (self, g_value_get_string (value));
      break;

    case PROP_SUBTITLE:
      ide_search_result_set_subtitle (self, g_value_get_string (value));
      break;

    case PROP_SCORE:
      ide_search_result_set_score (self, g_value_get_float (value));
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

  gParamSpecs [PROP_TITLE] =
    g_param_spec_string ("title",
                         _("Title"),
                         _("The title of the search result."),
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_TITLE,
                                   gParamSpecs [PROP_TITLE]);

  gParamSpecs [PROP_SUBTITLE] =
    g_param_spec_string ("subtitle",
                         _("Subtitle"),
                         _("The subtitle of the search result."),
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SUBTITLE,
                                   gParamSpecs [PROP_SUBTITLE]);

  gParamSpecs [PROP_SCORE] =
    g_param_spec_float ("score",
                        _("Score"),
                        _("The score of the search result."),
                        0.0,
                        1.0,
                        0.0,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SCORE,
                                   gParamSpecs [PROP_SCORE]);
}

static void
ide_search_result_init (IdeSearchResult *self)
{
}
