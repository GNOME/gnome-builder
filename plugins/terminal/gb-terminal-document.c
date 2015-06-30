/* gb-terminal-document.c
 *
 * Copyright (C) 2015 Sebastien Lafargue <slafargue@gnome.org>
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

#define G_LOG_DOMAIN "terminal-document"

#include <glib/gi18n.h>

#include "gb-terminal-document.h"
#include "gb-terminal-view.h"

struct _GbTerminalDocument
{
  GObjectClass   parent_instance;

  gchar         *title;
};

static void gb_document_init (GbDocumentInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GbTerminalDocument, gb_terminal_document, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GB_TYPE_DOCUMENT,
                                                gb_document_init))

enum {
  PROP_0,
  LAST_PROP,

  /* These are overridden */
  PROP_MODIFIED,
  PROP_READ_ONLY,
  PROP_TITLE
};

GbTerminalDocument *
gb_terminal_document_new (void)
{
  return g_object_new (GB_TYPE_TERMINAL_DOCUMENT, NULL);
}

void
gb_terminal_document_set_title (GbTerminalDocument *document,
                                const gchar        *title)
{
  g_return_if_fail (GB_IS_TERMINAL_DOCUMENT (document));

  if (document->title != title)
    {
      g_clear_pointer (&document->title, g_free);
      document->title = g_strdup_printf (_("Terminal (%s)"), title);
      g_object_notify (G_OBJECT (document), "title");
    }
}

gboolean
gb_terminal_document_get_modified (GbDocument *document)
{
  g_return_val_if_fail (GB_IS_TERMINAL_DOCUMENT (document), FALSE);

  return FALSE;
}

const gchar *
gb_terminal_document_get_title (GbDocument *document)
{
  GbTerminalDocument *self = (GbTerminalDocument *)document;

  g_return_val_if_fail (GB_IS_TERMINAL_DOCUMENT (self), NULL);

  if (self->title)
    return self->title;

  return _("Terminal");
}

static GtkWidget *
gb_terminal_document_create_view (GbDocument *document)
{
  g_return_val_if_fail (GB_IS_TERMINAL_DOCUMENT (document), NULL);

  return g_object_new (GB_TYPE_TERMINAL_VIEW,
                       "document", document,
                       "visible", TRUE,
                       NULL);
}

static void
gb_terminal_document_finalize (GObject *object)
{
  GbTerminalDocument *self = GB_TERMINAL_DOCUMENT (object);

  g_clear_pointer (&self->title, g_free);

  G_OBJECT_CLASS (gb_terminal_document_parent_class)->finalize (object);
}

static void
gb_terminal_document_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GbTerminalDocument *self = GB_TERMINAL_DOCUMENT (object);

  switch (prop_id)
    {
      case PROP_MODIFIED:
      g_value_set_boolean (value, gb_terminal_document_get_modified (GB_DOCUMENT (self)));
      break;

    case PROP_READ_ONLY:
      g_value_set_boolean (value, TRUE);
      break;

    case PROP_TITLE:
      g_value_set_string (value, gb_terminal_document_get_title (GB_DOCUMENT (self)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_terminal_document_class_init (GbTerminalDocumentClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gb_terminal_document_finalize;
  object_class->get_property = gb_terminal_document_get_property;

  g_object_class_override_property (object_class, PROP_MODIFIED, "modified");
  g_object_class_override_property (object_class, PROP_READ_ONLY, "read-only");
  g_object_class_override_property (object_class, PROP_TITLE, "title");
}

static void
gb_terminal_document_init (GbTerminalDocument *self)
{
}

static void
gb_document_init (GbDocumentInterface *iface)
{
  iface->get_title = gb_terminal_document_get_title;
  iface->create_view = gb_terminal_document_create_view;
}
