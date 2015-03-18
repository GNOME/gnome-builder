/* gb-editor-document.c
 *
 * Copyright (C) 2014-2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "gb-editor-document"

#include <glib/gi18n.h>
#include <ide.h>

#include "gb-document.h"
#include "gb-editor-document.h"
#include "gb-editor-view.h"

struct _GbEditorDocument
{
  IdeBuffer parent_instance;
};

enum {
  PROP_0,
  PROP_MODIFIED,
  PROP_READ_ONLY,
  LAST_PROP
};

enum {
  LAST_SIGNAL
};

static void document_interface_init (GbDocumentInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GbEditorDocument, gb_editor_document, IDE_TYPE_BUFFER,
                         G_IMPLEMENT_INTERFACE (GB_TYPE_DOCUMENT,
                                                document_interface_init))

static GtkWidget *
gb_editor_document_create_view (GbDocument *self)
{
  GtkWidget *ret;

  IDE_ENTRY;

  g_assert (GB_IS_EDITOR_DOCUMENT (self));

  ret = g_object_new (GB_TYPE_EDITOR_VIEW,
                      "document", self,
                      "visible", TRUE,
                      NULL);

  IDE_RETURN (ret);
}

static gboolean
gb_editor_document_get_modified (GbDocument *document)
{
  return gtk_text_buffer_get_modified (GTK_TEXT_BUFFER (document));
}

static gboolean
gb_editor_document_get_read_only (GbDocument *document)
{
  return FALSE;
}

static const gchar *
gb_editor_document_get_title (GbDocument *document)
{
  return ide_buffer_get_title (IDE_BUFFER (document));
}

static void
gb_editor_document_constructed (GObject *object)
{
  IDE_ENTRY;
  G_OBJECT_CLASS (gb_editor_document_parent_class)->constructed (object);
  IDE_EXIT;
}

static void
gb_editor_document_dispose (GObject *object)
{
  IDE_ENTRY;
  G_OBJECT_CLASS (gb_editor_document_parent_class)->dispose (object);
  IDE_EXIT;
}

static void
gb_editor_document_finalize (GObject *object)
{
  IDE_ENTRY;
  G_OBJECT_CLASS(gb_editor_document_parent_class)->finalize (object);
  IDE_EXIT;
}

static void
gb_editor_document_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GbEditorDocument *self = (GbEditorDocument *)object;

  switch (prop_id)
    {
    case PROP_READ_ONLY:
      g_value_set_boolean (value, gb_editor_document_get_read_only (GB_DOCUMENT (self)));
      break;

    case PROP_MODIFIED:
      g_value_set_boolean (value, gb_editor_document_get_modified (GB_DOCUMENT (self)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
gb_editor_document_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  //GbEditorDocument *self = (GbEditorDocument *)object;

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gb_editor_document_class_init (GbEditorDocumentClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gb_editor_document_constructed;
  object_class->dispose = gb_editor_document_dispose;
  object_class->finalize = gb_editor_document_finalize;
  object_class->get_property = gb_editor_document_get_property;
  object_class->set_property = gb_editor_document_set_property;

  g_object_class_override_property (object_class, PROP_READ_ONLY, "read-only");
  g_object_class_override_property (object_class, PROP_MODIFIED, "modified");
}

static void
gb_editor_document_init (GbEditorDocument *document)
{
}

static void
document_interface_init (GbDocumentInterface *iface)
{
  iface->create_view = gb_editor_document_create_view;
  iface->get_modified = gb_editor_document_get_modified;
  iface->get_read_only = gb_editor_document_get_read_only;
  iface->get_title = gb_editor_document_get_title;
}
