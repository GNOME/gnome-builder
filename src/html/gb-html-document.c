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

#include <glib.h>
#include <glib/gi18n.h>
#include <gtksourceview/gtksourcefile.h>
#include <ide.h>

#include "gb-editor-document.h"
#include "gb-html-document.h"
#include "gb-html-view.h"

struct _GbHtmlDocument
{
  GObject                  parent_instance;

  GtkTextBuffer           *buffer;
  gchar                   *title;
  GbHtmlDocumentTransform  transform;
};

static void gb_html_document_init_document (GbDocumentInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GbHtmlDocument, gb_html_document, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GB_TYPE_DOCUMENT,
                                                gb_html_document_init_document))

enum {
  PROP_0,
  PROP_BUFFER,
  LAST_PROP,

  /* These are overridden */
  PROP_MODIFIED,
  PROP_READ_ONLY,
  PROP_TITLE
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

  document->transform = transform;
}

gchar *
gb_html_document_get_content (GbHtmlDocument *document)
{
  GtkTextIter begin;
  GtkTextIter end;
  gchar *tmp;
  gchar *str;

  g_return_val_if_fail (GB_IS_HTML_DOCUMENT (document), NULL);

  if (!document->buffer)
    return NULL;

  gtk_text_buffer_get_bounds (document->buffer, &begin, &end);
  str = gtk_text_iter_get_slice (&begin, &end);

  if (document->transform)
    {
      tmp = document->transform (document, str);
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

  if (self->title)
    return self->title;

  return _("HTML Preview");
}

static void
gb_html_document_notify_location (GbHtmlDocument *document,
                                  GParamSpec     *pspec,
                                  IdeFile        *file)
{
  GFile *location;

  g_return_if_fail (GB_IS_HTML_DOCUMENT (document));
  g_return_if_fail (IDE_IS_FILE (file));

  location = ide_file_get_file (file);

  g_clear_pointer (&document->title, g_free);

  if (location)
    {
      gchar *filename;

      filename = g_file_get_basename (location);
      document->title = g_strdup_printf (_("%s (Preview)"), filename);
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
      IdeFile *file;

      file = ide_buffer_get_file (IDE_BUFFER (buffer));
      g_signal_connect_object (file,
                               "notify::file",
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
      IdeFile *file;

      file = ide_buffer_get_file (IDE_BUFFER (buffer));
      g_signal_handlers_disconnect_by_func (file,
                                            G_CALLBACK (gb_html_document_notify_location),
                                            document);
    }
}

GtkTextBuffer *
gb_html_document_get_buffer (GbHtmlDocument *document)
{
  g_return_val_if_fail (GB_IS_HTML_DOCUMENT (document), NULL);

  return document->buffer;
}

static void
gb_html_document_set_buffer (GbHtmlDocument *document,
                             GtkTextBuffer  *buffer)
{
  GbHtmlDocument *self = (GbHtmlDocument *)document;

  g_return_if_fail (GB_IS_HTML_DOCUMENT (self));
  g_return_if_fail (!buffer || GTK_IS_TEXT_BUFFER (buffer));

  if (buffer != self->buffer)
    {
      if (self->buffer)
        {
          gb_html_document_disconnect (self, buffer);
          g_clear_object (&self->buffer);
        }

      if (buffer)
        {
          self->buffer = g_object_ref (buffer);
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

static gboolean
gb_html_document_get_read_only (GbDocument *document)
{
  return TRUE;
}

static void
gb_html_document_finalize (GObject *object)
{
  GbHtmlDocument *self = GB_HTML_DOCUMENT (object);

  g_clear_pointer (&self->title, g_free);
  g_clear_object (&self->buffer);

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

    case PROP_READ_ONLY:
      g_value_set_boolean (value, TRUE);
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

  g_object_class_override_property (object_class, PROP_MODIFIED, "modified");
  g_object_class_override_property (object_class, PROP_READ_ONLY, "read-only");
  g_object_class_override_property (object_class, PROP_TITLE, "title");

  gParamSpecs [PROP_BUFFER] =
    g_param_spec_object ("buffer",
                         "Buffer",
                         "The buffer to monitor for changes.",
                         GTK_TYPE_TEXT_BUFFER,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);
}

static void
gb_html_document_init (GbHtmlDocument *self)
{
}

static void
gb_html_document_init_document (GbDocumentInterface *iface)
{
  iface->get_title = gb_html_document_get_title;
  iface->get_modified = gb_html_document_get_modified;
  iface->create_view = gb_html_document_create_view;
  iface->get_read_only = gb_html_document_get_read_only;
}

static gchar *
escape_for_script (const gchar *str)
{
  GString *output;

  output = g_string_sized_new (strlen (str) * 2);

  for (; *str; str = g_utf8_next_char (str))
    {
      gunichar ch = g_utf8_get_char (str);

      switch (ch)
        {
        case '\b':
          g_string_append (output, "\\b");
          break;

        case '\f':
          g_string_append (output, "\\f");
          break;

        case '\r':
          g_string_append (output, "\\r");
          break;

        case '\n':
          g_string_append (output, "\\n");
          break;

        case '\t':
          g_string_append (output, "\\t");
          break;

        case '\v':
          g_string_append (output, "\\v");
          break;

        case '\"':
          g_string_append (output, "\\\"");
          break;

        default:
          g_string_append_unichar (output, ch);
          break;
        }
    }

  return g_string_free (output, FALSE);
}

gchar *
gb_html_markdown_transform (GbHtmlDocument *document,
                            const gchar    *content)
{
  gchar *str;

  str = escape_for_script (content);

  if (str)
    {
      GBytes *css;
      const guint8 *css_data;
      const guint8 *marked_data;
      const guint8 *markdown_view_data;
      GBytes *marked;
      GBytes *markdown_view;
      gchar *tmp;

      css = g_resources_lookup_data ("/org/gnome/builder/markdown/markdown.css",
                                     0, NULL);
      css_data = g_bytes_get_data (css, NULL);

      marked = g_resources_lookup_data ("/org/gnome/builder/markdown/marked.js",
                                        0, NULL);
      marked_data = g_bytes_get_data (marked, NULL);

      markdown_view = g_resources_lookup_data (
                      "/org/gnome/builder/markdown/markdown-view.js", 0, NULL);
      markdown_view_data = g_bytes_get_data (markdown_view, NULL);

      tmp = g_strdup_printf ("<html>\n"
                             " <head>\n"
                             "  <style>%s</style>\n"
                             "  <script>var str=\"%s\";</script>\n"
                             "  <script>%s</script>\n"
                             "  <script>%s</script>\n"
                             " </head>\n"
                             " <body onload=\"preview()\">\n"
                             "  <div class=\"markdown-body\" id=\"preview\">\n"
                             "  </div>\n"
                             " </body>\n"
                             "</html>",
                             (gchar *)css_data,
                             str,
                             (gchar *)marked_data,
                             (gchar *)markdown_view_data);

      g_bytes_unref (css);
      g_bytes_unref (marked);
      g_bytes_unref (markdown_view);
      g_free (str);

      str = tmp;
    }

  return str;
}
