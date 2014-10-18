/* gb-editor-code-assistant.c
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

#include <glib/gstdio.h>

#include "gb-editor-code-assistant.h"
#include "gb-editor-tab-private.h"
#include "gb-log.h"
#include "gca-diagnostics.h"
#include "gca-service.h"

#define PARSE_TIMEOUT_MSEC 250

static void
gb_editor_code_assistant_diag_cb (GObject      *source_object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  GbEditorTab *tab = user_data;
  GcaDiagnostics *diag = (GcaDiagnostics *)source_object;
  GVariant *diags = NULL;
  GError *error = NULL;

  if (!gca_diagnostics_call_diagnostics_finish (diag, &diags, result, &error))
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
      GOTO (cleanup);
    }

  {
    gchar *str = g_variant_print (diags, TRUE);
    g_print (">> %s\n", str);
    g_free (str);
  }

cleanup:
  g_clear_pointer (&diags, g_variant_unref);
  g_object_unref (tab);
}

static void
gb_editor_code_assistant_parse_cb (GObject      *source_object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  GbEditorTab *tab = user_data;
  GcaService *service = (GcaService *)source_object;
  GcaDiagnostics *diag_proxy = NULL;
  GtkSourceLanguage *language;
  const gchar *lang_id;
  GError *error = NULL;
  gchar *document_path = NULL;
  gchar *name = NULL;

  ENTRY;

  g_return_if_fail (GCA_IS_SERVICE (service));
  g_return_if_fail (GB_IS_EDITOR_TAB (tab));

  if (!gca_service_call_parse_finish (service, &document_path, result, &error))
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
      GOTO (cleanup);
    }

  language =
    gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (tab->priv->document));
  if (!language)
    GOTO (cleanup);

  lang_id = gtk_source_language_get_id (language);
  name = g_strdup_printf ("org.gnome.CodeAssist.v1.%s", lang_id);

  diag_proxy =
    gca_diagnostics_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                            G_DBUS_PROXY_FLAGS_NONE,
                                            name, document_path,
                                            NULL, &error);
  if (!diag_proxy)
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
      GOTO (cleanup);
    }

  gca_diagnostics_call_diagnostics (diag_proxy, NULL,
                                    gb_editor_code_assistant_diag_cb,
                                    g_object_ref (tab));

cleanup:
  g_clear_object (&diag_proxy);
  g_free (name);
  g_free (document_path);
  g_object_unref (tab);

  EXIT;
}

static gboolean
gb_editor_code_assistant_parse (gpointer user_data)
{
  GbEditorTabPrivate *priv;
  GbEditorTab *tab = user_data;
  GtkTextIter begin;
  GtkTextIter end;
  GVariant *cursor;
  GVariant *options;
  GFile *location;
  gchar *data_path = g_get_current_dir (); /* TODO: Get from Git */
  gchar *path;
  gchar *text;

  g_return_val_if_fail (GB_IS_EDITOR_TAB (tab), G_SOURCE_REMOVE);

  priv = tab->priv;

  priv->gca_parse_timeout = 0;

  gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (priv->document), &begin, &end);
  text = gtk_text_buffer_get_text (GTK_TEXT_BUFFER (priv->document), &begin,
                                   &end, TRUE);
  g_file_set_contents (priv->gca_tmpfile, text, -1, NULL);

  location = gtk_source_file_get_location (priv->file);
  path = g_file_get_path (location);

  cursor = g_variant_new ("(xx)",
                          G_GINT64_CONSTANT (0),
                          G_GINT64_CONSTANT (0));
  options = g_variant_new ("a{sv}", 0);

  g_print ("PATH: %s\n", path);

  gca_service_call_parse (priv->gca_service,
                          path,
                          priv->gca_tmpfile,
                          cursor,
                          options,
                          NULL,
                          gb_editor_code_assistant_parse_cb,
                          g_object_ref (tab));

  g_free (data_path);
  g_free (path);
  g_free (text);

  return G_SOURCE_REMOVE;
}

static void
gb_editor_code_assistant_buffer_changed (GbEditorDocument *document,
                                         GbEditorTab      *tab)
{
  GbEditorTabPrivate *priv;

  g_return_if_fail (GB_IS_EDITOR_TAB (tab));
  g_return_if_fail (GB_IS_EDITOR_DOCUMENT (document));

  priv = tab->priv;

  if (priv->gca_parse_timeout)
    g_source_remove (priv->gca_parse_timeout);

  priv->gca_parse_timeout = g_timeout_add (PARSE_TIMEOUT_MSEC,
                                           gb_editor_code_assistant_parse,
                                           tab);
}

/**
 * gb_editor_code_assistant_init:
 *
 * Initializes the code assistant based on the open file, source language,
 * and document buffer.
 *
 * This will hook to the gnome-code-assistance service to provide warnings
 * if possible.
 */
void
gb_editor_code_assistant_init (GbEditorTab *tab)
{
  GbEditorTabPrivate *priv;
  GtkSourceLanguage *language;
  const gchar *lang_id;
  gchar *name;
  gchar *path;

  ENTRY;

  g_return_if_fail (GB_IS_EDITOR_TAB (tab));
  g_return_if_fail (!tab->priv->gca_service);

  priv = tab->priv;

  language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (priv->document));
  if (!language)
    EXIT;

  lang_id = gtk_source_language_get_id (language);
  name = g_strdup_printf ("org.gnome.CodeAssist.v1.%s", lang_id);
  path = g_strdup_printf ("/org/gnome/CodeAssist/v1/%s", lang_id);

  priv->gca_service =
    gca_service_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                        G_DBUS_PROXY_FLAGS_NONE,
                                        name, path, NULL, NULL);

  g_free (name);
  g_free (path);

  if (!priv->gca_service)
    {
      g_message ("No code assistance found for language \"%s\"", lang_id);
      EXIT;
    }

  priv->gca_tmpfd =
    g_file_open_tmp ("builder-code-assist.XXXXXX",
                     &priv->gca_tmpfile, NULL);

  priv->gca_buffer_changed_handler =
    g_signal_connect (priv->document, "changed",
                      G_CALLBACK (gb_editor_code_assistant_buffer_changed),
                      tab);
}

void
gb_editor_code_assistant_destroy (GbEditorTab *tab)
{
  GbEditorTabPrivate *priv;

  ENTRY;

  g_return_if_fail (GB_IS_EDITOR_TAB (tab));

  priv = tab->priv;

  g_clear_object (&priv->gca_service);

  if (priv->gca_buffer_changed_handler)
    {
      g_signal_handler_disconnect (priv->document,
                                   priv->gca_buffer_changed_handler);
      priv->gca_buffer_changed_handler = 0;
    }

  if (priv->gca_parse_timeout)
    {
      g_source_remove (priv->gca_parse_timeout);
      priv->gca_parse_timeout = 0;
    }

  if (priv->gca_tmpfile)
    {
      g_unlink (priv->gca_tmpfile);
      g_clear_pointer (&priv->gca_tmpfile, g_free);
    }

  if (priv->gca_tmpfd != -1)
    {
      close (priv->gca_tmpfd);
      priv->gca_tmpfd = -1;
    }


  EXIT;
}
