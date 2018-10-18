/* gbp-glade-layout-stack-addin.c
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#define G_LOG_DOMAIN "gbp-glade-layout-stack-addin"

#include <glib/gi18n.h>
#include <gladeui/glade.h>

#include "gbp-glade-layout-stack-addin.h"
#include "gbp-glade-view.h"

struct _GbpGladeLayoutStackAddin
{
  GObject         parent_instance;
  GtkMenuButton  *button;
  GladeInspector *inspector;
};

static void layout_stack_addin_iface_init (IdeLayoutStackAddinInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GbpGladeLayoutStackAddin, gbp_glade_layout_stack_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_LAYOUT_STACK_ADDIN,
                                                layout_stack_addin_iface_init))

static void
gbp_glade_layout_stack_addin_class_init (GbpGladeLayoutStackAddinClass *klass)
{
}

static void
gbp_glade_layout_stack_addin_init (GbpGladeLayoutStackAddin *self)
{
}

static void
gbp_glade_layout_stack_addin_load (IdeLayoutStackAddin *addin,
                                   IdeLayoutStack      *stack)
{
  GbpGladeLayoutStackAddin *self = (GbpGladeLayoutStackAddin *)addin;
  GtkPopover *popover;
  GtkWidget *header;

  g_assert (GBP_IS_GLADE_LAYOUT_STACK_ADDIN (self));
  g_assert (IDE_IS_LAYOUT_STACK (stack));

  header = ide_layout_stack_get_titlebar (stack);

  popover = g_object_new (GTK_TYPE_POPOVER,
                          "width-request", 400,
                          "height-request", 400,
                          "position", GTK_POS_BOTTOM,
                          NULL);
  dzl_gtk_widget_add_style_class (GTK_WIDGET (popover), "glade-stack-header");

  self->button = g_object_new (GTK_TYPE_MENU_BUTTON,
                               "label", _("Select Widget…"),
                               "popover", popover,
                               "visible", FALSE,
                               NULL);
  g_signal_connect (self->button,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->button);
  ide_layout_stack_header_add_custom_title (IDE_LAYOUT_STACK_HEADER (header),
                                            GTK_WIDGET (self->button),
                                            200);

  self->inspector = g_object_new (GLADE_TYPE_INSPECTOR,
                                  "visible", TRUE,
                                  NULL);
  gtk_container_add (GTK_CONTAINER (popover), GTK_WIDGET (self->inspector));
}

static void
gbp_glade_layout_stack_addin_unload (IdeLayoutStackAddin *addin,
                                     IdeLayoutStack      *stack)
{
  GbpGladeLayoutStackAddin *self = (GbpGladeLayoutStackAddin *)addin;

  g_assert (GBP_IS_GLADE_LAYOUT_STACK_ADDIN (self));
  g_assert (IDE_IS_LAYOUT_STACK (stack));

  if (self->button != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->button));
}

static void
gbp_glade_layout_stack_addin_set_view (IdeLayoutStackAddin *addin,
                                       IdeLayoutView       *view)
{
  GbpGladeLayoutStackAddin *self = (GbpGladeLayoutStackAddin *)addin;
  GladeProject *project = NULL;

  g_assert (GBP_IS_GLADE_LAYOUT_STACK_ADDIN (self));
  g_assert (!view || IDE_IS_LAYOUT_VIEW (view));

  if (GBP_IS_GLADE_VIEW (view))
    project = gbp_glade_view_get_project (GBP_GLADE_VIEW (view));

  glade_inspector_set_project (self->inspector, project);
  gtk_widget_set_visible (GTK_WIDGET (self->button), project != NULL);
}

static void
layout_stack_addin_iface_init (IdeLayoutStackAddinInterface *iface)
{
  iface->load = gbp_glade_layout_stack_addin_load;
  iface->unload = gbp_glade_layout_stack_addin_unload;
  iface->set_view = gbp_glade_layout_stack_addin_set_view;
}

