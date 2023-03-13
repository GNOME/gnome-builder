/* gbp-shortcutui-model.c
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-shortcutui-model"

#include "config.h"

#include <libide-gui.h>

#include "ide-shortcut-bundle-private.h"
#include "ide-shortcut-manager-private.h"

#include "gbp-shortcutui-model.h"
#include "gbp-shortcutui-shortcut.h"

struct _GbpShortcutuiModel
{
  GObject             parent_instance;
  IdeContext         *context;
  GtkFilterListModel *filter_model;
  GtkMapListModel    *map_model;
  GHashTable         *id_to_section_info;
};

typedef struct _SectionInfo
{
  const char *id;
  const char *page;
  const char *group;
} SectionInfo;

enum {
  PROP_0,
  PROP_CONTEXT,
  N_PROPS
};

static GType
gbp_shortcutui_model_get_item_type (GListModel *model)
{
  return G_TYPE_OBJECT;
}

static guint
gbp_shortcutui_model_get_n_items (GListModel *model)
{
  GbpShortcutuiModel *self = GBP_SHORTCUTUI_MODEL (model);

  if (self->map_model != NULL)
    return g_list_model_get_n_items (G_LIST_MODEL (self->map_model));

  return 0;
}

static gpointer
gbp_shortcutui_model_get_item (GListModel *model,
                               guint       position)
{
  GbpShortcutuiModel *self = GBP_SHORTCUTUI_MODEL (model);

  if (self->map_model != NULL)
    return g_list_model_get_item (G_LIST_MODEL (self->map_model), position);

  return NULL;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = gbp_shortcutui_model_get_item_type;
  iface->get_n_items = gbp_shortcutui_model_get_n_items;
  iface->get_item = gbp_shortcutui_model_get_item;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpShortcutuiModel, gbp_shortcutui_model, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static GParamSpec *properties [N_PROPS];

static gboolean
filter_func (gpointer item,
             gpointer user_data)
{
  GtkShortcut *shortcut = item;
  IdeShortcut *state = g_object_get_data (G_OBJECT (shortcut), "IDE_SHORTCUT");

  if (state == NULL)
    return FALSE;

  return !ide_str_empty0 (state->id);
}

static gpointer
map_func (gpointer item,
          gpointer user_data)
{
  g_autoptr(GtkShortcut) shortcut = item;
  GHashTable *id_to_section_info = user_data;
  IdeShortcut *state;
  SectionInfo *section_info;
  const char *page = NULL;
  const char *group = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GTK_IS_SHORTCUT (shortcut));
  g_assert (id_to_section_info != NULL);

  state = g_object_get_data (G_OBJECT (shortcut), "IDE_SHORTCUT");

  g_assert (state != NULL);
  g_assert (state->id != NULL);

  if ((section_info = g_hash_table_lookup (id_to_section_info, state->id)))
    {
      page = section_info->page;
      group = section_info->group;
    }

  return gbp_shortcutui_shortcut_new (shortcut, page, group);
}

static void
gbp_shortcutui_model_info_foreach_cb (const IdeShortcutInfo *info,
                                      gpointer               user_data)
{
  GHashTable *id_to_section_info = user_data;
  SectionInfo *section_info;
  const char *id;
  const char *page;
  const char *group;

  if (!(id = ide_shortcut_info_get_id (info)))
    return;

  if (!(section_info = g_hash_table_lookup (id_to_section_info, id)))
    {
      section_info = g_new0 (SectionInfo, 1);
      section_info->id = g_intern_string (id);
      g_hash_table_insert (id_to_section_info, (gpointer)section_info->id, section_info);
    }

  page = ide_shortcut_info_get_page (info);
  group = ide_shortcut_info_get_group (info);

  if (section_info->page == NULL && page != NULL)
    section_info->page = g_intern_string (page);

  if (section_info->group == NULL && group != NULL)
    section_info->group = g_intern_string (group);
}

static void
gbp_shortcutui_model_constructed (GObject *object)
{
  GbpShortcutuiModel *self = (GbpShortcutuiModel *)object;
  g_autoptr(GtkFilter) custom = NULL;
  IdeShortcutManager *shortcuts;

  G_OBJECT_CLASS (gbp_shortcutui_model_parent_class)->constructed (object);

  if (self->context == NULL)
    return;

  self->id_to_section_info = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);

  shortcuts = ide_shortcut_manager_from_context (self->context);

  ide_shortcut_info_foreach (G_LIST_MODEL (shortcuts),
                             gbp_shortcutui_model_info_foreach_cb,
                             self->id_to_section_info);

  custom = GTK_FILTER (gtk_custom_filter_new (filter_func, NULL, NULL));
  self->filter_model = gtk_filter_list_model_new (G_LIST_MODEL (shortcuts),
                                                  g_steal_pointer (&custom));

  self->map_model = gtk_map_list_model_new (g_object_ref (G_LIST_MODEL (self->filter_model)),
                                            map_func,
                                            g_hash_table_ref (self->id_to_section_info),
                                            (GDestroyNotify)g_hash_table_unref);

  g_signal_connect_object (self->map_model,
                           "items-changed",
                           G_CALLBACK (g_list_model_items_changed),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
gbp_shortcutui_model_dispose (GObject *object)
{
  GbpShortcutuiModel *self = (GbpShortcutuiModel *)object;

  g_clear_pointer (&self->id_to_section_info, g_hash_table_unref);
  g_clear_object (&self->context);
  g_clear_object (&self->filter_model);
  g_clear_object (&self->map_model);

  G_OBJECT_CLASS (gbp_shortcutui_model_parent_class)->dispose (object);
}

static void
gbp_shortcutui_model_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GbpShortcutuiModel *self = GBP_SHORTCUTUI_MODEL (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_shortcutui_model_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GbpShortcutuiModel *self = GBP_SHORTCUTUI_MODEL (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      self->context = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_shortcutui_model_class_init (GbpShortcutuiModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gbp_shortcutui_model_constructed;
  object_class->dispose = gbp_shortcutui_model_dispose;
  object_class->get_property = gbp_shortcutui_model_get_property;
  object_class->set_property = gbp_shortcutui_model_set_property;

  properties[PROP_CONTEXT] =
    g_param_spec_object ("context", NULL, NULL,
                         IDE_TYPE_CONTEXT,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_shortcutui_model_init (GbpShortcutuiModel *self)
{
}

GbpShortcutuiModel *
gbp_shortcutui_model_new (IdeContext *context)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  return g_object_new (GBP_TYPE_SHORTCUTUI_MODEL,
                       "context", context,
                       NULL);
}
