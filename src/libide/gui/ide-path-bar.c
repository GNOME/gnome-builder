/* ide-path-bar.c
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

#define G_LOG_DOMAIN "ide-path-bar"

#include "config.h"

#include "ide-path-bar.h"

typedef struct
{
  IdePath *path;
  IdePath *selection;
} IdePathBarPrivate;

enum {
  PROP_0,
  PROP_PATH,
  PROP_SELECTION,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (IdePathBar, ide_path_bar, GTK_TYPE_CONTAINER)

static GParamSpec *properties [N_PROPS];

static GtkWidget *
ide_path_bar_create_button (const gchar *title,
                            gboolean     with_arrow)
{
  GtkButton *button;
  GtkLabel *label;
  GtkBox *box;

  box = g_object_new (GTK_TYPE_BOX,
                      "spacing", 3,
                      "visible", TRUE,
                      NULL);

  label = g_object_new (GTK_TYPE_LABEL,
                        "visible", TRUE,
                        NULL);
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (label));

  if (with_arrow)
    gtk_container_add (GTK_CONTAINER (box),
                       g_object_new (GTK_TYPE_IMAGE,
                                     "visible", TRUE,
                                     NULL));

  button = g_object_new (GTK_TYPE_BUTTON,
                         "focus-on-click", FALSE,
                         "child", box,
                         "visible", TRUE,
                         NULL);

  return GTK_WIDGET (button);
}

static void
ide_path_bar_update_buttons (IdePathBar *self)
{
  IdePathBarPrivate *priv = ide_path_bar_get_instance_private (self);
  guint n_elements;

  g_assert (IDE_IS_PATH_BAR (self));

  gtk_container_foreach (GTK_CONTAINER (self),
                         (GtkCallback) gtk_widget_destroy,
                         NULL);

  if (priv->path == NULL)
    return;

  n_elements = ide_path_get_n_elements (priv->path);

  for (guint i = 0; i < n_elements; i++)
    {
      IdePathElement *element = ide_path_get_element (priv->path, i);
      const gchar *title;
      GtkWidget *button;
      gboolean has_arrow;

      g_assert (IDE_IS_PATH_ELEMENT (element));

      title = ide_path_element_get_title (element);
      has_arrow = i + 1 == n_elements;
      button = ide_path_bar_create_button (title, has_arrow);

      gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (button));
    }

}

static void
ide_path_bar_add (GtkContainer *container,
                  GtkWidget    *widget)
{
  g_assert (IDE_IS_PATH_BAR (container));
  g_assert (GTK_IS_WIDGET (widget));

  gtk_widget_set_parent (widget, GTK_WIDGET (container));
}

static void
ide_path_bar_finalize (GObject *object)
{
  IdePathBar *self = (IdePathBar *)object;
  IdePathBarPrivate *priv = ide_path_bar_get_instance_private (self);

  g_clear_object (&priv->path);
  g_clear_object (&priv->selection);

  G_OBJECT_CLASS (ide_path_bar_parent_class)->finalize (object);
}

static void
ide_path_bar_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  IdePathBar *self = IDE_PATH_BAR (object);

  switch (prop_id)
    {
    case PROP_PATH:
      g_value_set_object (value, ide_path_bar_get_path (self));
      break;

    case PROP_SELECTION:
      g_value_set_object (value, ide_path_bar_get_selection (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_path_bar_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  IdePathBar *self = IDE_PATH_BAR (object);

  switch (prop_id)
    {
    case PROP_PATH:
      ide_path_bar_set_path (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_path_bar_class_init (IdePathBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->finalize = ide_path_bar_finalize;
  object_class->get_property = ide_path_bar_get_property;
  object_class->set_property = ide_path_bar_set_property;

  container_class->add = ide_path_bar_add;

  properties [PROP_PATH] =
    g_param_spec_object ("path",
                         "Path",
                         "The path that is displayed",
                         IDE_TYPE_PATH,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_SELECTION] =
    g_param_spec_object ("selection",
                         "Selection",
                         "The selected portion of the path",
                         IDE_TYPE_PATH,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_path_bar_init (IdePathBar *self)
{
  GtkStyleContext *style_context;

  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);
  gtk_widget_set_redraw_on_allocate (GTK_WIDGET (self), FALSE);

  style_context = gtk_widget_get_style_context (GTK_WIDGET (self));
  gtk_style_context_add_class (style_context, GTK_STYLE_CLASS_LINKED);
}

/**
 * ide_path_bar_get_selection:
 * @self: an #IdePathBar
 *
 * Get the path up to the selected element.
 *
 * Returns: (transfer none) (nullable): an #IdePathBar or %NULL
 *
 * Since: 3.34
 */
IdePath *
ide_path_bar_get_selection (IdePathBar *self)
{
  IdePathBarPrivate *priv = ide_path_bar_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_PATH_BAR (self), NULL);

  return priv->selection;
}

/**
 * ide_path_bar_get_path:
 * @self: an #IdePathBar
 *
 * Gets the path for the whole path bar. This may include elements
 * after the selected element if the selected element is before
 * the end of the path.
 *
 * Returns: (transfer none) (nullable): an #IdePath or %NULL
 *
 * Since: 3.34
 */
IdePath *
ide_path_bar_get_path (IdePathBar *self)
{
  IdePathBarPrivate *priv = ide_path_bar_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_PATH_BAR (self), NULL);

  return priv->selection;
}

void
ide_path_bar_set_path (IdePathBar *self,
                       IdePath    *path)
{
  IdePathBarPrivate *priv = ide_path_bar_get_instance_private (self);

  g_return_if_fail (IDE_IS_PATH_BAR (self));
  g_return_if_fail (!path || IDE_IS_PATH (path));

  if (g_set_object (&priv->path, path))
    {
      g_set_object (&priv->selection, path);
      ide_path_bar_update_buttons (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PATH]);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SELECTION]);
    }
}
