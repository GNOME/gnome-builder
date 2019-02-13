/* ide-editor-page.c
 *
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

#define G_LOG_DOMAIN "ide-editor-page"

#include "config.h"

#include <dazzle.h>
#include <libpeas/peas.h>
#include <gtksourceview/gtksource.h>
#include <pango/pangofc-fontmap.h>

#include "ide-editor-page.h"
#include "ide-editor-page-addin.h"
#include "ide-editor-private.h"
#include "ide-line-change-gutter-renderer.h"

#define AUTO_HIDE_TIMEOUT_SECONDS 5

enum {
  PROP_0,
  PROP_AUTO_HIDE_MAP,
  PROP_BUFFER,
  PROP_SEARCH,
  PROP_SHOW_MAP,
  PROP_VIEW,
  N_PROPS
};

static void ide_editor_page_update_reveal_timer (IdeEditorPage *self);

G_DEFINE_TYPE (IdeEditorPage, ide_editor_page, IDE_TYPE_PAGE)

DZL_DEFINE_COUNTER (instances, "Editor", "N Views", "Number of editor views");

static GParamSpec *properties [N_PROPS];
static FcConfig *localFontConfig;

static void
ide_editor_page_load_fonts (IdeEditorPage *self)
{
  PangoFontMap *font_map;
  PangoFontDescription *font_desc;

  if (g_once_init_enter (&localFontConfig))
    {
      const gchar *font_path = PACKAGE_DATADIR "/gnome-builder/fonts/BuilderBlocks.ttf";
      FcConfig *config = FcInitLoadConfigAndFonts ();

      if (g_getenv ("GB_IN_TREE_FONTS") != NULL)
        font_path = "data/fonts/BuilderBlocks.ttf";

      if (!g_file_test (font_path, G_FILE_TEST_IS_REGULAR))
        g_warning ("Failed to locate \"%s\"", font_path);

      FcConfigAppFontAddFile (config, (const FcChar8 *)font_path);

      g_once_init_leave (&localFontConfig, config);
    }

  font_map = pango_cairo_font_map_new_for_font_type (CAIRO_FONT_TYPE_FT);
  pango_fc_font_map_set_config (PANGO_FC_FONT_MAP (font_map), localFontConfig);
  gtk_widget_set_font_map (GTK_WIDGET (self->map), font_map);
  font_desc = pango_font_description_from_string ("Builder Blocks 1");

  g_assert (localFontConfig != NULL);
  g_assert (font_map != NULL);
  g_assert (font_desc != NULL);

  g_object_set (self->map, "font-desc", font_desc, NULL);

  pango_font_description_free (font_desc);
  g_object_unref (font_map);
}

static void
ide_editor_page_update_icon (IdeEditorPage *self)
{
  g_autofree gchar *name = NULL;
  g_autofree gchar *content_type = NULL;
  g_autofree gchar *sniff = NULL;
  g_autoptr(GIcon) icon = NULL;
  GtkTextIter begin, end;
  GFile *file;

  g_assert (IDE_IS_EDITOR_PAGE (self));
  g_assert (IDE_IS_BUFFER (self->buffer));

  /* Get first 1024 bytes to help determine content type */
  gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (self->buffer), &begin, &end);
  if (gtk_text_iter_get_offset (&end) > 1024)
    gtk_text_iter_set_offset (&end, 1024);
  sniff = gtk_text_iter_get_slice (&begin, &end);

  /* Now get basename for content type */
  file = ide_buffer_get_file (self->buffer);
  name = g_file_get_basename (file);

  /* Guess content type */
  content_type = g_content_type_guess (name, (const guchar *)sniff, strlen (sniff), NULL);

  /* Update icon to match guess */
  icon = ide_g_content_type_get_symbolic_icon (content_type);
  ide_page_set_icon (IDE_PAGE (self), icon);
}

static void
ide_editor_page_buffer_notify_failed (IdeEditorPage *self,
                                      GParamSpec    *pspec,
                                      IdeBuffer     *buffer)
{
  gboolean failed;

  g_assert (IDE_IS_EDITOR_PAGE (self));
  g_assert (IDE_IS_BUFFER (buffer));

  failed = ide_buffer_get_failed (buffer);

  ide_page_set_failed (IDE_PAGE (self), failed);
}

static void
ide_editor_page_stop_search (IdeEditorPage      *self,
                             IdeEditorSearchBar *search_bar)
{
  g_assert (IDE_IS_EDITOR_PAGE (self));
  g_assert (IDE_IS_EDITOR_SEARCH_BAR (search_bar));

  gtk_revealer_set_reveal_child (self->search_revealer, FALSE);
  gtk_widget_grab_focus (GTK_WIDGET (self->source_view));
}

static void
ide_editor_page_notify_child_revealed (IdeEditorPage *self,
                                       GParamSpec    *pspec,
                                       GtkRevealer   *revealer)
{
  g_assert (IDE_IS_EDITOR_PAGE (self));
  g_assert (GTK_IS_REVEALER (revealer));

  if (gtk_revealer_get_child_revealed (revealer))
    {
      GtkWidget *toplevel = gtk_widget_get_ancestor (GTK_WIDGET (revealer), GTK_TYPE_WINDOW);
      GtkWidget *focus = gtk_window_get_focus (GTK_WINDOW (toplevel));

      /* Only focus the search bar if it doesn't already have focus,
       * as it can reselect the search text.
       */
      if (focus == NULL || !gtk_widget_is_ancestor (focus, GTK_WIDGET (revealer)))
        gtk_widget_grab_focus (GTK_WIDGET (self->search_bar));
    }
}

static gboolean
ide_editor_page_focus_in_event (IdeEditorPage *self,
                                GdkEventFocus *focus,
                                IdeSourceView *source_view)
{
  g_assert (IDE_IS_EDITOR_PAGE (self));
  g_assert (IDE_IS_SOURCE_VIEW (source_view));

  ide_page_mark_used (IDE_PAGE (self));

  return GDK_EVENT_PROPAGATE;
}

static void
ide_editor_page_buffer_loaded (IdeEditorPage *self,
                               IdeBuffer     *buffer)
{
  g_assert (IDE_IS_EDITOR_PAGE (self));
  g_assert (IDE_IS_BUFFER (buffer));

  ide_editor_page_update_icon (self);

  /* Scroll to the insertion location once the buffer
   * has loaded. This is useful if it is not onscreen.
   */
  ide_source_view_scroll_to_insert (self->source_view);
}

static void
ide_editor_page_buffer_modified_changed (IdeEditorPage *self,
                                         IdeBuffer     *buffer)
{
  gboolean modified = FALSE;

  g_assert (IDE_IS_EDITOR_PAGE (self));
  g_assert (IDE_IS_BUFFER (buffer));

  if (!ide_buffer_get_loading (buffer))
    modified = gtk_text_buffer_get_modified (GTK_TEXT_BUFFER (buffer));

  ide_page_set_modified (IDE_PAGE (self), modified);
}

static void
ide_editor_page_buffer_notify_language_cb (IdeExtensionSetAdapter *set,
                                           PeasPluginInfo         *plugin_info,
                                           PeasExtension          *exten,
                                           gpointer                user_data)
{
  const gchar *language_id = user_data;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_EDITOR_PAGE_ADDIN (exten));

  ide_editor_page_addin_language_changed (IDE_EDITOR_PAGE_ADDIN (exten), language_id);
}

static void
ide_editor_page_buffer_notify_language (IdeEditorPage *self,
                                        GParamSpec    *pspec,
                                        IdeBuffer     *buffer)
{
  const gchar *lang_id = NULL;

  g_assert (IDE_IS_EDITOR_PAGE (self));
  g_assert (IDE_IS_BUFFER (buffer));

  if (self->addins == NULL)
    return;

  lang_id = ide_buffer_get_language_id (buffer);

  /* Update extensions that change based on language */
  ide_extension_set_adapter_set_value (self->addins, lang_id);
  ide_extension_set_adapter_foreach (self->addins,
                                     ide_editor_page_buffer_notify_language_cb,
                                     (gpointer)lang_id);

  ide_editor_page_update_icon (self);
}

static void
ide_editor_page_buffer_notify_style_scheme (IdeEditorPage *self,
                                            GParamSpec    *pspec,
                                            IdeBuffer     *buffer)
{
  g_autofree gchar *background = NULL;
  g_autofree gchar *foreground = NULL;
  GtkSourceStyleScheme *scheme;
  GtkSourceStyle *style;
  gboolean background_set = FALSE;
  gboolean foreground_set = FALSE;
  GdkRGBA rgba;

  g_assert (IDE_IS_EDITOR_PAGE (self));
  g_assert (IDE_IS_BUFFER (buffer));

  if (NULL == (scheme = gtk_source_buffer_get_style_scheme (GTK_SOURCE_BUFFER (buffer))) ||
      NULL == (style = gtk_source_style_scheme_get_style (scheme, "text")))
    goto unset_primary_color;

  g_object_get (style,
                "background-set", &background_set,
                "background", &background,
                "foreground-set", &foreground_set,
                "foreground", &foreground,
                NULL);

  if (!background_set || background == NULL || !gdk_rgba_parse (&rgba, background))
    goto unset_primary_color;

  if (background_set && background != NULL && gdk_rgba_parse (&rgba, background))
    ide_page_set_primary_color_bg (IDE_PAGE (self), &rgba);
  else
    goto unset_primary_color;

  if (foreground_set && foreground != NULL && gdk_rgba_parse (&rgba, foreground))
    ide_page_set_primary_color_fg (IDE_PAGE (self), &rgba);
  else
    ide_page_set_primary_color_fg (IDE_PAGE (self), NULL);

  return;

unset_primary_color:
  ide_page_set_primary_color_bg (IDE_PAGE (self), NULL);
  ide_page_set_primary_color_fg (IDE_PAGE (self), NULL);
}

static void
ide_editor_page__buffer_notify_changed_on_volume (IdeEditorPage *self,
                                                  GParamSpec    *pspec,
                                                  IdeBuffer     *buffer)
{
  g_assert (IDE_IS_EDITOR_PAGE (self));
  g_assert (IDE_IS_BUFFER (buffer));

  gtk_revealer_set_reveal_child (self->modified_revealer,
                                 ide_buffer_get_changed_on_volume (buffer));
}

static void
ide_editor_page_hide_reload_bar (IdeEditorPage *self,
                                 GtkWidget     *button)
{
  g_assert (IDE_IS_EDITOR_PAGE (self));

  gtk_revealer_set_reveal_child (self->modified_revealer, FALSE);
}

static gboolean
ide_editor_page_source_view_event (IdeEditorPage *self,
                                   GdkEvent      *event,
                                   IdeSourceView *source_view)
{
  g_assert (IDE_IS_EDITOR_PAGE (self));
  g_assert (event != NULL);
  g_assert (IDE_IS_SOURCE_VIEW (source_view) || GTK_SOURCE_IS_MAP (source_view));

  if (self->auto_hide_map)
    {
      ide_editor_page_update_reveal_timer (self);
      gtk_revealer_set_reveal_child (self->map_revealer, TRUE);
    }

  return GDK_EVENT_PROPAGATE;
}

static void
ide_editor_page_bind_signals (IdeEditorPage  *self,
                              IdeBuffer      *buffer,
                              DzlSignalGroup *buffer_signals)
{
  g_assert (IDE_IS_EDITOR_PAGE (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (DZL_IS_SIGNAL_GROUP (buffer_signals));

  ide_editor_page_buffer_modified_changed (self, buffer);
  ide_editor_page_buffer_notify_language (self, NULL, buffer);
  ide_editor_page_buffer_notify_style_scheme (self, NULL, buffer);
  ide_editor_page_buffer_notify_failed (self, NULL, buffer);
}

static void
ide_editor_page_set_buffer (IdeEditorPage *self,
                            IdeBuffer     *buffer)
{
  g_assert (IDE_IS_EDITOR_PAGE (self));
  g_assert (!buffer || IDE_IS_BUFFER (buffer));

  if (g_set_object (&self->buffer, buffer))
    {
      dzl_signal_group_set_target (self->buffer_signals, buffer);
      dzl_binding_group_set_source (self->buffer_bindings, buffer);
      gtk_text_view_set_buffer (GTK_TEXT_VIEW (self->source_view),
                                GTK_TEXT_BUFFER (buffer));
      gtk_drag_dest_unset (GTK_WIDGET (self->source_view));
      ide_editor_page_update_icon (self);
    }
}

static IdePage *
ide_editor_page_create_split (IdePage *view)
{
  IdeEditorPage *self = (IdeEditorPage *)view;

  g_assert (IDE_IS_EDITOR_PAGE (self));

  return g_object_new (IDE_TYPE_EDITOR_PAGE,
                       "buffer", self->buffer,
                       "visible", TRUE,
                       NULL);
}

static void
ide_editor_page_notify_frame_set (IdeExtensionSetAdapter *set,
                                  PeasPluginInfo         *plugin_info,
                                  PeasExtension          *exten,
                                  gpointer                user_data)
{
  IdeFrame *frame = user_data;
  IdeEditorPageAddin *addin = (IdeEditorPageAddin *)exten;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_EDITOR_PAGE_ADDIN (addin));
  g_assert (IDE_IS_FRAME (frame));

  ide_editor_page_addin_frame_set (addin, frame);
}

static void
ide_editor_page_addin_added (IdeExtensionSetAdapter *set,
                             PeasPluginInfo         *plugin_info,
                             PeasExtension          *exten,
                             gpointer                user_data)
{
  IdeEditorPage *self = user_data;
  IdeEditorPageAddin *addin = (IdeEditorPageAddin *)exten;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_EDITOR_PAGE_ADDIN (addin));
  g_assert (IDE_IS_EDITOR_PAGE (self));

  ide_editor_page_addin_load (addin, self);

  /*
   * Notify of the current frame, but refetch the frame pointer just
   * to be sure we aren't re-using an old pointer in case we're racing
   * with a finalizer.
   */
  if (self->last_frame_ptr != NULL)
    {
      GtkWidget *frame = gtk_widget_get_ancestor (GTK_WIDGET (self), IDE_TYPE_FRAME);
      if (frame != NULL)
        ide_editor_page_addin_frame_set (addin, IDE_FRAME (frame));
    }
}

static void
ide_editor_page_addin_removed (IdeExtensionSetAdapter *set,
                               PeasPluginInfo         *plugin_info,
                               PeasExtension          *exten,
                               gpointer                user_data)
{
  IdeEditorPage *self = user_data;
  IdeEditorPageAddin *addin = (IdeEditorPageAddin *)exten;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_EDITOR_PAGE_ADDIN (addin));
  g_assert (IDE_IS_EDITOR_PAGE (self));

  ide_editor_page_addin_unload (addin, self);
}

static void
ide_editor_page_hierarchy_changed (GtkWidget *widget,
                                   GtkWidget *old_toplevel)
{
  IdeEditorPage *self = (IdeEditorPage *)widget;
  IdeFrame *frame;
  IdeContext *context;

  g_assert (IDE_IS_EDITOR_PAGE (self));
  g_assert (!old_toplevel || GTK_IS_WIDGET (old_toplevel));

  /*
   * We don't need to chain up today, but if IdePage starts
   * using the hierarchy_changed signal to handle anything, we want
   * to make sure we aren't surprised.
   */
  if (GTK_WIDGET_CLASS (ide_editor_page_parent_class)->hierarchy_changed)
    GTK_WIDGET_CLASS (ide_editor_page_parent_class)->hierarchy_changed (widget, old_toplevel);

  context = ide_widget_get_context (GTK_WIDGET (self));
  frame = (IdeFrame *)gtk_widget_get_ancestor (widget, IDE_TYPE_FRAME);

  /*
   * We don't want to create addins until the widget has been placed into
   * the widget tree. That way the addins can get access to the context
   * or other useful details.
   */
  if (context != NULL && self->addins == NULL)
    {
      self->addins = ide_extension_set_adapter_new (IDE_OBJECT (context),
                                                    peas_engine_get_default (),
                                                    IDE_TYPE_EDITOR_PAGE_ADDIN,
                                                    "Editor-Page-Languages",
                                                    ide_editor_page_get_language_id (self));

      g_signal_connect (self->addins,
                        "extension-added",
                        G_CALLBACK (ide_editor_page_addin_added),
                        self);

      g_signal_connect (self->addins,
                        "extension-removed",
                        G_CALLBACK (ide_editor_page_addin_removed),
                        self);

      ide_extension_set_adapter_foreach (self->addins,
                                         ide_editor_page_addin_added,
                                         self);
    }

  /*
   * If we have been moved into a new frame, notify the addins of the
   * hierarchy change.
   */
  if (frame != NULL && frame != self->last_frame_ptr && self->addins != NULL)
    {
      self->last_frame_ptr = frame;
      ide_extension_set_adapter_foreach (self->addins,
                                         ide_editor_page_notify_frame_set,
                                         frame);
    }
}

static void
ide_editor_page_update_map (IdeEditorPage *self)
{
  GtkWidget *parent;

  g_assert (IDE_IS_EDITOR_PAGE (self));

  parent = gtk_widget_get_parent (GTK_WIDGET (self->map));

  g_object_ref (self->map);

  gtk_container_remove (GTK_CONTAINER (parent), GTK_WIDGET (self->map));

  if (self->auto_hide_map)
    gtk_container_add (GTK_CONTAINER (self->map_revealer), GTK_WIDGET (self->map));
  else
    gtk_container_add (GTK_CONTAINER (self->scroller_box), GTK_WIDGET (self->map));

  gtk_widget_set_visible (GTK_WIDGET (self->map_revealer), self->show_map && self->auto_hide_map);
  gtk_widget_set_visible (GTK_WIDGET (self->map), self->show_map);
  gtk_revealer_set_reveal_child (self->map_revealer, self->show_map);

  ide_editor_page_update_reveal_timer (self);

  g_object_unref (self->map);
}

static void
search_revealer_notify_reveal_child (IdeEditorPage *self,
                                     GParamSpec    *pspec,
                                     GtkRevealer   *revealer)
{
  IdeCompletion *completion;

  g_return_if_fail (IDE_IS_EDITOR_PAGE (self));
  g_return_if_fail (pspec != NULL);
  g_return_if_fail (GTK_IS_REVEALER (revealer));

  completion = ide_source_view_get_completion (IDE_SOURCE_VIEW (self->source_view));

  if (!gtk_revealer_get_reveal_child (revealer))
    {
      ide_editor_search_end_interactive (self->search);

      /* Restore completion that we blocked below. */
      ide_completion_unblock_interactive (completion);
    }
  else
    {
      ide_editor_search_begin_interactive (self->search);

      /*
       * Block the completion while the search bar is set. It only
       * slows things down like replace functionality. We'll
       * restore it above when we clear state.
       */
      ide_completion_block_interactive (completion);
    }
}

static void
ide_editor_page_focus_location (IdeEditorPage *self,
                                IdeLocation   *location,
                                IdeSourceView *source_view)
{
  GtkWidget *editor;

  g_assert (IDE_IS_EDITOR_PAGE (self));
  g_assert (location != NULL);
  g_assert (IDE_IS_SOURCE_VIEW (source_view));

  editor = gtk_widget_get_ancestor (GTK_WIDGET (self), IDE_TYPE_EDITOR_SURFACE);
  ide_editor_surface_focus_location (IDE_EDITOR_SURFACE (editor), location);
}

static void
ide_editor_page_clear_search (IdeEditorPage *self,
                              IdeSourceView *view)
{
  g_assert (IDE_IS_EDITOR_PAGE (self));
  g_assert (IDE_IS_EDITOR_SEARCH (self->search));
  g_assert (IDE_IS_SOURCE_VIEW (view));

  ide_editor_search_set_search_text (self->search, NULL);
  ide_editor_search_set_visible (self->search, FALSE);
  gtk_revealer_set_reveal_child (self->search_revealer, FALSE);
}

static void
ide_editor_page_move_search (IdeEditorPage    *self,
                             GtkDirectionType  dir,
                             gboolean          extend_selection,
                             gboolean          select_match,
                             gboolean          exclusive,
                             gboolean          apply_count,
                             gboolean          at_word_boundaries,
                             IdeSourceView    *view)
{
  IdeEditorSearchSelect sel = 0;

  g_assert (IDE_IS_EDITOR_PAGE (self));
  g_assert (IDE_IS_EDITOR_SEARCH (self->search));
  g_assert (IDE_IS_SOURCE_VIEW (view));

  if (extend_selection && select_match)
    sel = IDE_EDITOR_SEARCH_SELECT_WITH_RESULT;
  else if (extend_selection)
    sel = IDE_EDITOR_SEARCH_SELECT_TO_RESULT;

  ide_editor_search_set_extend_selection (self->search, sel);
  ide_editor_search_set_visible (self->search, TRUE);

  if (apply_count)
    {
      ide_editor_search_set_repeat (self->search, ide_source_view_get_count (view));
      g_signal_emit_by_name (view, "clear-count");
    }

  ide_editor_search_set_at_word_boundaries (self->search, at_word_boundaries);

  switch (dir)
    {
    case GTK_DIR_DOWN:
    case GTK_DIR_RIGHT:
      ide_editor_search_set_reverse (self->search, FALSE);
      ide_editor_search_move (self->search, IDE_EDITOR_SEARCH_NEXT);
      break;

    case GTK_DIR_TAB_FORWARD:
      if (extend_selection)
        ide_editor_search_move (self->search, IDE_EDITOR_SEARCH_FORWARD);
      else
        ide_editor_search_move (self->search, IDE_EDITOR_SEARCH_NEXT);
      break;

    case GTK_DIR_UP:
    case GTK_DIR_LEFT:
      ide_editor_search_set_reverse (self->search, TRUE);
      ide_editor_search_move (self->search, IDE_EDITOR_SEARCH_NEXT);
      break;

    case GTK_DIR_TAB_BACKWARD:
      if (extend_selection)
        ide_editor_search_move (self->search, IDE_EDITOR_SEARCH_BACKWARD);
      else
        ide_editor_search_move (self->search, IDE_EDITOR_SEARCH_PREVIOUS);
      break;

    default:
      break;
    }
}

static void
ide_editor_page_set_search_text (IdeEditorPage *self,
                                 const gchar   *search_text,
                                 gboolean       from_selection,
                                 IdeSourceView *view)
{
  g_autofree gchar *freeme = NULL;
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (IDE_IS_EDITOR_PAGE (self));
  g_assert (IDE_IS_EDITOR_SEARCH (self->search));
  g_assert (search_text != NULL || from_selection);
  g_assert (IDE_IS_SOURCE_VIEW (view));

  /* Use interactive mode if we're copying from the clipboard, because that
   * is usually going to be followed by focusing the search box and we want
   * to make sure the occurrance count is updated.
   */

  if (from_selection)
    ide_editor_search_begin_interactive (self->search);

  if (from_selection)
    {
      if (gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (self->buffer), &begin, &end))
        search_text = freeme = gtk_text_iter_get_slice (&begin, &end);
    }

  ide_editor_search_set_search_text (self->search, search_text);
  ide_editor_search_set_regex_enabled (self->search, FALSE);

  if (from_selection)
    ide_editor_search_end_interactive (self->search);
}

static void
ide_editor_page_constructed (GObject *object)
{
  IdeEditorPage *self = (IdeEditorPage *)object;
  GtkSourceGutterRenderer *renderer;
  GtkSourceGutter *gutter;

  g_assert (IDE_IS_EDITOR_PAGE (self));

  G_OBJECT_CLASS (ide_editor_page_parent_class)->constructed (object);

  gutter = gtk_source_view_get_gutter (GTK_SOURCE_VIEW (self->map), GTK_TEXT_WINDOW_LEFT);
  renderer = g_object_new (IDE_TYPE_LINE_CHANGE_GUTTER_RENDERER,
                           "size", 1,
                           "visible", TRUE,
                           NULL);
  gtk_source_gutter_insert (gutter, renderer, 0);

  _ide_editor_page_init_actions (self);
  _ide_editor_page_init_shortcuts (self);
  _ide_editor_page_init_settings (self);

  g_signal_connect_swapped (self->source_view,
                            "focus-in-event",
                            G_CALLBACK (ide_editor_page_focus_in_event),
                            self);

  g_signal_connect_swapped (self->source_view,
                            "motion-notify-event",
                            G_CALLBACK (ide_editor_page_source_view_event),
                            self);

  g_signal_connect_swapped (self->source_view,
                            "scroll-event",
                            G_CALLBACK (ide_editor_page_source_view_event),
                            self);

  g_signal_connect_swapped (self->source_view,
                            "focus-location",
                            G_CALLBACK (ide_editor_page_focus_location),
                            self);

  g_signal_connect_swapped (self->source_view,
                            "set-search-text",
                            G_CALLBACK (ide_editor_page_set_search_text),
                            self);

  g_signal_connect_swapped (self->source_view,
                            "clear-search",
                            G_CALLBACK (ide_editor_page_clear_search),
                            self);

  g_signal_connect_swapped (self->source_view,
                            "move-search",
                            G_CALLBACK (ide_editor_page_move_search),
                            self);

  g_signal_connect_swapped (self->map,
                            "motion-notify-event",
                            G_CALLBACK (ide_editor_page_source_view_event),
                            self);



  /*
   * We want to track when the search revealer is visible. We will discard
   * the search context when the revealer is not visible so that we don't
   * continue performing expensive buffer operations.
   */
  g_signal_connect_swapped (self->search_revealer,
                            "notify::reveal-child",
                            G_CALLBACK (search_revealer_notify_reveal_child),
                            self);

  self->search = ide_editor_search_new (GTK_SOURCE_VIEW (self->source_view));
  ide_editor_search_bar_set_search (self->search_bar, self->search);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "editor-search",
                                  G_ACTION_GROUP (self->search));

  ide_editor_page_load_fonts (self);
  ide_editor_page_update_map (self);
}

static void
ide_editor_page_destroy (GtkWidget *widget)
{
  IdeEditorPage *self = (IdeEditorPage *)widget;

  g_assert (IDE_IS_EDITOR_PAGE (self));

  /*
   * WORKAROUND: We need to reset the drag dest to avoid warnings by Gtk
   * reseting the target list for the source view.
   */
  if (self->source_view != NULL)
    gtk_drag_dest_set (GTK_WIDGET (self->source_view),
                       GTK_DEST_DEFAULT_ALL,
                       NULL, 0, GDK_ACTION_COPY);

  dzl_clear_source (&self->toggle_map_source);

  ide_clear_and_destroy_object (&self->addins);

  gtk_widget_insert_action_group (widget, "editor-search", NULL);
  gtk_widget_insert_action_group (widget, "editor-page", NULL);

  g_cancellable_cancel (self->destroy_cancellable);
  g_clear_object (&self->destroy_cancellable);

  g_clear_object (&self->search);
  g_clear_object (&self->editor_settings);
  g_clear_object (&self->insight_settings);

  g_clear_object (&self->buffer);

  if (self->buffer_bindings != NULL)
    {
      dzl_binding_group_set_source (self->buffer_bindings, NULL);
      g_clear_object (&self->buffer_bindings);
    }

  if (self->buffer_signals != NULL)
    {
      dzl_signal_group_set_target (self->buffer_signals, NULL);
      g_clear_object (&self->buffer_signals);
    }

  GTK_WIDGET_CLASS (ide_editor_page_parent_class)->destroy (widget);
}

static void
ide_editor_page_finalize (GObject *object)
{
  G_OBJECT_CLASS (ide_editor_page_parent_class)->finalize (object);

  DZL_COUNTER_DEC (instances);
}

static void
ide_editor_page_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeEditorPage *self = IDE_EDITOR_PAGE (object);

  switch (prop_id)
    {
    case PROP_AUTO_HIDE_MAP:
      g_value_set_boolean (value, ide_editor_page_get_auto_hide_map (self));
      break;

    case PROP_BUFFER:
      g_value_set_object (value, ide_editor_page_get_buffer (self));
      break;

    case PROP_VIEW:
      g_value_set_object (value, ide_editor_page_get_view (self));
      break;

    case PROP_SEARCH:
      g_value_set_object (value, ide_editor_page_get_search (self));
      break;

    case PROP_SHOW_MAP:
      g_value_set_boolean (value, ide_editor_page_get_show_map (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_page_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  IdeEditorPage *self = IDE_EDITOR_PAGE (object);

  switch (prop_id)
    {
    case PROP_AUTO_HIDE_MAP:
      ide_editor_page_set_auto_hide_map (self, g_value_get_boolean (value));
      break;

    case PROP_BUFFER:
      ide_editor_page_set_buffer (self, g_value_get_object (value));
      break;

    case PROP_SHOW_MAP:
      ide_editor_page_set_show_map (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_page_class_init (IdeEditorPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  IdePageClass *page_class = IDE_PAGE_CLASS (klass);

  object_class->finalize = ide_editor_page_finalize;
  object_class->constructed = ide_editor_page_constructed;
  object_class->get_property = ide_editor_page_get_property;
  object_class->set_property = ide_editor_page_set_property;

  widget_class->destroy = ide_editor_page_destroy;
  widget_class->hierarchy_changed = ide_editor_page_hierarchy_changed;

  page_class->create_split = ide_editor_page_create_split;

  properties [PROP_BUFFER] =
    g_param_spec_object ("buffer",
                         "Buffer",
                         "The buffer for the view",
                         IDE_TYPE_BUFFER,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_SEARCH] =
    g_param_spec_object ("search",
                         "Search",
                         "An search helper for the document",
                         IDE_TYPE_EDITOR_SEARCH,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SHOW_MAP] =
    g_param_spec_boolean ("show-map",
                          "Show Map",
                          "If the overview map should be shown",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_AUTO_HIDE_MAP] =
    g_param_spec_boolean ("auto-hide-map",
                          "Auto Hide Map",
                          "If the overview map should be auto-hidden",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_VIEW] =
    g_param_spec_object ("view",
                         "View",
                         "The view for editing the buffer",
                         IDE_TYPE_SOURCE_VIEW,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-editor/ui/ide-editor-page.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeEditorPage, map);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorPage, map_revealer);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorPage, overlay);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorPage, progress_bar);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorPage, scroller);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorPage, scroller_box);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorPage, search_bar);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorPage, search_revealer);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorPage, modified_revealer);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorPage, modified_cancel_button);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorPage, source_view);
  gtk_widget_class_bind_template_callback (widget_class, ide_editor_page_notify_child_revealed);
  gtk_widget_class_bind_template_callback (widget_class, ide_editor_page_stop_search);

  g_type_ensure (IDE_TYPE_SOURCE_VIEW);
  g_type_ensure (IDE_TYPE_EDITOR_SEARCH_BAR);
}

static void
ide_editor_page_init (IdeEditorPage *self)
{
  DZL_COUNTER_INC (instances);

  gtk_widget_init_template (GTK_WIDGET (self));

  ide_page_set_can_split (IDE_PAGE (self), TRUE);
  ide_page_set_menu_id (IDE_PAGE (self), "ide-editor-page-document-menu");

  self->destroy_cancellable = g_cancellable_new ();

  /* Setup signals to monitor on the buffer. */
  self->buffer_signals = dzl_signal_group_new (IDE_TYPE_BUFFER);

  dzl_signal_group_connect_swapped (self->buffer_signals,
                                    "loaded",
                                    G_CALLBACK (ide_editor_page_buffer_loaded),
                                    self);

  dzl_signal_group_connect_swapped (self->buffer_signals,
                                    "modified-changed",
                                    G_CALLBACK (ide_editor_page_buffer_modified_changed),
                                    self);

  dzl_signal_group_connect_swapped (self->buffer_signals,
                                    "notify::failed",
                                    G_CALLBACK (ide_editor_page_buffer_notify_failed),
                                    self);

  dzl_signal_group_connect_swapped (self->buffer_signals,
                                    "notify::language",
                                    G_CALLBACK (ide_editor_page_buffer_notify_language),
                                    self);

  dzl_signal_group_connect_swapped (self->buffer_signals,
                                    "notify::style-scheme",
                                    G_CALLBACK (ide_editor_page_buffer_notify_style_scheme),
                                    self);
  dzl_signal_group_connect_swapped (self->buffer_signals,
                                    "notify::changed-on-volume",
                                    G_CALLBACK (ide_editor_page__buffer_notify_changed_on_volume),
                                    self);

  g_signal_connect_swapped (self->buffer_signals,
                            "bind",
                            G_CALLBACK (ide_editor_page_bind_signals),
                            self);

  g_signal_connect_object (self->modified_cancel_button,
                           "clicked",
                           G_CALLBACK (ide_editor_page_hide_reload_bar),
                           self,
                           G_CONNECT_SWAPPED);

  /* Setup bindings for the buffer. */
  self->buffer_bindings = dzl_binding_group_new ();
  dzl_binding_group_bind (self->buffer_bindings, "title", self, "title", 0);

  /* Load our custom font for the overview map. */
  gtk_source_map_set_view (self->map, GTK_SOURCE_VIEW (self->source_view));
}

/**
 * ide_editor_page_get_buffer:
 * @self: a #IdeEditorPage
 *
 * Gets the underlying buffer for the view.
 *
 * Returns: (transfer none): An #IdeBuffer
 *
 * Since: 3.32
 */
IdeBuffer *
ide_editor_page_get_buffer (IdeEditorPage *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_PAGE (self), NULL);

  return self->buffer;
}

/**
 * ide_editor_page_get_view:
 * @self: a #IdeEditorPage
 *
 * Gets the #IdeSourceView that is part of the #IdeEditorPage.
 *
 * Returns: (transfer none): An #IdeSourceView
 *
 * Since: 3.32
 */
IdeSourceView *
ide_editor_page_get_view (IdeEditorPage *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_PAGE (self), NULL);

  return self->source_view;
}

/**
 * ide_editor_page_get_language_id:
 * @self: a #IdeEditorPage
 *
 * This is a helper to get the language-id of the underlying buffer.
 *
 * Returns: (nullable): the language-id as a string, or %NULL
 *
 * Since: 3.32
 */
const gchar *
ide_editor_page_get_language_id (IdeEditorPage *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_PAGE (self), NULL);

  if (self->buffer != NULL)
    {
      GtkSourceLanguage *language;

      language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (self->buffer));

      if (language != NULL)
        return gtk_source_language_get_id (language);
    }

  return NULL;
}

/**
 * ide_editor_page_scroll_to_line:
 * @self: a #IdeEditorPage
 * @line: the line to scroll to
 *
 * This is a helper to quickly jump to a given line without all the frills. It
 * will also ensure focus on the editor view, so that refocusing the view
 * afterwards does not cause the view to restore the cursor to the previous
 * location.
 *
 * This will move the insert cursor.
 *
 * Lines start from 0.
 *
 * Since: 3.32
 */
void
ide_editor_page_scroll_to_line (IdeEditorPage *self,
                                guint          line)
{
  ide_editor_page_scroll_to_line_offset (self, line, 0);
}

/**
 * ide_editor_page_scroll_to_line_offset:
 * @self: a #IdeEditorPage
 * @line: the line to scroll to
 * @line_offset: the line offset
 *
 * Like ide_editor_page_scroll_to_line() but allows specifying the
 * line offset (column) to place the cursor on.
 *
 * This will move the insert cursor.
 *
 * Lines and offsets start from 0.
 *
 * If @line_offset is zero, the first non-space character of @line will be
 * used instead.
 *
 * Since: 3.32
 */
void
ide_editor_page_scroll_to_line_offset (IdeEditorPage *self,
                                       guint          line,
                                       guint          line_offset)
{
  GtkTextIter iter;

  g_return_if_fail (IDE_IS_EDITOR_PAGE (self));
  g_return_if_fail (self->buffer != NULL);
  g_return_if_fail (line <= G_MAXINT);

  gtk_widget_grab_focus (GTK_WIDGET (self->source_view));

  gtk_text_buffer_get_iter_at_line_offset (GTK_TEXT_BUFFER (self->buffer), &iter,
                                           line, line_offset);

  if (line_offset == 0)
    {
      while (!gtk_text_iter_ends_line (&iter) &&
             g_unichar_isspace (gtk_text_iter_get_char (&iter)))
        {
          if (!gtk_text_iter_forward_char (&iter))
            break;
        }
    }

  gtk_text_buffer_select_range (GTK_TEXT_BUFFER (self->buffer), &iter, &iter);
  ide_source_view_scroll_to_insert (self->source_view);
}

gboolean
ide_editor_page_get_auto_hide_map (IdeEditorPage *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_PAGE (self), FALSE);

  return self->auto_hide_map;
}

static gboolean
ide_editor_page_auto_hide_cb (gpointer user_data)
{
  IdeEditorPage *self = user_data;

  g_assert (IDE_IS_EDITOR_PAGE (self));

  self->toggle_map_source = 0;
  gtk_revealer_set_reveal_child (self->map_revealer, FALSE);

  return G_SOURCE_REMOVE;
}

static void
ide_editor_page_update_reveal_timer (IdeEditorPage *self)
{
  g_assert (IDE_IS_EDITOR_PAGE (self));

  dzl_clear_source (&self->toggle_map_source);

  if (self->auto_hide_map && gtk_revealer_get_reveal_child (self->map_revealer))
    {
      self->toggle_map_source =
        gdk_threads_add_timeout_seconds_full (G_PRIORITY_LOW,
                                              AUTO_HIDE_TIMEOUT_SECONDS,
                                              ide_editor_page_auto_hide_cb,
                                              g_object_ref (self),
                                              g_object_unref);
    }
}

void
ide_editor_page_set_auto_hide_map (IdeEditorPage *self,
                                   gboolean       auto_hide_map)
{
  g_return_if_fail (IDE_IS_EDITOR_PAGE (self));

  auto_hide_map = !!auto_hide_map;

  if (auto_hide_map != self->auto_hide_map)
    {
      self->auto_hide_map = auto_hide_map;
      ide_editor_page_update_map (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_AUTO_HIDE_MAP]);
    }
}

gboolean
ide_editor_page_get_show_map (IdeEditorPage *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_PAGE (self), FALSE);

  return self->show_map;
}

void
ide_editor_page_set_show_map (IdeEditorPage *self,
                              gboolean       show_map)
{
  g_return_if_fail (IDE_IS_EDITOR_PAGE (self));

  show_map = !!show_map;

  if (show_map != self->show_map)
    {
      self->show_map = show_map;
      g_object_set (self->scroller,
                    "vscrollbar-policy", show_map ? GTK_POLICY_EXTERNAL : GTK_POLICY_AUTOMATIC,
                    NULL);
      ide_editor_page_update_map (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_AUTO_HIDE_MAP]);
    }
}

/**
 * ide_editor_page_set_language:
 * @self: a #IdeEditorPage
 *
 * This is a convenience function to set the language on the underlying
 * #IdeBuffer text buffer.
 *
 * Since: 3.32
 */
void
ide_editor_page_set_language (IdeEditorPage     *self,
                              GtkSourceLanguage *language)
{
  g_return_if_fail (IDE_IS_EDITOR_PAGE (self));
  g_return_if_fail (!language || GTK_SOURCE_IS_LANGUAGE (language));

  gtk_source_buffer_set_language (GTK_SOURCE_BUFFER (self->buffer), language);
}

/**
 * ide_editor_page_get_language:
 * @self: a #IdeEditorPage
 *
 * Gets the #GtkSourceLanguage that is used by the underlying buffer.
 *
 * Returns: (transfer none) (nullable): a #GtkSourceLanguage or %NULL.
 *
 * Since: 3.32
 */
GtkSourceLanguage *
ide_editor_page_get_language (IdeEditorPage *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_PAGE (self), NULL);

  return gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (self->buffer));
}

/**
 * ide_editor_page_move_next_error:
 * @self: a #IdeEditorPage
 *
 * Moves to the next error, if any.
 *
 * If there is no error, the insertion cursor is not moved.
 *
 * Since: 3.32
 */
void
ide_editor_page_move_next_error (IdeEditorPage *self)
{
  g_return_if_fail (IDE_IS_EDITOR_PAGE (self));

  g_signal_emit_by_name (self->source_view, "move-error", GTK_DIR_DOWN);
}

/**
 * ide_editor_page_move_previous_error:
 * @self: a #IdeEditorPage
 *
 * Moves the insertion cursor to the previous error.
 *
 * If there is no error, the insertion cursor is not moved.
 *
 * Since: 3.32
 */
void
ide_editor_page_move_previous_error (IdeEditorPage *self)
{
  g_return_if_fail (IDE_IS_EDITOR_PAGE (self));

  g_signal_emit_by_name (self->source_view, "move-error", GTK_DIR_UP);
}

/**
 * ide_editor_page_move_next_search_result:
 * @self: a #IdeEditorPage
 *
 * Moves the insertion cursor to the next search result.
 *
 * If there is no search result, the insertion cursor is not moved.
 *
 * Since: 3.32
 */
void
ide_editor_page_move_next_search_result (IdeEditorPage *self)
{
  g_return_if_fail (IDE_IS_EDITOR_PAGE (self));
  g_return_if_fail (self->destroy_cancellable != NULL);
  g_return_if_fail (self->buffer != NULL);

  ide_editor_search_move (self->search, IDE_EDITOR_SEARCH_NEXT);
}

/**
 * ide_editor_page_move_previous_search_result:
 * @self: a #IdeEditorPage
 *
 * Moves the insertion cursor to the previous search result.
 *
 * If there is no search result, the insertion cursor is not moved.
 *
 * Since: 3.32
 */
void
ide_editor_page_move_previous_search_result (IdeEditorPage *self)
{
  g_return_if_fail (IDE_IS_EDITOR_PAGE (self));
  g_return_if_fail (self->destroy_cancellable != NULL);
  g_return_if_fail (self->buffer != NULL);

  ide_editor_search_move (self->search, IDE_EDITOR_SEARCH_PREVIOUS);
}

/**
 * ide_editor_page_get_search:
 * @self: a #IdeEditorPage
 *
 * Gets the #IdeEditorSearch used to search within the document.
 *
 * Returns: (transfer none): An #IdeEditorSearch
 *
 * Since: 3.32
 */
IdeEditorSearch *
ide_editor_page_get_search (IdeEditorPage *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_PAGE (self), NULL);

  return self->search;
}

/**
 * ide_editor_page_get_file:
 * @self: a #IdeEditorPage
 *
 * Gets the #GFile that represents the current file. This may be a temporary
 * file, but a #GFile will still be used for the temporary file.
 *
 * Returns: (transfer none): a #GFile for the current buffer
 *
 * Since: 3.32
 */
GFile *
ide_editor_page_get_file (IdeEditorPage *self)
{
  IdeBuffer *buffer;

  g_return_val_if_fail (IDE_IS_EDITOR_PAGE (self), NULL);

  if ((buffer = ide_editor_page_get_buffer (self)))
    return ide_buffer_get_file (buffer);

  return NULL;
}
