/* ide-greeter-buttons-section.c
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

#define G_LOG_DOMAIN "ide-greeter-buttons-section"

#include "config.h"

#include <glib/gi18n.h>

#include "ide-greeter-buttons-section.h"

struct _IdeGreeterButtonsSection
{
  GtkBin  parent_instance;
  GtkBox *box;
};

G_DEFINE_TYPE_WITH_CODE (IdeGreeterButtonsSection, ide_greeter_buttons_section, GTK_TYPE_BIN,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_GREETER_SECTION, NULL))

static void
ide_greeter_buttons_section_class_init (IdeGreeterButtonsSectionClass *klass)
{
}

static void
ide_greeter_buttons_section_init (IdeGreeterButtonsSection *self)
{
  PangoAttrList *attrs;
  GtkBox *vbox;
  GtkLabel *label;

  g_object_set (self,
                "width-request", 600,
                "halign", GTK_ALIGN_CENTER,
                NULL);

  attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));

  vbox = g_object_new (GTK_TYPE_BOX,
                       "orientation", GTK_ORIENTATION_VERTICAL,
                       "spacing", 6,
                       "visible", TRUE,
                       NULL);
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (vbox));

  label = g_object_new (GTK_TYPE_LABEL,
                        "attributes", attrs,
                        "xalign", 0.0f,
                        "label", _("Add a Project"),
                        "visible", TRUE,
                        NULL);
  dzl_gtk_widget_add_style_class (GTK_WIDGET (label), "dim-label");
  gtk_container_add (GTK_CONTAINER (vbox), GTK_WIDGET (label));

  self->box = g_object_new (DZL_TYPE_PRIORITY_BOX,
                            "orientation", GTK_ORIENTATION_HORIZONTAL,
                            "homogeneous", TRUE,
                            "spacing", 12,
                            "visible", TRUE,
                            NULL);
  gtk_container_add (GTK_CONTAINER (vbox), GTK_WIDGET (self->box));

  ide_greeter_buttons_section_add_button (self,
                                          0,
                                          g_object_new (GTK_TYPE_BUTTON,
                                                        "label", _("Select a Folder…"),
                                                        "visible", TRUE,
                                                        "action-name", "win.open",
                                                        NULL));
  ide_greeter_buttons_section_add_button (self,
                                          100,
                                          g_object_new (GTK_TYPE_BUTTON,
                                                        "label", _("Clone Repository…"),
                                                        "visible", TRUE,
                                                        "action-name", "win.surface",
                                                        "action-target", g_variant_new_string ("clone"),
                                                        NULL));

  g_clear_pointer (&attrs, pango_attr_list_unref);
}

void
ide_greeter_buttons_section_add_button (IdeGreeterButtonsSection *self,
                                        gint                      priority,
                                        GtkWidget                *widget)
{
  g_return_if_fail (IDE_IS_GREETER_BUTTONS_SECTION (self));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_container_add_with_properties (GTK_CONTAINER (self->box), widget,
                                     "priority", priority,
                                     NULL);
}
