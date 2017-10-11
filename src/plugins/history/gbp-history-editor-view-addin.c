/* gbp-history-editor-view-addin.c
 *
 * Copyright © 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-history-editor-view-addin"

#include "gbp-history-editor-view-addin.h"
#include "gbp-history-item.h"
#include "gbp-history-layout-stack-addin.h"

struct _GbpHistoryEditorViewAddin
{
  GObject                     parent_instance;

  /* Unowned pointer */
  IdeEditorView              *editor;

  /* Weak pointer */
  GbpHistoryLayoutStackAddin *stack_addin;

  gsize                       last_change_count;

  guint                       queued_edit_line;
  guint                       queued_edit_source;
};

static void
gbp_history_editor_view_addin_stack_set (IdeEditorViewAddin *addin,
                                         IdeLayoutStack     *stack)
{
  GbpHistoryEditorViewAddin *self = (GbpHistoryEditorViewAddin *)addin;
  IdeLayoutStackAddin *stack_addin;

  IDE_ENTRY;

  g_assert (IDE_IS_EDITOR_VIEW_ADDIN (self));
  g_assert (IDE_IS_LAYOUT_STACK (stack));

  stack_addin = ide_layout_stack_addin_find_by_module_name (stack, "history-plugin");

  g_assert (stack_addin != NULL);
  g_assert (GBP_IS_HISTORY_LAYOUT_STACK_ADDIN (stack_addin));

  ide_set_weak_pointer (&self->stack_addin, GBP_HISTORY_LAYOUT_STACK_ADDIN (stack_addin));

  IDE_EXIT;
}

static void
gbp_history_editor_view_addin_push (GbpHistoryEditorViewAddin *self,
                                    const GtkTextIter         *iter)
{
  g_autoptr(GbpHistoryItem) item = NULL;
  GtkTextBuffer *buffer;
  GtkTextMark *mark;

  IDE_ENTRY;

  g_assert (GBP_IS_HISTORY_EDITOR_VIEW_ADDIN (self));
  g_assert (iter != NULL);
  g_assert (self->editor != NULL);

  if (self->stack_addin == NULL)
    IDE_GOTO (no_stack_loaded);

  /*
   * Create an unnamed mark for this history item, and push the history
   * item into the stacks history.
   */
  buffer = gtk_text_iter_get_buffer (iter);
  mark = gtk_text_buffer_create_mark (buffer, NULL, iter, TRUE);
  item = gbp_history_item_new (mark);

  gbp_history_layout_stack_addin_push (self->stack_addin, item);

no_stack_loaded:
  IDE_EXIT;
}

static void
gbp_history_editor_view_addin_jump (GbpHistoryEditorViewAddin *self,
                                    const GtkTextIter         *from,
                                    const GtkTextIter         *to,
                                    IdeSourceView             *source_view)
{
  IdeBuffer *buffer;
  gsize change_count;

  g_assert (GBP_IS_HISTORY_EDITOR_VIEW_ADDIN (self));
  g_assert (from != NULL);
  g_assert (to != NULL);
  g_assert (IDE_IS_SOURCE_VIEW (source_view));

  buffer = IDE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (source_view)));
  change_count = ide_buffer_get_change_count (buffer);

  /*
   * If the buffer has changed since the last jump was recorded,
   * we want to track this as an edit point so that we can come
   * back to it later.
   */

#if 0
  g_print ("Cursor jumped from %u:%u\n",
           gtk_text_iter_get_line (iter) + 1,
           gtk_text_iter_get_line_offset (iter) + 1);
  g_print ("Now=%lu Prev=%lu\n", change_count, self->last_change_count);
#endif

  //if (change_count != self->last_change_count)
    {
      self->last_change_count = change_count;
      gbp_history_editor_view_addin_push (self, from);
      gbp_history_editor_view_addin_push (self, to);
    }
}

static gboolean
gbp_history_editor_view_addin_flush_edit (gpointer user_data)
{
  GbpHistoryEditorViewAddin *self = user_data;
  IdeBuffer *buffer;
  GtkTextIter iter;

  g_assert (GBP_IS_HISTORY_EDITOR_VIEW_ADDIN (self));
  g_assert (self->editor != NULL);

  self->queued_edit_source = 0;

  buffer = ide_editor_view_get_buffer (self->editor);
  gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (buffer), &iter, self->queued_edit_line);
  gbp_history_editor_view_addin_push (self, &iter);

  return G_SOURCE_REMOVE;
}

static void
gbp_history_editor_view_addin_queue (GbpHistoryEditorViewAddin *self,
                                     guint                      line)
{
  /*
   * If the buffer is modified, we want to keep track of this position in the
   * history (the layout stack will automatically merge it with the previous
   * entry if they are close).
   *
   * However, the insert-text signal can happen in rapid succession, so we only
   * want to deal with it after a small timeout to coallesce the entries into a
   * single push() into the history stack.
   */

  if (self->queued_edit_source == 0)
    {
      self->queued_edit_line = line;
      self->queued_edit_source = gdk_threads_add_idle_full (G_PRIORITY_LOW,
                                                            gbp_history_editor_view_addin_flush_edit,
                                                            g_object_ref (self),
                                                            g_object_unref);
    }
}

static void
gbp_history_editor_view_addin_insert_text (GbpHistoryEditorViewAddin *self,
                                           const GtkTextIter         *location,
                                           const gchar               *text,
                                           gint                       length,
                                           IdeBuffer                 *buffer)
{
  g_assert (GBP_IS_HISTORY_EDITOR_VIEW_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (location != NULL);
  g_assert (text != NULL);

  gbp_history_editor_view_addin_queue (self, gtk_text_iter_get_line (location));
}

static void
gbp_history_editor_view_addin_delete_range (GbpHistoryEditorViewAddin *self,
                                            const GtkTextIter         *begin,
                                            const GtkTextIter         *end,
                                            IdeBuffer                 *buffer)
{
  g_assert (GBP_IS_HISTORY_EDITOR_VIEW_ADDIN (self));
  g_assert (begin != NULL);
  g_assert (end != NULL);
  g_assert (IDE_IS_BUFFER (buffer));

  gbp_history_editor_view_addin_queue (self, gtk_text_iter_get_line (begin));
}

static void
gbp_history_editor_view_addin_load (IdeEditorViewAddin *addin,
                                    IdeEditorView      *view)
{
  GbpHistoryEditorViewAddin *self = (GbpHistoryEditorViewAddin *)addin;
  IdeSourceView *source_view;
  IdeBuffer *buffer;

  g_assert (GBP_IS_HISTORY_EDITOR_VIEW_ADDIN (self));
  g_assert (IDE_IS_EDITOR_VIEW (view));

  self->editor = view;

  buffer = ide_editor_view_get_buffer (view);
  source_view = ide_editor_view_get_view (view);

  self->last_change_count = ide_buffer_get_change_count (buffer);

  g_signal_connect_swapped (source_view,
                            "jump",
                            G_CALLBACK (gbp_history_editor_view_addin_jump),
                            addin);

  g_signal_connect_swapped (buffer,
                            "insert-text",
                            G_CALLBACK (gbp_history_editor_view_addin_insert_text),
                            self);

  g_signal_connect_swapped (buffer,
                            "delete-range",
                            G_CALLBACK (gbp_history_editor_view_addin_delete_range),
                            self);
}

static void
gbp_history_editor_view_addin_unload (IdeEditorViewAddin *addin,
                                      IdeEditorView      *view)
{
  GbpHistoryEditorViewAddin *self = (GbpHistoryEditorViewAddin *)addin;
  IdeSourceView *source_view;
  IdeBuffer *buffer;

  g_assert (GBP_IS_HISTORY_EDITOR_VIEW_ADDIN (self));
  g_assert (IDE_IS_EDITOR_VIEW (view));

  ide_clear_source (&self->queued_edit_source);

  source_view = ide_editor_view_get_view (view);
  buffer = ide_editor_view_get_buffer (view);

  g_signal_handlers_disconnect_by_func (source_view,
                                        G_CALLBACK (gbp_history_editor_view_addin_jump),
                                        self);

  g_signal_handlers_disconnect_by_func (buffer,
                                        G_CALLBACK (gbp_history_editor_view_addin_insert_text),
                                        self);

  g_signal_handlers_disconnect_by_func (buffer,
                                        G_CALLBACK (gbp_history_editor_view_addin_delete_range),
                                        self);

  ide_clear_weak_pointer (&self->stack_addin);

  self->editor = NULL;
}

static void
editor_view_addin_iface_init (IdeEditorViewAddinInterface *iface)
{
  iface->load = gbp_history_editor_view_addin_load;
  iface->unload = gbp_history_editor_view_addin_unload;
  iface->stack_set = gbp_history_editor_view_addin_stack_set;
}

G_DEFINE_TYPE_WITH_CODE (GbpHistoryEditorViewAddin, gbp_history_editor_view_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_EDITOR_VIEW_ADDIN,
                                                editor_view_addin_iface_init))

static void
gbp_history_editor_view_addin_class_init (GbpHistoryEditorViewAddinClass *klass)
{
}

static void
gbp_history_editor_view_addin_init (GbpHistoryEditorViewAddin *self)
{
}
