/* gbp-vcsui-switcher-popover.c
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

#define G_LOG_DOMAIN "gbp-vcsui-switcher-popover"

#include "config.h"

#include "gbp-vcsui-switcher-popover.h"

struct _GbpVcsuiSwitcherPopover
{
  GtkPopover parent_instance;
  IdeVcs *vcs;
};

enum {
  PROP_0,
  PROP_VCS,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpVcsuiSwitcherPopover, gbp_vcsui_switcher_popover, GTK_TYPE_POPOVER)

static GParamSpec *properties [N_PROPS];

static void
gbp_vcsui_switcher_popover_dispose (GObject *object)
{
  GbpVcsuiSwitcherPopover *self = (GbpVcsuiSwitcherPopover *)object;

  g_clear_object (&self->vcs);

  G_OBJECT_CLASS (gbp_vcsui_switcher_popover_parent_class)->dispose (object);
}

static void
gbp_vcsui_switcher_popover_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  GbpVcsuiSwitcherPopover *self = GBP_VCSUI_SWITCHER_POPOVER (object);

  switch (prop_id)
    {
    case PROP_VCS:
      g_value_set_object (value, self->vcs);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_vcsui_switcher_popover_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  GbpVcsuiSwitcherPopover *self = GBP_VCSUI_SWITCHER_POPOVER (object);

  switch (prop_id)
    {
    case PROP_VCS:
      gbp_vcsui_switcher_popover_set_vcs (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_vcsui_switcher_popover_class_init (GbpVcsuiSwitcherPopoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gbp_vcsui_switcher_popover_dispose;
  object_class->get_property = gbp_vcsui_switcher_popover_get_property;
  object_class->set_property = gbp_vcsui_switcher_popover_set_property;

  properties [PROP_VCS] =
    g_param_spec_object ("vcs",
                         "Vcs",
                         "The version control system",
                         IDE_TYPE_VCS,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/vcsui/gbp-vcsui-switcher-popover.ui");
}

static void
gbp_vcsui_switcher_popover_init (GbpVcsuiSwitcherPopover *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

IdeVcs *
gbp_vcsui_switcher_popover_get_vcs (GbpVcsuiSwitcherPopover *self)
{
  g_return_val_if_fail (GBP_IS_VCSUI_SWITCHER_POPOVER (self), NULL);

  return self->vcs;
}

void
gbp_vcsui_switcher_popover_set_vcs (GbpVcsuiSwitcherPopover *self,
                                    IdeVcs                  *vcs)
{
  g_return_if_fail (GBP_IS_VCSUI_SWITCHER_POPOVER (self));
  g_return_if_fail (!vcs || IDE_IS_VCS (vcs));

  if (g_set_object (&self->vcs, vcs))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_VCS]);
    }
}

GtkWidget *
gbp_vcsui_switcher_popover_new (void)
{
  return g_object_new (GBP_TYPE_VCSUI_SWITCHER_POPOVER, NULL);
}
