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

#define G_LOG_DOMAIN "gb-html-view"

#include <glib/gi18n.h>
#include <gtksourceview/gtksourcefile.h>
#include <ide.h>
#include <webkit2/webkit2.h>

#include "gb-editor-document.h"
#include "gb-html-document.h"
#include "gb-html-view.h"
#include "gb-widget.h"

struct _GbHtmlView
{
  GbView          parent_instance;

  /* Objects owned by view */
  GbHtmlDocument *document;

  /* References owned by Gtk template */
  WebKitWebView  *web_view;
};

G_DEFINE_TYPE (GbHtmlView, gb_html_view, GB_TYPE_VIEW)

enum {
  PROP_0,
  PROP_DOCUMENT,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

static void
gb_html_view_changed (GbHtmlView    *self,
                      GtkTextBuffer *buffer)
{
  gchar *content;
  gchar *base_uri = NULL;

  IDE_ENTRY;

  g_return_if_fail (GB_IS_HTML_VIEW (self));
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  if (GB_IS_EDITOR_DOCUMENT (self->document))
    {
      IdeFile *file;

      file = ide_buffer_get_file (IDE_BUFFER (self->document));

      if (file)
        {
          GFile *location;

          location = ide_file_get_file (file);

          if (location)
            base_uri = g_file_get_uri (location);
        }
    }

  content = gb_html_document_get_content (self->document);
  webkit_web_view_load_html (self->web_view, content, base_uri);

  g_free (content);
  g_free (base_uri);

  IDE_EXIT;
}

static void
gb_html_view_connect (GbHtmlView     *self,
                      GbHtmlDocument *document)
{
  GtkTextBuffer *buffer;

  g_return_if_fail (GB_IS_HTML_VIEW (self));
  g_return_if_fail (GB_IS_HTML_DOCUMENT (document));

  buffer = gb_html_document_get_buffer (document);
  if (!buffer)
    return;

  g_signal_connect_object (buffer,
                           "changed",
                           G_CALLBACK (gb_html_view_changed),
                           self,
                           G_CONNECT_SWAPPED);

  gb_html_view_changed (self, buffer);
}

static void
gb_html_view_disconnect (GbHtmlView     *self,
                         GbHtmlDocument *document)
{
  GtkTextBuffer *buffer;

  g_return_if_fail (GB_IS_HTML_VIEW (self));
  g_return_if_fail (GB_IS_HTML_DOCUMENT (document));

  buffer = gb_html_document_get_buffer (document);
  if (!buffer)
    return;

  g_signal_handlers_disconnect_by_func (buffer,
                                        G_CALLBACK (gb_html_view_changed),
                                        self);
}

static GbDocument *
gb_html_view_get_document (GbView *view)
{
  GbHtmlView *self = (GbHtmlView *)view;

  g_return_val_if_fail (GB_IS_HTML_VIEW (self), NULL);

  return GB_DOCUMENT (self->document);
}

static void
gb_html_view_set_document (GbHtmlView *self,
                           GbDocument *document)
{
  g_return_if_fail (GB_IS_HTML_VIEW (self));
  g_return_if_fail (GB_IS_DOCUMENT (document));

  if (!GB_IS_HTML_DOCUMENT (document))
    {
      g_warning ("GbHtmlView does not know how to handle a document "
                 "of type %s",
                 g_type_name (G_TYPE_FROM_INSTANCE (document)));
      return;
    }

  if (document != (GbDocument *)self->document)
    {
      if (self->document)
        {
          gb_html_view_disconnect (self, self->document);
          g_clear_object (&self->document);
        }

      if (document)
        {
          self->document = g_object_ref (document);
          gb_html_view_connect (self, self->document);
        }

      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_DOCUMENT]);
    }
}

static void
gb_html_view_refresh (GSimpleAction *action,
                      GVariant      *parameters,
                      gpointer       user_data)
{
  GtkTextBuffer *buffer;
  GbHtmlView *self = user_data;

  g_return_if_fail (GB_IS_HTML_VIEW (self));

  if (!self->document)
    return;

  buffer = gb_html_document_get_buffer (self->document);
  if (!buffer)
    return;

  gb_html_view_changed (self, buffer);
}

static void
gb_html_view_finalize (GObject *object)
{
  GbHtmlView *self = (GbHtmlView *)object;

  g_clear_object (&self->document);

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
                          gb_html_view_get_document (GB_VIEW (self)));
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
  GbViewClass *view_class = GB_VIEW_CLASS (klass);

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

  GB_WIDGET_CLASS_TEMPLATE (klass, "gb-html-view.ui");
  GB_WIDGET_CLASS_BIND (klass, GbHtmlView, web_view);

  g_type_ensure (WEBKIT_TYPE_WEB_VIEW);
}

static void
gb_html_view_init (GbHtmlView *self)
{
  static const GActionEntry entries[] = {
    { "refresh", gb_html_view_refresh },
  };
  GSimpleActionGroup *actions;
  GtkWidget *controls;

  gtk_widget_init_template (GTK_WIDGET (self));

  controls = gb_view_get_controls (GB_VIEW (self));

  actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (actions), entries,
                                   G_N_ELEMENTS (entries), self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "html-view",
                                  G_ACTION_GROUP (actions));
  gtk_widget_insert_action_group (controls, "html-view",
                                  G_ACTION_GROUP (actions));
  g_object_unref (actions);
}
