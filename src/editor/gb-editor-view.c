/* gb-editor-view.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "gb-editor-view"

#include <glib/gi18n.h>
#include <glib/gprintf.h>

#include "gb-editor-frame-private.h"
#include "gb-editor-view-actions.h"
#include "gb-editor-view-addin.h"
#include "gb-editor-view.h"
#include "gb-editor-view-addin-private.h"
#include "gb-editor-view-private.h"
#include "gb-string.h"
#include "gb-widget.h"

#define SYMBOL_UPDATE_SECS 10

G_DEFINE_TYPE (GbEditorView, gb_editor_view, GB_TYPE_VIEW)

enum {
  PROP_0,
  PROP_DOCUMENT,
  LAST_PROP
};

enum {
  REQUEST_DOCUMENTATION,
  LAST_SIGNAL
};

static GParamSpec *properties [LAST_PROP];
static guint signals [LAST_SIGNAL];

static GbDocument *
gb_editor_view_get_document (GbView *view)
{
  GbEditorView *self = (GbEditorView *)view;

  g_assert (GB_IS_EDITOR_VIEW (self));

  return GB_DOCUMENT (self->document);
}

static GbEditorFrame *
gb_editor_view_get_last_focused (GbEditorView *self)
{
  g_assert (self->last_focused_frame != NULL);

  return self->last_focused_frame;
}

static void
gb_editor_view_navigate_to (GbView            *view,
                            IdeSourceLocation *location)
{
  GbEditorView *self = (GbEditorView *)view;
  GbEditorFrame *frame;
  GtkTextMark *insert;
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  guint line;
  guint line_offset;

  IDE_ENTRY;

  g_assert (GB_IS_EDITOR_VIEW (self));
  g_assert (location != NULL);

  frame = gb_editor_view_get_last_focused (self);
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (frame->source_view));

  line = ide_source_location_get_line (location);
  line_offset = ide_source_location_get_line_offset (location);

  gtk_text_buffer_get_iter_at_line_offset (buffer, &iter, line, line_offset);

  gtk_text_buffer_select_range (buffer, &iter, &iter);

  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_view_scroll_to_mark (GTK_TEXT_VIEW (frame->source_view), insert, 0.0, TRUE, 1.0, 0.5);

  g_signal_emit_by_name (frame->source_view, "save-insert-mark");

  IDE_EXIT;
}

static gboolean
language_to_string (GBinding     *binding,
                    const GValue *from_value,
                    GValue       *to_value,
                    gpointer      user_data)
{
  GtkSourceLanguage *language;

  language = g_value_get_object (from_value);

  if (language != NULL)
    g_value_set_string (to_value, gtk_source_language_get_name (language));
  else
    g_value_set_string (to_value, _("Plain Text"));

  return TRUE;
}

static gboolean
gb_editor_view_get_modified (GbView *view)
{
  GbEditorView *self = (GbEditorView *)view;

  g_assert (GB_IS_EDITOR_VIEW (self));

  return gtk_text_buffer_get_modified (GTK_TEXT_BUFFER (self->document));
}

static void
gb_editor_view__buffer_modified_changed (GbEditorView  *self,
                                         GParamSpec    *pspec,
                                         GtkTextBuffer *buffer)
{
  g_assert (GB_IS_EDITOR_VIEW (self));

  g_object_notify (G_OBJECT (self), "modified");
}

static void
force_scroll_to_top (IdeSourceView *source_view)
{
  GtkAdjustment *vadj;
  GtkAdjustment *hadj;
  gdouble lower;

  /*
   * FIXME:
   *
   * See the comment in gb_editor_view__buffer_changed_on_volume()
   */

  vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (source_view));
  hadj = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (source_view));

  lower = gtk_adjustment_get_lower (vadj);
  gtk_adjustment_set_value (vadj, lower);

  lower = gtk_adjustment_get_lower (hadj);
  gtk_adjustment_set_value (hadj, lower);
}

static gboolean
no_really_scroll_to_the_top (gpointer data)
{
  g_autoptr(GbEditorView) self = data;

  force_scroll_to_top (self->frame1->source_view);
  if (self->frame2 != NULL)
    force_scroll_to_top (self->frame2->source_view);

  return G_SOURCE_REMOVE;
}

static void
gb_editor_view__buffer_changed_on_volume (GbEditorView *self,
                                          GParamSpec   *pspec,
                                          IdeBuffer    *buffer)
{
  g_assert (GB_IS_EDITOR_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));

  if (ide_buffer_get_changed_on_volume (buffer))
    gtk_revealer_set_reveal_child (self->modified_revealer, TRUE);
  else if (gtk_revealer_get_reveal_child (self->modified_revealer))
    {
      GtkTextIter iter;

      gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &iter);
      gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer), &iter, &iter);

      /*
       * FIXME:
       *
       * Without this delay, I see a condition with split view where the
       * non-focused split will just render blank. Well that isn't totally
       * correct, it renders empty gutters and proper line grid background. But
       * no textual content. And the adjustment is way out of sync. Even
       * changing the adjustment manually doesn't help. So whatever, I'll
       * insert a short delay and we'll pick up after the textview has
       * stablized.
       */
      g_timeout_add (10, no_really_scroll_to_the_top, g_object_ref (self));

      gtk_revealer_set_reveal_child (self->modified_revealer, FALSE);
    }
}

static const gchar *
gb_editor_view_get_special_title (GbView *view)
{
  return ((GbEditorView *)view)->title;
}

static void
gb_editor_view__buffer_notify_title (GbEditorView *self,
                                     GParamSpec   *pspec,
                                     IdeBuffer    *buffer)
{
  const gchar *title;
  gchar **parts;
  gboolean needs_prefix;
  gchar *str;

  g_assert (GB_IS_EDITOR_VIEW (self));
  g_assert (GB_IS_EDITOR_DOCUMENT (buffer));

  g_free (self->title);

  title = ide_buffer_get_title (buffer);

  if (title == NULL)
    {
      /* translators: this shouldn't ever happen */
      self->title = g_strdup ("untitled");
      return;
    }

  if ((needs_prefix = (title [0] == G_DIR_SEPARATOR)))
    title++;

  parts = g_strsplit (title, G_DIR_SEPARATOR_S, 0);
  str = g_strjoinv (" "G_DIR_SEPARATOR_S" ", parts);

  if (needs_prefix)
    {
      self->title = g_strdup_printf (G_DIR_SEPARATOR_S" %s", str);
      g_free (str);
    }
  else
    {
      self->title = str;
    }

  g_strfreev (parts);

  g_object_notify (G_OBJECT (self), "title");
}

static void
notify_language_foreach (PeasExtensionSet *set,
                         PeasPluginInfo   *plugin_info,
                         PeasExtension    *exten,
                         gpointer          user_data)
{
  const gchar *language_id = user_data;

  gb_editor_view_addin_language_changed (GB_EDITOR_VIEW_ADDIN (exten), language_id);
}

static void
gb_editor_view__buffer_notify_language (GbEditorView     *self,
                                        GParamSpec       *pspec,
                                        GbEditorDocument *document)
{
  g_assert (GB_IS_EDITOR_VIEW (self));
  g_assert (GB_IS_EDITOR_DOCUMENT (document));

  if (self->extensions != NULL)
    {
      GtkSourceLanguage *language;
      const gchar *language_id;

      language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (document));
      language_id = language ? gtk_source_language_get_id (language) : NULL;

      peas_extension_set_foreach (self->extensions,
                                  notify_language_foreach,
                                  (gchar *)language_id);
    }
}

static void
gb_editor_view__buffer_cursor_moved (GbEditorView      *self,
                                     const GtkTextIter *iter,
                                     GtkTextBuffer     *buffer)
{
  GtkTextIter bounds;
  GtkTextMark *mark;
  gchar *str;
  guint line;
  gint column;
  gint column2;

  g_assert (GB_IS_EDITOR_VIEW (self));
  g_assert (iter != NULL);
  g_assert (IDE_IS_BUFFER (buffer));

  ide_source_view_get_visual_position (self->frame1->source_view, &line, (guint *)&column);

  mark = gtk_text_buffer_get_selection_bound (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &bounds, mark);

  if (!gtk_widget_has_focus (GTK_WIDGET (self->frame1->source_view)) ||
      gtk_text_iter_equal (&bounds, iter) ||
      (gtk_text_iter_get_line (iter) != gtk_text_iter_get_line (&bounds)))
    {
      str = g_strdup_printf ("%d:%d", line + 1, column + 1);
      gtk_label_set_text (self->cursor_label, str);
      g_free (str);
      return;
    }

  /* We have a selection that is on the same line.
   * Lets give some detail as to how long the selection is.
   */
  column2 = gtk_source_view_get_visual_column (GTK_SOURCE_VIEW (self->frame1->source_view),
                                               &bounds);
  str = g_strdup_printf ("%d:%d (%d)", line + 1, column + 1, ABS (column2 - column));
  gtk_label_set_text (self->cursor_label, str);
  g_free (str);
}

static void
gb_editor_view_set_document (GbEditorView     *self,
                             GbEditorDocument *document)
{
  g_return_if_fail (GB_IS_EDITOR_VIEW (self));
  g_return_if_fail (GB_IS_EDITOR_DOCUMENT (document));

  if (g_set_object (&self->document, document))
    {
      if (self->frame1)
        gb_editor_frame_set_document (self->frame1, document);

      if (self->frame2)
        gb_editor_frame_set_document (self->frame2, document);

      g_settings_bind (self->settings, "style-scheme-name",
                       document, "style-scheme-name",
                       G_SETTINGS_BIND_GET);
      g_settings_bind (self->settings, "highlight-matching-brackets",
                       document, "highlight-matching-brackets",
                       G_SETTINGS_BIND_GET);

      g_signal_connect_object (document,
                               "cursor-moved",
                               G_CALLBACK (gb_editor_view__buffer_cursor_moved),
                               self,
                               G_CONNECT_SWAPPED);

      g_object_bind_property_full (document, "language", self->tweak_button,
                                   "label", G_BINDING_SYNC_CREATE,
                                   language_to_string, NULL, NULL, NULL);

      g_signal_connect_object (document,
                               "modified-changed",
                               G_CALLBACK (gb_editor_view__buffer_modified_changed),
                               self,
                               G_CONNECT_SWAPPED);

      g_signal_connect_object (document,
                               "notify::title",
                               G_CALLBACK (gb_editor_view__buffer_notify_title),
                               self,
                               G_CONNECT_SWAPPED);

      g_signal_connect_object (document,
                               "notify::language",
                               G_CALLBACK (gb_editor_view__buffer_notify_language),
                               self,
                               G_CONNECT_SWAPPED);

      g_signal_connect_object (document,
                               "notify::changed-on-volume",
                               G_CALLBACK (gb_editor_view__buffer_changed_on_volume),
                               self,
                               G_CONNECT_SWAPPED);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DOCUMENT]);

      g_object_bind_property (document, "has-diagnostics",
                              self->warning_button, "visible",
                              G_BINDING_SYNC_CREATE);

      gb_editor_view__buffer_notify_language (self, NULL, document);
      gb_editor_view__buffer_notify_title (self, NULL, IDE_BUFFER (document));

      gb_editor_view_actions_update (self);
    }
}

static GbView *
gb_editor_view_create_split (GbView *view)
{
  GbEditorView *self = (GbEditorView *)view;
  GbView *ret;

  g_assert (GB_IS_EDITOR_VIEW (self));

  ret = g_object_new (GB_TYPE_EDITOR_VIEW,
                      "document", self->document,
                      "visible", TRUE,
                      NULL);

  return ret;
}

static void
gb_editor_view_grab_focus (GtkWidget *widget)
{
  GbEditorView *self = (GbEditorView *)widget;

  g_assert (GB_IS_EDITOR_VIEW (self));
  g_assert (GB_IS_EDITOR_FRAME (self->last_focused_frame));

  gtk_widget_grab_focus (GTK_WIDGET (self->last_focused_frame->source_view));
}

static void
gb_editor_view_request_documentation (GbEditorView  *self,
                                      IdeSourceView *source_view)
{
  g_autofree gchar *word = NULL;
  IdeBuffer *buffer;
  GtkTextMark *mark;
  GtkTextIter iter;

  g_assert (GB_IS_EDITOR_VIEW (self));
  g_assert (IDE_IS_SOURCE_VIEW (source_view));

  buffer = IDE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (source_view)));
  mark = gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (buffer));
  gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer), &iter, mark);

  word = ide_buffer_get_word_at_iter (buffer, &iter);

  g_signal_emit (self, signals [REQUEST_DOCUMENTATION], 0, word);
}

static void
gb_editor_view__focused_frame_weak_notify (gpointer  data,
                                           GObject  *object)
{
  GbEditorView  *self = data;

  g_assert (GB_IS_EDITOR_VIEW (self));

  self->last_focused_frame = self->frame1;
}

static gboolean
gb_editor_view__focus_in_event (GbEditorView  *self,
                                GdkEvent      *event,
                                IdeSourceView *source_view)
{
  g_assert (GB_IS_EDITOR_VIEW (self));
  g_assert (IDE_IS_SOURCE_VIEW (source_view));

  if (self->last_focused_frame && self->last_focused_frame->source_view == source_view)
      return FALSE;

  if (self->frame2 && self->frame2->source_view == source_view)
    {
      self->last_focused_frame = self->frame2;
      g_object_weak_ref (G_OBJECT (self->frame2), gb_editor_view__focused_frame_weak_notify, self);
    }
  else
    {
      g_object_weak_unref (G_OBJECT (self->frame2), gb_editor_view__focused_frame_weak_notify, self);
      self->last_focused_frame = self->frame1;
    }

  return FALSE;
}

static void
gb_editor_view_set_split_view (GbView   *view,
                               gboolean  split_view)
{
  GbEditorView *self = (GbEditorView *)view;

  g_assert (GB_IS_EDITOR_VIEW (self));

  if (split_view && (self->frame2 != NULL))
    return;

  if (!split_view && (self->frame2 == NULL))
    return;

  if (split_view)
    {
      self->frame2 = g_object_new (GB_TYPE_EDITOR_FRAME,
                                   "show-ruler", TRUE,
                                   "document", self->document,
                                   "visible", TRUE,
                                   NULL);
      g_signal_connect_object (self->frame2->source_view,
                               "request-documentation",
                               G_CALLBACK (gb_editor_view_request_documentation),
                               self,
                               G_CONNECT_SWAPPED);

      g_signal_connect_object (self->frame2->source_view,
                               "focus-in-event",
                               G_CALLBACK (gb_editor_view__focus_in_event),
                               self,
                               G_CONNECT_SWAPPED);

      gtk_container_add_with_properties (GTK_CONTAINER (self->paned), GTK_WIDGET (self->frame2),
                                         "shrink", FALSE,
                                         "resize", TRUE,
                                         NULL);
      gtk_widget_grab_focus (GTK_WIDGET (self->frame2));
    }
  else
    {
      GtkWidget *copy = GTK_WIDGET (self->frame2);

      self->frame2 = NULL;
      gtk_container_remove (GTK_CONTAINER (self->paned), copy);
      gtk_widget_grab_focus (GTK_WIDGET (self->frame1));
    }
}

static void
gb_editor_view_set_back_forward_list (GbView             *view,
                                      IdeBackForwardList *back_forward_list)
{
  GbEditorView *self = (GbEditorView *)view;

  g_assert (GB_IS_VIEW (view));
  g_assert (IDE_IS_BACK_FORWARD_LIST (back_forward_list));

  g_object_set (self->frame1, "back-forward-list", back_forward_list, NULL);
  if (self->frame2)
    g_object_set (self->frame2, "back-forward-list", back_forward_list, NULL);
}

static void
gb_editor_view_hide_reload_bar (GbEditorView *self,
                                GtkWidget    *button)
{
  g_assert (GB_IS_EDITOR_VIEW (self));

  gtk_revealer_set_reveal_child (self->modified_revealer, FALSE);
}

static GtkSizeRequestMode
gb_editor_view_get_request_mode (GtkWidget *widget)
{
  return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static void
gb_editor_view_get_preferred_height (GtkWidget *widget,
                                     gint      *min_height,
                                     gint      *nat_height)
{
  /*
   * FIXME: Workaround GtkStack changes.
   *
   * This can probably be removed once upstream changes land.
   *
   * This ignores our potential giant size requests since we don't actually
   * care about keeping our size requests between animated transitions in
   * the stack.
   */
  GTK_WIDGET_CLASS (gb_editor_view_parent_class)->get_preferred_height (widget, min_height, nat_height);
  *nat_height = *min_height;
}

static void
gb_editor_view_goto_line_activate (GbEditorView    *self,
                                   const gchar     *text,
                                   GbSimplePopover *popover)
{
  gint64 value;

  g_assert (GB_IS_EDITOR_VIEW (self));
  g_assert (GB_IS_SIMPLE_POPOVER (popover));

  if (!ide_str_empty0 (text))
    {
      value = g_ascii_strtoll (text, NULL, 10);

      if ((value > 0) && (value < G_MAXINT))
        {
          GtkTextIter iter;
          GtkTextBuffer *buffer = GTK_TEXT_BUFFER (self->document);

          gtk_widget_grab_focus (GTK_WIDGET (self->frame1->source_view));
          gtk_text_buffer_get_iter_at_line (buffer, &iter, value - 1);
          gtk_text_buffer_select_range (buffer, &iter, &iter);
          ide_source_view_scroll_to_iter (self->frame1->source_view,
                                          &iter, 0.25, TRUE, 1.0, 0.5, TRUE);
        }
    }
}

static gboolean
gb_editor_view_goto_line_insert_text (GbEditorView    *self,
                                      guint            position,
                                      const gchar     *chars,
                                      guint            n_chars,
                                      GbSimplePopover *popover)
{
  g_assert (GB_IS_EDITOR_VIEW (self));
  g_assert (GB_IS_SIMPLE_POPOVER (popover));
  g_assert (chars != NULL);

  for (; *chars; chars = g_utf8_next_char (chars))
    {
      if (!g_unichar_isdigit (g_utf8_get_char (chars)))
        return GDK_EVENT_STOP;
    }

  return GDK_EVENT_PROPAGATE;
}

static void
gb_editor_view_goto_line_changed (GbEditorView    *self,
                                  GbSimplePopover *popover)
{
  gchar *message;
  const gchar *text;
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (GB_IS_EDITOR_VIEW (self));
  g_assert (GB_IS_SIMPLE_POPOVER (popover));

  text = gb_simple_popover_get_text (popover);

  gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (self->document), &begin, &end);

  if (!ide_str_empty0 (text))
    {
      gint64 value;

      value = g_ascii_strtoll (text, NULL, 10);

      if (value > 0)
        {
          if (value <= gtk_text_iter_get_line (&end) + 1)
            {
              gb_simple_popover_set_message (popover, NULL);
              gb_simple_popover_set_ready (popover, TRUE);
              return;
            }

        }
    }

  /* translators: the user selected a number outside the value range for the document. */
  message = g_strdup_printf (_("Provide a number between 1 and %u"),
                             gtk_text_iter_get_line (&end) + 1);
  gb_simple_popover_set_message (popover, message);
  gb_simple_popover_set_ready (popover, FALSE);

  g_free (message);
}

static void
gb_editor_view__extension_added (PeasExtensionSet  *set,
                                 PeasPluginInfo    *info,
                                 GbEditorViewAddin *addin,
                                 GbEditorView      *self)
{
  GbDocument *document;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (info != NULL);
  g_assert (GB_IS_EDITOR_VIEW_ADDIN (addin));
  g_assert (GB_IS_EDITOR_VIEW (self));

  gb_editor_view_addin_load (addin, self);

  document = gb_view_get_document (GB_VIEW (self));

  if (document != NULL)
    {
      GtkSourceLanguage *language;

      language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (document));

      if (language != NULL)
        {
          const gchar *language_id;

          language_id = gtk_source_language_get_id (language);
          gb_editor_view_addin_language_changed (addin, language_id);
        }
    }
}

static void
gb_editor_view__extension_removed (PeasExtensionSet  *set,
                                   PeasPluginInfo    *info,
                                   GbEditorViewAddin *addin,
                                   GbEditorView      *self)
{
  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (info != NULL);
  g_assert (GB_IS_EDITOR_VIEW_ADDIN (addin));
  g_assert (GB_IS_EDITOR_VIEW (self));

  gb_editor_view_addin_unload (addin, self);
}

static void
gb_editor_view_warning_button_clicked (GbEditorView *self,
                                       GtkButton    *button)
{
  GbEditorFrame *frame;

  g_assert (GB_IS_EDITOR_VIEW (self));
  g_assert (GTK_IS_BUTTON (button));

  frame = gb_editor_view_get_last_focused (self);
  gtk_widget_grab_focus (GTK_WIDGET (frame));
  g_signal_emit_by_name (frame->source_view, "move-error", GTK_DIR_DOWN);
}

static void
gb_editor_view_constructed (GObject *object)
{
  GbEditorView *self = (GbEditorView *)object;
  PeasEngine *engine;

  G_OBJECT_CLASS (gb_editor_view_parent_class)->constructed (object);

  engine = peas_engine_get_default ();
  self->extensions = peas_extension_set_new (engine, GB_TYPE_EDITOR_VIEW_ADDIN, NULL);
  g_signal_connect_object (self->extensions,
                           "extension-added",
                           G_CALLBACK (gb_editor_view__extension_added),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->extensions,
                           "extension-added",
                           G_CALLBACK (gb_editor_view__extension_removed),
                           self,
                           G_CONNECT_SWAPPED);
  peas_extension_set_foreach (self->extensions,
                              (PeasExtensionSetForeachFunc)gb_editor_view__extension_added,
                              self);
}

static void
gb_editor_view_destroy (GtkWidget *widget)
{
  GbEditorView *self = (GbEditorView *)widget;

  GTK_WIDGET_CLASS (gb_editor_view_parent_class)->destroy (widget);

  g_clear_object (&self->document);
}

static void
gb_editor_view_finalize (GObject *object)
{
  GbEditorView *self = (GbEditorView *)object;

  g_clear_pointer (&self->title, g_free);
  g_clear_object (&self->extensions);
  g_clear_object (&self->document);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (gb_editor_view_parent_class)->finalize (object);
}

static void
gb_editor_view_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GbEditorView *self = GB_EDITOR_VIEW (object);

  switch (prop_id)
    {
    case PROP_DOCUMENT:
      g_value_set_object (value, gb_editor_view_get_document (GB_VIEW (self)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_editor_view_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GbEditorView *self = GB_EDITOR_VIEW (object);

  switch (prop_id)
    {
    case PROP_DOCUMENT:
      gb_editor_view_set_document (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_editor_view_class_init (GbEditorViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GbViewClass *view_class = GB_VIEW_CLASS (klass);

  object_class->constructed = gb_editor_view_constructed;
  object_class->finalize = gb_editor_view_finalize;
  object_class->get_property = gb_editor_view_get_property;
  object_class->set_property = gb_editor_view_set_property;

  widget_class->destroy = gb_editor_view_destroy;
  widget_class->grab_focus = gb_editor_view_grab_focus;
  widget_class->get_request_mode = gb_editor_view_get_request_mode;
  widget_class->get_preferred_height = gb_editor_view_get_preferred_height;

  view_class->create_split = gb_editor_view_create_split;
  view_class->get_document = gb_editor_view_get_document;
  view_class->get_special_title = gb_editor_view_get_special_title;
  view_class->get_modified = gb_editor_view_get_modified;
  view_class->set_split_view = gb_editor_view_set_split_view;
  view_class->set_back_forward_list = gb_editor_view_set_back_forward_list;
  view_class->navigate_to = gb_editor_view_navigate_to;

  properties [PROP_DOCUMENT] =
    g_param_spec_object ("document",
                         "Document",
                         "The editor document.",
                         GB_TYPE_EDITOR_DOCUMENT,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  signals [REQUEST_DOCUMENTATION] =
    g_signal_new ("request-documentation",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_STRING);

  GB_WIDGET_CLASS_TEMPLATE (klass, "gb-editor-view.ui");

  GB_WIDGET_CLASS_BIND (klass, GbEditorView, cursor_label);
  GB_WIDGET_CLASS_BIND (klass, GbEditorView, frame1);
  GB_WIDGET_CLASS_BIND (klass, GbEditorView, modified_cancel_button);
  GB_WIDGET_CLASS_BIND (klass, GbEditorView, modified_revealer);
  GB_WIDGET_CLASS_BIND (klass, GbEditorView, paned);
  GB_WIDGET_CLASS_BIND (klass, GbEditorView, progress_bar);
  GB_WIDGET_CLASS_BIND (klass, GbEditorView, tweak_button);
  GB_WIDGET_CLASS_BIND (klass, GbEditorView, tweak_widget);
  GB_WIDGET_CLASS_BIND (klass, GbEditorView, goto_line_button);
  GB_WIDGET_CLASS_BIND (klass, GbEditorView, goto_line_popover);
  GB_WIDGET_CLASS_BIND (klass, GbEditorView, warning_button);

  g_type_ensure (GB_TYPE_EDITOR_FRAME);
  g_type_ensure (GB_TYPE_EDITOR_TWEAK_WIDGET);
  g_type_ensure (GB_TYPE_SIMPLE_POPOVER);
}

static void
gb_editor_view_init (GbEditorView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->settings = g_settings_new ("org.gnome.builder.editor");
  self->last_focused_frame = self->frame1;

  gb_editor_view_actions_init (self);

  /*
   * XXX: Refactor all of this.
   *
   * In frame1, we don't show the floating bar, so no need to alter the
   * editor map allocation.
   */
  g_object_set (self->frame1->source_map_container,
                "floating-bar", NULL,
                NULL);

  g_signal_connect_object (self->modified_cancel_button,
                           "clicked",
                           G_CALLBACK (gb_editor_view_hide_reload_bar),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->frame1->source_view,
                           "request-documentation",
                           G_CALLBACK (gb_editor_view_request_documentation),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->frame1->source_view,
                           "focus-in-event",
                           G_CALLBACK (gb_editor_view__focus_in_event),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->goto_line_popover,
                           "activate",
                           G_CALLBACK (gb_editor_view_goto_line_activate),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->goto_line_popover,
                           "insert-text",
                           G_CALLBACK (gb_editor_view_goto_line_insert_text),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->goto_line_popover,
                           "changed",
                           G_CALLBACK (gb_editor_view_goto_line_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->warning_button,
                           "clicked",
                           G_CALLBACK (gb_editor_view_warning_button_clicked),
                           self,
                           G_CONNECT_SWAPPED);
}
