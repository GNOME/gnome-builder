/* gb-search-result.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#include "gb-search-result.h"

struct _GbSearchResultPrivate
{
  gint   priority;
  gfloat score;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbSearchResult, gb_search_result, GTK_TYPE_BIN)

enum {
  PROP_0,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

GtkWidget *
gb_search_result_new (void)
{
  return g_object_new (GB_TYPE_SEARCH_RESULT, NULL);
}

gint
gb_search_result_compare_func (gconstpointer result1,
                               gconstpointer result2)
{
  const GbSearchResult *r1 = result1;
  const GbSearchResult *r2 = result2;
  gint ret;

  ret = r1->priv->priority - r2->priv->priority;

  if (ret == 0)
    {
      if (r1->priv->score > r2->priv->score)
        return 1;
      else if (r1->priv->score < r2->priv->score)
        return -1;
    }

  return ret;
}

static void
gb_search_result_finalize (GObject *object)
{
  G_OBJECT_CLASS (gb_search_result_parent_class)->finalize (object);
}

static void
gb_search_result_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  //GbSearchResult *self = GB_SEARCH_RESULT (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_search_result_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  //GbSearchResult *self = GB_SEARCH_RESULT (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_search_result_class_init (GbSearchResultClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gb_search_result_finalize;
  object_class->get_property = gb_search_result_get_property;
  object_class->set_property = gb_search_result_set_property;
}

static void
gb_search_result_init (GbSearchResult *self)
{
  self->priv = gb_search_result_get_instance_private (self);
}
