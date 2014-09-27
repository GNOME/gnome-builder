/* gb-editor-tab.c
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

#define G_LOG_DOMAIN "editor"

#include <glib/gi18n.h>

#include "gb-editor-tab.h"
#include "gb-editor-tab-private.h"
#include "gb-log.h"
#include "gb-rgba.h"
#include "gb-source-change-gutter-renderer.h"
#include "gb-source-highlight-menu.h"
#include "gb-source-snippet.h"
#include "gb-source-snippets-manager.h"
#include "gb-source-snippets.h"
#include "gb-string.h"
#include "gb-widget.h"
#include "gb-workbench.h"

#define GB_EDITOR_TAB_UI_RESOURCE "/org/gnome/builder/ui/gb-editor-tab.ui"

enum {
  PROP_0,
  PROP_DOCUMENT,
  PROP_FILE,
  PROP_FONT_DESC,
  PROP_SETTINGS,
  LAST_PROP
};

G_DEFINE_TYPE_WITH_PRIVATE (GbEditorTab, gb_editor_tab, GB_TYPE_TAB)

static GParamSpec *gParamSpecs[LAST_PROP];

GtkWidget *
gb_editor_tab_new (void)
{
  return g_object_new (GB_TYPE_EDITOR_TAB, NULL);
}

/**
 * gb_editor_tab_get_is_default:
 * @tab: A #GbEditorTab.
 *
 * Returns #TRUE if the tab has not been modified since being created
 * from an empty state. This means the tab is a candidate to be
 * dropped or repurposed for loading a new file.
 *
 * Returns: #TRUE if tab is in default state.
 */
gboolean
gb_editor_tab_get_is_default (GbEditorTab *tab)
{
  GbEditorTabPrivate *priv;
  GtkTextIter begin;
  GtkTextIter end;

  g_return_val_if_fail (GB_IS_EDITOR_TAB (tab), FALSE);

  priv = tab->priv;

  if (gtk_source_file_get_location (priv->file))
    return FALSE;

  if (gtk_text_buffer_get_modified (GTK_TEXT_BUFFER (priv->document)))
    return FALSE;

  gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (priv->document), &begin, &end);

  if (gtk_text_iter_compare (&begin, &end) != 0)
    return FALSE;

  return TRUE;
}

/**
 * gb_editor_tab_get_document:
 * @tab: A #GbEditorTab.
 *
 * Fetches the document for the tab.
 *
 * Returns: (transfer none): A #GbEditorDocument.
 */
GbEditorDocument *
gb_editor_tab_get_document (GbEditorTab *tab)
{
  g_return_val_if_fail (GB_IS_EDITOR_TAB (tab), NULL);

  return tab->priv->document;
}

/**
 * gb_editor_tab_get_file:
 * @tab: A #GbEditorTab.
 *
 * Returns the current file for this tab, if there is one.
 * If no file has been specified, then NULL is returned.
 *
 * Returns: (transfer none): A #GtkSourceFile.
 */
GtkSourceFile *
gb_editor_tab_get_file (GbEditorTab *tab)
{
  g_return_val_if_fail (GB_IS_EDITOR_TAB (tab), NULL);

  return tab->priv->file;
}

static void
gb_editor_tab_reload_snippets (GbEditorTab       *tab,
                               GtkSourceLanguage *language)
{
  GbSourceSnippetsManager *manager;
  GbEditorTabPrivate *priv;
  GbSourceSnippets *snippets = NULL;

  g_return_if_fail (GB_IS_EDITOR_TAB (tab));

  priv = tab->priv;

  if (language)
    {
      manager = gb_source_snippets_manager_get_default ();
      snippets = gb_source_snippets_manager_get_for_language (manager,
                                                              language);
    }

  g_object_set (priv->snippets_provider, "snippets", snippets, NULL);
}

static void
gb_editor_tab_connect_settings (GbEditorTab      *tab,
                                GbEditorSettings *settings)
{
  GbEditorTabPrivate *priv;

  ENTRY;

  g_return_if_fail (GB_IS_EDITOR_TAB (tab));
  g_return_if_fail (GB_IS_EDITOR_SETTINGS (settings));

  priv = tab->priv;

#define ADD_BINDING(name, dst, prop, loc) \
  G_STMT_START { \
    (loc) = g_object_bind_property (settings, name, (dst), \
                                    prop, G_BINDING_SYNC_CREATE); \
    g_object_add_weak_pointer (G_OBJECT ((loc)), (gpointer *) &(loc)); \
  } G_STMT_END

  ADD_BINDING ("auto-indent", priv->source_view, "auto-indent",
               priv->auto_indent_binding);
  ADD_BINDING ("highlight-current-line", priv->source_view,
               "highlight-current-line",
               priv->highlight_current_line_binding);
  ADD_BINDING ("indent-on-tab", priv->source_view, "indent-on-tab",
               priv->indent_on_tab_binding);
  ADD_BINDING ("insert-spaces-instead-of-tabs", priv->source_view,
               "insert-spaces-instead-of-tabs",
               priv->insert_spaces_instead_of_tabs_binding);
  ADD_BINDING ("show-line-marks", priv->source_view, "show-line-marks",
               priv->show_line_marks_binding);
  ADD_BINDING ("show-line-numbers", priv->source_view, "show-line-numbers",
               priv->show_line_numbers_binding);
  ADD_BINDING ("show-right-margin", priv->source_view, "show-right-margin",
               priv->show_right_margin_binding);
  ADD_BINDING ("smart-home-end", priv->source_view, "smart-home-end",
               priv->smart_home_end_binding);
  ADD_BINDING ("indent-width", priv->source_view, "indent-width",
               priv->indent_width_binding);
  ADD_BINDING ("tab-width", priv->source_view, "tab-width",
               priv->tab_width_binding);
  ADD_BINDING ("right-margin-position", priv->source_view,
               "right-margin-position", priv->right_margin_position_binding);
  ADD_BINDING ("font-desc", tab, "font-desc", priv->font_desc_binding);
  ADD_BINDING ("style-scheme", priv->document, "style-scheme",
               priv->style_scheme_binding);

#undef ADD_BINDING

  EXIT;
}

static void
gb_editor_tab_disconnect_settings (GbEditorTab *tab)
{
  GbEditorTabPrivate *priv;

  ENTRY;

  g_assert (GB_IS_EDITOR_TAB (tab));

  priv = tab->priv;

#define REMOVE_BINDING(b) \
  if (b) { \
      g_object_remove_weak_pointer (G_OBJECT (b), (gpointer *) &(b)); \
      g_binding_unbind (b); \
      (b) = NULL; \
    }

  REMOVE_BINDING (priv->auto_indent_binding);
  REMOVE_BINDING (priv->highlight_current_line_binding);
  REMOVE_BINDING (priv->indent_on_tab_binding);
  REMOVE_BINDING (priv->insert_spaces_instead_of_tabs_binding);
  REMOVE_BINDING (priv->show_line_marks_binding);
  REMOVE_BINDING (priv->show_line_numbers_binding);
  REMOVE_BINDING (priv->show_right_margin_binding);
  REMOVE_BINDING (priv->smart_home_end_binding);
  REMOVE_BINDING (priv->indent_width_binding);
  REMOVE_BINDING (priv->tab_width_binding);
  REMOVE_BINDING (priv->right_margin_position_binding);
  REMOVE_BINDING (priv->font_desc_binding);

#undef REMOVE_BINDING

  EXIT;
}

/**
 * gb_editor_tab_get_settings:
 * @tab: A #GbEditorTab.
 *
 * Fetches the settings used on the editor tab.
 *
 * Returns: (transfer none): A #GbEditorSettings.
 */
GbEditorSettings *
gb_editor_tab_get_settings (GbEditorTab *tab)
{
  g_return_val_if_fail (GB_IS_EDITOR_TAB (tab), NULL);

  return tab->priv->settings;
}

/**
 * gb_editor_tab_set_settings:
 * @tab: A #GbEditorTab.
 *
 * Sets the settings to use for this tab, including tab spacing and
 * style schemes.
 */
void
gb_editor_tab_set_settings (GbEditorTab      *tab,
                            GbEditorSettings *settings)
{
  GbEditorTabPrivate *priv;

  ENTRY;

  g_return_if_fail (GB_IS_EDITOR_TAB (tab));
  g_return_if_fail (!settings || GB_IS_EDITOR_SETTINGS (settings));

  priv = tab->priv;

  if (priv->settings)
    {
      gb_editor_tab_disconnect_settings (tab);
      g_clear_object (&priv->settings);
    }

  if (settings)
    priv->settings = g_object_ref (settings);
  else
    priv->settings = g_object_new (GB_TYPE_EDITOR_SETTINGS, NULL);

  gb_editor_tab_connect_settings (tab, priv->settings);

  EXIT;
}

static void
set_search_position_label (GbEditorTab *tab,
                           const gchar *text)
{
  GbEditorTabPrivate *priv;

  g_return_if_fail (GB_IS_EDITOR_TAB (tab));

  priv = tab->priv;

  if (!text || !*text)
    {
      if (priv->search_entry_tag)
        {
          gd_tagged_entry_remove_tag (priv->search_entry,
                                      priv->search_entry_tag);
          g_clear_object (&priv->search_entry_tag);
        }
      return;
    }

  if (!priv->search_entry_tag)
    {
      priv->search_entry_tag = gd_tagged_entry_tag_new ("");
      gd_tagged_entry_tag_set_style (priv->search_entry_tag,
                                     "gb-search-entry-occurrences-tag");
      gd_tagged_entry_add_tag (priv->search_entry,
                               priv->search_entry_tag);
    }

  gd_tagged_entry_tag_set_label (priv->search_entry_tag, text);
}

static void
update_search_position_label (GbEditorTab *tab)
{
  GbEditorTabPrivate *priv;
  GtkStyleContext *context;
  GtkTextIter begin;
  GtkTextIter end;
  const gchar *search_text;
  gchar *text;
  gint count;
  gint pos;

  g_assert (GB_IS_EDITOR_TAB (tab));

  priv = tab->priv;

  gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (priv->document),
                                        &begin, &end);
  pos = gtk_source_search_context_get_occurrence_position (
    priv->search_context, &begin, &end);
  count = gtk_source_search_context_get_occurrences_count (
    priv->search_context);

  if ((pos == -1) || (count == -1))
    {
      /*
       * We are not yet done scanning the buffer.
       * We will be updated when we know more, so just hide it for now.
       */
      set_search_position_label (tab, NULL);
      return;
    }

  context = gtk_widget_get_style_context (GTK_WIDGET (priv->search_entry));
  search_text = gtk_entry_get_text (GTK_ENTRY (priv->search_entry));

  if ((count == 0) && !gb_str_empty0 (search_text))
    gtk_style_context_add_class (context, GTK_STYLE_CLASS_ERROR);
  else
    gtk_style_context_remove_class (context, GTK_STYLE_CLASS_ERROR);

  text = g_strdup_printf (_ ("%u of %u"), pos, count);
  set_search_position_label (tab, text);
  g_free (text);
}

static void
on_search_occurrences_notify (GbEditorTab            *tab,
                              GParamSpec             *pspec,
                              GtkSourceSearchContext *search_context)
{
  g_return_if_fail (GB_IS_EDITOR_TAB (tab));
  g_return_if_fail (GTK_SOURCE_IS_SEARCH_CONTEXT (search_context));

  update_search_position_label (tab);
}

static void
gb_editor_tab_language_changed (GbEditorTab      *tab,
                                GParamSpec       *pspec,
                                GbEditorDocument *document)
{
  GbSourceAutoIndenter *indenter = NULL;
  GtkSourceLanguage *language;

  g_return_if_fail (GB_IS_EDITOR_TAB (tab));
  g_return_if_fail (GB_IS_EDITOR_DOCUMENT (document));

  language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (document));

  if (language)
    {
      const gchar *lang_id = gtk_source_language_get_id (language);

      if ((g_strcmp0 (lang_id, "c") == 0) ||
          (g_strcmp0 (lang_id, "chdr") == 0))
        indenter = gb_source_auto_indenter_c_new ();
    }

  gb_source_view_set_auto_indenter (tab->priv->source_view, indenter);
  g_clear_object (&indenter);

  gb_editor_tab_reload_snippets (tab, language);

}


static void
gb_editor_tab_cursor_moved (GbEditorTab      *tab,
                            GbEditorDocument *document)
{
  GtkSourceView *source_view;
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkTextMark *mark;
  gchar *text;
  guint ln;
  guint col;

  g_return_if_fail (GB_IS_EDITOR_TAB (tab));
  g_return_if_fail (GB_IS_EDITOR_DOCUMENT (document));

  source_view = GTK_SOURCE_VIEW (tab->priv->source_view);
  buffer = GTK_TEXT_BUFFER (document);

  mark = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);

  ln = gtk_text_iter_get_line (&iter);
  col = gtk_source_view_get_visual_column (source_view, &iter);

  text = g_strdup_printf (_ ("Line %u, Column %u"), ln + 1, col + 1);
  nautilus_floating_bar_set_primary_label (tab->priv->floating_bar, text);
  g_free (text);

  update_search_position_label (tab);
}

static void
gb_editor_tab_modified_changed (GbEditorTab   *tab,
                                GtkTextBuffer *buffer)
{
  gboolean dirty;

  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (GB_IS_EDITOR_TAB (tab));

  dirty = gtk_text_buffer_get_modified (buffer);
  gb_tab_set_dirty (GB_TAB (tab), dirty);
}

void
gb_editor_tab_set_font_desc (GbEditorTab                *tab,
                             const PangoFontDescription *font_desc)
{
  g_return_if_fail (GB_IS_EDITOR_TAB (tab));

  gtk_widget_override_font (GTK_WIDGET (tab->priv->source_view), font_desc);
}

static void
gb_editor_tab_freeze_drag (GbTab *tab)
{
  GbEditorTab *editor = (GbEditorTab *) tab;

  ENTRY;

  g_return_if_fail (GB_IS_EDITOR_TAB (editor));

  /*
   * WORKAROUND:
   *
   * Unset drag 'n drop for the source view so that it doesn't
   * highjack drop target signals when what we really want is to
   * be dropped into a notebook.
   */
  gtk_drag_dest_unset (GTK_WIDGET (editor->priv->source_view));

  EXIT;
}

static void
gb_editor_tab_thaw_drag (GbTab *tab)
{
  GbEditorTabPrivate *priv;
  GtkTargetList *target_list;
  GbEditorTab *editor = (GbEditorTab *) tab;

  ENTRY;

  g_return_if_fail (GB_IS_EDITOR_TAB (editor));

  priv = editor->priv;

  /*
   * WORKAROUND:
   *
   * Restore drag 'n drop for this tab. These match the values in
   * gtktextview.c.
   */
  gtk_drag_dest_set (GTK_WIDGET (priv->source_view), 0, NULL, 0,
                     GDK_ACTION_COPY | GDK_ACTION_MOVE);
  target_list = gtk_target_list_new (NULL, 0);
  gtk_drag_dest_set_target_list (GTK_WIDGET (priv->source_view), target_list);
  gtk_target_list_unref (target_list);

  EXIT;
}

static void
gb_editor_tab_grab_focus (GtkWidget *widget)
{
  GbEditorTab *tab = (GbEditorTab *) widget;

  ENTRY;

  g_return_if_fail (GB_IS_EDITOR_TAB (tab));

  gtk_widget_grab_focus (GTK_WIDGET (tab->priv->source_view));

  EXIT;
}

static gboolean
on_search_entry_key_press_event (GdTaggedEntry *entry,
                                 GdkEventKey   *event,
                                 GbEditorTab   *tab)
{
  g_assert (GD_IS_TAGGED_ENTRY (entry));
  g_assert (GB_IS_EDITOR_TAB (tab));

  if (event->keyval == GDK_KEY_Escape)
    {
      gtk_revealer_set_reveal_child (tab->priv->revealer, FALSE);
      gb_source_view_set_show_shadow (tab->priv->source_view, FALSE);
      gtk_widget_grab_focus (GTK_WIDGET (tab->priv->source_view));
      return GDK_EVENT_STOP;
    }

  return GDK_EVENT_PROPAGATE;
}

static gboolean
on_search_entry_focus_in (GdTaggedEntry *entry,
                          GdkEvent      *event,
                          GbEditorTab   *tab)
{
  g_return_val_if_fail (GD_IS_TAGGED_ENTRY (entry), FALSE);
  g_return_val_if_fail (GB_IS_EDITOR_TAB (tab), FALSE);

  gtk_widget_queue_draw (GTK_WIDGET (tab->priv->source_view));

  return FALSE;
}

static gboolean
on_search_entry_focus_out (GdTaggedEntry *entry,
                           GdkEvent      *event,
                           GbEditorTab   *tab)
{
  g_return_val_if_fail (GD_IS_TAGGED_ENTRY (entry), FALSE);
  g_return_val_if_fail (GB_IS_EDITOR_TAB (tab), FALSE);

  gtk_widget_queue_draw (GTK_WIDGET (tab->priv->source_view));

  return FALSE;
}

static gboolean
do_delayed_animation (gpointer data)
{
  GbEditorTabPrivate *priv;
  GbBoxTheatric *theatric;
  GdkFrameClock *frame_clock;
  GbEditorTab *tab = data;
  GtkTextView *text_view;
  GtkTextBuffer *buffer;
  GtkSourceStyleScheme *scheme;
  GtkSourceStyle *style;
  GdkRectangle begin_rect;
  GdkRectangle end_rect;
  GtkTextIter begin;
  GtkTextIter end;
  GdkRGBA rgba;
  gchar *color;

  priv = tab->priv;

  gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (priv->document),
                                        &begin, &end);
  if (gtk_text_iter_compare (&begin, &end) == 0)
    return G_SOURCE_REMOVE;

  text_view = GTK_TEXT_VIEW (priv->source_view);
  buffer = gtk_text_view_get_buffer (text_view);
  scheme = gtk_source_buffer_get_style_scheme (GTK_SOURCE_BUFFER (buffer));
  style = gtk_source_style_scheme_get_style (scheme, "search-match");
  g_object_get (style, "background", &color, NULL);
  gdk_rgba_parse (&rgba, color);
  gb_rgba_shade (&rgba, &rgba, 1.2);
  g_free (color);
  color = gdk_rgba_to_string (&rgba);

  gtk_text_view_get_iter_location (GTK_TEXT_VIEW (priv->source_view),
                                   &begin, &begin_rect);
  gtk_text_view_buffer_to_window_coords (GTK_TEXT_VIEW (priv->source_view),
                                         GTK_TEXT_WINDOW_WIDGET,
                                         begin_rect.x,
                                         begin_rect.y,
                                         &begin_rect.x,
                                         &begin_rect.y);

  gtk_text_view_get_iter_location (GTK_TEXT_VIEW (priv->source_view),
                                   &end, &end_rect);
  gtk_text_view_buffer_to_window_coords (GTK_TEXT_VIEW (priv->source_view),
                                         GTK_TEXT_WINDOW_WIDGET,
                                         end_rect.x,
                                         end_rect.y,
                                         &end_rect.x,
                                         &end_rect.y);

  /*
   * TODO: This might actually need to wrap around more.
   */
  gdk_rectangle_union (&begin_rect, &end_rect, &begin_rect);

#define X_GROW 25
#define Y_GROW 25

  end_rect = begin_rect;
  end_rect.x -= X_GROW;
  end_rect.y -= Y_GROW;
  end_rect.width += X_GROW * 2;
  end_rect.height += Y_GROW * 2;

  theatric = g_object_new (GB_TYPE_BOX_THEATRIC,
                           "target", priv->source_view,
                           "x", begin_rect.x,
                           "y", begin_rect.y,
                           "width", begin_rect.width,
                           "height", begin_rect.height,
                           "background", color,
                           "alpha", 0.5,
                           NULL);

  frame_clock = gtk_widget_get_frame_clock (GTK_WIDGET (priv->source_view));

  gb_object_animate (theatric,
                     GB_ANIMATION_EASE_OUT_CUBIC,
                     250,
                     frame_clock,
                     "alpha", 0.0,
                     "height", end_rect.height,
                     "width", end_rect.width,
                     "x", end_rect.x,
                     "y", end_rect.y,
                     NULL);

#undef X_GROW
#undef Y_GROW

  g_object_unref (tab);
  g_free (color);

  return G_SOURCE_REMOVE;
}

static void
delayed_animation (GbEditorTab *tab)
{
  g_timeout_add (200, do_delayed_animation, g_object_ref (tab));
}

static void
select_and_animate (GbEditorTab       *tab,
                    const GtkTextIter *begin,
                    const GtkTextIter *end)
{
  GbEditorTabPrivate *priv;
  GtkTextIter copy;

  g_return_if_fail (GB_IS_EDITOR_TAB (tab));
  g_return_if_fail (begin);
  g_return_if_fail (end);

  priv = tab->priv;

  gtk_text_iter_assign (&copy, begin);

  gtk_text_buffer_select_range (GTK_TEXT_BUFFER (priv->document), begin, end);
  gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW (priv->source_view), &copy, 0.0,
                                FALSE, 0.0, 0.5);

  if (gtk_text_iter_compare (begin, end) != 0)
    delayed_animation (tab);
}

static void
gb_editor_tab_move_next_match (GbEditorTab *tab)
{
  GbEditorTabPrivate *priv;
  GtkTextIter select_begin;
  GtkTextIter select_end;
  GtkTextIter match_begin;
  GtkTextIter match_end;

  ENTRY;

  g_return_if_fail (GB_IS_EDITOR_TAB (tab));

  priv = tab->priv;

  /*
   * Start by trying from our current location.
   */

  gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (priv->document),
                                        &select_begin, &select_end);

  if (gtk_source_search_context_forward (priv->search_context, &select_end,
                                         &match_begin, &match_end))
    {
      select_and_animate (tab, &match_begin, &match_end);
      EXIT;
    }

  /*
   * Didn't find anything, let's try from the beginning of the buffer.
   */

  gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (priv->document),
                              &select_begin, &select_end);

  if (gtk_source_search_context_forward (priv->search_context, &select_begin,
                                         &match_begin, &match_end))
    {
      select_and_animate (tab, &match_begin, &match_end);
      EXIT;
    }

  EXIT;
}

static void
gb_editor_tab_move_previous_match (GbEditorTab *tab)
{
  GbEditorTabPrivate *priv;
  GtkTextIter select_begin;
  GtkTextIter select_end;
  GtkTextIter match_begin;
  GtkTextIter match_end;

  ENTRY;

  g_return_if_fail (GB_IS_EDITOR_TAB (tab));

  priv = tab->priv;

  gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (priv->document),
                                        &select_begin, &select_end);

  if (gtk_source_search_context_backward (priv->search_context, &select_begin,
                                          &match_begin, &match_end))
    select_and_animate (tab, &match_begin, &match_end);

  EXIT;
}

static void
on_search_entry_activate (GdTaggedEntry *search_entry,
                          GbEditorTab   *tab)
{
  g_return_if_fail (GD_IS_TAGGED_ENTRY (search_entry));
  g_return_if_fail (GB_IS_EDITOR_TAB (tab));

  gb_editor_tab_move_next_match (tab);
}

static gboolean
on_source_view_focus_in_event (GbSourceView *view,
                               GdkEvent     *event,
                               GbEditorTab  *tab)
{
  g_return_val_if_fail (GB_IS_EDITOR_TAB (tab), FALSE);

  gtk_revealer_set_reveal_child (tab->priv->revealer, FALSE);
  gtk_source_search_context_set_highlight (tab->priv->search_context, FALSE);

  return GDK_EVENT_PROPAGATE;
}

static void
on_source_view_populate_popup (GtkTextView *text_view,
                               GtkWidget   *popup,
                               GbEditorTab *tab)
{
  GbWorkbench *workbench;
  GbWorkspace *workspace;

  g_return_if_fail (GB_IS_SOURCE_VIEW (text_view));
  g_return_if_fail (GB_IS_EDITOR_TAB (tab));

  if (GTK_IS_MENU (popup))
    {
      PangoFontDescription *font = NULL;
      GtkStyleContext *context;
      GMenuModel *model;
      GtkWidget *menu_item;
      GtkWidget *menu;
      GtkWidget *separator;

      workbench = GB_WORKBENCH (gtk_widget_get_toplevel (GTK_WIDGET (text_view)));
      workspace = gb_workbench_get_active_workspace (workbench);

      /*
       * WORKAROUND:
       *
       * GtkSourceView (and really, GtkTextView) inherit the font for the
       * popup window from the GtkTextView. This is problematic since we
       * override it to be a font such as Monospace.
       *
       * The following code works around that by applying the font that
       * is the default for the editor tab to the popup window.
       */
      context = gtk_widget_get_style_context (GTK_WIDGET (tab));
      gtk_style_context_get (context, GTK_STATE_FLAG_NORMAL,
                             "font", &font,
                             NULL);
      if (font)
        {
          gtk_widget_override_font (popup, font);
          pango_font_description_free (font);
        }

      /*
       * TODO: Add menu for controlling font size.
       */

      /*
       * Add separator.
       */
      separator = gtk_separator_menu_item_new ();
      gtk_menu_shell_prepend (GTK_MENU_SHELL (popup), GTK_WIDGET (separator));
      gtk_widget_show (separator);

      /*
       * Add menu for highlight mode.
       */
      model = gb_source_highlight_menu_new ();
      menu = gtk_menu_new_from_model (model);
      menu_item = gtk_menu_item_new_with_label (_("Highlight Mode"));
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item), menu);
      gtk_menu_shell_prepend (GTK_MENU_SHELL (popup), GTK_WIDGET (menu_item));
      gtk_widget_insert_action_group (GTK_WIDGET (menu_item), "editor",
                                      gb_workspace_get_actions (workspace));
      gtk_widget_show (GTK_WIDGET (menu_item));
      g_object_unref (model);
    }
}

static void
on_source_view_push_snippet (GbSourceView           *source_view,
                             GbSourceSnippet        *snippet,
                             GbSourceSnippetContext *context,
                             GtkTextIter            *iter,
                             GbEditorTab            *tab)
{
  GFile *file;

  g_return_if_fail (GB_IS_SOURCE_VIEW (source_view));
  g_return_if_fail (GB_IS_SOURCE_SNIPPET (snippet));
  g_return_if_fail (GB_IS_SOURCE_SNIPPET_CONTEXT (context));
  g_return_if_fail (iter);
  g_return_if_fail (GB_IS_EDITOR_TAB (tab));

  file = gtk_source_file_get_location (tab->priv->file);
  g_assert (!file || G_IS_FILE (file));

  if (file)
    {
      gchar *name = g_file_get_basename (file);
      gb_source_snippet_context_add_variable (context, "filename", name);
      g_free (name);
    }
}

void
gb_editor_tab_scroll_to_line (GbEditorTab *tab,
                              guint        line,
                              guint        line_offset)
{
  GtkTextIter iter;

  g_return_if_fail (GB_IS_EDITOR_TAB (tab));

  gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (tab->priv->document),
                                    &iter, line);
  gtk_text_iter_set_line_offset (&iter, line_offset);
  gtk_text_buffer_select_range (GTK_TEXT_BUFFER (tab->priv->document),
                                &iter, &iter);
  gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW (tab->priv->source_view), &iter,
                                0.0, FALSE, 0.0, 0.5);
}

static gboolean
transform_file_to_language (GBinding     *binding,
                            const GValue *src_value,
                            GValue       *dst_value,
                            gpointer      user_data)
{
  GtkSourceLanguage *language = NULL;
  GFile *location;

  location = g_value_get_object (src_value);

  if (location)
    {
      GtkSourceLanguageManager *manager;
      gchar *filename;
      gchar *content_type = NULL;

      filename = g_file_get_basename (location);

      /*
       * TODO: Load content_type using g_file_query_info().
       */

      manager = gtk_source_language_manager_get_default ();
      language = gtk_source_language_manager_guess_language (manager, filename,
                                                             content_type);

      g_free (filename);
      g_free (content_type);
    }

  g_value_set_object (dst_value, language);

  return TRUE;
}

static gboolean
transform_file_to_title (GBinding     *binding,
                         const GValue *src_value,
                         GValue       *dst_value,
                         gpointer      user_data)
{
  GbEditorTab *tab = user_data;
  gchar *title;
  GFile *file;

  g_return_val_if_fail (GB_IS_EDITOR_TAB (tab), FALSE);
  g_return_val_if_fail (G_VALUE_HOLDS (src_value, G_TYPE_FILE), FALSE);
  g_return_val_if_fail (G_VALUE_HOLDS (dst_value, G_TYPE_STRING), FALSE);

  file = g_value_get_object (src_value);

  if (file)
    title = g_file_get_basename (file);
  else
    title = g_strdup (_("unsaved file"));

  g_value_take_string (dst_value, title);

  return TRUE;
}

static void
gb_editor_tab_constructed (GObject *object)
{
  GtkSourceCompletion *comp;
  GbEditorTabPrivate *priv;
  GbEditorTab *tab = (GbEditorTab *) object;
  GtkSourceGutter *gutter;

  ENTRY;

  g_return_if_fail (GB_IS_EDITOR_TAB (tab));
  g_return_if_fail (GB_IS_EDITOR_DOCUMENT (tab->priv->document));

  priv = tab->priv;

  if (!priv->settings)
    gb_editor_tab_set_settings (tab, NULL);

  g_signal_connect_swapped (priv->document,
                            "modified-changed",
                            G_CALLBACK (gb_editor_tab_modified_changed),
                            tab);
  g_signal_connect_swapped (priv->document,
                            "cursor-moved",
                            G_CALLBACK (gb_editor_tab_cursor_moved),
                            tab);
  g_signal_connect_swapped (priv->document,
                            "notify::language",
                            G_CALLBACK (gb_editor_tab_language_changed),
                            tab);

  g_signal_connect (priv->source_view,
                    "focus-in-event",
                    G_CALLBACK (on_source_view_focus_in_event),
                    tab);
  g_signal_connect (priv->source_view,
                    "populate-popup",
                    G_CALLBACK (on_source_view_populate_popup),
                    tab);
  g_signal_connect (priv->source_view,
                    "push-snippet",
                    G_CALLBACK (on_source_view_push_snippet),
                    tab);

  g_signal_connect_swapped (priv->go_down_button,
                            "clicked",
                            G_CALLBACK (gb_editor_tab_move_next_match),
                            tab);
  g_signal_connect_swapped (priv->go_up_button,
                            "clicked",
                            G_CALLBACK (gb_editor_tab_move_previous_match),
                            tab);

  g_signal_connect_swapped (priv->search_context,
                            "notify::occurrences-count",
                            G_CALLBACK (on_search_occurrences_notify),
                            tab);

  comp = gtk_source_view_get_completion (GTK_SOURCE_VIEW (priv->source_view));
  gtk_source_completion_add_provider (comp, priv->snippets_provider, NULL);

  /*
   * WORKAROUND:

   * Once GtkSourceView exports this as an internal child, we can do this from
   * the gb-editor-tab.ui file.
   */
  g_object_set (comp,
                "show-headers", FALSE,
                "select-on-show", TRUE,
                NULL);

  g_signal_connect (priv->search_entry,
                    "activate",
                    G_CALLBACK (on_search_entry_activate),
                    tab);
  g_signal_connect (priv->search_entry,
                    "key-press-event",
                    G_CALLBACK (on_search_entry_key_press_event),
                    tab);
  g_signal_connect (priv->search_entry,
                    "focus-in-event",
                    G_CALLBACK (on_search_entry_focus_in),
                    tab);
  g_signal_connect (priv->search_entry,
                    "focus-out-event",
                    G_CALLBACK (on_search_entry_focus_out),
                    tab);
  g_object_bind_property (priv->search_entry, "text",
                          priv->search_settings, "search-text",
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property (priv->revealer, "reveal-child",
                          priv->source_view, "show-shadow",
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property (priv->file, "location",
                          priv->change_monitor, "file",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property_full (priv->file, "location", tab, "title",
                               G_BINDING_SYNC_CREATE, transform_file_to_title,
                               NULL, tab, NULL);
  g_object_bind_property_full (priv->file, "location", priv->document,
                               "language", G_BINDING_SYNC_CREATE,
                               transform_file_to_language, NULL, tab, NULL);

  gutter = gtk_source_view_get_gutter (GTK_SOURCE_VIEW (priv->source_view),
                                       GTK_TEXT_WINDOW_LEFT);
  priv->change_renderer =
      g_object_new (GB_TYPE_SOURCE_CHANGE_GUTTER_RENDERER,
                    "change-monitor", priv->change_monitor,
                    "size", 2,
                    "visible", TRUE,
                    "xpad", 1,
                    NULL);
  gtk_source_gutter_insert (gutter, priv->change_renderer, 0);

  gb_editor_tab_cursor_moved (tab, priv->document);

  EXIT;
}

static void
gb_editor_tab_close (GbTab *tab)
{
  GbEditorTabPrivate *priv;
  GtkTextBuffer *buffer;
  GtkWidget *parent;

  ENTRY;

  g_return_if_fail (GB_IS_EDITOR_TAB (tab));

  priv = GB_EDITOR_TAB (tab)->priv;

  buffer = GTK_TEXT_BUFFER (priv->document);

  if (gtk_text_buffer_get_modified (buffer))
    {
      g_message ("TODO: handle dirty editor state.");
    }

  /*
   * WORKAROUND:
   *
   * The search entry seems to have some sort of idle task that is causing
   * this to segfault while it is still around.
   */
  g_clear_pointer (&priv->search_entry, gtk_widget_destroy);

  /*
   * Remove the tab from the notebook.
   */
  parent = gtk_widget_get_parent (GTK_WIDGET (tab));
  gtk_container_remove (GTK_CONTAINER (parent), GTK_WIDGET (tab));

  EXIT;
}

static void
gb_editor_tab_dispose (GObject *object)
{
  GbEditorTab *tab = (GbEditorTab *) object;

  ENTRY;

  g_assert (GB_IS_EDITOR_TAB (tab));

  gb_editor_tab_disconnect_settings (tab);

  g_clear_object (&tab->priv->document);
  g_clear_object (&tab->priv->search_entry_tag);
  g_clear_object (&tab->priv->file);
  g_clear_object (&tab->priv->snippets_provider);
  g_clear_object (&tab->priv->search_highlighter);
  g_clear_object (&tab->priv->search_settings);
  g_clear_object (&tab->priv->search_context);
  g_clear_object (&tab->priv->settings);

  EXIT;
}

static void
gb_editor_tab_finalize (GObject *object)
{
  ENTRY;

  G_OBJECT_CLASS (gb_editor_tab_parent_class)->finalize (object);

  EXIT;
}

static void
gb_editor_tab_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GbEditorTab *tab = GB_EDITOR_TAB (object);

  switch (prop_id)
    {
    case PROP_DOCUMENT:
      g_value_set_object (value, gb_editor_tab_get_document (tab));
      break;

    case PROP_FILE:
      g_value_set_object (value, gb_editor_tab_get_file (tab));
      break;

    case PROP_SETTINGS:
      g_value_set_object (value, gb_editor_tab_get_settings (tab));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_editor_tab_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GbEditorTab *tab = GB_EDITOR_TAB (object);

  switch (prop_id)
    {
    case PROP_FONT_DESC:
      gb_editor_tab_set_font_desc (tab, g_value_get_boxed (value));
      break;

    case PROP_SETTINGS:
      gb_editor_tab_set_settings (tab, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_editor_tab_class_init (GbEditorTabClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GbTabClass *tab_class = GB_TAB_CLASS (klass);

  object_class->constructed = gb_editor_tab_constructed;
  object_class->dispose = gb_editor_tab_dispose;
  object_class->finalize = gb_editor_tab_finalize;
  object_class->get_property = gb_editor_tab_get_property;
  object_class->set_property = gb_editor_tab_set_property;

  widget_class->grab_focus = gb_editor_tab_grab_focus;

  tab_class->close = gb_editor_tab_close;
  tab_class->freeze_drag = gb_editor_tab_freeze_drag;
  tab_class->thaw_drag = gb_editor_tab_thaw_drag;

  gParamSpecs [PROP_DOCUMENT] =
    g_param_spec_object ("document",
                         _ ("Document"),
                         _ ("The document to edit."),
                         GB_TYPE_EDITOR_DOCUMENT,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_DOCUMENT,
                                   gParamSpecs[PROP_DOCUMENT]);

  gParamSpecs [PROP_FILE] =
      g_param_spec_object ("file",
                           _("File"),
                           _("The file for the tab."),
                           GTK_SOURCE_TYPE_FILE,
                           (G_PARAM_READABLE |
                            G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_FILE,
                                   gParamSpecs [PROP_FILE]);

  gParamSpecs [PROP_FONT_DESC] =
    g_param_spec_boxed ("font-desc",
                        _ ("Font Description"),
                        _ ("The Pango Font Description to use in the editor."),
                        PANGO_TYPE_FONT_DESCRIPTION,
                        (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_FONT_DESC,
                                   gParamSpecs[PROP_FONT_DESC]);

  gParamSpecs [PROP_SETTINGS] =
    g_param_spec_object ("settings",
                         _ ("Settings"),
                         _ ("The editor settings."),
                         GB_TYPE_EDITOR_SETTINGS,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SETTINGS,
                                   gParamSpecs[PROP_SETTINGS]);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               GB_EDITOR_TAB_UI_RESOURCE);

  gtk_widget_class_bind_template_child_private (widget_class, GbEditorTab,
                                                floating_bar);
  gtk_widget_class_bind_template_child_private (widget_class, GbEditorTab,
                                                document);
  gtk_widget_class_bind_template_child_private (widget_class, GbEditorTab,
                                                change_monitor);
  gtk_widget_class_bind_template_child_private (widget_class, GbEditorTab,
                                                file);
  gtk_widget_class_bind_template_child_private (widget_class, GbEditorTab,
                                                go_down_button);
  gtk_widget_class_bind_template_child_private (widget_class, GbEditorTab,
                                                go_up_button);
  gtk_widget_class_bind_template_child_private (widget_class, GbEditorTab,
                                                overlay);
  gtk_widget_class_bind_template_child_private (widget_class, GbEditorTab,
                                                preview_container);
  gtk_widget_class_bind_template_child_private (widget_class, GbEditorTab,
                                                progress_bar);
  gtk_widget_class_bind_template_child_private (widget_class, GbEditorTab,
                                                revealer);
  gtk_widget_class_bind_template_child_private (widget_class, GbEditorTab,
                                                scroller);
  gtk_widget_class_bind_template_child_private (widget_class, GbEditorTab,
                                                search_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GbEditorTab,
                                                search_context);
  gtk_widget_class_bind_template_child_private (widget_class, GbEditorTab,
                                                search_highlighter);
  gtk_widget_class_bind_template_child_private (widget_class, GbEditorTab,
                                                search_settings);
  gtk_widget_class_bind_template_child_private (widget_class, GbEditorTab,
                                                snippets_provider);
  gtk_widget_class_bind_template_child_private (widget_class, GbEditorTab,
                                                source_view);

  g_type_ensure (GB_TYPE_EDITOR_DOCUMENT);
  g_type_ensure (GB_TYPE_SOURCE_CHANGE_MONITOR);
  g_type_ensure (GB_TYPE_SOURCE_VIEW);
  g_type_ensure (GB_TYPE_SOURCE_SNIPPET_COMPLETION_PROVIDER);
  g_type_ensure (GB_TYPE_SOURCE_SEARCH_HIGHLIGHTER);
  g_type_ensure (GD_TYPE_TAGGED_ENTRY);
  g_type_ensure (NAUTILUS_TYPE_FLOATING_BAR);
}

static void
gb_editor_tab_init (GbEditorTab *tab)
{
  tab->priv = gb_editor_tab_get_instance_private (tab);

  gtk_widget_init_template (GTK_WIDGET (tab));
}
