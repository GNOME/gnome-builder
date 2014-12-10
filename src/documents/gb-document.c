/* gb-document.c
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

#include <glib/gi18n.h>

#include "gb-document.h"
#include "gb-document-view.h"

G_DEFINE_INTERFACE (GbDocument, gb_document, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_MODIFIED,
  PROP_TITLE,
  LAST_PROP
};

enum {
  CREATE_VIEW,
  LAST_SIGNAL
};

static GParamSpec *gParamSpecs [LAST_PROP];
static guint       gSignals [LAST_SIGNAL];

gboolean
gb_document_get_modified (GbDocument *document)
{
  g_return_val_if_fail (GB_IS_DOCUMENT (document), FALSE);

  return GB_DOCUMENT_GET_INTERFACE (document)->get_modified (document);
}

const gchar *
gb_document_get_title (GbDocument *document)
{
  g_return_val_if_fail (GB_IS_DOCUMENT (document), NULL);

  return GB_DOCUMENT_GET_INTERFACE (document)->get_title (document);
}

GtkWidget *
gb_document_create_view (GbDocument *document)
{
  GtkWidget *ret = NULL;

  g_return_val_if_fail (GB_IS_DOCUMENT (document), NULL);

  g_signal_emit (document, gSignals [CREATE_VIEW], 0, &ret);

  if (!ret)
    g_warning ("%s failed to implement create_view() signal",
               g_type_name (G_TYPE_FROM_INSTANCE (document)));

  return ret;
}

void
gb_document_save (GbDocument *document)
{
  g_return_if_fail (GB_IS_DOCUMENT (document));

  if (GB_DOCUMENT_GET_INTERFACE (document)->save)
    GB_DOCUMENT_GET_INTERFACE (document)->save (document);
}

void
gb_document_save_as (GbDocument *document)
{
  g_return_if_fail (GB_IS_DOCUMENT (document));

  if (GB_DOCUMENT_GET_INTERFACE (document)->save_as)
    GB_DOCUMENT_GET_INTERFACE (document)->save_as (document);
}

static void
gb_document_default_init (GbDocumentInterface *iface)
{
  gParamSpecs [PROP_MODIFIED] =
    g_param_spec_boolean ("modified",
                          _("Modified"),
                          _("If the document has been modified from disk."),
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_interface_install_property (iface, gParamSpecs [PROP_MODIFIED]);

  gParamSpecs [PROP_TITLE] =
    g_param_spec_string ("title",
                         _("Title"),
                         _("The title of the document."),
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_interface_install_property (iface, gParamSpecs [PROP_TITLE]);

  gSignals [CREATE_VIEW] =
    g_signal_new ("create-view",
                  GB_TYPE_DOCUMENT,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GbDocumentInterface, create_view),
                  g_signal_accumulator_first_wins,
                  NULL,
                  g_cclosure_marshal_generic,
                  GB_TYPE_DOCUMENT_VIEW,
                  0);
}
