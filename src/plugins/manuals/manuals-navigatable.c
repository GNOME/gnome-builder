/*
 * manuals-navigatable.c
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

#include <gtk/gtk.h>

#include "manuals-book.h"
#include "manuals-heading.h"
#include "manuals-keyword.h"
#include "manuals-navigatable.h"

struct _ManualsNavigatable
{
  GObject parent_instance;

  GObject *item;
  GIcon *icon;
  GIcon *menu_icon;
  char *menu_title;
  char *title;
  char *uri;
};

G_DEFINE_FINAL_TYPE (ManualsNavigatable, manuals_navigatable, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_ICON,
  PROP_ITEM,
  PROP_MENU_ICON,
  PROP_MENU_TITLE,
  PROP_TITLE,
  PROP_URI,
  N_PROPS
};

enum {
  FIND_CHILDREN,
  FIND_PARENT,
  N_SIGNALS
};

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];
static GIcon *book_symbolic;
static GIcon *library_symbolic;

static DexFuture *
manuals_navigatable_not_supported (ManualsNavigatable *self)
{
  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "Not Supported");
}

static DexFuture *
manuals_navigatable_wrap_in_navigatable (DexFuture *completed,
                                         gpointer   user_data)
{
  const GValue *value;
  GObject *resource;

  g_assert (DEX_IS_FUTURE (completed));

  value = dex_future_get_value (completed, NULL);
  resource = g_value_get_object (value);

  return dex_future_new_take_object (manuals_navigatable_new_for_resource (resource));
}

static gpointer
manuals_navigatable_wrap_in_map_func (gpointer item,
                                      gpointer user_data)
{
  g_autoptr(GObject) object = item;

  return manuals_navigatable_new_for_resource (object);
}

static DexFuture *
manuals_navigatable_wrap_in_map (DexFuture *completed,
                                 gpointer   user_data)
{
  GtkMapListModel *map;
  const GValue *value;

  g_assert (DEX_IS_FUTURE (completed));

  value = dex_future_get_value (completed, NULL);
  map = gtk_map_list_model_new (g_value_dup_object (value),
                                manuals_navigatable_wrap_in_map_func,
                                NULL, NULL);

  return dex_future_new_take_object (map);
}

static DexFuture *
join_future_models (DexFuture *completed,
                    gpointer   user_data)
{
  GListModel *first = g_value_get_object (dex_future_set_get_value_at (DEX_FUTURE_SET (completed), 0, NULL));
  GListModel *second = g_value_get_object (dex_future_set_get_value_at (DEX_FUTURE_SET (completed), 1, NULL));
  GListStore *store = g_list_store_new (G_TYPE_LIST_MODEL);

  g_list_store_append (store, first);
  g_list_store_append (store, second);

  return dex_future_new_take_object (gtk_flatten_list_model_new (G_LIST_MODEL (store)));
}

static DexFuture *
manuals_navigatable_find_parent_for_resource (ManualsNavigatable *self,
                                              GObject            *object)
{
  g_assert (MANUALS_IS_NAVIGATABLE (self));
  g_assert (G_IS_OBJECT (object));

  if (MANUALS_IS_SDK (object))
    {
      ManualsRepository *repository = NULL;
      g_object_get (object, "repository", &repository, NULL);
      return dex_future_then (dex_future_new_take_object (repository),
                              manuals_navigatable_wrap_in_navigatable,
                              NULL, NULL);
    }

  if (MANUALS_IS_BOOK (object))
    return dex_future_then (manuals_book_find_sdk (MANUALS_BOOK (object)),
                            manuals_navigatable_wrap_in_navigatable,
                            NULL, NULL);

  if (MANUALS_IS_HEADING (object))
    return dex_future_then (manuals_heading_find_parent (MANUALS_HEADING (object)),
                            manuals_navigatable_wrap_in_navigatable,
                            NULL, NULL);

  if (MANUALS_IS_KEYWORD (object))
    return dex_future_then (manuals_keyword_find_book (MANUALS_KEYWORD (object)),
                            manuals_navigatable_wrap_in_navigatable,
                            NULL, NULL);

  return manuals_navigatable_not_supported (self);
}

static DexFuture *
manuals_navigatable_find_children_for_resource (ManualsNavigatable *self,
                                                GObject            *object)
{
  g_assert (MANUALS_IS_NAVIGATABLE (self));
  g_assert (G_IS_OBJECT (object));

  if (MANUALS_IS_REPOSITORY (object))
    return dex_future_then (manuals_repository_list_sdks (MANUALS_REPOSITORY (object)),
                            manuals_navigatable_wrap_in_map,
                            NULL, NULL);

  if (MANUALS_IS_HEADING (object))
    return dex_future_then (manuals_heading_list_headings (MANUALS_HEADING (object)),
                            manuals_navigatable_wrap_in_map,
                            NULL, NULL);

  if (MANUALS_IS_BOOK (object))
    return dex_future_then (manuals_book_list_headings (MANUALS_BOOK (object)),
                            manuals_navigatable_wrap_in_map,
                            NULL, NULL);

  if (MANUALS_IS_SDK (object))
    return dex_future_then (manuals_sdk_list_books (MANUALS_SDK (object)),
                            manuals_navigatable_wrap_in_map,
                            NULL, NULL);

  return manuals_navigatable_not_supported (self);
}

static void
manuals_navigatable_finalize (GObject *object)
{
  ManualsNavigatable *self = (ManualsNavigatable *)object;

  g_clear_pointer (&self->menu_title, g_free);
  g_clear_pointer (&self->title, g_free);
  g_clear_pointer (&self->uri, g_free);
  g_clear_object (&self->icon);
  g_clear_object (&self->menu_icon);
  g_clear_object (&self->item);

  G_OBJECT_CLASS (manuals_navigatable_parent_class)->finalize (object);
}

static void
manuals_navigatable_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  ManualsNavigatable *self = MANUALS_NAVIGATABLE (object);

  switch (prop_id)
    {
    case PROP_ICON:
      g_value_set_object (value, manuals_navigatable_get_icon (self));
      break;

    case PROP_ITEM:
      g_value_set_object (value, manuals_navigatable_get_item (self));
      break;

    case PROP_MENU_ICON:
      g_value_set_object (value, manuals_navigatable_get_menu_icon (self));
      break;

    case PROP_MENU_TITLE:
      g_value_set_string (value, manuals_navigatable_get_menu_title (self));
      break;

    case PROP_TITLE:
      g_value_set_string (value, manuals_navigatable_get_title (self));
      break;

    case PROP_URI:
      g_value_set_string (value, manuals_navigatable_get_uri (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
manuals_navigatable_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  ManualsNavigatable *self = MANUALS_NAVIGATABLE (object);

  switch (prop_id)
    {
    case PROP_ICON:
      manuals_navigatable_set_icon (self, g_value_get_object (value));
      break;

    case PROP_ITEM:
      manuals_navigatable_set_item (self, g_value_get_object (value));
      break;

    case PROP_MENU_ICON:
      manuals_navigatable_set_menu_icon (self, g_value_get_object (value));
      break;

    case PROP_MENU_TITLE:
      manuals_navigatable_set_menu_title (self, g_value_get_string (value));
      break;

    case PROP_TITLE:
      manuals_navigatable_set_title (self, g_value_get_string (value));
      break;

    case PROP_URI:
      manuals_navigatable_set_uri (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
manuals_navigatable_class_init (ManualsNavigatableClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = manuals_navigatable_finalize;
  object_class->get_property = manuals_navigatable_get_property;
  object_class->set_property = manuals_navigatable_set_property;

  properties[PROP_ICON] =
    g_param_spec_object ("icon", NULL, NULL,
                         G_TYPE_ICON,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_ITEM] =
    g_param_spec_object ("item", NULL, NULL,
                         G_TYPE_OBJECT,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_MENU_ICON] =
    g_param_spec_object ("menu-icon", NULL, NULL,
                         G_TYPE_ICON,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_MENU_TITLE] =
    g_param_spec_string ("menu-title", NULL, NULL,
                         NULL,
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

  signals[FIND_PARENT] =
    g_signal_new_class_handler ("find-parent",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (manuals_navigatable_not_supported),
                                g_signal_accumulator_first_wins, NULL,
                                NULL,
                                G_TYPE_POINTER, 0);

  signals[FIND_CHILDREN] =
    g_signal_new_class_handler ("find-children",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (manuals_navigatable_not_supported),
                                g_signal_accumulator_first_wins,
                                NULL,
                                NULL,
                                G_TYPE_POINTER, 0);
}

static void
manuals_navigatable_init (ManualsNavigatable *self)
{
}

ManualsNavigatable *
manuals_navigatable_new (void)
{
  return g_object_new (MANUALS_TYPE_NAVIGATABLE, NULL);
}

ManualsNavigatable *
manuals_navigatable_new_for_resource (GObject *object)
{
  ManualsNavigatable *self;
  g_autoptr(GIcon) icon = NULL;
  g_autofree char *freeme_title = NULL;
  const char *title = NULL;
  const char *uri = NULL;

  g_return_val_if_fail (G_IS_OBJECT (object), NULL);

  if (MANUALS_IS_NAVIGATABLE (object))
    return g_object_ref (MANUALS_NAVIGATABLE (object));

  if (library_symbolic == NULL)
    library_symbolic = g_themed_icon_new ("library-symbolic");

  if (book_symbolic == NULL)
    book_symbolic = g_themed_icon_new ("book-symbolic");

  if (MANUALS_IS_REPOSITORY (object))
    {
      title = _("Manuals");
      icon = g_object_ref (library_symbolic);
      uri = NULL;
    }
  else if (MANUALS_IS_SDK (object))
    {
      ManualsSdk *sdk = MANUALS_SDK (object);
      const char *icon_name;

      title = freeme_title = manuals_sdk_dup_title (sdk);
      icon_name = manuals_sdk_get_icon_name (sdk);
      uri = NULL;

      if (icon_name != NULL)
        icon = g_themed_icon_new (icon_name);
    }
  else if (MANUALS_IS_BOOK (object))
    {
      ManualsBook *book = MANUALS_BOOK (object);

      title = manuals_book_get_title (book);
      uri = manuals_book_get_default_uri (book);
      icon = g_object_ref (book_symbolic);
    }
  else if (MANUALS_IS_HEADING (object))
    {
      ManualsHeading *heading = MANUALS_HEADING (object);

      title = manuals_heading_get_title (heading);
      uri = manuals_heading_get_uri (heading);
    }
  else if (MANUALS_IS_KEYWORD (object))
    {
      ManualsKeyword *keyword = MANUALS_KEYWORD (object);
      const char *icon_name = NULL;
      const char *kind;

      title = manuals_keyword_get_name (keyword);
      uri = manuals_keyword_get_uri (keyword);
      kind = manuals_keyword_get_kind (keyword);

      if (g_strcmp0 (kind, "function") == 0)
        icon_name = "lang-function-symbolic";
      else if (g_strcmp0 (kind, "struct") == 0)
        icon_name = "lang-struct-symbolic";
      else if (g_strcmp0 (kind, "enum") == 0)
        icon_name = "lang-enum-symbolic";
      else if (g_strcmp0 (kind, "member") == 0)
        icon_name = "lang-struct-field-symbolic";
      else if (g_strcmp0 (kind, "constant") == 0)
        icon_name = "lang-constant-symbolic";
      else if (g_strcmp0 (kind, "macro") == 0)
        icon_name = "lang-macro-symbolic";

      if (title != NULL && g_str_has_prefix (title, "The "))
        {
          if (g_str_has_suffix (title, " property"))
            icon_name = "lang-method-symbolic";
          else if (g_str_has_suffix (title, " method"))
            icon_name = "lang-method-symbolic";
          else if (g_str_has_suffix (title, " signal"))
            icon_name = "lang-signal-symbolic";
        }

      if (icon_name != NULL)
        icon = g_themed_icon_new (icon_name);
    }

  self = g_object_new (MANUALS_TYPE_NAVIGATABLE,
                       "uri", uri,
                       "title", title,
                       "icon", icon,
                       "item", object,
                       NULL);

  g_signal_connect_object (self,
                           "find-parent",
                           G_CALLBACK (manuals_navigatable_find_parent_for_resource),
                           object,
                           0);

  g_signal_connect_object (self,
                           "find-children",
                           G_CALLBACK (manuals_navigatable_find_children_for_resource),
                           object,
                           0);

  return g_steal_pointer (&self);
}

GIcon *
manuals_navigatable_get_icon (ManualsNavigatable *self)
{
  g_return_val_if_fail (MANUALS_IS_NAVIGATABLE (self), NULL);

  return self->icon;
}

void
manuals_navigatable_set_icon (ManualsNavigatable *self,
                              GIcon              *icon)
{
  g_return_if_fail (MANUALS_IS_NAVIGATABLE (self));

  if (g_set_object (&self->icon, icon))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ICON]);

      if (self->menu_icon == NULL)
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MENU_ICON]);
    }
}

GIcon *
manuals_navigatable_get_menu_icon (ManualsNavigatable *self)
{
  g_return_val_if_fail (MANUALS_IS_NAVIGATABLE (self), NULL);

  if (self->menu_icon == NULL)
    return self->icon;

  return self->menu_icon;
}

void
manuals_navigatable_set_menu_icon (ManualsNavigatable *self,
                                   GIcon              *menu_icon)
{
  g_return_if_fail (MANUALS_IS_NAVIGATABLE (self));
  g_return_if_fail (!menu_icon || G_IS_ICON (menu_icon));

  if (g_set_object (&self->menu_icon, menu_icon))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MENU_ICON]);
}

const char *
manuals_navigatable_get_title (ManualsNavigatable *self)
{
  g_return_val_if_fail (MANUALS_IS_NAVIGATABLE (self), NULL);

  return self->title;
}

void
manuals_navigatable_set_title (ManualsNavigatable *self,
                               const char         *title)
{
  g_return_if_fail (MANUALS_IS_NAVIGATABLE (self));

  if (g_set_str (&self->title, title))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TITLE]);

      if (self->menu_title == NULL)
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MENU_TITLE]);
    }
}

const char *
manuals_navigatable_get_menu_title (ManualsNavigatable *self)
{
  g_return_val_if_fail (MANUALS_IS_NAVIGATABLE (self), NULL);

  if (self->menu_title == NULL)
    return self->title;

  return self->menu_title;
}

void
manuals_navigatable_set_menu_title (ManualsNavigatable *self,
                                    const char         *menu_title)
{
  g_return_if_fail (MANUALS_IS_NAVIGATABLE (self));

  if (g_set_str (&self->menu_title, menu_title))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MENU_TITLE]);
}

const char *
manuals_navigatable_get_uri (ManualsNavigatable *self)
{
  g_return_val_if_fail (MANUALS_IS_NAVIGATABLE (self), NULL);

  return self->uri;
}

void
manuals_navigatable_set_uri (ManualsNavigatable *self,
                             const char         *uri)
{
  g_return_if_fail (MANUALS_IS_NAVIGATABLE (self));

  if (g_set_str (&self->uri, uri))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_URI]);
}

DexFuture *
manuals_navigatable_find_parent (ManualsNavigatable *self)
{
  DexFuture *future = NULL;

  g_return_val_if_fail (MANUALS_IS_NAVIGATABLE (self), NULL);

  g_signal_emit (self, signals[FIND_PARENT], 0, &future);

  return future;
}

DexFuture *
manuals_navigatable_find_children (ManualsNavigatable *self)
{
  DexFuture *future = NULL;

  g_return_val_if_fail (MANUALS_IS_NAVIGATABLE (self), NULL);

  g_signal_emit (self, signals[FIND_CHILDREN], 0, &future);

  return future;
}

gpointer
manuals_navigatable_get_item (ManualsNavigatable *self)
{
  g_return_val_if_fail (MANUALS_IS_NAVIGATABLE (self), NULL);

  return self->item;
}

void
manuals_navigatable_set_item (ManualsNavigatable *self,
                              gpointer            item)
{
  g_return_if_fail (MANUALS_IS_NAVIGATABLE (self));
  g_return_if_fail (!item || G_IS_OBJECT (item));

  if (g_set_object (&self->item, item))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ITEM]);
}

static DexFuture *
manuals_navigatable_find_parents_children (DexFuture *completed,
                                           gpointer   user_data)
{
  g_autoptr(ManualsNavigatable) parent = NULL;

  g_assert (DEX_IS_FUTURE (completed));

  parent = dex_await_object (dex_ref (completed), NULL);

  return manuals_navigatable_find_children (parent);
}


DexFuture *
manuals_navigatable_find_peers (ManualsNavigatable *self)
{
  DexFuture *alternates;

  g_return_val_if_fail (MANUALS_IS_NAVIGATABLE (self), NULL);

  if (MANUALS_IS_HEADING (self->item))
    alternates = manuals_heading_list_alternates (MANUALS_HEADING (self->item));
  else if (MANUALS_IS_KEYWORD (self->item))
    alternates = manuals_keyword_list_alternates (MANUALS_KEYWORD (self->item));
  else if (MANUALS_IS_BOOK (self->item))
    alternates = manuals_book_list_alternates (MANUALS_BOOK (self->item));
  else
    alternates = dex_future_new_take_object (g_list_store_new (MANUALS_TYPE_NAVIGATABLE));

  return dex_future_then (dex_future_all (alternates,
                                          dex_future_then (manuals_navigatable_find_parent (self),
                                                           manuals_navigatable_find_parents_children,
                                                           NULL, NULL),
                                          NULL),
                          join_future_models,
                          NULL, NULL);
}
