/* ide-editor-utilities.c
 *
 * Copyright © 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-editor-utilities"

#include "editor/ide-editor-utilities.h"

/**
 * SECTION:ide-editor-utilities
 * @title: IdeEditorUtilities
 * @short_description: Container for utilities in the editor perspective
 *
 * The #IdeEditorUtilities widget is a convenient container for widgets that
 * are not primarily navigation based but are useful from the editor. Such
 * an example could be build logs, application output, and other ancillary
 * information for the user.
 *
 * You can get this widget via ide_editor_perspective_get_utilities().
 *
 * Since: 3.26
 */

struct _IdeEditorUtilities
{
  IdeLayoutPane     parent_instance;

  GtkStackSwitcher *stack_switcher;
  GtkStack         *stack;

  guint             loading : 1;
};

G_DEFINE_TYPE (IdeEditorUtilities, ide_editor_utilities, IDE_TYPE_LAYOUT_PANE)

static void
tweak_radio_button (GtkWidget *widget,
                    gpointer   user_data)
{
  gtk_widget_set_vexpand (widget, TRUE);
}

static void
ide_editor_utilities_add (GtkContainer *container,
                          GtkWidget    *widget)
{
  IdeEditorUtilities *self = (IdeEditorUtilities *)container;

  if (self->loading)
    GTK_CONTAINER_CLASS (ide_editor_utilities_parent_class)->add (container, widget);
  else
    gtk_container_add (GTK_CONTAINER (self->stack), widget);

  if (DZL_IS_DOCK_WIDGET (widget))
    {
      g_autofree gchar *icon_name = NULL;
      g_autofree gchar *title = NULL;

      g_object_get (widget,
                    "icon-name", &icon_name,
                    "title", &title,
                    NULL);

      gtk_container_child_set (GTK_CONTAINER (self->stack), widget,
                               "title", title,
                               "icon-name", icon_name,
                               NULL);

      gtk_container_foreach (GTK_CONTAINER (self->stack_switcher),
                             tweak_radio_button,
                             NULL);
    }
}

static void
ide_editor_utilities_destroy (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (ide_editor_utilities_parent_class)->destroy (widget);
}

static void
ide_editor_utilities_class_init (IdeEditorUtilitiesClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  widget_class->destroy = ide_editor_utilities_destroy;

  container_class->add = ide_editor_utilities_add;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-editor-utilities.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeEditorUtilities, stack_switcher);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorUtilities, stack);
  gtk_widget_class_set_css_name (widget_class, "ideeditorutilities");
}

static void
ide_editor_utilities_init (IdeEditorUtilities *self)
{
  self->loading = TRUE;
  gtk_widget_init_template (GTK_WIDGET (self));
  self->loading = FALSE;
}
