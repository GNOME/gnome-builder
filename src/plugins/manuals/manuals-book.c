/*
 * manuals-book.c
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

#include <glib/gi18n.h>

#include "manuals-book.h"
#include "manuals-heading.h"
#include "manuals-navigatable.h"
#include "manuals-repository.h"

struct _ManualsBook
{
  GomResource parent_instance;
  gint64 id;
  gint64 sdk_id;
  char *etag;
  char *language;
  char *online_uri;
  char *title;
  char *uri;
  char *default_uri;
};

enum {
  PROP_0,
  PROP_DEFAULT_URI,
  PROP_ETAG,
  PROP_ID,
  PROP_LANGUAGE,
  PROP_ONLINE_URI,
  PROP_SDK_ID,
  PROP_TITLE,
  PROP_URI,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (ManualsBook, manuals_book, GOM_TYPE_RESOURCE)

static GParamSpec *properties [N_PROPS];

static void
manuals_book_finalize (GObject *object)
{
  ManualsBook *self = (ManualsBook *)object;

  g_clear_pointer (&self->default_uri, g_free);
  g_clear_pointer (&self->etag, g_free);
  g_clear_pointer (&self->language, g_free);
  g_clear_pointer (&self->online_uri, g_free);
  g_clear_pointer (&self->title, g_free);
  g_clear_pointer (&self->uri, g_free);

  G_OBJECT_CLASS (manuals_book_parent_class)->finalize (object);
}

static void
manuals_book_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  ManualsBook *self = MANUALS_BOOK (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_int64 (value, manuals_book_get_id (self));
      break;

    case PROP_DEFAULT_URI:
      g_value_set_string (value, manuals_book_get_default_uri (self));
      break;

    case PROP_ETAG:
      g_value_set_string (value, manuals_book_get_etag (self));
      break;

    case PROP_LANGUAGE:
      g_value_set_string (value, manuals_book_get_language (self));
      break;

    case PROP_ONLINE_URI:
      g_value_set_string (value, manuals_book_get_online_uri (self));
      break;

    case PROP_SDK_ID:
      g_value_set_int64 (value, manuals_book_get_sdk_id (self));
      break;

    case PROP_TITLE:
      g_value_set_string (value, manuals_book_get_title (self));
      break;

    case PROP_URI:
      g_value_set_string (value, manuals_book_get_uri (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
manuals_book_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  ManualsBook *self = MANUALS_BOOK (object);

  switch (prop_id)
    {
    case PROP_ID:
      manuals_book_set_id (self, g_value_get_int64 (value));
      break;

    case PROP_DEFAULT_URI:
      manuals_book_set_default_uri (self, g_value_get_string (value));
      break;

    case PROP_ETAG:
      manuals_book_set_etag (self, g_value_get_string (value));
      break;

    case PROP_LANGUAGE:
      manuals_book_set_language (self, g_value_get_string (value));
      break;

    case PROP_ONLINE_URI:
      manuals_book_set_online_uri (self, g_value_get_string (value));
      break;

    case PROP_SDK_ID:
      manuals_book_set_sdk_id (self, g_value_get_int64 (value));
      break;

    case PROP_TITLE:
      manuals_book_set_title (self, g_value_get_string (value));
      break;

    case PROP_URI:
      manuals_book_set_uri (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
manuals_book_class_init (ManualsBookClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomResourceClass *resource_class = GOM_RESOURCE_CLASS (klass);

  object_class->finalize = manuals_book_finalize;
  object_class->get_property = manuals_book_get_property;
  object_class->set_property = manuals_book_set_property;

  properties[PROP_ID] =
    g_param_spec_int64 ("id", NULL, NULL,
                        0, G_MAXINT64, 0,
                        (G_PARAM_READWRITE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  properties[PROP_DEFAULT_URI] =
    g_param_spec_string ("default-uri", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_ETAG] =
    g_param_spec_string ("etag", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_LANGUAGE] =
    g_param_spec_string ("language", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_ONLINE_URI] =
    g_param_spec_string ("online-uri", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_SDK_ID] =
    g_param_spec_int64 ("sdk-id", NULL, NULL,
                        0, G_MAXINT64, 0,
                        (G_PARAM_READWRITE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  properties[PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_URI] =
    g_param_spec_string ("uri", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gom_resource_class_set_table (resource_class, "books");
  gom_resource_class_set_primary_key (resource_class, "id");
  gom_resource_class_set_reference (resource_class, "sdk-id", "sdks", "id");
  gom_resource_class_set_notnull (resource_class, "title");
  gom_resource_class_set_notnull (resource_class, "uri");
}

static void
manuals_book_init (ManualsBook *self)
{
}

gint64
manuals_book_get_id (ManualsBook *self)
{
  g_return_val_if_fail (MANUALS_IS_BOOK (self), 0);

  return self->id;
}

gint64
manuals_book_get_sdk_id (ManualsBook *self)
{
  g_return_val_if_fail (MANUALS_IS_BOOK (self), 0);

  return self->sdk_id;
}

const char *
manuals_book_get_default_uri (ManualsBook *self)
{
  g_return_val_if_fail (MANUALS_IS_BOOK (self), NULL);

  return self->default_uri;
}

const char *
manuals_book_get_etag (ManualsBook *self)
{
  g_return_val_if_fail (MANUALS_IS_BOOK (self), NULL);

  return self->etag;
}

const char *
manuals_book_get_language (ManualsBook *self)
{
  g_return_val_if_fail (MANUALS_IS_BOOK (self), NULL);

  return self->language;
}

const char *
manuals_book_get_online_uri (ManualsBook *self)
{
  g_return_val_if_fail (MANUALS_IS_BOOK (self), NULL);

  return self->online_uri;
}

const char *
manuals_book_get_title (ManualsBook *self)
{
  g_return_val_if_fail (MANUALS_IS_BOOK (self), NULL);

  return self->title;
}

const char *
manuals_book_get_uri (ManualsBook *self)
{
  g_return_val_if_fail (MANUALS_IS_BOOK (self), NULL);

  return self->uri;
}

void
manuals_book_set_id (ManualsBook *self,
                     gint64       id)
{
  g_return_if_fail (MANUALS_IS_BOOK (self));

  if (id != self->id)
    {
      self->id = id;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ID]);
    }
}

void
manuals_book_set_sdk_id (ManualsBook *self,
                         gint64       sdk_id)
{
  g_return_if_fail (MANUALS_IS_BOOK (self));

  if (sdk_id != self->sdk_id)
    {
      self->sdk_id = sdk_id;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SDK_ID]);
    }
}

void
manuals_book_set_default_uri (ManualsBook *self,
                              const char  *default_uri)
{
  g_return_if_fail (MANUALS_IS_BOOK (self));

  if (g_set_str (&self->default_uri, default_uri))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DEFAULT_URI]);
}

void
manuals_book_set_etag (ManualsBook *self,
                       const char  *etag)
{
  g_return_if_fail (MANUALS_IS_BOOK (self));

  if (g_set_str (&self->etag, etag))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ETAG]);
}

void
manuals_book_set_online_uri (ManualsBook *self,
                             const char  *online_uri)
{
  g_return_if_fail (MANUALS_IS_BOOK (self));

  if (g_set_str (&self->online_uri, online_uri))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ONLINE_URI]);
}

void
manuals_book_set_title (ManualsBook *self,
                        const char  *title)
{
  g_return_if_fail (MANUALS_IS_BOOK (self));

  if (g_set_str (&self->title, title))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TITLE]);
}

void
manuals_book_set_uri (ManualsBook *self,
                      const char  *uri)
{
  g_return_if_fail (MANUALS_IS_BOOK (self));

  if (g_set_str (&self->uri, uri))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_URI]);
}

void
manuals_book_set_language (ManualsBook *self,
                           const char  *language)
{
  g_return_if_fail (MANUALS_IS_BOOK (self));

  if (g_set_str (&self->language, language))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LANGUAGE]);
}

DexFuture *
manuals_book_list_headings (ManualsBook *self)
{
  g_autoptr(ManualsRepository) repository = NULL;
  g_autoptr(GomFilter) book_id = NULL;
  g_autoptr(GomFilter) parent_id = NULL;
  g_autoptr(GomFilter) and = NULL;
  g_auto(GValue) value = G_VALUE_INIT;

  g_return_val_if_fail (MANUALS_IS_BOOK (self), NULL);

  g_object_get (self, "repository", &repository, NULL);

  if (repository == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_SUPPORTED,
                                  "No repository to query");

  g_value_init (&value, G_TYPE_INT64);
  g_value_set_int64 (&value, self->id);
  book_id = gom_filter_new_eq (MANUALS_TYPE_HEADING, "book-id", &value);

  g_value_set_int64 (&value, 0);
  parent_id = gom_filter_new_eq (MANUALS_TYPE_HEADING, "parent-id", &value);

  and = gom_filter_new_and (book_id, parent_id);

  return manuals_repository_list (repository, MANUALS_TYPE_HEADING, and);
}

DexFuture *
manuals_book_find_sdk (ManualsBook *self)
{
  g_autoptr(ManualsRepository) repository = NULL;
  g_autoptr(GomFilter) sdk_id = NULL;
  g_auto(GValue) value = G_VALUE_INIT;

  g_return_val_if_fail (MANUALS_IS_BOOK (self), NULL);

  g_object_get (self, "repository", &repository, NULL);

  if (repository == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_SUPPORTED,
                                  "No repository to query");

  g_value_init (&value, G_TYPE_INT64);
  g_value_set_int64 (&value, self->sdk_id);
  sdk_id = gom_filter_new_eq (MANUALS_TYPE_SDK, "id", &value);

  return manuals_repository_find_one (repository, MANUALS_TYPE_SDK, sdk_id);
}

static DexFuture *
manuals_book_list_alternates_fiber (gpointer data)
{
  ManualsBook *self = data;
  g_autoptr(ManualsRepository) repository = NULL;
  g_autoptr(GomFilter) books_filter = NULL;
  g_autoptr(GListStore) store = NULL;
  g_autoptr(GListModel) books = NULL;
  g_auto(GValue) title_value = G_VALUE_INIT;
  guint n_books;

  g_assert (MANUALS_IS_BOOK (self));

  store = g_list_store_new (MANUALS_TYPE_NAVIGATABLE);
  g_object_get (self, "repository", &repository, NULL);
  if (repository == NULL)
    goto failure;

  /* Create filter where book title is same */
  g_value_init (&title_value, G_TYPE_STRING);
  g_value_set_string (&title_value, self->title);
  books_filter = gom_filter_new_eq (MANUALS_TYPE_BOOK, "title", &title_value);

  /* Now find other books that have the same title */
  if (!(books = dex_await_object (manuals_repository_list (repository,
                                                           MANUALS_TYPE_BOOK,
                                                           books_filter),
                                  NULL)))
    goto failure;

  /* Now create entries using SDK for menu key */
  n_books = g_list_model_get_n_items (books);
  for (guint i = 0; i < n_books; i++)
    {
      g_autoptr(ManualsBook) this_book = g_list_model_get_item (books, i);
      g_autoptr(ManualsNavigatable) navigatable = NULL;
      g_autoptr(ManualsSdk) sdk = NULL;
      g_autofree char *sdk_title = NULL;
      g_autofree char *title = NULL;
      g_autoptr(GIcon) jump_icon = NULL;
      const char *icon_name;

      if (manuals_book_get_id (this_book) == self->id)
        continue;

      if (!(sdk = dex_await_object (manuals_book_find_sdk (this_book), NULL)))
        continue;

      if ((icon_name = manuals_sdk_get_icon_name (sdk)))
        jump_icon = g_themed_icon_new (icon_name);

      sdk_title = manuals_sdk_dup_title (sdk);
      title = g_strdup_printf (_("View in %s"), sdk_title);
      navigatable = manuals_navigatable_new_for_resource (G_OBJECT (this_book));
      g_object_set (navigatable,
                    "menu-title", title,
                    "menu-icon", jump_icon,
                    NULL);

      g_list_store_append (store, navigatable);
    }

failure:
  return dex_future_new_take_object (g_steal_pointer (&store));
}

DexFuture *
manuals_book_list_alternates (ManualsBook *self)
{
  g_return_val_if_fail (MANUALS_IS_BOOK (self), NULL);

  return dex_scheduler_spawn (NULL, 0,
                              manuals_book_list_alternates_fiber,
                              g_object_ref (self),
                              g_object_unref);
}
