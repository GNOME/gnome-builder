/* gb-greeter-window.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include <glib/gi18n.h>

#include "gb-greeter-project-row.h"
#include "gb-greeter-window.h"
#include "gb-scrolled-window.h"

struct _GbGreeterWindow
{
  GtkApplicationWindow parent_instance;

  GtkWidget  *header_bar;
  GtkListBox *my_projects_list_box;
  GtkListBox *other_projects_list_box;
};

G_DEFINE_TYPE (GbGreeterWindow, gb_greeter_window, GTK_TYPE_APPLICATION_WINDOW)

enum {
  PROP_0,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

static void
gb_greeter_window_row_header_cb (GtkListBoxRow *row,
                                 GtkListBoxRow *before,
                                 gpointer       user_data)
{
  g_assert (GTK_IS_LIST_BOX_ROW (row));

  if (before != NULL)
    {
      GtkWidget *header;

      header = g_object_new (GTK_TYPE_SEPARATOR,
                             "orientation", GTK_ORIENTATION_HORIZONTAL,
                             "visible", TRUE,
                             NULL);
      gtk_list_box_row_set_header (row, header);
    }
}

static void
gb_greeter_window_finalize (GObject *object)
{
  GbGreeterWindow *self = (GbGreeterWindow *)object;

  G_OBJECT_CLASS (gb_greeter_window_parent_class)->finalize (object);
}

static void
gb_greeter_window_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GbGreeterWindow *self = GB_GREETER_WINDOW (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_greeter_window_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GbGreeterWindow *self = GB_GREETER_WINDOW (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_greeter_window_class_init (GbGreeterWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gb_greeter_window_finalize;
  object_class->get_property = gb_greeter_window_get_property;
  object_class->set_property = gb_greeter_window_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/gb-greeter-window.ui");
  gtk_widget_class_bind_template_child (widget_class, GbGreeterWindow, header_bar);
  gtk_widget_class_bind_template_child (widget_class, GbGreeterWindow, my_projects_list_box);
  gtk_widget_class_bind_template_child (widget_class, GbGreeterWindow, other_projects_list_box);

  g_type_ensure (GB_TYPE_GREETER_PROJECT_ROW);
  g_type_ensure (GB_TYPE_SCROLLED_WINDOW);
}

static void
gb_greeter_window_init (GbGreeterWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_header_func (self->my_projects_list_box,
                                gb_greeter_window_row_header_cb,
                                NULL, NULL);
}
