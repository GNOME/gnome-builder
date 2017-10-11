/* ide-build-log-panel.c
 *
 * Copyright © 2015 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-build-log-panel"

#include <dazzle.h>
#include <glib/gi18n.h>
#include <ide.h>

#include "buildui/ide-build-log-panel.h"

typedef struct _ColorCodeState
{
  /* A value of -1 is used to specify a default foreground orr background */
  gint16 foreground;
  gint16 background;

  guint  bold       : 1;
  guint  dim        : 1;
  guint  underlined : 1;
  guint  reverse    : 1;
  guint  hidden     : 1;
} ColorCodeState;

struct _IdeBuildLogPanel
{
  DzlDockWidget      parent_instance;

  IdeBuildPipeline  *pipeline;
  GtkCssProvider    *css;
  GSettings         *settings;
  GtkTextBuffer     *buffer;

  GtkScrolledWindow *scroller;
  GtkTextView       *text_view;

  GtkTextTag        *stderr_tag;
  GPtrArray         *color_codes_foreground_tags;
  GPtrArray         *color_codes_background_tags;
  GtkTextTag        *color_codes_bold_tag;
  GtkTextTag        *color_codes_underlined_tag;
  ColorCodeState     color_codes_state;
  ColorCodeState     current_color_codes_state;

  guint              log_observer;
};

enum {
  PROP_0,
  PROP_PIPELINE,
  LAST_PROP
};

G_DEFINE_TYPE (IdeBuildLogPanel, ide_build_log_panel, DZL_TYPE_DOCK_WIDGET)

static GParamSpec *properties [LAST_PROP];

/* TODO: Same hard-coded palette as terminal-view
 * till we have code for custom palettes
 */
#define COLOR_PALETTE_NB_COLORS 16

static const GdkRGBA solarized_palette[] =
{
  /*
   * Solarized palette (1.0.0beta2):
   * http://ethanschoonover.com/solarized
   */
  { 0.02745,  0.211764, 0.258823, 1 },
  { 0.862745, 0.196078, 0.184313, 1 },
  { 0.521568, 0.6,      0,        1 },
  { 0.709803, 0.537254, 0,        1 },
  { 0.149019, 0.545098, 0.823529, 1 },
  { 0.82745,  0.211764, 0.509803, 1 },
  { 0.164705, 0.631372, 0.596078, 1 },
  { 0.933333, 0.909803, 0.835294, 1 },
  { 0,        0.168627, 0.211764, 1 },
  { 0.796078, 0.294117, 0.086274, 1 },
  { 0.345098, 0.431372, 0.458823, 1 },
  { 0.396078, 0.482352, 0.513725, 1 },
  { 0.513725, 0.580392, 0.588235, 1 },
  { 0.423529, 0.443137, 0.768627, 1 },
  { 0.57647,  0.631372, 0.631372, 1 },
  { 0.992156, 0.964705, 0.890196, 1 },
};

typedef enum
{
  COLOR_CODE_NONE,
  COLOR_CODE_TAG,
  COLOR_CODE_INVALID,
  COLOR_CODE_SKIP,
} ColorCodeType;

static void
init_color_tags_from_palette (IdeBuildLogPanel *self)
{
  GtkTextTag *tag;
  GdkRGBA rgba;

  g_assert (IDE_IS_BUILD_LOG_PANEL (self));
  g_assert (self->buffer != NULL);

  g_clear_pointer (&self->color_codes_foreground_tags, g_ptr_array_unref);
  g_clear_pointer (&self->color_codes_background_tags, g_ptr_array_unref);
  g_clear_object (&self->color_codes_bold_tag);
  g_clear_object (&self->color_codes_underlined_tag);

  self->color_codes_foreground_tags = g_ptr_array_new ();
  for (gint i = 0; i < COLOR_PALETTE_NB_COLORS; ++i)
    {
      rgba = solarized_palette [i];
      tag = gtk_text_buffer_create_tag (self->buffer, NULL, "foreground-rgba", &rgba, NULL);
      g_ptr_array_add (self->color_codes_foreground_tags, tag);
    }

  self->color_codes_background_tags = g_ptr_array_new ();
  for (gint i = 0; i < COLOR_PALETTE_NB_COLORS; ++i)
    {
      rgba = solarized_palette [i];
      tag = gtk_text_buffer_create_tag (self->buffer, NULL, "background-rgba", &rgba, NULL);
      g_ptr_array_add (self->color_codes_background_tags, tag);
    }

  self->color_codes_bold_tag =
    g_object_ref (gtk_text_buffer_create_tag (self->buffer, NULL,
                                              "weight", PANGO_WEIGHT_BOLD,
                                              NULL));
  self->color_codes_underlined_tag =
    g_object_ref (gtk_text_buffer_create_tag (self->buffer, NULL,
                                              "underline", PANGO_UNDERLINE_SINGLE,
                                              NULL));
}

static inline gboolean
is_foreground_color_value (gint value)
{
  return ((value >= 30 && value <= 37) || (value >= 90 && value <= 97));
}

static inline gboolean
is_background_color_value (gint value)
{
  return ((value >= 40 && value <= 47) || (value >= 100 && value <= 107));
}

static inline gboolean
is_format_color_value (gint value)
{
  return (value == 1 || value == 2 || value == 4 || value == 5 || value == 7 || value == 8);
}

static inline gboolean
is_reset_format_color_value (gint value)
{
  return (value == 21 || value == 22 || value == 24 || value == 25 || value == 27 || value == 28);
}

static inline gboolean
is_reset_all_color_value (gint value)
{
  return (value == 0);
}

/* Return -1 if not valid.
 * Cursor is updated in every cases.
 */
static gint
str_to_int (const gchar **cursor_ptr)
{
  gint value = 0;

  g_assert (cursor_ptr != NULL && *cursor_ptr != NULL);

  if (**cursor_ptr == 'm')
    return 0;

  while (**cursor_ptr >= '0' && **cursor_ptr <= '9')
    {
      value *= 10;
      value += **cursor_ptr - '0';

      ++(*cursor_ptr);
    }

  if (is_foreground_color_value (value) ||
      is_background_color_value (value) ||
      is_format_color_value (value) ||
      is_reset_format_color_value (value) ||
      value == 0 || value == 39 || value == 49)
    return value;
  else
    return -1;
}

static gint
color_code_value_to_tag_index (gint value)
{
  if (value >=30 && value <= 37)
    return value - 30;

  if (value >=90 && value <= 97)
    return value - 82;

  return -1;
}

static void
color_codes_state_reset (ColorCodeState *color_codes_state)
{
  g_assert (color_codes_state != NULL);

  color_codes_state->foreground = -1;
  color_codes_state->background = -1;

  color_codes_state->bold = FALSE;
  color_codes_state->dim = FALSE;
  color_codes_state->reverse = FALSE;
  color_codes_state->underlined = FALSE;
  color_codes_state->hidden = FALSE;
}

static void
color_codes_state_update (IdeBuildLogPanel *self,
                          ColorCodeState   *color_codes_state,
                          gint              value)
{
  g_assert (IDE_IS_BUILD_LOG_PANEL (self));
  g_assert (color_codes_state != NULL);

  if (value == 0)
    color_codes_state_reset (color_codes_state);
  else if (value == 39)
    color_codes_state->foreground = -1;
  else if (value == 49)
    color_codes_state->background = -1;
  else if (is_foreground_color_value (value))
    color_codes_state->foreground = value;
  else if (is_background_color_value (value))
    color_codes_state->background = value;
  else if (is_format_color_value (value))
    {
      if (value == 1)
        color_codes_state->bold = TRUE;
      else if (value == 4)
        color_codes_state->underlined = TRUE;
    }
  else if (is_reset_format_color_value (value))
    {
      if (value == 21)
        color_codes_state->bold = FALSE;
      else if (value == 24)
        color_codes_state->underlined = FALSE;
    }
}

static void
color_codes_state_apply (IdeBuildLogPanel *self,
                         ColorCodeState   *color_codes_state,
                         GtkTextIter      *begin,
                         GtkTextIter      *end)
{
  GtkTextTag *tag;
  gint tag_index;

  g_assert (IDE_IS_BUILD_LOG_PANEL (self));
  g_assert (color_codes_state != NULL);
  g_assert (begin != NULL);
  g_assert (end != NULL);

  if (color_codes_state->foreground != -1)
    {
      tag_index = color_code_value_to_tag_index (color_codes_state->foreground);
      g_assert (tag_index != -1);

      tag = g_ptr_array_index (self->color_codes_foreground_tags, tag_index);
      gtk_text_buffer_apply_tag (self->buffer, tag, begin, end);
    }

  if (color_codes_state->background != -1)
    {
      tag_index = color_code_value_to_tag_index (color_codes_state->background);
      g_assert (tag_index != -1);

      tag = g_ptr_array_index (self->color_codes_background_tags, tag_index);
      gtk_text_buffer_apply_tag (self->buffer, tag, begin, end);
    }

  if (color_codes_state->bold ==  TRUE)
    gtk_text_buffer_apply_tag (self->buffer, self->color_codes_bold_tag, begin, end);

  if (color_codes_state->underlined ==  TRUE)
    gtk_text_buffer_apply_tag (self->buffer, self->color_codes_underlined_tag, begin, end);
}

static ColorCodeType
fetch_color_codes_tags (IdeBuildLogPanel  *self,
                        const gchar      **cursor,
                        ColorCodeState    *color_codes_state)
{
  gint value;
  ColorCodeType ret = COLOR_CODE_NONE;
  ColorCodeState tmp_color_codes_state = *color_codes_state;

  g_assert (IDE_IS_BUILD_LOG_PANEL (self));
  g_assert (cursor != NULL && *cursor != NULL);
  g_assert (color_codes_state  != NULL);

  while (**cursor != '\0')
    {
      value = str_to_int (cursor);
      if (value != -1)
        {
          if (is_foreground_color_value (value) ||
              is_background_color_value (value) ||
              is_format_color_value (value) ||
              is_reset_format_color_value (value) ||
              is_reset_all_color_value (value))
            {
              color_codes_state_update (self, &tmp_color_codes_state, value);
              ret = COLOR_CODE_TAG;
            }
        }
      else if (ret == COLOR_CODE_NONE)
        ret = COLOR_CODE_INVALID;

      if (**cursor == 'm')
      {
        if (ret != COLOR_CODE_INVALID)
          *color_codes_state = tmp_color_codes_state;

        ++(*cursor);
        return ret;
      }

      if (**cursor != ';')
        break;

      ++(*cursor);
    }

  return COLOR_CODE_INVALID;
}

/**
 * find_color_code:
 * @self: a #GbpBuildLogPanel
 * @msg: text to search in
 * @color_codes_state: (inout) : if a color code is found, the state is updated
 * @start: (out) point to the first char of a found code
 * @end: (out) point to the last char + 1 of a found code
 *
 * If no color code is found, start and end point to the string's end.
 *
 * Returns: a #ColorCodeType indicating the state of the search.
 */

static ColorCodeType
find_color_code (IdeBuildLogPanel  *self,
                 const gchar       *msg,
                 ColorCodeState    *color_codes_state,
                 const gchar      **start,
                 const gchar      **end)
{
  const gchar *cursor = msg;
  ColorCodeType ret;

  g_assert (IDE_IS_BUILD_LOG_PANEL (self));
  g_assert (!ide_str_empty0 (msg));
  g_assert (color_codes_state != NULL);
  g_assert (start != NULL);
  g_assert (end != NULL);

  while (*cursor != '\0')
    {
      if (*cursor == '\\' && *(cursor + 1) == 'e')
        {
          *start = cursor;
          cursor += 2;
        }
      else if (*cursor == '\033')
        {
          *start = cursor;
          ++cursor;
        }
      else
        goto next;

      if (*cursor == '[')
        {
          ++cursor;
          if (*cursor == '\0')
            goto end;

          if (*cursor == 'K')
            {
              *end = cursor + 1;
              return COLOR_CODE_SKIP;
            }

          ret = fetch_color_codes_tags (self, &cursor, color_codes_state);
          *end = cursor;

          return ret;
        }

      if (*cursor == '\0')
        goto end;

next:
      /* TODO: skip a possible escaped char */
      cursor = g_utf8_next_char (cursor);
    }

end:
  *start = *end = cursor;
  return COLOR_CODE_NONE;
}

/* Transform VT color codes into tags before inserting the text */
static void
ide_build_log_panel_insert_text (IdeBuildLogPanel  *self,
                                 const gchar       *message,
                                 GtkTextIter       *iter,
                                 IdeBuildLogStream  stream)
{
  ColorCodeType tag_type;
  ColorCodeType current_tag_type = COLOR_CODE_NONE;
  GtkTextIter pos = *iter;
  const gchar *cursor = message;
  const gchar *tag_start;
  const gchar *tag_end;
  gsize len;

  g_assert (IDE_IS_BUILD_LOG_PANEL (self));
  g_assert (iter != NULL);

  if (ide_str_empty0 (message))
    return;

  while (*cursor != '\0')
    {
      tag_type = find_color_code (self, cursor, &self->color_codes_state, &tag_start, &tag_end);
      len = tag_start - cursor;
      if (len > 0)
        {
          GtkTextIter begin;
          guint offset;

          offset = gtk_text_iter_get_offset (&pos);
          gtk_text_buffer_insert (self->buffer, &pos, cursor, len);
          gtk_text_buffer_get_iter_at_offset (self->buffer, &begin, offset);

          if (current_tag_type == COLOR_CODE_TAG || current_tag_type == COLOR_CODE_SKIP)
            color_codes_state_apply (self, &self->current_color_codes_state, &begin, &pos);

          if (G_LIKELY (stream != IDE_BUILD_LOG_STDOUT))
            gtk_text_buffer_apply_tag (self->buffer, self->stderr_tag, &begin, &pos);
        }

      current_tag_type = tag_type;
      self->current_color_codes_state = self->color_codes_state;

      if (tag_type == COLOR_CODE_NONE)
        break;

      cursor = tag_end;
    }

  gtk_text_buffer_insert (self->buffer, &pos, "\n", 1);
}

static void
ide_build_log_panel_reset_view (IdeBuildLogPanel *self)
{
  GtkStyleContext *context;

  g_assert (IDE_IS_BUILD_LOG_PANEL (self));

  g_clear_object (&self->buffer);

  if (self->text_view != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->text_view));

  self->buffer = gtk_text_buffer_new (NULL);
  self->stderr_tag = gtk_text_buffer_create_tag (self->buffer,
                                                 "stderr-tag",
                                                 "foreground", "#ff0000",
                                                 "weight", PANGO_WEIGHT_NORMAL,
                                                 NULL);

  /* We set VT color codes tags after stderr_tag so that they have higher priority */
  init_color_tags_from_palette (self);
  color_codes_state_reset (&self->current_color_codes_state);
  color_codes_state_reset (&self->color_codes_state);

  self->text_view = g_object_new (GTK_TYPE_TEXT_VIEW,
                                  "bottom-margin", 3,
                                  "buffer", self->buffer,
                                  "cursor-visible", FALSE,
                                  "editable", FALSE,
                                  "left-margin", 3,
                                  "monospace", TRUE,
                                  "right-margin", 3,
                                  "top-margin", 3,
                                  "visible", TRUE,
                                  NULL);
  context = gtk_widget_get_style_context (GTK_WIDGET (self->text_view));
  gtk_style_context_add_provider (context,
                                  GTK_STYLE_PROVIDER (self->css),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  gtk_container_add (GTK_CONTAINER (self->scroller), GTK_WIDGET (self->text_view));
}

static void
ide_build_log_panel_log_observer (IdeBuildLogStream  stream,
                                  const gchar       *message,
                                  gssize             message_len,
                                  gpointer           user_data)
{
  IdeBuildLogPanel *self = user_data;
  GtkTextMark *insert;
  GtkTextIter iter, enditer;

  g_assert (IDE_IS_BUILD_LOG_PANEL (self));
  g_assert (message != NULL);
  g_assert (message_len >= 0);
  g_assert (message[message_len] == '\0');

  gtk_text_buffer_get_end_iter (self->buffer, &iter);
  ide_build_log_panel_insert_text (self, message, &iter, stream);

  insert = gtk_text_buffer_get_insert (self->buffer);
  gtk_text_view_scroll_to_mark (self->text_view, insert, 0.0, TRUE, 1.0, 0.0);

  gtk_text_buffer_get_end_iter (self->buffer, &enditer);
  gtk_text_buffer_place_cursor (self->buffer, &enditer);
}

void
ide_build_log_panel_set_pipeline (IdeBuildLogPanel *self,
                                  IdeBuildPipeline *pipeline)
{
  g_return_if_fail (IDE_IS_BUILD_LOG_PANEL (self));
  g_return_if_fail (!pipeline || IDE_IS_BUILD_PIPELINE (pipeline));

  if (pipeline != self->pipeline)
    {
      if (self->pipeline != NULL)
        {
          ide_build_pipeline_remove_log_observer (self->pipeline, self->log_observer);
          self->log_observer = 0;
          g_clear_object (&self->pipeline);
        }

      if (pipeline != NULL)
        {
          self->pipeline = g_object_ref (pipeline);
          self->log_observer =
            ide_build_pipeline_add_log_observer (self->pipeline,
                                                 ide_build_log_panel_log_observer,
                                                 self,
                                                 NULL);
        }
    }
}

static void
ide_build_log_panel_changed_font_name (IdeBuildLogPanel *self,
                                       const gchar      *key,
                                       GSettings        *settings)
{
  gchar *font_name;
  PangoFontDescription *font_desc;

  g_assert (IDE_IS_BUILD_LOG_PANEL (self));
  g_assert (g_strcmp0 (key, "font-name") == 0);
  g_assert (G_IS_SETTINGS (settings));

  font_name = g_settings_get_string (settings, key);
  font_desc = pango_font_description_from_string (font_name);

  if (font_desc != NULL)
    {
      gchar *fragment;
      gchar *css;

      fragment = dzl_pango_font_description_to_css (font_desc);
      css = g_strdup_printf ("textview { %s }", fragment);

      gtk_css_provider_load_from_data (self->css, css, -1, NULL);

      pango_font_description_free (font_desc);
      g_free (fragment);
      g_free (css);
    }

  g_free (font_name);
}

static void
ide_build_log_panel_finalize (GObject *object)
{
  IdeBuildLogPanel *self = (IdeBuildLogPanel *)object;

  self->stderr_tag = NULL;

  g_clear_object (&self->pipeline);
  g_clear_object (&self->css);
  g_clear_object (&self->settings);

  g_clear_pointer (&self->color_codes_foreground_tags, g_ptr_array_unref);
  g_clear_pointer (&self->color_codes_background_tags, g_ptr_array_unref);
  g_clear_object (&self->color_codes_bold_tag);
  g_clear_object (&self->color_codes_underlined_tag);

  G_OBJECT_CLASS (ide_build_log_panel_parent_class)->finalize (object);
}

static void
ide_build_log_panel_dispose (GObject *object)
{
  IdeBuildLogPanel *self = (IdeBuildLogPanel *)object;

  ide_build_log_panel_set_pipeline (self, NULL);

  G_OBJECT_CLASS (ide_build_log_panel_parent_class)->dispose (object);
}

static void
ide_build_log_panel_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  IdeBuildLogPanel *self = IDE_BUILD_LOG_PANEL (object);

  switch (prop_id)
    {
    case PROP_PIPELINE:
      g_value_set_object (value, self->pipeline);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_build_log_panel_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  IdeBuildLogPanel *self = IDE_BUILD_LOG_PANEL (object);

  switch (prop_id)
    {
    case PROP_PIPELINE:
      ide_build_log_panel_set_pipeline (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_build_log_panel_class_init (IdeBuildLogPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ide_build_log_panel_dispose;
  object_class->finalize = ide_build_log_panel_finalize;
  object_class->get_property = ide_build_log_panel_get_property;
  object_class->set_property = ide_build_log_panel_set_property;

  gtk_widget_class_set_css_name (widget_class, "buildlogpanel");
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/buildui/ide-build-log-panel.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeBuildLogPanel, scroller);

  properties [PROP_PIPELINE] =
    g_param_spec_object ("pipeline",
                         "Result",
                         "Result",
                         IDE_TYPE_BUILD_PIPELINE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
ide_build_log_panel_clear_activate (GSimpleAction *action,
                                    GVariant      *param,
                                    gpointer       user_data)
{
  IdeBuildLogPanel *self = user_data;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_BUILD_LOG_PANEL (self));

  gtk_text_buffer_set_text (self->buffer, "", 0);
}

static void
ide_build_log_panel_save_in_file (GSimpleAction *action,
                                  GVariant      *param,
                                  gpointer       user_data)
{
  IdeBuildLogPanel *self = user_data;
  g_autoptr(GtkFileChooserNative) native = NULL;
  GtkWidget *window;
  gint res;

  IDE_ENTRY;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_BUILD_LOG_PANEL (self));

  window = gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_WINDOW);
  native = gtk_file_chooser_native_new (_("Save File"),
                                        GTK_WINDOW (window),
                                        GTK_FILE_CHOOSER_ACTION_SAVE,
                                        _("_Save"),
                                        _("_Cancel"));

  res = gtk_native_dialog_run (GTK_NATIVE_DIALOG (native));

  if (res == GTK_RESPONSE_ACCEPT)
    {
      g_autofree gchar *filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (native));
      g_autofree gchar *text = NULL;
      GtkTextIter begin;
      GtkTextIter end;

      gtk_text_buffer_get_bounds (self->buffer, &begin, &end);
      text = gtk_text_buffer_get_text (self->buffer, &begin, &end, FALSE);
      g_file_set_contents (filename, text, -1, NULL);
    }

  IDE_EXIT;
}

static void
ide_build_log_panel_init (IdeBuildLogPanel *self)
{
  static GActionEntry entries[] = {
    { "clear", ide_build_log_panel_clear_activate },
    { "save", ide_build_log_panel_save_in_file },
  };
  g_autoptr(GSimpleActionGroup) actions = NULL;

  self->css = gtk_css_provider_new ();

  gtk_widget_init_template (GTK_WIDGET (self));

  g_object_set (self, "title", _("Build Output"), NULL);

  ide_build_log_panel_reset_view (self);

  self->settings = g_settings_new ("org.gnome.builder.terminal");
  g_signal_connect_object (self->settings,
                           "changed::font-name",
                           G_CALLBACK (ide_build_log_panel_changed_font_name),
                           self,
                           G_CONNECT_SWAPPED);
  ide_build_log_panel_changed_font_name (self, "font-name", self->settings);

  actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (actions), entries, G_N_ELEMENTS (entries), self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "build-log", G_ACTION_GROUP (actions));
}
