/*
 * ide-tweaks-panel-list-row.c
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

#define G_LOG_DOMAIN "ide-tweaks-panel-list-row"

#include "config.h"

#include "ide-tweaks-panel-list-row-private.h"

struct _IdeTweaksPanelListRow
{
  GtkListBoxRow  parent_instance;
  IdeTweaksItem *item;
};

G_DEFINE_FINAL_TYPE (IdeTweaksPanelListRow, ide_tweaks_panel_list_row, GTK_TYPE_LIST_BOX_ROW)

enum {
  PROP_0,
  PROP_ITEM,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
ide_tweaks_panel_list_row_dispose (GObject *object)
{
  IdeTweaksPanelListRow *self = (IdeTweaksPanelListRow *)object;

  g_clear_object (&self->item);

  G_OBJECT_CLASS (ide_tweaks_panel_list_row_parent_class)->dispose (object);
}

static void
ide_tweaks_panel_list_row_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  IdeTweaksPanelListRow *self = IDE_TWEAKS_PANEL_LIST_ROW (object);

  switch (prop_id)
    {
    case PROP_ITEM:
      g_value_set_object (value, ide_tweaks_panel_list_row_get_item (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_panel_list_row_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  IdeTweaksPanelListRow *self = IDE_TWEAKS_PANEL_LIST_ROW (object);

  switch (prop_id)
    {
    case PROP_ITEM:
      ide_tweaks_panel_list_row_set_item (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_panel_list_row_class_init (IdeTweaksPanelListRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ide_tweaks_panel_list_row_dispose;
  object_class->get_property = ide_tweaks_panel_list_row_get_property;
  object_class->set_property = ide_tweaks_panel_list_row_set_property;

  properties [PROP_ITEM] =
    g_param_spec_object ("item", NULL, NULL,
                         IDE_TYPE_TWEAKS_ITEM,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-tweaks/ide-tweaks-panel-list-row.ui");
}

static void
ide_tweaks_panel_list_row_init (IdeTweaksPanelListRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

IdeTweaksItem *
ide_tweaks_panel_list_row_get_item (IdeTweaksPanelListRow *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_PANEL_LIST_ROW (self), NULL);

  return self->item;
}

void
ide_tweaks_panel_list_row_set_item (IdeTweaksPanelListRow *self,
                                    IdeTweaksItem         *item)
{
  g_return_if_fail (IDE_IS_TWEAKS_PANEL_LIST_ROW (self));
  g_return_if_fail (!item || IDE_IS_TWEAKS_ITEM (item));

  if (g_set_object (&self->item, item))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ITEM]);
}
