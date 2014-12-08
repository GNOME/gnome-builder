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

#define G_LOG_DOMAIN "code-assistant"

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtksourceview/gtksource.h>

#include "gb-editor-document.h"
#include "gb-log.h"
#include "gb-source-code-assistant.h"
#include "gb-string.h"
#include "gca-diagnostics.h"
#include "gca-service.h"
#include "gca-structs.h"

struct _GbSourceCodeAssistantPrivate
{
  GtkTextBuffer  *buffer;
  GcaService     *proxy;
  GcaDiagnostics *document_proxy;
  GArray         *diagnostics;
  gchar          *document_path;
  GCancellable   *cancellable;

  gchar          *tmpfile_path;
  int             tmpfile_fd;

  gulong          changed_handler;
  gulong          notify_language_handler;

  guint           parse_timeout;
  guint           active;
};

enum {
  PROP_0,
  PROP_ACTIVE,
  PROP_BUFFER,
  LAST_PROP
};

enum {
  CHANGED,
  LAST_SIGNAL
};

G_DEFINE_TYPE_WITH_PRIVATE (GbSourceCodeAssistant,
                            gb_source_code_assistant,
                            G_TYPE_OBJECT)

static GParamSpec      *gParamSpecs [LAST_PROP];
static guint            gSignals [LAST_SIGNAL];
static GDBusConnection *gDBus;

#define PARSE_TIMEOUT_MSEC 350

static void
gb_source_code_assistant_queue_parse (GbSourceCodeAssistant *assistant);

GbSourceCodeAssistant *
gb_source_code_assistant_new (GtkTextBuffer *buffer)
{
  return g_object_new (GB_TYPE_SOURCE_CODE_ASSISTANT,
                       "buffer", buffer,
                       NULL);
}

static const gchar *
remap_language (const gchar *lang_id)
{
  g_return_val_if_fail (lang_id, NULL);

  if (g_str_equal (lang_id, "chdr") ||
      g_str_equal (lang_id, "objc") ||
      g_str_equal (lang_id, "cpp"))
    return "c";

  return lang_id;
}

static void
gb_source_code_assistant_inc_active (GbSourceCodeAssistant *assistant,
                                     gint                   amount)
{
  g_return_if_fail (GB_IS_SOURCE_CODE_ASSISTANT (assistant));

  assistant->priv->active += amount;
  g_object_notify_by_pspec (G_OBJECT (assistant), gParamSpecs [PROP_ACTIVE]);
}

static void
gb_source_code_assistant_proxy_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  GbSourceCodeAssistant *assistant = user_data;
  GcaService *proxy;
  GError *error = NULL;

  ENTRY;

  g_return_if_fail (GB_IS_SOURCE_CODE_ASSISTANT (assistant));

  gb_source_code_assistant_inc_active (assistant, -1);

  proxy = gca_service_proxy_new_finish (result, &error);

  if (!proxy)
    {
      g_message ("%s", error->message);
      g_clear_error (&error);
      EXIT;
    }

  g_clear_object (&assistant->priv->proxy);
  assistant->priv->proxy = proxy;

  gb_source_code_assistant_queue_parse (assistant);

  g_object_unref (assistant);

  EXIT;
}

static void
gb_source_code_assistant_load_service (GbSourceCodeAssistant *assistant)
{
  GbSourceCodeAssistantPrivate *priv;
  GtkSourceLanguage *language;
  GtkSourceBuffer *buffer;
  const gchar *lang_id;
  gchar *name;
  gchar *object_path;

  ENTRY;

  g_return_if_fail (GB_IS_SOURCE_CODE_ASSISTANT (assistant));
  g_return_if_fail (assistant->priv->buffer);

  priv = assistant->priv;

  if (!gDBus)
    EXIT;

  if (!GTK_SOURCE_IS_BUFFER (priv->buffer))
    EXIT;

  g_clear_object (&priv->proxy);

  buffer = GTK_SOURCE_BUFFER (priv->buffer);
  language = gtk_source_buffer_get_language (buffer);
  if (!language)
    EXIT;

  lang_id = remap_language (gtk_source_language_get_id (language));

  name = g_strdup_printf ("org.gnome.CodeAssist.v1.%s", lang_id);
  object_path = g_strdup_printf ("/org/gnome/CodeAssist/v1/%s", lang_id);

  gb_source_code_assistant_inc_active (assistant, 1);

  gca_service_proxy_new (gDBus,
                         G_DBUS_PROXY_FLAGS_NONE,
                         name,
                         object_path,
                         assistant->priv->cancellable,
                         gb_source_code_assistant_proxy_cb,
                         g_object_ref (assistant));

  g_free (name);
  g_free (object_path);

  EXIT;
}

/**
 * gb_source_code_assistant_get_diagnostics:
 * @assistant: (in): A #GbSourceCodeAssistant.
 *
 * Fetches the diagnostics for the buffer. Free the result with
 * g_array_unref().
 *
 * Returns: (transfer full): A #GArray of #GcaDiagnostic.
 */
GArray *
gb_source_code_assistant_get_diagnostics (GbSourceCodeAssistant *assistant)
{
  g_return_val_if_fail (GB_IS_SOURCE_CODE_ASSISTANT (assistant), NULL);

  if (assistant->priv->diagnostics)
    return g_array_ref (assistant->priv->diagnostics);

  return NULL;
}

static void
gb_source_code_assistant_diag_cb (GObject      *source_object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  GbSourceCodeAssistantPrivate *priv;
  GbSourceCodeAssistant *assistant = user_data;
  GcaDiagnostics *proxy = GCA_DIAGNOSTICS (source_object);
  GError *error = NULL;
  GVariant *diags = NULL;

  ENTRY;

  g_return_if_fail (GB_IS_SOURCE_CODE_ASSISTANT (assistant));

  priv = assistant->priv;

  gb_source_code_assistant_inc_active (assistant, -1);

  if (!gca_diagnostics_call_diagnostics_finish (proxy, &diags, result, &error))
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
      GOTO (failure);
    }

  g_clear_pointer (&priv->diagnostics, g_array_unref);

  priv->diagnostics = gca_diagnostics_from_variant (diags);

  /* TODO: update buffer text tags */

  g_signal_emit (assistant, gSignals [CHANGED], 0);

failure:
  g_object_unref (assistant);
  g_clear_pointer (&diags, g_variant_unref);

  EXIT;
}

static void
gb_source_code_assistant_diag_proxy_cb (GObject      *source_object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  GbSourceCodeAssistantPrivate *priv;
  GbSourceCodeAssistant *assistant = user_data;
  GcaDiagnostics *proxy;
  GError *error = NULL;

  ENTRY;

  g_return_if_fail (GB_IS_SOURCE_CODE_ASSISTANT (assistant));

  priv = assistant->priv;

  gb_source_code_assistant_inc_active (assistant, -1);

  if (!(proxy = gca_diagnostics_proxy_new_finish (result, &error)))
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
      GOTO (failure);
    }

  g_clear_object (&priv->document_proxy);
  priv->document_proxy = proxy;

  gb_source_code_assistant_inc_active (assistant, 1);
  gca_diagnostics_call_diagnostics (proxy,
                                    priv->cancellable,
                                    gb_source_code_assistant_diag_cb,
                                    g_object_ref (assistant));

failure:
  g_object_unref (assistant);

  EXIT;
}

static void
gb_source_code_assistant_parse_cb (GObject      *source_object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  GbSourceCodeAssistantPrivate *priv;
  GbSourceCodeAssistant *assistant = user_data;
  GcaService *service = GCA_SERVICE (source_object);
  GtkSourceLanguage *language;
  const gchar *lang_id;
  GError *error = NULL;
  gchar *name = NULL;
  gchar *document_path = NULL;

  ENTRY;

  g_return_if_fail (GB_IS_SOURCE_CODE_ASSISTANT (assistant));

  priv = assistant->priv;

  gb_source_code_assistant_inc_active (assistant, -1);

  if (!gca_service_call_parse_finish (service, &document_path, result, &error))
    {
      g_warning ("%s", error->message);
      GOTO (failure);
    }

  language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (priv->buffer));
  if (!language)
    GOTO (failure);

  lang_id = remap_language (gtk_source_language_get_id (language));
  name = g_strdup_printf ("org.gnome.CodeAssist.v1.%s", lang_id);

  if (priv->document_proxy)
    {
      const gchar *object_path;

      object_path = g_dbus_proxy_get_object_path (G_DBUS_PROXY (priv->document_proxy));
      if (g_strcmp0 (document_path, object_path) != 0)
        g_clear_object (&priv->document_proxy);
    }

  if (!priv->document_proxy)
    {
      gb_source_code_assistant_inc_active (assistant, 1);
      gca_diagnostics_proxy_new (gDBus,
                                 G_DBUS_PROXY_FLAGS_NONE,
                                 name,
                                 document_path,
                                 priv->cancellable,
                                 gb_source_code_assistant_diag_proxy_cb,
                                 g_object_ref (assistant));
    }
  else
    {
      gb_source_code_assistant_inc_active (assistant, 1);
      gca_diagnostics_call_diagnostics (priv->document_proxy,
                                        priv->cancellable,
                                        gb_source_code_assistant_diag_cb,
                                        g_object_ref (assistant));
    }

failure:
  g_clear_error (&error);
  g_free (document_path);
  g_free (name);
  g_object_unref (assistant);

  EXIT;
}

static gboolean
gb_source_code_assistant_do_parse (gpointer data)
{
  GbSourceCodeAssistantPrivate *priv;
  GbSourceCodeAssistant *assistant = data;
  GError *error = NULL;
  GtkTextMark *insert;
  GtkTextIter iter;
  GtkTextIter begin;
  GtkTextIter end;
  GVariant *cursor;
  GVariant *options;
  GFile *gfile = NULL;
  gchar *path = NULL;
  gchar *text = NULL;
  gint64 line;
  gint64 line_offset;

  ENTRY;

  g_return_val_if_fail (GB_IS_SOURCE_CODE_ASSISTANT (assistant), FALSE);

  priv = assistant->priv;

  priv->parse_timeout = 0;

  if (!priv->proxy)
    RETURN (G_SOURCE_REMOVE);

  insert = gtk_text_buffer_get_insert (priv->buffer);
  gtk_text_buffer_get_iter_at_mark (priv->buffer, &iter, insert);
  line = gtk_text_iter_get_line (&iter);
  line_offset = gtk_text_iter_get_line_offset (&iter);
  cursor = g_variant_new ("(xx)", line, line_offset);
  options = g_variant_new ("a{sv}", 0);

  if (GB_IS_EDITOR_DOCUMENT (priv->buffer))
    {
      GtkSourceFile *file;

      file = gb_editor_document_get_file (GB_EDITOR_DOCUMENT (priv->buffer));
      if (file)
        gfile = gtk_source_file_get_location (file);
    }

  if (gfile)
    path = g_file_get_path (gfile);

  if (gb_str_empty0 (path))
    RETURN (G_SOURCE_REMOVE);

  if (!priv->tmpfile_path)
    {
      int fd;

      fd = g_file_open_tmp ("builder-code-assistant.XXXXXX",
                            &priv->tmpfile_path,
                            &error);
      if (fd == -1)
        {
          g_warning ("%s", error->message);
          g_clear_error (&error);
          GOTO (failure);
        }

      priv->tmpfile_fd = fd;
    }

  gtk_text_buffer_get_bounds (priv->buffer, &begin, &end);
  text = gtk_text_buffer_get_text (priv->buffer, &begin, &end, TRUE);
  if (!g_file_set_contents (priv->tmpfile_path, text, -1, &error))
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
      GOTO (failure);
    }

  gb_source_code_assistant_inc_active (assistant, 1);
  gca_service_call_parse (priv->proxy,
                          path,
                          priv->tmpfile_path,
                          cursor,
                          options,
                          priv->cancellable,
                          gb_source_code_assistant_parse_cb,
                          g_object_ref (assistant));

failure:
  g_free (path);
  g_free (text);

  RETURN (G_SOURCE_REMOVE);
}

static void
gb_source_code_assistant_queue_parse (GbSourceCodeAssistant *assistant)
{
  g_return_if_fail (GB_IS_SOURCE_CODE_ASSISTANT (assistant));

  if (assistant->priv->parse_timeout)
    g_source_remove (assistant->priv->parse_timeout);

  assistant->priv->parse_timeout =
    g_timeout_add (PARSE_TIMEOUT_MSEC,
                   gb_source_code_assistant_do_parse,
                   assistant);
}

static void
gb_source_code_assistant_buffer_notify_language (GbSourceCodeAssistant *assistant,
                                                 GParamSpec            *pspec,
                                                 GtkSourceBuffer       *buffer)
{
  ENTRY;

  g_return_if_fail (GB_IS_SOURCE_CODE_ASSISTANT (assistant));
  g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));

  gb_source_code_assistant_load_service (assistant);

  EXIT;
}

static void
gb_source_code_assistant_buffer_changed (GbSourceCodeAssistant *assistant,
                                         GtkTextBuffer         *buffer)
{
  g_return_if_fail (GB_IS_SOURCE_CODE_ASSISTANT (assistant));
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  gb_source_code_assistant_queue_parse (assistant);
}

static void
gb_source_code_assistant_disconnect (GbSourceCodeAssistant *assistant)
{
  g_return_if_fail (GB_IS_SOURCE_CODE_ASSISTANT (assistant));

  g_signal_handler_disconnect (assistant->priv->buffer,
                               assistant->priv->changed_handler);
  assistant->priv->changed_handler = 0;

  g_signal_handler_disconnect (assistant->priv->buffer,
                               assistant->priv->notify_language_handler);
  assistant->priv->notify_language_handler = 0;
}

static void
gb_source_code_assistant_connect (GbSourceCodeAssistant *assistant)
{
  g_return_if_fail (GB_IS_SOURCE_CODE_ASSISTANT (assistant));

  assistant->priv->changed_handler =
    g_signal_connect_object (assistant->priv->buffer,
                             "changed",
                             G_CALLBACK (gb_source_code_assistant_buffer_changed),
                             assistant,
                             G_CONNECT_SWAPPED);

  assistant->priv->notify_language_handler =
    g_signal_connect_object (assistant->priv->buffer,
                             "notify::language",
                             G_CALLBACK (gb_source_code_assistant_buffer_notify_language),
                             assistant,
                             G_CONNECT_SWAPPED);
}

/**
 * gb_source_code_assistant_get_buffer:
 * @assistant: (in): A #GbSourceCodeAssistant.
 *
 * Fetches the underlying text buffer.
 *
 * Returns: (transfer none): A #GtkTextBuffer.
 */
GtkTextBuffer *
gb_source_code_assistant_get_buffer (GbSourceCodeAssistant *assistant)
{
  g_return_val_if_fail (GB_IS_SOURCE_CODE_ASSISTANT (assistant), NULL);

  return assistant->priv->buffer;
}

static void
gb_source_code_assistant_buffer_disposed (gpointer  user_data,
                                          GObject  *where_object_was)
{
  GbSourceCodeAssistant *assistant = user_data;

  g_return_if_fail (GB_IS_SOURCE_CODE_ASSISTANT (assistant));

  assistant->priv->buffer = NULL;
  g_cancellable_cancel (assistant->priv->cancellable);
}

static void
gb_source_code_assistant_set_buffer (GbSourceCodeAssistant *assistant,
                                     GtkTextBuffer         *buffer)
{
  GbSourceCodeAssistantPrivate *priv;

  g_return_if_fail (GB_IS_SOURCE_CODE_ASSISTANT (assistant));

  priv = assistant->priv;

  if (priv->buffer != buffer)
    {
      if (priv->buffer)
        {
          gb_source_code_assistant_disconnect (assistant);
          g_object_weak_unref (G_OBJECT (priv->buffer),
                               gb_source_code_assistant_buffer_disposed,
                               assistant);
          priv->buffer = NULL;
        }

      if (buffer)
        {
          priv->buffer = buffer;
          g_object_weak_ref (G_OBJECT (priv->buffer),
                             gb_source_code_assistant_buffer_disposed,
                             assistant);
          gb_source_code_assistant_connect (assistant);
        }

      gb_source_code_assistant_load_service (assistant);

      g_object_notify_by_pspec (G_OBJECT (assistant),
                                gParamSpecs [PROP_BUFFER]);
    }
}

/**
 * gb_source_code_assistant_get_active:
 * @assistant: (in): A #GbSourceCodeAssistant.
 *
 * Fetches the "active" property, indicating if the code assistanace service
 * is currently parsing the buffer.
 *
 * Returns: %TRUE if the file is being parsed.
 */
gboolean
gb_source_code_assistant_get_active (GbSourceCodeAssistant *assistant)
{
  g_return_val_if_fail (GB_IS_SOURCE_CODE_ASSISTANT (assistant), FALSE);

  return assistant->priv->active;
}

static void
gb_source_code_assistant_finalize (GObject *object)
{
  GbSourceCodeAssistantPrivate *priv;

  ENTRY;

  priv = GB_SOURCE_CODE_ASSISTANT (object)->priv;

  if (priv->parse_timeout)
    {
      g_source_remove (priv->parse_timeout);
      priv->parse_timeout = 0;
    }

  if (priv->buffer)
    {
      g_object_add_weak_pointer (G_OBJECT (priv->buffer),
                                 (gpointer *)&priv->buffer);
      priv->buffer = NULL;
    }

  g_clear_object (&priv->proxy);

  if (priv->tmpfile_path)
    {
      g_unlink (priv->tmpfile_path);
      g_clear_pointer (&priv->tmpfile_path, g_free);
    }

  close (priv->tmpfile_fd);
  priv->tmpfile_fd = -1;

  g_clear_pointer (&priv->document_path, g_free);
  g_clear_object (&priv->document_proxy);
  g_clear_object (&priv->cancellable);

  G_OBJECT_CLASS (gb_source_code_assistant_parent_class)->finalize (object);

  EXIT;
}

static void
gb_source_code_assistant_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  GbSourceCodeAssistant *self = GB_SOURCE_CODE_ASSISTANT (object);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, gb_source_code_assistant_get_active (self));
      break;

    case PROP_BUFFER:
      g_value_set_object (value, gb_source_code_assistant_get_buffer (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
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
    case PROP_BUFFER:
      gb_source_code_assistant_set_buffer (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_source_code_assistant_class_init (GbSourceCodeAssistantClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GError *error = NULL;
  gchar *address = NULL;

  object_class->finalize = gb_source_code_assistant_finalize;
  object_class->get_property = gb_source_code_assistant_get_property;
  object_class->set_property = gb_source_code_assistant_set_property;

  gParamSpecs [PROP_ACTIVE] =
    g_param_spec_boolean ("active",
                         _("Active"),
                         _("If code assistance is currently processing."),
                         FALSE,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_ACTIVE,
                                   gParamSpecs [PROP_ACTIVE]);

  gParamSpecs [PROP_BUFFER] =
    g_param_spec_object ("buffer",
                         _("Buffer"),
                         _("The buffer "),
                         GTK_TYPE_TEXT_BUFFER,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_BUFFER,
                                   gParamSpecs [PROP_BUFFER]);

  gSignals [CHANGED] =
    g_signal_new ("changed",
                  GB_TYPE_SOURCE_CODE_ASSISTANT,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GbSourceCodeAssistantClass, changed),
                  NULL,
                  NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  address = g_dbus_address_get_for_bus_sync (G_BUS_TYPE_SESSION, NULL, &error);
  if (!address)
    GOTO (failure);

  gDBus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
  if (!gDBus)
    GOTO (failure);

#if 0
  g_dbus_connection_set_exit_on_close (gDBus, FALSE);
#endif

failure:
  if (error)
    {
      g_warning (_("Failed to load DBus connection. "
                   "Code assistance will be disabled. "
                   "\"%s\" (%s)"),
                 error->message, address);
      g_clear_error (&error);
    }

  g_free (address);
}

static void
gb_source_code_assistant_init (GbSourceCodeAssistant *assistant)
{
  assistant->priv = gb_source_code_assistant_get_instance_private (assistant);
  assistant->priv->tmpfile_fd = -1;
  assistant->priv->cancellable = g_cancellable_new ();
}
