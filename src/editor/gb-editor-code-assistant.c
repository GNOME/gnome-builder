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

#include "gb-editor-code-assistant.h"
#include "gb-editor-tab-private.h"
#include "gb-log.h"
#include "gca-service.h"

static void
gb_editor_code_assistant_buffer_changed (GbEditorDocument *document,
                                         GbEditorTab      *tab)
{
  g_return_if_fail (GB_IS_EDITOR_TAB (tab));
  g_return_if_fail (GB_IS_EDITOR_DOCUMENT (document));

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

  EXIT;
}
