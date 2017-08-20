/* gbp-documentation-card-view-addin.c
 *
 * Copyright (C) 2017 Lucie Charvat <luci.charvat@gmail.com>
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

#define G_LOG_DOMAIN "gbp-documentation-card-view-addin"

#include <ide.h>

#include "gbp-documentation-card-view-addin.h"
#include "gbp-documentation-card.h"

#define POPUP_TIMEOUT          1
#define POPDOWN_TIMEOUT        500
#define SPACE_TOLERANCE        15

struct _GbpDocumentationCardViewAddin
{
  GObject               parent_instance;

  IdeEditorView        *editor_view;
  GbpDocumentationCard *popover;
  gchar                *previous_text;

  guint                 timeout_id;
  gulong                motion_handler_id;
  guint                 poped_up : 1;
  guint                 last_x, last_y;
};

static void iface_init (IdeEditorViewAddinInterface *iface);

G_DEFINE_TYPE_EXTENDED (GbpDocumentationCardViewAddin, gbp_documentation_card_view_addin, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_EDITOR_VIEW_ADDIN, iface_init))

static gboolean
within_space (GbpDocumentationCardViewAddin *self,
              guint                          x,
              guint                          y)
{
  return (x <= self->last_x + SPACE_TOLERANCE &&
      x >= self->last_x - SPACE_TOLERANCE &&
      y <= self->last_y + SPACE_TOLERANCE &&
      y >= self->last_y - SPACE_TOLERANCE);
}

static gboolean
unichar_issymbol (gunichar ch)
{
  return g_unichar_islower (ch) || g_unichar_isdigit (ch) || ch == '_';
}


static gboolean
search_document_cb (gpointer data)
{
  GbpDocumentationCardViewAddin *self = GBP_DOCUMENTATION_CARD_VIEW_ADDIN  (data);
  IdeBuffer *buffer;
  IdeSourceView *source_view;
  GtkSourceLanguage *lang;
  IdeContext *context;
  IdeDocumentation *doc;
  IdeDocumentationContext doc_context;

  GdkDisplay *display;
  GdkWindow *window;
  GdkDevice *device;

  GtkTextIter begin;
  GtkTextIter end;

  g_autoptr(IdeDocumentationInfo) info = NULL;
  g_autofree gchar *selected_text = NULL;
  gint x, y;

  self->timeout_id = 0;

  /* Be defensive against widget destruction */
  if (self->editor_view == NULL ||
      NULL == (window = gtk_widget_get_parent_window (GTK_WIDGET (self->editor_view))) ||
      NULL == (display = gdk_window_get_display (window)) ||
      NULL == (device = gdk_seat_get_pointer (gdk_display_get_default_seat (display))))
    {
      if (self->poped_up)
        self->poped_up = FALSE;
      gbp_documentation_card_popdown (self->popover);
      return G_SOURCE_REMOVE;
    }

  gdk_window_get_device_position (window, device, &x, &y, NULL);

  if (self->poped_up)
    {
      if (within_space (self, x, y))
        return G_SOURCE_REMOVE;
      self->poped_up = FALSE;
      gbp_documentation_card_popdown (self->popover);
      return G_SOURCE_REMOVE;
    }

  self->last_x = x;
  self->last_y = y;

  source_view = ide_editor_view_get_view (self->editor_view);
  if (!GTK_SOURCE_IS_VIEW (source_view))
    return G_SOURCE_REMOVE;

  buffer = ide_editor_view_get_buffer (self->editor_view);
  if (buffer == NULL)
    return G_SOURCE_REMOVE;

  context = ide_buffer_get_context (buffer);
  doc =  ide_context_get_documentation (context);

  lang = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (buffer));
  if (lang == NULL)
    return G_SOURCE_REMOVE;

  if (ide_str_equal0 (gtk_source_language_get_id(lang), "c"))
    doc_context = IDE_DOCUMENTATION_CONTEXT_CARD_C;
  else
    return G_SOURCE_REMOVE;

  gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (source_view), GTK_TEXT_WINDOW_WIDGET, x, y, &x, &y);
  gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (source_view), &end, x, y);
  gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (source_view), &begin, x, y);

  while (unichar_issymbol (gtk_text_iter_get_char (&begin)))
    if (!gtk_text_iter_backward_char (&begin))
      break;
  gtk_text_iter_forward_char (&begin);

  while (unichar_issymbol (gtk_text_iter_get_char (&end)))
    if (!gtk_text_iter_forward_char (&end))
      break;

  selected_text = gtk_text_iter_get_slice (&begin, &end);

  if (g_strcmp0 (selected_text, self->previous_text) != 0)
    {
      info = ide_documentation_get_info (doc, selected_text, doc_context);
      if (ide_documentation_info_get_size (info) == 0)
        return G_SOURCE_REMOVE;

      gbp_documentation_card_set_info (self->popover, info);
      g_free (self->previous_text);
      self->previous_text = g_steal_pointer (&selected_text);
    }

  gbp_documentation_card_popup (self->popover);
  self->poped_up = TRUE;

  return G_SOURCE_REMOVE;
}



static gboolean
motion_notify_event_cb (gpointer data)
{
  GbpDocumentationCardViewAddin *self = GBP_DOCUMENTATION_CARD_VIEW_ADDIN  (data);

  ide_clear_source (&self->timeout_id);

  if (!self->poped_up)
    self->timeout_id =
            gdk_threads_add_timeout_seconds_full (G_PRIORITY_LOW,
                                                  POPUP_TIMEOUT,
                                                  search_document_cb,
                                                  g_object_ref (self),
                                                  g_object_unref);
  else
    search_document_cb (self);

  return FALSE;
}

static void
gbp_documentation_card_view_addin_load (IdeEditorViewAddin *addin,
                                        IdeEditorView      *view)
{
  GbpDocumentationCardViewAddin *self;

  g_assert (GBP_IS_DOCUMENTATION_CARD_VIEW_ADDIN (addin));
  g_assert (IDE_IS_EDITOR_VIEW (view));

  self = GBP_DOCUMENTATION_CARD_VIEW_ADDIN (addin);
  self->editor_view = view;
  self->popover = g_object_new (GBP_TYPE_DOCUMENTATION_CARD,
                                "relative-to", view,
                                "position", GTK_POS_TOP,
                                "modal", FALSE,
                                 NULL);
  g_signal_connect (self->popover,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->popover);
  self->motion_handler_id =
    g_signal_connect_object (view,
                            "motion-notify-event",
                            G_CALLBACK (motion_notify_event_cb),
                            self,
                            G_CONNECT_SWAPPED);

}

static void
gbp_documentation_card_view_addin_unload (IdeEditorViewAddin *addin,
                                          IdeEditorView      *view)
{
  GbpDocumentationCardViewAddin *self;

  g_assert (GBP_IS_DOCUMENTATION_CARD_VIEW_ADDIN (addin));
  g_assert (IDE_IS_EDITOR_VIEW (view));

  self = GBP_DOCUMENTATION_CARD_VIEW_ADDIN (addin);

  ide_clear_source (&self->timeout_id);
  ide_clear_signal_handler (self->editor_view, &self->motion_handler_id);
  g_clear_pointer (&self->previous_text, g_free);

  if (self->popover != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->popover));

  self->editor_view = NULL;

}

static void
gbp_documentation_card_view_addin_class_init (GbpDocumentationCardViewAddinClass *klass)
{
}

static void
gbp_documentation_card_view_addin_init (GbpDocumentationCardViewAddin *self)
{
}

static void
iface_init (IdeEditorViewAddinInterface *iface)
{
  iface->load = gbp_documentation_card_view_addin_load;
  iface->unload = gbp_documentation_card_view_addin_unload;
}
