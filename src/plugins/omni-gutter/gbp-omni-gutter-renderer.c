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
 */

#define RIGHT_MARGIN 6
#define CHANGE_WIDTH 2
#define DELETE_WIDTH 5
#define DELETE_HEIGHT 2
#define BREAKPOINT_XPAD (CHANGE_WIDTH + 1)
#define BREAKPOINT_YPAD 1
#define BREAKPOINT_CORNER_RADIUS 5

#define IS_BREAKPOINT(i)  ((i)->is_breakpoint || (i)->is_countpoint || (i)->is_watchpoint)
#define IS_DIAGNOSTIC(i)  ((i)->is_error || (i)->is_warning || (i)->is_note)
#define IS_LINE_CHANGE(i) ((i)->is_add || (i)->is_change || \
                           (i)->is_delete || (i)->is_next_delete || (i)->is_prev_delete)

struct _GbpOmniGutterRenderer
{
  GtkSourceGutterRenderer parent_instance;

  IdeDebuggerBreakpoints *breakpoints;

  GArray *lines;

  GSignalGroup   *view_signals;
  GSignalGroup   *buffer_signals;

  GdkPaintable *note;
  GdkPaintable *warning;
  GdkPaintable *error;

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
  } text, current, bkpt, ctpt, sel, view;
  GdkRGBA stopped_bg;
  GdkRGBA current_line;
  GdkRGBA margin_bg;
  struct {
    GdkRGBA add;
    GdkRGBA remove;
    GdkRGBA change;
  } changes;

  /* Tracks changes to the buffer to give us line marks */
  IdeBufferChangeMonitor *change_monitor;

  /* The last line that was cursor to help avoid redraws */
  guint last_cursor_line;

  /*
   * We need to reuse a single pango layout while drawing all the lines
   * to keep the overhead low. We don't have pixel caching on the gutter
   * data so keeping this stuff fast is critical.
   */
  PangoLayout *layout;

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
  int stopped_line;

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
  int number_width;
  int number_height;

  /*
   * Calculated size for diagnostics, to be a nearest icon-size based
   * on the height of the line text.
   */
  int diag_size;

  /* Line that the cursor is on. Used for relative line number rendering. */
  guint cursor_line;

  /* Delayed reload timeout source */
  guint reload_source;

  /* Cached information for drawing */
  double draw_width;
  double draw_width_with_margin;
  guint draw_has_focus : 1;
  guint draw_has_selection : 1;
  guint selection_is_multi_line : 1;

  /*
   * Some users might want to toggle off individual features of the
   * omni gutter, and these boolean properties provide that. Other
   * components map them to GSettings values to be toggled.
   */
  guint show_line_changes : 1;
  guint show_line_numbers : 1;
  guint show_relative_line_numbers : 1;
  guint show_line_diagnostics : 1;
  guint show_line_selection_styling : 1;
};

enum {
  FOREGROUND,
  BACKGROUND,
  LINE_BACKGROUND,
};

enum {
  PROP_0,
  PROP_SHOW_LINE_CHANGES,
  PROP_SHOW_LINE_NUMBERS,
  PROP_SHOW_RELATIVE_LINE_NUMBERS,
  PROP_SHOW_LINE_DIAGNOSTICS,
  PROP_SHOW_LINE_SELECTION_STYLING,
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

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpOmniGutterRenderer,
                               gbp_omni_gutter_renderer,
                               GTK_SOURCE_TYPE_GUTTER_RENDERER,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_GUTTER, gutter_iface_init))

static GParamSpec *properties [N_PROPS];
static GQuark selection_quark;
static PangoAttrList *bold_attrs;

static inline gboolean
lookup_color (GtkStyleContext *context,
              const char      *name,
              GdkRGBA         *color)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  return gtk_style_context_lookup_color (context, name, color);
G_GNUC_END_IGNORE_DEPRECATIONS
}

static int
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

      for (int i = fi.len - 1; i >= 0; i--)
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

static void
gbp_omni_gutter_renderer_set_change_monitor (GbpOmniGutterRenderer  *self,
                                             IdeBufferChangeMonitor *change_monitor)
{
  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (self));
  g_assert (!change_monitor || IDE_IS_BUFFER_CHANGE_MONITOR (change_monitor));

  if (self->change_monitor == change_monitor)
    return;

  if (self->change_monitor != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->change_monitor,
                                            G_CALLBACK (gtk_widget_queue_draw),
                                            self);
      g_clear_object (&self->change_monitor);
    }

  if (change_monitor != NULL)
    {
      self->change_monitor = g_object_ref (change_monitor);
      g_signal_connect_object (change_monitor,
                               "changed",
                               G_CALLBACK (gtk_widget_queue_draw),
                               self,
                               G_CONNECT_SWAPPED);
    }

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
gbp_omni_gutter_renderer_set_breakpoints (GbpOmniGutterRenderer  *self,
                                          IdeDebuggerBreakpoints *breakpoints)
{
  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (self));
  g_assert (!breakpoints || IDE_IS_DEBUGGER_BREAKPOINTS (breakpoints));

  if (self->breakpoints == breakpoints)
    return;

  if (self->breakpoints != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->breakpoints,
                                            G_CALLBACK (gtk_widget_queue_draw),
                                            self);
      g_clear_object (&self->breakpoints);
    }

  if (breakpoints != NULL)
    {
      self->breakpoints = g_object_ref (breakpoints);
      g_signal_connect_object (breakpoints,
                               "changed",
                               G_CALLBACK (gtk_widget_queue_draw),
                               self,
                               G_CONNECT_SWAPPED);
    }

  gtk_widget_queue_draw (GTK_WIDGET (self));
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
 * @type should be set to BACKGROUND or FOREGROUND or LINE_BACKGROUND.
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
  GtkSourceLanguageManager *langs;
  GtkSourceLanguage *def;
  GtkSourceStyle *style = NULL;
  const char *fallback = style_name;

  g_assert (!scheme || GTK_SOURCE_IS_STYLE_SCHEME (scheme));
  g_assert (style_name != NULL);
  g_assert (type == FOREGROUND || type == BACKGROUND || type == LINE_BACKGROUND);
  g_assert (rgba != NULL);

  memset (rgba, 0, sizeof *rgba);

  if (scheme == NULL)
    return FALSE;

  langs = gtk_source_language_manager_get_default ();
  def = gtk_source_language_manager_get_language (langs, "def");

  g_assert (def != NULL);

  while (style == NULL && fallback != NULL)
    {
      if (!(style = gtk_source_style_scheme_get_style (scheme, fallback)))
        fallback = gtk_source_language_get_style_fallback (def, fallback);
    }

  if (style != NULL)
    {
      const char *name;
      const char *name_set;
      g_autofree gchar *str = NULL;
      gboolean set = FALSE;

      switch (type)
        {
        default:
        case FOREGROUND:
          name = "foreground";
          name_set = "foreground-set";
          break;

        case BACKGROUND:
          name = "background";
          name_set = "background-set";
          break;

        case LINE_BACKGROUND:
          name = "line-background";
          name_set = "line-background-set";
          break;
        }

      g_object_get (style,
                    name, &str,
                    name_set, &set,
                    NULL);

      if (str != NULL)
        gdk_rgba_parse (rgba, str);

      return set && rgba->alpha > .0;
    }

  return FALSE;
}

static void
reload_style_colors (GbpOmniGutterRenderer *self,
                     GtkSourceStyleScheme  *scheme)
{
  static GdkRGBA transparent;
  GtkStyleContext *context;
  GtkSourceView *view;
  GdkRGBA fg;
  GdkRGBA margin_bg;
  gboolean had_sel_fg = FALSE;
  gboolean has_margin_border;

  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (self));
  g_assert (!scheme || GTK_SOURCE_IS_STYLE_SCHEME (scheme));

  view = gtk_source_gutter_renderer_get_view (GTK_SOURCE_GUTTER_RENDERER (self));
  if (view == NULL)
    return;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  context = gtk_widget_get_style_context (GTK_WIDGET (view));
  gtk_style_context_get_color (context, &fg);
  G_GNUC_END_IGNORE_DEPRECATIONS

  if (!get_style_rgba (scheme, "text", BACKGROUND, &self->view.bg))
    {
      if (!lookup_color (context, "view_bg_color", &self->view.bg))
        self->view.bg.alpha = .0;
    }

  if (!get_style_rgba (scheme, "text", FOREGROUND, &self->view.fg))
    {
      if (!lookup_color (context, "view_fg_color", &self->view.fg))
        self->view.fg = fg;
    }

  has_margin_border = get_style_rgba (scheme, "line-numbers-border", BACKGROUND, &margin_bg);

  if (!get_style_rgba (scheme, "selection", FOREGROUND, &self->sel.fg))
    {
      if (!lookup_color (context, "accent_fg_color", &self->sel.fg))
        self->sel.fg = fg;
    }
  else
    {
      had_sel_fg = TRUE;
    }

  if (!get_style_rgba (scheme, "selection", BACKGROUND, &self->sel.bg))
    {
      if (gtk_widget_get_state_flags (GTK_WIDGET (view)) & GTK_STATE_FLAG_FOCUS_WITHIN)
        {
          lookup_color (context, "accent_bg_color", &self->sel.bg);
          /* Make selection like libadwaita would */
          self->sel.bg.alpha *= .3;
        }
      else
        {
          gtk_widget_get_color (GTK_WIDGET (view), &self->sel.bg);
          self->sel.bg.alpha *= .1;
        }
    }
  else if (!had_sel_fg)
    {
      /* gtksourceview will fixup bad selections */
      self->sel.bg.alpha = .3;
    }

  /* Extract common values from style schemes. */
  if (!get_style_rgba (scheme, "line-numbers", FOREGROUND, &self->text.fg))
    self->text.fg = fg;

  if (!get_style_rgba (scheme, "line-numbers", BACKGROUND, &self->text.bg))
    self->text.bg = transparent;

  if (!style_get_is_bold (scheme, "line-numbers", &self->text.bold))
    self->text.bold = FALSE;

  if (!get_style_rgba (scheme, "current-line-number", FOREGROUND, &self->current.fg))
    self->current.fg = self->text.fg;

  if (!get_style_rgba (scheme, "current-line-number", BACKGROUND, &self->current.bg))
    self->current.bg = transparent;

  if (!style_get_is_bold (scheme, "current-line-number", &self->current.bold))
    self->current.bold = TRUE;

  if (!get_style_rgba (scheme, "current-line", BACKGROUND, &self->current_line))
    self->current_line = transparent;

  if (has_margin_border)
    self->margin_bg = self->text.bg;
  else
    self->margin_bg = self->view.bg;

  /* These -0uilder: prefix values come from Builder's style-scheme xml
   * files, but other style schemes may also support them now too.
   */
  if (!get_style_rgba (scheme, "-Builder:added-line", FOREGROUND, &self->changes.add) &&
      !get_style_rgba (scheme, "diff:added-line", FOREGROUND, &self->changes.add))
    gdk_rgba_parse (&self->changes.add, IDE_LINE_CHANGES_FALLBACK_ADDED);

  if (!get_style_rgba (scheme, "-Builder:changed-line", FOREGROUND, &self->changes.change) &&
      !get_style_rgba (scheme, "diff:changed-line", FOREGROUND, &self->changes.change))
    gdk_rgba_parse (&self->changes.change, IDE_LINE_CHANGES_FALLBACK_CHANGED);

  if (!get_style_rgba (scheme, "-Builder:removed-line", FOREGROUND, &self->changes.remove) &&
      !get_style_rgba (scheme, "diff:removed-line", FOREGROUND, &self->changes.remove))
    gdk_rgba_parse (&self->changes.remove, IDE_LINE_CHANGES_FALLBACK_REMOVED);

  /*
   * These -Builder: prefix values come from Builder's style-scheme xml
   * as well as in the IdeBuffer class. Other style schemes may also
   * support them, though.
   */
  if (!get_style_rgba (scheme, "-Builder:current-breakpoint", LINE_BACKGROUND, &self->stopped_bg))
    gdk_rgba_parse (&self->stopped_bg, IDE_LINE_CHANGES_FALLBACK_CHANGED);

  if (!get_style_rgba (scheme, "-Builder:breakpoint", FOREGROUND, &self->bkpt.fg) &&
      !get_style_rgba (scheme, "selection", FOREGROUND, &self->bkpt.fg))
    self->bkpt.fg = fg;

  if (!get_style_rgba (scheme, "-Builder:breakpoint", LINE_BACKGROUND, &self->bkpt.bg) &&
      !get_style_rgba (scheme, "selection", BACKGROUND, &self->bkpt.bg))
    {
      lookup_color (context, "accent_bg_color", &self->bkpt.bg);
      gdk_rgba_parse (&self->bkpt.fg, "#ffffff");
    }

  if (!style_get_is_bold (scheme, "-Builder:breakpoint", &self->bkpt.bold))
    self->bkpt.bold = TRUE;

  /* Slight different color for countpoint, fallback to mix(selection,diff:add) */
  if (!get_style_rgba (scheme, "-Builder:countpoint", FOREGROUND, &self->ctpt.fg))
    get_style_rgba (scheme, "selection", FOREGROUND, &self->ctpt.fg);
  if (!get_style_rgba (scheme, "-Builder:countpoint", BACKGROUND, &self->ctpt.bg))
    {
      if (!get_style_rgba (scheme, "selection", BACKGROUND, &self->ctpt.bg))
        self->ctpt.bg = self->bkpt.bg;

      self->ctpt.bg.red = (self->ctpt.bg.red + self->changes.add.red) / 2.0;
      self->ctpt.bg.green = (self->ctpt.bg.green + self->changes.add.green) / 2.0;
      self->ctpt.bg.blue = (self->ctpt.bg.blue + self->changes.add.blue) / 2.0;
    }
  if (!style_get_is_bold (scheme, "-Builder:countpoint", &self->ctpt.bold))
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

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (state != NULL);
  g_assert (state->begin_line <= state->end_line);

  if (line < state->begin_line ||
      line > state->end_line ||
      line - state->begin_line >= state->lines->len)
    return;

  info = &g_array_index (state->lines, LineInfo, line - state->begin_line);
  info->is_warning |= severity == IDE_DIAGNOSTIC_WARNING
                      || severity == IDE_DIAGNOSTIC_DEPRECATED
                      || severity == IDE_DIAGNOSTIC_UNUSED;
  info->is_error |= severity == IDE_DIAGNOSTIC_ERROR || severity == IDE_DIAGNOSTIC_FATAL;
  info->is_note |= severity == IDE_DIAGNOSTIC_NOTE;
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

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (state != NULL);
  g_assert (state->begin_line <= state->end_line);

  if (line < state->begin_line ||
      line > state->end_line ||
      line - state->begin_line >= state->lines->len)
    return;

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
  if (file == NULL)
    return;

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

  if (self->change_monitor != NULL)
    ide_buffer_change_monitor_foreach_change (self->change_monitor,
                                              state.begin_line,
                                              state.end_line,
                                              populate_changes_cb,
                                              &state);
}

static inline int
count_num_digits (int num_lines)
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

static int
calculate_diagnostics_size (int height)
{
  static guint sizes[] = { 64, 48, 32, 24, 16, 12, 10, 8 };

  for (guint i = 0; i < G_N_ELEMENTS (sizes); i++)
    {
      if (height >= sizes[i])
        return sizes[i];
    }

  return sizes [G_N_ELEMENTS (sizes) - 1];
}

static void
gbp_omni_gutter_renderer_measure (GbpOmniGutterRenderer *self)
{
  g_autofree gchar *numbers = NULL;
  GtkTextBuffer *buffer;
  GtkSourceView *view;
  PangoLayout *layout;
  GtkTextIter end;
  guint line;
  int size = 0;
  int old_width;

  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (self));

  /* There is nothing we can do until a view has been attached. */
  view = gtk_source_gutter_renderer_get_view (GTK_SOURCE_GUTTER_RENDERER (self));
  if (!IDE_IS_SOURCE_VIEW (view))
    return;

  gtk_widget_get_size_request (GTK_WIDGET (self), &old_width, NULL);

  /*
   * First, we need to get the size of the text for the last line of the
   * buffer (which will be the longest). We size the font with '9' since it
   * will generally be one of the widest of the numbers. Although, we only
   * "support" * monospace anyway, so it shouldn't be drastic if we're off.
   */

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
  gtk_text_buffer_get_end_iter (buffer, &end);
  line = gtk_text_iter_get_line (&end) + 1;

  self->n_chars = count_num_digits (line);
  numbers = g_strnfill (self->n_chars, '9');

  /*
   * Get the font description used by the IdeSourceView so we can
   * match the font styling as much as possible.
   */
  layout = gtk_widget_create_pango_layout (GTK_WIDGET (self), numbers);
  pango_layout_set_single_paragraph_mode (layout, TRUE);

  /*
   * Now cache the width of the text layout so we can simplify our
   * positioning later. We simply size everything the same and then
   * align to the right to reduce the draw overhead.
   */
  pango_layout_get_pixel_size (layout, &self->number_width, &self->number_height);
  pango_layout_set_attributes (layout, bold_attrs);

  /*
   * Calculate the nearest size for diagnostics so they scale somewhat
   * reasonable with the character size.
   */
  self->diag_size = calculate_diagnostics_size (MAX (8, self->number_height));
  g_assert (self->diag_size > 0);

  /* Now calculate the size based on enabled features */
  size = self->show_line_diagnostics || self->show_line_numbers || self->show_line_changes ? 2 : 0;

  if (self->show_line_diagnostics)
    size += self->diag_size + 2;
  if (self->show_line_numbers)
    size += self->number_width + 2;

  /* The arrow overlaps the changes if we can have breakpoints,
   * otherwise we just need the space for the line changes.
   */
  if (self->show_line_changes)
    size += CHANGE_WIDTH + 2;

  /* Extra margin */
  size += RIGHT_MARGIN;

  if (size != old_width)
    {
      /* Update the size and ensure we are re-drawn */
      gtk_widget_set_size_request (GTK_WIDGET (self), size, -1);
      gtk_widget_queue_resize (GTK_WIDGET (self));
    }

  g_clear_object (&layout);
}

static void
gbp_omni_gutter_renderer_notify_font (GbpOmniGutterRenderer *self,
                                      GParamSpec            *pspec,
                                      IdeSourceView         *view)
{
  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (self));
  g_assert (IDE_IS_SOURCE_VIEW (view));

  gbp_omni_gutter_renderer_measure (self);
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
                                GtkSourceGutterLines    *lines)
{
  GbpOmniGutterRenderer *self = (GbpOmniGutterRenderer *)renderer;
  GtkTextTagTable *table;
  GtkTextBuffer *buffer;
  IdeSourceView *view;
  GtkTextTag *tag;
  GtkTextIter bkpt;
  GtkTextIter begin, end;
  GtkTextIter sel_begin, sel_end;
  guint end_line;
  int width;
  int left_margin;

  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (renderer));
  g_assert (lines != NULL);

  /*
   * This is the start of our draw process. The first thing we want to
   * do is collect as much information as we'll need when doing the
   * actual draw. That helps us coalesce similar work together, which is
   * good for the CPU usage. We are *very* sensitive to CPU usage here
   * as the GtkTextView does not pixel cache the gutter.
   */

  self->stopped_line = -1;

  buffer = GTK_TEXT_BUFFER (gtk_source_gutter_renderer_get_buffer (GTK_SOURCE_GUTTER_RENDERER (renderer)));
  view = IDE_SOURCE_VIEW (gtk_source_gutter_renderer_get_view (renderer));
  left_margin = gtk_text_view_get_left_margin (GTK_TEXT_VIEW (view));
  width = gtk_widget_get_width (GTK_WIDGET (self));

  self->draw_width = width;
  self->draw_width_with_margin = width + left_margin;
  self->draw_has_focus = gtk_widget_has_focus (GTK_WIDGET (view));
  self->draw_has_selection = gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);
  self->selection_is_multi_line = gtk_text_iter_get_line (&begin) != gtk_text_iter_get_line (&end);

  self->begin_line = gtk_source_gutter_lines_get_first (lines);
  end_line = gtk_source_gutter_lines_get_last (lines);

  /* Locate the current stopped breakpoint if any. */
  gtk_text_buffer_get_iter_at_line (buffer, &begin, self->begin_line);
  gtk_text_buffer_get_iter_at_line (buffer, &end, end_line);
  table = gtk_text_buffer_get_tag_table (buffer);
  tag = gtk_text_tag_table_lookup (table, "-Builder:current-breakpoint");
  if (tag != NULL)
    {
      bkpt = begin;
      gtk_text_iter_backward_char (&bkpt);
      if (gtk_text_iter_forward_to_tag_toggle (&bkpt, tag) &&
          gtk_text_iter_starts_tag (&bkpt, tag))
        self->stopped_line = gtk_text_iter_get_line (&bkpt);
    }

  /* Add quark for line selections which will display all the way to the
   * left margin so that we can draw selection borders (rounded corners
   * which extend under the line numbers).
   */
  if (self->show_line_selection_styling)
    {
      if (gtk_text_buffer_get_selection_bounds (buffer, &sel_begin, &sel_end))
        {
          int first_sel = -1, last_sel = -1;

          gtk_text_iter_order (&sel_begin, &sel_end);

          if (gtk_text_iter_starts_line (&sel_begin))
            first_sel = gtk_text_iter_get_line (&sel_begin);
          else if (gtk_text_iter_get_line (&sel_begin) != gtk_text_iter_get_line (&sel_end))
            first_sel = gtk_text_iter_get_line (&sel_begin) + 1;

          if (!gtk_text_iter_starts_line (&sel_end))
            last_sel = gtk_text_iter_get_line (&sel_end);
          else if (gtk_text_iter_get_line (&sel_begin) != gtk_text_iter_get_line (&sel_end))
            last_sel = gtk_text_iter_get_line (&sel_end) - 1;

          if (first_sel != -1 && last_sel != -1)
            {
              first_sel = MAX (first_sel, gtk_source_gutter_lines_get_first (lines));
              last_sel = MIN (last_sel, gtk_source_gutter_lines_get_last (lines));

              for (int i = first_sel; i <= last_sel; i++)
                gtk_source_gutter_lines_add_qclass (lines, i, selection_quark);
            }
        }
    }


  /*
   * This function is called before we render any of the lines in
   * the gutter. To reduce our overhead, we want to collect information
   * for all of the line numbers upfront.
   */
  ide_source_view_get_visual_position (view, &self->cursor_line, NULL);

  /* Give ourselves a fresh array to stash our line info */
  g_array_set_size (self->lines, end_line - self->begin_line + 1);
  memset (self->lines->data, 0, self->lines->len * sizeof (LineInfo));

  /* Now load breakpoints, diagnostics, and line changes */
  gbp_omni_gutter_renderer_load_basic (self, &begin, self->lines);
  gbp_omni_gutter_renderer_load_breakpoints (self, &begin, &end, self->lines);

  /* Create a new layout for rendering lines to */
  self->layout = gtk_widget_create_pango_layout (GTK_WIDGET (self), "");
  pango_layout_set_alignment (self->layout, PANGO_ALIGN_RIGHT);
  pango_layout_set_width (self->layout, (width - BREAKPOINT_XPAD - RIGHT_MARGIN - 4) * PANGO_SCALE);
}

static gboolean
gbp_omni_gutter_renderer_query_activatable (GtkSourceGutterRenderer *renderer,
                                            GtkTextIter             *begin,
                                            GdkRectangle            *area)
{
  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (renderer));
  g_assert (begin != NULL);
  g_assert (area != NULL);

  /* Clicking will move the cursor, so always TRUE */

  return TRUE;
}

static void
gbp_omni_gutter_renderer_activate (GtkSourceGutterRenderer *renderer,
                                   GtkTextIter             *iter,
                                   GdkRectangle            *area,
                                   guint                    button,
                                   GdkModifierType          state,
                                   int                      n_presses)
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
        _ide_debug_manager_remove_breakpoint (debug_manager, breakpoint);
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
draw_selection_bg (GbpOmniGutterRenderer *self,
                   GtkSnapshot           *snapshot,
                   double                 line_y,
                   double                 width,
                   double                 height,
                   GtkSourceGutterLines  *lines,
                   guint                  line)
{
  if (self->sel.bg.alpha == .0)
    return;

  {
    GskRoundedRect rounded_rect = GSK_ROUNDED_RECT_INIT (2, line_y, width - 2, height);
    gboolean is_first_line = line == 0 || line == gtk_source_gutter_lines_get_first (lines) || !gtk_source_gutter_lines_has_qclass (lines, line - 1, selection_quark);
    gboolean is_last_line = line == gtk_source_gutter_lines_get_last (lines) || !gtk_source_gutter_lines_has_qclass (lines, line + 1, selection_quark);

    if (is_first_line)
      rounded_rect.corner[0] = GRAPHENE_SIZE_INIT (9, 9);

    if (is_last_line)
      rounded_rect.corner[3] = GRAPHENE_SIZE_INIT (9, 9);

    gtk_snapshot_push_rounded_clip (snapshot, &rounded_rect);
    gtk_snapshot_append_color (snapshot,
                               &self->sel.bg,
                               &GRAPHENE_RECT_INIT (2, line_y, width - 2, height));
    gtk_snapshot_pop (snapshot);
  }
}

static void
draw_breakpoint_bg (GbpOmniGutterRenderer *self,
                    GtkSnapshot           *snapshot,
                    int                    line_y,
                    int                    width,
                    int                    height,
                    gboolean               is_prelit,
                    const LineInfo        *info)
{
  GskRoundedRect rounded_rect;
  GdkRGBA rgba;

  if (info->is_countpoint)
    rgba = self->ctpt.bg;
  else
    rgba = self->bkpt.bg;

  if (is_prelit)
    {
      if (IS_BREAKPOINT (info))
        rgba.alpha *= 0.8;
      else
        rgba.alpha *= 0.4;
    }

  rounded_rect = GSK_ROUNDED_RECT_INIT (0, line_y, width - BREAKPOINT_XPAD, height - BREAKPOINT_YPAD);
  rounded_rect.corner[1] = GRAPHENE_SIZE_INIT (BREAKPOINT_CORNER_RADIUS, BREAKPOINT_CORNER_RADIUS);
  rounded_rect.corner[2] = GRAPHENE_SIZE_INIT (BREAKPOINT_CORNER_RADIUS, BREAKPOINT_CORNER_RADIUS);

  gtk_snapshot_push_rounded_clip (snapshot, &rounded_rect);
  gtk_snapshot_append_color (snapshot,
                             &rgba,
                             &GRAPHENE_RECT_INIT (0, line_y, width, height));
  gtk_snapshot_pop (snapshot);
}

static void
draw_line_change (GbpOmniGutterRenderer *self,
                  GtkSnapshot           *snapshot,
                  int                    line_y,
                  int                    width,
                  int                    height,
                  gboolean               is_prelit,
                  const LineInfo        *info)
{
  if (info->is_add || info->is_change)
    {
      gtk_snapshot_append_color (snapshot,
                                 info->is_add ? &self->changes.add : &self->changes.change,
                                 &GRAPHENE_RECT_INIT (width - CHANGE_WIDTH - 1, line_y, CHANGE_WIDTH, height));
    }

  if (info->is_prev_delete)
    {
      gtk_snapshot_append_color (snapshot,
                                 &self->changes.remove,
                                 &GRAPHENE_RECT_INIT (width - DELETE_WIDTH, line_y, DELETE_WIDTH, DELETE_HEIGHT));
    }

  if (info->is_next_delete)
    {
      gtk_snapshot_append_color (snapshot,
                                 &self->changes.remove,
                                 &GRAPHENE_RECT_INIT (width - DELETE_WIDTH, line_y + height - DELETE_HEIGHT, DELETE_WIDTH, DELETE_HEIGHT));
    }
}

static void
draw_diagnostic (GbpOmniGutterRenderer *self,
                 GtkSnapshot           *snapshot,
                 int                    line_y,
                 int                    width,
                 int                    height,
                 gboolean               is_prelit,
                 const LineInfo        *info)
{
  GdkPaintable *paintable = NULL;
  GdkRGBA colors[4];

  if (info->is_error)
    paintable = self->error;
  else if (info->is_warning)
    paintable = self->warning;
  else if (info->is_note)
    paintable = self->note;

  if (IS_BREAKPOINT (info))
    {
      colors[0] = self->sel.fg;
      colors[1] = self->sel.bg;
      colors[2] = self->changes.change;
      colors[3] = self->changes.remove;
    }
  else
    {
      colors[0] = self->text.fg;
      colors[1] = self->text.bg;
      colors[2] = self->changes.change;
      colors[3] = self->changes.remove;
    }

  if (paintable != NULL)
    {
      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot,
                              &GRAPHENE_POINT_INIT (2,
                                                    line_y + ((height - self->diag_size) / 2)));
      gtk_symbolic_paintable_snapshot_symbolic (GTK_SYMBOLIC_PAINTABLE (paintable), snapshot, self->diag_size, self->diag_size,  colors, G_N_ELEMENTS (colors));
      gtk_snapshot_restore (snapshot);
    }
}

static void
gbp_omni_gutter_renderer_snapshot (GtkWidget   *widget,
                                   GtkSnapshot *snapshot)
{
  GbpOmniGutterRenderer *self = (GbpOmniGutterRenderer *)widget;
  int width = gtk_widget_get_width (widget);
  int height = gtk_widget_get_height (widget);


  gtk_snapshot_append_color (snapshot,
                             &self->margin_bg,
                             &GRAPHENE_RECT_INIT (width - RIGHT_MARGIN - CHANGE_WIDTH, 0, RIGHT_MARGIN + CHANGE_WIDTH, height));

  GTK_WIDGET_CLASS (gbp_omni_gutter_renderer_parent_class)->snapshot (widget, snapshot);
}

static void
gbp_omni_gutter_renderer_snapshot_line (GtkSourceGutterRenderer *renderer,
                                        GtkSnapshot             *snapshot,
                                        GtkSourceGutterLines    *lines,
                                        guint                    line)
{
  GbpOmniGutterRenderer *self = (GbpOmniGutterRenderer *)renderer;
  GtkSourceView *view;
  gboolean highlight_line;
  double line_y;
  double line_height;
  int width;

  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (self));
  g_assert (GTK_IS_SNAPSHOT (snapshot));
  g_assert (lines != NULL);

  /*
   * This is our primary draw routine. It is called for every line that
   * is visible. We are incredibly sensitive to performance churn here
   * so it is important that we be as minimal as possible while
   * retaining the features we need.
   */

  view = gtk_source_gutter_renderer_get_view (GTK_SOURCE_GUTTER_RENDERER (self));
  highlight_line = gtk_source_view_get_highlight_current_line (GTK_SOURCE_VIEW (view));

  gtk_source_gutter_lines_get_line_extent (lines, line, GTK_SOURCE_GUTTER_RENDERER_ALIGNMENT_MODE_CELL, &line_y, &line_height);
  width = self->draw_width;

  if ((line - self->begin_line) < self->lines->len)
    {
      LineInfo *info = &g_array_index (self->lines, LineInfo, line - self->begin_line);
      gboolean active = gtk_source_gutter_lines_is_prelit (lines, line);
      gboolean is_cursor = gtk_source_gutter_lines_is_cursor (lines, line);
      gboolean is_selected_line = gtk_source_gutter_lines_has_qclass (lines, line, selection_quark);
      gboolean has_breakpoint = FALSE;
      gboolean bold = FALSE;

      /* Fill in gap for what would look like the "highlight-current-line"
       * within the textarea that we are pretending to look like.
       */
      if (highlight_line &&
          (!self->draw_has_selection || !self->selection_is_multi_line) &&
          is_cursor)
        gtk_snapshot_append_color (snapshot,
                                   &self->current_line,
                                   &GRAPHENE_RECT_INIT (width - RIGHT_MARGIN - CHANGE_WIDTH, line_y,
                                                        RIGHT_MARGIN + CHANGE_WIDTH, line_height));

      /*
       * Draw some background for the line so that it looks like the
       * breakpoint arrow draws over it. Debugger break line takes
       * precidence over the current highlight line. Also, ensure that
       * the view is drawing the highlight line first.
       */
      if (line == self->stopped_line)
        gtk_snapshot_append_color (snapshot,
                                   &self->stopped_bg,
                                   &GRAPHENE_RECT_INIT (0, line_y, width, line_height));
      else if (highlight_line &&
               !self->draw_has_selection &&
               is_cursor)
        gtk_snapshot_append_color (snapshot,
                                   &self->current.bg,
                                   &GRAPHENE_RECT_INIT (0, line_y, width - RIGHT_MARGIN, line_height));

      /* If the selection bg is solid, we need to draw it under the line text
       * and various other line features.
       */
      if (is_selected_line && self->sel.bg.alpha == 1.)
        draw_selection_bg (self, snapshot, line_y, self->draw_width_with_margin, line_height, lines, line);

      /* Draw line changes next so it will show up underneath the
       * breakpoint arrows.
       */
      if (self->show_line_changes && IS_LINE_CHANGE (info))
        draw_line_change (self, snapshot, line_y, width - RIGHT_MARGIN, line_height, active, info);

      /* Draw breakpoint arrows if we have any breakpoints that could
       * potentially match.
       */
      if (self->breakpoints != NULL)
        {
          has_breakpoint = IS_BREAKPOINT (info);
          if (has_breakpoint || active)
            draw_breakpoint_bg (self, snapshot, line_y, width, line_height, active, info);
        }

      /*
       * Now draw the line numbers if we are showing them. Ensure
       * we tweak the style to match closely to how the default
       * gtksourceview lines gutter renderer does it.
       */
      if (self->show_line_numbers)
        {
          const GdkRGBA *rgba;
          const gchar *linestr = NULL;
          guint shown_line;
          int len;

          if (!self->show_relative_line_numbers || line == self->cursor_line)
            shown_line = line + 1;
          else if (line > self->cursor_line)
            shown_line = line - self->cursor_line;
          else
            shown_line = self->cursor_line - line;

          len = int_to_string (shown_line, &linestr);
          pango_layout_set_text (self->layout, linestr, len);


          if (has_breakpoint || (self->breakpoints != NULL && active))
            {
              rgba = &self->bkpt.fg;
              bold = self->bkpt.bold;
            }
          else if (!self->selection_is_multi_line && gtk_source_gutter_lines_is_cursor (lines, line))
            {
              rgba = &self->current.fg;
              bold = self->current.bold;
            }
          else if (gtk_source_gutter_lines_has_qclass (lines, line, selection_quark))
            {
              rgba = &self->view.fg;
              bold = self->text.bold;
            }
          else
            {
              rgba = &self->text.fg;
              bold = self->text.bold;
            }

          pango_layout_set_attributes (self->layout, bold ? bold_attrs : NULL);

          gtk_snapshot_save (snapshot);
          gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (0, line_y + ((line_height - self->number_height) / 2)));
          gtk_snapshot_append_layout (snapshot, self->layout, rgba);
          gtk_snapshot_restore (snapshot);
        }

      /* Draw our selection edges which overlap the gutter. This is drawn last since
       * they will have alpha to draw over the original text and we want it to blend
       * in a similar way to the text within the document.
       */
      if (is_selected_line && self->sel.bg.alpha < 1.)
        draw_selection_bg (self, snapshot, line_y, self->draw_width_with_margin, line_height, lines, line);

      /* Now that we might have an altered background for the line,
       * we can draw the diagnostic icon (with possibly altered
       * color for symbolic icon).
       */
      if (self->show_line_diagnostics && IS_DIAGNOSTIC (info))
        draw_diagnostic (self, snapshot, line_y, width - RIGHT_MARGIN, line_height, active, info);
    }
}

static void
state_flags_changed_cb (GbpOmniGutterRenderer *self,
                        GtkStateFlags          flags,
                        GtkWidget             *view)
{
  GtkStateFlags new_flags = gtk_widget_get_state_flags (view);

  if (((flags ^ new_flags) & GTK_STATE_FLAG_FOCUS_WITHIN) > 0)
    ide_gutter_style_changed (IDE_GUTTER (self));
}

static GdkPaintable *
get_icon_paintable (GbpOmniGutterRenderer *self,
                    GtkWidget             *widget,
                    const gchar           *icon_name,
                    int                    size,
                    gboolean               selected)
{
  GtkIconPaintable *paintable;
  GtkTextDirection direction;
  GtkIconTheme *icon_theme;
  GdkDisplay *display;
  int scale;

  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (self));
  g_assert (GTK_IS_WIDGET (widget));
  g_assert (icon_name != NULL);
  g_assert (size > 0);

  display = gtk_widget_get_display (widget);
  icon_theme = gtk_icon_theme_get_for_display (display);
  scale = gtk_widget_get_scale_factor (widget);
  direction = gtk_widget_get_direction (widget);
  paintable = gtk_icon_theme_lookup_icon (icon_theme,
                                          icon_name,
                                          NULL,
                                          size,
                                          scale,
                                          direction,
                                          GTK_ICON_LOOKUP_PRELOAD);

  return GDK_PAINTABLE (paintable);
}

static void
gbp_omni_gutter_renderer_reload_icons (GbpOmniGutterRenderer *self)
{
  GtkSourceView *view;

  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (self));

  g_clear_object (&self->note);
  g_clear_object (&self->warning);
  g_clear_object (&self->error);

  view = gtk_source_gutter_renderer_get_view (GTK_SOURCE_GUTTER_RENDERER (self));
  if (view == NULL)
    return;

  self->note = get_icon_paintable (self, GTK_WIDGET (view), "dialog-information-symbolic", self->diag_size, FALSE);
  self->warning = get_icon_paintable (self, GTK_WIDGET (view), "dialog-warning-symbolic", self->diag_size, FALSE);
  self->error = get_icon_paintable (self, GTK_WIDGET (view), "builder-build-stop-symbolic", self->diag_size, FALSE);
}

static gboolean
gbp_omni_gutter_renderer_do_reload (GbpOmniGutterRenderer *self)
{
  g_autoptr(IdeDebuggerBreakpoints) breakpoints = NULL;
  IdeBufferChangeMonitor *change_monitor = NULL;
  GtkSourceBuffer *buffer;

  IDE_ENTRY;

  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (self));

  self->reload_source = 0;

  buffer = gtk_source_gutter_renderer_get_buffer (GTK_SOURCE_GUTTER_RENDERER (self));

  if (IDE_IS_BUFFER (buffer))
    {
      g_autoptr(IdeContext) context = NULL;
      IdeDebugManager *debug_manager;
      const gchar *lang_id;

      context = ide_buffer_ref_context (IDE_BUFFER (buffer));
      change_monitor = ide_buffer_get_change_monitor (IDE_BUFFER (buffer));
      lang_id = ide_buffer_get_language_id (IDE_BUFFER (buffer));

      debug_manager = ide_debug_manager_from_context (context);

      if (ide_debug_manager_supports_language (debug_manager, lang_id))
        {
          GFile *file = ide_buffer_get_file (IDE_BUFFER (buffer));

          breakpoints = ide_debug_manager_get_breakpoints_for_file (debug_manager, file);
        }
    }

  gbp_omni_gutter_renderer_set_change_monitor (self, change_monitor);
  gbp_omni_gutter_renderer_set_breakpoints (self, breakpoints);

  /* Reload icons and then recalcuate our physical size */
  gbp_omni_gutter_renderer_measure (self);
  gbp_omni_gutter_renderer_reload_icons (self);

  IDE_RETURN (G_SOURCE_REMOVE);
}

static void
gbp_omni_gutter_renderer_reload (GbpOmniGutterRenderer *self)
{
  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (self));

  /* Ignore if we aren't fully setup or are tearing down */
  if (gtk_source_gutter_renderer_get_view (GTK_SOURCE_GUTTER_RENDERER (self)) == NULL ||
      gtk_source_gutter_renderer_get_buffer (GTK_SOURCE_GUTTER_RENDERER (self)) == NULL)
    return;

  if (self->reload_source == 0)
    self->reload_source = g_idle_add_full (G_PRIORITY_DEFAULT,
                                           (GSourceFunc) gbp_omni_gutter_renderer_do_reload,
                                           self,
                                           NULL);
}

static gboolean
gbp_omni_gutter_renderer_do_measure_in_idle (gpointer data)
{
  GbpOmniGutterRenderer *self = data;

  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (self));

  self->resize_source = 0;

  gbp_omni_gutter_renderer_measure (self);

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
    self->resize_source = g_idle_add_full (G_PRIORITY_HIGH,
                                           gbp_omni_gutter_renderer_do_measure_in_idle,
                                           g_object_ref (self),
                                           g_object_unref);
}

static void
gbp_omni_gutter_renderer_cursor_moved (GbpOmniGutterRenderer *self,
                                       GtkTextBuffer         *buffer)
{
  GtkTextIter iter;
  GtkTextMark *insert;
  guint line;

  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (self));
  g_assert (IDE_IS_BUFFER (buffer));

  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);
  line = gtk_text_iter_get_line (&iter);

  if (line != self->last_cursor_line ||
      self->show_relative_line_numbers ||
      gtk_text_buffer_get_has_selection (buffer))
    gtk_widget_queue_draw (GTK_WIDGET (self));

  self->last_cursor_line = line;
}

static void
gbp_omni_gutter_renderer_change_buffer (GtkSourceGutterRenderer *renderer,
                                        GtkSourceBuffer         *old_buffer)
{
  GbpOmniGutterRenderer *self = (GbpOmniGutterRenderer *)renderer;
  GtkSourceBuffer *buffer;

  IDE_ENTRY;

  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (self));
  g_assert (!old_buffer || GTK_SOURCE_IS_BUFFER (old_buffer));

  buffer = gtk_source_gutter_renderer_get_buffer (GTK_SOURCE_GUTTER_RENDERER (self));
  g_signal_group_set_target (self->buffer_signals, buffer);

  gbp_omni_gutter_renderer_reload (self);

  IDE_EXIT;
}

static void
gbp_omni_gutter_renderer_change_view (GtkSourceGutterRenderer *renderer,
                                      GtkSourceView           *old_view)
{
  GbpOmniGutterRenderer *self = (GbpOmniGutterRenderer *)renderer;
  GtkSourceView *view;

  IDE_ENTRY;

  g_assert (GBP_IS_OMNI_GUTTER_RENDERER (self));
  g_assert (!old_view || GTK_SOURCE_IS_VIEW (old_view));

  GTK_SOURCE_GUTTER_RENDERER_CLASS (gbp_omni_gutter_renderer_parent_class)->change_view (renderer, old_view);

  view = gtk_source_gutter_renderer_get_view (GTK_SOURCE_GUTTER_RENDERER (self));
  g_signal_group_set_target (self->view_signals, view);

  gbp_omni_gutter_renderer_reload (self);

  IDE_EXIT;
}

static void
gbp_omni_gutter_renderer_dispose (GObject *object)
{
  GbpOmniGutterRenderer *self = (GbpOmniGutterRenderer *)object;

  g_clear_handle_id (&self->resize_source, g_source_remove);
  g_clear_handle_id (&self->reload_source, g_source_remove);

  gbp_omni_gutter_renderer_set_change_monitor (self, NULL);
  gbp_omni_gutter_renderer_set_breakpoints (self, NULL);

  g_clear_pointer (&self->lines, g_array_unref);

  g_clear_object (&self->view_signals);
  g_clear_object (&self->buffer_signals);

  g_clear_object (&self->note);
  g_clear_object (&self->warning);
  g_clear_object (&self->error);

  g_clear_object (&self->layout);

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

    case PROP_SHOW_LINE_SELECTION_STYLING:
      g_value_set_boolean (value, self->show_line_selection_styling);
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

    case PROP_SHOW_LINE_SELECTION_STYLING:
      gbp_omni_gutter_renderer_set_show_line_selection_styling (self, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_omni_gutter_renderer_css_changed (GtkWidget         *widget,
                                      GtkCssStyleChange *change)
{
  GTK_WIDGET_CLASS (gbp_omni_gutter_renderer_parent_class)->css_changed (widget, change);

  ide_gutter_style_changed (IDE_GUTTER (widget));
}

static void
gbp_omni_gutter_renderer_class_init (GbpOmniGutterRendererClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkSourceGutterRendererClass *renderer_class = GTK_SOURCE_GUTTER_RENDERER_CLASS (klass);

  object_class->dispose = gbp_omni_gutter_renderer_dispose;
  object_class->get_property = gbp_omni_gutter_renderer_get_property;
  object_class->set_property = gbp_omni_gutter_renderer_set_property;

  widget_class->snapshot = gbp_omni_gutter_renderer_snapshot;
  widget_class->css_changed = gbp_omni_gutter_renderer_css_changed;

  renderer_class->snapshot_line = gbp_omni_gutter_renderer_snapshot_line;
  renderer_class->begin = gbp_omni_gutter_renderer_begin;
  renderer_class->end = gbp_omni_gutter_renderer_end;
  renderer_class->query_activatable = gbp_omni_gutter_renderer_query_activatable;
  renderer_class->activate = gbp_omni_gutter_renderer_activate;
  renderer_class->change_buffer = gbp_omni_gutter_renderer_change_buffer;
  renderer_class->change_view = gbp_omni_gutter_renderer_change_view;
  renderer_class->query_data = NULL; /* opt out */

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

  properties [PROP_SHOW_LINE_SELECTION_STYLING] =
    g_param_spec_boolean ("show-line-selection-styling", NULL, NULL, TRUE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  bold_attrs = pango_attr_list_new ();
  pango_attr_list_insert (bold_attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));

  selection_quark = g_quark_from_static_string ("omni-selection");
}

static void
gbp_omni_gutter_renderer_init (GbpOmniGutterRenderer *self)
{
  self->diag_size = 16;
  self->show_line_changes = TRUE;
  self->show_line_diagnostics = TRUE;
  self->show_line_diagnostics = TRUE;

  self->lines = g_array_new (FALSE, FALSE, sizeof (LineInfo));

  self->buffer_signals = g_signal_group_new (IDE_TYPE_BUFFER);
  g_signal_group_connect_swapped (self->buffer_signals,
                                    "notify::file",
                                    G_CALLBACK (gbp_omni_gutter_renderer_reload),
                                    self);
  g_signal_group_connect_swapped (self->buffer_signals,
                                    "notify::language",
                                    G_CALLBACK (gbp_omni_gutter_renderer_reload),
                                    self);
  g_signal_group_connect_swapped (self->buffer_signals,
                                    "notify::change-monitor",
                                    G_CALLBACK (gbp_omni_gutter_renderer_reload),
                                    self);
  g_signal_group_connect_object (self->buffer_signals,
                                   "notify::diagnostics",
                                   G_CALLBACK (gtk_widget_queue_draw),
                                   self,
                                   G_CONNECT_SWAPPED);
  g_signal_group_connect_object (self->buffer_signals,
                                   "notify::has-selection",
                                   G_CALLBACK (gtk_widget_queue_draw),
                                   self,
                                   G_CONNECT_SWAPPED);
  g_signal_group_connect_swapped (self->buffer_signals,
                                    "changed",
                                    G_CALLBACK (gbp_omni_gutter_renderer_buffer_changed),
                                    self);
  g_signal_group_connect_swapped (self->buffer_signals,
                                    "cursor-moved",
                                    G_CALLBACK (gbp_omni_gutter_renderer_cursor_moved),
                                    self);

  self->view_signals = g_signal_group_new (IDE_TYPE_SOURCE_VIEW);
  g_signal_group_connect_swapped (self->view_signals,
                                    "notify::font-desc",
                                    G_CALLBACK (gbp_omni_gutter_renderer_notify_font),
                                    self);
  g_signal_group_connect_swapped (self->view_signals,
                                    "notify::font-scale",
                                    G_CALLBACK (gbp_omni_gutter_renderer_notify_font),
                                    self);
  g_signal_group_connect_swapped (self->view_signals,
                                    "notify::highlight-current-line",
                                    G_CALLBACK (gtk_widget_queue_draw),
                                    self);
  g_signal_group_connect_swapped (self->view_signals,
                                  "state-flags-changed",
                                  G_CALLBACK (state_flags_changed_cb),
                                  self);
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

gboolean
gbp_omni_gutter_renderer_get_show_line_selection_styling (GbpOmniGutterRenderer *self)
{
  g_return_val_if_fail (GBP_IS_OMNI_GUTTER_RENDERER (self), FALSE);

  return self->show_line_selection_styling;
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
      gbp_omni_gutter_renderer_measure (self);
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
      gbp_omni_gutter_renderer_measure (self);
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
      gbp_omni_gutter_renderer_measure (self);
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
      gtk_widget_queue_draw (GTK_WIDGET (self));
    }
}

void
gbp_omni_gutter_renderer_set_show_line_selection_styling (GbpOmniGutterRenderer *self,
                                                          gboolean               show_line_selection_styling)
{
  g_return_if_fail (GBP_IS_OMNI_GUTTER_RENDERER (self));

  show_line_selection_styling = !!show_line_selection_styling;

  if (show_line_selection_styling != self->show_line_selection_styling)
    {
      self->show_line_selection_styling = show_line_selection_styling;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SHOW_LINE_SELECTION_STYLING]);
      gtk_widget_queue_draw (GTK_WIDGET (self));
    }
}

static void
gbp_omni_gutter_renderer_style_changed (IdeGutter *gutter)
{
  GbpOmniGutterRenderer *self = (GbpOmniGutterRenderer *)gutter;
  GtkSourceStyleScheme *scheme;
  GtkSourceBuffer *buffer;

  g_return_if_fail (GBP_IS_OMNI_GUTTER_RENDERER (self));

  buffer = gtk_source_gutter_renderer_get_buffer (GTK_SOURCE_GUTTER_RENDERER (self));
  scheme = gtk_source_buffer_get_style_scheme (buffer);

  reload_style_colors (self, scheme);
  gbp_omni_gutter_renderer_measure (self);
  gbp_omni_gutter_renderer_reload_icons (self);
}

static void
gutter_iface_init (IdeGutterInterface *iface)
{
  iface->style_changed = gbp_omni_gutter_renderer_style_changed;
}
