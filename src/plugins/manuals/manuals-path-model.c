/*
 * manuals-path-model.c
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

#include <libdex.h>

#include "manuals-navigatable.h"
#include "manuals-path-element.h"
#include "manuals-path-model.h"
#include "manuals-search-result.h"

struct _ManualsPathModel
{
  GObject             parent_instance;
  GPtrArray          *items;
  ManualsNavigatable *navigatable;
};

enum {
  PROP_0,
  PROP_NAVIGATABLE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static GType
manuals_path_model_get_item_type (GListModel *model)
{
  return MANUALS_TYPE_PATH_ELEMENT;
}

static guint
manuals_path_model_get_n_items (GListModel *model)
{
  return MANUALS_PATH_MODEL (model)->items->len;
}

static gpointer
manuals_path_model_get_item (GListModel *model,
                             guint       position)
{
  ManualsPathModel *self = MANUALS_PATH_MODEL (model);

  if (position < self->items->len)
    return g_object_ref (g_ptr_array_index (self->items, position));

  return NULL;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = manuals_path_model_get_item_type;
  iface->get_n_items = manuals_path_model_get_n_items;
  iface->get_item = manuals_path_model_get_item;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (ManualsPathModel, manuals_path_model, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static void
manuals_path_model_dispose (GObject *object)
{
  ManualsPathModel *self = (ManualsPathModel *)object;

  g_clear_pointer (&self->items, g_ptr_array_unref);
  g_clear_object (&self->navigatable);

  G_OBJECT_CLASS (manuals_path_model_parent_class)->dispose (object);
}

static void
manuals_path_model_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  ManualsPathModel *self = MANUALS_PATH_MODEL (object);

  switch (prop_id)
    {
    case PROP_NAVIGATABLE:
      g_value_set_object (value, self->navigatable);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
manuals_path_model_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  ManualsPathModel *self = MANUALS_PATH_MODEL (object);

  switch (prop_id)
    {
    case PROP_NAVIGATABLE:
      manuals_path_model_set_navigatable (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
manuals_path_model_class_init (ManualsPathModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = manuals_path_model_dispose;
  object_class->get_property = manuals_path_model_get_property;
  object_class->set_property = manuals_path_model_set_property;

  properties[PROP_NAVIGATABLE] =
    g_param_spec_object ("navigatable", NULL, NULL,
                         MANUALS_TYPE_NAVIGATABLE,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
manuals_path_model_init (ManualsPathModel *self)
{
  self->items = g_ptr_array_new_with_free_func (g_object_unref);
}

ManualsPathModel *
manuals_path_model_new (void)
{
  return g_object_new (MANUALS_TYPE_PATH_MODEL, NULL);
}

static ManualsNavigatable *
find_parent (ManualsNavigatable *object)
{
  g_autoptr(ManualsNavigatable) freeme = object;

  g_assert (MANUALS_IS_NAVIGATABLE (object));

  if (object != NULL)
    return dex_await_object (manuals_navigatable_find_parent (object), NULL);

  return NULL;
}

static char *
get_title (ManualsNavigatable *object)
{
  return g_strdup (manuals_navigatable_get_title (object));
}

static GIcon *
get_icon (ManualsNavigatable *navigatable)
{
  return manuals_navigatable_get_icon (navigatable);
}

static DexFuture *
manuals_path_model_set_navigatable_fiber (gpointer user_data)
{
  ManualsPathModel *self = user_data;
  g_autoptr(ManualsNavigatable) navigatable = NULL;
  g_autoptr(ManualsNavigatable) parent = NULL;
  g_autoptr(GPtrArray) items = NULL;
  ManualsPathElement *first;
  ManualsPathElement *last;

  g_assert (MANUALS_IS_PATH_MODEL (self));

  if (!g_set_object (&navigatable, self->navigatable))
    goto complete;

  items = g_ptr_array_new_with_free_func (g_object_unref);

  g_set_object (&parent, navigatable);

  while (parent != NULL)
    {
      g_autofree char *title = get_title (parent);
      GIcon *icon = get_icon (parent);

      g_ptr_array_insert (items,
                          0,
                          g_object_new (MANUALS_TYPE_PATH_ELEMENT,
                                        "item", parent,
                                        "title", title,
                                        "icon", icon,
                                        NULL));
      parent = find_parent (g_steal_pointer (&parent));
    }

  first = g_ptr_array_index (items, 0);
  last = g_ptr_array_index (items, items->len-1);

  first->is_root = TRUE;
  last->is_leaf = TRUE;

  if (navigatable == self->navigatable)
    {
      g_autoptr(GPtrArray) old = g_steal_pointer (&self->items);
      guint old_len = old->len;
      guint new_len = items->len;

      self->items = g_steal_pointer (&items);

      if (old_len > 0 || new_len > 0)
        g_list_model_items_changed (G_LIST_MODEL (self), 0, old_len, new_len);
    }

complete:
  return dex_future_new_for_boolean (TRUE);
}

void
manuals_path_model_set_navigatable (ManualsPathModel   *self,
                                    ManualsNavigatable *navigatable)
{
  g_return_if_fail (MANUALS_IS_PATH_MODEL (self));
  g_return_if_fail (!navigatable || MANUALS_IS_NAVIGATABLE (navigatable));

  if (g_set_object (&self->navigatable, navigatable))
    {
      dex_future_disown (dex_scheduler_spawn (NULL, 0,
                                              manuals_path_model_set_navigatable_fiber,
                                              g_object_ref (self),
                                              g_object_unref));
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NAVIGATABLE]);
    }
}
