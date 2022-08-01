/* ide-tweaks-panel.c
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

#define G_LOG_DOMAIN "ide-tweaks-panel"

#include "config.h"

#include "ide-tweaks-panel-private.h"

typedef struct
{
  IdeTweaksItem *item;
  char *title;
  guint folded : 1;
} IdeTweaksPanelPrivate;

enum {
  PROP_0,
  PROP_FOLDED,
  PROP_ITEM,
  PROP_TITLE,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (IdeTweaksPanel, ide_tweaks_panel, ADW_TYPE_BIN)

static GParamSpec *properties [N_PROPS];

static void
ide_tweaks_panel_set_item (IdeTweaksPanel *self,
                           IdeTweaksItem  *item)
{
  IdeTweaksPanelPrivate *priv = ide_tweaks_panel_get_instance_private (self);

  g_return_if_fail (IDE_IS_TWEAKS_PANEL (self));
  g_return_if_fail (!item || IDE_IS_TWEAKS_ITEM (item));

  g_set_object (&priv->item, item);
}

static void
ide_tweaks_panel_dispose (GObject *object)
{
  IdeTweaksPanel *self = (IdeTweaksPanel *)object;
  IdeTweaksPanelPrivate *priv = ide_tweaks_panel_get_instance_private (self);

  g_clear_object (&priv->item);

  G_OBJECT_CLASS (ide_tweaks_panel_parent_class)->dispose (object);
}

static void
ide_tweaks_panel_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  IdeTweaksPanel *self = IDE_TWEAKS_PANEL (object);

  switch (prop_id)
    {
    case PROP_FOLDED:
      g_value_set_boolean (value, ide_tweaks_panel_get_folded (self));
      break;

    case PROP_ITEM:
      g_value_set_object (value, ide_tweaks_panel_get_item (self));
      break;

    case PROP_TITLE:
      g_value_set_string (value, ide_tweaks_panel_get_title (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_panel_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  IdeTweaksPanel *self = IDE_TWEAKS_PANEL (object);

  switch (prop_id)
    {
    case PROP_ITEM:
      ide_tweaks_panel_set_item (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_panel_class_init (IdeTweaksPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ide_tweaks_panel_dispose;
  object_class->get_property = ide_tweaks_panel_get_property;
  object_class->set_property = ide_tweaks_panel_set_property;

  properties[PROP_FOLDED] =
    g_param_spec_boolean ("folded", NULL, NULL,
                         FALSE,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_ITEM] =
    g_param_spec_object ("item", NULL, NULL,
                         IDE_TYPE_TWEAKS_ITEM,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties[PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-tweaks/ide-tweaks-panel.ui");
}

static void
ide_tweaks_panel_init (IdeTweaksPanel *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

IdeTweaksItem *
ide_tweaks_panel_get_item (IdeTweaksPanel *self)
{
  IdeTweaksPanelPrivate *priv = ide_tweaks_panel_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TWEAKS_PANEL (self), NULL);

  return priv->item;
}

const char *
ide_tweaks_panel_get_title (IdeTweaksPanel *self)
{
  IdeTweaksPanelPrivate *priv = ide_tweaks_panel_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TWEAKS_PANEL (self), NULL);

  return priv->title;
}

gboolean
ide_tweaks_panel_get_folded (IdeTweaksPanel *self)
{
  IdeTweaksPanelPrivate *priv = ide_tweaks_panel_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TWEAKS_PANEL (self), FALSE);

  return priv->folded;
}
