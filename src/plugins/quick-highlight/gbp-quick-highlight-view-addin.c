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
  GSettings               *settings;

  /* No reference, just for quick comparison */
  GtkTextMark             *insert_mark;

  gulong                   notify_style_scheme_handler;
  gulong                   mark_set_handler;
  gulong                   changed_enabled_handler;
  gulong                   delete_range_handler;

  guint                    queued_update;

  guint                    enabled : 1;
};

static void editor_view_addin_iface_init (IdeEditorViewAddinInterface *iface);

G_DEFINE_TYPE_EXTENDED (GbpQuickHighlightViewAddin,
                        gbp_quick_highlight_view_addin,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_EDITOR_VIEW_ADDIN, editor_view_addin_iface_init))

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
  GbpQuickHighlightViewAddin *self = user_data;
  g_autofree gchar *text = NULL;
  GtkSourceStyleScheme *style_scheme;
  GtkSourceStyle *style;

  g_assert (GBP_IS_QUICK_HIGHLIGHT_VIEW_ADDIN (self));
  g_assert (GTK_SOURCE_IS_BUFFER (buffer));

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
}

static void
gbp_quick_highlight_view_addin_match (GbpQuickHighlightViewAddin *self)
{
  g_autofree gchar *text = NULL;
  GtkTextBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (GBP_IS_QUICK_HIGHLIGHT_VIEW_ADDIN (self));

  buffer = GTK_TEXT_BUFFER (ide_editor_view_get_buffer (self->editor_view));

  if (gtk_text_buffer_get_selection_bounds (buffer, &begin, &end))
    {
      text = gtk_text_buffer_get_text (buffer, &begin, &end, FALSE);

      g_strstrip (text);

      if (text[0])
        {
          gtk_source_search_settings_set_search_text (self->search_settings, text);
          gtk_source_search_context_set_highlight (self->search_context, TRUE);
        }
      else
        {
          gtk_source_search_settings_set_search_text (self->search_settings, NULL);
          gtk_source_search_context_set_highlight (self->search_context, FALSE);
        }
    }
  else
    {
      gtk_source_search_settings_set_search_text (self->search_settings, NULL);
      gtk_source_search_context_set_highlight (self->search_context, FALSE);
    }
}


static gboolean
gbp_quick_highlight_view_addin_do_update (gpointer data)
{
  GbpQuickHighlightViewAddin *self = data;

  g_assert (GBP_IS_QUICK_HIGHLIGHT_VIEW_ADDIN (self));

  self->queued_update = 0;

  if (self->editor_view != NULL)
    gbp_quick_highlight_view_addin_match (self);

  return G_SOURCE_REMOVE;
}

static void
gbp_quick_highlight_view_addin_queue_update (GbpQuickHighlightViewAddin *self)
{
  g_assert (GBP_IS_QUICK_HIGHLIGHT_VIEW_ADDIN (self));

  if (self->queued_update == 0)
    self->queued_update =
      gdk_threads_add_idle_full (G_PRIORITY_LOW,
                                 gbp_quick_highlight_view_addin_do_update,
                                 g_object_ref (self),
                                 g_object_unref);
}

static void
gbp_quick_highlight_view_addin_mark_set (GtkTextBuffer *buffer,
                                         GtkTextIter   *location,
                                         GtkTextMark   *mark,
                                         gpointer       user_data)
{
  GbpQuickHighlightViewAddin *self = user_data;

  g_assert (GBP_IS_QUICK_HIGHLIGHT_VIEW_ADDIN (self));

  if G_LIKELY (mark != self->insert_mark)
    return;

  gbp_quick_highlight_view_addin_queue_update (self);
}

static void
gbp_quick_highlight_view_addin_delete_range (GbpQuickHighlightViewAddin *self,
                                             GtkTextIter                *begin,
                                             GtkTextIter                *end,
                                             GtkTextBuffer              *buffer)
{
  g_assert (GBP_IS_QUICK_HIGHLIGHT_VIEW_ADDIN (self));
  g_assert (begin != NULL);
  g_assert (end != NULL);
  g_assert (GTK_IS_TEXT_BUFFER (buffer));

  gbp_quick_highlight_view_addin_queue_update (self);
}

static void
gbp_quick_highlight_view_addin_enabled_changed (GbpQuickHighlightViewAddin *self,
                                                const gchar                *key,
                                                GSettings                  *settings)
{
  IdeBuffer *buffer;
  gboolean enabled;

  g_assert (GBP_IS_QUICK_HIGHLIGHT_VIEW_ADDIN (self));
  g_assert (G_IS_SETTINGS (settings));

  buffer = ide_editor_view_get_buffer (self->editor_view);
  enabled = g_settings_get_boolean (settings, "enabled");

  if (!self->enabled && enabled)
    {
      g_signal_handler_unblock (buffer, self->notify_style_scheme_handler);
      g_signal_handler_unblock (buffer, self->mark_set_handler);
    }
  else if (self->enabled && !enabled)
    {
      g_signal_handler_block (buffer, self->notify_style_scheme_handler);
      g_signal_handler_block (buffer, self->mark_set_handler);

      gtk_source_search_settings_set_search_text (self->search_settings, NULL);

      gtk_source_search_context_set_highlight (self->search_context, FALSE);
    }

  self->enabled = enabled;
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

  buffer = GTK_SOURCE_BUFFER (ide_editor_view_get_buffer (view));

  self->insert_mark = gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (buffer));

  self->search_settings = g_object_new (GTK_SOURCE_TYPE_SEARCH_SETTINGS,
                                        "search-text", NULL,
                                        NULL);
  self->search_context = g_object_new (GTK_SOURCE_TYPE_SEARCH_CONTEXT,
                                       "buffer", buffer,
                                       "highlight", FALSE,
                                       "settings", self->search_settings,
                                       NULL);

  style_scheme = gtk_source_buffer_get_style_scheme (buffer);

  style = gtk_source_style_scheme_get_style (style_scheme, "quick-highlight");
  if (style == NULL)
    style = gtk_source_style_scheme_get_style (style_scheme, "current-line");

  gtk_source_search_context_set_match_style (self->search_context, style);

  self->notify_style_scheme_handler =
    g_signal_connect_object (buffer,
                             "notify::style-scheme",
                             G_CALLBACK (gbp_quick_highlight_view_addin_change_style),
                             self,
                             G_CONNECT_AFTER);

  self->mark_set_handler =
    g_signal_connect_object (buffer,
                             "mark-set",
                             G_CALLBACK (gbp_quick_highlight_view_addin_mark_set),
                             self,
                             G_CONNECT_AFTER);

  self->delete_range_handler =
    g_signal_connect_object (buffer,
                             "delete-range",
                             G_CALLBACK (gbp_quick_highlight_view_addin_delete_range),
                             self,
                             G_CONNECT_AFTER | G_CONNECT_SWAPPED);

  /* Use conventions from IdeExtensionSetAdapter */
  self->settings = g_settings_new_with_path ("org.gnome.builder.extension-type",
                                             "/org/gnome/builder/extension-types/quick-highlight-plugin/GbpQuickHighlightViewAddin/");

  self->changed_enabled_handler =
    g_signal_connect_object (self->settings,
                             "changed::enabled",
                             G_CALLBACK (gbp_quick_highlight_view_addin_enabled_changed),
                             self,
                             G_CONNECT_SWAPPED);

  self->enabled = TRUE;
}

static void
gbp_quick_highlight_view_addin_unload (IdeEditorViewAddin *addin,
                                       IdeEditorView      *view)
{
  GbpQuickHighlightViewAddin *self = (GbpQuickHighlightViewAddin *)addin;
  GtkSourceBuffer *buffer;

  g_assert (GBP_IS_QUICK_HIGHLIGHT_VIEW_ADDIN (self));

  buffer = GTK_SOURCE_BUFFER (ide_editor_view_get_buffer (view));

  ide_clear_source (&self->queued_update);

  ide_clear_signal_handler (buffer, &self->notify_style_scheme_handler);
  ide_clear_signal_handler (buffer, &self->mark_set_handler);
  ide_clear_signal_handler (buffer, &self->delete_range_handler);
  ide_clear_signal_handler (self->settings, &self->changed_enabled_handler);

  g_clear_object (&self->search_settings);
  g_clear_object (&self->search_context);
  g_clear_object (&self->settings);

  self->editor_view = NULL;
}

static void
editor_view_addin_iface_init (IdeEditorViewAddinInterface *iface)
{
  iface->load = gbp_quick_highlight_view_addin_load;
  iface->unload = gbp_quick_highlight_view_addin_unload;
}
