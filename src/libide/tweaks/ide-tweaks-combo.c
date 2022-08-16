/* ide-tweaks-combo.c
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

#define G_LOG_DOMAIN "ide-tweaks-combo"

#include "config.h"

#include <adwaita.h>

#include "ide-tweaks.h"
#include "ide-tweaks-choice.h"
#include "ide-tweaks-combo.h"
#include "ide-tweaks-combo-row.h"

struct _IdeTweaksCombo
{
  IdeTweaksWidget parent_instance;
  IdeTweaksSettings *settings;
  char *title;
  char *subtitle;
  char *key;
};

enum {
  PROP_0,
  PROP_KEY,
  PROP_SETTINGS,
  PROP_TITLE,
  PROP_SUBTITLE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeTweaksCombo, ide_tweaks_combo, IDE_TYPE_TWEAKS_WIDGET)

static GParamSpec *properties [N_PROPS];

static GtkWidget *
ide_tweaks_combo_create_for_item (IdeTweaksWidget *instance,
                                  IdeTweaksItem   *widget)
{
  IdeTweaksCombo *self = (IdeTweaksCombo *)widget;
  g_autoptr(IdeSettings) settings = NULL;
  g_autoptr(GListStore) store = NULL;
  g_autoptr(GVariant) value = NULL;
  IdeTweaksItem *root;
  AdwComboRow *row;
  const char *project_id;
  int selected = -1;
  guint i = 0;

  g_assert (IDE_IS_TWEAKS_COMBO (self));

  root = ide_tweaks_item_get_root (IDE_TWEAKS_ITEM (widget));
  project_id = ide_tweaks_get_project_id (IDE_TWEAKS (root));
  settings = IDE_SETTINGS (ide_tweaks_settings_create_action_group (self->settings, project_id));

  store = g_list_store_new (IDE_TYPE_TWEAKS_CHOICE);
  value = ide_settings_get_value (settings, self->key);

  for (IdeTweaksItem *child = ide_tweaks_item_get_first_child (IDE_TWEAKS_ITEM (self));
       child != NULL;
       child = ide_tweaks_item_get_next_sibling (child))
    {
      GVariant *target = ide_tweaks_choice_get_action_target (IDE_TWEAKS_CHOICE (child));

      if (g_variant_equal (value, target))
        selected = i;

      g_list_store_append (store, child);
      i++;
    }

  row = g_object_new (IDE_TYPE_TWEAKS_COMBO_ROW,
                      "model", store,
                      "settings", settings,
                      "key", self->key,
                      "title", self->title,
                      "subtitle", self->subtitle,
                      "selected", selected > -1 ? selected : 0,
                      NULL);

  return GTK_WIDGET (row);
}

static gboolean
ide_tweaks_combo_accepts (IdeTweaksItem *item,
                          IdeTweaksItem *child)
{
  return IDE_IS_TWEAKS_CHOICE (child);
}

static void
ide_tweaks_combo_dispose (GObject *object)
{
  IdeTweaksCombo *self = (IdeTweaksCombo *)object;

  g_clear_object (&self->settings);
  g_clear_pointer (&self->title, g_free);
  g_clear_pointer (&self->subtitle, g_free);

  G_OBJECT_CLASS (ide_tweaks_combo_parent_class)->dispose (object);
}

static void
ide_tweaks_combo_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  IdeTweaksCombo *self = IDE_TWEAKS_COMBO (object);

  switch (prop_id)
    {
    case PROP_SETTINGS:
      g_value_set_object (value, ide_tweaks_combo_get_settings (self));
      break;

    case PROP_TITLE:
      g_value_set_string (value, ide_tweaks_combo_get_title (self));
      break;

    case PROP_SUBTITLE:
      g_value_set_string (value, ide_tweaks_combo_get_subtitle (self));
      break;

    case PROP_KEY:
      g_value_set_string (value, ide_tweaks_combo_get_key (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_combo_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  IdeTweaksCombo *self = IDE_TWEAKS_COMBO (object);

  switch (prop_id)
    {
    case PROP_SETTINGS:
      ide_tweaks_combo_set_settings (self, g_value_get_object (value));
      break;

    case PROP_TITLE:
      ide_tweaks_combo_set_title (self, g_value_get_string (value));
      break;

    case PROP_SUBTITLE:
      ide_tweaks_combo_set_subtitle (self, g_value_get_string (value));
      break;

    case PROP_KEY:
      ide_tweaks_combo_set_key (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_combo_class_init (IdeTweaksComboClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeTweaksItemClass *item_class = IDE_TWEAKS_ITEM_CLASS (klass);
  IdeTweaksWidgetClass *widget_class = IDE_TWEAKS_WIDGET_CLASS (klass);

  object_class->dispose = ide_tweaks_combo_dispose;
  object_class->get_property = ide_tweaks_combo_get_property;
  object_class->set_property = ide_tweaks_combo_set_property;

  item_class->accepts = ide_tweaks_combo_accepts;

  widget_class->create_for_item = ide_tweaks_combo_create_for_item;

  properties[PROP_SETTINGS] =
    g_param_spec_object ("settings", NULL, NULL,
                         IDE_TYPE_TWEAKS_SETTINGS,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties[PROP_KEY] =
    g_param_spec_string ("key", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties[PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties[PROP_SUBTITLE] =
    g_param_spec_string ("subtitle", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_tweaks_combo_init (IdeTweaksCombo *self)
{
}

IdeTweaksCombo *
ide_tweaks_combo_new (void)
{
  return g_object_new (IDE_TYPE_TWEAKS_COMBO, NULL);
}

const char *
ide_tweaks_combo_get_title (IdeTweaksCombo *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_COMBO (self), NULL);

  return self->title;
}

void
ide_tweaks_combo_set_title (IdeTweaksCombo *self,
                            const char     *title)
{
  g_return_if_fail (IDE_IS_TWEAKS_COMBO (self));

  if (ide_set_string (&self->title, title))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TITLE]);
}

const char *
ide_tweaks_combo_get_subtitle (IdeTweaksCombo *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_COMBO (self), NULL);

  return self->subtitle;
}

void
ide_tweaks_combo_set_subtitle (IdeTweaksCombo *self,
                               const char     *subtitle)
{
  g_return_if_fail (IDE_IS_TWEAKS_COMBO (self));

  if (ide_set_string (&self->subtitle, subtitle))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SUBTITLE]);
}

const char *
ide_tweaks_combo_get_key (IdeTweaksCombo *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_COMBO (self), NULL);

  return self->key;
}

void
ide_tweaks_combo_set_key (IdeTweaksCombo *self,
                          const char     *key)
{
  g_return_if_fail (IDE_IS_TWEAKS_COMBO (self));

  if (ide_set_string (&self->key, key))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_KEY]);
}

/**
 * ide_tweaks_combo_get_settings:
 * @self: a #IdeTweaksCombo
 *
 * Gets the settings for the combo.
 *
 * Returns: (nullable) (transfer none): an #IdeTweaksSettings or %NULL
 */
IdeTweaksSettings *
ide_tweaks_combo_get_settings (IdeTweaksCombo *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_COMBO (self), NULL);

  return self->settings;
}

void
ide_tweaks_combo_set_settings (IdeTweaksCombo    *self,
                               IdeTweaksSettings *settings)
{
  g_return_if_fail (IDE_IS_TWEAKS_COMBO (self));
  g_return_if_fail (!settings || IDE_IS_TWEAKS_SETTINGS (settings));

  if (g_set_object (&self->settings, settings))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SETTINGS]);
}
