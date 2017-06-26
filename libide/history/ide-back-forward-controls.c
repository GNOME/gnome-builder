/* ide-back-forward-controls.c
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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
 */

#define G_LOG_DOMAIN "ide-back-forward-controls"

#include <dazzle.h>

#include "ide-back-forward-controls.h"

struct _IdeBackForwardControls
{
  GtkBox parent_instance;

  GtkButton *previous_button;
  GtkButton *next_button;
};

G_DEFINE_TYPE (IdeBackForwardControls, ide_back_forward_controls, GTK_TYPE_BOX)

static void
ide_back_forward_controls_class_init (IdeBackForwardControlsClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_css_name (widget_class, "idebackforwardcontrols");
}

static void
ide_back_forward_controls_init (IdeBackForwardControls *self)
{
  dzl_gtk_widget_add_style_class (GTK_WIDGET (self), "linked");

  gtk_widget_set_sensitive (GTK_WIDGET (self), FALSE);

  self->previous_button = g_object_new (GTK_TYPE_BUTTON,
                                        "child", g_object_new (GTK_TYPE_IMAGE,
                                                               "icon-name", "pan-start-symbolic",
                                                               "visible", TRUE,
                                                               NULL),
                                        "visible", TRUE,
                                        NULL);
  g_signal_connect (self->previous_button,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->previous_button);
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->previous_button));

  self->next_button = g_object_new (GTK_TYPE_BUTTON,
                                    "child", g_object_new (GTK_TYPE_IMAGE,
                                                           "icon-name", "pan-end-symbolic",
                                                           "visible", TRUE,
                                                           NULL),
                                    "visible", TRUE,
                                    NULL);
  g_signal_connect (self->next_button,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->next_button);
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->next_button));
}

GtkWidget *
ide_back_forward_controls_new (void)
{
  return g_object_new (IDE_TYPE_BACK_FORWARD_CONTROLS, NULL);
}
