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

G_DEFINE_INTERFACE (GbDocument, gb_document, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_CAN_SAVE,
  PROP_TITLE,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

gboolean
gb_document_get_can_save (GbDocument *document)
{
  g_return_val_if_fail (GB_IS_DOCUMENT (document), FALSE);

  return GB_DOCUMENT_GET_INTERFACE (document)->get_can_save (document);
}

const gchar *
gb_document_get_title (GbDocument *document)
{
  g_return_val_if_fail (GB_IS_DOCUMENT (document), NULL);

  return GB_DOCUMENT_GET_INTERFACE (document)->get_title (document);
}

static void
gb_document_default_init (GbDocumentInterface *iface)
{
  gParamSpecs [PROP_CAN_SAVE] =
    g_param_spec_boolean ("can-save",
                          _("Can Save"),
                          _("If the document can be saved."),
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_interface_install_property (iface, gParamSpecs [PROP_CAN_SAVE]);

  gParamSpecs [PROP_TITLE] =
    g_param_spec_string ("title",
                         _("Title"),
                         _("The title of the document."),
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_interface_install_property (iface, gParamSpecs [PROP_TITLE]);
}
