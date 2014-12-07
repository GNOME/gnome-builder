/* gb-document-view.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "document-view"

#include <glib/gi18n.h>

#include "gb-document-view.h"

struct _GbDocumentViewPrivate
{
  GtkBox *controls;
};

static void buildable_init (GtkBuildableIface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GbDocumentView,
                                  gb_document_view,
                                  GTK_TYPE_BOX,
                                  G_ADD_PRIVATE (GbDocumentView)
                                  G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                         buildable_init))

enum {
  PROP_0,
  PROP_CONTROLS,
  PROP_DOCUMENT,
  LAST_PROP
};

enum {
  CLOSE,
  LAST_SIGNAL
};

static GParamSpec *gParamSpecs [LAST_PROP];
static guint       gSignals [LAST_SIGNAL];

/**
 * gb_document_view_get_controls:
 *
 * This returns a #GtkBox that can be used to place controls that should be
 * displayed at the top of the document stack. It is available in the Gtk
 * template as an internal child named "controls".
 *
 * Returns: (transfer none): A #GtkBox
 */
GtkWidget *
gb_document_view_get_controls (GbDocumentView *view)
{
  g_return_val_if_fail (GB_IS_DOCUMENT_VIEW (view), NULL);

  return GTK_WIDGET (view->priv->controls);
}

/**
 * gb_document_view_get_document:
 *
 * Retrieves the #GbDocument being viewed.
 *
 * Returns: (transfer none): A #GbDocument.
 */
GbDocument *
gb_document_view_get_document (GbDocumentView *view)
{
  g_return_val_if_fail (GB_IS_DOCUMENT_VIEW (view), NULL);

  if (GB_DOCUMENT_VIEW_GET_CLASS (view)->get_document)
    return GB_DOCUMENT_VIEW_GET_CLASS (view)->get_document (view);

  g_warning ("%s() does not implement get_document() vfunc.",
             g_type_name (G_TYPE_FROM_INSTANCE (view)));

  return NULL;
}

void
gb_document_view_close (GbDocumentView *view)
{
  g_return_if_fail (GB_IS_DOCUMENT_VIEW (view));

  g_signal_emit (view, gSignals [CLOSE], 0);
}

static GObject *
gb_document_view_get_internal_child (GtkBuildable *buildable,
                                     GtkBuilder   *builder,
                                     const gchar  *childname)
{
  GbDocumentView *view = (GbDocumentView *)buildable;

  if (g_strcmp0 (childname, "controls") == 0)
    return G_OBJECT (view->priv->controls);

  return NULL;
}

static void
gb_document_view_destroy (GtkWidget *widget)
{
  GbDocumentViewPrivate *priv = GB_DOCUMENT_VIEW (widget)->priv;

  g_clear_object (&priv->controls);

  GTK_WIDGET_CLASS (gb_document_view_parent_class)->destroy (widget);
}

static void
gb_document_view_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GbDocumentView *self = GB_DOCUMENT_VIEW (object);

  switch (prop_id)
    {
    case PROP_CONTROLS:
      g_value_set_object (value, gb_document_view_get_controls (self));
      break;

    case PROP_DOCUMENT:
      g_value_set_object (value, gb_document_view_get_document (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_document_view_class_init (GbDocumentViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = gb_document_view_get_property;

  widget_class->destroy = gb_document_view_destroy;

  gParamSpecs [PROP_CONTROLS] =
    g_param_spec_object ("controls",
                         _("Controls"),
                         _("The widget containing the view controls."),
                         GTK_TYPE_BOX,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_CONTROLS,
                                   gParamSpecs [PROP_CONTROLS]);

  gParamSpecs [PROP_DOCUMENT] =
    g_param_spec_object ("document",
                         _("Document"),
                         _("The document being viewed."),
                         GB_TYPE_DOCUMENT,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_DOCUMENT,
                                   gParamSpecs [PROP_DOCUMENT]);

  gSignals [CLOSE] =
    g_signal_new ("close",
                  GB_TYPE_DOCUMENT_VIEW,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GbDocumentViewClass, close),
                  NULL,
                  NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);
}

static void
gb_document_view_init (GbDocumentView *self)
{
  self->priv = gb_document_view_get_instance_private (self);

  self->priv->controls = g_object_new (GTK_TYPE_BOX,
                                       "orientation", GTK_ORIENTATION_HORIZONTAL,
                                       "visible", TRUE,
                                       NULL);
  g_object_ref_sink (self->priv->controls);
}

static void
buildable_init (GtkBuildableIface *iface)
{
  iface->get_internal_child = gb_document_view_get_internal_child;
}
