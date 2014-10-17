/* gb-source-code-assistant.c
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

#include "gb-log.h"
#include "gb-source-code-assistant.h"
#include "gca-service.h"

#define PARSE_TIMEOUT 500

struct _GbSourceCodeAssistantPrivate
{
  GbSourceView    *source_view;
  GcaServiceProxy *proxy;
  GCancellable    *cancellable;
  guint            parse_timeout;
  gulong           changed_handler;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbSourceCodeAssistant,
                            gb_source_code_assistant,
                            G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_SOURCE_VIEW,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

GbSourceCodeAssistant *
gb_source_code_assistant_new (GbSourceView *source_view)
{
  return g_object_new (GB_TYPE_SOURCE_CODE_ASSISTANT,
                       "source-view", source_view,
                       NULL);
}

static GcaService *
gb_source_code_assistant_get_proxy (GbSourceCodeAssistant *assistant)
{
  return NULL;
}

static void
gb_source_code_assistant_parse_cb (GObject      *source_object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  GbSourceCodeAssistant *assistant = user_data;
  GcaService *proxy = (GcaService *)source_object;
  GError *error = NULL;
  gchar *result_path = NULL;

  ENTRY;

  g_return_if_fail (GB_IS_SOURCE_CODE_ASSISTANT (assistant));
  g_return_if_fail (GCA_IS_SERVICE (proxy));

  if (!gca_service_call_parse_finish (proxy, &result_path, result, &error))
    {
      g_message ("%s", error->message);
      g_clear_error (&error);
    }

  g_print ("Fetch document info from path: %s\n", result_path);

  g_free (result_path);
  g_object_unref (proxy);

  EXIT;
}

static gboolean
gb_source_code_assistant_do_parse (gpointer data)
{
  GbSourceCodeAssistantPrivate *priv;
  GbSourceCodeAssistant *assistant = data;
  GcaService *proxy;
  gchar *file_path = NULL;
  gchar *data_path = NULL;

  ENTRY;

  g_return_val_if_fail (GB_IS_SOURCE_CODE_ASSISTANT (assistant), FALSE);

  priv = assistant->priv;

  priv->parse_timeout = 0;

  proxy = gb_source_code_assistant_get_proxy (assistant);
  if (!proxy)
    GOTO (failure);

  gca_service_call_parse (proxy,
                          file_path,
                          data_path,
                          NULL,
                          NULL,
                          priv->cancellable,
                          gb_source_code_assistant_parse_cb,
                          g_object_ref (assistant));

failure:
  RETURN (G_SOURCE_REMOVE);
}
static void
gb_source_code_assistant_queue_parse (GbSourceCodeAssistant *assistant)
{
  g_return_if_fail (GB_IS_SOURCE_CODE_ASSISTANT (assistant));

  if (assistant->priv->parse_timeout)
    g_source_remove (assistant->priv->parse_timeout);

  assistant->priv->parse_timeout =
    g_timeout_add (PARSE_TIMEOUT,
                   gb_source_code_assistant_do_parse,
                   assistant);
}

static void
gb_source_code_assistant_on_buffer_changed (GtkTextBuffer         *buffer,
                                            GbSourceCodeAssistant *assistant)
{
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (GB_IS_SOURCE_CODE_ASSISTANT (assistant));

  gb_source_code_assistant_queue_parse (assistant);
}

static void
gb_source_code_assistant_connect (GbSourceCodeAssistant *assistant)
{
  GbSourceCodeAssistantPrivate *priv;
  GtkTextBuffer *buffer;

  g_return_if_fail (GB_IS_SOURCE_CODE_ASSISTANT (assistant));

  priv = assistant->priv;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->source_view));

  priv->changed_handler =
    g_signal_connect (buffer, "changed",
                      G_CALLBACK (gb_source_code_assistant_on_buffer_changed),
                      assistant);
}

static void
gb_source_code_assistant_disconnect (GbSourceCodeAssistant *assistant)
{
  GbSourceCodeAssistantPrivate *priv;
  GtkTextBuffer *buffer;

  g_return_if_fail (GB_IS_SOURCE_CODE_ASSISTANT (assistant));

  priv = assistant->priv;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->source_view));

  if (priv->changed_handler)
    {
      g_signal_handler_disconnect (buffer, priv->changed_handler);
      priv->changed_handler = 0;
    }

  if (priv->parse_timeout)
    {
      g_source_remove (priv->parse_timeout);
      priv->parse_timeout = 0;
    }
}

static void
gb_source_code_assistant_set_source_view (GbSourceCodeAssistant *assistant,
                                          GbSourceView          *source_view)
{
  GbSourceCodeAssistantPrivate *priv;

  g_return_if_fail (GB_IS_SOURCE_CODE_ASSISTANT (assistant));
  g_return_if_fail (!source_view || GB_IS_SOURCE_VIEW (source_view));

  priv = assistant->priv;

  if (source_view != priv->source_view)
    {
      if (priv->source_view)
        {
          gb_source_code_assistant_disconnect (assistant);
          g_object_remove_weak_pointer (G_OBJECT (priv->source_view),
                                        (gpointer *)&priv->source_view);
          priv->source_view = NULL;
        }

      if (source_view)
        {
          priv->source_view = source_view;
          g_object_add_weak_pointer (G_OBJECT (priv->source_view),
                                     (gpointer *)&priv->source_view);
          gb_source_code_assistant_connect (assistant);
        }
    }
}

static void
gb_source_code_assistant_dispose (GObject *object)
{
  GbSourceCodeAssistant *assistant = (GbSourceCodeAssistant *)object;

  gb_source_code_assistant_set_source_view (assistant, NULL);
  g_cancellable_cancel (assistant->priv->cancellable);
}

static void
gb_source_code_assistant_finalize (GObject *object)
{
  GbSourceCodeAssistant *assistant = (GbSourceCodeAssistant *)object;

  g_clear_object (&assistant->priv->cancellable);
  g_clear_object (&assistant->priv->proxy);
}

static void
gb_source_code_assistant_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  GbSourceCodeAssistant *self = GB_SOURCE_CODE_ASSISTANT (object);

  switch (prop_id)
    {
    case PROP_SOURCE_VIEW:
      gb_source_code_assistant_set_source_view (self,
                                                g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_source_code_assistant_class_init (GbSourceCodeAssistantClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gb_source_code_assistant_dispose;
  object_class->finalize = gb_source_code_assistant_finalize;
  object_class->set_property = gb_source_code_assistant_set_property;

  gParamSpecs [PROP_SOURCE_VIEW] =
    g_param_spec_object ("source-view",
                         _("Source View"),
                         _("The source view to attach to."),
                         GB_TYPE_SOURCE_VIEW,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SOURCE_VIEW,
                                   gParamSpecs [PROP_SOURCE_VIEW]);
}

static void
gb_source_code_assistant_init (GbSourceCodeAssistant *self)
{
  self->priv = gb_source_code_assistant_get_instance_private (self);
  self->priv->cancellable = g_cancellable_new ();
}
