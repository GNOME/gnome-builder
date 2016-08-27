/* gbp-quick-highlight-view-addin.c
 *
 * Copyright (C) 2016 Martin Blanchard <tchaik@gmx.com>
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

#include <string.h>
#include <glib/gi18n.h>
#include <ide.h>

#include "gbp-quick-highlight-view-addin.h"

struct _GbpQuickHighlightViewAddin
{
  GObject                  parent_instance;

  IdeEditorView           *editor_view;

  GtkSourceSearchContext  *search_context;
  GtkSourceSearchSettings *search_settings;
};

static void editor_view_addin_iface_init (IdeEditorViewAddinInterface *iface);

G_DEFINE_TYPE_EXTENDED (GbpQuickHighlightViewAddin,
                        gbp_quick_highlight_view_addin,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_EDITOR_VIEW_ADDIN,
                                               editor_view_addin_iface_init))

static void
gbp_quick_highlight_view_addin_class_init (GbpQuickHighlightViewAddinClass *klass)
{
}

static void
gbp_quick_highlight_view_addin_init (GbpQuickHighlightViewAddin *self)
{
}

static void
gbp_quick_highlight_view_addin_change_style (GtkSourceBuffer *buffer,
                                             GParamSpec      *pspec,
                                             gpointer         user_data)
{
  GbpQuickHighlightViewAddin *self;
  GtkSourceStyleScheme *style_scheme;
  GtkSourceStyle *style;
  gchar *text;

  g_assert (GTK_SOURCE_IS_BUFFER (buffer));

  self = GBP_QUICK_HIGHLIGHT_VIEW_ADDIN (user_data);

  text = g_strdup (gtk_source_search_settings_get_search_text (self->search_settings));

  gtk_source_search_settings_set_search_text (self->search_settings, NULL);
  gtk_source_search_context_set_highlight (self->search_context, FALSE);

  style_scheme = gtk_source_buffer_get_style_scheme (buffer);
  style = gtk_source_style_scheme_get_style (style_scheme, "current-line");

  gtk_source_search_context_set_match_style (self->search_context, style);

  if (text != NULL && strlen (text) > 0)
    {
      gtk_source_search_settings_set_search_text (self->search_settings, text);
      gtk_source_search_context_set_highlight (self->search_context, TRUE);
    }

  g_free (text);
}

static void
gbp_quick_highlight_view_addin_match (GtkTextBuffer *buffer,
                                      GtkTextIter   *location,
                                      GtkTextMark   *mark,
                                      gpointer       user_data)
{
  GbpQuickHighlightViewAddin *self;
  GtkTextMark *insert_mark;
  GtkTextIter begin;
  GtkTextIter end;
  gchar *text;

  g_assert (GTK_IS_TEXT_BUFFER (buffer));

  self = GBP_QUICK_HIGHLIGHT_VIEW_ADDIN (user_data);

  insert_mark = gtk_text_buffer_get_insert (buffer);
  if (insert_mark != mark)
    return;

  if (gtk_text_buffer_get_selection_bounds (buffer, &begin, &end))
    {
      text = gtk_text_buffer_get_text (buffer, &begin, &end, FALSE);

      if (text != NULL && g_strstrip (text) != NULL)
        {
          gtk_source_search_settings_set_search_text (self->search_settings, text);
          gtk_source_search_context_set_highlight (self->search_context, TRUE);
        }
      else
        {
          gtk_source_search_settings_set_search_text (self->search_settings, NULL);
          gtk_source_search_context_set_highlight (self->search_context, FALSE);
        }

      g_free (text);
    }
  else
    {
      gtk_source_search_settings_set_search_text (self->search_settings, NULL);
      gtk_source_search_context_set_highlight (self->search_context, FALSE);
    }
}

static void
gbp_quick_highlight_view_addin_load (IdeEditorViewAddin *addin,
                                     IdeEditorView      *view)
{
  GbpQuickHighlightViewAddin *self;
  GtkSourceStyleScheme *style_scheme;
  GtkSourceStyle *style;
  GtkSourceBuffer *buffer;

  g_assert (GBP_IS_QUICK_HIGHLIGHT_VIEW_ADDIN (addin));
  g_assert (IDE_IS_EDITOR_VIEW (view));

  self = GBP_QUICK_HIGHLIGHT_VIEW_ADDIN (addin);

  self->editor_view = view;

  buffer = GTK_SOURCE_BUFFER (ide_editor_view_get_document (view));

  self->search_settings = g_object_new (GTK_SOURCE_TYPE_SEARCH_SETTINGS,
                                        "search-text", NULL,
                                        NULL);
  self->search_context = g_object_new (GTK_SOURCE_TYPE_SEARCH_CONTEXT,
                                       "buffer", buffer,
                                       "highlight", FALSE,
                                       "settings", self->search_settings,
                                       NULL);

  style_scheme = gtk_source_buffer_get_style_scheme (buffer);
  style = gtk_source_style_scheme_get_style (style_scheme, "current-line");

  gtk_source_search_context_set_match_style (self->search_context, style);

  g_signal_connect_object (buffer,
                           "notify::style-scheme",
                           G_CALLBACK (gbp_quick_highlight_view_addin_change_style),
                           self,
                           G_CONNECT_AFTER);
  g_signal_connect_object (buffer,
                           "mark-set",
                           G_CALLBACK (gbp_quick_highlight_view_addin_match),
                           self,
                           G_CONNECT_AFTER);
}

static void
gbp_quick_highlight_view_addin_unload (IdeEditorViewAddin *addin,
                                       IdeEditorView      *view)
{
  GbpQuickHighlightViewAddin *self;
  GtkSourceBuffer *buffer;

  g_assert (GBP_IS_QUICK_HIGHLIGHT_VIEW_ADDIN (addin));

  self = GBP_QUICK_HIGHLIGHT_VIEW_ADDIN (addin);

  buffer = GTK_SOURCE_BUFFER (ide_editor_view_get_document (view));

  g_signal_handlers_disconnect_by_func (buffer,
                                        G_CALLBACK (gbp_quick_highlight_view_addin_change_style),
                                        self);
  g_signal_handlers_disconnect_by_func (buffer,
                                        G_CALLBACK (gbp_quick_highlight_view_addin_match),
                                        self);

  g_clear_object (&self->search_settings);
  g_clear_object (&self->search_context);

  self->editor_view = NULL;
}

static void
editor_view_addin_iface_init (IdeEditorViewAddinInterface *iface)
{
  iface->load = gbp_quick_highlight_view_addin_load;
  iface->unload = gbp_quick_highlight_view_addin_unload;
}
