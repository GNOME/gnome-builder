/* ide-editor-perspective.c
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

#define G_LOG_DOMAIN "ide-editor-perspective"

#include "ide-editor-perspective.h"

struct _IdeEditorPerspective
{
  IdeLayout      parent_instance;

  IdeLayoutGrid *grid;
};

enum {
  PROP_0,
  N_PROPS
};

G_DEFINE_TYPE (IdeEditorPerspective, ide_editor_perspective, IDE_TYPE_LAYOUT)

static GParamSpec *properties [N_PROPS];

static void
ide_editor_perspective_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_perspective_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_perspective_class_init (IdeEditorPerspectiveClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = ide_editor_perspective_get_property;
  object_class->set_property = ide_editor_perspective_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-editor-perspective.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeEditorPerspective, grid);
}

static void
ide_editor_perspective_init (IdeEditorPerspective *self)
{
}

/**
 * ide_editor_perspective_get_grid:
 * @self: a #IdeEditorPerspective
 *
 * Gets the grid for the perspective. This is the area containing
 * grid columns, stacks, and views.
 *
 * Returns: (transfer none): An #IdeLayoutGrid.
 *
 * Since: 3.26
 */
IdeLayoutGrid *
ide_editor_perspective_get_grid (IdeEditorPerspective *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_PERSPECTIVE (self), NULL);

  return self->grid;
}
