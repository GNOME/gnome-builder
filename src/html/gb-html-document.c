/* gb-html-document.c
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

#define G_LOG_DOMAIN "html-document"

#include <glib/gi18n.h>
#include <gtksourceview/gtksourcefile.h>

#include "gb-editor-document.h"
#include "gb-html-document.h"
#include "gb-html-view.h"
#include "gs-markdown.h"

struct _GbHtmlDocumentPrivate
{
  GtkTextBuffer           *buffer;
  gchar                   *title;
  GbHtmlDocumentTransform  transform;
};

static void gb_html_document_init_document (GbDocumentInterface *iface);

G_DEFINE_TYPE_EXTENDED (GbHtmlDocument, gb_html_document, G_TYPE_OBJECT, 0,
                        G_ADD_PRIVATE (GbHtmlDocument)
                        G_IMPLEMENT_INTERFACE (GB_TYPE_DOCUMENT,
                                               gb_html_document_init_document))

enum {
  PROP_0,
  PROP_BUFFER,
  PROP_MODIFIED,
  PROP_TITLE,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

GbHtmlDocument *
gb_html_document_new (void)
{
  return g_object_new (GB_TYPE_HTML_DOCUMENT, NULL);
}

void
gb_html_document_set_transform_func (GbHtmlDocument          *document,
                                     GbHtmlDocumentTransform  transform)
{
  g_return_if_fail (GB_IS_HTML_DOCUMENT (document));

  document->priv->transform = transform;
}

gchar *
gb_html_document_get_content (GbHtmlDocument *document)
{
  GtkTextIter begin;
  GtkTextIter end;
  gchar *tmp;
  gchar *str;

  g_return_val_if_fail (GB_IS_HTML_DOCUMENT (document), NULL);

  if (!document->priv->buffer)
    return NULL;

  gtk_text_buffer_get_bounds (document->priv->buffer, &begin, &end);
  str = gtk_text_iter_get_slice (&begin, &end);

  if (document->priv->transform)
    {
      tmp = document->priv->transform (document, str);
      g_free (str);
      str = tmp;
    }

  return str;
}

static const gchar *
gb_html_document_get_title (GbDocument *document)
{
  GbHtmlDocument *self = (GbHtmlDocument *)document;

  g_return_val_if_fail (GB_IS_HTML_DOCUMENT (self), NULL);

  if (self->priv->title)
    return self->priv->title;

  return _("HTML Preview");
}

static void
gb_html_document_notify_location (GbHtmlDocument *document,
                                  GParamSpec     *pspec,
                                  GtkSourceFile  *file)
{
  GFile *location;

  g_return_if_fail (GB_IS_HTML_DOCUMENT (document));
  g_return_if_fail (GTK_SOURCE_IS_FILE (file));

  location = gtk_source_file_get_location (file);

  g_clear_pointer (&document->priv->title, g_free);

  if (location)
    {
      gchar *filename;

      filename = g_file_get_basename (location);
      document->priv->title = g_strdup_printf (_("%s (Preview)"), filename);
    }

  g_object_notify (G_OBJECT (document), "title");
}

static void
gb_html_document_connect (GbHtmlDocument *document,
                          GtkTextBuffer  *buffer)
{
  g_return_if_fail (GB_IS_HTML_DOCUMENT (document));
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  if (GB_IS_EDITOR_DOCUMENT (buffer))
    {
      GtkSourceFile *file;

      file = gb_editor_document_get_file (GB_EDITOR_DOCUMENT (buffer));
      g_signal_connect_object (file,
                               "notify::location",
                               G_CALLBACK (gb_html_document_notify_location),
                               document,
                               G_CONNECT_SWAPPED);
      gb_html_document_notify_location (document, NULL, file);
    }
}

static void
gb_html_document_disconnect (GbHtmlDocument *document,
                             GtkTextBuffer  *buffer)
{
  g_return_if_fail (GB_IS_HTML_DOCUMENT (document));
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  if (GB_IS_EDITOR_DOCUMENT (buffer))
    {
      GtkSourceFile *file;

      file = gb_editor_document_get_file (GB_EDITOR_DOCUMENT (buffer));
      g_signal_handlers_disconnect_by_func (file,
                                            G_CALLBACK (gb_html_document_notify_location),
                                            document);
    }
}

GtkTextBuffer *
gb_html_document_get_buffer (GbHtmlDocument *document)
{
  g_return_val_if_fail (GB_IS_HTML_DOCUMENT (document), NULL);

  return document->priv->buffer;
}

static void
gb_html_document_set_buffer (GbHtmlDocument *document,
                             GtkTextBuffer  *buffer)
{
  GbHtmlDocument *self = (GbHtmlDocument *)document;

  g_return_if_fail (GB_IS_HTML_DOCUMENT (self));
  g_return_if_fail (!buffer || GTK_IS_TEXT_BUFFER (buffer));

  if (buffer != self->priv->buffer)
    {
      if (self->priv->buffer)
        {
          gb_html_document_disconnect (self, buffer);
          g_clear_object (&self->priv->buffer);
        }

      if (buffer)
        {
          self->priv->buffer = g_object_ref (buffer);
          gb_html_document_connect (self, buffer);
        }

      g_object_notify_by_pspec (G_OBJECT (document), gParamSpecs [PROP_BUFFER]);
    }
}

static gboolean
gb_html_document_get_modified (GbDocument *document)
{
  return FALSE;
}

static GtkWidget *
gb_html_document_create_view (GbDocument *document)
{
  GbHtmlDocument *self = (GbHtmlDocument *)document;

  g_return_val_if_fail (GB_IS_HTML_DOCUMENT (self), NULL);

  return g_object_new (GB_TYPE_HTML_VIEW,
                       "document", self,
                       "visible", TRUE,
                       NULL);
}

static void
gb_html_document_finalize (GObject *object)
{
  GbHtmlDocumentPrivate *priv = GB_HTML_DOCUMENT (object)->priv;

  g_clear_pointer (&priv->title, g_free);
  g_clear_object (&priv->buffer);

  G_OBJECT_CLASS (gb_html_document_parent_class)->finalize (object);
}

static void
gb_html_document_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GbHtmlDocument *self = GB_HTML_DOCUMENT (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      g_value_set_object (value, gb_html_document_get_buffer (self));
      break;

    case PROP_MODIFIED:
      g_value_set_boolean (value,
                           gb_html_document_get_modified (GB_DOCUMENT (self)));
      break;

    case PROP_TITLE:
      g_value_set_string (value,
                          gb_html_document_get_title (GB_DOCUMENT (self)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_html_document_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GbHtmlDocument *self = GB_HTML_DOCUMENT (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      gb_html_document_set_buffer (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_html_document_class_init (GbHtmlDocumentClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gb_html_document_finalize;
  object_class->get_property = gb_html_document_get_property;
  object_class->set_property = gb_html_document_set_property;

  g_object_class_override_property (object_class, PROP_TITLE, "modified");
  g_object_class_override_property (object_class, PROP_TITLE, "title");

  gParamSpecs [PROP_BUFFER] =
    g_param_spec_object ("buffer",
                         _("Buffer"),
                         _("The buffer to monitor for changes."),
                         GTK_TYPE_TEXT_BUFFER,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_BUFFER,
                                   gParamSpecs [PROP_BUFFER]);
}

static void
gb_html_document_init (GbHtmlDocument *self)
{
  self->priv = gb_html_document_get_instance_private (self);
}

static void
gb_html_document_init_document (GbDocumentInterface *iface)
{
  iface->get_title = gb_html_document_get_title;
  iface->get_modified = gb_html_document_get_modified;
  iface->create_view = gb_html_document_create_view;
}

gchar *
gb_html_markdown_transform (GbHtmlDocument *document,
                            const gchar    *content)
{
  GsMarkdown *md;
  gchar *str;

  g_return_if_fail (GB_IS_HTML_DOCUMENT (document));
  g_return_if_fail (content);

  md = gs_markdown_new (GS_MARKDOWN_OUTPUT_HTML);
  gs_markdown_set_autolinkify (md, TRUE);
  gs_markdown_set_escape (md, TRUE);

  str = gs_markdown_parse (md, content);

  if (str)
    {
      GBytes *css;
      const guint8 *css_data;
      gchar *tmp;

      css = g_resources_lookup_data ("/org/gnome/builder/css/markdown.css",
                                     0, NULL);
      css_data = g_bytes_get_data (css, NULL);
      tmp = g_strdup_printf ("<html>\n"
                             " <style>%s</style>\n"
                             " <body>\n"
                             "  <div class=\"markdown-body\">\n"
                             "   %s\n"
                             "  </div>\n"
                             " </body>\n"
                             "</html>",
                             (gchar *)css_data, str);
      g_free (str);

      str = tmp;
    }

  g_clear_object (&md);

  return str;
}
