/* gb-editor-view.c
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

#define G_LOG_DOMAIN "editor-view"

#include <glib/gi18n.h>

#include "gb-editor-frame.h"
#include "gb-editor-view.h"

struct _GbEditorViewPrivate
{
  /* References owned by view */
  GbEditorDocument *document;

  /* References owned by GtkWidget template */
  GtkPaned        *paned;
  GtkToggleButton *split_button;
  GbEditorFrame   *frame;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbEditorView, gb_editor_view, GB_TYPE_DOCUMENT_VIEW)

enum {
  PROP_0,
  PROP_DOCUMENT,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

GtkWidget *
gb_editor_view_new (GbEditorDocument *document)
{
  return g_object_new (GB_TYPE_EDITOR_VIEW,
                       "document", document,
                       NULL);
}

static void
gb_editor_view_connect (GbEditorView     *view,
                        GbEditorDocument *document)
{
  g_return_if_fail (GB_IS_EDITOR_VIEW (view));
  g_return_if_fail (GB_IS_EDITOR_DOCUMENT (document));

  gb_editor_frame_set_document (view->priv->frame, document);
}

static void
gb_editor_view_disconnect (GbEditorView     *view,
                           GbEditorDocument *document)
{
  g_return_if_fail (GB_IS_EDITOR_VIEW (view));
  g_return_if_fail (GB_IS_EDITOR_DOCUMENT (document));

  gb_editor_frame_set_document (view->priv->frame, NULL);
}

static GbDocument *
gb_editor_view_get_document (GbDocumentView *view)
{
  GbEditorViewPrivate *priv;

  g_return_val_if_fail (GB_IS_EDITOR_VIEW (view), NULL);

  priv = GB_EDITOR_VIEW (view)->priv;

  return GB_DOCUMENT (priv->document);
}

static void
gb_editor_view_set_document (GbEditorView     *view,
                             GbEditorDocument *document)
{
  g_return_if_fail (GB_IS_EDITOR_VIEW (view));
  g_return_if_fail (GB_IS_EDITOR_DOCUMENT (document));

  if (document != view->priv->document)
    {
      if (view->priv->document)
        {
          gb_editor_view_disconnect (view, document);
          g_clear_object (&view->priv->document);
        }

      if (document)
        {
          view->priv->document = g_object_ref (document);
          gb_editor_view_connect (view, document);
        }

      g_object_notify_by_pspec (G_OBJECT (view), gParamSpecs [PROP_DOCUMENT]);
    }
}

static void
gb_editor_view_finalize (GObject *object)
{
  GbEditorViewPrivate *priv = GB_EDITOR_VIEW (object)->priv;
  GbEditorView *view = (GbEditorView *)object;

  if (priv->document)
    {
      gb_editor_view_disconnect (view, priv->document);
      g_clear_object (&priv->document);
    }

  G_OBJECT_CLASS (gb_editor_view_parent_class)->finalize (object);
}

static void
gb_editor_view_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GbEditorView *self = GB_EDITOR_VIEW (object);

  switch (prop_id)
    {
    case PROP_DOCUMENT:
      g_value_set_object (value, self->priv->document);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_editor_view_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GbEditorView *self = GB_EDITOR_VIEW (object);

  switch (prop_id)
    {
    case PROP_DOCUMENT:
      gb_editor_view_set_document (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_editor_view_class_init (GbEditorViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GbDocumentViewClass *view_class = GB_DOCUMENT_VIEW_CLASS (klass);

  object_class->finalize = gb_editor_view_finalize;
  object_class->get_property = gb_editor_view_get_property;
  object_class->set_property = gb_editor_view_set_property;

  view_class->get_document = gb_editor_view_get_document;

  gParamSpecs [PROP_DOCUMENT] =
    g_param_spec_object ("document",
                         _("Document"),
                         _("The document edited by the view."),
                         GB_TYPE_DOCUMENT,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_DOCUMENT,
                                   gParamSpecs [PROP_DOCUMENT]);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/builder/ui/gb-editor-view.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GbEditorView, frame);
  gtk_widget_class_bind_template_child_private (widget_class, GbEditorView, paned);
  gtk_widget_class_bind_template_child_private (widget_class, GbEditorView, split_button);

  g_type_ensure (GB_TYPE_EDITOR_FRAME);
}

static void
gb_editor_view_init (GbEditorView *self)
{
  self->priv = gb_editor_view_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));
}
