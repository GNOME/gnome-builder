/* ide-omni-gutter-renderer.c
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-omni-gutter-renderer"

#include <dazzle.h>
#include <string.h>

#include "ide-context.h"
#include "ide-debug.h"

#include "buffers/ide-buffer.h"
#include "debugger/ide-debug-manager.h"
#include "debugger/ide-debugger-breakpoint.h"
#include "debugger/ide-debugger-breakpoints.h"
#include "debugger/ide-debugger-private.h"
#include "files/ide-file.h"
#include "sourceview/ide-omni-gutter-renderer.h"
#include "sourceview/ide-source-view.h"

/**
 * SECTION:ide-omni-gutter-renderer
 * @title: IdeOmniGutterRenderer
 * @short_description: A featureful gutter renderer for the code editor
 *
 * This is a #GtkSourceGutterRenderer that knows how to render many of
 * our components necessary for Builder. Because of the complexity of
 * Builder, using traditional gutter renderers takes up a great deal
 * of horizontal space.
 *
 * By overlapping some of our components, we can take up less space and
 * be easier for the user with increased hit-targets.
 *
 * Additionally, we can render faster because we can coalesce work.
 *
 * Since: 3.28
 */

#define DIAGNOSTICS_SIZE 16
#define ARROW_WIDTH      5
#define CHANGE_WIDTH     2
#define DELETE_WIDTH     5.0
#define DELETE_HEIGHT    8.0

#define IS_BREAKPOINT(i)  ((i)->is_breakpoint || (i)->is_countpoint || (i)->is_watchpoint)
#define IS_DIAGNOSTIC(i)  ((i)->is_error || (i)->is_warning || (i)->is_note)
#define IS_LINE_CHANGE(i) ((i)->is_add || (i)->is_change || \
                           (i)->is_delete || (i)->is_next_delete || (i)->is_prev_delete)

struct _IdeOmniGutterRenderer
{
  GtkSourceGutterRenderer parent_instance;

  IdeDebuggerBreakpoints *breakpoints;

  GArray *lines;

  DzlSignalGroup *view_signals;
  DzlSignalGroup *buffer_signals;

  /* TODO: It would be nice to use some basic caching here
   *       so we don't waste 6Kb-12Kb of data on these surfaces.
   *       But that can be done later after this patch set merges.
   */
  cairo_surface_t *note_surface;
  cairo_surface_t *warning_surface;
  cairo_surface_t *error_surface;
  cairo_surface_t *note_selected_surface;
  cairo_surface_t *warning_selected_surface;
  cairo_surface_t *error_selected_surface;

  /*
   * We cache various colors we need from the style scheme to avoid
   * looking them up very often, as it is CPU time consuming. We also
   * use these colors to prime the symbolic colors for the icon surfaces
   * to they look appropriate for the style scheme.
   */
  struct {
    GdkRGBA fg;
    GdkRGBA bg;
    gboolean bold;
  } text, current, bkpt;
  GdkRGBA stopped_bg;
  struct {
    GdkRGBA add;
    GdkRGBA remove;
    GdkRGBA change;
  } changes;

  /*
   * We need to reuse a single pango layout while drawing all the lines
   * to keep the overhead low. We don't have pixel caching on the gutter
   * data so keeping this stuff fast is critical.
   */
  PangoLayout *layout;

  /*
   * We reuse a simple bold attr list for the current line number
   * information.  This way we don't have to do any pango markup
   * parsing.
   */
  PangoAttrList *bold_attrs;

  /* We stash a copy of how long the line numbers could be. 1000 => 4. */
  guint n_chars;

  /* While processing the lines, we track what our first line number is
   * so that differential calculation for each line is cheap by avoiding
   * accessing GtkTextIter information.
   */
  guint begin_line;

  /*
   * While starting a render, we check to see what the current
   * breakpoint line is (so we can draw the proper background.
   *
   * TODO: Add a callback to the debug manager to avoid querying this
   *       information on every draw cycle.
   */
  gint stopped_line;

  /*
   * To avoid doing multiple line recalculations inline, we defer our
   * changed handler until we've re-entered teh main loop. Otherwise
   * we could handle lots of small changes during automated processing
   * of the underlying buffer.
   */
  guint resize_source;

  /*
   * The number_width field contains the maximum width of the text as
   * sized by Pango. It is in pixel units in the scale of the widget
   * as the underlying components will automatically deal with scaling
   * for us (as necessary).
   */
  gint number_width;

  /*
   * Some users might want to toggle off individual features of the
   * omni gutter, and these boolean properties provide that. Other
   * components map them to GSettings values to be toggled.
   */
  guint show_line_changes : 1;
  guint show_line_numbers : 1;
  guint show_line_diagnostics : 1;
};

enum {
  FOREGROUND,
  BACKGROUND,
};

enum {
  PROP_0,
  PROP_SHOW_LINE_CHANGES,
  PROP_SHOW_LINE_NUMBERS,
  PROP_SHOW_LINE_DIAGNOSTICS,
  N_PROPS
};

typedef struct
{
  /* The line contains a regular breakpoint */
  guint is_breakpoint : 1;

  /* The line contains a countpoint styl breakpoint */
  guint is_countpoint : 1;

  /* The line contains a watchpoint style breakpoint */
  guint is_watchpoint : 1;

  /* The line is an addition to the buffer */
  guint is_add : 1;

  /* The line has changed in the buffer */
  guint is_change : 1;

  /* The line is part of a deleted range in the buffer */
  guint is_delete : 1;

  /* The previous line was a delete */
  guint is_prev_delete : 1;

  /* The next line is a delete */
  guint is_next_delete : 1;

  /* The line contains a diagnostic error */
  guint is_error : 1;

  /* The line contains a diagnostic warning */
  guint is_warning : 1;

  /* The line contains a diagnostic note */
  guint is_note : 1;
} LineInfo;

G_DEFINE_TYPE (IdeOmniGutterRenderer, ide_omni_gutter_renderer, GTK_SOURCE_TYPE_GUTTER_RENDERER)

static GParamSpec *properties [N_PROPS];

/*
 * style_get_is_bold:
 *
 * This helper is used to extract the "bold" field from a GtkSourceStyle
 * within a GtkSourceStyleScheme.
 *
 * Returns; %TRUE if @val was set to a trusted value.
 */
static gboolean
style_get_is_bold (GtkSourceStyleScheme *scheme,
                   const gchar          *style_name,
                   gboolean             *val)
{
  GtkSourceStyle *style;

  g_assert (!scheme || GTK_SOURCE_IS_STYLE_SCHEME (scheme));
  g_assert (style_name != NULL);
  g_assert (val != NULL);

  *val = FALSE;

  if (scheme == NULL)
    return FALSE;

  if (NULL != (style = gtk_source_style_scheme_get_style (scheme, style_name)))
    {
      gboolean bold_set = FALSE;
      g_object_get (style,
                    "bold-set", &bold_set,
                    "bold", val,
                    NULL);
      return bold_set;
    }

  return FALSE;
}

/*
 * get_style_rgba:
 *
 * Gets a #GdkRGBA for a particular field of a style within @scheme.
 *
 * @type should be set to BACKGROUND or FOREGROUND.
 *
 * If we fail to locate the style, @rgba is set to transparent black.
 * such as #rgba(0,0,0,0).
 *
 * Returns: %TRUE if the value placed into @rgba can be trusted.
 */
static gboolean
get_style_rgba (GtkSourceStyleScheme *scheme,
                const gchar          *style_name,
                int                   type,
                GdkRGBA              *rgba)
{
  GtkSourceStyle *style;

  g_assert (!scheme || GTK_SOURCE_IS_STYLE_SCHEME (scheme));
  g_assert (style_name != NULL);
  g_assert (type == FOREGROUND || type == BACKGROUND);
  g_assert (rgba != NULL);

  memset (rgba, 0, sizeof *rgba);

  if (scheme == NULL)
    return FALSE;

  if (NULL != (style = gtk_source_style_scheme_get_style (scheme, style_name)))
    {
      g_autofree gchar *str = NULL;
      gboolean set = FALSE;

      g_object_get (style,
                    type ? "background" : "foreground", &str,
                    type ? "background-set" : "foreground-set", &set,
                    NULL);

      if (str != NULL)
        gdk_rgba_parse (rgba, str);

      return set;
    }

  return FALSE;
}

static void
reload_style_colors (IdeOmniGutterRenderer *self,
                     GtkSourceStyleScheme  *scheme)
{
  GtkStyleContext *context;
  GtkTextView *view;
  GtkStateFlags state;
  GdkRGBA fg;
  GdkRGBA bg;

  g_assert (IDE_IS_OMNI_GUTTER_RENDERER (self));
  g_assert (!scheme || GTK_SOURCE_IS_STYLE_SCHEME (scheme));

  view = gtk_source_gutter_renderer_get_view (GTK_SOURCE_GUTTER_RENDERER (self));
  if (view == NULL)
    return;

  context = gtk_widget_get_style_context (GTK_WIDGET (view));
  state = gtk_style_context_get_state (context);
  gtk_style_context_get_color (context, state, &fg);
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  gtk_style_context_get_background_color (context, state, &bg);
  G_GNUC_END_IGNORE_DEPRECATIONS;

  /* Extract common values from style schemes. */
  if (!get_style_rgba (scheme, "line-numbers", FOREGROUND, &self->text.fg))
    self->text.fg = fg;

  if (!get_style_rgba (scheme, "line-numbers", BACKGROUND, &self->text.bg))
    self->text.bg = bg;

  if (!style_get_is_bold (scheme, "line-numbers", &self->text.bold))
    self->text.bold = FALSE;

  if (!get_style_rgba (scheme, "current-line-number", FOREGROUND, &self->current.fg))
    self->current.fg = fg;

  if (!get_style_rgba (scheme, "current-line-number", BACKGROUND, &self->current.bg))
    self->current.bg = bg;

  if (!style_get_is_bold (scheme, "current-line-number", &self->current.bold))
    self->current.bold = TRUE;

  /*
   * These debugger:: prefix values come from Builder's style-scheme xml
   * as well as in the IdeBuffer class. Other style schemes may also
   * support them, though.
   */
  if (!get_style_rgba (scheme, "debugger::current-breakpoint", BACKGROUND, &self->stopped_bg))
    gdk_rgba_parse (&self->stopped_bg, "#fcaf3e");

  if (!get_style_rgba (scheme, "debugger::breakpoint", FOREGROUND, &self->bkpt.fg))
    get_style_rgba (scheme, "selection", FOREGROUND, &self->bkpt.fg);

  if (!get_style_rgba (scheme, "debugger::breakpoint", BACKGROUND, &self->bkpt.bg))
    get_style_rgba (scheme, "selection", BACKGROUND, &self->bkpt.bg);

  if (!style_get_is_bold (scheme, "debugger::breakpoint", &self->bkpt.bold))
    self->bkpt.bold = FALSE;

  /* These gutter:: prefix values come from Builder's style-scheme xml
   * files, but other style schemes may also support them now too.
   */
  if (!get_style_rgba (scheme, "gutter::added-line", FOREGROUND, &self->changes.add))
    gdk_rgba_parse (&self->changes.add, "#8ae234");

  if (!get_style_rgba (scheme, "gutter::changed-line", FOREGROUND, &self->changes.change))
    gdk_rgba_parse (&self->changes.change, "#fcaf3e");

  if (!get_style_rgba (scheme, "gutter::removed-line", FOREGROUND, &self->changes.remove))
    gdk_rgba_parse (&self->changes.remove, "#ef2929");
}

static void
collect_breakpoint_info (IdeDebuggerBreakpoint *breakpoint,
                         gpointer               user_data)
{
  struct {
    GArray *lines;
    guint begin;
    guint end;
  } *bkpt_info = user_data;
  guint line;

  g_assert (IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));
  g_assert (bkpt_info != NULL);

  /* Debugger breakpoints are 1-based line numbers */
  if (!(line = ide_debugger_breakpoint_get_line (breakpoint)))
    return;

  line--;

  if (line >= bkpt_info->begin && line <= bkpt_info->end)
    {
      IdeDebuggerBreakMode mode = ide_debugger_breakpoint_get_mode (breakpoint);
      LineInfo *info = &g_array_index (bkpt_info->lines, LineInfo, line - bkpt_info->begin);

      info->is_watchpoint = !!(mode & IDE_DEBUGGER_BREAK_WATCHPOINT);
      info->is_countpoint = !!(mode & IDE_DEBUGGER_BREAK_COUNTPOINT);
      info->is_breakpoint = !!(mode & IDE_DEBUGGER_BREAK_BREAKPOINT);
    }
}

static void
ide_omni_gutter_renderer_load_breakpoints (IdeOmniGutterRenderer *self,
                                           GtkTextIter           *begin,
                                           GtkTextIter           *end,
                                           GArray                *lines)
{
  g_assert (IDE_IS_OMNI_GUTTER_RENDERER (self));
  g_assert (begin != NULL);
  g_assert (lines != NULL);
  g_assert (lines->len > 0);

  if (self->breakpoints != NULL)
    {
      struct {
        GArray *lines;
        guint begin;
        guint end;
      } info;

      info.lines = lines;
      info.begin = gtk_text_iter_get_line (begin);
      info.end = gtk_text_iter_get_line (end);

      ide_debugger_breakpoints_foreach (self->breakpoints,
                                        (GFunc)collect_breakpoint_info,
                                        &info);
    }
}

static void
ide_omni_gutter_renderer_load_basic (IdeOmniGutterRenderer *self,
                                     GtkTextIter           *begin,
                                     GArray                *lines)
{
  GtkTextBuffer *buffer;
  LineInfo *last = NULL;
  guint line;

  g_assert (IDE_IS_OMNI_GUTTER_RENDERER (self));
  g_assert (begin != NULL);
  g_assert (lines != NULL);
  g_assert (lines->len > 0);

  buffer = gtk_text_iter_get_buffer (begin);
  if (!IDE_IS_BUFFER (buffer))
    return;

  line = gtk_text_iter_get_line (begin);

  for (guint i = 0; i < lines->len; i++)
    {
      LineInfo *info = &g_array_index (lines, LineInfo, i);
      IdeBufferLineFlags flags = ide_buffer_get_line_flags (IDE_BUFFER (buffer), line + i);

      info->is_add = !!(flags & IDE_BUFFER_LINE_FLAGS_ADDED);
      info->is_change = !!(flags & IDE_BUFFER_LINE_FLAGS_CHANGED);
      info->is_delete = !!(flags & IDE_BUFFER_LINE_FLAGS_DELETED);
      info->is_warning = !!(flags & IDE_BUFFER_LINE_FLAGS_WARNING);
      info->is_note = !!(flags & IDE_BUFFER_LINE_FLAGS_NOTE);
      info->is_error = !!(flags & IDE_BUFFER_LINE_FLAGS_ERROR);

      if (last != NULL)
        {
          info->is_prev_delete = last->is_delete;
          last->is_next_delete = info->is_delete;
        }

      last = info;
    }
}

static inline gint
count_num_digits (gint num_lines)
{
  if (num_lines < 100)
    return 2;
  else if (num_lines < 1000)
    return 3;
  else if (num_lines < 10000)
    return 4;
  else if (num_lines < 100000)
    return 5;
  else if (num_lines < 1000000)
    return 6;
  else
    return 10;
}

static void
ide_omni_gutter_renderer_recalculate_size (IdeOmniGutterRenderer *self)
{
  const PangoFontDescription *font_desc;
  g_autofree gchar *numbers = NULL;
  GtkTextBuffer *buffer;
  GtkTextView *view;
  PangoLayout *layout;
  GtkTextIter end;
  guint line;
  int height;
  gint size = 0;

  g_assert (IDE_IS_OMNI_GUTTER_RENDERER (self));

  /* There is nothing we can do until a view has been attached. */
  view = gtk_source_gutter_renderer_get_view (GTK_SOURCE_GUTTER_RENDERER (self));
  if (!IDE_IS_SOURCE_VIEW (view))
    return;

  /*
   * First, we need to get the size of the text for the last line of the
   * buffer (which will be the longest). We size the font with '9' since it
   * will generally be one of the widest of the numbers. Although, we only
   * "support" * monospace anyway, so it shouldn't be drastic if we're off.
   */

  buffer = gtk_text_view_get_buffer (view);
  gtk_text_buffer_get_end_iter (buffer, &end);
  line = gtk_text_iter_get_line (&end) + 1;

  self->n_chars = count_num_digits (line);
  numbers = g_strnfill (self->n_chars, '9');

  /*
   * Get the font description used by the IdeSourceView so we can
   * match the font styling as much as possible.
   */
  font_desc = ide_source_view_get_font_desc (IDE_SOURCE_VIEW (view));
  layout = gtk_widget_create_pango_layout (GTK_WIDGET (view), numbers);
  pango_layout_set_font_description (layout, font_desc);

  /*
   * Now cache the width of the text layout so we can simplify our
   * positioning later. We simply size everything the same and then
   * align to the right to reduce the draw overhead.
   */
  pango_layout_get_pixel_size (layout, &self->number_width, &height);

  /* Now calculate the size based on enabled features */
  size = 2;
  if (self->show_line_diagnostics)
    size += DIAGNOSTICS_SIZE + 2;
  if (self->show_line_numbers)
    size += self->number_width + 2;

  /* The arrow overlaps the changes if we can have breakpoints,
   * otherwise we just need the space for the line changes.
   */
  if (self->breakpoints != NULL)
    size += ARROW_WIDTH + 2;
  else if (self->show_line_changes)
    size += CHANGE_WIDTH + 2;

  /* Update the size and ensure we are re-drawn */
  gtk_source_gutter_renderer_set_size (GTK_SOURCE_GUTTER_RENDERER (self), size );
  gtk_source_gutter_renderer_queue_draw (GTK_SOURCE_GUTTER_RENDERER (self));

  g_clear_object (&layout);
}

static void
ide_omni_gutter_renderer_notify_font_desc (IdeOmniGutterRenderer *self,
                                           GParamSpec            *pspec,
                                           IdeSourceView         *view)
{
  g_assert (IDE_IS_OMNI_GUTTER_RENDERER (self));
  g_assert (IDE_IS_SOURCE_VIEW (view));

  ide_omni_gutter_renderer_recalculate_size (self);
}

static void
ide_omni_gutter_renderer_end (GtkSourceGutterRenderer *renderer)
{
  IdeOmniGutterRenderer *self = (IdeOmniGutterRenderer *)renderer;

  g_assert (IDE_IS_OMNI_GUTTER_RENDERER (self));

  g_clear_object (&self->layout);
}

static void
ide_omni_gutter_renderer_begin (GtkSourceGutterRenderer *renderer,
                                cairo_t                 *cr,
                                GdkRectangle            *bg_area,
                                GdkRectangle            *cell_area,
                                GtkTextIter             *begin,
                                GtkTextIter             *end)
{
  IdeOmniGutterRenderer *self = (IdeOmniGutterRenderer *)renderer;
  GtkTextTagTable *table;
  GtkTextBuffer *buffer;
  IdeSourceView *view;
  GtkTextTag *tag;
  GtkTextIter bkpt;
  guint end_line;

  g_assert (IDE_IS_OMNI_GUTTER_RENDERER (renderer));
  g_assert (cr != NULL);
  g_assert (bg_area != NULL);
  g_assert (cell_area != NULL);
  g_assert (begin != NULL);
  g_assert (end != NULL);

  /*
   * This is the start of our draw process. The first thing we want to
   * do is collect as much information as we'll need when doing the
   * actual draw. That helps us coalesce similar work together, which is
   * good for the CPU usage. We are *very* sensitive to CPU usage here
   * as the GtkTextView does not pixel cache the gutter.
   */

  self->stopped_line = -1;

  /* Locate the current stopped breakpoint if any. */
  buffer = gtk_text_iter_get_buffer (begin);
  table = gtk_text_buffer_get_tag_table (buffer);
  tag = gtk_text_tag_table_lookup (table, "debugger::current-breakpoint");
  if (tag != NULL)
    {
      bkpt = *begin;
      gtk_text_iter_backward_char (&bkpt);
      if (gtk_text_iter_forward_to_tag_toggle (&bkpt, tag) &&
          gtk_text_iter_starts_tag (&bkpt, tag))
        self->stopped_line = gtk_text_iter_get_line (&bkpt);
    }

  /*
   * This function is called before we render any of the lines in
   * the gutter. To reduce our overhead, we want to collect information
   * for all of the line numbers upfront.
   */

  view = IDE_SOURCE_VIEW (gtk_source_gutter_renderer_get_view (renderer));

  self->begin_line = gtk_text_iter_get_line (begin);
  end_line = gtk_text_iter_get_line (end);

  /* Give ourselves a fresh array to stash our line info */
  g_array_set_size (self->lines, end_line - self->begin_line + 1);
  memset (self->lines->data, 0, self->lines->len * sizeof (LineInfo));

  /* Now load breakpoints, diagnostics, and line changes */
  ide_omni_gutter_renderer_load_basic (self, begin, self->lines);
  ide_omni_gutter_renderer_load_breakpoints (self, begin, end, self->lines);

  /* Create a new layout for rendering lines to */
  self->layout = gtk_widget_create_pango_layout (GTK_WIDGET (view), "");
  pango_layout_set_alignment (self->layout, PANGO_ALIGN_RIGHT);
  pango_layout_set_font_description (self->layout, ide_source_view_get_font_desc (view));

  /* Tweak the sizing (for proper alignment) based on the if we are
   * going to be rendering the breakpoints arrow or not.
   */
  if (self->breakpoints != NULL)
    pango_layout_set_width (self->layout, (cell_area->width - ARROW_WIDTH - 4) * PANGO_SCALE);
  else
    pango_layout_set_width (self->layout, (cell_area->width - CHANGE_WIDTH - 2) * PANGO_SCALE);
}

static gboolean
ide_omni_gutter_renderer_query_activatable (GtkSourceGutterRenderer *renderer,
                                            GtkTextIter             *begin,
                                            GdkRectangle            *area,
                                            GdkEvent                *event)
{
  g_assert (IDE_IS_OMNI_GUTTER_RENDERER (renderer));
  g_assert (begin != NULL);
  g_assert (area != NULL);
  g_assert (event != NULL);

  /* Clicking will move the cursor, so always TRUE */

  return TRUE;
}

static void
animate_at_iter (IdeOmniGutterRenderer *self,
                 GdkRectangle          *area,
                 GtkTextIter           *iter)
{
  DzlBoxTheatric *theatric;
  GtkTextView *view;

  g_assert (IDE_IS_OMNI_GUTTER_RENDERER (self));
  g_assert (area != NULL);
  g_assert (iter != NULL);

  /* Show a little bullet animation shooting right */

  view = gtk_source_gutter_renderer_get_view (GTK_SOURCE_GUTTER_RENDERER (self));

  theatric = g_object_new (DZL_TYPE_BOX_THEATRIC,
                           "alpha", 0.3,
                           "background", "#729fcf",
                           "height", area->height,
                           "target", view,
                           "width", area->width,
                           "x", area->x,
                           "y", area->y,
                           NULL);

  dzl_object_animate_full (theatric,
                           DZL_ANIMATION_EASE_IN_CUBIC,
                           100,
                           gtk_widget_get_frame_clock (GTK_WIDGET (view)),
                           g_object_unref,
                           theatric,
                           "x", area->x + 250,
                           "alpha", 0.0,
                           NULL);
}

static void
ide_omni_gutter_renderer_activate (GtkSourceGutterRenderer *renderer,
                                   GtkTextIter             *iter,
                                   GdkRectangle            *area,
                                   GdkEvent                *event)
{
  IdeOmniGutterRenderer *self = (IdeOmniGutterRenderer *)renderer;
  IdeDebuggerBreakpoint *breakpoint;
  IdeDebuggerBreakMode break_type = IDE_DEBUGGER_BREAK_NONE;
  g_autofree gchar *path = NULL;
  IdeDebugManager *debug_manager;
  GtkTextBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;
  IdeContext *context;
  GFile *file;
  guint line;

  IDE_ENTRY;

  g_assert (IDE_IS_OMNI_GUTTER_RENDERER (self));
  g_assert (iter != NULL);
  g_assert (area != NULL);
  g_assert (event != NULL);

  /* TODO: We could check for event->button.button to see if we
   *       can display a popover with information such as
   *       diagnostics, or breakpoints, or git blame.
   */

  buffer = gtk_text_iter_get_buffer (iter);

  /* Select this row if it isn't currently selected */
  if (!gtk_text_buffer_get_selection_bounds (buffer, &begin, &end) &&
      gtk_text_iter_get_line (&begin) != gtk_text_iter_get_line (iter))
    gtk_text_buffer_select_range (buffer, iter, iter);

  /* Nothing more we can do if this file doesn't support breakpoints */
  if (self->breakpoints == NULL)
    return;

  context = ide_buffer_get_context (IDE_BUFFER (buffer));
  debug_manager = ide_context_get_debug_manager (context);

  line = gtk_text_iter_get_line (iter) + 1;
  file = ide_debugger_breakpoints_get_file (self->breakpoints);
  path = g_file_get_path (file);

  /* TODO: Should we show a Popover here to select the type? */
  IDE_TRACE_MSG ("Toggle breakpoint on line %u [breakpoints=%p]",
                 line, self->breakpoints);

  breakpoint = ide_debugger_breakpoints_get_line (self->breakpoints, line);
  if (breakpoint != NULL)
    break_type = ide_debugger_breakpoint_get_mode (breakpoint);

  switch (break_type)
    {
    case IDE_DEBUGGER_BREAK_NONE:
      {
        g_autoptr(IdeDebuggerBreakpoint) to_insert = NULL;

        to_insert = ide_debugger_breakpoint_new (NULL);

        ide_debugger_breakpoint_set_line (to_insert, line);
        ide_debugger_breakpoint_set_file (to_insert, path);
        ide_debugger_breakpoint_set_mode (to_insert, IDE_DEBUGGER_BREAK_BREAKPOINT);
        ide_debugger_breakpoint_set_enabled (to_insert, TRUE);

        _ide_debug_manager_add_breakpoint (debug_manager, to_insert);
      }
      break;

    case IDE_DEBUGGER_BREAK_BREAKPOINT:
    case IDE_DEBUGGER_BREAK_COUNTPOINT:
    case IDE_DEBUGGER_BREAK_WATCHPOINT:
      if (breakpoint != NULL)
        {
          _ide_debug_manager_remove_breakpoint (debug_manager, breakpoint);
          animate_at_iter (self, area, iter);
        }
      break;

    default:
      g_return_if_reached ();
    }

  /*
   * We will wait for changes to be applied to the #IdeDebuggerBreakpoints
   * by the #IdeDebugManager. That will cause the gutter to be invalidated
   * and redrawn.
   */

  IDE_EXIT;
}

static void
draw_breakpoint_bg (IdeOmniGutterRenderer        *self,
                    cairo_t                      *cr,
                    GdkRectangle                 *bg_area,
                    LineInfo                     *info,
                    GtkSourceGutterRendererState  state)
{
  GdkRectangle area;
  GdkRGBA rgba;

  g_assert (GTK_SOURCE_IS_GUTTER_RENDERER (self));
  g_assert (cr != NULL);
  g_assert (bg_area != NULL);

  /*
   * This draws a little arrow starting from the left and pointing
   * over the line changes portion of the gutter.
   */

  area.x = bg_area->x;
  area.y = bg_area->y;
  area.height = bg_area->height;
  area.width = bg_area->width;

  cairo_move_to (cr, area.x, area.y);
  cairo_line_to (cr,
                 dzl_cairo_rectangle_x2 (&area) - ARROW_WIDTH,
                 area.y);
  cairo_line_to (cr,
                 dzl_cairo_rectangle_x2 (&area),
                 dzl_cairo_rectangle_middle (&area));
  cairo_line_to (cr,
                 dzl_cairo_rectangle_x2 (&area) - ARROW_WIDTH,
                 dzl_cairo_rectangle_y2 (&area));
  cairo_line_to (cr, area.x, dzl_cairo_rectangle_y2 (&area));
  cairo_close_path (cr);

  rgba = self->bkpt.bg;

  /*
   * Tweak the brightness based on if we are in pre-light and
   * if we are also still an active breakpoint.
   */

  if ((state & GTK_SOURCE_GUTTER_RENDERER_STATE_PRELIT) != 0)
    {
      if (IS_BREAKPOINT (info))
        rgba.alpha *= 0.8;
      else
        rgba.alpha *= 0.4;
    }

  /* And draw... */

  gdk_cairo_set_source_rgba (cr, &rgba);
  cairo_fill (cr);
}

static void
draw_line_change (IdeOmniGutterRenderer        *self,
                  cairo_t                      *cr,
                  GdkRectangle                 *area,
                  LineInfo                     *info,
                  GtkSourceGutterRendererState  state)
{
  g_assert (IDE_IS_OMNI_GUTTER_RENDERER (self));
  g_assert (cr != NULL);
  g_assert (area != NULL);

  /*
   * Draw a simple line with the appropriate color from the style scheme
   * based on the type of change we have.
   */

  if (info->is_add || info->is_change)
    {
      cairo_rectangle (cr,
                       area->x + area->width - 2 - CHANGE_WIDTH,
                       area->y,
                       CHANGE_WIDTH,
                       area->y + area->height);

      if (info->is_add)
        gdk_cairo_set_source_rgba (cr, &self->changes.add);
      else
        gdk_cairo_set_source_rgba (cr, &self->changes.change);

      cairo_fill (cr);
    }

  if (info->is_next_delete && !info->is_delete)
    {
      cairo_move_to (cr,
                     area->x + area->width,
                     area->y + area->height);
      cairo_line_to (cr,
                     area->x + area->width - DELETE_WIDTH,
                     area->y + area->height);
      cairo_line_to (cr,
                     area->x + area->width - DELETE_WIDTH,
                     area->y + area->height - (DELETE_HEIGHT / 2));
      cairo_line_to (cr,
                     area->x + area->width,
                     area->y + area->height);
      gdk_cairo_set_source_rgba (cr, &self->changes.remove);
      cairo_fill (cr);
    }

  if (info->is_delete && !info->is_prev_delete)
    {
      cairo_move_to (cr,
                     area->x + area->width,
                     area->y);
      cairo_line_to (cr,
                     area->x + area->width - DELETE_WIDTH,
                     area->y);
      cairo_line_to (cr,
                     area->x + area->width - DELETE_WIDTH,
                     area->y + (DELETE_HEIGHT / 2));
      cairo_line_to (cr,
                     area->x + area->width,
                     area->y);
      gdk_cairo_set_source_rgba (cr, &self->changes.remove);
      cairo_fill (cr);
    }
}

static void
draw_diagnostic (IdeOmniGutterRenderer        *self,
                 cairo_t                      *cr,
                 GdkRectangle                 *area,
                 LineInfo                     *info,
                 GtkSourceGutterRendererState  state)
{
  cairo_surface_t *surface = NULL;

  g_assert (IDE_IS_OMNI_GUTTER_RENDERER (self));
  g_assert (cr != NULL);
  g_assert (area != NULL);

  if (IS_BREAKPOINT (info) || (state & GTK_SOURCE_GUTTER_RENDERER_STATE_PRELIT))
    {
      if (info->is_error)
        surface = self->error_selected_surface;
      else if (info->is_warning)
        surface = self->warning_selected_surface;
      else if (info->is_note)
        surface = self->note_selected_surface;
    }
  else
    {
      if (info->is_error)
        surface = self->error_surface;
      else if (info->is_warning)
        surface = self->warning_surface;
      else if (info->is_note)
        surface = self->note_surface;
    }

  if (surface != NULL)
    {
      cairo_rectangle (cr,
                       area->x + 2,
                       area->y + ((area->height - DIAGNOSTICS_SIZE) / 2),
                       DIAGNOSTICS_SIZE, DIAGNOSTICS_SIZE);
      cairo_set_source_surface (cr,
                                surface,
                                area->x + 2,
                                area->y + ((area->height - DIAGNOSTICS_SIZE) / 2));
      cairo_paint (cr);
    }
}

static void
ide_omni_gutter_renderer_draw (GtkSourceGutterRenderer      *renderer,
                               cairo_t                      *cr,
                               GdkRectangle                 *bg_area,
                               GdkRectangle                 *cell_area,
                               GtkTextIter                  *begin,
                               GtkTextIter                  *end,
                               GtkSourceGutterRendererState  state)
{
  IdeOmniGutterRenderer *self = (IdeOmniGutterRenderer *)renderer;
  GtkTextView *view;
  gboolean has_focus;
  gboolean highlight_line;
  guint line;

  g_assert (IDE_IS_OMNI_GUTTER_RENDERER (self));
  g_assert (cr != NULL);
  g_assert (bg_area != NULL);
  g_assert (cell_area != NULL);
  g_assert (begin != NULL);
  g_assert (end != NULL);

  /*
   * This is our primary draw routine. It is called for every line that
   * is visible. We are incredibly sensitive to performance churn here
   * so it is important that we be as minimal as possible while
   * retaining the features we need.
   */

  view = gtk_source_gutter_renderer_get_view (GTK_SOURCE_GUTTER_RENDERER (self));
  highlight_line = gtk_source_view_get_highlight_current_line (GTK_SOURCE_VIEW (view));
  has_focus = gtk_widget_has_focus (GTK_WIDGET (view));

  line = gtk_text_iter_get_line (begin);

  if ((line - self->begin_line) < self->lines->len)
    {
      LineInfo *info = &g_array_index (self->lines, LineInfo, line - self->begin_line);
      gboolean active = state & GTK_SOURCE_GUTTER_RENDERER_STATE_PRELIT;
      gboolean has_breakpoint = FALSE;
      gboolean bold = FALSE;
      gchar linestr[16];
      gint len;

      /*
       * Draw some background for the line so that it looks like the
       * breakpoint arrow draws over it. Debugger break line takes
       * precidence over the current highlight line. Also, ensure that
       * the view is drawing the highlight line first.
       */
      if (line == self->stopped_line)
        {
          gdk_cairo_rectangle (cr, bg_area);
          gdk_cairo_set_source_rgba (cr, &self->stopped_bg);
          cairo_fill (cr);
        }
      else if (highlight_line && has_focus && (state & GTK_SOURCE_GUTTER_RENDERER_STATE_CURSOR))
        {
          gdk_cairo_rectangle (cr, bg_area);
          gdk_cairo_set_source_rgba (cr, &self->current.bg);
          cairo_fill (cr);
        }

      /* Draw line changes next so it will show up underneath the
       * breakpoint arrows.
       */
      if (self->show_line_changes && IS_LINE_CHANGE (info))
        draw_line_change (self, cr, cell_area, info, state);

      /* Draw breakpoint arrows if we have any breakpoints that could
       * potentially match.
       */
      if (self->breakpoints != NULL)
        {
          has_breakpoint = IS_BREAKPOINT (info);
          if (has_breakpoint || active)
            draw_breakpoint_bg (self, cr, cell_area, info, state);
        }

      /* Now that we might have an altered background for the line,
       * we can draw the diagnostic icon (with possibly altered
       * color for symbolic icon).
       */
      if (self->show_line_diagnostics && IS_DIAGNOSTIC (info))
        draw_diagnostic (self, cr, cell_area, info, state);

      /*
       * Now draw the line numbers if we are showing them. Ensure
       * we tweak the style to match closely to how the default
       * gtksourceview lines gutter renderer does it.
       */
      if (self->show_line_numbers)
        {
          /* TODO: Easy performance win here is to use an array of
           *       strings containing the first 1000 or so line numbers
           *       with direct index offsets.
           */
          len = g_snprintf (linestr, sizeof linestr, "%u", line + 1);
          pango_layout_set_text (self->layout, linestr, len);

          cairo_move_to (cr, cell_area->x, cell_area->y);

          if (has_breakpoint || active)
            {
              gdk_cairo_set_source_rgba (cr, &self->bkpt.fg);
              bold = self->bkpt.bold;
            }
          else if (state & GTK_SOURCE_GUTTER_RENDERER_STATE_CURSOR)
            {
              gdk_cairo_set_source_rgba (cr, &self->current.fg);
              bold = self->current.bold;
            }
          else
            {
              gdk_cairo_set_source_rgba (cr, &self->text.fg);
              bold = self->text.bold;
            }

          /* Current line is always bold */
          if (state & GTK_SOURCE_GUTTER_RENDERER_STATE_CURSOR)
            bold |= self->current.bold;

          pango_layout_set_attributes (self->layout, bold ? self->bold_attrs : NULL);
          pango_cairo_show_layout (cr, self->layout);
        }
    }
}

static cairo_surface_t *
get_icon_surface (IdeOmniGutterRenderer *self,
                  GtkWidget             *widget,
                  const gchar           *icon_name,
                  gint                   size,
                  gboolean               selected)
{
  GtkIconTheme *icon_theme;
  GtkIconInfo *info;
  GdkScreen *screen;
  GtkIconLookupFlags flags;
  gint scale;

  g_assert (IDE_IS_OMNI_GUTTER_RENDERER (self));
  g_assert (GTK_IS_WIDGET (widget));
  g_assert (icon_name != NULL);

  /*
   * This deals with loading a given icon by icon name and trying to
   * apply our current style as the symbolic colors. We do not support
   * error/warning/etc for symbolic icons so they are all replaced with
   * the proper foreground color.
   *
   * If selected is set, we alter the color to make sure it will look
   * good on top of a breakpoint arrow.
   */

  screen = gtk_widget_get_screen (widget);
  icon_theme = gtk_icon_theme_get_for_screen (screen);

  flags = GTK_ICON_LOOKUP_USE_BUILTIN;
  scale = gtk_widget_get_scale_factor (widget);

  info = gtk_icon_theme_lookup_icon_for_scale (icon_theme, icon_name, size, scale, flags);

  if (info != NULL)
    {
      g_autoptr(GdkPixbuf) pixbuf = NULL;

      if (gtk_icon_info_is_symbolic (info))
        {
          GdkRGBA fg;

          if (selected)
            fg = self->bkpt.fg;
          else
            fg = self->text.fg;

          pixbuf = gtk_icon_info_load_symbolic (info, &fg, &fg, &fg, &fg, NULL, NULL);
        }
      else
        pixbuf = gtk_icon_info_load_icon (info, NULL);

      if (pixbuf != NULL)
        return gdk_cairo_surface_create_from_pixbuf (pixbuf, scale, NULL);
    }

  return NULL;
}

static void
ide_omni_gutter_renderer_reload_icons (IdeOmniGutterRenderer *self)
{
  GtkTextView *view;

  g_assert (IDE_IS_OMNI_GUTTER_RENDERER (self));

  /*
   * This isn't ideal (we should find a better way to cache icons that
   * is safe with scale and foreground color changes we need).
   *
   * TODO: Create something similar to pixbuf helpers that allow for
   *       more control over the cache key so it can be shared between
   *       multiple instances.
   */

  g_clear_pointer (&self->note_surface, cairo_surface_destroy);
  g_clear_pointer (&self->warning_surface, cairo_surface_destroy);
  g_clear_pointer (&self->error_surface, cairo_surface_destroy);
  g_clear_pointer (&self->note_selected_surface, cairo_surface_destroy);
  g_clear_pointer (&self->warning_selected_surface, cairo_surface_destroy);
  g_clear_pointer (&self->error_selected_surface, cairo_surface_destroy);

  view = gtk_source_gutter_renderer_get_view (GTK_SOURCE_GUTTER_RENDERER (self));
  if (view == NULL)
    return;

  self->note_surface = get_icon_surface (self, GTK_WIDGET (view), "dialog-information-symbolic", DIAGNOSTICS_SIZE, FALSE);
  self->warning_surface = get_icon_surface (self, GTK_WIDGET (view), "dialog-warning-symbolic", DIAGNOSTICS_SIZE, FALSE);
  self->error_surface = get_icon_surface (self, GTK_WIDGET (view), "process-stop-symbolic", DIAGNOSTICS_SIZE, FALSE);

  self->note_selected_surface = get_icon_surface (self, GTK_WIDGET (view), "dialog-information-symbolic", DIAGNOSTICS_SIZE, TRUE);
  self->warning_selected_surface = get_icon_surface (self, GTK_WIDGET (view), "dialog-warning-symbolic", DIAGNOSTICS_SIZE, TRUE);
  self->error_selected_surface = get_icon_surface (self, GTK_WIDGET (view), "process-stop-symbolic", DIAGNOSTICS_SIZE, TRUE);
}

static void
ide_omni_gutter_renderer_reload (IdeOmniGutterRenderer *self)
{
  g_autoptr(IdeDebuggerBreakpoints) breakpoints = NULL;
  GtkSourceLanguage *language;
  GtkTextBuffer *buffer;
  GtkTextView *view;
  const gchar *id = NULL;

  g_assert (IDE_IS_OMNI_GUTTER_RENDERER (self));

  view = gtk_source_gutter_renderer_get_view (GTK_SOURCE_GUTTER_RENDERER (self));
  buffer = gtk_text_view_get_buffer (view);

  /*
   * Use the Language ID to determine if it makes sense to show
   * breakpoints. We don't want to show them for things like
   * markdown files and such.
   */

  if (NULL != (language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (buffer))))
    id = gtk_source_language_get_id (language);

  if (IDE_IS_BUFFER (buffer))
    {
      IdeContext *context = ide_buffer_get_context (IDE_BUFFER (buffer));
      IdeDebugManager *debug_manager = ide_context_get_debug_manager (context);

      if (ide_debug_manager_supports_language (debug_manager, id))
        {
          IdeFile *file = ide_buffer_get_file (IDE_BUFFER (buffer));
          GFile *gfile = ide_file_get_file (file);

          breakpoints = ide_debug_manager_get_breakpoints_for_file (debug_manager, gfile);
        }
    }

  /* Replace our previous breakpoints */
  g_set_object (&self->breakpoints, breakpoints);

  /* Reload icons and then recalcuate our physical size */
  ide_omni_gutter_renderer_reload_icons (self);
  ide_omni_gutter_renderer_recalculate_size (self);
}

static void
ide_omni_gutter_renderer_notify_buffer (IdeOmniGutterRenderer *self,
                                        GParamSpec            *pspec,
                                        IdeSourceView         *view)
{
  g_assert (IDE_IS_OMNI_GUTTER_RENDERER (self));
  g_assert (IDE_IS_SOURCE_VIEW (view));

  if (self->buffer_signals != NULL)
    {
      GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

      if (!IDE_IS_BUFFER (buffer))
        buffer = NULL;

      dzl_signal_group_set_target (self->buffer_signals, buffer);
      ide_omni_gutter_renderer_reload (self);
    }
}

static void
ide_omni_gutter_renderer_bind_view (IdeOmniGutterRenderer *self,
                                    IdeSourceView         *view,
                                    DzlSignalGroup        *view_signals)
{
  g_assert (IDE_IS_OMNI_GUTTER_RENDERER (self));
  g_assert (IDE_IS_SOURCE_VIEW (view));
  g_assert (DZL_IS_SIGNAL_GROUP (view_signals));

  ide_omni_gutter_renderer_notify_buffer (self, NULL, view);
}

static void
ide_omni_gutter_renderer_notify_file (IdeOmniGutterRenderer *self,
                                      GParamSpec            *pspec,
                                      IdeBuffer             *buffer)
{
  g_assert (IDE_IS_OMNI_GUTTER_RENDERER (self));
  g_assert (pspec != NULL);
  g_assert (IDE_IS_BUFFER (buffer));

  ide_omni_gutter_renderer_reload (self);
}

static void
ide_omni_gutter_renderer_notify_view (IdeOmniGutterRenderer *self)
{
  GtkTextView *view;

  g_assert (IDE_IS_OMNI_GUTTER_RENDERER (self));

  view = gtk_source_gutter_renderer_get_view (GTK_SOURCE_GUTTER_RENDERER (self));
  if (!IDE_IS_SOURCE_VIEW (view))
    view = NULL;

  dzl_signal_group_set_target (self->view_signals, view);
}

static gboolean
ide_omni_gutter_renderer_do_recalc (gpointer data)
{
  IdeOmniGutterRenderer *self = data;

  g_assert (IDE_IS_OMNI_GUTTER_RENDERER (self));

  self->resize_source = 0;
  ide_omni_gutter_renderer_recalculate_size (self);
  return G_SOURCE_REMOVE;
}

static void
ide_omni_gutter_renderer_buffer_changed (IdeOmniGutterRenderer *self,
                                         IdeBuffer             *buffer)
{
  g_assert (IDE_IS_OMNI_GUTTER_RENDERER (self));
  g_assert (IDE_IS_BUFFER (buffer));

  /* Run immediately at the end of this main loop iteration */
  if (self->resize_source == 0)
    self->resize_source = gdk_threads_add_idle_full (G_PRIORITY_HIGH,
                                                     ide_omni_gutter_renderer_do_recalc,
                                                     g_object_ref (self),
                                                     g_object_unref);
}

static void
ide_omni_gutter_renderer_notify_style_scheme (IdeOmniGutterRenderer *self,
                                              GParamSpec            *pspec,
                                              IdeBuffer             *buffer)
{
  GtkSourceStyleScheme *scheme;

  g_assert (IDE_IS_OMNI_GUTTER_RENDERER (self));
  g_assert (IDE_IS_BUFFER (buffer));

  /* Update our cached rgba colors */
  scheme = gtk_source_buffer_get_style_scheme (GTK_SOURCE_BUFFER (buffer));
  reload_style_colors (self, scheme);

  /* Regenerate icons matching the scheme colors */
  ide_omni_gutter_renderer_reload_icons (self);
}

static void
ide_omni_gutter_renderer_bind_buffer (IdeOmniGutterRenderer *self,
                                      IdeBuffer             *buffer,
                                      DzlSignalGroup        *buffer_signals)
{
  g_assert (IDE_IS_OMNI_GUTTER_RENDERER (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (DZL_IS_SIGNAL_GROUP (buffer_signals));

  ide_omni_gutter_renderer_notify_style_scheme (self, NULL, buffer);
}

static void
ide_omni_gutter_renderer_constructed (GObject *object)
{
  IdeOmniGutterRenderer *self = (IdeOmniGutterRenderer *)object;
  GtkTextView *view;

  g_assert (IDE_IS_OMNI_GUTTER_RENDERER (self));

  G_OBJECT_CLASS (ide_omni_gutter_renderer_parent_class)->constructed (object);

  view = gtk_source_gutter_renderer_get_view (GTK_SOURCE_GUTTER_RENDERER (self));
  dzl_signal_group_set_target (self->view_signals, view);
}

static void
ide_omni_gutter_renderer_dispose (GObject *object)
{
  IdeOmniGutterRenderer *self = (IdeOmniGutterRenderer *)object;

  ide_clear_source (&self->resize_source);

  g_clear_object (&self->breakpoints);
  g_clear_pointer (&self->lines, g_array_unref);

  g_clear_object (&self->view_signals);
  g_clear_object (&self->buffer_signals);

  g_clear_pointer (&self->note_surface, cairo_surface_destroy);
  g_clear_pointer (&self->warning_surface, cairo_surface_destroy);
  g_clear_pointer (&self->error_surface, cairo_surface_destroy);
  g_clear_pointer (&self->note_selected_surface, cairo_surface_destroy);
  g_clear_pointer (&self->warning_selected_surface, cairo_surface_destroy);
  g_clear_pointer (&self->error_selected_surface, cairo_surface_destroy);

  g_clear_object (&self->layout);
  g_clear_pointer (&self->bold_attrs, pango_attr_list_unref);

  G_OBJECT_CLASS (ide_omni_gutter_renderer_parent_class)->dispose (object);
}

static void
ide_omni_gutter_renderer_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  IdeOmniGutterRenderer *self = IDE_OMNI_GUTTER_RENDERER (object);

  switch (prop_id)
    {
    case PROP_SHOW_LINE_CHANGES:
      g_value_set_boolean (value, self->show_line_changes);
      break;

    case PROP_SHOW_LINE_DIAGNOSTICS:
      g_value_set_boolean (value, self->show_line_diagnostics);
      break;

    case PROP_SHOW_LINE_NUMBERS:
      g_value_set_boolean (value, self->show_line_numbers);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_omni_gutter_renderer_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  IdeOmniGutterRenderer *self = IDE_OMNI_GUTTER_RENDERER (object);

  switch (prop_id)
    {
    case PROP_SHOW_LINE_CHANGES:
      ide_omni_gutter_renderer_set_show_line_changes (self, g_value_get_boolean (value));
      break;

    case PROP_SHOW_LINE_DIAGNOSTICS:
      ide_omni_gutter_renderer_set_show_line_diagnostics (self, g_value_get_boolean (value));
      break;

    case PROP_SHOW_LINE_NUMBERS:
      ide_omni_gutter_renderer_set_show_line_numbers (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_omni_gutter_renderer_class_init (IdeOmniGutterRendererClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkSourceGutterRendererClass *renderer_class = GTK_SOURCE_GUTTER_RENDERER_CLASS (klass);

  object_class->constructed = ide_omni_gutter_renderer_constructed;
  object_class->dispose = ide_omni_gutter_renderer_dispose;
  object_class->get_property = ide_omni_gutter_renderer_get_property;
  object_class->set_property = ide_omni_gutter_renderer_set_property;

  renderer_class->draw = ide_omni_gutter_renderer_draw;
  renderer_class->begin = ide_omni_gutter_renderer_begin;
  renderer_class->end = ide_omni_gutter_renderer_end;
  renderer_class->query_activatable = ide_omni_gutter_renderer_query_activatable;
  renderer_class->activate = ide_omni_gutter_renderer_activate;

  properties [PROP_SHOW_LINE_CHANGES] =
    g_param_spec_boolean ("show-line-changes", NULL, NULL, TRUE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties [PROP_SHOW_LINE_NUMBERS] =
    g_param_spec_boolean ("show-line-numbers", NULL, NULL, TRUE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties [PROP_SHOW_LINE_DIAGNOSTICS] =
    g_param_spec_boolean ("show-line-diagnostics", NULL, NULL, TRUE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_omni_gutter_renderer_init (IdeOmniGutterRenderer *self)
{
  self->show_line_changes = TRUE;
  self->show_line_diagnostics = TRUE;
  self->show_line_diagnostics = TRUE;

  self->lines = g_array_new (FALSE, FALSE, sizeof (LineInfo));

  g_signal_connect (self,
                    "notify::view",
                    G_CALLBACK (ide_omni_gutter_renderer_notify_view),
                    NULL);

  self->buffer_signals = dzl_signal_group_new (IDE_TYPE_BUFFER);

  g_signal_connect_swapped (self->buffer_signals,
                            "bind",
                            G_CALLBACK (ide_omni_gutter_renderer_bind_buffer),
                            self);

  dzl_signal_group_connect_swapped (self->buffer_signals,
                                    "notify::file",
                                    G_CALLBACK (ide_omni_gutter_renderer_notify_file),
                                    self);

  dzl_signal_group_connect_swapped (self->buffer_signals,
                                    "notify::style-scheme",
                                    G_CALLBACK (ide_omni_gutter_renderer_notify_style_scheme),
                                    self);

  dzl_signal_group_connect_swapped (self->buffer_signals,
                                    "changed",
                                    G_CALLBACK (ide_omni_gutter_renderer_buffer_changed),
                                    self);

  self->view_signals = dzl_signal_group_new (IDE_TYPE_SOURCE_VIEW);

  g_signal_connect_swapped (self->view_signals,
                            "bind",
                            G_CALLBACK (ide_omni_gutter_renderer_bind_view),
                            self);

  dzl_signal_group_connect_swapped (self->view_signals,
                                    "notify::buffer",
                                    G_CALLBACK (ide_omni_gutter_renderer_notify_buffer),
                                    self);

  dzl_signal_group_connect_swapped (self->view_signals,
                                    "notify::font-desc",
                                    G_CALLBACK (ide_omni_gutter_renderer_notify_font_desc),
                                    self);

  self->bold_attrs = pango_attr_list_new ();
  pango_attr_list_insert (self->bold_attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
}

IdeOmniGutterRenderer *
ide_omni_gutter_renderer_new (void)
{
  return g_object_new (IDE_TYPE_OMNI_GUTTER_RENDERER, NULL);
}

gboolean
ide_omni_gutter_renderer_get_show_line_changes (IdeOmniGutterRenderer *self)
{
  g_return_val_if_fail (IDE_IS_OMNI_GUTTER_RENDERER (self), FALSE);
  return self->show_line_changes;
}

gboolean
ide_omni_gutter_renderer_get_show_line_diagnostics (IdeOmniGutterRenderer *self)
{
  g_return_val_if_fail (IDE_IS_OMNI_GUTTER_RENDERER (self), FALSE);
  return self->show_line_diagnostics;
}

gboolean
ide_omni_gutter_renderer_get_show_line_numbers (IdeOmniGutterRenderer *self)
{
  g_return_val_if_fail (IDE_IS_OMNI_GUTTER_RENDERER (self), FALSE);
  return self->show_line_numbers;
}

void
ide_omni_gutter_renderer_set_show_line_changes (IdeOmniGutterRenderer *self,
                                                gboolean               show_line_changes)
{
  g_return_if_fail (IDE_IS_OMNI_GUTTER_RENDERER (self));

  show_line_changes = !!show_line_changes;

  if (show_line_changes != self->show_line_changes)
    {
      self->show_line_changes = show_line_changes;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SHOW_LINE_CHANGES]);
      ide_omni_gutter_renderer_recalculate_size (self);
    }
}

void
ide_omni_gutter_renderer_set_show_line_diagnostics (IdeOmniGutterRenderer *self,
                                                    gboolean               show_line_diagnostics)
{
  g_return_if_fail (IDE_IS_OMNI_GUTTER_RENDERER (self));

  show_line_diagnostics = !!show_line_diagnostics;

  if (show_line_diagnostics != self->show_line_diagnostics)
    {
      self->show_line_diagnostics = show_line_diagnostics;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SHOW_LINE_DIAGNOSTICS]);
      ide_omni_gutter_renderer_recalculate_size (self);
    }
}

void
ide_omni_gutter_renderer_set_show_line_numbers (IdeOmniGutterRenderer *self,
                                                gboolean               show_line_numbers)
{
  g_return_if_fail (IDE_IS_OMNI_GUTTER_RENDERER (self));

  show_line_numbers = !!show_line_numbers;

  if (show_line_numbers != self->show_line_numbers)
    {
      self->show_line_numbers = show_line_numbers;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SHOW_LINE_NUMBERS]);
      ide_omni_gutter_renderer_recalculate_size (self);
    }
}
