/* gb-devhelp-document.c
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

#define G_LOG_DOMAIN "devhelp-document"

#include <devhelp/devhelp.h>
#include <glib/gi18n.h>

#include "gb-devhelp-document.h"
#include "gb-devhelp-view.h"

struct _GbDevhelpDocumentPrivate
{
  DhBookManager *book_manager;
  DhKeywordModel *model;
  gchar *title;
  gchar *uri;
};

static void gb_document_init (GbDocumentInterface *iface);

G_DEFINE_TYPE_EXTENDED (GbDevhelpDocument,
                        gb_devhelp_document,
                        G_TYPE_OBJECT,
                        0,
                        G_ADD_PRIVATE (GbDevhelpDocument)
                        G_IMPLEMENT_INTERFACE (GB_TYPE_DOCUMENT,
                                               gb_document_init))

enum {
  PROP_0,
  PROP_MODIFIED,
  PROP_TITLE,
  PROP_URI,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

GbDevhelpDocument *
gb_devhelp_document_new (void)
{
  return g_object_new (GB_TYPE_DEVHELP_DOCUMENT, NULL);
}

static void
gb_devhelp_document_set_title (GbDevhelpDocument *document,
                               const gchar       *title)
{
  g_return_if_fail (GB_IS_DEVHELP_DOCUMENT (document));

  if (document->priv->title != title)
    {
      g_clear_pointer (&document->priv->title, g_free);
      document->priv->title = g_strdup_printf (_("Documentation (%s)"), title);
      g_object_notify (G_OBJECT (document), "title");
    }
}

const gchar *
gb_devhelp_document_get_uri (GbDevhelpDocument *document)
{
  g_return_val_if_fail (GB_IS_DEVHELP_DOCUMENT (document), NULL);

  return document->priv->uri;
}

static void
gb_devhelp_document_set_uri (GbDevhelpDocument *document,
                             const gchar       *uri)
{
  g_return_if_fail (GB_IS_DEVHELP_DOCUMENT (document));
  g_return_if_fail (uri);

  if (document->priv->uri != uri)
    {
      g_clear_pointer (&document->priv->uri, g_free);
      document->priv->uri = g_strdup (uri);
      g_object_notify_by_pspec (G_OBJECT (document), gParamSpecs [PROP_URI]);
    }
}

void
gb_devhelp_document_set_search (GbDevhelpDocument *document,
                                const gchar       *search)
{
  GbDevhelpDocumentPrivate *priv;
  GtkTreeIter iter;

  g_return_if_fail (GB_IS_DEVHELP_DOCUMENT (document));

  priv = document->priv;

  /* TODO: Filter books/language based on project? */
  dh_keyword_model_filter (priv->model, search, NULL, NULL);

  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->model), &iter))
    {
      DhLink *link = NULL;
      gchar *name = NULL;

      /*
       * NOTE:
       *
       * Note that the DH_KEYWORD_MODEL_COL_LINK is specified as a
       * G_TYPE_POINTER so dh_link_unref() does not need to be called
       * on the resulting structure.
       */
      gtk_tree_model_get (GTK_TREE_MODEL (priv->model), &iter,
                          DH_KEYWORD_MODEL_COL_NAME, &name,
                          DH_KEYWORD_MODEL_COL_LINK, &link,
                          -1);

      if (name && link)
        {
          g_debug ("Name=\"%s\" Uri=\"%s\"", name, dh_link_get_uri (link));
          gb_devhelp_document_set_title (document, name);
          gb_devhelp_document_set_uri (document, dh_link_get_uri (link));
        }

      g_clear_pointer (&name, g_free);
    }
}

const gchar *
gb_devhelp_document_get_title (GbDocument *document)
{
  GbDevhelpDocument *self = (GbDevhelpDocument *)document;

  g_return_val_if_fail (GB_IS_DEVHELP_DOCUMENT (self), NULL);

  if (self->priv->title)
    return self->priv->title;

  return _("Documentation");
}

gboolean
gb_devhelp_document_get_modified (GbDocument *document)
{
  g_return_val_if_fail (GB_IS_DEVHELP_DOCUMENT (document), FALSE);

  return FALSE;
}

static GtkWidget *
gb_devhelp_document_create_view (GbDocument *document)
{
  g_return_val_if_fail (GB_IS_DEVHELP_DOCUMENT (document), NULL);

  return g_object_new (GB_TYPE_DEVHELP_VIEW,
                       "document", document,
                       "visible", TRUE,
                       NULL);
}

static void
gb_devhelp_document_constructed (GObject *object)
{
  GbDevhelpDocumentPrivate *priv = GB_DEVHELP_DOCUMENT (object)->priv;

  dh_book_manager_populate (priv->book_manager);
  dh_keyword_model_set_words (priv->model, priv->book_manager);

  G_OBJECT_CLASS (gb_devhelp_document_parent_class)->constructed (object);
}

static void
gb_devhelp_document_finalize (GObject *object)
{
  GbDevhelpDocumentPrivate *priv = GB_DEVHELP_DOCUMENT (object)->priv;

  g_clear_pointer (&priv->title, g_free);
  g_clear_object (&priv->book_manager);

  G_OBJECT_CLASS (gb_devhelp_document_parent_class)->finalize (object);
}

static void
gb_devhelp_document_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GbDevhelpDocument *self = GB_DEVHELP_DOCUMENT (object);

  switch (prop_id)
    {
    case PROP_MODIFIED:
      g_value_set_boolean (value,
                           gb_devhelp_document_get_modified (GB_DOCUMENT (self)));
      break;

    case PROP_TITLE:
      g_value_set_string (value,
                          gb_devhelp_document_get_title (GB_DOCUMENT (self)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_devhelp_document_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
#if 0
  GbDevhelpDocument *self = GB_DEVHELP_DOCUMENT (object);
#endif

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_devhelp_document_class_init (GbDevhelpDocumentClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gb_devhelp_document_constructed;
  object_class->finalize = gb_devhelp_document_finalize;
  object_class->get_property = gb_devhelp_document_get_property;
  object_class->set_property = gb_devhelp_document_set_property;

  g_object_class_override_property (object_class, PROP_TITLE, "title");
  g_object_class_override_property (object_class, PROP_MODIFIED, "modified");

  gParamSpecs [PROP_URI] =
    g_param_spec_string ("uri",
                         _("URI"),
                         _("The uri to load."),
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_URI,
                                   gParamSpecs [PROP_URI]);
}

static void
gb_devhelp_document_init (GbDevhelpDocument *self)
{
  self->priv = gb_devhelp_document_get_instance_private (self);
  self->priv->book_manager = dh_book_manager_new ();
  self->priv->model = dh_keyword_model_new ();
}

static void
gb_document_init (GbDocumentInterface *iface)
{
  iface->get_title = gb_devhelp_document_get_title;
  iface->get_modified = gb_devhelp_document_get_modified;
  iface->create_view = gb_devhelp_document_create_view;
}
