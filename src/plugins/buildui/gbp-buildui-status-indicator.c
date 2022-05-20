/* gbp-buildui-status-indicator.c
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

#define G_LOG_DOMAIN "gbp-buildui-status-indicator"

#include "config.h"

#include <libide-foundry.h>

#include "gbp-buildui-status-indicator.h"

struct _GbpBuilduiStatusIndicator
{
  GtkWidget parent_instance;

  GtkWidget *box;
  GtkWidget *error_label;
  GtkWidget *warning_label;
};

G_DEFINE_FINAL_TYPE (GbpBuilduiStatusIndicator, gbp_buildui_status_indicator, GTK_TYPE_WIDGET)

static void
gbp_buildui_status_indicator_dispose (GObject *object)
{
  GbpBuilduiStatusIndicator *self = (GbpBuilduiStatusIndicator *)object;

  g_clear_pointer (&self->box, gtk_widget_unparent);

  G_OBJECT_CLASS (gbp_buildui_status_indicator_parent_class)->dispose (object);
}

static void
gbp_buildui_status_indicator_class_init (GbpBuilduiStatusIndicatorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gbp_buildui_status_indicator_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/buildui/gbp-buildui-status-indicator.ui");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiStatusIndicator, box);
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiStatusIndicator, error_label);
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiStatusIndicator, warning_label);
}

static void
gbp_buildui_status_indicator_init (GbpBuilduiStatusIndicator *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
gbp_buildui_status_indicator_connect (GbpBuilduiStatusIndicator *self,
                                      IdeContext                *context)
{
  IdeBuildManager *build_manager;

  IDE_ENTRY;

  g_assert (GBP_IS_BUILDUI_STATUS_INDICATOR (self));
  g_assert (IDE_IS_CONTEXT (context));

  build_manager = ide_build_manager_from_context (context);

  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  g_object_bind_property (build_manager, "error-count", self->error_label, "label", G_BINDING_SYNC_CREATE);
  g_object_bind_property (build_manager, "warning-count", self->warning_label, "label", G_BINDING_SYNC_CREATE);

  IDE_EXIT;
}

GbpBuilduiStatusIndicator *
gbp_buildui_status_indicator_new (IdeContext *context)
{
  GbpBuilduiStatusIndicator *self;

  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (ide_context_has_project (context), NULL);

  self = g_object_new (GBP_TYPE_BUILDUI_STATUS_INDICATOR, NULL);
  gbp_buildui_status_indicator_connect (self, context);

  return self;
}
