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

#include "ide-search-result.h"

typedef struct
{
  GIcon  *icon;
  gfloat  score;
  guint   priority;
} IdeSearchResultPrivate;

enum {
  PROP_0,
  PROP_SCORE,
  PROP_PRIORITY,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (IdeSearchResult, ide_search_result, DZL_TYPE_SUGGESTION)

static GParamSpec *properties [N_PROPS];

static GIcon *
ide_search_result_real_get_icon (DzlSuggestion *suggestion)
{
  IdeSearchResult *self = (IdeSearchResult *)suggestion;
  IdeSearchResultPrivate *priv = ide_search_result_get_instance_private (self);

  g_assert (IDE_IS_SEARCH_RESULT (self));

  if (priv->icon != NULL)
    return g_object_ref (priv->icon);

  return DZL_SUGGESTION_CLASS (ide_search_result_parent_class)->get_icon (suggestion);
}

static void
ide_search_result_finalize (GObject *object)
{
  IdeSearchResult *self = (IdeSearchResult *)object;
  IdeSearchResultPrivate *priv = ide_search_result_get_instance_private (self);

  g_clear_object (&priv->icon);

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
    case PROP_SCORE:
      g_value_set_float (value, ide_search_result_get_score (self));
      break;

    case PROP_PRIORITY:
      g_value_set_int (value, ide_search_result_get_priority (self));
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
    case PROP_SCORE:
      ide_search_result_set_score (self, g_value_get_float (value));
      break;

    case PROP_PRIORITY:
      ide_search_result_set_priority (self, g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_search_result_class_init (IdeSearchResultClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  DzlSuggestionClass *suggestion_class = DZL_SUGGESTION_CLASS (klass);

  object_class->finalize = ide_search_result_finalize;
  object_class->get_property = ide_search_result_get_property;
  object_class->set_property = ide_search_result_set_property;

  suggestion_class->get_icon = ide_search_result_real_get_icon;

  properties [PROP_SCORE] =
    g_param_spec_float ("score",
                        "Score",
                        "The score of the result",
                        -G_MINFLOAT,
                        G_MAXFLOAT,
                        0.0f,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_PRIORITY] =
    g_param_spec_int ("priority",
                      "Priority",
                      "The priority of search result group",
                      G_MININT,
                      G_MAXINT,
                      0,
                      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

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

gint
ide_search_result_compare (gconstpointer a,
                           gconstpointer b)
{
  IdeSearchResult *ra = (IdeSearchResult *)a;
  IdeSearchResult *rb = (IdeSearchResult *)b;
  IdeSearchResultPrivate *priva = ide_search_result_get_instance_private (ra);
  IdeSearchResultPrivate *privb = ide_search_result_get_instance_private (rb);
  gint ret;

  ret = priva->priority - privb->priority;

  if (ret == 0)
    {
      if (priva->score < privb->score)
        priva->score = -1;
      else if (priva->score > privb->score)
        priva->score = 1;
    }

  return ret;
}

gfloat
ide_search_result_get_score (IdeSearchResult *self)
{
  IdeSearchResultPrivate *priv = ide_search_result_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SEARCH_RESULT (self), 0.0f);

  return priv->score;
}

void
ide_search_result_set_score (IdeSearchResult *self,
                             gfloat           score)
{
  IdeSearchResultPrivate *priv = ide_search_result_get_instance_private (self);

  g_return_if_fail (IDE_IS_SEARCH_RESULT (self));

  if (priv->score != score)
    {
      priv->score = score;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SCORE]);
    }
}

gint
ide_search_result_get_priority (IdeSearchResult *self)
{
  IdeSearchResultPrivate *priv = ide_search_result_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SEARCH_RESULT (self), 0.0);

  return priv->priority;
}

void
ide_search_result_set_priority (IdeSearchResult *self,
                                gint             priority)
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
 *
 * Since: 3.32
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

void
ide_search_result_set_icon (IdeSearchResult *self,
                            GIcon           *icon)
{
  IdeSearchResultPrivate *priv = ide_search_result_get_instance_private (self);

  g_return_if_fail (IDE_IS_SEARCH_RESULT (self));

  /* don't notify, since it's not really needed */
  g_set_object (&priv->icon, icon);
}
