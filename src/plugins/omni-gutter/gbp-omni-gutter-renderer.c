/* gbp-omni-gutter-renderer.c
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

#define G_LOG_DOMAIN "gbp-omni-gutter-renderer"

#include "config.h"

#include <dazzle.h>
#include <glib/gi18n.h>
#include <string.h>

#include <libide-core.h>
#include <libide-code.h>
#include <libide-debugger.h>
#include <libide-sourceview.h>

#include "ide-debugger-private.h"

#include "gbp-omni-gutter-renderer.h"

/**
 * SECTION:gbp-omni-gutter-renderer
 * @title: GbpOmniGutterRenderer
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
 * Since: 3.32
 */

#define ARROW_WIDTH      5
#define CHANGE_WIDTH     2
#define DELETE_WIDTH     5.0
#define DELETE_HEIGHT    8.0

#define IS_BREAKPOINT(i)  ((i)->is_breakpoint || (i)->is_countpoint || (i)->is_watchpoint)
#define IS_DIAGNOSTIC(i)  ((i)->is_error || (i)->is_warning || (i)->is_note)
#define IS_LINE_CHANGE(i) ((i)->is_add || (i)->is_change || \
                           (i)->is_delete || (i)->is_next_delete || (i)->is_prev_delete)

struct _GbpOmniGutterRenderer
{
  GtkSourceGutterRenderer parent_instance;

  GSettings *settings;
  gint line_spacing;

  IdeDebuggerBreakpoints *breakpoints;

  GArray *lines;

  DzlSignalGroup *view_signals;
  DzlSignalGroup *buffer_signals;

  /*
   * A scaled font description that matches the size of the text
   * within the source view. Cached to avoid recreating it on ever
   * frame render.
   */
  PangoFontDescription *scaled_font_desc;

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
  } text, current, bkpt, ctpt;
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
   * Calculated size for diagnostics, to be a nearest icon-size based
   * on the height of the line text.
   */
  gint diag_size;

  /*
   * Line that the cursor is on. Used for relative line number rendering.
   */
  guint cursor_line;

  /*
   * Some users might want to toggle off individual features of the
   * omni gutter, and these boolean properties provide that. Other
   * components map them to GSettings values to be toggled.
   */
  guint show_line_changes : 1;
  guint show_line_numbers : 1;
  guint show_relative_line_numbers : 1;
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
  PROP_SHOW_RELATIVE_LINE_NUMBERS,
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

static void gbp_omni_gutter_renderer_reload_icons (GbpOmniGutterRenderer *self);
static void gutter_iface_init                     (IdeGutterInterface    *iface);

G_DEFINE_TYPE_WITH_CODE (GbpOmniGutterRenderer,
                         gbp_omni_gutter_renderer,
                         GTK_SOURCE_TYPE_GUTTER_RENDERER,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_GUTTER, gutter_iface_init))

static GParamSpec *properties [N_PROPS];

static gint
int_to_string (guint         value,
               const gchar **outstr)
{
	static struct{
		guint value;
		guint len;
		gchar str[12];
	} fi;

  *outstr = fi.str;

  if (value == fi.value)
    return fi.len;

  if G_LIKELY (value == fi.value + 1)
    {
      guint carry = 1;

      for (gint i = fi.len - 1; i >= 0; i--)
        {
          fi.str[i] += carry;
          carry = fi.str[i] == ':';

          if (carry)
            fi.str[i] = '0';
          else
            break;
        }

      if G_UNLIKELY (carry)
        {
          for (guint i = fi.len; i > 0; i--)
            fi.str[i] = fi.str[i-1];

          fi.len++;
          fi.str[0] = '1';
          fi.str[fi.len] = 0;
        }

      fi.value++;

      return fi.len;
    }

  fi.len = snprintf (fi.str, sizeof fi.str - 1, "%u", value);
  fi.str[fi.len] = 0;
  fi.value = value;

  return fi.len;
}

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
reload_style_colors (GbpOmniGutterRenderer *self,
                     GtkSourceStyleScheme  *scheme)
{
  GtkStyleContext *context;
  GtkTextView *view;
  GtkStateFlags state;
  GdkRGBA fg;
  GdkRGBA bg;

  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (self));
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

  /* These gutter:: prefix values come from Builder's style-scheme xml
   * files, but other style schemes may also support them now too.
   */
  if (!get_style_rgba (scheme, "gutter::added-line", FOREGROUND, &self->changes.add))
    gdk_rgba_parse (&self->changes.add, "#8ae234");

  if (!get_style_rgba (scheme, "gutter::changed-line", FOREGROUND, &self->changes.change))
    gdk_rgba_parse (&self->changes.change, "#fcaf3e");

  if (!get_style_rgba (scheme, "gutter::removed-line", FOREGROUND, &self->changes.remove))
    gdk_rgba_parse (&self->changes.remove, "#ef2929");

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

  /* Slight different color for countpoint, fallback to mix(selection,diff:add) */
  if (!get_style_rgba (scheme, "debugger::countpoint", FOREGROUND, &self->ctpt.fg))
    get_style_rgba (scheme, "selection", FOREGROUND, &self->ctpt.fg);
  if (!get_style_rgba (scheme, "debugger::countpoint", BACKGROUND, &self->ctpt.bg))
    {
      get_style_rgba (scheme, "selection", BACKGROUND, &self->ctpt.bg);
      self->ctpt.bg.red = (self->ctpt.bg.red + self->changes.add.red) / 2.0;
      self->ctpt.bg.green = (self->ctpt.bg.green + self->changes.add.green) / 2.0;
      self->ctpt.bg.blue = (self->ctpt.bg.blue + self->changes.add.blue) / 2.0;
    }
  if (!style_get_is_bold (scheme, "debugger::countpoint", &self->ctpt.bold))
    self->ctpt.bold = FALSE;
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
gbp_omni_gutter_renderer_load_breakpoints (GbpOmniGutterRenderer *self,
                                           GtkTextIter           *begin,
                                           GtkTextIter           *end,
                                           GArray                *lines)
{
  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (self));
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
populate_diagnostics_cb (guint                 line,
                         IdeDiagnosticSeverity severity,
                         gpointer              user_data)
{
  LineInfo *info;
  struct {
    GArray *lines;
    guint   begin_line;
    guint   end_line;
  } *state = user_data;

  g_assert (line >= state->begin_line);
  g_assert (line <= state->end_line);

  info = &g_array_index (state->lines, LineInfo, line - state->begin_line);
  info->is_warning |= !!(severity & (IDE_DIAGNOSTIC_WARNING | IDE_DIAGNOSTIC_DEPRECATED));
  info->is_error |= !!(severity & (IDE_DIAGNOSTIC_ERROR | IDE_DIAGNOSTIC_FATAL));
  info->is_note |= !!(severity & IDE_DIAGNOSTIC_NOTE);
}

static void
populate_changes_cb (guint               line,
                     IdeBufferLineChange change,
                     gpointer            user_data)
{
  LineInfo *info;
  struct {
    GArray *lines;
    guint   begin_line;
    guint   end_line;
  } *state = user_data;
  guint pos;

  g_assert (line >= state->begin_line);
  g_assert (line <= state->end_line);

  pos = line - state->begin_line;

  info = &g_array_index (state->lines, LineInfo, pos);
  info->is_add = !!(change & IDE_BUFFER_LINE_CHANGE_ADDED);
  info->is_change = !!(change & IDE_BUFFER_LINE_CHANGE_CHANGED);
  info->is_delete = !!(change & IDE_BUFFER_LINE_CHANGE_DELETED);
  info->is_prev_delete = !!(change & IDE_BUFFER_LINE_CHANGE_PREVIOUS_DELETED);

  if (pos > 0)
    {
      LineInfo *last = &g_array_index (state->lines, LineInfo, pos - 1);

      info->is_prev_delete |= last->is_delete;
      last->is_next_delete = info->is_delete;
    }
}

static void
gbp_omni_gutter_renderer_load_basic (GbpOmniGutterRenderer *self,
                                     GtkTextIter           *begin,
                                     GArray                *lines)
{
  IdeBufferChangeMonitor *monitor;
  IdeDiagnostics *diagnostics;
  GtkTextBuffer *buffer;
  GFile *file;
  struct {
    GArray *lines;
    guint   begin_line;
    guint   end_line;
  } state;

  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (self));
  g_assert (begin != NULL);
  g_assert (lines != NULL);
  g_assert (lines->len > 0);

  buffer = gtk_text_iter_get_buffer (begin);
  if (!IDE_IS_BUFFER (buffer))
    return;

  file = ide_buffer_get_file (IDE_BUFFER (buffer));

  state.lines = lines;
  state.begin_line = gtk_text_iter_get_line (begin);
  state.end_line = state.begin_line + lines->len;

  if ((diagnostics = ide_buffer_get_diagnostics (IDE_BUFFER (buffer))))
    ide_diagnostics_foreach_line_in_range (diagnostics,
                                           file,
                                           state.begin_line,
                                           state.end_line,
                                           populate_diagnostics_cb,
                                           &state);

  if ((monitor = ide_buffer_get_change_monitor (IDE_BUFFER (buffer))))
    ide_buffer_change_monitor_foreach_change (monitor,
                                              state.begin_line,
                                              state.end_line,
                                              populate_changes_cb,
                                              &state);
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

static gint
calculate_diagnostics_size (gint height)
{
  static guint sizes[] = { 64, 48, 32, 24, 16, 8 };

  for (guint i = 0; i < G_N_ELEMENTS (sizes); i++)
    {
      if (height >= sizes[i])
        return sizes[i];
    }

  return sizes [G_N_ELEMENTS (sizes) - 1];
}

static void
gbp_omni_gutter_renderer_recalculate_size (GbpOmniGutterRenderer *self)
{
  g_autofree gchar *numbers = NULL;
  GtkTextBuffer *buffer;
  GtkTextView *view;
  PangoLayout *layout;
  GtkTextIter end;
  guint line;
  int height;
  gint size = 0;

  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (self));

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
   * Stash the font description for future use.
   */
  g_clear_pointer (&self->scaled_font_desc, pango_font_description_free);
  self->scaled_font_desc = ide_source_view_get_scaled_font_desc (IDE_SOURCE_VIEW (view));

  /*
   * Get the font description used by the IdeSourceView so we can
   * match the font styling as much as possible.
   */
  layout = gtk_widget_create_pango_layout (GTK_WIDGET (view), numbers);
  pango_layout_set_font_description (layout, self->scaled_font_desc);

  /*
   * Now cache the width of the text layout so we can simplify our
   * positioning later. We simply size everything the same and then
   * align to the right to reduce the draw overhead.
   */
  pango_layout_get_pixel_size (layout, &self->number_width, &height);

  /*
   * Calculate the nearest size for diagnostics so they scale somewhat
   * reasonable with the character size.
   */
  self->diag_size = calculate_diagnostics_size (MAX (16, height));
  g_assert (self->diag_size > 0);

  /* Now calculate the size based on enabled features */
  size = 2;
  if (self->show_line_diagnostics)
    size += self->diag_size + 2;
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
  gtk_source_gutter_renderer_set_size (GTK_SOURCE_GUTTER_RENDERER (self), size);
  gtk_source_gutter_renderer_queue_draw (GTK_SOURCE_GUTTER_RENDERER (self));

  g_clear_object (&layout);
}

static void
gbp_omni_gutter_renderer_notify_font_desc (GbpOmniGutterRenderer *self,
                                           GParamSpec            *pspec,
                                           IdeSourceView         *view)
{
  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (self));
  g_assert (IDE_IS_SOURCE_VIEW (view));

  gbp_omni_gutter_renderer_recalculate_size (self);
  gbp_omni_gutter_renderer_reload_icons (self);
}

static void
gbp_omni_gutter_renderer_end (GtkSourceGutterRenderer *renderer)
{
  GbpOmniGutterRenderer *self = (GbpOmniGutterRenderer *)renderer;

  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (self));

  g_clear_object (&self->layout);
}

static void
gbp_omni_gutter_renderer_begin (GtkSourceGutterRenderer *renderer,
                                cairo_t                 *cr,
                                GdkRectangle            *bg_area,
                                GdkRectangle            *cell_area,
                                GtkTextIter             *begin,
                                GtkTextIter             *end)
{
  GbpOmniGutterRenderer *self = (GbpOmniGutterRenderer *)renderer;
  GtkTextTagTable *table;
  GtkTextBuffer *buffer;
  IdeSourceView *view;
  GtkTextTag *tag;
  GtkTextIter bkpt;
  guint end_line;

  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (renderer));
  g_assert (cr != NULL);
  g_assert (bg_area != NULL);
  g_assert (cell_area != NULL);
  g_assert (begin != NULL);
  g_assert (end != NULL);

  /* Draw the full background color up front */
  gdk_cairo_rectangle (cr, cell_area);
  gdk_cairo_set_source_rgba (cr, &self->text.bg);
  cairo_fill (cr);

  self->line_spacing = g_settings_get_int (self->settings, "line-spacing");

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

  ide_source_view_get_visual_position (view, &self->cursor_line, NULL);

  /* Give ourselves a fresh array to stash our line info */
  g_array_set_size (self->lines, end_line - self->begin_line + 1);
  memset (self->lines->data, 0, self->lines->len * sizeof (LineInfo));

  /* Now load breakpoints, diagnostics, and line changes */
  gbp_omni_gutter_renderer_load_basic (self, begin, self->lines);
  gbp_omni_gutter_renderer_load_breakpoints (self, begin, end, self->lines);

  /* Create a new layout for rendering lines to */
  self->layout = gtk_widget_create_pango_layout (GTK_WIDGET (view), "");
  pango_layout_set_alignment (self->layout, PANGO_ALIGN_RIGHT);
  pango_layout_set_font_description (self->layout, self->scaled_font_desc);
  pango_layout_set_width (self->layout, (cell_area->width - ARROW_WIDTH - 4) * PANGO_SCALE);
}

static gboolean
gbp_omni_gutter_renderer_query_activatable (GtkSourceGutterRenderer *renderer,
                                            GtkTextIter             *begin,
                                            GdkRectangle            *area,
                                            GdkEvent                *event)
{
  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (renderer));
  g_assert (begin != NULL);
  g_assert (area != NULL);
  g_assert (event != NULL);

  /* Clicking will move the cursor, so always TRUE */

  return TRUE;
}

static void
animate_at_iter (GbpOmniGutterRenderer *self,
                 GdkRectangle          *area,
                 GtkTextIter           *iter)
{
  DzlBoxTheatric *theatric;
  GtkTextView *view;

  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (self));
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
gbp_omni_gutter_renderer_activate (GtkSourceGutterRenderer *renderer,
                                   GtkTextIter             *iter,
                                   GdkRectangle            *area,
                                   GdkEvent                *event)
{
  GbpOmniGutterRenderer *self = (GbpOmniGutterRenderer *)renderer;
  IdeDebuggerBreakpoint *breakpoint;
  IdeDebuggerBreakMode break_type = IDE_DEBUGGER_BREAK_NONE;
  g_autofree gchar *path = NULL;
  g_autoptr(IdeContext) context = NULL;
  IdeDebugManager *debug_manager;
  GtkTextBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;
  GFile *file;
  guint line;

  IDE_ENTRY;

  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (self));
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

  context = ide_buffer_ref_context (IDE_BUFFER (buffer));
  debug_manager = ide_debug_manager_from_context (context);

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
draw_breakpoint_bg (GbpOmniGutterRenderer        *self,
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

  if (info->is_countpoint)
    rgba = self->ctpt.bg;
  else
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
draw_line_change (GbpOmniGutterRenderer        *self,
                  cairo_t                      *cr,
                  GdkRectangle                 *area,
                  LineInfo                     *info,
                  guint                         line,
                  GtkSourceGutterRendererState  state)
{
  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (self));
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

  if (line == 0 && info->is_prev_delete)
    {
      cairo_move_to (cr,
                     area->x + area->width,
                     area->y);
      cairo_line_to (cr,
                     area->x + area->width - DELETE_WIDTH,
                     area->y + DELETE_HEIGHT / 2);
      cairo_line_to (cr,
                     area->x + area->width - DELETE_WIDTH,
                     area->y);
      cairo_line_to (cr,
                     area->x + area->width,
                     area->y);
      gdk_cairo_set_source_rgba (cr, &self->changes.remove);
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
draw_diagnostic (GbpOmniGutterRenderer        *self,
                 cairo_t                      *cr,
                 GdkRectangle                 *area,
                 LineInfo                     *info,
                 gint                          diag_size,
                 GtkSourceGutterRendererState  state)
{
  cairo_surface_t *surface = NULL;

  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (self));
  g_assert (cr != NULL);
  g_assert (area != NULL);
  g_assert (diag_size > 0);

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
                       area->y + ((area->height - diag_size) / 2),
                       diag_size, diag_size);
      cairo_set_source_surface (cr,
                                surface,
                                area->x + 2,
                                area->y + ((area->height - diag_size) / 2));
      cairo_paint (cr);
    }
}

static void
gbp_omni_gutter_renderer_draw (GtkSourceGutterRenderer      *renderer,
                               cairo_t                      *cr,
                               GdkRectangle                 *bg_area,
                               GdkRectangle                 *cell_area,
                               GtkTextIter                  *begin,
                               GtkTextIter                  *end,
                               GtkSourceGutterRendererState  state)
{
  GbpOmniGutterRenderer *self = (GbpOmniGutterRenderer *)renderer;
  GtkTextView *view;
  gboolean has_focus;
  gboolean highlight_line;
  guint line;

  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (self));
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
        draw_line_change (self, cr, cell_area, info, line, state);

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
        draw_diagnostic (self, cr, cell_area, info, self->diag_size, state);

      /*
       * Now draw the line numbers if we are showing them. Ensure
       * we tweak the style to match closely to how the default
       * gtksourceview lines gutter renderer does it.
       */
      if (self->show_line_numbers)
        {
          const gchar *linestr = NULL;
          gint len;
          guint shown_line;

          if (!self->show_relative_line_numbers || line == self->cursor_line)
            shown_line = line + 1;
          else if (line > self->cursor_line)
            shown_line = line - self->cursor_line;
          else
            shown_line = self->cursor_line - line;

          len = int_to_string (shown_line, &linestr);
          pango_layout_set_text (self->layout, linestr, len);

          cairo_move_to (cr, cell_area->x, cell_area->y);

          if (has_breakpoint || (self->breakpoints != NULL && active))
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

          cairo_move_to (cr, cell_area->x, cell_area->y + self->line_spacing);
          pango_layout_set_attributes (self->layout, bold ? self->bold_attrs : NULL);
          pango_cairo_show_layout (cr, self->layout);
        }
    }
}

static cairo_surface_t *
get_icon_surface (GbpOmniGutterRenderer *self,
                  GtkWidget             *widget,
                  const gchar           *icon_name,
                  gint                   size,
                  gboolean               selected)
{
  g_autoptr(GtkIconInfo) info = NULL;
  GtkIconTheme *icon_theme;
  GdkScreen *screen;
  GtkIconLookupFlags flags;
  gint scale;

  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (self));
  g_assert (GTK_IS_WIDGET (widget));
  g_assert (icon_name != NULL);
  g_assert (size > 0);

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
gbp_omni_gutter_renderer_reload_icons (GbpOmniGutterRenderer *self)
{
  GtkTextView *view;

  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (self));

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

  self->note_surface = get_icon_surface (self, GTK_WIDGET (view), "dialog-information-symbolic", self->diag_size, FALSE);
  self->warning_surface = get_icon_surface (self, GTK_WIDGET (view), "dialog-warning-symbolic", self->diag_size, FALSE);
  self->error_surface = get_icon_surface (self, GTK_WIDGET (view), "process-stop-symbolic", self->diag_size, FALSE);

  self->note_selected_surface = get_icon_surface (self, GTK_WIDGET (view), "dialog-information-symbolic", self->diag_size, TRUE);
  self->warning_selected_surface = get_icon_surface (self, GTK_WIDGET (view), "dialog-warning-symbolic", self->diag_size, TRUE);
  self->error_selected_surface = get_icon_surface (self, GTK_WIDGET (view), "process-stop-symbolic", self->diag_size, TRUE);
}

static void
gbp_omni_gutter_renderer_reload (GbpOmniGutterRenderer *self)
{
  g_autoptr(IdeDebuggerBreakpoints) breakpoints = NULL;
  GtkTextBuffer *buffer;
  GtkTextView *view;

  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (self));

  view = gtk_source_gutter_renderer_get_view (GTK_SOURCE_GUTTER_RENDERER (self));
  buffer = gtk_text_view_get_buffer (view);

  if (IDE_IS_BUFFER (buffer))
    {
      g_autoptr(IdeContext) context = NULL;
      IdeDebugManager *debug_manager;
      const gchar *lang_id;

      context = ide_buffer_ref_context (IDE_BUFFER (buffer));
      debug_manager = ide_debug_manager_from_context (context);
      lang_id = ide_buffer_get_language_id (IDE_BUFFER (buffer));

      if (ide_debug_manager_supports_language (debug_manager, lang_id))
        {
          GFile *file = ide_buffer_get_file (IDE_BUFFER (buffer));

          breakpoints = ide_debug_manager_get_breakpoints_for_file (debug_manager, file);
        }
    }

  /* Replace our previous breakpoints */
  g_set_object (&self->breakpoints, breakpoints);

  /* Reload icons and then recalcuate our physical size */
  gbp_omni_gutter_renderer_recalculate_size (self);
  gbp_omni_gutter_renderer_reload_icons (self);
}

static void
gbp_omni_gutter_renderer_notify_buffer (GbpOmniGutterRenderer *self,
                                        GParamSpec            *pspec,
                                        IdeSourceView         *view)
{
  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (self));
  g_assert (IDE_IS_SOURCE_VIEW (view));

  if (self->buffer_signals != NULL)
    {
      GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

      if (!IDE_IS_BUFFER (buffer))
        buffer = NULL;

      dzl_signal_group_set_target (self->buffer_signals, buffer);
      gbp_omni_gutter_renderer_reload (self);
    }
}

static void
gbp_omni_gutter_renderer_bind_view (GbpOmniGutterRenderer *self,
                                    IdeSourceView         *view,
                                    DzlSignalGroup        *view_signals)
{
  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (self));
  g_assert (IDE_IS_SOURCE_VIEW (view));
  g_assert (DZL_IS_SIGNAL_GROUP (view_signals));

  gbp_omni_gutter_renderer_notify_buffer (self, NULL, view);
}

static void
gbp_omni_gutter_renderer_notify_view (GbpOmniGutterRenderer *self)
{
  GtkTextView *view;

  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (self));

  view = gtk_source_gutter_renderer_get_view (GTK_SOURCE_GUTTER_RENDERER (self));
  if (!IDE_IS_SOURCE_VIEW (view))
    view = NULL;

  dzl_signal_group_set_target (self->view_signals, view);
}

static gboolean
gbp_omni_gutter_renderer_do_recalc (gpointer data)
{
  GbpOmniGutterRenderer *self = data;

  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (self));

  self->resize_source = 0;

  gbp_omni_gutter_renderer_recalculate_size (self);

  return G_SOURCE_REMOVE;
}

static void
gbp_omni_gutter_renderer_buffer_changed (GbpOmniGutterRenderer *self,
                                         IdeBuffer             *buffer)
{
  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (self));
  g_assert (IDE_IS_BUFFER (buffer));

  /* Run immediately at the end of this main loop iteration */
  if (self->resize_source == 0)
    self->resize_source = gdk_threads_add_idle_full (G_PRIORITY_HIGH,
                                                     gbp_omni_gutter_renderer_do_recalc,
                                                     g_object_ref (self),
                                                     g_object_unref);
}

static void
gbp_omni_gutter_renderer_cursor_moved (GbpOmniGutterRenderer *self,
                                       const GtkTextIter     *iter,
                                       GtkTextBuffer         *buffer)
{
  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (self));

  if (self->show_relative_line_numbers)
    gtk_source_gutter_renderer_queue_draw (GTK_SOURCE_GUTTER_RENDERER (self));
}

static void
gbp_omni_gutter_renderer_notify_style_scheme (GbpOmniGutterRenderer *self,
                                              GParamSpec            *pspec,
                                              IdeBuffer             *buffer)
{
  GtkSourceStyleScheme *scheme;

  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (self));
  g_assert (IDE_IS_BUFFER (buffer));

  /* Update our cached rgba colors */
  scheme = gtk_source_buffer_get_style_scheme (GTK_SOURCE_BUFFER (buffer));
  reload_style_colors (self, scheme);

  /* Regenerate icons matching the scheme colors */
  gbp_omni_gutter_renderer_reload_icons (self);
}

static void
gbp_omni_gutter_renderer_bind_buffer (GbpOmniGutterRenderer *self,
                                      IdeBuffer             *buffer,
                                      DzlSignalGroup        *buffer_signals)
{
  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (DZL_IS_SIGNAL_GROUP (buffer_signals));

  gbp_omni_gutter_renderer_notify_style_scheme (self, NULL, buffer);
}

static void
gbp_omni_gutter_renderer_constructed (GObject *object)
{
  GbpOmniGutterRenderer *self = (GbpOmniGutterRenderer *)object;
  GtkTextView *view;

  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (self));

  G_OBJECT_CLASS (gbp_omni_gutter_renderer_parent_class)->constructed (object);

  view = gtk_source_gutter_renderer_get_view (GTK_SOURCE_GUTTER_RENDERER (self));
  dzl_signal_group_set_target (self->view_signals, view);

  self->settings = g_settings_new ("org.gnome.builder.editor");
}

static void
gbp_omni_gutter_renderer_dispose (GObject *object)
{
  GbpOmniGutterRenderer *self = (GbpOmniGutterRenderer *)object;

  dzl_clear_source (&self->resize_source);

  g_clear_object (&self->settings);
  g_clear_object (&self->breakpoints);
  g_clear_pointer (&self->lines, g_array_unref);

  g_clear_pointer (&self->scaled_font_desc, pango_font_description_free);

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

  G_OBJECT_CLASS (gbp_omni_gutter_renderer_parent_class)->dispose (object);
}

static void
gbp_omni_gutter_renderer_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  GbpOmniGutterRenderer *self = GBP_OMNI_GUTTER_RENDERER (object);

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

    case PROP_SHOW_RELATIVE_LINE_NUMBERS:
      g_value_set_boolean (value, self->show_relative_line_numbers);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_omni_gutter_renderer_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  GbpOmniGutterRenderer *self = GBP_OMNI_GUTTER_RENDERER (object);

  switch (prop_id)
    {
    case PROP_SHOW_LINE_CHANGES:
      gbp_omni_gutter_renderer_set_show_line_changes (self, g_value_get_boolean (value));
      break;

    case PROP_SHOW_LINE_DIAGNOSTICS:
      gbp_omni_gutter_renderer_set_show_line_diagnostics (self, g_value_get_boolean (value));
      break;

    case PROP_SHOW_LINE_NUMBERS:
      gbp_omni_gutter_renderer_set_show_line_numbers (self, g_value_get_boolean (value));
      break;

    case PROP_SHOW_RELATIVE_LINE_NUMBERS:
      gbp_omni_gutter_renderer_set_show_relative_line_numbers (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_omni_gutter_renderer_class_init (GbpOmniGutterRendererClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkSourceGutterRendererClass *renderer_class = GTK_SOURCE_GUTTER_RENDERER_CLASS (klass);

  object_class->constructed = gbp_omni_gutter_renderer_constructed;
  object_class->dispose = gbp_omni_gutter_renderer_dispose;
  object_class->get_property = gbp_omni_gutter_renderer_get_property;
  object_class->set_property = gbp_omni_gutter_renderer_set_property;

  renderer_class->draw = gbp_omni_gutter_renderer_draw;
  renderer_class->begin = gbp_omni_gutter_renderer_begin;
  renderer_class->end = gbp_omni_gutter_renderer_end;
  renderer_class->query_activatable = gbp_omni_gutter_renderer_query_activatable;
  renderer_class->activate = gbp_omni_gutter_renderer_activate;

  properties [PROP_SHOW_LINE_CHANGES] =
    g_param_spec_boolean ("show-line-changes", NULL, NULL, TRUE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties [PROP_SHOW_LINE_NUMBERS] =
    g_param_spec_boolean ("show-line-numbers", NULL, NULL, TRUE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties [PROP_SHOW_RELATIVE_LINE_NUMBERS] =
    g_param_spec_boolean ("show-relative-line-numbers", NULL, NULL, FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties [PROP_SHOW_LINE_DIAGNOSTICS] =
    g_param_spec_boolean ("show-line-diagnostics", NULL, NULL, TRUE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_omni_gutter_renderer_init (GbpOmniGutterRenderer *self)
{
  self->diag_size = 16;
  self->show_line_changes = TRUE;
  self->show_line_diagnostics = TRUE;
  self->show_line_diagnostics = TRUE;

  self->lines = g_array_new (FALSE, FALSE, sizeof (LineInfo));

  g_signal_connect (self,
                    "notify::view",
                    G_CALLBACK (gbp_omni_gutter_renderer_notify_view),
                    NULL);

  self->buffer_signals = dzl_signal_group_new (IDE_TYPE_BUFFER);

  g_signal_connect_swapped (self->buffer_signals,
                            "bind",
                            G_CALLBACK (gbp_omni_gutter_renderer_bind_buffer),
                            self);

  dzl_signal_group_connect_swapped (self->buffer_signals,
                                    "notify::file",
                                    G_CALLBACK (gbp_omni_gutter_renderer_reload),
                                    self);

  dzl_signal_group_connect_swapped (self->buffer_signals,
                                    "notify::language",
                                    G_CALLBACK (gbp_omni_gutter_renderer_reload),
                                    self);

  dzl_signal_group_connect_swapped (self->buffer_signals,
                                    "notify::style-scheme",
                                    G_CALLBACK (gbp_omni_gutter_renderer_notify_style_scheme),
                                    self);

  dzl_signal_group_connect_swapped (self->buffer_signals,
                                    "changed",
                                    G_CALLBACK (gbp_omni_gutter_renderer_buffer_changed),
                                    self);

  dzl_signal_group_connect_swapped (self->buffer_signals,
                                    "cursor-moved",
                                    G_CALLBACK (gbp_omni_gutter_renderer_cursor_moved),
                                    self);

  self->view_signals = dzl_signal_group_new (IDE_TYPE_SOURCE_VIEW);

  g_signal_connect_swapped (self->view_signals,
                            "bind",
                            G_CALLBACK (gbp_omni_gutter_renderer_bind_view),
                            self);

  dzl_signal_group_connect_swapped (self->view_signals,
                                    "notify::buffer",
                                    G_CALLBACK (gbp_omni_gutter_renderer_notify_buffer),
                                    self);

  dzl_signal_group_connect_swapped (self->view_signals,
                                    "notify::font-desc",
                                    G_CALLBACK (gbp_omni_gutter_renderer_notify_font_desc),
                                    self);

  self->bold_attrs = pango_attr_list_new ();
  pango_attr_list_insert (self->bold_attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
}

GbpOmniGutterRenderer *
gbp_omni_gutter_renderer_new (void)
{
  return g_object_new (GBP_TYPE_OMNI_GUTTER_RENDERER, NULL);
}

gboolean
gbp_omni_gutter_renderer_get_show_line_changes (GbpOmniGutterRenderer *self)
{
  g_return_val_if_fail (GBP_IS_OMNI_GUTTER_RENDERER (self), FALSE);

  return self->show_line_changes;
}

gboolean
gbp_omni_gutter_renderer_get_show_line_diagnostics (GbpOmniGutterRenderer *self)
{
  g_return_val_if_fail (GBP_IS_OMNI_GUTTER_RENDERER (self), FALSE);

  return self->show_line_diagnostics;
}

gboolean
gbp_omni_gutter_renderer_get_show_line_numbers (GbpOmniGutterRenderer *self)
{
  g_return_val_if_fail (GBP_IS_OMNI_GUTTER_RENDERER (self), FALSE);

  return self->show_line_numbers;
}

gboolean
gbp_omni_gutter_renderer_get_show_relative_line_numbers (GbpOmniGutterRenderer *self)
{
  g_return_val_if_fail (GBP_IS_OMNI_GUTTER_RENDERER (self), FALSE);

  return self->show_relative_line_numbers;
}

void
gbp_omni_gutter_renderer_set_show_line_changes (GbpOmniGutterRenderer *self,
                                                gboolean               show_line_changes)
{
  g_return_if_fail (GBP_IS_OMNI_GUTTER_RENDERER (self));

  show_line_changes = !!show_line_changes;

  if (show_line_changes != self->show_line_changes)
    {
      self->show_line_changes = show_line_changes;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SHOW_LINE_CHANGES]);
      gbp_omni_gutter_renderer_recalculate_size (self);
    }
}

void
gbp_omni_gutter_renderer_set_show_line_diagnostics (GbpOmniGutterRenderer *self,
                                                    gboolean               show_line_diagnostics)
{
  g_return_if_fail (GBP_IS_OMNI_GUTTER_RENDERER (self));

  show_line_diagnostics = !!show_line_diagnostics;

  if (show_line_diagnostics != self->show_line_diagnostics)
    {
      self->show_line_diagnostics = show_line_diagnostics;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SHOW_LINE_DIAGNOSTICS]);
      gbp_omni_gutter_renderer_recalculate_size (self);
    }
}

void
gbp_omni_gutter_renderer_set_show_line_numbers (GbpOmniGutterRenderer *self,
                                                gboolean               show_line_numbers)
{
  g_return_if_fail (GBP_IS_OMNI_GUTTER_RENDERER (self));

  show_line_numbers = !!show_line_numbers;

  if (show_line_numbers != self->show_line_numbers)
    {
      self->show_line_numbers = show_line_numbers;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SHOW_LINE_NUMBERS]);
      gbp_omni_gutter_renderer_recalculate_size (self);
    }
}

void
gbp_omni_gutter_renderer_set_show_relative_line_numbers (GbpOmniGutterRenderer *self,
                                                         gboolean               show_relative_line_numbers)
{
  g_return_if_fail (GBP_IS_OMNI_GUTTER_RENDERER (self));

  show_relative_line_numbers = !!show_relative_line_numbers;

  if (show_relative_line_numbers != self->show_relative_line_numbers)
    {
      self->show_relative_line_numbers = show_relative_line_numbers;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SHOW_RELATIVE_LINE_NUMBERS]);
      gtk_source_gutter_renderer_queue_draw (GTK_SOURCE_GUTTER_RENDERER (self));
    }
}

static void
gbp_omni_gutter_renderer_style_changed (IdeGutter *gutter)
{
  GbpOmniGutterRenderer *self = (GbpOmniGutterRenderer *)gutter;

  g_return_if_fail (GBP_IS_OMNI_GUTTER_RENDERER (self));

  gbp_omni_gutter_renderer_recalculate_size (self);
  gbp_omni_gutter_renderer_reload_icons (self);
}

static void
gutter_iface_init (IdeGutterInterface *iface)
{
  iface->style_changed = gbp_omni_gutter_renderer_style_changed;
}
