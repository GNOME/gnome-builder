/*
 * manuals-search-result.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#include "manuals-search-result.h"

struct _ManualsSearchResult
{
  GObject  parent_instance;
  GObject *item;
  guint    position;
};

G_DEFINE_FINAL_TYPE (ManualsSearchResult, manuals_search_result, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_ITEM,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

ManualsSearchResult *
manuals_search_result_new (guint position)
{
  ManualsSearchResult *self;

  self = g_object_new (MANUALS_TYPE_SEARCH_RESULT,
                       NULL);
  self->position = position;

  return self;
}

static void
manuals_search_result_dispose (GObject *object)
{
  ManualsSearchResult *self = (ManualsSearchResult *)object;

  g_clear_object (&self->item);

  G_OBJECT_CLASS (manuals_search_result_parent_class)->dispose (object);
}

static void
manuals_search_result_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  ManualsSearchResult *self = MANUALS_SEARCH_RESULT (object);

  switch (prop_id)
    {
    case PROP_ITEM:
      g_value_set_object (value, manuals_search_result_get_item (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
manuals_search_result_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  ManualsSearchResult *self = MANUALS_SEARCH_RESULT (object);

  switch (prop_id)
    {
    case PROP_ITEM:
      manuals_search_result_set_item (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
manuals_search_result_class_init (ManualsSearchResultClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = manuals_search_result_dispose;
  object_class->get_property = manuals_search_result_get_property;
  object_class->set_property = manuals_search_result_set_property;

  properties[PROP_ITEM] =
    g_param_spec_object ("item", NULL, NULL,
                         G_TYPE_OBJECT,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
manuals_search_result_init (ManualsSearchResult *self)
{
}

gpointer
manuals_search_result_get_item (ManualsSearchResult *self)
{
  g_return_val_if_fail (MANUALS_IS_SEARCH_RESULT (self), NULL);

  return self->item;
}

void
manuals_search_result_set_item (ManualsSearchResult *self,
                                gpointer             item)
{
  g_return_if_fail (MANUALS_IS_SEARCH_RESULT (self));
  g_return_if_fail (!item || G_IS_OBJECT (item));

  if (g_set_object (&self->item, item))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ITEM]);
}

guint
manuals_search_result_get_position (ManualsSearchResult *self)
{
  g_return_val_if_fail (MANUALS_IS_SEARCH_RESULT (self), 0);

  return self->position;
}
