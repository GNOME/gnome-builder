/* gbp-dspy-surface.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-dspy-surface"

#include "config.h"

#include <dazzle.h>
#include <dspy.h>
#include <glib/gi18n.h>

#include "gbp-dspy-surface.h"

struct _GbpDspySurface
{
  IdeSurface  parent_instance;
  DspyView   *view;
};

G_DEFINE_TYPE (GbpDspySurface, gbp_dspy_surface, IDE_TYPE_SURFACE)

static void
gbp_dspy_surface_class_init (GbpDspySurfaceClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/dspy/gbp-dspy-surface.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpDspySurface, view);

  g_type_ensure (DSPY_TYPE_VIEW);
}

static void
gbp_dspy_surface_init (GbpDspySurface *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GbpDspySurface *
gbp_dspy_surface_new (void)
{
  return g_object_new (GBP_TYPE_DSPY_SURFACE, NULL);
}
