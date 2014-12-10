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

#include <glib/gi18n.h>

#include "gb-devhelp-document.h"

struct _GbDevhelpDocumentPrivate
{
  gchar *title;
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
  LAST_PROP
};

GbDevhelpDocument *
gb_devhelp_document_new (void)
{
  return g_object_new (GB_TYPE_DEVHELP_DOCUMENT, NULL);
}

void
gb_devhelp_document_set_search (GbDevhelpDocument *document,
                                const gchar       *search)
{
  g_return_if_fail (GB_IS_DEVHELP_DOCUMENT (document));
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

static void
gb_devhelp_document_finalize (GObject *object)
{
  GbDevhelpDocumentPrivate *priv = GB_DEVHELP_DOCUMENT (object)->priv;

  g_clear_pointer (&priv->title, g_free);

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

  object_class->finalize = gb_devhelp_document_finalize;
  object_class->get_property = gb_devhelp_document_get_property;
  object_class->set_property = gb_devhelp_document_set_property;

  g_object_class_override_property (object_class, PROP_TITLE, "title");
  g_object_class_override_property (object_class, PROP_MODIFIED, "modified");
}

static void
gb_devhelp_document_init (GbDevhelpDocument *self)
{
  self->priv = gb_devhelp_document_get_instance_private (self);
}

static void
gb_document_init (GbDocumentInterface *iface)
{
  iface->get_title = gb_devhelp_document_get_title;
}
