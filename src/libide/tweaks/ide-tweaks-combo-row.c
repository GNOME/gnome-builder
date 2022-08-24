/* ide-tweaks-combo-row.c
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

#define G_LOG_DOMAIN "ide-tweaks-combo-row"

#include "config.h"

#include <libide-core.h>

#include "ide-tweaks-choice.h"
#include "ide-tweaks-combo-row.h"
#include "ide-tweaks-settings.h"

struct _IdeTweaksComboRow
{
  AdwComboRow parent_instance;
  IdeSettings *settings;
  char *key;
};

G_DEFINE_FINAL_TYPE (IdeTweaksComboRow, ide_tweaks_combo_row, ADW_TYPE_COMBO_ROW)

enum {
  PROP_0,
  PROP_KEY,
  PROP_SETTINGS,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
ide_tweaks_combo_row_notify_selected_item (IdeTweaksComboRow *self,
                                           GParamSpec        *pspec)
{
  IdeTweaksChoice *choice;

  g_assert (IDE_IS_TWEAKS_COMBO_ROW (self));
  g_assert (self->settings != NULL);
  g_assert (self->key != NULL);

  if ((choice = adw_combo_row_get_selected_item (ADW_COMBO_ROW (self))))
    {
      GVariant *action_target = ide_tweaks_choice_get_value (choice);

      ide_settings_set_value (self->settings, self->key, action_target);
    }
}

static void
ide_tweaks_combo_row_constructed (GObject *object)
{
  IdeTweaksComboRow *self = (IdeTweaksComboRow *)object;

  G_OBJECT_CLASS (ide_tweaks_combo_row_parent_class)->constructed (object);

  g_signal_connect (self,
                    "notify::selected-item",
                    G_CALLBACK (ide_tweaks_combo_row_notify_selected_item),
                    NULL);
}

static void
ide_tweaks_combo_row_dispose (GObject *object)
{
  IdeTweaksComboRow *self = (IdeTweaksComboRow *)object;

  g_clear_object (&self->settings);
  g_clear_pointer (&self->key, g_free);

  G_OBJECT_CLASS (ide_tweaks_combo_row_parent_class)->dispose (object);
}

static void
ide_tweaks_combo_row_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  IdeTweaksComboRow *self = IDE_TWEAKS_COMBO_ROW (object);

  switch (prop_id)
    {
    case PROP_KEY:
      g_value_set_string (value, self->key);
      break;

    case PROP_SETTINGS:
      g_value_set_object (value, self->settings);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_combo_row_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  IdeTweaksComboRow *self = IDE_TWEAKS_COMBO_ROW (object);

  switch (prop_id)
    {
    case PROP_KEY:
      if (ide_set_string (&self->key, g_value_get_string (value)))
        g_object_notify_by_pspec (object, pspec);
      break;

    case PROP_SETTINGS:
      if (g_set_object (&self->settings, g_value_get_object (value)))
        g_object_notify_by_pspec (object, pspec);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_combo_row_class_init (IdeTweaksComboRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = ide_tweaks_combo_row_constructed;
  object_class->dispose = ide_tweaks_combo_row_dispose;
  object_class->get_property = ide_tweaks_combo_row_get_property;
  object_class->set_property = ide_tweaks_combo_row_set_property;

  properties[PROP_KEY] =
    g_param_spec_string ("key", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties[PROP_SETTINGS] =
    g_param_spec_object ("settings", NULL, NULL,
                         IDE_TYPE_SETTINGS,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-tweaks/ide-tweaks-combo-row.ui");
}

static void
ide_tweaks_combo_row_init (IdeTweaksComboRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
