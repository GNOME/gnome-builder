/* ide-editor-search.c
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

#define G_LOG_DOMAIN "ide-editor-search"

#include "config.h"

#include <dazzle.h>
#include <libide-sourceview.h>
#include <string.h>

#include "ide-editor-search.h"

/**
 * SECTION:ide-editor-search
 * @title: IdeEditorSearch
 *
 * The #IdeEditorSearch object manages the search features associated
 * with a single #IdeEditorView (and it's view of the underlying text
 * buffer).
 *
 * This object is meant to help reduce the number of layers performing
 * search on the underlying buffer as well as track highlighting based
 * on focus, performance considerations, and directional movements in
 * a unified manner.
 *
 * Additionally, it provides an addin layer to highlight similar words
 * when then buffer selection changes.
 *
 * Since: 3.32
 */

struct _IdeEditorSearch
{
  GObject                  parent_instance;

  GtkSourceView           *view;
  GtkSourceSearchContext  *context;
  GtkSourceSearchSettings *settings;
  gchar                   *replacement_text;
  DzlSignalGroup          *buffer_signals;
  DzlSignalGroup          *view_signals;
  GCancellable            *lookahead_cancellable;

  gint                     interactive;
  gdouble                  scroll_value;

  guint                    match_count;
  guint                    match_position;

  guint                    repeat;

  GdkRGBA                  search_shadow_rgba;
  GdkRGBA                  bubble_color1;
  GdkRGBA                  bubble_color2;

  IdeEditorSearchSelect    extend_selection : 2;
  guint                    reverse : 1;
  guint                    show_search_bubbles : 1;
  guint                    show_search_shadow : 1;
  guint                    visible : 1;
};

enum {
  PROP_0,
  PROP_ACTIVE,
  PROP_AT_WORD_BOUNDARIES,
  PROP_CASE_SENSITIVE,
  PROP_EXTEND_SELECTION,
  PROP_MATCH_COUNT,
  PROP_MATCH_POSITION,
  PROP_REGEX_ENABLED,
  PROP_REPEAT,
  PROP_REPLACEMENT_TEXT,
  PROP_REVERSE,
  PROP_SEARCH_TEXT,
  PROP_VIEW,
  PROP_VISIBLE,
  N_PROPS
};

static void ide_editor_search_actions_move_next        (IdeEditorSearch *self,
                                                        GVariant        *param);
static void ide_editor_search_actions_move_previous    (IdeEditorSearch *self,
                                                        GVariant        *param);
static void ide_editor_search_actions_replace          (IdeEditorSearch *self,
                                                        GVariant        *param);
static void ide_editor_search_actions_replace_all      (IdeEditorSearch *self,
                                                        GVariant        *param);
static void ide_editor_search_actions_at_word_boundary (IdeEditorSearch *self,
                                                        GVariant        *param);

DZL_DEFINE_ACTION_GROUP (IdeEditorSearch, ide_editor_search, {
  { "move-next", ide_editor_search_actions_move_next },
  { "move-previous", ide_editor_search_actions_move_previous },
  { "replace", ide_editor_search_actions_replace },
  { "replace-all", ide_editor_search_actions_replace_all },
  { "at-word-boundaries", ide_editor_search_actions_at_word_boundary, "b" },
})

G_DEFINE_TYPE_WITH_CODE (IdeEditorSearch, ide_editor_search, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, ide_editor_search_init_action_group))

static GParamSpec *properties [N_PROPS];

static void
draw_bezel (cairo_t                     *cr,
            const cairo_rectangle_int_t *rect,
            guint                        radius,
            const GdkRGBA               *rgba)
{
  GdkRectangle r;

  r.x = rect->x - radius;
  r.y = rect->y - radius;
  r.width = rect->width + (radius * 2);
  r.height = rect->height + (radius * 2);

  gdk_cairo_set_source_rgba (cr, rgba);
  dzl_cairo_rounded_rectangle (cr, &r, radius, radius);
  cairo_fill (cr);
}

static void
add_match (GtkTextView       *text_view,
           cairo_region_t    *region,
           const GtkTextIter *begin,
           const GtkTextIter *end)
{
  GdkRectangle begin_rect;
  GdkRectangle end_rect;
  cairo_rectangle_int_t rect;

  g_assert (GTK_IS_TEXT_VIEW (text_view));
  g_assert (region != NULL);
  g_assert (begin != NULL);
  g_assert (end != NULL);

  /* NOTE: @end is not inclusive of the match. */
  if (gtk_text_iter_get_line (begin) == gtk_text_iter_get_line (end))
    {
      gtk_text_view_get_iter_location (text_view, begin, &begin_rect);
      gtk_text_view_buffer_to_window_coords (text_view, GTK_TEXT_WINDOW_TEXT,
                                             begin_rect.x, begin_rect.y,
                                             &begin_rect.x, &begin_rect.y);
      gtk_text_view_get_iter_location (text_view, end, &end_rect);
      gtk_text_view_buffer_to_window_coords (text_view, GTK_TEXT_WINDOW_TEXT,
                                             end_rect.x, end_rect.y,
                                             &end_rect.x, &end_rect.y);
      rect.x = begin_rect.x;
      rect.y = begin_rect.y;
      rect.width = end_rect.x - begin_rect.x;
      rect.height = MAX (begin_rect.height, end_rect.height);
      cairo_region_union_rectangle (region, &rect);
      return;
    }

  /*
   * TODO: Add support for multi-line matches. When @begin and @end are not
   *       on the same line, we need to add the match region to @region so
   *       ide_source_view_draw_search_bubbles() can draw search bubbles
   *       around it.
   */
}

static guint
add_matches (GtkTextView            *text_view,
             cairo_region_t         *region,
             GtkSourceSearchContext *search_context,
             const GtkTextIter      *begin,
             const GtkTextIter      *end)
{
  GtkTextIter first_begin;
  GtkTextIter new_begin;
  GtkTextIter match_begin;
  GtkTextIter match_end;
  gboolean has_wrapped;
  guint count = 1;

  g_assert (GTK_IS_TEXT_VIEW (text_view));
  g_assert (region);
  g_assert (GTK_SOURCE_IS_SEARCH_CONTEXT (search_context));
  g_assert (begin);
  g_assert (end);

  if (!gtk_source_search_context_forward (search_context,
                                          begin,
                                          &first_begin,
                                          &match_end,
                                          &has_wrapped))
    return 0;

  add_match (text_view, region, &first_begin, &match_end);

  for (;;)
    {
      gtk_text_iter_assign (&new_begin, &match_end);

      if (gtk_source_search_context_forward (search_context,
                                             &new_begin,
                                             &match_begin,
                                             &match_end,
                                             &has_wrapped) &&
          (gtk_text_iter_compare (&match_begin, end) < 0) &&
          (gtk_text_iter_compare (&first_begin, &match_begin) != 0))
        {
          add_match (text_view, region, &match_begin, &match_end);
          count++;
          continue;
        }

      break;
    }

  return count;
}

static void
ide_editor_search_draw_bubbles (IdeEditorSearch *self,
                                cairo_t         *cr,
                                IdeSourceView   *view)
{
  cairo_region_t *clip_region;
  cairo_region_t *match_region;
  GdkRectangle area;
  GtkTextIter begin;
  GtkTextIter end;
  cairo_rectangle_int_t r;
  guint count;
  gint buffer_x = 0;
  gint buffer_y = 0;
  gint n;
  gint i;

  g_assert (IDE_IS_EDITOR_SEARCH (self));
  g_assert (cr != NULL);
  g_assert (IDE_IS_SOURCE_VIEW (view));

  if (!gdk_cairo_get_clip_rectangle (cr, &area))
    gtk_widget_get_allocation (GTK_WIDGET (self), &area);

  gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (view),
                                         GTK_TEXT_WINDOW_TEXT,
                                         area.x, area.y,
                                         &buffer_x, &buffer_y);
  gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (view), &begin,
                                      buffer_x, buffer_y);
  gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (view), &end,
                                      buffer_x + area.width,
                                      buffer_y + area.height);

  clip_region = cairo_region_create_rectangle (&area);
  match_region = cairo_region_create ();
  count = add_matches (GTK_TEXT_VIEW (view),
                       match_region,
                       self->context,
                       &begin, &end);

  cairo_region_subtract (clip_region, match_region);

  if (self->show_search_shadow &&
      (count || gtk_source_search_context_get_occurrences_count (self->context)))
    {
      gdk_cairo_region (cr, clip_region);
      gdk_cairo_set_source_rgba (cr, &self->search_shadow_rgba);
      cairo_fill (cr);
    }

  gdk_cairo_region (cr, clip_region);
  cairo_clip (cr);

  n = cairo_region_num_rectangles (match_region);

  for (i = 0; i < n; i++)
    {
      cairo_region_get_rectangle (match_region, i, &r);
      draw_bezel (cr, &r, 3, &self->bubble_color1);
      draw_bezel (cr, &r, 2, &self->bubble_color2);
    }

  cairo_region_destroy (clip_region);
  cairo_region_destroy (match_region);
}

static void
ide_editor_search_settings_notify (IdeEditorSearch         *self,
                                   GParamSpec              *pspec,
                                   GtkSourceSearchSettings *settings)
{
  g_assert (IDE_IS_EDITOR_SEARCH (self));
  g_assert (pspec != NULL);
  g_assert (GTK_SOURCE_IS_SEARCH_SETTINGS (settings));

  /* Proxy the notify from the settings to our instance */
  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (self), pspec->name);
  if (pspec != NULL)
    g_object_notify_by_pspec (G_OBJECT (self), pspec);
}

static void
ide_editor_search_notify_style_scheme (IdeEditorSearch *self,
                                       GParamSpec      *pspec,
                                       GtkSourceBuffer *buffer)
{
  g_assert (IDE_IS_EDITOR_SEARCH (self));
  g_assert (GTK_SOURCE_IS_BUFFER (buffer));

  if (self->context != NULL)
    {
      GtkSourceStyleScheme *style_scheme;
      GtkSourceStyle *match_style = NULL;

      style_scheme = gtk_source_buffer_get_style_scheme (buffer);

      if (style_scheme != NULL)
        match_style = gtk_source_style_scheme_get_style (style_scheme, "search-match");

      memset (&self->bubble_color1, 0, sizeof self->bubble_color1);
      memset (&self->bubble_color2, 0, sizeof self->bubble_color2);

      if (match_style != NULL)
        {
          g_autofree gchar *background = NULL;
          GdkRGBA rgba;

          g_object_get (match_style,
                        "background", &background,
                        NULL);

          if (gdk_rgba_parse (&rgba, background))
            {
              dzl_rgba_shade (&rgba, &self->bubble_color1, 0.8);
              dzl_rgba_shade (&rgba, &self->bubble_color2, 1.1);
            }
        }

      gtk_source_search_context_set_match_style (self->context, match_style);
    }
}

static void
ide_editor_search_bind_buffer (IdeEditorSearch *self,
                               GtkSourceBuffer *buffer,
                               DzlSignalGroup  *buffer_signals)
{
  g_assert (IDE_IS_EDITOR_SEARCH (self));
  g_assert (GTK_SOURCE_IS_BUFFER (buffer));
  g_assert (DZL_IS_SIGNAL_GROUP (buffer_signals));

  ide_editor_search_notify_style_scheme (self, NULL, buffer);
}

static void
ide_editor_search_unbind_buffer (IdeEditorSearch *self,
                                 DzlSignalGroup  *buffer_signals)
{
  g_assert (IDE_IS_EDITOR_SEARCH (self));
  g_assert (DZL_IS_SIGNAL_GROUP (buffer_signals));

  g_clear_object (&self->context);
}

static void
ide_editor_search_dispose (GObject *object)
{
  IdeEditorSearch *self = (IdeEditorSearch *)object;

  if (self->buffer_signals != NULL)
    {
      dzl_signal_group_set_target (self->buffer_signals, NULL);
      g_clear_object (&self->buffer_signals);
    }

  if (self->view_signals != NULL)
    {
      dzl_signal_group_set_target (self->view_signals, NULL);
      g_clear_object (&self->view_signals);
    }

  g_clear_object (&self->view);
  g_clear_object (&self->context);
  g_clear_object (&self->settings);
  g_clear_object (&self->lookahead_cancellable);
  g_clear_pointer (&self->replacement_text, g_free);

  G_OBJECT_CLASS (ide_editor_search_parent_class)->dispose (object);
}

static void
ide_editor_search_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  IdeEditorSearch *self = IDE_EDITOR_SEARCH (object);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, ide_editor_search_get_active (self));
      break;

    case PROP_CASE_SENSITIVE:
      g_value_set_boolean (value, ide_editor_search_get_case_sensitive (self));
      break;

    case PROP_EXTEND_SELECTION:
      g_value_set_enum (value, ide_editor_search_get_extend_selection (self));
      break;

    case PROP_VIEW:
      g_value_set_object (value, self->view);
      break;

    case PROP_SEARCH_TEXT:
      g_value_set_string (value, ide_editor_search_get_search_text (self));
      break;

    case PROP_VISIBLE:
      g_value_set_boolean (value, ide_editor_search_get_visible (self));
      break;

    case PROP_REGEX_ENABLED:
      g_value_set_boolean (value, ide_editor_search_get_regex_enabled (self));
      break;

    case PROP_REPEAT:
      g_value_set_uint (value, ide_editor_search_get_repeat (self));
      break;

    case PROP_REPLACEMENT_TEXT:
      g_value_set_string (value, ide_editor_search_get_replacement_text (self));
      break;

    case PROP_AT_WORD_BOUNDARIES:
      g_value_set_boolean (value, ide_editor_search_get_at_word_boundaries (self));
      break;

    case PROP_MATCH_COUNT:
      g_value_set_uint (value, ide_editor_search_get_match_count (self));
      break;

    case PROP_MATCH_POSITION:
      g_value_set_uint (value, ide_editor_search_get_match_position (self));
      break;

    case PROP_REVERSE:
      g_value_set_boolean (value, ide_editor_search_get_reverse (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_search_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  IdeEditorSearch *self = IDE_EDITOR_SEARCH (object);

  switch (prop_id)
    {
    case PROP_CASE_SENSITIVE:
      ide_editor_search_set_case_sensitive (self, g_value_get_boolean (value));
      break;

    case PROP_EXTEND_SELECTION:
      ide_editor_search_set_extend_selection (self, g_value_get_enum (value));
      break;

    case PROP_SEARCH_TEXT:
      ide_editor_search_set_search_text (self, g_value_get_string (value));
      break;

    case PROP_VISIBLE:
      ide_editor_search_set_visible (self, g_value_get_boolean (value));
      break;

    case PROP_REGEX_ENABLED:
      ide_editor_search_set_regex_enabled (self, g_value_get_boolean (value));
      break;

    case PROP_REPEAT:
      ide_editor_search_set_repeat (self, g_value_get_uint (value));
      break;

    case PROP_REPLACEMENT_TEXT:
      ide_editor_search_set_replacement_text (self, g_value_get_string (value));
      break;

    case PROP_AT_WORD_BOUNDARIES:
      ide_editor_search_set_at_word_boundaries (self, g_value_get_boolean (value));
      break;

    case PROP_REVERSE:
      ide_editor_search_set_reverse (self, g_value_get_boolean (value));
      break;

    case PROP_VIEW:
      self->view = g_value_dup_object (value);
      dzl_signal_group_set_target (self->view_signals, self->view);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_search_class_init (IdeEditorSearchClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_editor_search_dispose;
  object_class->get_property = ide_editor_search_get_property;
  object_class->set_property = ide_editor_search_set_property;

  /**
   * IdeEditorSearch:active:
   *
   * The "active" property is %TRUE when their is an active search
   * in progress.
   *
   * Since: 3.32
   */
  properties [PROP_ACTIVE] =
    g_param_spec_boolean ("active", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * IdeEditorSearch:view:
   *
   * The "view" property is the underlying #GtkSourceView that
   * is being searched. This must be set when creating the
   * #IdeEditorSearch and may not be changed after construction.
   *
   * Since: 3.32
   */
  properties [PROP_VIEW] =
    g_param_spec_object ("view", NULL,  NULL,
                         GTK_SOURCE_TYPE_VIEW,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  /**
   * IdeEditorSearch:at-word-boundaries:
   *
   * The "at-word-boundaries" property specifies if the search-text must
   * only be matched starting from the beginning of a word.
   *
   * Since: 3.32
   */
  properties [PROP_AT_WORD_BOUNDARIES] =
    g_param_spec_boolean ("at-word-boundaries", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * IdeEditorSearch:case-sensitive:
   *
   * The "case-sensitive" property specifies if the search text should
   * be case sensitive.
   *
   * Since: 3.32
   */
  properties [PROP_CASE_SENSITIVE] =
    g_param_spec_boolean ("case-sensitive", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * IdeEditorSearch:extend-selection:
   *
   * The "extend-selection" property specifies that the selection within
   * the editor should be extended as the user navigates between search
   * results.
   *
   * Since: 3.32
   */
  properties [PROP_EXTEND_SELECTION] =
    g_param_spec_enum ("extend-selection",
                       "Extend Selection",
                       "If the selection should be extended when moving through results",
                       IDE_TYPE_EDITOR_SEARCH_SELECT,
                       IDE_EDITOR_SEARCH_SELECT_NONE,
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * IdeEditorSearch:match-count:
   *
   * The "match-count" property contains the number of matches that have
   * been discovered. This is reset to zeor when the #IdeEditorSearch
   * determines it can destroy it's #GtkSourceSearchContext.
   *
   * Generally, you should only rely on it's accuracy after calling
   * ide_editor_search_begin_interactive() and before calling
   * ide_editor_search_end_interactive().
   *
   * Since: 3.32
   */
  properties [PROP_MATCH_COUNT] =
    g_param_spec_uint ("match-count", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * IdeEditorSearch:match-position:
   *
   * The "match-position" property contains the position within the
   * discovered search results for which the insertion cursor is placed.
   *
   * This value starts from 1, and 0 indicates that the insertion cursor
   * is not placed within the a search result.
   *
   * Since: 3.32
   */
  properties [PROP_MATCH_POSITION] =
    g_param_spec_uint ("match-position", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * IdeEditorSearch:repeat:
   *
   * The number of times to repeat a move operation when calling
   * ide_editor_search_move(). This allows for stateful moves when as
   * might be necessary when activating keybindings.
   *
   * This property will be cleared after an attempt to move.
   *
   * Since: 3.32
   */
  properties [PROP_REPEAT] =
    g_param_spec_uint ("repeat", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * IdeEditorSearch:regex-enabled:
   *
   * The "regex-enabled" property determines if #GRegex should be used
   * to scan for the #IdeEditorSearch:search-text. Doing so allows the
   * user to search using common regex values such as "foo.*bar". It
   * also allows for capture groups to be used in replacement text.
   *
   * Since: 3.32
   */
  properties [PROP_REGEX_ENABLED] =
    g_param_spec_boolean ("regex-enabled", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * IdeEditorSearch:replacement-text:
   *
   * The "replacement-text" property determines the text to be used when
   * performing search and replace with ide_editor_search_replace() or
   * ide_editor_search_replace_all().
   *
   * If #IdeEditorSearch:regex-enabled is %TRUE, then the user may use
   * references to capture groups specified in #IdeEditorSearch:search-text.
   *
   * Since: 3.32
   */
  properties [PROP_REPLACEMENT_TEXT] =
    g_param_spec_string ("replacement-text", NULL, NULL, NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * IdeEditorSearch:reverse:
   *
   * The "reverse" property determines if relative directions should be
   * switched, so next is backward, and previous is forward.
   *
   * Since: 3.32
   */
  properties [PROP_REVERSE] =
    g_param_spec_boolean ("reverse", NULL, NULL, FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * IdeEditorSearch:search-text:
   *
   * The "search-text" property contains the text to search within the buffer.
   *
   * If the #IdeEditorSearch:regex-enabled property is set to %TRUE, then
   * the user may use regular expressions supported by #GRegex to scan the
   * buffer. They may also specify capture groups to use in search and
   * replace.
   *
   * Since: 3.32
   */
  properties [PROP_SEARCH_TEXT] =
    g_param_spec_string ("search-text", NULL, NULL, NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * IdeEditorSearch:visible:
   *
   * The "visible" property is used to specify if the search results should
   * be highlighted in the buffer. Generally, you'll want this off while the
   * interactive search is hidden as it allows the #IdeEditorSearch to perform
   * various optimizations.
   *
   * However, some cases, such as Vim search movements, may want to show
   * the search highlights, but are not within an interactive search.
   *
   * Since: 3.32
   */
  properties [PROP_VISIBLE] =
    g_param_spec_string ("visible", NULL, NULL, FALSE,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_editor_search_init (IdeEditorSearch *self)
{
  self->show_search_bubbles = TRUE;

  self->settings = gtk_source_search_settings_new ();

  g_signal_connect_swapped (self->settings,
                            "notify",
                            G_CALLBACK (ide_editor_search_settings_notify),
                            self);

  self->buffer_signals = dzl_signal_group_new (GTK_SOURCE_TYPE_BUFFER);

  g_signal_connect_swapped (self->buffer_signals,
                            "bind",
                            G_CALLBACK (ide_editor_search_bind_buffer),
                            self);

  g_signal_connect_swapped (self->buffer_signals,
                            "unbind",
                            G_CALLBACK (ide_editor_search_unbind_buffer),
                            self);

  dzl_signal_group_connect_swapped (self->buffer_signals,
                                    "notify::style-scheme",
                                    G_CALLBACK (ide_editor_search_notify_style_scheme),
                                    self);

  self->view_signals = dzl_signal_group_new (IDE_TYPE_SOURCE_VIEW);

  dzl_signal_group_connect_swapped (self->view_signals,
                                    "draw-bubbles",
                                    G_CALLBACK (ide_editor_search_draw_bubbles),
                                    self);

  dzl_signal_group_block (self->view_signals);
}

/**
 * ide_editor_search_new:
 *
 * Creates a new #IdeEditorSearch instance for the #GtkSourceView.
 * You should only create one of these per #IdeEditorView.
 *
 * Returns: (transfer full): A new #IdeEditorSearch instance
 *
 * Since: 3.32
 */
IdeEditorSearch *
ide_editor_search_new (GtkSourceView *view)
{
  return g_object_new (IDE_TYPE_EDITOR_SEARCH,
                       "view", view,
                       NULL);
}

static void
ide_editor_search_notify_occurrences_count (IdeEditorSearch        *self,
                                            GParamSpec             *pspec,
                                            GtkSourceSearchContext *context)
{
  GtkTextBuffer *buffer;
  GtkTextView *view;
  GtkTextIter begin;
  GtkTextIter end;
  gint count;
  gint pos;

  g_assert (IDE_IS_EDITOR_SEARCH (self));
  g_assert (GTK_SOURCE_IS_SEARCH_CONTEXT (context));

  if (self->view == NULL)
    return;

  count = gtk_source_search_context_get_occurrences_count (context);
  self->match_count = MAX (0, count);

  view = GTK_TEXT_VIEW (self->view);
  buffer = gtk_text_view_get_buffer (view);
  gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);
  gtk_text_iter_order (&begin, &end);

  pos = gtk_source_search_context_get_occurrence_position (context, &begin, &end);
  self->match_position = MAX (0, pos);

  ide_editor_search_set_action_enabled (self, "replace", pos > 0 && count > 0);
  ide_editor_search_set_action_enabled (self, "replace-all", count > 0);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MATCH_COUNT]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MATCH_POSITION]);
}

static GtkSourceSearchContext *
ide_editor_search_acquire_context (IdeEditorSearch *self)
{
  g_assert (IDE_IS_EDITOR_SEARCH (self));
  g_assert (self->settings != NULL);
  g_assert (self->view != NULL);

  if (self->context == NULL)
    {
      GtkSourceBuffer *buffer;
      GtkTextView *view;
      gboolean highlight;

      view = GTK_TEXT_VIEW (self->view);
      buffer = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (view));

      /* Create our new context */
      self->context = gtk_source_search_context_new (buffer, self->settings);

      /* Update match info as the context discovers matches */
      g_signal_connect_object (self->context,
                               "notify::occurrences-count",
                               G_CALLBACK (ide_editor_search_notify_occurrences_count),
                               self,
                               G_CONNECT_SWAPPED);

      /* Determine if we should highlight immediately */
      highlight = self->visible || self->interactive > 0;
      gtk_source_search_context_set_highlight (self->context, highlight);

      /* Update text tag styling */
      ide_editor_search_notify_style_scheme (self, NULL, buffer);

      /* Draw bubbles while the context is live */
      dzl_signal_group_unblock (self->view_signals);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ACTIVE]);
    }

  return self->context;
}

static void
ide_editor_search_release_context (IdeEditorSearch *self)
{
  g_assert (IDE_IS_EDITOR_SEARCH (self));

  if (self->context != NULL && self->interactive == 0 && self->visible == FALSE)
    {
      g_signal_handlers_disconnect_by_func (self->context,
                                            G_CALLBACK (ide_editor_search_notify_occurrences_count),
                                            self);
      g_clear_object (&self->context);
      dzl_signal_group_block (self->view_signals);
      gtk_widget_queue_draw (GTK_WIDGET (self->view));
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ACTIVE]);
    }
}

/**
 * ide_editor_search_set_case_sensitive:
 * @self: An #IdeEditorSearch
 * @case_sensitive: %TRUE if the search should be case-sensitive
 *
 * See also: #GtkSourceSearchSettings:case-sensitive
 * Since: 3.32
 */
void
ide_editor_search_set_case_sensitive (IdeEditorSearch *self,
                                      gboolean         case_sensitive)
{
  g_return_if_fail (IDE_IS_EDITOR_SEARCH (self));

  gtk_source_search_settings_set_case_sensitive (self->settings, case_sensitive);
}

/**
 * ide_editor_search_get_case_sensitive:
 * @self: An #IdeEditorSearch
 *
 * Gets if the search should be case sensitive.
 *
 * Returns: %TRUE if the search is case-sensitive.
 *
 * See also: #GtkSourceSearchSettings:case-sensitive
 * Since: 3.32
 */
gboolean
ide_editor_search_get_case_sensitive (IdeEditorSearch *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_SEARCH (self), FALSE);

  return gtk_source_search_settings_get_case_sensitive (self->settings);
}

static void
ide_editor_search_scan_forward_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  GtkSourceSearchContext *context = (GtkSourceSearchContext *)object;
  g_autoptr(IdeEditorSearch) self = user_data;
  GtkTextIter begin;
  GtkTextIter end;
  gboolean r;

  g_assert (GTK_SOURCE_IS_SEARCH_CONTEXT (context));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_EDITOR_SEARCH (self));

  if (self->view == NULL)
    return;

  r = gtk_source_search_context_forward_finish (context, result, &begin, &end, NULL, NULL);

  if (r == TRUE)
    {
      /* Scan forward to the location of the next match */
      ide_source_view_scroll_to_iter (IDE_SOURCE_VIEW (self->view),
                                      &begin,
                                      0.0,
                                      IDE_SOURCE_SCROLL_BOTH,
                                      1.0,
                                      0.5,
                                      FALSE);
    }
  else if (self->interactive > 0)
    {
      GtkAdjustment *adj;

      /* No match was found, restore to our position pre-search */
      adj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (self->view));
      gtk_adjustment_set_value (adj, self->scroll_value);
    }

  ide_editor_search_notify_occurrences_count (self, NULL, context);
}

/**
 * ide_editor_search_set_search_text:
 * @self: An #IdeEditorSearch
 * @search_text: (nullable): The search text or %NULL
 *
 * See also: #GtkSourceSearchSettings:search-text
 *
 * Since: 3.32
 */
void
ide_editor_search_set_search_text (IdeEditorSearch *self,
                                   const gchar     *search_text)
{
  g_autofree gchar *unescaped = NULL;

  g_return_if_fail (IDE_IS_EDITOR_SEARCH (self));

  if (!ide_editor_search_get_regex_enabled (self))
    search_text = unescaped = gtk_source_utils_unescape_search_text (search_text);

  gtk_source_search_settings_set_search_text (self->settings, search_text);

  /*
   * If we are in an interactive search, start scrolling to the next search
   * result (without moving the insertion cursor). Upon completion of the
   * interactive search, we will scroll back to our original position if
   * no move was made.
   */
  if (self->interactive > 0 && self->view != NULL)
    {
      GtkSourceSearchContext *context;
      GtkTextBuffer *buffer;
      GtkTextIter begin;
      GtkTextIter end;

      /* Cancel any previous lookahead */
      g_cancellable_cancel (self->lookahead_cancellable);
      g_clear_object (&self->lookahead_cancellable);

      /* Setup some necessary objects */
      context = ide_editor_search_acquire_context (self);
      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->view));
      self->lookahead_cancellable = g_cancellable_new ();

      /* Get our start position for the forward scan */
      gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);
      gtk_text_iter_order (&begin, &end);
      gtk_text_iter_forward_char (&end);

      /* Ensure we wrap around to the beginning of the buffer */
      gtk_source_search_settings_set_wrap_around (self->settings, TRUE);

      gtk_source_search_context_forward_async (context,
                                               &end,
                                               self->lookahead_cancellable,
                                               ide_editor_search_scan_forward_cb,
                                               g_object_ref (self));
    }
}

/**
 * ide_editor_search_get_search_text:
 * @self: An #IdeEditorSearch
 *
 * Gets the search-text currently being searched.
 *
 * Returns: (nullable): The search text or %NULL
 *
 * Since: 3.32
 */
const gchar *
ide_editor_search_get_search_text (IdeEditorSearch *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_SEARCH (self), NULL);

  return gtk_source_search_settings_get_search_text (self->settings);
}

/**
 * ide_editor_search_get_search_text_invalid:
 * @self: An #IdeEditorSearch
 * @invalid_begin: (nullable) (out): a begin location for the invalid range
 * @invalid_end: (nullable) (out): an end location for the invalid range
 *
 * Checks to see if the search text contains invalid contents, such
 * as an invalid regex.
 *
 * Returns: %TRUE if the search text contains invalid content. If %TRUE,
 *   then @invalid_begin and @invalid_end is set.
 *
 * Since: 3.32
 */
gboolean
ide_editor_search_get_search_text_invalid (IdeEditorSearch  *self,
                                           guint            *invalid_begin,
                                           guint            *invalid_end,
                                           GError          **error)
{
  const gchar *text;
  guint dummy;

  g_return_val_if_fail (IDE_IS_EDITOR_SEARCH (self), FALSE);

  /* Fallback to avoid dereference checks */
  invalid_begin = invalid_begin ? invalid_begin : &dummy;
  invalid_end = invalid_end ? invalid_end : &dummy;

  text = gtk_source_search_settings_get_search_text (self->settings);
  if (text == NULL)
    text = "";

  if (ide_editor_search_get_regex_enabled (self))
    {
      g_autoptr(GRegex) regex = NULL;
      g_autoptr(GError) local_error = NULL;

      if (NULL == (regex = g_regex_new (text, 0, 0, &local_error)))
        {
          const gchar *endptr;

          *invalid_begin = 0;
          *invalid_end = strlen (text);

          /*
           * Error from GRegex will look something like:
           * Error while compiling regular expression foo\\\\ at char 7: (message)
           */

          if (NULL != (endptr = strrchr (local_error->message, ':')))
            {
              while (endptr > local_error->message)
                {
                  if (g_ascii_isdigit (*(endptr - 1)))
                    {
                      endptr--;
                    }
                  else
                    {
                      *invalid_begin = (guint)g_ascii_strtoull (endptr, NULL, 10);
                      /* Translate to zero based index */
                      if ((*invalid_begin) > 0)
                        (*invalid_begin)--;
                      break;
                    }
                }

              g_propagate_error (error, g_steal_pointer (&local_error));
            }

          return TRUE;
        }
    }

  *invalid_begin = 0;
  *invalid_end = 0;

  return FALSE;
}

/**
 * ide_editor_search_set_visible:
 * @self: An #IdeEditorSearch
 * @visible: if the search results should be visible
 *
 * Sets the visibility of the search results. You might want to disable
 * the visibility of search results when the user has requested them to
 * be dismissed.
 *
 * This will allow the user to still make search movements based on the
 * previous search request, and re-enable visibility upon doing so.
 *
 * Since: 3.32
 */
void
ide_editor_search_set_visible (IdeEditorSearch *self,
                               gboolean         visible)
{
  g_return_if_fail (IDE_IS_EDITOR_SEARCH (self));

  visible = !!visible;

  if (visible != self->visible)
    {
      self->visible = visible;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_VISIBLE]);
      if (!visible)
        ide_editor_search_release_context (self);
    }
}

/**
 * ide_editor_search_get_visible:
 * @self: An #IdeEditorSearch
 *
 * Gets the #IdeEditorSearch:visible property. This is true if the current
 * search text should be highlighted in the editor.
 *
 * Returns: %TRUE if the current search should be highlighted.
 *
 * Since: 3.32
 */
gboolean
ide_editor_search_get_visible (IdeEditorSearch *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_SEARCH (self), FALSE);

  return self->visible;
}

/**
 * ide_editor_search_set_regex_enabled:
 * @self: An #IdeEditorSearch
 * @regex_enabled: If regex search should be used
 *
 * See also: #GtkSourceSearchSettings:regex-enabled
 *
 * Since: 3.32
 */
void
ide_editor_search_set_regex_enabled (IdeEditorSearch *self,
                                     gboolean         regex_enabled)
{
  g_return_if_fail (IDE_IS_EDITOR_SEARCH (self));

  gtk_source_search_settings_set_regex_enabled (self->settings, regex_enabled);
  ide_editor_search_set_action_state (self,
                                      "regex-enabled",
                                      g_variant_new_boolean (regex_enabled));
}

/**
 * ide_editor_search_get_regex_enabled:
 * @self: An #IdeEditorSearch
 *
 * Gets the #IdeEditorSearch:regex-enabled property. This is true if the
 * search text can contain regular expressions supported by #GRegex.
 *
 * Returns: %TRUE if search text can use regex
 *
 * Since: 3.32
 */
gboolean
ide_editor_search_get_regex_enabled (IdeEditorSearch *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_SEARCH (self), FALSE);

  return gtk_source_search_settings_get_regex_enabled (self->settings);
}

/**
 * ide_editor_search_set_replacement_text:
 * @self: An #IdeEditorSearch
 * @replacement_text: (nullable): The text to use in replacement operations
 *
 * This sets the text to use when performing search and replace. See
 * ide_editor_search_replace() or ide_editor_search_replace_all() to
 * perform one or more replacements.
 *
 * If #IdeEditorSearch:regex-enabled is set, then you may reference
 * regex groups from the regex in #IdeEditorSearch:search-text.
 *
 * Since: 3.32
 */
void
ide_editor_search_set_replacement_text (IdeEditorSearch *self,
                                        const gchar     *replacement_text)
{
  g_return_if_fail (IDE_IS_EDITOR_SEARCH (self));

  if (g_strcmp0 (self->replacement_text, replacement_text) != 0)
    {
      g_free (self->replacement_text);
      self->replacement_text = g_strdup (replacement_text);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_REPLACEMENT_TEXT]);
    }
}

/**
 * ide_editor_search_get_replacement_text:
 * @self: An #IdeEditorSearch
 *
 * Gets the #IdeEditorSearch:replacement-text property. This is the text
 * that will be used when calling ide_editor_search_replace() or
 * ide_editor_search_replace_all().
 *
 * Returns: (nullable): the replacement text, or %NULL
 *
 * Since: 3.32
 */
const gchar *
ide_editor_search_get_replacement_text (IdeEditorSearch *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_SEARCH (self), NULL);

  return self->replacement_text;
}

/**
 * ide_editor_search_set_at_word_boundaries:
 * @self: An #IdeEditorSearch
 * @at_word_boundaries: %TRUE if search should match only on word boundaries
 *
 * See also: gtk_source_search_settings_set_word_boundaries()
 *
 * Since: 3.32
 */
void
ide_editor_search_set_at_word_boundaries (IdeEditorSearch *self,
                                          gboolean         at_word_boundaries)
{
  g_return_if_fail (IDE_IS_EDITOR_SEARCH (self));

  gtk_source_search_settings_set_at_word_boundaries (self->settings, at_word_boundaries);
  ide_editor_search_set_action_state (self, "at-word-boundaries",
                                      g_variant_new_boolean (at_word_boundaries));
}

/**
 * ide_editor_search_get_at_word_boundaries:
 * @self: An #IdeEditorSearch
 *
 * Gets the #IdeEditorSearch:at-word-boundaries property.
 *
 * Returns: %TRUE if the search should only match word boundaries.
 *
 * See also: #GtkSourceSearchSettings:at-word-boundaries
 * Since: 3.32
 */
gboolean
ide_editor_search_get_at_word_boundaries (IdeEditorSearch *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_SEARCH (self), FALSE);

  return gtk_source_search_settings_get_at_word_boundaries (self->settings);
}

/**
 * ide_editor_search_get_match_count:
 * @self: An #IdeEditorSearch
 *
 * Gets the number of matches currently found in the editor. This
 * will update as new matches are found while scanning the buffer.
 *
 * Since: 3.32
 */
guint
ide_editor_search_get_match_count (IdeEditorSearch *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_SEARCH (self), 0);

  return self->match_count;
}

/**
 * ide_editor_search_get_match_position:
 * @self: An #IdeEditorSearch
 *
 * Gets the match position of the cursor within the buffer. If the
 * cursor is within a match, this will be a 1-based index
 * will update as new matches are found while scanning the buffer.
 *
 * Since: 3.32
 */
guint
ide_editor_search_get_match_position (IdeEditorSearch *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_SEARCH (self), 0);

  return self->match_position;
}

static gboolean
buffer_selection_contains (GtkTextBuffer *buffer,
                           GtkTextIter   *iter)
{
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (GTK_IS_TEXT_BUFFER (buffer));
  g_assert (iter != NULL);

  if (gtk_text_buffer_get_selection_bounds (buffer, &begin, &end))
    {
      gtk_text_iter_order (&begin, &end);

      if (gtk_text_iter_compare (&begin, iter) <= 0 &&
          gtk_text_iter_compare (&end, iter) >= 0)
        return TRUE;
    }

  return FALSE;
}

static void
ide_editor_search_forward_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  GtkSourceSearchContext *context = (GtkSourceSearchContext *)object;
  g_autoptr(IdeEditorSearch) self = user_data;
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (GTK_SOURCE_IS_SEARCH_CONTEXT (context));
  g_assert (IDE_IS_EDITOR_SEARCH (self));

  if (gtk_source_search_context_forward_finish (context, result, &begin, &end, NULL, NULL))
    {
      if (self->view != NULL)
        {
          GtkTextBuffer *buffer = gtk_text_iter_get_buffer (&begin);
          GtkTextMark *insert = gtk_text_buffer_get_insert (buffer);

          if (self->extend_selection != IDE_EDITOR_SEARCH_SELECT_NONE)
            {
              GtkTextIter *dest = &end;

              if (buffer_selection_contains (buffer, &begin) &&
                  self->extend_selection == IDE_EDITOR_SEARCH_SELECT_WITH_RESULT)
                dest = &begin;

              gtk_text_buffer_move_mark (buffer, insert, dest);
            }
          else if (self->interactive > 0)
            gtk_text_buffer_select_range (buffer, &begin, &end);
          else
            gtk_text_buffer_select_range (buffer, &begin, &begin);

          ide_source_view_scroll_to_mark (IDE_SOURCE_VIEW (self->view),
                                          insert,
                                          0.25,
                                          IDE_SOURCE_SCROLL_BOTH,
                                          1.0,
                                          0.5,
                                          FALSE);

          if (self->repeat > 0)
            {
              self->repeat--;
              gtk_source_search_context_forward_async (context,
                                                       &end,
                                                       NULL,
                                                       ide_editor_search_forward_cb,
                                                       g_object_ref (self));
              g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_REPEAT]);
            }
        }
    }

  ide_editor_search_notify_occurrences_count (self, NULL, context);
}

static void
ide_editor_search_backward_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  GtkSourceSearchContext *context = (GtkSourceSearchContext *)object;
  g_autoptr(IdeEditorSearch) self = user_data;
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (GTK_SOURCE_IS_SEARCH_CONTEXT (context));
  g_assert (IDE_IS_EDITOR_SEARCH (self));

  if (gtk_source_search_context_forward_finish (context, result, &begin, &end, NULL, NULL))
    {
      if (self->view != NULL)
        {
          GtkTextBuffer *buffer = gtk_text_iter_get_buffer (&begin);
          GtkTextMark *insert = gtk_text_buffer_get_insert (buffer);

          if (self->extend_selection != IDE_EDITOR_SEARCH_SELECT_NONE)
            {
              GtkTextIter *dest = &begin;

              if (buffer_selection_contains (buffer, &begin) &&
                  self->extend_selection == IDE_EDITOR_SEARCH_SELECT_WITH_RESULT)
                dest = &end;

              gtk_text_buffer_move_mark (buffer, insert, dest);
            }
          else if (self->interactive > 0)
            gtk_text_buffer_select_range (buffer, &begin, &end);
          else
            gtk_text_buffer_select_range (buffer, &begin, &begin);

          ide_source_view_scroll_to_mark (IDE_SOURCE_VIEW (self->view),
                                          insert,
                                          0.25,
                                          IDE_SOURCE_SCROLL_BOTH,
                                          1.0,
                                          0.5,
                                          FALSE);

          if (self->repeat > 0)
            {
              self->repeat--;
              gtk_source_search_context_backward_async (context,
                                                        &begin,
                                                        NULL,
                                                        ide_editor_search_backward_cb,
                                                        g_object_ref (self));
              g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_REPEAT]);
            }
        }
    }

  ide_editor_search_notify_occurrences_count (self, NULL, context);
}

static void
maybe_flip_selection_bounds (IdeEditorSearch *self,
                             GtkTextBuffer   *buffer,
                             gboolean         backwards)
{
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (IDE_IS_EDITOR_SEARCH (self));
  g_assert (GTK_IS_TEXT_BUFFER (buffer));

  /*
   * The goal of this operation is to try to handle a special case where
   * we are moving forwards/backwards with an initial selection that matches
   * the current search-text.
   *
   * Instead of potentially unselecting the match, we want to flip the
   * insert/selection-bound marks so that the selection is extended in
   * the proper direction.
   */

  if (gtk_text_buffer_get_selection_bounds (buffer, &begin, &end))
    {
      const gchar *search_text = ide_editor_search_get_search_text (self);
      g_autofree gchar *slice = NULL;
      guint length;

      if (search_text == NULL)
        search_text = "";

      gtk_text_iter_order (&begin, &end);
      length = gtk_text_iter_get_offset (&end) - gtk_text_iter_get_offset (&begin);

      if (!ide_editor_search_get_regex_enabled (self) &&
          length == strlen (search_text) &&
          NULL != (slice = gtk_text_iter_get_slice (&begin, &end)) &&
          strcmp (search_text, slice) == 0)
        {
          /* NOTE: This does not work for Regex based search, but that
           *       is much less likely to be important compared to the
           *       simple word match check.
           */

          if (backwards)
            gtk_text_buffer_select_range (buffer, &begin, &end);
          else
            gtk_text_buffer_select_range (buffer, &end, &begin);
        }
    }
}

/**
 * ide_editor_search_move:
 * @self: An #IdeEditorSearch
 * @direction: An #IdeEditorSearchDirection
 *
 * This moves the insertion cursor in the buffer to the next match based
 * upon @direction.
 *
 * If direction is %IDE_EDITOR_SEARCH_BACKWARD, the search will stop
 * at the beginning of the buffer.
 *
 * If direction is %IDE_EDITOR_SEARCH_FORWARD, the search will stop
 * at the end of the buffer.
 *
 * If direction is %IDE_EDITOR_SEARCH_NEXT, it will automatically wrap
 * around to the beginning of the buffer after reaching the end of the
 * buffer.
 *
 * If direction is %IDE_EDITOR_SEARCH_PREVIOUS, the search will
 * automatically wrap around to the end of the buffer once the beginning
 * of the buffer has been reached.
 *
 * Since: 3.32
 */
void
ide_editor_search_move (IdeEditorSearch          *self,
                        IdeEditorSearchDirection  direction)
{
  GtkSourceSearchContext *context;
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;
  GtkTextIter begin, end;
  gboolean selected;

  g_return_if_fail (IDE_IS_EDITOR_SEARCH (self));
  g_return_if_fail (self->view != NULL);
  g_return_if_fail (direction >= 0);
  g_return_if_fail (direction <= IDE_EDITOR_SEARCH_AFTER_REPLACE);

  context = ide_editor_search_acquire_context (self);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->view));
  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);
  selected = gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);
  gtk_text_iter_order (&begin, &end);

  if (self->reverse)
    {
      if (direction == IDE_EDITOR_SEARCH_NEXT)
        direction = IDE_EDITOR_SEARCH_PREVIOUS;
      else if (direction == IDE_EDITOR_SEARCH_PREVIOUS)
        direction = IDE_EDITOR_SEARCH_NEXT;
    }

  if (self->repeat > 0)
    self->repeat--;

  switch (direction)
    {
    case IDE_EDITOR_SEARCH_FORWARD:
      if (!selected)
        gtk_text_iter_forward_char (&iter);
      else
        iter = end;
      gtk_source_search_settings_set_wrap_around (self->settings, FALSE);
      maybe_flip_selection_bounds (self, buffer, FALSE);
      gtk_source_search_context_forward_async (context,
                                               &iter,
                                               NULL,
                                               ide_editor_search_forward_cb,
                                               g_object_ref (self));
      break;

    case IDE_EDITOR_SEARCH_NEXT:
      if (!selected)
        gtk_text_iter_forward_char (&iter);
      else
        iter = end;
      G_GNUC_FALLTHROUGH;
    case IDE_EDITOR_SEARCH_AFTER_REPLACE:
      gtk_source_search_settings_set_wrap_around (self->settings, TRUE);
      gtk_source_search_context_forward_async (context,
                                               &iter,
                                               NULL,
                                               ide_editor_search_forward_cb,
                                               g_object_ref (self));
      break;

    case IDE_EDITOR_SEARCH_BACKWARD:
      if (!self)
        gtk_text_iter_backward_char (&iter);
      else
        iter = begin;
      gtk_source_search_settings_set_wrap_around (self->settings, FALSE);
      maybe_flip_selection_bounds (self, buffer, TRUE);
      gtk_source_search_context_backward_async (context,
                                                &iter,
                                                NULL,
                                                ide_editor_search_backward_cb,
                                                g_object_ref (self));
      break;

    case IDE_EDITOR_SEARCH_PREVIOUS:
      if (!selected)
        gtk_text_iter_backward_char (&iter);
      else
        iter = begin;
      gtk_source_search_settings_set_wrap_around (self->settings, TRUE);
      gtk_source_search_context_backward_async (context,
                                                &iter,
                                                NULL,
                                                ide_editor_search_backward_cb,
                                                g_object_ref (self));
      break;

    default:
      g_assert_not_reached ();
    }

  ide_editor_search_release_context (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_REPEAT]);
}

/**
 * ide_editor_search_replace:
 * @self: An #IdeEditorSearch
 *
 * Replaces the next occurrance of a search result with the
 * value of #IdeEditorSearch:replacement-text.
 *
 * Since: 3.32
 */
void
ide_editor_search_replace (IdeEditorSearch *self)
{
  g_autofree gchar *unescaped = NULL;
  GtkSourceSearchContext *context;
  const gchar *replacement;
  GtkTextBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;

  g_return_if_fail (IDE_IS_EDITOR_SEARCH (self));
  g_return_if_fail (self->view != NULL);
  g_return_if_fail (self->match_count > 0);
  g_return_if_fail (self->match_position > 0);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->view));
  gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);
  gtk_text_iter_order (&begin, &end);

  replacement = self->replacement_text ? self->replacement_text : "";
  unescaped = gtk_source_utils_unescape_search_text (replacement);
  context = ide_editor_search_acquire_context (self);

  /* Replace the current word */
  gtk_source_search_context_replace (context, &begin, &end, unescaped, -1, NULL);

  /* Now scan to the next search result */
  ide_editor_search_move (self, IDE_EDITOR_SEARCH_AFTER_REPLACE);

  ide_editor_search_release_context (self);
}

/**
 * ide_editor_search_replace_all:
 * @self: An #IdeEditorSearch
 *
 * Replaces all the occurrances of #IdeEditorSearch:search-text with the
 * value of #IdeEditorSearch:replacement-text.
 *
 * Since: 3.32
 */
void
ide_editor_search_replace_all (IdeEditorSearch *self)
{
  GtkSourceSearchContext *context;
  const gchar *replacement;
  g_autofree gchar *unescaped = NULL;

  g_return_if_fail (IDE_IS_EDITOR_SEARCH (self));

  /* TODO: We should set the busy bit and do this incrementally */

  replacement = self->replacement_text ? self->replacement_text : "";
  unescaped = gtk_source_utils_unescape_search_text (replacement);
  context = ide_editor_search_acquire_context (self);
  gtk_source_search_context_replace_all (context, unescaped, -1, NULL);
  ide_editor_search_release_context (self);
}

/**
 * ide_editor_search_begin_interactive:
 * @self: An #IdeEditorSearch
 *
 * This function is used to track when the user begin doing an
 * interactive search, which is one where they are typing the search
 * query.
 *
 * Tracking this behavior is useful because it allows the editor to
 * "rubberband", which is to say it can scan forward to the first search
 * result automatically, and then snap back to the previous location if
 * the search is aborted.
 *
 * Since: 3.32
 */
void
ide_editor_search_begin_interactive (IdeEditorSearch *self)
{
  GtkAdjustment *adj;

  g_return_if_fail (IDE_IS_EDITOR_SEARCH (self));
  g_return_if_fail (self->view != NULL);

  self->interactive++;

  /* Disable reverse search when interactive */
  ide_editor_search_set_reverse (self, FALSE);

  /* Clear any repeat that was previously set */
  ide_editor_search_set_repeat (self, 0);

  /* Always highlight matches while in interactive mode */
  if (self->context != NULL)
    gtk_source_search_context_set_highlight (self->context, TRUE);

  adj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (self->view));
  self->scroll_value = gtk_adjustment_get_value (adj);
}

/**
 * ide_editor_search_end_interactive:
 * @self: An #IdeEditorSearch
 *
 * This function completes an interactive search previously performed
 * with ide_editor_search_begin_interactive().
 *
 * This should be called when the user has left the search controls,
 * as it might allow the editor to restore positioning back to the
 * previous editor location from before the interactive search began.
 *
 * Since: 3.32
 */
void
ide_editor_search_end_interactive (IdeEditorSearch *self)
{
  g_return_if_fail (IDE_IS_EDITOR_SEARCH (self));

  self->interactive--;

  /* If we are leaving interactive mode, we want to disable the search
   * highlight unless they were requested manually by other code.
   */
  if (self->context != NULL && self->interactive == 0 && self->visible == FALSE)
    gtk_source_search_context_set_highlight (self->context, self->visible);

  /* Maybe cleanup our search context. */
  ide_editor_search_release_context (self);
}

/**
 * ide_editor_search_get_reverse:
 * @self: a #IdeEditorSearch
 *
 * Checks if search movements should be reversed for relative movements such
 * as %IDE_EDITOR_SEARCH_NEXT and %IDE_EDITOR_SEARCH_PREVIOUS.
 *
 * This might be used when performing searches such as vim's # or * search
 * operators. After that movements like n or N need to swap directions.
 *
 * Returns: %TRUE if relative movements are reversed directions.
 *
 * Since: 3.32
 */
gboolean
ide_editor_search_get_reverse (IdeEditorSearch *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_SEARCH (self), FALSE);

  return self->reverse;
}

/**
 * ide_editor_search_set_reverse:
 * @self: a #IdeEditorSearch
 * @reverse: if relative search directions should reverse
 *
 * Sets the "reverse" property.
 *
 * This is used to alter the direction for relative search movements.
 * %IDE_EDITOR_SEARCH_NEXT and %IDE_EDITOR_SEARCH_PREVIOUS will swap
 * directions so that %IDE_EDITOR_SEARCH_PREVIOUS will search forwards
 * in the buffer and %IDE_EDITOR_SEARCH_NEXT wills earch backwards.
 *
 * Since: 3.32
 */
void
ide_editor_search_set_reverse (IdeEditorSearch *self,
                               gboolean         reverse)
{
  g_return_if_fail (IDE_IS_EDITOR_SEARCH (self));

  reverse = !!reverse;

  if (reverse != self->reverse)
    {
      self->reverse = reverse;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_REVERSE]);
    }
}

/**
 * ide_editor_search_get_extend_selection:
 * @self: a #IdeEditorSearch
 *
 * Gets the "extend-selection" property.
 *
 * This property determines if the selection should be extended and
 * how when moving between search results.
 *
 * Returns: An %IdeEditorSearchSelect
 *
 * Since: 3.32
 */
IdeEditorSearchSelect
ide_editor_search_get_extend_selection (IdeEditorSearch *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_SEARCH (self), FALSE);

  return self->extend_selection;
}

void
ide_editor_search_set_extend_selection (IdeEditorSearch       *self,
                                        IdeEditorSearchSelect  extend_selection)
{
  g_return_if_fail (IDE_IS_EDITOR_SEARCH (self));
  g_return_if_fail (extend_selection <= IDE_EDITOR_SEARCH_SELECT_TO_RESULT);

  if (self->extend_selection != extend_selection)
    {
      self->extend_selection = extend_selection;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_EXTEND_SELECTION]);
    }
}

/**
 * ide_editor_search_get_repeat:
 * @self: a #IdeEditorSearch
 *
 * Gets the #IdeEditorSearch:repeat property containing the number of
 * times to perform a move. A value of 1 performs a single move. A
 * value of 2 performs a second move after the first.
 *
 * A value of zero indicates the property is unset and a single move
 * will be performed.
 *
 * Returns: A number containing the number of moves.
 *
 * Since: 3.32
 */
guint
ide_editor_search_get_repeat (IdeEditorSearch *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_SEARCH (self), 0);

  return self->repeat;
}

/**
 * ide_editor_search_set_repeat:
 * @self: a #IdeEditorSearch
 * @repeat: The new value for the repeat count
 *
 * Sets the repeat count. A @repeat value of 0 indicates that the value
 * is unset. When unset, the default value of 1 is used.
 *
 * See also: ide_editor_search_get_repeat()
 *
 * Since: 3.32
 */
void
ide_editor_search_set_repeat (IdeEditorSearch *self,
                              guint            repeat)
{
  g_return_if_fail (IDE_IS_EDITOR_SEARCH (self));

  if (self->repeat != repeat)
    {
      self->repeat = repeat;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_REPEAT]);
    }
}

/**
 * ide_editor_search_get_active:
 * @self: a #IdeEditorSearch
 *
 * Gets the "active" property.
 *
 * The #IdeEditorSearch is considered active if there is a search
 * context loaded and the search text is not empty.
 *
 * Returns: %TRUE if a search is active
 *
 * Since: 3.32
 */
gboolean
ide_editor_search_get_active (IdeEditorSearch *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_SEARCH (self), FALSE);

  if (self->context != NULL)
    return !dzl_str_empty0 (ide_editor_search_get_search_text (self));

  return FALSE;
}

static void
ide_editor_search_actions_move_next (IdeEditorSearch *self,
                                     GVariant        *param)
{
  ide_editor_search_move (self, IDE_EDITOR_SEARCH_NEXT);
}

static void
ide_editor_search_actions_move_previous (IdeEditorSearch *self,
                                         GVariant        *param)
{
  ide_editor_search_move (self, IDE_EDITOR_SEARCH_PREVIOUS);
}

static void
ide_editor_search_actions_at_word_boundary (IdeEditorSearch *self,
                                            GVariant        *param)
{
  ide_editor_search_set_at_word_boundaries (self, g_variant_get_boolean (param));
}

static void
ide_editor_search_actions_replace_all (IdeEditorSearch *self,
                                       GVariant        *param)
{
  ide_editor_search_replace_all (self);
}

static void
ide_editor_search_actions_replace (IdeEditorSearch *self,
                                   GVariant        *param)
{
  ide_editor_search_replace (self);
}

GType
ide_editor_search_select_get_type (void)
{
  static GType type_id;

  if (g_once_init_enter (&type_id))
    {
      static const GEnumValue values[] = {
        { IDE_EDITOR_SEARCH_SELECT_NONE, "IDE_EDITOR_SEARCH_SELECT_NONE", "none" },
        { IDE_EDITOR_SEARCH_SELECT_WITH_RESULT, "IDE_EDITOR_SEARCH_SELECT_WITH_RESULT", "with-result" },
        { IDE_EDITOR_SEARCH_SELECT_TO_RESULT, "IDE_EDITOR_SEARCH_SELECT_TO_RESULT", "to-result" },
        { 0 }
      };
      GType _type_id = g_enum_register_static ("IdeEditorSearchSelect", values);
      g_once_init_leave (&type_id, _type_id);
    }

  return type_id;
}

GType
ide_editor_search_direction_get_type (void)
{
  static GType type_id;

  if (g_once_init_enter (&type_id))
    {
      static const GEnumValue values[] = {
        { IDE_EDITOR_SEARCH_FORWARD, "IDE_EDITOR_SEARCH_FORWARD", "forward" },
        { IDE_EDITOR_SEARCH_NEXT, "IDE_EDITOR_SEARCH_NEXT", "next" },
        { IDE_EDITOR_SEARCH_PREVIOUS, "IDE_EDITOR_SEARCH_PREVIOUS", "previous" },
        { IDE_EDITOR_SEARCH_BACKWARD, "IDE_EDITOR_SEARCH_BACKWARD", "backward" },
        { 0 }
      };
      GType _type_id = g_enum_register_static ("IdeEditorSearchDirection", values);
      g_once_init_leave (&type_id, _type_id);
    }

  return type_id;
}
