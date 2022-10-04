/* gbp-shortcutui-action-model.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-shortcutui-action-model"

#include "config.h"

#include <libide-gui.h>

#include "ide-application-private.h"
#include "ide-shortcut-manager-private.h"

#include "gbp-shortcutui-action.h"
#include "gbp-shortcutui-action-model.h"

struct _GbpShortcutuiActionModel
{
  GObject     parent_instance;
  GListModel *model;
  GPtrArray  *items;
};

static guint
gbp_shortcutui_action_model_get_n_items (GListModel *model)
{
  return GBP_SHORTCUTUI_ACTION_MODEL (model)->items->len;
}

static GType
gbp_shortcutui_action_model_get_item_type (GListModel *model)
{
  return GBP_TYPE_SHORTCUTUI_ACTION;
}

static gpointer
gbp_shortcutui_action_model_get_item (GListModel *model,
                                      guint       position)
{
  GPtrArray *items = GBP_SHORTCUTUI_ACTION_MODEL (model)->items;

  if (position < items->len)
    return g_object_ref (g_ptr_array_index (items, position));

  return NULL;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_n_items = gbp_shortcutui_action_model_get_n_items;
  iface->get_item = gbp_shortcutui_action_model_get_item;
  iface->get_item_type = gbp_shortcutui_action_model_get_item_type;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpShortcutuiActionModel, gbp_shortcutui_action_model, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

enum {
  PROP_0,
  PROP_MODEL,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static int
sort_actions (gconstpointer a,
              gconstpointer b)
{
  const GbpShortcutuiAction * const *aptr = a;
  const GbpShortcutuiAction * const *bptr = b;

  return gbp_shortcutui_action_compare (*aptr, *bptr);
}

static void
populate_shortcut_info_cb (const IdeShortcutInfo *info,
                           gpointer               user_data)
{
  GPtrArray *items = user_data;

  g_assert (info != NULL);
  g_assert (items != NULL);

  g_ptr_array_add (items,
                   g_object_new (GBP_TYPE_SHORTCUTUI_ACTION,
                                 "action-name", ide_shortcut_info_get_action_name (info),
                                 "action-target", ide_shortcut_info_get_action_target (info),
                                 "accelerator", ide_shortcut_info_get_accelerator (info),
                                 "title", ide_shortcut_info_get_title (info),
                                 "subtitle", ide_shortcut_info_get_subtitle (info),
                                 "page", ide_shortcut_info_get_page (info),
                                 "group", ide_shortcut_info_get_group (info),
                                 NULL));
}

static void
gbp_shortcutui_action_model_constructed (GObject *object)
{
  GbpShortcutuiActionModel *self = (GbpShortcutuiActionModel *)object;

  G_OBJECT_CLASS (gbp_shortcutui_action_model_parent_class)->constructed (object);

  ide_shortcut_info_foreach (self->model,
                             populate_shortcut_info_cb,
                             self->items);
  g_ptr_array_sort (self->items, sort_actions);
}

static void
gbp_shortcutui_action_model_dispose (GObject *object)
{
  GbpShortcutuiActionModel *self = (GbpShortcutuiActionModel *)object;

  g_clear_object (&self->model);
  g_clear_pointer (&self->items, g_ptr_array_unref);

  G_OBJECT_CLASS (gbp_shortcutui_action_model_parent_class)->dispose (object);
}

static void
gbp_shortcutui_action_model_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  GbpShortcutuiActionModel *self = GBP_SHORTCUTUI_ACTION_MODEL (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, self->model);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_shortcutui_action_model_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  GbpShortcutuiActionModel *self = GBP_SHORTCUTUI_ACTION_MODEL (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      self->model = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_shortcutui_action_model_class_init (GbpShortcutuiActionModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gbp_shortcutui_action_model_constructed;
  object_class->dispose = gbp_shortcutui_action_model_dispose;
  object_class->get_property = gbp_shortcutui_action_model_get_property;
  object_class->set_property = gbp_shortcutui_action_model_set_property;

  properties[PROP_MODEL] =
    g_param_spec_object ("model", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_shortcutui_action_model_init (GbpShortcutuiActionModel *self)
{
  self->items = g_ptr_array_new_with_free_func (g_object_unref);
}

GListModel *
gbp_shortcutui_action_model_new (GListModel *model)
{
  g_return_val_if_fail (G_IS_LIST_MODEL (model), NULL);

  return g_object_new (GBP_TYPE_SHORTCUTUI_ACTION_MODEL,
                       "model", model,
                       NULL);
}
