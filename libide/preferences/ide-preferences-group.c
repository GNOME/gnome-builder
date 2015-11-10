/* ide-preferences-group.c
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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

#include "ide-preferences-bin.h"
#include "ide-preferences-group.h"

struct _IdePreferencesGroup
{
  GtkBin      parent_instance;

  gint        priority;
  guint       is_list : 1;

  GtkLabel   *title;
  GtkBox     *box;
  GtkListBox *list_box;
  GtkFrame   *list_box_frame;
};

G_DEFINE_TYPE (IdePreferencesGroup, ide_preferences_group, GTK_TYPE_BIN)

enum {
  PROP_0,
  PROP_IS_LIST,
  PROP_PRIORITY,
  PROP_TITLE,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static void
ide_preferences_group_row_activated (IdePreferencesGroup *self,
                                     GtkListBoxRow       *row,
                                     GtkListBox          *list_box)
{
  GtkWidget *child;

  g_assert (IDE_IS_PREFERENCES_GROUP (self));
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  child = gtk_bin_get_child (GTK_BIN (row));
  if (child != NULL)
    gtk_widget_activate (child);
}

static void
ide_preferences_group_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  IdePreferencesGroup *self = IDE_PREFERENCES_GROUP (object);

  switch (prop_id)
    {
    case PROP_IS_LIST:
      g_value_set_boolean (value, self->is_list);
      break;

    case PROP_PRIORITY:
      g_value_set_int (value, self->priority);
      break;

    case PROP_TITLE:
      g_value_set_string (value, gtk_label_get_label (self->title));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_preferences_group_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  IdePreferencesGroup *self = IDE_PREFERENCES_GROUP (object);

  switch (prop_id)
    {
    case PROP_IS_LIST:
      self->is_list = g_value_get_boolean (value);
      gtk_widget_set_visible (GTK_WIDGET (self->box), !self->is_list);
      gtk_widget_set_visible (GTK_WIDGET (self->list_box_frame), self->is_list);
      break;

    case PROP_PRIORITY:
      self->priority = g_value_get_int (value);
      break;

    case PROP_TITLE:
      gtk_label_set_label (self->title, g_value_get_string (value));
      gtk_widget_set_visible (GTK_WIDGET (self->title), !!g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_preferences_group_class_init (IdePreferencesGroupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = ide_preferences_group_get_property;
  object_class->set_property = ide_preferences_group_set_property;

  properties [PROP_IS_LIST] =
    g_param_spec_boolean ("is-list",
                          "Is List",
                          "If the group should be rendered as a listbox.",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_PRIORITY] =
    g_param_spec_int ("priority",
                      "Priority",
                      "Priority",
                      G_MININT,
                      G_MAXINT,
                      0,
                      (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "Title",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-preferences-group.ui");
  gtk_widget_class_bind_template_child (widget_class, IdePreferencesGroup, box);
  gtk_widget_class_bind_template_child (widget_class, IdePreferencesGroup, list_box);
  gtk_widget_class_bind_template_child (widget_class, IdePreferencesGroup, list_box_frame);
  gtk_widget_class_bind_template_child (widget_class, IdePreferencesGroup, title);
}

static void
ide_preferences_group_init (IdePreferencesGroup *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self->list_box,
                           "row-activated",
                           G_CALLBACK (ide_preferences_group_row_activated),
                           self,
                           G_CONNECT_SWAPPED);
}

void
ide_preferences_group_add (IdePreferencesGroup *self,
                           GtkWidget           *widget)
{
  gint position = -1;

  g_return_if_fail (IDE_IS_PREFERENCES_GROUP (self));
  g_return_if_fail (IDE_IS_PREFERENCES_BIN (widget));

  if (self->is_list)
    {
      GtkWidget *row;

      row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                          "child", widget,
                          "visible", TRUE,
                          NULL);
      gtk_container_add (GTK_CONTAINER (self->list_box), row);
    }
  else
    {
      gtk_container_add_with_properties (GTK_CONTAINER (self->box), widget,
                                         "position", position,
                                         NULL);
    }
}
