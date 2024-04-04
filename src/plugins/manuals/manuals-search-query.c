/*
 * manuals-search-query.c
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

#include <gtk/gtk.h>

#include "manuals-gom.h"
#include "manuals-keyword.h"
#include "manuals-repository.h"
#include "manuals-search-model.h"
#include "manuals-search-result.h"
#include "manuals-search-query.h"
#include "manuals-utils.h"

struct _ManualsSearchQuery
{
  GObject parent_instance;
  GListModel *results;
  char *text;
  guint state : 2;
};

enum {
  STATE_INITIAL,
  STATE_RUNNING,
  STATE_COMPLETED,
};

static GType
manuals_search_query_get_item_type (GListModel *model)
{
  return MANUALS_TYPE_SEARCH_RESULT;
}

static guint
manuals_search_query_get_n_items (GListModel *model)
{
  ManualsSearchQuery *self = MANUALS_SEARCH_QUERY (model);

  if (self->state == STATE_COMPLETED && self->results != NULL)
    return g_list_model_get_n_items (self->results);

  return 0;
}

static gpointer
manuals_search_query_get_item (GListModel *model,
                               guint       position)
{
  ManualsSearchQuery *self = MANUALS_SEARCH_QUERY (model);

  if (self->state == STATE_COMPLETED && self->results != NULL)
    return g_list_model_get_item (self->results, position);

  return NULL;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = manuals_search_query_get_item_type;
  iface->get_item = manuals_search_query_get_item;
  iface->get_n_items = manuals_search_query_get_n_items;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (ManualsSearchQuery, manuals_search_query, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

enum {
  PROP_0,
  PROP_N_ITEMS,
  PROP_TEXT,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

ManualsSearchQuery *
manuals_search_query_new (void)
{
  return g_object_new (MANUALS_TYPE_SEARCH_QUERY, NULL);
}

static void
manuals_search_query_dispose (GObject *object)
{
  ManualsSearchQuery *self = (ManualsSearchQuery *)object;

  g_clear_object (&self->results);
  g_clear_pointer (&self->text, g_free);

  G_OBJECT_CLASS (manuals_search_query_parent_class)->dispose (object);
}

static void
manuals_search_query_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ManualsSearchQuery *self = MANUALS_SEARCH_QUERY (object);

  switch (prop_id)
    {
    case PROP_N_ITEMS:
      g_value_set_uint (value, g_list_model_get_n_items (G_LIST_MODEL (self)));
      break;

    case PROP_TEXT:
      g_value_set_string (value, manuals_search_query_get_text (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
manuals_search_query_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ManualsSearchQuery *self = MANUALS_SEARCH_QUERY (object);

  switch (prop_id)
    {
    case PROP_TEXT:
      manuals_search_query_set_text (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
manuals_search_query_class_init (ManualsSearchQueryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = manuals_search_query_dispose;
  object_class->get_property = manuals_search_query_get_property;
  object_class->set_property = manuals_search_query_set_property;

  properties[PROP_N_ITEMS] =
    g_param_spec_uint ("n-items", NULL, NULL,
                       0, G_MAXUINT-1, 0,
                       (G_PARAM_READABLE |
                        G_PARAM_STATIC_STRINGS));

  properties[PROP_TEXT] =
    g_param_spec_string ("text", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
manuals_search_query_init (ManualsSearchQuery *self)
{
}

const char *
manuals_search_query_get_text (ManualsSearchQuery *self)
{
  g_return_val_if_fail (MANUALS_IS_SEARCH_QUERY (self), NULL);

  return self->text;
}

void
manuals_search_query_set_text (ManualsSearchQuery *self,
                               const char         *text)
{
  g_return_if_fail (MANUALS_IS_SEARCH_QUERY (self));

  if (g_set_str (&self->text, text))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TEXT]);
}

static char *
like_string (const char *str)
{
  GString *gstr;

  if (_g_str_empty0 (str))
    return g_strdup ("%");

  gstr = g_string_new (NULL);
  g_string_append_c (gstr, '%');

  if (str != NULL)
    {
      g_string_append (gstr, str);
      g_string_append_c (gstr, '%');
      g_string_replace (gstr, " ", "%", 0);
    }

  return g_string_free (gstr, FALSE);
}

static DexFuture *
manuals_search_query_execute_cb (DexFuture *completed,
                                 gpointer   user_data)
{
  g_autoptr(ManualsSearchModel) model = NULL;
  GListStore *store = user_data;
  GomResourceGroup *group;
  const GValue *value;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (G_IS_LIST_STORE (store));

  value = dex_future_get_value (completed, NULL);
  group = g_value_get_object (value);
  model = manuals_search_model_new (group);

  g_list_store_append (store, model);

  return dex_future_new_for_boolean (TRUE);
}

static DexFuture *
manuals_search_query_completed_cb (DexFuture *completed,
                                   gpointer   user_data)
{
  g_autoptr(GtkFlattenListModel) model = NULL;
  GListStore *store = user_data;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (G_IS_LIST_STORE (store));

  return dex_future_new_take_object (gtk_flatten_list_model_new (g_object_ref (G_LIST_MODEL (store))));
}

static DexFuture *
manuals_search_query_propagate_cb (DexFuture *completed,
                                   gpointer   user_data)
{
  ManualsSearchQuery *self = user_data;
  GListModel *model;
  guint new_len;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (MANUALS_IS_SEARCH_QUERY (self));

  self->state = STATE_COMPLETED;

  model = g_value_get_object (dex_future_get_value (completed, NULL));
  g_set_object (&self->results, model);
  new_len = g_list_model_get_n_items (G_LIST_MODEL (model));

  if (new_len)
    {
      g_list_model_items_changed (G_LIST_MODEL (self), 0, 0, new_len);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_N_ITEMS]);
    }

  return dex_future_new_for_boolean (TRUE);
}

DexFuture *
manuals_search_query_execute (ManualsSearchQuery *self,
                              ManualsRepository  *repository)
{
  g_autoptr(GListStore) store = NULL;
  g_autoptr(GomFilter) keyword_filter = NULL;
  g_auto(GValue) like_value = G_VALUE_INIT;
  DexFuture *future;

  g_return_val_if_fail (MANUALS_IS_SEARCH_QUERY (self), NULL);
  g_return_val_if_fail (MANUALS_IS_REPOSITORY (repository), NULL);

  if (self->state != STATE_INITIAL || _g_str_empty0 (self->text))
    return dex_future_new_for_boolean (TRUE);

  self->state = STATE_RUNNING;

  store = g_list_store_new (G_TYPE_LIST_MODEL);

  g_value_init (&like_value, G_TYPE_STRING);
  g_value_take_string (&like_value, like_string (self->text));
  keyword_filter = gom_filter_new_like (MANUALS_TYPE_KEYWORD, "name", &like_value);

  future = dex_future_then (gom_repository_find (GOM_REPOSITORY (repository), MANUALS_TYPE_KEYWORD, keyword_filter),
                            manuals_search_query_execute_cb,
                            g_object_ref (store),
                            g_object_unref);
  future = dex_future_then (future,
                            manuals_search_query_completed_cb,
                            g_object_ref (store),
                            g_object_unref);
  future = dex_future_then (future,
                            manuals_search_query_propagate_cb,
                            g_object_ref (self),
                            g_object_unref);

  return future;
}
