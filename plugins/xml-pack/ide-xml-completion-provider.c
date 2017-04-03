/* ide-xml-completion-provider.c
 *
 * Copyright (C) 2017 Sebastien Lafargue <slafargue@gnome.org>
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

#define G_LOG_DOMAIN "xml-completion"

#include <libpeas/peas.h>

#include "ide-xml-completion-provider.h"
#include "ide-xml-position.h"
#include "ide-xml-service.h"

struct _IdeXmlCompletionProvider
{
  IdeObject parent_instance;
};

static void completion_provider_init (GtkSourceCompletionProviderIface *);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (IdeXmlCompletionProvider,
                                ide_xml_completion_provider,
                                IDE_TYPE_OBJECT,
                                0,
                                G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROVIDER, completion_provider_init)
                                G_IMPLEMENT_INTERFACE (IDE_TYPE_COMPLETION_PROVIDER, NULL))

enum {
  PROP_0,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

typedef struct
{
  IdeXmlCompletionProvider    *self;
  GtkSourceCompletionContext  *completion_context;
  IdeFile                     *ifile;
  IdeBuffer                   *buffer;
  gint                         line;
  gint                         line_offset;
} PopulateState;

static void
populate_state_free (PopulateState *state)
{
  g_assert (state != NULL);

  g_object_unref (state->self);
  g_object_unref (state->completion_context);
  g_object_unref (state->ifile);
  g_object_unref (state->buffer);
}

static void
populate_cb (GObject      *object,
             GAsyncResult *result,
             gpointer      user_data)
{
  IdeXmlService *service = (IdeXmlService *)object;
  PopulateState *state = (PopulateState *)user_data;
  IdeXmlCompletionProvider *self = state->self;
  g_autoptr (IdeXmlPosition) position = NULL;
  GtkSourceCompletionItem *item;
  g_autofree gchar *text = NULL;
  g_autofree gchar *label = NULL;
  g_autoptr (GList) results = NULL;
  GError *error = NULL;

  g_assert (IDE_IS_XML_COMPLETION_PROVIDER (self));
  g_assert (IDE_IS_XML_SERVICE (service));

  position = ide_xml_service_get_position_from_cursor_finish (service, result, &error);
  text = g_strdup ("xml item text");
  label = g_strdup ("xml item label");
  item = g_object_new (GTK_SOURCE_TYPE_COMPLETION_ITEM,
                       "text", text,
                       "label", label,
                       NULL);

  /* TODO: show position content for debug */

  results = g_list_prepend (results, item);
  gtk_source_completion_context_add_proposals (state->completion_context,
                                               GTK_SOURCE_COMPLETION_PROVIDER (self),
                                               results,
                                               TRUE);

  populate_state_free (state);
}

static void
ide_xml_completion_provider_populate (GtkSourceCompletionProvider *self,
                                      GtkSourceCompletionContext  *completion_context)
{
  IdeContext *ide_context;
  IdeXmlService *service;
  GtkTextIter iter;
  GCancellable *cancellable;
  GtkTextBuffer *buffer;
  PopulateState *state;

  g_assert (IDE_IS_XML_COMPLETION_PROVIDER (self));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (completion_context));

  ide_context = ide_object_get_context (IDE_OBJECT (self));
  service = ide_context_get_service_typed (ide_context, IDE_TYPE_XML_SERVICE);

  gtk_source_completion_context_get_iter (completion_context, &iter);

  cancellable = g_cancellable_new ();
  state = g_slice_new0 (PopulateState);

  state->self = g_object_ref (self);
  state->completion_context = g_object_ref (completion_context);
  state->buffer = g_object_ref (gtk_text_iter_get_buffer (&iter));
  state->ifile = g_object_ref (ide_buffer_get_file (IDE_BUFFER (buffer)));
  state->line = gtk_text_iter_get_line (&iter);
  state->line_offset = gtk_text_iter_get_line_offset (&iter);

  printf ("path:%s at (%i,%i)\n",
          g_file_get_path (ide_file_get_file (state->ifile)),
          state->line,
          state->line_offset);

  ide_xml_service_get_position_from_cursor_async (service,
                                                  state->ifile,
                                                  IDE_BUFFER (buffer),
                                                  state->line,
                                                  state->line_offset,
                                                  cancellable,
                                                  populate_cb,
                                                  state);
}

static GdkPixbuf *
ide_xml_completion_provider_get_icon (GtkSourceCompletionProvider *provider)
{
  return NULL;
}

IdeXmlCompletionProvider *
ide_xml_completion_provider_new (void)
{
  return g_object_new (IDE_TYPE_XML_COMPLETION_PROVIDER, NULL);
}

static void
ide_xml_completion_provider_finalize (GObject *object)
{
  IdeXmlCompletionProvider *self = (IdeXmlCompletionProvider *)object;

  G_OBJECT_CLASS (ide_xml_completion_provider_parent_class)->finalize (object);
}

static void
ide_xml_completion_provider_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  IdeXmlCompletionProvider *self = IDE_XML_COMPLETION_PROVIDER (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_xml_completion_provider_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  IdeXmlCompletionProvider *self = IDE_XML_COMPLETION_PROVIDER (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_xml_completion_provider_class_finalize (IdeXmlCompletionProviderClass *klass)
{
}

static void
ide_xml_completion_provider_class_init (IdeXmlCompletionProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_xml_completion_provider_finalize;
  object_class->get_property = ide_xml_completion_provider_get_property;
  object_class->set_property = ide_xml_completion_provider_set_property;
}

static void
ide_xml_completion_provider_init (IdeXmlCompletionProvider *self)
{
  printf ("xml completion provider init\n");
}

static void
completion_provider_init (GtkSourceCompletionProviderIface *iface)
{
  iface->get_icon = ide_xml_completion_provider_get_icon;
  iface->populate = ide_xml_completion_provider_populate;
}

void
_ide_xml_completion_provider_register_type (GTypeModule *module)
{
  ide_xml_completion_provider_register_type (module);
}
