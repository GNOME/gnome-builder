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

#include "ide-tweaks-page.h"
#include "ide-tweaks-panel-private.h"

struct _IdeTweaksPanel
{
  AdwBin         parent_instance;
  IdeTweaksPage *page;
  guint          folded : 1;
};

enum {
  PROP_0,
  PROP_FOLDED,
  PROP_PAGE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeTweaksPanel, ide_tweaks_panel, ADW_TYPE_BIN)

static GParamSpec *properties [N_PROPS];

static void
ide_tweaks_panel_dispose (GObject *object)
{
  IdeTweaksPanel *self = (IdeTweaksPanel *)object;

  g_clear_object (&self->page);

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

    case PROP_PAGE:
      g_value_set_object (value, ide_tweaks_panel_get_page (self));
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
    case PROP_PAGE:
      self->page = g_value_dup_object (value);
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

  properties[PROP_PAGE] =
    g_param_spec_object ("page", NULL, NULL,
                         IDE_TYPE_TWEAKS_PAGE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-tweaks/ide-tweaks-panel.ui");
}

static void
ide_tweaks_panel_init (IdeTweaksPanel *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
ide_tweaks_panel_new (IdeTweaksPage *page)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_PAGE (page), NULL);

  return g_object_new (IDE_TYPE_TWEAKS_PANEL,
                       "page", page,
                       NULL);
}

IdeTweaksPage *
ide_tweaks_panel_get_page (IdeTweaksPanel *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_PANEL (self), NULL);

  return self->page;
}

gboolean
ide_tweaks_panel_get_folded (IdeTweaksPanel *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_PANEL (self), FALSE);

  return self->folded;
}
