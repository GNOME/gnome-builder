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
#include "gca-structs.h"

#define PARSE_TIMEOUT_MSEC 500

static GDBusConnection *gSessionBus;

static void
add_diagnostic_range (GbEditorTab    *tab,
                      GcaDiagnostic  *diag,
                      GcaSourceRange *range)
{
  GtkTextBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;
  guint column;

  g_assert (GB_IS_EDITOR_TAB (tab));
  g_assert (diag);
  g_assert (range);

  buffer = GTK_TEXT_BUFFER (tab->priv->document);

  gtk_text_buffer_get_iter_at_line (buffer, &begin, range->begin.line);
  for (column = range->begin.column; column; column--)
    if (gtk_text_iter_ends_line (&begin) || !gtk_text_iter_forward_char (&begin))
      break;

  gtk_text_buffer_get_iter_at_line (buffer, &end, range->end.line);
  for (column = range->end.column; column; column--)
    if (gtk_text_iter_ends_line (&end) || !gtk_text_iter_forward_char (&end))
      break;

  if (gtk_text_iter_equal (&begin, &end))
    gtk_text_iter_forward_to_line_end (&end);

  gtk_text_buffer_apply_tag_by_name (buffer, "ErrorTag", &begin, &end);
}

static void
add_diagnostic (GbEditorTab   *tab,
                GcaDiagnostic *diag)
{
  guint i;

  g_return_if_fail (GB_IS_EDITOR_TAB (tab));
  g_return_if_fail (diag);

#if 0
  g_print ("DIAG: %s\n", diag->message);
#endif

  for (i = 0; i < diag->locations->len; i++)
    {
      GcaSourceRange *range;

      range = &g_array_index (diag->locations, GcaSourceRange, i);
      add_diagnostic_range (tab, diag, range);
    }
}

static const gchar *
get_language (GbSourceView *view)
{
  GtkTextBuffer *buffer;
  GtkSourceLanguage *lang;
  const gchar *lang_id;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
  if (!GTK_SOURCE_IS_BUFFER (buffer))
    return NULL;

  lang = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (buffer));
  if (!lang)
    return NULL;

  /* TODO: probably should get the mapping from GCA service for this */
  lang_id = gtk_source_language_get_id (lang);
  if (g_str_equal (lang_id, "chdr") || g_str_equal (lang_id, "objc"))
    return "c";

  return lang_id;
}

static void
gb_editor_code_assistant_diag_cb (GObject      *source_object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  GbEditorTab *tab = user_data;
  GtkTextBuffer *buffer;
  GtkTextTagTable *tag_table;
  GtkTextIter begin;
  GtkTextIter end;
  GtkTextTag *tag;
  GcaDiagnostics *proxy = (GcaDiagnostics *)source_object;
  GVariant *diags = NULL;
  GArray *ar;
  GError *error = NULL;
  guint i;

  ENTRY;

  if (!gca_diagnostics_call_diagnostics_finish (proxy, &diags, result, &error))
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
      GOTO (cleanup);
    }

  buffer = GTK_TEXT_BUFFER (tab->priv->document);
  tag_table = gtk_text_buffer_get_tag_table (buffer);
  tag = gtk_text_tag_table_lookup (tag_table, "ErrorTag");

  if (!tag)
    tag = gtk_text_buffer_create_tag (buffer, "ErrorTag",
                                      "underline", PANGO_UNDERLINE_ERROR,
                                      NULL);

  gtk_text_buffer_get_bounds (buffer, &begin, &end);
  gtk_text_buffer_remove_tag (buffer, tag, &begin, &end);

  ar = gca_diagnostics_from_variant (diags);

  for (i = 0; i < ar->len; i++)
    {
      GcaDiagnostic *diag;

      diag = &g_array_index (ar, GcaDiagnostic, i);
      add_diagnostic (tab, diag);
    }

  if (tab->priv->gca_diagnostics)
    g_array_unref (tab->priv->gca_diagnostics);

  tab->priv->gca_diagnostics = ar;

cleanup:
  g_clear_pointer (&diags, g_variant_unref);
  g_object_unref (tab);

  EXIT;
}

static void
diagnostics_proxy_new_cb (GObject      *source_object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  GcaDiagnostics *proxy = NULL;
  GbEditorTab *tab = user_data;
  GError *error = NULL;

  proxy = gca_diagnostics_proxy_new_finish (result, &error);

  if (!proxy)
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
      GOTO (cleanup);
    }

  gca_diagnostics_call_diagnostics (proxy, NULL,
                                    gb_editor_code_assistant_diag_cb,
                                    g_object_ref (tab));


cleanup:
  g_clear_object (&tab);
  g_clear_object (&proxy);
}

static void
gb_editor_code_assistant_parse_cb (GObject      *source_object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  GbEditorTab *tab = user_data;
  GcaService *service = (GcaService *)source_object;
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

  lang_id = get_language (tab->priv->source_view);
  if (!lang_id)
    GOTO (cleanup);

  name = g_strdup_printf ("org.gnome.CodeAssist.v1.%s", lang_id);

  gca_diagnostics_proxy_new (gSessionBus,
                             G_DBUS_PROXY_FLAGS_NONE,
                             name,
                             document_path,
                             NULL,
                             diagnostics_proxy_new_cb,
                             g_object_ref (tab));

cleanup:
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

  ENTRY;

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

  RETURN (G_SOURCE_REMOVE);
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

static gboolean
on_query_tooltip (GbSourceView *source_view,
                  gint          x,
                  gint          y,
                  gboolean      keyboard_mode,
                  GtkTooltip   *tooltip,
                  GbEditorTab  *tab)
{
  GbEditorTabPrivate *priv;
  GtkTextIter iter;
  guint line;
  guint i;

  g_assert (GB_IS_SOURCE_VIEW (source_view));
  g_assert (GB_IS_EDITOR_TAB (tab));

  priv = tab->priv;

  if (!priv->gca_diagnostics || !priv->gca_diagnostics->len)
    return FALSE;

  gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (source_view),
                                         GTK_TEXT_WINDOW_WIDGET,
                                         x, y, &x, &y);

  gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (source_view),
                                      &iter, x, y);

  line = gtk_text_iter_get_line (&iter);

  for (i = 0; i < priv->gca_diagnostics->len; i++)
    {
      GcaDiagnostic *diag;
      guint j;

      diag = &g_array_index (priv->gca_diagnostics, GcaDiagnostic, i);

      for (j = 0; j < diag->locations->len; j++)
        {
          GcaSourceRange *loc;

          loc = &g_array_index (diag->locations, GcaSourceRange, j);

          if ((loc->begin.line <= line) && (loc->end.line >= line))
            {
              gtk_tooltip_set_text (tooltip, diag->message);
              return TRUE;
            }
        }
    }

  return FALSE;
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
  const gchar *lang_id;
  gchar *name;
  gchar *path;

  ENTRY;

  g_return_if_fail (GB_IS_EDITOR_TAB (tab));
  g_return_if_fail (!tab->priv->gca_service);

  priv = tab->priv;

  if (!gSessionBus)
    {
      gSessionBus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
      if (!gSessionBus)
        EXIT;
    }

  lang_id = get_language (tab->priv->source_view);
  if (!lang_id)
    EXIT;

  name = g_strdup_printf ("org.gnome.CodeAssist.v1.%s", lang_id);
  path = g_strdup_printf ("/org/gnome/CodeAssist/v1/%s", lang_id);

  priv->gca_service =
    gca_service_proxy_new_sync (gSessionBus,
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

  priv->gca_tooltip_handler =
    g_signal_connect (priv->source_view,
                      "query-tooltip",
                      G_CALLBACK (on_query_tooltip),
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

  if (priv->gca_tooltip_handler)
    {
      g_signal_handler_disconnect (priv->document,
                                   priv->gca_tooltip_handler);
      priv->gca_tooltip_handler = 0;
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

  if (priv->gca_diagnostics)
    {
      g_array_unref (priv->gca_diagnostics);
      priv->gca_diagnostics = NULL;
    }

  EXIT;
}
