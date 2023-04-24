/* gbp-quick-highlight-editor-page-addin.c
 *
 * Copyright 2016 Martin Blanchard <tchaik@gmx.com>
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "gbp-quick-highlight-editor-page-addin"

#include <libide-editor.h>

#include "gbp-quick-highlight-editor-page-addin.h"

#define HIGHLIGHT_STYLE_NAME "quick-highlight-match"
#define SELECTION_STYLE_NAME "selection"

struct _GbpQuickHighlightEditorPageAddin
{
  GObject                 parent_instance;

  GSettings              *settings;

  IdeEditorPage          *view;

  GSignalGroup           *buffer_signals;
  GSignalGroup           *search_signals;
  GtkSourceSearchContext *search_context;

  guint                   queued_match_source;

  guint                   has_selection : 1;
  guint                   search_active : 1;
};

static gboolean
do_delayed_quick_highlight (GbpQuickHighlightEditorPageAddin *self)
{
  GtkSourceSearchSettings *search_settings;
  g_autofree gchar *slice = NULL;
  IdeBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;
  int selection_length;
  int min_length;

  g_assert (GBP_IS_QUICK_HIGHLIGHT_EDITOR_PAGE_ADDIN (self));
  g_assert (self->view != NULL);

  self->queued_match_source = 0;

  /*
   * Get the current selection, if any. Short circuit if we find a situation
   * that should have caused us to cancel the current quick-highlight.
   */
  buffer = ide_editor_page_get_buffer (self->view);
  if (self->search_active ||
      !self->has_selection ||
      !gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (buffer), &begin, &end))
    {
      g_clear_object (&self->search_context);
      return G_SOURCE_REMOVE;
    }

  min_length = g_settings_get_int (self->settings, "min-char-selected");
  selection_length = gtk_text_iter_get_offset (&end) - gtk_text_iter_get_offset (&begin);
  if (selection_length < min_length)
    {
      g_clear_object (&self->search_context);
      return G_SOURCE_REMOVE;
    }

  /*
   * If the current selection goes across a line, then ignore trying to match
   * anything similar as it's unlikely to be what the user wants.
   */
  gtk_text_iter_order (&begin, &end);
  if (gtk_text_iter_get_line (&begin) != gtk_text_iter_get_line (&end))
    {
      g_clear_object (&self->search_context);
      return G_SOURCE_REMOVE;
    }

  /*
   * Create our search context to scan the buffer if necessary.
   */
  if (self->search_context == NULL)
    {
      g_autoptr(GtkSourceSearchSettings) settings = NULL;
      GtkSourceStyleScheme *style_scheme;
      GtkSourceStyle *style = NULL;

      style_scheme = gtk_source_buffer_get_style_scheme (GTK_SOURCE_BUFFER (buffer));

      if (style_scheme != NULL)
        {
          if (!(style = gtk_source_style_scheme_get_style (style_scheme, HIGHLIGHT_STYLE_NAME)))
            style = gtk_source_style_scheme_get_style (style_scheme, SELECTION_STYLE_NAME);
        }

      settings = g_object_new (GTK_SOURCE_TYPE_SEARCH_SETTINGS,
                               "at-word-boundaries", FALSE,
                               "case-sensitive", TRUE,
                               "regex-enabled", FALSE,
                               NULL);

      /* Set highlight to false initially, or we get the wrong style from
       * the the GtkSourceSearchContext.
       */
      self->search_context = g_object_new (GTK_SOURCE_TYPE_SEARCH_CONTEXT,
                                           "buffer", buffer,
                                           "highlight", FALSE,
                                           "match-style", style,
                                           "settings", settings,
                                           NULL);
    }

  search_settings = gtk_source_search_context_get_settings (self->search_context);

  /* Now assign our search text */
  slice = gtk_text_iter_get_slice (&begin, &end);
  gtk_source_search_settings_set_search_text (search_settings, slice);

  /* (Re)enable highlight so that we have the correct style */
  gtk_source_search_context_set_highlight (self->search_context, TRUE);

  return G_SOURCE_REMOVE;
}

static void
buffer_cursor_moved (GbpQuickHighlightEditorPageAddin *self,
                     IdeBuffer                        *buffer)
{
  g_assert (GBP_IS_QUICK_HIGHLIGHT_EDITOR_PAGE_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  if (self->has_selection && !self->search_active)
    {
      if (self->queued_match_source == 0)
        self->queued_match_source =
          g_idle_add_full (G_PRIORITY_LOW + 100,
                           (GSourceFunc) do_delayed_quick_highlight,
                           g_object_ref (self),
                           g_object_unref);
    }
  else
    {
      g_clear_handle_id (&self->queued_match_source, g_source_remove);
      g_clear_object (&self->search_context);
    }
}

static void
buffer_notify_style_scheme (GbpQuickHighlightEditorPageAddin *self,
                            GParamSpec                       *pspec,
                            IdeBuffer                        *buffer)
{
  g_assert (GBP_IS_QUICK_HIGHLIGHT_EDITOR_PAGE_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  if (self->search_context != NULL)
    {
      GtkSourceStyleScheme *style_scheme;
      GtkSourceStyle *style = NULL;

      style_scheme = gtk_source_buffer_get_style_scheme (GTK_SOURCE_BUFFER (buffer));
      if (style_scheme != NULL)
        style = gtk_source_style_scheme_get_style (style_scheme, HIGHLIGHT_STYLE_NAME);

      gtk_source_search_context_set_match_style (self->search_context, style);
    }
}

static void
buffer_notify_has_selection (GbpQuickHighlightEditorPageAddin *self,
                             GParamSpec                       *pspec,
                             IdeBuffer                        *buffer)
{
  g_assert (GBP_IS_QUICK_HIGHLIGHT_EDITOR_PAGE_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  self->has_selection = gtk_text_buffer_get_has_selection (GTK_TEXT_BUFFER (buffer));
}

#if 0
static void
search_notify_active (GbpQuickHighlightEditorPageAddin *self,
                      GParamSpec                       *pspec,
                      IdeEditorSearch                  *search)
{
  g_assert (GBP_IS_QUICK_HIGHLIGHT_EDITOR_PAGE_ADDIN (self));
  g_assert (IDE_IS_EDITOR_SEARCH (search));

  self->search_active = ide_editor_search_get_active (search);
  do_delayed_quick_highlight (self);
}
#endif

static void
gbp_quick_highlight_editor_page_addin_load (IdeEditorPageAddin *addin,
                                            IdeEditorPage      *view)
{
  GbpQuickHighlightEditorPageAddin *self = (GbpQuickHighlightEditorPageAddin *)addin;

  g_assert (GBP_IS_QUICK_HIGHLIGHT_EDITOR_PAGE_ADDIN (addin));
  g_assert (IDE_IS_EDITOR_PAGE (view));

  self->view = view;

  self->settings = g_settings_new ("org.gnome.builder.editor");

  self->buffer_signals = g_signal_group_new (IDE_TYPE_BUFFER);
  g_signal_group_connect_swapped (self->buffer_signals,
                                    "notify::has-selection",
                                    G_CALLBACK (buffer_notify_has_selection),
                                    self);
  g_signal_group_connect_swapped (self->buffer_signals,
                                    "notify::style-scheme",
                                    G_CALLBACK (buffer_notify_style_scheme),
                                    self);
  g_signal_group_connect_swapped (self->buffer_signals,
                                    "cursor-moved",
                                    G_CALLBACK (buffer_cursor_moved),
                                    self);
  g_signal_group_set_target (self->buffer_signals, ide_editor_page_get_buffer (view));

#if 0
  self->search_signals = g_signal_group_new (IDE_TYPE_EDITOR_SEARCH);
  g_signal_group_connect_swapped (self->search_signals,
                                    "notify::active",
                                    G_CALLBACK (search_notify_active),
                                    self);
  g_signal_group_set_target (self->search_signals, ide_editor_page_get_search (view));
#endif
}

static void
gbp_quick_highlight_editor_page_addin_unload (IdeEditorPageAddin *addin,
                                              IdeEditorPage      *view)
{
  GbpQuickHighlightEditorPageAddin *self = (GbpQuickHighlightEditorPageAddin *)addin;

  g_assert (GBP_IS_QUICK_HIGHLIGHT_EDITOR_PAGE_ADDIN (addin));
  g_assert (IDE_IS_EDITOR_PAGE (view));

  g_clear_object (&self->search_context);
  g_clear_handle_id (&self->queued_match_source, g_source_remove);

  g_signal_group_set_target (self->buffer_signals, NULL);
  g_clear_object (&self->buffer_signals);

#if 0
  g_signal_group_set_target (self->search_signals, NULL);
  g_clear_object (&self->search_signals);
#endif

  g_clear_object (&self->settings);

  self->view = NULL;
}

static void
editor_view_addin_iface_init (IdeEditorPageAddinInterface *iface)
{
  iface->load = gbp_quick_highlight_editor_page_addin_load;
  iface->unload = gbp_quick_highlight_editor_page_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpQuickHighlightEditorPageAddin,
                               gbp_quick_highlight_editor_page_addin,
                               G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_EDITOR_PAGE_ADDIN, editor_view_addin_iface_init))

static void
gbp_quick_highlight_editor_page_addin_class_init (GbpQuickHighlightEditorPageAddinClass *klass)
{
}

static void
gbp_quick_highlight_editor_page_addin_init (GbpQuickHighlightEditorPageAddin *self)
{
}
