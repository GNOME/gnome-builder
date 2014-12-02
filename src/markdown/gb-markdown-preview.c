/* gb-markdown-preview.c
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

#define G_LOG_DOMAIN "markdown"

#include <glib/gi18n.h>

#include "gb-log.h"
#include "gb-markdown-preview.h"
#include "gs-markdown.h"

struct _GbMarkdownPreviewPrivate
{
  GtkTextBuffer *buffer;
  guint          buffer_changed_handler;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbMarkdownPreview, gb_markdown_preview, WEBKIT_TYPE_WEB_VIEW)

enum {
  PROP_0,
  PROP_BUFFER,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

GtkWidget *
gb_markdown_preview_new (void)
{
  return g_object_new (GB_TYPE_MARKDOWN_PREVIEW, NULL);
}

/**
 * gb_markdown_preview_get_buffer:
 *
 *
 *
 * Returns: (transfer none): A #GtkTextBuffer.
 */
GtkTextBuffer *
gb_markdown_preview_get_buffer (GbMarkdownPreview *preview)
{
  g_return_val_if_fail (GB_IS_MARKDOWN_PREVIEW (preview), NULL);

  return preview->priv->buffer;
}

static void
gb_markdown_preview_load_html (GbMarkdownPreview *preview,
                               const gchar       *html)
{
  GBytes *css;
  gchar *built_html;
  const guint8 *css_data;

  css = g_resources_lookup_data ("/org/gnome/builder/css/markdown.css", 0, NULL);
  css_data = g_bytes_get_data (css, NULL);
  built_html = g_strdup_printf ("<html>\n"
                                " <style>%s</style>\n"
                                " <body>\n"
                                "  <div class=\"markdown-body\">\n"
                                "   %s\n"
                                "  </div>\n"
                                " </body>\n"
                                "</html>",
                                (gchar *)css_data, html);

  /*
   * TODO: Set base_uri based on a GFile or something.
   */
  webkit_web_view_load_html (WEBKIT_WEB_VIEW (preview),
                             built_html, NULL);

  g_bytes_unref (css);
  g_free (built_html);
}

static void
gb_markdown_preview_reload (GbMarkdownPreview *preview)
{
  GbMarkdownPreviewPrivate *priv;
  GsMarkdown *markdown;
  GtkTextIter begin;
  GtkTextIter end;
  gchar *text;
  gchar *html = NULL;

  ENTRY;

  g_return_if_fail (GB_IS_MARKDOWN_PREVIEW (preview));

  priv = preview->priv;

  if (!priv->buffer)
    EXIT;

  gtk_text_buffer_get_bounds (priv->buffer, &begin, &end);
  text = gtk_text_buffer_get_text (priv->buffer, &begin, &end, TRUE);

  markdown = gs_markdown_new (GS_MARKDOWN_OUTPUT_HTML);
  gs_markdown_set_autolinkify (markdown, TRUE);
  gs_markdown_set_escape (markdown, FALSE);

  if (!(html = gs_markdown_parse (markdown, text)))
    {
      g_warning (_("Failed to parse markdown."));
      GOTO (cleanup);
    }

  gb_markdown_preview_load_html (preview, html);

cleanup:
  g_free (html);
  g_free (text);
  g_object_unref (markdown);

  EXIT;
}

static void
on_buffer_changed_cb (GbMarkdownPreview *preview,
                      GtkTextBuffer     *buffer)
{
  g_return_if_fail (GB_IS_MARKDOWN_PREVIEW (preview));
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  gb_markdown_preview_reload (preview);
}

void
gb_markdown_preview_set_buffer (GbMarkdownPreview *preview,
                                GtkTextBuffer     *buffer)
{
  GbMarkdownPreviewPrivate *priv;

  g_return_if_fail (GB_IS_MARKDOWN_PREVIEW (preview));
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  priv = preview->priv;

  if (priv->buffer == buffer)
    return;

  if (priv->buffer)
    {
      g_signal_handler_disconnect (priv->buffer, priv->buffer_changed_handler);
      priv->buffer_changed_handler = 0;

      g_object_remove_weak_pointer (G_OBJECT (priv->buffer),
                                    (gpointer *)&priv->buffer);
      priv->buffer = NULL;
    }

  if (buffer)
    {
      priv->buffer = buffer;
      g_object_add_weak_pointer (G_OBJECT (priv->buffer),
                                 (gpointer *)&priv->buffer);
      priv->buffer_changed_handler =
        g_signal_connect_object (priv->buffer,
                                 "changed",
                                 G_CALLBACK (on_buffer_changed_cb),
                                 preview,
                                 G_CONNECT_SWAPPED);
      gb_markdown_preview_reload (preview);
    }
}

static void
gb_markdown_preview_dispose (GObject *object)
{
  GbMarkdownPreviewPrivate *priv = GB_MARKDOWN_PREVIEW (object)->priv;

  if (priv->buffer)
    {
      g_signal_handler_disconnect (priv->buffer, priv->buffer_changed_handler);
      priv->buffer_changed_handler = 0;
      g_object_remove_weak_pointer (G_OBJECT (priv->buffer),
                                    (gpointer *)&priv->buffer);
    }

  G_OBJECT_CLASS (gb_markdown_preview_parent_class)->dispose (object);
}

static void
gb_markdown_preview_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GbMarkdownPreview *self = GB_MARKDOWN_PREVIEW (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      g_value_set_object (value, gb_markdown_preview_get_buffer (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_markdown_preview_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GbMarkdownPreview *self = GB_MARKDOWN_PREVIEW (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      gb_markdown_preview_set_buffer (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_markdown_preview_class_init (GbMarkdownPreviewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gb_markdown_preview_dispose;
  object_class->get_property = gb_markdown_preview_get_property;
  object_class->set_property = gb_markdown_preview_set_property;

  gParamSpecs [PROP_BUFFER] =
    g_param_spec_object ("buffer",
                         _("Buffer"),
                         _("The text buffer containing the markdown text."),
                         GTK_TYPE_TEXT_BUFFER,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_BUFFER,
                                   gParamSpecs [PROP_BUFFER]);
}

static void
gb_markdown_preview_init (GbMarkdownPreview *self)
{
  self->priv = gb_markdown_preview_get_instance_private (self);
}
