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
#include "ide-tweaks-factory.h"
#include "ide-tweaks-item-private.h"
#include "ide-tweaks-model-private.h"
#include "ide-tweaks-variant.h"

#include "gsettings-mapping.h"

struct _IdeTweaksCombo
{
  IdeTweaksWidget parent_instance;
  char *title;
  char *subtitle;
};

typedef struct
{
  IdeTweaksItem *root;
  GVariant *variant;
  guint pos;
  int selected;
} VisitState;

enum {
  PROP_0,
  PROP_TITLE,
  PROP_SUBTITLE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeTweaksCombo, ide_tweaks_combo, IDE_TYPE_TWEAKS_WIDGET)

static GParamSpec *properties [N_PROPS];

static IdeTweaksItemVisitResult
ide_tweaks_combo_visit_children_cb (IdeTweaksItem *item,
                                    gpointer       user_data)
{
  VisitState *state = user_data;

  if (IDE_IS_TWEAKS_FACTORY (item))
    return IDE_TWEAKS_ITEM_VISIT_RECURSE;

  if (IDE_IS_TWEAKS_CHOICE (item) &&
      !_ide_tweaks_item_is_hidden (item, state->root))
    {
      if (state->variant)
        {
          GVariant *choice_variant = ide_tweaks_choice_get_value (IDE_TWEAKS_CHOICE (item));

          if (choice_variant && g_variant_equal (state->variant, choice_variant))
            state->selected = state->pos;
        }

      state->pos++;

      return IDE_TWEAKS_ITEM_VISIT_ACCEPT_AND_CONTINUE;
    }

  return IDE_TWEAKS_ITEM_VISIT_CONTINUE;
}

static void
visit_state_finalize (VisitState *state)
{
  g_clear_pointer (&state->variant, g_variant_unref);
  g_clear_weak_pointer (&state->root);
}

static void
visit_state_unref (gpointer data)
{
  g_rc_box_release_full (data, (GDestroyNotify)visit_state_finalize);
}

static GtkWidget *
ide_tweaks_combo_create_for_item (IdeTweaksWidget *instance,
                                  IdeTweaksItem   *widget)
{
  IdeTweaksCombo *self = (IdeTweaksCombo *)widget;
  g_autoptr(IdeTweaksModel) model = NULL;
  g_autoptr(GtkExpression) expression = NULL;
  g_autoptr(GVariant) variant = NULL;
  IdeTweaksBinding *binding = NULL;
  AdwComboRow *row;
  VisitState *state;
  GType type;

  g_assert (IDE_IS_TWEAKS_COMBO (self));

  if (!(binding = ide_tweaks_widget_get_binding (IDE_TWEAKS_WIDGET (widget))))
    return NULL;

  if (ide_tweaks_binding_get_expected_type (binding, &type))
    {
      const GVariantType *expected_type = _ide_tweaks_gtype_to_variant_type (type);
      g_auto(GValue) value = G_VALUE_INIT;

      g_value_init (&value, type);
      if (ide_tweaks_binding_get_value (binding, &value))
        {
          if ((variant = g_settings_set_mapping (&value, expected_type, NULL)))
            g_variant_take_ref (variant);
        }
    }

  state = g_rc_box_new0 (VisitState);
  g_set_weak_pointer (&state->root, ide_tweaks_item_get_root (IDE_TWEAKS_ITEM (self)));
  state->selected = -1;
  state->variant = variant ? g_variant_ref (variant) : NULL;

  model = ide_tweaks_model_new (IDE_TWEAKS_ITEM (self),
                                ide_tweaks_combo_visit_children_cb,
                                state, visit_state_unref);

  expression = gtk_property_expression_new (IDE_TYPE_TWEAKS_CHOICE, NULL, "title");

  row = g_object_new (IDE_TYPE_TWEAKS_COMBO_ROW,
                      "expression", expression,
                      "title", self->title,
                      "subtitle", self->subtitle,
                      "binding", binding,
                      "model", model,
                      "selected", state->selected > -1 ? state->selected : 0,
                      NULL);

  return GTK_WIDGET (row);
}

static gboolean
ide_tweaks_combo_accepts (IdeTweaksItem *item,
                          IdeTweaksItem *child)
{
  return IDE_IS_TWEAKS_CHOICE (child) ||
         IDE_IS_TWEAKS_FACTORY (child);
}

static void
ide_tweaks_combo_dispose (GObject *object)
{
  IdeTweaksCombo *self = (IdeTweaksCombo *)object;

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
    case PROP_TITLE:
      g_value_set_string (value, ide_tweaks_combo_get_title (self));
      break;

    case PROP_SUBTITLE:
      g_value_set_string (value, ide_tweaks_combo_get_subtitle (self));
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
    case PROP_TITLE:
      ide_tweaks_combo_set_title (self, g_value_get_string (value));
      break;

    case PROP_SUBTITLE:
      ide_tweaks_combo_set_subtitle (self, g_value_get_string (value));
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

  if (g_set_str (&self->title, title))
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

  if (g_set_str (&self->subtitle, subtitle))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SUBTITLE]);
}
