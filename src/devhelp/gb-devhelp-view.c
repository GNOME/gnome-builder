/* gb-devhelp-view.c
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

#define G_LOG_DOMAIN "devhelp-view"

#include <glib/gi18n.h>
#include <webkit2/webkit2.h>

#include "gb-devhelp-view.h"

struct _GbDevhelpViewPrivate
{
  /* References owned by view */
  GbDevhelpDocument *document;

  /* References owned by Gtk template */
  WebKitWebView *web_view;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbDevhelpView, gb_devhelp_view,
                            GB_TYPE_DOCUMENT_VIEW)

enum {
  PROP_0,
  PROP_DOCUMENT,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

GbDocumentView *
gb_devhelp_view_new (GbDevhelpDocument *document)
{
  return g_object_new (GB_TYPE_DEVHELP_VIEW,
                       "document", document,
                       NULL);
}

static GbDocument *
gb_devhelp_view_get_document (GbDocumentView *view)
{
  g_return_val_if_fail (GB_IS_DEVHELP_VIEW (view), NULL);

  return GB_DOCUMENT (GB_DEVHELP_VIEW (view)->priv->document);
}

static void
gb_devhelp_view_set_document (GbDevhelpView     *view,
                              GbDevhelpDocument *document)
{
  g_return_if_fail (GB_IS_DEVHELP_VIEW (view));
}

static void
gb_devhelp_view_finalize (GObject *object)
{
  GbDevhelpViewPrivate *priv = GB_DEVHELP_VIEW (object)->priv;

  g_clear_object (&priv->document);

  G_OBJECT_CLASS (gb_devhelp_view_parent_class)->finalize (object);
}

static void
gb_devhelp_view_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GbDevhelpView *self = GB_DEVHELP_VIEW (object);

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
gb_devhelp_view_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GbDevhelpView *self = GB_DEVHELP_VIEW (object);

  switch (prop_id)
    {
    case PROP_DOCUMENT:
      gb_devhelp_view_set_document (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_devhelp_view_class_init (GbDevhelpViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GbDocumentViewClass *view_class = GB_DOCUMENT_VIEW_CLASS (klass);

  object_class->finalize = gb_devhelp_view_finalize;
  object_class->get_property = gb_devhelp_view_get_property;
  object_class->set_property = gb_devhelp_view_set_property;

  view_class->get_document = gb_devhelp_view_get_document;

  gParamSpecs [PROP_DOCUMENT] =
    g_param_spec_object ("document",
                         _("Document"),
                         _("The document for the devhelp view."),
                         GB_TYPE_DEVHELP_DOCUMENT,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_DOCUMENT,
                                   gParamSpecs [PROP_DOCUMENT]);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/builder/ui/gb-devhelp-view.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GbDevhelpView, web_view);

  g_type_ensure (WEBKIT_TYPE_WEB_VIEW);
}

static void
gb_devhelp_view_init (GbDevhelpView *self)
{
  self->priv = gb_devhelp_view_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));
}
