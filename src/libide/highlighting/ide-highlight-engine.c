/* ide-highlight-engine.c
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-highlight-engine"

#include <dazzle.h>
#include <glib/gi18n.h>
#include <string.h>

#include "ide-context.h"
#include "ide-debug.h"
#include "ide-types.h"

#include "highlighting/ide-highlight-engine.h"
#include "plugins/ide-extension-adapter.h"

#define HIGHLIGHT_QUANTA_USEC 5000
#define PRIVATE_TAG_PREFIX    "gb-private-tag"

struct _IdeHighlightEngine
{
  IdeObject            parent_instance;

  DzlSignalGroup      *signal_group;
  IdeBuffer           *buffer;
  IdeHighlighter      *highlighter;
  GSettings           *settings;

  IdeExtensionAdapter *extension;

  GtkTextMark         *invalid_begin;
  GtkTextMark         *invalid_end;

  GSList              *private_tags;
  GSList              *public_tags;

  gint64               quanta_expiration;

  guint                work_timeout;

  guint                enabled : 1;
};

G_DEFINE_TYPE (IdeHighlightEngine, ide_highlight_engine, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_BUFFER,
  PROP_HIGHLIGHTER,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];
static GQuark      engineQuark;

static gboolean
get_invalidation_area (GtkTextIter *begin,
                       GtkTextIter *end)
{
  GtkTextIter begin_tmp;
  GtkTextIter end_tmp;

  g_assert (begin != NULL);
  g_assert (end != NULL);

  /*
   * Move to the beginning of line.We dont use gtk_text_iter_backward_line
   * because if begin is at the beginning of the line we dont want to
   * move to the previous line
   */
  gtk_text_iter_set_line_offset (begin, 0);

  /* Move to the beginning of the next line. */
  gtk_text_iter_forward_line (end);

  /* Save the original locations. We will need them down the line. */
  begin_tmp = *begin;
  end_tmp = *end;

  /*
   * Fordward begin iter character by character until:
   * - We reach a non space character
   * - We reach end iter
   */
  while (g_unichar_isspace (gtk_text_iter_get_char (begin)) &&
         gtk_text_iter_compare (begin, &end_tmp) < 0)
    gtk_text_iter_forward_char (begin);


  /*
   * If after moving forward the begin iter, we reached the end iter,
   * there is no need to play with the end iter.
   */
  if (gtk_text_iter_compare (begin, end) < 0)
    {
      /*
       * Backward end iter character by character until:
       * - We reach a non space character
       * - We reach begin iter
       */
      while (g_unichar_isspace (gtk_text_iter_get_char (end)) &&
             gtk_text_iter_compare (end, &begin_tmp) > 0)
        gtk_text_iter_backward_char (end);

      /*
       * If we found the character we are looking for then move one
       * character forward in order to include it as the last
       * character of the begin - end range.
       */
      if (gtk_text_iter_compare (end, &end_tmp) < 0)
        gtk_text_iter_forward_char (end);
    }

  return gtk_text_iter_compare (begin, end) < 0;
}

static void
sync_tag_style (GtkSourceStyleScheme *style_scheme,
                GtkTextTag           *tag)
{
  g_autofree gchar *foreground = NULL;
  g_autofree gchar *background = NULL;
  g_autofree gchar *tag_name = NULL;
  gchar *style_name = NULL;
  const gchar *colon;
  GtkSourceStyle *style;
  gboolean foreground_set = FALSE;
  gboolean background_set = FALSE;
  gboolean bold = FALSE;
  gboolean bold_set = FALSE;
  gboolean underline = FALSE;
  gboolean underline_set = FALSE;
  gboolean italic = FALSE;
  gboolean italic_set = FALSE;
  gsize tag_name_len;
  gsize prefix_len;

  g_object_set (tag,
                "foreground-set", FALSE,
                "background-set", FALSE,
                "weight-set", FALSE,
                "underline-set", FALSE,
                "style-set", FALSE,
                NULL);

  g_object_get (tag, "name", &tag_name, NULL);

  if (tag_name == NULL || style_scheme == NULL)
    return;

  prefix_len = strlen (PRIVATE_TAG_PREFIX);
  tag_name_len = strlen (tag_name);
  style_name = tag_name;

  /*
   * Check if this is a private tag.A tag is private if it starts with
   * PRIVATE_TAG_PREFIX "gb-private-tag".
   * ex: gb-private-tag:c:boolean
   * If the tag is private extract the original style name by moving the string
   * strlen (PRIVATE_TAG_PREFIX) + 1 (the colon) characters.
   */
  if (tag_name_len > prefix_len && memcmp (tag_name, PRIVATE_TAG_PREFIX, prefix_len) == 0)
    style_name = tag_name + prefix_len + 1;

  style = gtk_source_style_scheme_get_style (style_scheme, style_name);
  if (style == NULL && (colon = strchr (style_name, ':')))
    {
      gchar defname[64];
      g_snprintf (defname, sizeof defname, "def%s", colon);
      style = gtk_source_style_scheme_get_style (style_scheme, defname);
      if (style == NULL)
        return;
    }

  g_object_get (style,
                "background", &background,
                "background-set", &background_set,
                "foreground", &foreground,
                "foreground-set", &foreground_set,
                "bold", &bold,
                "bold-set", &bold_set,
                "underline", &underline,
                "underline-set", &underline_set,
                "italic", &italic,
                "italic-set", &italic_set,
                NULL);

  if (background_set)
    g_object_set (tag, "background", background, NULL);

  if (foreground_set)
    g_object_set (tag, "foreground", foreground, NULL);

  if (bold_set && bold)
    g_object_set (tag, "weight", PANGO_WEIGHT_BOLD, NULL);

  if (italic_set && italic)
    g_object_set (tag, "style", PANGO_STYLE_ITALIC, NULL);

  if (underline_set && underline)
    g_object_set (tag, "underline", PANGO_UNDERLINE_SINGLE, NULL);
}

static GtkTextTag *
create_tag_from_style (IdeHighlightEngine *self,
                       const gchar        *style_name)
{
  GtkSourceStyleScheme *style_scheme;
  GtkTextTag *tag;

  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));
  g_assert (IDE_IS_BUFFER (self->buffer));
  g_assert (style_name != NULL);

  tag = gtk_text_buffer_create_tag (GTK_TEXT_BUFFER (self->buffer), style_name, NULL);
  gtk_text_tag_set_priority (tag, 0);
  style_scheme = gtk_source_buffer_get_style_scheme (GTK_SOURCE_BUFFER (self->buffer));
  sync_tag_style (style_scheme, tag);

  return tag;
}

GtkTextTag *
get_tag_from_style (IdeHighlightEngine *self,
                    const gchar        *style_name,
                    gboolean            private_tag)
{
  GtkTextTagTable *tag_table;
  GtkTextTag *tag;
  g_autofree gchar *tmp_style_name = NULL;

  g_return_val_if_fail (IDE_IS_HIGHLIGHT_ENGINE (self), NULL);
  g_return_val_if_fail (style_name != NULL, NULL);

  /*
   * If is private tag prepend the PRIVATE_TAG_PREFIX (gb-private-tag)
   * to the string.This is used because tag name is the key used
   * for saving tags in GtkTextTagTable and we dont want conflicts between
   * public and private tags.
   */
  if (private_tag)
    tmp_style_name = g_strdup_printf ("%s:%s", PRIVATE_TAG_PREFIX, style_name);
  else
    tmp_style_name = g_strdup (style_name);

  tag_table = gtk_text_buffer_get_tag_table (GTK_TEXT_BUFFER (self->buffer));
  tag = gtk_text_tag_table_lookup (tag_table, tmp_style_name);

  if (tag == NULL)
    {
      tag = create_tag_from_style (self, tmp_style_name);
      if (private_tag)
        self->private_tags = g_slist_prepend (self->private_tags, tag);
      else
        self->public_tags = g_slist_prepend (self->public_tags, tag);
    }

  return tag;
}


static IdeHighlightResult
ide_highlight_engine_apply_style (const GtkTextIter *begin,
                                  const GtkTextIter *end,
                                  const gchar       *style_name)
{
  IdeHighlightEngine *self;
  GtkTextBuffer *buffer;
  GtkTextTag *tag;

  buffer = gtk_text_iter_get_buffer (begin);
  self = g_object_get_qdata (G_OBJECT (buffer), engineQuark);
  tag = get_tag_from_style (self, style_name, TRUE);

  gtk_text_buffer_apply_tag (buffer, tag, begin, end);

  if (g_get_monotonic_time () >= self->quanta_expiration)
    return IDE_HIGHLIGHT_STOP;

  return IDE_HIGHLIGHT_CONTINUE;
}

static gboolean
ide_highlight_engine_tick (IdeHighlightEngine *self)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkTextIter invalid_begin;
  GtkTextIter invalid_end;
  GSList *tags_iter;

  IDE_PROBE;

  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));
  g_assert (self->buffer != NULL);
  g_assert (self->highlighter != NULL);
  g_assert (self->invalid_begin != NULL);
  g_assert (self->invalid_end != NULL);

  self->quanta_expiration = g_get_monotonic_time () + HIGHLIGHT_QUANTA_USEC;

  buffer = GTK_TEXT_BUFFER (self->buffer);

  gtk_text_buffer_get_iter_at_mark (buffer, &invalid_begin, self->invalid_begin);
  gtk_text_buffer_get_iter_at_mark (buffer, &invalid_end, self->invalid_end);

  IDE_TRACE_MSG ("Highlight Range [%u:%u,%u:%u] (%s)",
                 gtk_text_iter_get_line (&invalid_begin),
                 gtk_text_iter_get_line_offset (&invalid_begin),
                 gtk_text_iter_get_line (&invalid_end),
                 gtk_text_iter_get_line_offset (&invalid_end),
                 G_OBJECT_TYPE_NAME (self->highlighter));

  if (gtk_text_iter_compare (&invalid_begin, &invalid_end) >= 0)
    IDE_GOTO (up_to_date);

  /* Clear all our tags */
  for (tags_iter = self->private_tags; tags_iter; tags_iter = tags_iter->next)
    gtk_text_buffer_remove_tag (buffer,
                                GTK_TEXT_TAG (tags_iter->data),
                                &invalid_begin,
                                &invalid_end);

  iter = invalid_begin;

  ide_highlighter_update (self->highlighter, ide_highlight_engine_apply_style,
                          &invalid_begin, &invalid_end, &iter);

  if (gtk_text_iter_compare (&iter, &invalid_end) >= 0)
    IDE_GOTO (up_to_date);

  /* Stop processing until further instruction if no movement was made */
  if (gtk_text_iter_equal (&iter, &invalid_begin))
    return FALSE;

  gtk_text_buffer_move_mark (buffer, self->invalid_begin, &iter);

  return TRUE;

up_to_date:
  gtk_text_buffer_get_start_iter (buffer, &iter);
  gtk_text_buffer_move_mark (buffer, self->invalid_begin, &iter);
  gtk_text_buffer_move_mark (buffer, self->invalid_end, &iter);

  return FALSE;
}

static gboolean
ide_highlight_engine_work_timeout_handler (gpointer data)
{
  IdeHighlightEngine *self = data;

  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));

  if (self->enabled)
    {
      if (ide_highlight_engine_tick (self))
        return G_SOURCE_CONTINUE;
    }

  self->work_timeout = 0;

  return G_SOURCE_REMOVE;
}

static void
ide_highlight_engine_queue_work (IdeHighlightEngine *self)
{
  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));

  if ((self->highlighter == NULL) || (self->buffer == NULL) || (self->work_timeout != 0))
    return;

  /*
   * NOTE: It would be really nice if we could use the GdkFrameClock here to
   *       drive the next update instead of a timeout. It's possible that our
   *       callback could get scheduled right before the frame processing would
   *       begin. However, since that gets driven by something like a Wayland
   *       callback, it won't yet be scheduled. So instead our function gets
   *       called and we potentially cause a frame to drop.
   */

  self->work_timeout = gdk_threads_add_idle_full (G_PRIORITY_LOW + 1,
                                                  ide_highlight_engine_work_timeout_handler,
                                                  self,
                                                  NULL);
}

static gboolean
invalidate_and_highlight (IdeHighlightEngine *self,
                          GtkTextIter        *begin,
                          GtkTextIter        *end)
{

  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));
  g_assert (begin != NULL);
  g_assert (end != NULL);

  if (!self->enabled)
    return FALSE;

  if (get_invalidation_area (begin, end))
    {
      GtkTextIter begin_tmp;
      GtkTextIter end_tmp;
      GtkTextBuffer *text_buffer = GTK_TEXT_BUFFER (self->buffer);

      gtk_text_buffer_get_iter_at_mark (text_buffer, &begin_tmp, self->invalid_begin);
      gtk_text_buffer_get_iter_at_mark (text_buffer, &end_tmp, self->invalid_end);

      if (gtk_text_iter_equal (&begin_tmp, &end_tmp))
        {
          gtk_text_buffer_move_mark (text_buffer, self->invalid_begin, begin);
          gtk_text_buffer_move_mark (text_buffer, self->invalid_end, end);
        }
      else
        {
          if (gtk_text_iter_compare (begin, &begin_tmp) < 0)
            gtk_text_buffer_move_mark (text_buffer, self->invalid_begin, begin);
          if (gtk_text_iter_compare (end, &end_tmp) > 0)
            gtk_text_buffer_move_mark (text_buffer, self->invalid_end, end);
        }

      ide_highlight_engine_queue_work (self);

      return TRUE;
    }

  return FALSE;
}

static void
ide_highlight_engine_reload (IdeHighlightEngine *self)
{
  GtkTextBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;
  GSList *iter;

  IDE_ENTRY;

  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));

  if (self->work_timeout != 0)
    {
      g_source_remove (self->work_timeout);
      self->work_timeout = 0;
    }

  if (self->buffer == NULL)
    IDE_EXIT;

  buffer = GTK_TEXT_BUFFER (self->buffer);

  gtk_text_buffer_get_bounds (buffer, &begin, &end);

  /*
   * Invalidate the whole buffer.
   */
  gtk_text_buffer_move_mark (buffer, self->invalid_begin, &begin);
  gtk_text_buffer_move_mark (buffer, self->invalid_end, &end);

  /*
   * Remove our highlight tags from the buffer.
   */
  for (iter = self->private_tags; iter; iter = iter->next)
    gtk_text_buffer_remove_tag (buffer, iter->data, &begin, &end);
  g_clear_pointer (&self->private_tags, g_slist_free);

  for (iter = self->public_tags; iter; iter = iter->next)
    gtk_text_buffer_remove_tag (buffer, iter->data, &begin, &end);
  g_clear_pointer (&self->public_tags, g_slist_free);

  if (self->highlighter == NULL)
    IDE_EXIT;

  ide_highlight_engine_queue_work (self);

  IDE_EXIT;
}

static void
ide_highlight_engine_set_highlighter (IdeHighlightEngine *self,
                                      IdeHighlighter     *highlighter)
{
  g_return_if_fail (IDE_IS_HIGHLIGHT_ENGINE (self));
  g_return_if_fail (!highlighter || IDE_IS_HIGHLIGHTER (highlighter));

  if (g_set_object (&self->highlighter, highlighter))
    {
      if (highlighter != NULL)
        {
          IDE_HIGHLIGHTER_GET_IFACE (highlighter)->set_engine (highlighter, self);
          ide_highlighter_load (highlighter);
        }

      ide_highlight_engine_reload (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HIGHLIGHTER]);
    }
}

static void
ide_highlight_engine__buffer_insert_text_cb (IdeHighlightEngine *self,
                                             GtkTextIter        *location,
                                             gchar              *text,
                                             gint                len,
                                             IdeBuffer          *buffer)
{
  GtkTextIter begin;
  GtkTextIter end;

  IDE_ENTRY;

  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));
  g_assert (location);
  g_assert (text);
  g_assert (IDE_IS_BUFFER (buffer));

  if (!self->enabled)
    IDE_EXIT;

  /*
   * Backward the begin iter len characters from location
   * (location points to the end of the string) in order to get
   * the iter position where our inserted text was started.
   */
  begin = *location;
  gtk_text_iter_backward_chars (&begin, g_utf8_strlen (text, len));

  end = *location;

  invalidate_and_highlight (self, &begin, &end);

  IDE_EXIT;
}

static void
ide_highlight_engine__buffer_delete_range_cb (IdeHighlightEngine *self,
                                              GtkTextIter        *range_begin,
                                              GtkTextIter        *range_end,
                                              IdeBuffer          *buffer)
{
  GtkTextIter begin;
  GtkTextIter end;

  IDE_ENTRY;

  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));
  g_assert (range_begin);
  g_assert (IDE_IS_BUFFER (buffer));

  if (!self->enabled)
    IDE_EXIT;

  /*
   * No need to use the range_end since everything that
   * was after range_end will now be after range_begin
   */
  begin = *range_begin;
  end = *range_begin;

  invalidate_and_highlight (self, &begin, &end);

  IDE_EXIT;
}

static void
ide_highlight_engine__notify_language_cb (IdeHighlightEngine *self,
                                          GParamSpec         *pspec,
                                          IdeBuffer          *buffer)
{
  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));
  g_assert (IDE_IS_BUFFER (buffer));

  if (self->extension != NULL)
    {
      GtkSourceLanguage *language;
      const gchar *lang_id = NULL;

      if ((language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (buffer))))
        lang_id = gtk_source_language_get_id (language);

      ide_extension_adapter_set_value (self->extension, lang_id);
    }
}

static void
ide_highlight_engine__notify_style_scheme_cb (IdeHighlightEngine *self,
                                              GParamSpec         *pspec,
                                              IdeBuffer          *buffer)
{
  GtkSourceStyleScheme *style_scheme;
  GSList *iter;

  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));
  g_assert (IDE_IS_BUFFER (buffer));

  style_scheme = gtk_source_buffer_get_style_scheme (GTK_SOURCE_BUFFER (buffer));

  for (iter = self->private_tags; iter; iter = iter->next)
    sync_tag_style (style_scheme, iter->data);
  for (iter = self->public_tags; iter; iter = iter->next)
    sync_tag_style (style_scheme, iter->data);
}

void
ide_highlight_engine_clear (IdeHighlightEngine *self)
{
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));

  if (self->buffer != NULL)
    {
      GtkTextBuffer *buffer = GTK_TEXT_BUFFER (self->buffer);
      GSList *iter;

      gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (self->buffer), &begin, &end);

      for (iter = self->public_tags; iter; iter = iter->next)
        gtk_text_buffer_remove_tag (buffer, iter->data, &begin, &end);
    }
}

static void
ide_highlight_engine__bind_buffer_cb (IdeHighlightEngine *self,
                                      IdeBuffer          *buffer,
                                      DzlSignalGroup     *group)
{
  GtkTextBuffer *text_buffer = (GtkTextBuffer *)buffer;
  GtkTextIter begin;
  GtkTextIter end;

  IDE_ENTRY;

  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (DZL_IS_SIGNAL_GROUP (group));

  self->buffer = buffer;

  g_object_set_qdata (G_OBJECT (buffer), engineQuark, self);

  gtk_text_buffer_get_bounds (text_buffer, &begin, &end);

  self->invalid_begin = gtk_text_buffer_create_mark (text_buffer, NULL, &begin, TRUE);
  self->invalid_end = gtk_text_buffer_create_mark (text_buffer, NULL, &end, FALSE);

  ide_highlight_engine__notify_style_scheme_cb (self, NULL, buffer);
  ide_highlight_engine__notify_language_cb (self, NULL, buffer);

  ide_highlight_engine_reload (self);

  IDE_EXIT;
}

static void
ide_highlight_engine__unbind_buffer_cb (IdeHighlightEngine  *self,
                                        DzlSignalGroup      *group)
{
  GtkTextBuffer *text_buffer;
  GtkTextTagTable *tag_table;
  GtkTextIter begin;
  GtkTextIter end;
  GSList *iter;

  IDE_ENTRY;

  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));
  g_assert (DZL_IS_SIGNAL_GROUP (group));

  text_buffer = GTK_TEXT_BUFFER (self->buffer);

  if (self->work_timeout)
    {
      g_source_remove (self->work_timeout);
      self->work_timeout = 0;
    }

  g_object_set_qdata (G_OBJECT (text_buffer), engineQuark, NULL);

  tag_table = gtk_text_buffer_get_tag_table (text_buffer);

  gtk_text_buffer_delete_mark (text_buffer, self->invalid_begin);
  gtk_text_buffer_delete_mark (text_buffer, self->invalid_end);

  self->invalid_begin = NULL;
  self->invalid_end = NULL;

  gtk_text_buffer_get_bounds (text_buffer, &begin, &end);

  for (iter = self->private_tags; iter; iter = iter->next)
    {
      gtk_text_buffer_remove_tag (text_buffer, iter->data, &begin, &end);
      gtk_text_tag_table_remove (tag_table, iter->data);
    }
  g_clear_pointer (&self->private_tags, g_slist_free);

  for (iter = self->public_tags; iter; iter = iter->next)
    {
      gtk_text_buffer_remove_tag (text_buffer, iter->data, &begin, &end);
      gtk_text_tag_table_remove (tag_table, iter->data);
    }
  g_clear_pointer (&self->public_tags, g_slist_free);

  self->buffer = NULL;

  IDE_EXIT;
}

static void
ide_highlight_engine_set_buffer (IdeHighlightEngine *self,
                                 IdeBuffer          *buffer)
{
  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));
  g_assert (!buffer || GTK_IS_TEXT_BUFFER (buffer));

  /* We can get GtkSourceBuffer intermittently here. */
  if (!buffer || IDE_IS_BUFFER (buffer))
    {
      dzl_signal_group_set_target (self->signal_group, buffer);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUFFER]);
    }
}

static void
ide_highlight_engine_settings_changed (IdeHighlightEngine *self,
                                       const gchar        *key,
                                       GSettings          *settings)
{
  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));
  g_assert (G_IS_SETTINGS (settings));

  if (g_settings_get_boolean (settings, "semantic-highlighting"))
    {
      self->enabled = TRUE;
      ide_highlight_engine_rebuild (self);
    }
  else
    {
      self->enabled = FALSE;
      ide_highlight_engine_clear (self);
    }
}

static void
ide_highlight_engine__notify_extension (IdeHighlightEngine  *self,
                                        GParamSpec          *pspec,
                                        IdeExtensionAdapter *adapter)
{
  IdeHighlighter *highlighter;

  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));
  g_assert (IDE_IS_EXTENSION_ADAPTER (adapter));

  highlighter = ide_extension_adapter_get_extension (adapter);
  g_return_if_fail (!highlighter || IDE_IS_HIGHLIGHTER (highlighter));

  ide_highlight_engine_set_highlighter (self, highlighter);
}

static void
ide_highlight_engine_constructed (GObject *object)
{
  IdeHighlightEngine *self = (IdeHighlightEngine *)object;
  IdeContext *context;

  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));

  /* We require a context at construction time */
  context = ide_object_get_context (IDE_OBJECT (self));
  g_assert (IDE_IS_CONTEXT (context));

  self->extension = ide_extension_adapter_new (context,
                                               NULL,
                                               IDE_TYPE_HIGHLIGHTER,
                                               "Highlighter-Languages",
                                               NULL);
  g_signal_connect_object (self->extension,
                           "notify::extension",
                           G_CALLBACK (ide_highlight_engine__notify_extension),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
ide_highlight_engine_dispose (GObject *object)
{
  IdeHighlightEngine *self = (IdeHighlightEngine *)object;

  g_clear_object (&self->signal_group);
  g_clear_object (&self->extension);
  g_clear_object (&self->highlighter);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (ide_highlight_engine_parent_class)->dispose (object);
}

static void
ide_highlight_engine_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  IdeHighlightEngine *self = IDE_HIGHLIGHT_ENGINE (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      g_value_set_object (value, ide_highlight_engine_get_buffer (self));
      break;

    case PROP_HIGHLIGHTER:
      g_value_set_object (value, ide_highlight_engine_get_highlighter (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_highlight_engine_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  IdeHighlightEngine *self = IDE_HIGHLIGHT_ENGINE (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      ide_highlight_engine_set_buffer (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_highlight_engine_class_init (IdeHighlightEngineClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_highlight_engine_constructed;
  object_class->dispose = ide_highlight_engine_dispose;
  object_class->get_property = ide_highlight_engine_get_property;
  object_class->set_property = ide_highlight_engine_set_property;

  properties [PROP_BUFFER] =
    g_param_spec_object ("buffer",
                         "Buffer",
                         "The buffer to highlight.",
                         IDE_TYPE_BUFFER,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_HIGHLIGHTER] =
    g_param_spec_object ("highlighter",
                         "Highlighter",
                         "The highlighter to use for type information.",
                         IDE_TYPE_HIGHLIGHTER,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  engineQuark = g_quark_from_string ("IDE_HIGHLIGHT_ENGINE");
}

static void
ide_highlight_engine_init (IdeHighlightEngine *self)
{
  self->settings = g_settings_new ("org.gnome.builder.code-insight");
  self->enabled = g_settings_get_boolean (self->settings, "semantic-highlighting");
  self->signal_group = dzl_signal_group_new (IDE_TYPE_BUFFER);

  dzl_signal_group_connect_object (self->signal_group,
                                   "insert-text",
                                   G_CALLBACK (ide_highlight_engine__buffer_insert_text_cb),
                                   self,
                                   G_CONNECT_SWAPPED | G_CONNECT_AFTER);

  dzl_signal_group_connect_object (self->signal_group,
                                   "delete-range",
                                   G_CALLBACK (ide_highlight_engine__buffer_delete_range_cb),
                                   self,
                                   G_CONNECT_SWAPPED | G_CONNECT_AFTER);

  dzl_signal_group_connect_object (self->signal_group,
                                   "notify::language",
                                   G_CALLBACK (ide_highlight_engine__notify_language_cb),
                                   self,
                                   G_CONNECT_SWAPPED);

  dzl_signal_group_connect_object (self->signal_group,
                                   "notify::style-scheme",
                                   G_CALLBACK (ide_highlight_engine__notify_style_scheme_cb),
                                   self,
                                   G_CONNECT_SWAPPED);

  g_signal_connect_object (self->signal_group,
                           "bind",
                           G_CALLBACK (ide_highlight_engine__bind_buffer_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->signal_group,
                           "unbind",
                           G_CALLBACK (ide_highlight_engine__unbind_buffer_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->settings,
                           "changed::semantic-highlighting",
                           G_CALLBACK (ide_highlight_engine_settings_changed),
                           self,
                           G_CONNECT_SWAPPED);
}

IdeHighlightEngine *
ide_highlight_engine_new (IdeBuffer *buffer)
{
  IdeContext *context;

  g_return_val_if_fail (IDE_IS_BUFFER (buffer), NULL);

  context = ide_buffer_get_context (buffer);

  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  return g_object_new (IDE_TYPE_HIGHLIGHT_ENGINE,
                       "buffer", buffer,
                       "context", context,
                       NULL);
}

/**
 * ide_highlight_engine_get_highlighter:
 * @self: A #IdeHighlightEngine.
 *
 * Gets the IdeHighlightEngine:highlighter property.
 *
 * Returns: (transfer none): An #IdeHighlighter.
 */
IdeHighlighter *
ide_highlight_engine_get_highlighter (IdeHighlightEngine *self)
{
  g_return_val_if_fail (IDE_IS_HIGHLIGHT_ENGINE (self), NULL);

  return self->highlighter;
}

/**
 * ide_highlight_engine_get_buffer:
 * @self: A #IdeHighlightEngine.
 *
 * Gets the IdeHighlightEngine:buffer property.
 *
 * Returns: (transfer none): An #IdeBuffer.
 */
IdeBuffer *
ide_highlight_engine_get_buffer (IdeHighlightEngine *self)
{
  g_return_val_if_fail (IDE_IS_HIGHLIGHT_ENGINE (self), NULL);

  return self->buffer;
}

void
ide_highlight_engine_rebuild (IdeHighlightEngine *self)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_HIGHLIGHT_ENGINE (self));

  if (self->buffer != NULL)
    {
      GtkTextBuffer *buffer = GTK_TEXT_BUFFER (self->buffer);
      GtkTextIter begin;
      GtkTextIter end;

      gtk_text_buffer_get_bounds (buffer, &begin, &end);
      gtk_text_buffer_move_mark (buffer, self->invalid_begin, &begin);
      gtk_text_buffer_move_mark (buffer, self->invalid_end, &end);
      ide_highlight_engine_queue_work (self);
    }

  IDE_EXIT;
}

/**
 * ide_highlight_engine_invalidate:
 * @self: An #IdeHighlightEngine.
 * @begin: the beginning of the range to invalidate
 * @end: the end of the range to invalidate
 *
 * This function will extend the invalidated range of the buffer to include
 * the range of @begin to @end.
 *
 * The highlighter will be queued to interactively update the invalidated
 * region.
 *
 * Updating the invalidated region of the buffer may take some time, as it is
 * important that the highlighter does not block for more than 1-2 milliseconds
 * to avoid dropping frames.
 */
void
ide_highlight_engine_invalidate (IdeHighlightEngine *self,
                                 const GtkTextIter  *begin,
                                 const GtkTextIter  *end)
{
  GtkTextBuffer *buffer;
  GtkTextIter mark_begin;
  GtkTextIter mark_end;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_HIGHLIGHT_ENGINE (self));
  g_return_if_fail (begin != NULL);
  g_return_if_fail (end != NULL);
  g_return_if_fail (gtk_text_iter_get_buffer (begin) == GTK_TEXT_BUFFER (self->buffer));
  g_return_if_fail (gtk_text_iter_get_buffer (end) == GTK_TEXT_BUFFER (self->buffer));

  buffer = GTK_TEXT_BUFFER (self->buffer);

  gtk_text_buffer_get_iter_at_mark (buffer, &mark_begin, self->invalid_begin);
  gtk_text_buffer_get_iter_at_mark (buffer, &mark_end, self->invalid_end);

  if (gtk_text_iter_equal (&mark_begin, &mark_end))
    {
      gtk_text_buffer_move_mark (buffer, self->invalid_begin, begin);
      gtk_text_buffer_move_mark (buffer, self->invalid_end, end);
    }
  else
    {
      if (gtk_text_iter_compare (begin, &mark_begin) < 0)
        gtk_text_buffer_move_mark (buffer, self->invalid_begin, begin);
      if (gtk_text_iter_compare (end, &mark_end) > 0)
        gtk_text_buffer_move_mark (buffer, self->invalid_end, end);
    }

  ide_highlight_engine_queue_work (self);

  IDE_EXIT;
}

/**
 * ide_highlight_engine_get_style:
 * @self: the #IdeHighlightEngine
 * @style_name: the name of the style to retrieve
 *
 * A #GtkTextTag for @style_name.
 *
 * Returns: (transfer none): A #GtkTextTag.
 */
GtkTextTag *
ide_highlight_engine_get_style (IdeHighlightEngine *self,
                                const gchar        *style_name)
{
  return get_tag_from_style (self, style_name, FALSE);
}

void
ide_highlight_engine_pause (IdeHighlightEngine *self)
{
  g_return_if_fail (IDE_IS_HIGHLIGHT_ENGINE (self));

  dzl_signal_group_block (self->signal_group);
}

void
ide_highlight_engine_unpause (IdeHighlightEngine *self)
{
  g_return_if_fail (IDE_IS_HIGHLIGHT_ENGINE (self));
  g_return_if_fail (self->signal_group != NULL);

  dzl_signal_group_unblock (self->signal_group);

  if (self->buffer != NULL)
    {
      /* Notify of some blocked signals */
      ide_highlight_engine__notify_style_scheme_cb (self, NULL, self->buffer);
      ide_highlight_engine__notify_language_cb (self, NULL, self->buffer);

      ide_highlight_engine_reload (self);
    }
}
