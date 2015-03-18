/* gb-view.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>

#include "gb-view.h"

typedef struct
{
  GtkBox *controls;
} GbViewPrivate;

static void buildable_iface_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GbView, gb_view, GTK_TYPE_BOX,
                         G_ADD_PRIVATE (GbView)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, buildable_iface_init))

enum {
  PROP_0,
  PROP_CAN_SPLIT,
  PROP_DOCUMENT,
  PROP_TITLE,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

/**
 * gb_view_get_can_split:
 * @self: A #GbView.
 *
 * Checks if @self can create a split view. If so, %TRUE is returned. Otherwise, %FALSE.
 *
 * Returns: %TRUE if @self can create a split.
 */
gboolean
gb_view_get_can_split (GbView *self)
{
  g_return_val_if_fail (GB_IS_VIEW (self), FALSE);

  if (GB_VIEW_GET_CLASS (self)->get_can_split)
    return GB_VIEW_GET_CLASS (self)->get_can_split (self);

  return FALSE;
}

/**
 * gb_view_create_split:
 * @self: A #GbView.
 *
 * Creates a new view similar to @self that can be displayed in a split.
 * If the view does not support splits, %NULL will be returned.
 *
 * Returns: (transfer full): A #GbView.
 */
GbView *
gb_view_create_split (GbView *self)
{
  g_return_val_if_fail (GB_IS_VIEW (self), NULL);

  if (GB_VIEW_GET_CLASS (self)->create_split)
    return GB_VIEW_GET_CLASS (self)->create_split (self);

  return NULL;
}

/**
 * gb_view_get_controls:
 * @self: A #GbView.
 *
 * Gets the controls for the view.
 *
 * Returns: (transfer none) (nullable): A #GtkWidget.
 */
GtkWidget *
gb_view_get_controls (GbView *self)
{
  GbViewPrivate *priv = gb_view_get_instance_private (self);

  g_return_val_if_fail (GB_IS_VIEW (self), NULL);

  return GTK_WIDGET (priv->controls);
}

/**
 * gb_view_get_document:
 * @self: A #GbView.
 *
 * Gets the document for the view.
 *
 * Returns: (transfer none): A #GbDocument.
 */
GbDocument *
gb_view_get_document (GbView *self)
{
  g_return_val_if_fail (GB_IS_VIEW (self), NULL);

  if (GB_VIEW_GET_CLASS (self)->get_document)
    return GB_VIEW_GET_CLASS (self)->get_document (self);

  return NULL;
}

const gchar *
gb_view_get_title (GbView *self)
{
  GbDocument *document;

  if (GB_VIEW_GET_CLASS (self)->get_title)
    return GB_VIEW_GET_CLASS (self)->get_title (self);

  document = gb_view_get_document (self);

  return gb_document_get_title (document);
}

static void
gb_view_destroy (GtkWidget *widget)
{
  GbView *self = (GbView *)widget;
  GbViewPrivate *priv = gb_view_get_instance_private (self);

  g_clear_object (&priv->controls);

  GTK_WIDGET_CLASS (gb_view_parent_class)->destroy (widget);
}

static void
gb_view_get_property (GObject    *object,
                      guint       prop_id,
                      GValue     *value,
                      GParamSpec *pspec)
{
  GbView *self = GB_VIEW (object);

  switch (prop_id)
    {
    case PROP_CAN_SPLIT:
      g_value_set_boolean (value, gb_view_get_can_split (self));
      break;

    case PROP_DOCUMENT:
      g_value_set_object (value, gb_view_get_document (self));
      break;

    case PROP_TITLE:
      g_value_set_string (value, gb_view_get_title (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}


static void
gb_view_class_init (GbViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = gb_view_get_property;

  widget_class->destroy = gb_view_destroy;

  gParamSpecs [PROP_CAN_SPLIT] =
    g_param_spec_boolean ("can-split",
                          _("Can Split"),
                          _("If the view can be split."),
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_CAN_SPLIT, gParamSpecs [PROP_CAN_SPLIT]);

  gParamSpecs [PROP_DOCUMENT] =
    g_param_spec_object ("document",
                         _("Document"),
                         _("The underlying document."),
                         GB_TYPE_DOCUMENT,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_DOCUMENT, gParamSpecs [PROP_DOCUMENT]);

  gParamSpecs [PROP_TITLE] =
    g_param_spec_string ("title",
                         _("Title"),
                         _("The view title."),
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_TITLE, gParamSpecs [PROP_TITLE]);
}

static void
gb_view_init (GbView *self)
{
  GbViewPrivate *priv = gb_view_get_instance_private (self);
  GtkBox *controls;

  controls = g_object_new (GTK_TYPE_BOX,
                           "orientation", GTK_ORIENTATION_HORIZONTAL,
                           "visible", TRUE,
                           NULL);
  priv->controls = g_object_ref_sink (controls);
}

static GObject *
gb_view_get_internal_child (GtkBuildable *buildable,
                            GtkBuilder   *builder,
                            const gchar  *childname)
{
  GbView *self = (GbView *)buildable;
  GbViewPrivate *priv = gb_view_get_instance_private (self);

  g_assert (GB_IS_VIEW (self));

  if (g_strcmp0 (childname, "controls") == 0)
    return G_OBJECT (priv->controls);

  return NULL;
}

static void
buildable_iface_init (GtkBuildableIface *iface)
{
  iface->get_internal_child = gb_view_get_internal_child;
}
