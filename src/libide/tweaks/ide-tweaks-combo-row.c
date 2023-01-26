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

#include "ide-tweaks-binding.h"
#include "ide-tweaks-choice.h"
#include "ide-tweaks-combo-row.h"
#include "ide-tweaks-variant.h"

#include "gsettings-mapping.h"

struct _IdeTweaksComboRow
{
  AdwComboRow       parent_instance;
  IdeTweaksBinding *binding;
  guint             selecting_item : 1;
};

G_DEFINE_FINAL_TYPE (IdeTweaksComboRow, ide_tweaks_combo_row, ADW_TYPE_COMBO_ROW)

enum {
  PROP_0,
  PROP_BINDING,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
ide_tweaks_combo_row_notify_selected (IdeTweaksComboRow *self,
                                      GParamSpec        *pspec)
{
  IdeTweaksChoice *choice;
  const char *tooltip_text = NULL;

  g_assert (IDE_IS_TWEAKS_COMBO_ROW (self));

  if (self->binding == NULL || self->selecting_item)
    return;

  self->selecting_item = TRUE;

  if ((choice = adw_combo_row_get_selected_item (ADW_COMBO_ROW (self))))
    {
      GVariant *variant = ide_tweaks_choice_get_value (choice);
      GType type;

      tooltip_text = ide_tweaks_choice_get_title (choice);

      if (variant == NULL)
        goto cleanup;

      if (ide_tweaks_binding_get_expected_type (self->binding, &type))
        {
          g_auto(GValue) value = G_VALUE_INIT;

          g_value_init (&value, type);

          if (g_settings_get_mapping (&value, variant, NULL))
            {
              ide_tweaks_binding_set_value (self->binding, &value);
              goto cleanup;
            }
        }
    }

  g_warning ("Failed to update choice!");

cleanup:
  self->selecting_item = FALSE;

  gtk_widget_set_tooltip_text (GTK_WIDGET (self), tooltip_text);
}

static void
ide_tweaks_combo_row_binding_changed_cb (IdeTweaksComboRow *self,
                                         IdeTweaksBinding  *binding)
{
  g_autoptr(GVariant) variant = NULL;
  g_auto(GValue) value = G_VALUE_INIT;
  const GVariantType *expected_type;
  GListModel *model;
  GType type;
  guint n_items;
  int selected = -1;

  g_assert (IDE_IS_TWEAKS_COMBO_ROW (self));
  g_assert (IDE_IS_TWEAKS_BINDING (binding));

  if (self->selecting_item)
    return;

  model = adw_combo_row_get_model (ADW_COMBO_ROW (self));
  n_items = g_list_model_get_n_items (model);

  if (n_items == 0)
    return;

  if (!ide_tweaks_binding_get_expected_type (binding, &type))
    return;

  expected_type = _ide_tweaks_gtype_to_variant_type (type);

  g_value_init (&value, type);
  if (ide_tweaks_binding_get_value (binding, &value))
    variant = g_settings_set_mapping (&value, expected_type, NULL);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(IdeTweaksChoice) choice = g_list_model_get_item (model, i);
      GVariant *choice_variant = ide_tweaks_choice_get_value (choice);

      if (variant && choice_variant && g_variant_equal (variant, choice_variant))
        {
          selected = i;
          break;
        }
    }

  if (selected > -1)
    adw_combo_row_set_selected (ADW_COMBO_ROW (self), selected);
}

static void
ide_tweaks_combo_row_constructed (GObject *object)
{
  IdeTweaksComboRow *self = (IdeTweaksComboRow *)object;

  G_OBJECT_CLASS (ide_tweaks_combo_row_parent_class)->constructed (object);

  g_signal_connect (self,
                    "notify::selected",
                    G_CALLBACK (ide_tweaks_combo_row_notify_selected),
                    NULL);

  if (self->binding)
    g_signal_connect_object (self->binding,
                             "changed",
                             G_CALLBACK (ide_tweaks_combo_row_binding_changed_cb),
                             self,
                             G_CONNECT_SWAPPED);
}

static void
ide_tweaks_combo_row_dispose (GObject *object)
{
  IdeTweaksComboRow *self = (IdeTweaksComboRow *)object;

  g_clear_object (&self->binding);

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
    case PROP_BINDING:
      g_value_set_object (value, self->binding);
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
    case PROP_BINDING:
      self->binding = g_value_dup_object (value);
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

  properties [PROP_BINDING] =
    g_param_spec_object ("binding", NULL, NULL,
                         IDE_TYPE_TWEAKS_BINDING,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-tweaks/ide-tweaks-combo-row.ui");
}

static void
ide_tweaks_combo_row_init (IdeTweaksComboRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
