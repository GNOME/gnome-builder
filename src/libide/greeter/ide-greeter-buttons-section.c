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
  GtkWidget  parent_instance;
  GtkBox    *box;
};

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeGreeterButtonsSection, ide_greeter_buttons_section, GTK_TYPE_WIDGET,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_GREETER_SECTION, NULL))

static void
ide_greeter_buttons_section_dispose (GObject *object)
{
  IdeGreeterButtonsSection *self = (IdeGreeterButtonsSection *)object;

  g_clear_pointer ((GtkWidget **)&self->box, gtk_widget_unparent);

  G_OBJECT_CLASS (ide_greeter_buttons_section_parent_class)->dispose (object);
}

static void
ide_greeter_buttons_section_class_init (IdeGreeterButtonsSectionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ide_greeter_buttons_section_dispose;

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

static void
ide_greeter_buttons_section_init (IdeGreeterButtonsSection *self)
{
  self->box = g_object_new (GTK_TYPE_BOX,
                            "margin-bottom", 3,
                            "margin-top", 3,
                            "orientation", GTK_ORIENTATION_HORIZONTAL,
                            "homogeneous", TRUE,
                            "halign", GTK_ALIGN_CENTER,
                            "hexpand", TRUE,
                            "spacing", 12,
                            "width-request", 600,
                            "visible", TRUE,
                            NULL);
  gtk_widget_set_parent (GTK_WIDGET (self->box), GTK_WIDGET (self));

  ide_greeter_buttons_section_add_button (self,
                                          0,
                                          g_object_new (GTK_TYPE_BUTTON,
                                                        "label", _("Select a _Folder…"),
                                                        "visible", TRUE,
                                                        "action-name", "greeter.open",
                                                        "use-underline", TRUE,
                                                        NULL));
}

#define GET_PRIORITY(w)   GPOINTER_TO_INT(g_object_get_data(G_OBJECT(w),"PRIORITY"))
#define SET_PRIORITY(w,i) g_object_set_data(G_OBJECT(w),"PRIORITY",GINT_TO_POINTER(i))

void
ide_greeter_buttons_section_add_button (IdeGreeterButtonsSection *self,
                                        gint                      priority,
                                        GtkWidget                *widget)
{
  GtkWidget *sibling = NULL;

  g_return_if_fail (IDE_IS_GREETER_BUTTONS_SECTION (self));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  SET_PRIORITY (widget, priority);

  for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->box));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (priority < GET_PRIORITY (child))
        break;
      sibling = child;
    }

  gtk_box_insert_child_after (self->box, widget, sibling);
}
