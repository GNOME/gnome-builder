/* gb-html-view.c
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

#define G_LOG_DOMAIN "html-view"

#include <glib/gi18n.h>
#include <gtksourceview/gtksourcefile.h>
#include <webkit2/webkit2.h>

#include "gb-editor-document.h"
#include "gb-html-view.h"
#include "gb-log.h"

struct _GbHtmlViewPrivate
{
  /* Objects owned by view */
  GbHtmlDocument *document;

  /* References owned by Gtk template */
  WebKitWebView  *web_view;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbHtmlView, gb_html_view, GB_TYPE_DOCUMENT_VIEW)

enum {
  PROP_0,
  PROP_DOCUMENT,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

GtkWidget *
gb_html_view_new (GbHtmlDocument *document)
{
  return g_object_new (GB_TYPE_HTML_VIEW,
                       "document", document,
                       NULL);
}

static void
gb_html_view_changed (GbHtmlView    *view,
                      GtkTextBuffer *buffer)
{
  GbHtmlViewPrivate *priv;
  gchar *content;
  gchar *base_uri = NULL;

  ENTRY;

  g_return_if_fail (GB_IS_HTML_VIEW (view));
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  priv = view->priv;

  if (GB_IS_EDITOR_DOCUMENT (view->priv->document))
    {
      GtkSourceFile *file;

      file = gb_editor_document_get_file (GB_EDITOR_DOCUMENT (priv->document));

      if (file)
        {
          GFile *location;

          location = gtk_source_file_get_location (file);

          if (location)
            base_uri = g_file_get_uri (location);
        }
    }

  content = gb_html_document_get_content (view->priv->document);
  webkit_web_view_load_html (view->priv->web_view, content, base_uri);

  g_free (content);
  g_free (base_uri);

  EXIT;
}

static void
gb_html_view_connect (GbHtmlView     *view,
                      GbHtmlDocument *document)
{
  GtkTextBuffer *buffer;

  g_return_if_fail (GB_IS_HTML_VIEW (view));
  g_return_if_fail (GB_IS_HTML_DOCUMENT (document));

  buffer = gb_html_document_get_buffer (document);
  if (!buffer)
    return;

  g_signal_connect_object (buffer,
                           "changed",
                           G_CALLBACK (gb_html_view_changed),
                           view,
                           G_CONNECT_SWAPPED);

  gb_html_view_changed (view, buffer);
}

static void
gb_html_view_disconnect (GbHtmlView     *view,
                         GbHtmlDocument *document)
{
  GtkTextBuffer *buffer;

  g_return_if_fail (GB_IS_HTML_VIEW (view));
  g_return_if_fail (GB_IS_HTML_DOCUMENT (document));

  buffer = gb_html_document_get_buffer (document);
  if (!buffer)
    return;

  g_signal_handlers_disconnect_by_func (buffer,
                                        G_CALLBACK (gb_html_view_changed),
                                        view);
}

static GbDocument *
gb_html_view_get_document (GbDocumentView *view)
{
  GbHtmlView *self = (GbHtmlView *)view;

  g_return_val_if_fail (GB_IS_HTML_VIEW (self), NULL);

  return GB_DOCUMENT (self->priv->document);
}

static void
gb_html_view_set_document (GbHtmlView *view,
                           GbDocument *document)
{
  g_return_if_fail (GB_IS_HTML_VIEW (view));
  g_return_if_fail (GB_IS_DOCUMENT (document));

  if (!GB_IS_HTML_DOCUMENT (document))
    {
      g_warning ("GbHtmlView does not know how to handle a document "
                 "of type %s",
                 g_type_name (G_TYPE_FROM_INSTANCE (document)));
      return;
    }

  if (document != (GbDocument *)view->priv->document)
    {
      if (view->priv->document)
        {
          gb_html_view_disconnect (view, view->priv->document);
          g_clear_object (&view->priv->document);
        }

      if (document)
        {
          view->priv->document = g_object_ref (document);
          gb_html_view_connect (view, view->priv->document);
        }

      g_object_notify_by_pspec (G_OBJECT (view), gParamSpecs [PROP_DOCUMENT]);
    }
}

static void
gb_html_view_finalize (GObject *object)
{
  GbHtmlViewPrivate *priv = GB_HTML_VIEW (object)->priv;

  g_clear_object (&priv->document);

  G_OBJECT_CLASS (gb_html_view_parent_class)->finalize (object);
}

static void
gb_html_view_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GbHtmlView *self = GB_HTML_VIEW (object);

  switch (prop_id)
    {
    case PROP_DOCUMENT:
      g_value_set_object (value,
                          gb_html_view_get_document (GB_DOCUMENT_VIEW (self)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_html_view_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GbHtmlView *self = GB_HTML_VIEW (object);

  switch (prop_id)
    {
    case PROP_DOCUMENT:
      gb_html_view_set_document (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_html_view_class_init (GbHtmlViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GbDocumentViewClass *view_class = GB_DOCUMENT_VIEW_CLASS (klass);

  object_class->finalize = gb_html_view_finalize;
  object_class->get_property = gb_html_view_get_property;
  object_class->set_property = gb_html_view_set_property;

  view_class->get_document = gb_html_view_get_document;

  gParamSpecs [PROP_DOCUMENT] =
    g_param_spec_object ("document",
                         _("Document"),
                         _("The document to view as HTML."),
                         GB_TYPE_DOCUMENT,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_DOCUMENT,
                                   gParamSpecs [PROP_DOCUMENT]);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/builder/ui/gb-html-view.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GbHtmlView, web_view);

  g_type_ensure (WEBKIT_TYPE_WEB_VIEW);
}

static void
gb_html_view_init (GbHtmlView *self)
{
  self->priv = gb_html_view_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));
}
