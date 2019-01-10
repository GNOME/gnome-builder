/* ide-editor-utilities.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-editor-utilities"

#include "config.h"

#include "ide-editor-utilities.h"

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
 * Since: 3.32
 */

struct _IdeEditorUtilities
{
  IdePanel  parent_instance;
  DzlDockStack  *stack;
};

G_DEFINE_TYPE (IdeEditorUtilities, ide_editor_utilities, IDE_TYPE_PANEL)

static void
ide_editor_utilities_add (GtkContainer *container,
                          GtkWidget    *widget)
{
  IdeEditorUtilities *self = (IdeEditorUtilities *)container;

  g_assert (IDE_IS_EDITOR_UTILITIES (self));
  g_assert (GTK_IS_WIDGET (widget));

  gtk_container_add (GTK_CONTAINER (self->stack), widget);
}

static void
ide_editor_utilities_class_init (IdeEditorUtilitiesClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  container_class->add = ide_editor_utilities_add;

  gtk_widget_class_set_css_name (widget_class, "ideeditorutilities");
}

static void
ide_editor_utilities_init (IdeEditorUtilities *self)
{
  self->stack = g_object_new (DZL_TYPE_DOCK_STACK,
                              "visible", TRUE,
                              NULL);
  GTK_CONTAINER_CLASS (ide_editor_utilities_parent_class)->add (GTK_CONTAINER (self),
                                                                GTK_WIDGET (self->stack));
  g_object_set (self->stack,
                "style", DZL_TAB_ICONS,
                "edge", GTK_POS_LEFT,
                NULL);
}
