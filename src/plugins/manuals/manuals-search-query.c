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

#include "manuals-book.h"
#include "manuals-gom.h"
#include "manuals-keyword.h"
#include "manuals-repository.h"
#include "manuals-sdk.h"
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

static void
manuals_search_query_get_section (GtkSectionModel *model,
                                  guint            position,
                                  guint           *out_start,
                                  guint           *out_end)
{
  ManualsSearchQuery *self = MANUALS_SEARCH_QUERY (model);

  if (self->state == STATE_COMPLETED && self->results != NULL)
    return gtk_section_model_get_section (GTK_SECTION_MODEL (self->results),
                                          position,
                                          out_start,
                                          out_end);

  *out_start = *out_end = 0;
}

static void
section_model_iface_init (GtkSectionModelInterface *iface)
{
  iface->get_section = manuals_search_query_get_section;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (ManualsSearchQuery, manuals_search_query, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init)
                               G_IMPLEMENT_INTERFACE (GTK_TYPE_SECTION_MODEL, section_model_iface_init))


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
      gtk_section_model_sections_changed (GTK_SECTION_MODEL (self), 0, new_len);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_N_ITEMS]);
    }

  return dex_future_new_for_boolean (TRUE);
}

typedef struct
{
  ManualsRepository *repository;
  GomFilter *keyword_filter;
} Execute;

static void
execute_free (Execute *execute)
{
  g_clear_object (&execute->keyword_filter);
  g_clear_object (&execute->repository);
  g_free (execute);
}

static DexFuture *
manuals_search_query_execute_fiber (gpointer user_data)
{
  g_autoptr(GListModel) sdks = NULL;
  g_autoptr(GListStore) store = NULL;
  g_autoptr(GPtrArray) futures = NULL;
  g_autoptr(GError) error = NULL;
  Execute *execute = user_data;
  guint n_sdks;

  g_assert (execute != NULL);
  g_assert (GOM_IS_FILTER (execute->keyword_filter));
  g_assert (MANUALS_IS_REPOSITORY (execute->repository));

  if (!(sdks = dex_await_object (manuals_repository_list_sdks_by_newest (execute->repository), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  futures = g_ptr_array_new_with_free_func (dex_unref);
  n_sdks = g_list_model_get_n_items (sdks);

  for (guint i = 0; i < n_sdks; i++)
    {
      g_autoptr(ManualsSdk) sdk = g_list_model_get_item (sdks, i);
      g_autoptr(GArray) values = g_array_new (FALSE, TRUE, sizeof (GValue));
      g_autoptr(GString) str = g_string_new ("\"book-id\" IN (");
      g_autoptr(GListModel) books = NULL;
      g_autoptr(GomFilter) book_filter = NULL;
      g_autoptr(GomFilter) filter = NULL;
      guint n_books;

      if (!(books = dex_await_object (manuals_sdk_list_books (sdk), NULL)) ||
          (n_books = g_list_model_get_n_items (books)) == 0)
        continue;

      for (guint j = 0; j < n_books; j++)
        {
          g_autoptr(ManualsBook) book = g_list_model_get_item (books, j);
          GValue value = G_VALUE_INIT;

          g_value_init (&value, G_TYPE_INT64);
          g_value_set_int64 (&value, manuals_book_get_id (book));

          g_array_append_val (values, value);
          g_string_append_c (str, '?');

          if (j + 1 < n_books)
            g_string_append_c (str, ',');
        }

      g_string_append_c (str, ')');

      book_filter = gom_filter_new_sql (str->str, values);
      filter = gom_filter_new_and (book_filter, execute->keyword_filter);

      g_ptr_array_add (futures,
                       gom_repository_find (GOM_REPOSITORY (execute->repository),
                                            MANUALS_TYPE_KEYWORD,
                                            filter));
    }

  if (futures->len == 0)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_SUPPORTED,
                                  "Not supported");

  if (!dex_await (dex_future_allv ((DexFuture **)futures->pdata, futures->len), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  store = g_list_store_new (G_TYPE_LIST_MODEL);

  for (guint i = 0; i < futures->len; i++)
    {
      DexFuture *future = g_ptr_array_index (futures, i);
      GomResourceGroup *group = g_value_get_object (dex_future_get_value (future, NULL));
      g_autoptr(ManualsSearchModel) wrapped = manuals_search_model_new (group);

      g_list_store_append (store, wrapped);
    }

  return dex_future_new_take_object (gtk_flatten_list_model_new (g_object_ref (G_LIST_MODEL (store))));
}

DexFuture *
manuals_search_query_execute (ManualsSearchQuery *self,
                              ManualsRepository  *repository)
{
  g_autoptr(GListStore) store = NULL;
  g_auto(GValue) like_value = G_VALUE_INIT;
  DexFuture *future;
  Execute *execute;

  g_return_val_if_fail (MANUALS_IS_SEARCH_QUERY (self), NULL);
  g_return_val_if_fail (MANUALS_IS_REPOSITORY (repository), NULL);

  if (self->state != STATE_INITIAL || _g_str_empty0 (self->text))
    return dex_future_new_for_boolean (TRUE);

  self->state = STATE_RUNNING;

  g_value_init (&like_value, G_TYPE_STRING);
  g_value_take_string (&like_value, like_string (self->text));

  execute = g_new0 (Execute, 1);
  execute->keyword_filter = gom_filter_new_like (MANUALS_TYPE_KEYWORD, "name", &like_value);
  execute->repository = g_object_ref (repository);

  future = dex_scheduler_spawn (NULL, 0,
                                manuals_search_query_execute_fiber,
                                execute,
                                (GDestroyNotify)execute_free);
  future = dex_future_then (future,
                            manuals_search_query_propagate_cb,
                            g_object_ref (self),
                            g_object_unref);

  return future;
}
