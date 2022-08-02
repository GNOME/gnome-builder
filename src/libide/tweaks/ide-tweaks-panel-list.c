/* ide-tweaks-panel-list.c
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

#define G_LOG_DOMAIN "ide-tweaks-panel-list"

#include "config.h"

#include "ide-tweaks-panel-list-private.h"

struct _IdeTweaksPanelList
{
  AdwBin         parent_instance;

  IdeTweaksItem *item;

  GtkListBox    *list_box;
};

enum {
  PROP_0,
  PROP_ITEM,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeTweaksPanelList, ide_tweaks_panel_list, ADW_TYPE_BIN)

static GParamSpec *properties [N_PROPS];

static void
ide_tweaks_panel_list_dispose (GObject *object)
{
  IdeTweaksPanelList *self = (IdeTweaksPanelList *)object;

  g_clear_object (&self->item);

  G_OBJECT_CLASS (ide_tweaks_panel_list_parent_class)->dispose (object);
}

static void
ide_tweaks_panel_list_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  IdeTweaksPanelList *self = IDE_TWEAKS_PANEL_LIST (object);

  switch (prop_id)
    {
    case PROP_ITEM:
      g_value_set_object (value, ide_tweaks_panel_list_get_item (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_panel_list_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  IdeTweaksPanelList *self = IDE_TWEAKS_PANEL_LIST (object);

  switch (prop_id)
    {
    case PROP_ITEM:
      ide_tweaks_panel_list_set_item (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_panel_list_class_init (IdeTweaksPanelListClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ide_tweaks_panel_list_dispose;
  object_class->get_property = ide_tweaks_panel_list_get_property;
  object_class->set_property = ide_tweaks_panel_list_set_property;

  properties [PROP_ITEM] =
    g_param_spec_object ("item", NULL, NULL,
                         IDE_TYPE_TWEAKS_ITEM,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-tweaks/ide-tweaks-panel-list.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeTweaksPanelList, list_box);
}

static void
ide_tweaks_panel_list_init (IdeTweaksPanelList *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
ide_tweaks_panel_list_new (void)
{
  return g_object_new (IDE_TYPE_TWEAKS_PANEL_LIST, NULL);
}

/**
 * ide_tweaks_panel_list_get_item:
 *
 * Gets the parent item of the panel list. Children of this
 * item are what are displayed in the panel list.
 *
 * Returns: (transfer none) (nullable): an #IdeTweaksItem or %NULL
 */
IdeTweaksItem *
ide_tweaks_panel_list_get_item (IdeTweaksPanelList *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_PANEL_LIST (self), NULL);

  return self->item;
}

void
ide_tweaks_panel_list_set_item (IdeTweaksPanelList *self,
                                IdeTweaksItem      *item)
{
  g_return_if_fail (IDE_IS_TWEAKS_PANEL_LIST (self));
  g_return_if_fail (IDE_IS_TWEAKS_ITEM (item));

  if (g_set_object (&self->item, item))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ITEM]);
    }
}
