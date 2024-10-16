/* ide-highlight-engine.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-highlight-engine"

#include "config.h"

#include <glib/gi18n.h>

#include <gtksourceview/gtksource.h>
#include <string.h>

#include <libide-plugins.h>

#include "ide-buffer.h"
#include "ide-buffer-private.h"
#include "ide-highlight-engine.h"
#include "ide-highlight-index.h"
#include "ide-highlighter.h"

#include "cjhtextregionprivate.h"

#define PRIVATE_TAG_PREFIX "Builder"

#define RUN_UNCHECKED GSIZE_TO_POINTER(0)
#define RUN_CHECKED   GSIZE_TO_POINTER(1)

struct _IdeHighlightEngine
{
  IdeObject            parent_instance;

  GWeakRef             buffer_wref;

  GSignalGroup        *signal_group;
  IdeHighlighter      *highlighter;
  GSettings           *settings;

  IdeExtensionAdapter *extension;

  CjhTextRegion       *region;

  GSList              *private_tags;
  GSList              *public_tags;

  gint64               quanta_expiration;

  gsize                work_scheduled;

  guint                commit_funcs_handler;

  guint                enabled : 1;
};

G_DEFINE_FINAL_TYPE (IdeHighlightEngine, ide_highlight_engine, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_BUFFER,
  PROP_HIGHLIGHTER,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static void
sync_tag_style (GtkSourceStyleScheme *style_scheme,
                GtkSourceLanguage    *language,
                GtkTextTag           *tag)
{
  g_autofree gchar *foreground = NULL;
  g_autofree gchar *background = NULL;
  g_autofree gchar *tag_name = NULL;
  const char *style_name = NULL;
  GtkSourceStyle *style = NULL;
  const char *fallback;
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
   * Check if this is a private tag. A tag is private if it starts with
   * PRIVATE_TAG_PREFIX "Builder" such as "Builder:c:boolean".
   * If the tag is private extract the original style name by moving the string
   * strlen (PRIVATE_TAG_PREFIX) + 1 (the colon) characters.
   */
  if (tag_name_len > prefix_len && memcmp (tag_name, PRIVATE_TAG_PREFIX, prefix_len) == 0)
    style_name = tag_name + prefix_len + 1;

  fallback = style_name;
  while (style == NULL && fallback != NULL)
    {
      if (!(style = gtk_source_style_scheme_get_style (style_scheme, fallback)))
        {
          if (language)
            fallback = gtk_source_language_get_style_fallback (language, fallback);
          else
            fallback = NULL;
        }
    }

  if (style == NULL)
    return;

  g_object_get (style,
                "background", &background,
                "background-set", &background_set,
                "foreground", &foreground,
                "foreground-set", &foreground_set,
                "bold", &bold,
                "bold-set", &bold_set,
                "pango-underline", &underline,
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
  g_autoptr(IdeBuffer) buffer = NULL;
  GtkSourceStyleScheme *style_scheme;
  GtkSourceLanguage *language;
  GtkTextTag *tag = NULL;

  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));
  g_assert (style_name != NULL);

  buffer = g_weak_ref_get (&self->buffer_wref);

  if (buffer != NULL)
    {
      tag = gtk_text_buffer_create_tag (GTK_TEXT_BUFFER (buffer), style_name, NULL);
      gtk_text_tag_set_priority (tag, 0);

      style_scheme = gtk_source_buffer_get_style_scheme (GTK_SOURCE_BUFFER (buffer));
      language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (buffer));

      sync_tag_style (style_scheme, language, tag);
    }

  return tag;
}

static GtkTextTag *
get_tag_from_style (IdeHighlightEngine *self,
                    const gchar        *style_name,
                    gboolean            private_tag)
{
  g_autoptr(IdeBuffer) buffer = NULL;
  g_autofree gchar *tmp_style_name = NULL;
  GtkTextTagTable *tag_table;
  GtkTextTag *tag;

  g_return_val_if_fail (IDE_IS_HIGHLIGHT_ENGINE (self), NULL);
  g_return_val_if_fail (style_name != NULL, NULL);

  buffer = g_weak_ref_get (&self->buffer_wref);
  if (buffer == NULL)
    return NULL;

  /*
   * If is private tag prepend the PRIVATE_TAG_PREFIX (Builder)
   * to the string.This is used because tag name is the key used
   * for saving tags in GtkTextTagTable and we dont want conflicts between
   * public and private tags.
   */
  if (private_tag)
    tmp_style_name = g_strdup_printf ("%s:%s", PRIVATE_TAG_PREFIX, style_name);
  else
    tmp_style_name = g_strdup (style_name);

  tag_table = gtk_text_buffer_get_tag_table (GTK_TEXT_BUFFER (buffer));
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
  self = _ide_buffer_get_highlight_engine (IDE_BUFFER (buffer));
  tag = get_tag_from_style (self, style_name, TRUE);

  gtk_text_buffer_apply_tag (buffer, tag, begin, end);

  if (g_get_monotonic_time () >= self->quanta_expiration)
    return IDE_HIGHLIGHT_STOP;

  return IDE_HIGHLIGHT_CONTINUE;
}

typedef struct
{
  gsize offset;
  gsize length;
} GetUncheckedRange;

static inline gboolean
get_unchecked_start_cb (gsize                   offset,
                        const CjhTextRegionRun *run,
                        gpointer                user_data)
{
  GetUncheckedRange *range = user_data;

  g_assert (run != NULL);
  g_assert (run->length > 0);

  if (range->offset == G_MAXSIZE && run->data == RUN_CHECKED)
    return FALSE;

  if (run->data == RUN_UNCHECKED)
    {
      if (range->offset == G_MAXSIZE)
        range->offset = offset;
      range->length += run->length;
      return FALSE;
    }

  return TRUE;
}

static gboolean
get_next_range (CjhTextRegion *region,
                GtkTextBuffer *buffer,
                GtkTextIter   *begin,
                GtkTextIter   *end)
{
  GetUncheckedRange range = {G_MAXSIZE, 0};

  _cjh_text_region_foreach (region, get_unchecked_start_cb, &range);

  if (range.length == 0 || range.offset == G_MAXSIZE)
    return FALSE;

  gtk_text_buffer_get_iter_at_offset (buffer, begin, range.offset);
  gtk_text_buffer_get_iter_at_offset (buffer, end, range.offset + range.length);

  return !gtk_text_iter_equal (begin, end);
}

static gboolean
ide_highlight_engine_tick (IdeHighlightEngine *self,
                           gint64              deadline)
{
  g_autoptr(GtkTextBuffer) buffer = NULL;
  GtkTextIter iter;
  GtkTextIter invalid_begin;
  GtkTextIter invalid_end;

  IDE_PROBE;

  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));
  g_assert (self->highlighter != NULL);

  if (!(buffer = g_weak_ref_get (&self->buffer_wref)))
    return G_SOURCE_REMOVE;

  self->quanta_expiration = deadline;

  if (!get_next_range (self->region, buffer, &invalid_begin, &invalid_end))
    return G_SOURCE_REMOVE;

again:
  g_assert (gtk_text_iter_compare (&invalid_begin, &invalid_end) <= 0);

  IDE_TRACE_MSG ("Highlight Range [%u:%u,%u:%u] (%s)",
                 gtk_text_iter_get_line (&invalid_begin) + 1,
                 gtk_text_iter_get_line_offset (&invalid_begin) + 1,
                 gtk_text_iter_get_line (&invalid_end) + 1,
                 gtk_text_iter_get_line_offset (&invalid_end) + 1,
                 G_OBJECT_TYPE_NAME (self->highlighter));

  iter = invalid_begin;

  while (gtk_text_iter_ends_line (&iter))
    {
      if (!gtk_text_iter_forward_char (&iter))
        break;
    }

  if (gtk_text_iter_compare (&iter, &invalid_end) < 0)
    {
      GtkTextIter alt_begin = iter;

      ide_highlighter_update (self->highlighter,
                              self->private_tags,
                              ide_highlight_engine_apply_style,
                              &alt_begin,
                              &invalid_end,
                              &iter);
    }

  if (!gtk_text_iter_equal (&iter, &invalid_begin))
    _cjh_text_region_replace (self->region,
                              gtk_text_iter_get_offset (&invalid_begin),
                              gtk_text_iter_get_offset (&iter) - gtk_text_iter_get_offset (&invalid_begin),
                              RUN_CHECKED);

  if (gtk_text_iter_compare (&iter, &invalid_end) >= 0)
    {
      if (get_next_range (self->region, buffer, &invalid_begin, &invalid_end))
        IDE_GOTO (again);
    }

  /* Stop processing until further instruction if no movement was made */
  if (gtk_text_iter_equal (&iter, &invalid_begin))
    return G_SOURCE_REMOVE;

  return G_SOURCE_CONTINUE;
}

static gboolean
ide_highlight_engine_worker (gint64   deadline,
                             gpointer user_data)
{
  IdeHighlightEngine *self = user_data;

  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));

  if (self->enabled)
    {
      if (ide_highlight_engine_tick (self, deadline))
        return G_SOURCE_CONTINUE;
    }

  self->work_scheduled = 0;

  return G_SOURCE_REMOVE;
}

static void
ide_highlight_engine_queue_work (IdeHighlightEngine *self)
{
  g_autoptr(GtkTextBuffer) buffer = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));

  if (!self->enabled || self->work_scheduled != 0 || self->highlighter == NULL)
    return;

  if (!(buffer = g_weak_ref_get (&self->buffer_wref)))
    return;

  self->work_scheduled = gtk_source_scheduler_add (ide_highlight_engine_worker, self);
}

/**
 * ide_highlight_engine_advance:
 * @self: a #IdeHighlightEngine
 *
 * This function is useful for #IdeHighlighter implementations that need to
 * asynchronously do work to process the highlighting.
 *
 * If they return from their update function without advancing, nothing will
 * happen until they call this method to proceed.
 */
void
ide_highlight_engine_advance (IdeHighlightEngine *self)
{
  g_return_if_fail (IDE_IS_HIGHLIGHT_ENGINE (self));

  ide_highlight_engine_queue_work (self);
}

static void
ide_highlight_engine_reload (IdeHighlightEngine *self)
{
  g_autoptr(GtkTextBuffer) buffer = NULL;
  GtkTextIter begin;
  GtkTextIter end;
  GSList *iter;

  IDE_ENTRY;

  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));

  gtk_source_scheduler_clear (&self->work_scheduled);

  if (!(buffer = g_weak_ref_get (&self->buffer_wref)))
    IDE_EXIT;

  gtk_text_buffer_get_bounds (buffer, &begin, &end);
  ide_highlight_engine_invalidate (self, &begin, &end);

  /* Remove our highlight tags from the buffer. */
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
  GtkSourceLanguage *language;
  GSList *iter;

  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));
  g_assert (IDE_IS_BUFFER (buffer));

  style_scheme = gtk_source_buffer_get_style_scheme (GTK_SOURCE_BUFFER (buffer));
  language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (buffer));

  for (iter = self->private_tags; iter; iter = iter->next)
    sync_tag_style (style_scheme, language, iter->data);
  for (iter = self->public_tags; iter; iter = iter->next)
    sync_tag_style (style_scheme, language, iter->data);
}

void
ide_highlight_engine_clear (IdeHighlightEngine *self)
{
  g_autoptr(GtkTextBuffer) buffer = NULL;
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));

  buffer = g_weak_ref_get (&self->buffer_wref);

  if (buffer != NULL)
    {
      gtk_text_buffer_get_bounds (buffer, &begin, &end);

      for (const GSList *iter = self->public_tags; iter; iter = iter->next)
        {
          GtkTextTag *tag = iter->data;
          gtk_text_buffer_remove_tag (buffer, tag, &begin, &end);
        }
    }
}

static void
mark_unchecked (IdeHighlightEngine *self,
                IdeBuffer          *buffer,
                guint               offset,
                guint               length)
{
  GtkTextIter begin, end;

  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));

  gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &begin, offset);
  gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &end, offset + length);

  while (!gtk_text_iter_starts_line (&begin))
    {
      gunichar ch;

      if (!gtk_text_iter_backward_char (&begin))
        break;

      ch = gtk_text_iter_get_char (&begin);

      if (ch != '_' && !g_unichar_isalnum (ch))
        break;
    }

  while (!gtk_text_iter_ends_line (&end))
    {
      gunichar ch;

      if (!gtk_text_iter_forward_char (&end))
        break;

      ch = gtk_text_iter_get_char (&end);

      if (ch != '_' && !g_unichar_isalnum (ch))
        break;
    }

  ide_highlight_engine_invalidate (self, &begin, &end);
}

static void
ide_highlight_engine_insert_before_cb (GtkTextBuffer            *buffer,
                                       GtkTextBufferNotifyFlags  flags,
                                       guint                     offset,
                                       guint                     length,
                                       gpointer                  user_data)
{
  IdeHighlightEngine *self = user_data;

  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));

  _cjh_text_region_insert (self->region, offset, length, RUN_UNCHECKED);
}

static void
ide_highlight_engine_insert_after_cb (GtkTextBuffer            *buffer,
                                      GtkTextBufferNotifyFlags  flags,
                                      guint                     offset,
                                      guint                     length,
                                      gpointer                  user_data)
{
  IdeHighlightEngine *self = user_data;
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));

  /* Mark the whole line as unchecked */

  gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &begin, offset);
  gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &end, offset + length);

  if (!gtk_text_iter_starts_line (&begin))
    gtk_text_iter_set_line_offset (&begin, 0);

  if (!gtk_text_iter_ends_line (&end))
    gtk_text_iter_forward_to_line_end (&end);

  mark_unchecked (self,
                  IDE_BUFFER (buffer),
                  gtk_text_iter_get_offset (&begin),
                  gtk_text_iter_get_offset (&end) - gtk_text_iter_get_offset (&begin));
}

static void
ide_highlight_engine_delete_before_cb (GtkTextBuffer            *buffer,
                                       GtkTextBufferNotifyFlags  flags,
                                       guint                     offset,
                                       guint                     length,
                                       gpointer                  user_data)
{
  IdeHighlightEngine *self = user_data;

  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));

  _cjh_text_region_remove (self->region, offset, length);
}

static void
ide_highlight_engine_delete_after_cb (GtkTextBuffer            *buffer,
                                      GtkTextBufferNotifyFlags  flags,
                                      guint                     offset,
                                      guint                     length,
                                      gpointer                  user_data)
{
  IdeHighlightEngine *self = user_data;
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));

  /* Mark the whole line as unchecked */

  gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &begin, offset);
  end = begin;

  if (!gtk_text_iter_starts_line (&begin))
    gtk_text_iter_set_line_offset (&begin, 0);

  if (!gtk_text_iter_ends_line (&end))
    gtk_text_iter_forward_to_line_end (&end);

  mark_unchecked (self,
                  IDE_BUFFER (buffer),
                  gtk_text_iter_get_offset (&begin),
                  gtk_text_iter_get_offset (&end) - gtk_text_iter_get_offset (&begin));
}

static void
ide_highlight_engine_commit_notify (GtkTextBuffer            *buffer,
                                    GtkTextBufferNotifyFlags  flags,
                                    guint                     position,
                                    guint                     length,
                                    gpointer                  user_data)
{
  if (flags == GTK_TEXT_BUFFER_NOTIFY_BEFORE_INSERT)
    ide_highlight_engine_insert_before_cb (buffer, flags, position, length, user_data);
  else if (flags == GTK_TEXT_BUFFER_NOTIFY_AFTER_INSERT)
    ide_highlight_engine_insert_after_cb (buffer, flags, position, length, user_data);
  else if (flags == GTK_TEXT_BUFFER_NOTIFY_BEFORE_DELETE)
    ide_highlight_engine_delete_before_cb (buffer, flags, position, length, user_data);
  else if (flags == GTK_TEXT_BUFFER_NOTIFY_AFTER_DELETE)
    ide_highlight_engine_delete_after_cb (buffer, flags, position, length, user_data);
}

static void
ide_highlight_engine__bind_buffer_cb (IdeHighlightEngine *self,
                                      IdeBuffer          *buffer,
                                      GSignalGroup       *group)
{
  GtkTextIter begin;
  GtkTextIter end;

  IDE_ENTRY;

  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_SIGNAL_GROUP (group));

  g_weak_ref_set (&self->buffer_wref, buffer);

  self->commit_funcs_handler =
    gtk_text_buffer_add_commit_notify (GTK_TEXT_BUFFER (buffer),
                                       (GTK_TEXT_BUFFER_NOTIFY_BEFORE_INSERT |
                                        GTK_TEXT_BUFFER_NOTIFY_AFTER_INSERT |
                                        GTK_TEXT_BUFFER_NOTIFY_BEFORE_DELETE |
                                        GTK_TEXT_BUFFER_NOTIFY_AFTER_DELETE),
                                       ide_highlight_engine_commit_notify,
                                       self, NULL);

  gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &begin, &end);

  if (gtk_text_iter_compare (&begin, &end) != 0)
    {
      guint length = gtk_text_iter_get_offset (&end);

      _cjh_text_region_insert (self->region, 0, length, RUN_UNCHECKED);
    }

  ide_highlight_engine__notify_style_scheme_cb (self, NULL, buffer);
  ide_highlight_engine__notify_language_cb (self, NULL, buffer);

  ide_highlight_engine_reload (self);

  IDE_EXIT;
}

static void
ide_highlight_engine__unbind_buffer_cb (IdeHighlightEngine  *self,
                                        GSignalGroup        *group)
{
  g_autoptr(GtkTextBuffer) text_buffer = NULL;
  gsize length;

  IDE_ENTRY;

  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));
  g_assert (G_IS_SIGNAL_GROUP (group));

  text_buffer = g_weak_ref_get (&self->buffer_wref);

  gtk_source_scheduler_clear (&self->work_scheduled);

  if ((length = _cjh_text_region_get_length (self->region)))
    _cjh_text_region_remove (self->region, 0, length - 1);

  if (text_buffer != NULL)
    {
      g_autoptr(GSList) private_tags = NULL;
      g_autoptr(GSList) public_tags = NULL;
      GtkTextTagTable *tag_table;
      GtkTextIter begin;
      GtkTextIter end;

      if (self->commit_funcs_handler)
        {
          gtk_text_buffer_remove_commit_notify (text_buffer,
                                                self->commit_funcs_handler);
          self->commit_funcs_handler = 0;
        }

      tag_table = gtk_text_buffer_get_tag_table (text_buffer);

      gtk_text_buffer_get_bounds (text_buffer, &begin, &end);

      private_tags = g_steal_pointer (&self->private_tags);
      public_tags = g_steal_pointer (&self->public_tags);

      for (const GSList *iter = private_tags; iter; iter = iter->next)
        {
          gtk_text_buffer_remove_tag (text_buffer, iter->data, &begin, &end);
          gtk_text_tag_table_remove (tag_table, iter->data);
        }

      for (const GSList *iter = public_tags; iter; iter = iter->next)
        {
          gtk_text_buffer_remove_tag (text_buffer, iter->data, &begin, &end);
          gtk_text_tag_table_remove (tag_table, iter->data);
        }
    }

  g_clear_pointer (&self->public_tags, g_slist_free);
  g_clear_pointer (&self->private_tags, g_slist_free);

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
      g_signal_group_set_target (self->signal_group, buffer);
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
ide_highlight_engine_parent_set (IdeObject *object,
                                 IdeObject *parent)
{
  IdeHighlightEngine *self = (IdeHighlightEngine *)object;

  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));
  g_assert (!parent || IDE_IS_OBJECT (parent));

  if (parent == NULL)
    {
      g_clear_object (&self->extension);
      return;
    }

  self->extension = ide_extension_adapter_new (IDE_OBJECT (self),
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
ide_highlight_engine_destroy (IdeObject *object)
{
  IdeHighlightEngine *self = (IdeHighlightEngine *)object;

  g_weak_ref_set (&self->buffer_wref, NULL);
  g_clear_object (&self->signal_group);
  g_clear_object (&self->extension);
  g_clear_object (&self->highlighter);
  g_clear_object (&self->settings);
  g_clear_pointer (&self->region, _cjh_text_region_free);

  IDE_OBJECT_CLASS (ide_highlight_engine_parent_class)->destroy (object);
}

static void
ide_highlight_engine_finalize (GObject *object)
{
  IdeHighlightEngine *self = (IdeHighlightEngine *)object;

  g_weak_ref_clear (&self->buffer_wref);

  G_OBJECT_CLASS (ide_highlight_engine_parent_class)->finalize (object);
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
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  object_class->finalize = ide_highlight_engine_finalize;
  object_class->get_property = ide_highlight_engine_get_property;
  object_class->set_property = ide_highlight_engine_set_property;

  i_object_class->destroy = ide_highlight_engine_destroy;
  i_object_class->parent_set = ide_highlight_engine_parent_set;

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
}

static void
ide_highlight_engine_init (IdeHighlightEngine *self)
{
  g_weak_ref_init (&self->buffer_wref, NULL);

  self->settings = g_settings_new ("org.gnome.builder.code-insight");
  self->enabled = g_settings_get_boolean (self->settings, "semantic-highlighting");
  self->signal_group = g_signal_group_new (IDE_TYPE_BUFFER);

  self->region = _cjh_text_region_new (NULL, NULL);

  g_signal_group_connect_object (self->signal_group,
                                   "notify::language",
                                   G_CALLBACK (ide_highlight_engine__notify_language_cb),
                                   self,
                                   G_CONNECT_SWAPPED);

  g_signal_group_connect_object (self->signal_group,
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
  IdeHighlightEngine *self;
  IdeObjectBox *box;

  g_return_val_if_fail (IDE_IS_BUFFER (buffer), NULL);

  self = g_object_new (IDE_TYPE_HIGHLIGHT_ENGINE,
                       "buffer", buffer,
                       NULL);

  box = ide_object_box_from_object (G_OBJECT (buffer));
  ide_object_append (IDE_OBJECT (box), IDE_OBJECT (self));

  return g_steal_pointer (&self);
}

/**
 * ide_highlight_engine_get_highlighter:
 * @self: an #IdeHighlightEngine.
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
 * @self: an #IdeHighlightEngine.
 *
 * Gets the IdeHighlightEngine:buffer property.
 *
 * Returns: (transfer none): An #IdeBuffer.
 */
IdeBuffer *
ide_highlight_engine_get_buffer (IdeHighlightEngine *self)
{
  g_autoptr(IdeBuffer) buffer = NULL;

  g_return_val_if_fail (IDE_IS_HIGHLIGHT_ENGINE (self), NULL);

  /* We don't need the "thread-safety" provided by GWeakRef here,
   * (where it gives us a new object reference). It is safe to
   * return a borrowed reference instead.
   */
  buffer = g_weak_ref_get (&self->buffer_wref);
  return buffer;
}

void
ide_highlight_engine_rebuild (IdeHighlightEngine *self)
{
  g_autoptr(GtkTextBuffer) buffer = NULL;
  GtkTextIter begin, end;
  gsize length;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_HIGHLIGHT_ENGINE (self));

  if (!self->enabled)
    IDE_EXIT;

  if ((buffer = g_weak_ref_get (&self->buffer_wref)))
    {
      /* We remove using the known length from the region */
      if ((length = _cjh_text_region_get_length (self->region)) > 0)
        _cjh_text_region_remove (self->region, 0, length - 1);

      /* We add using the length from the buffer because if we were not
       * enabled previously, the textregion would be empty.
       */
      gtk_text_buffer_get_bounds (buffer, &begin, &end);
      if (!gtk_text_iter_equal (&begin, &end))
        {
          length = gtk_text_iter_get_offset (&end) - gtk_text_iter_get_offset (&begin);

          if (length > 0)
            _cjh_text_region_insert (self->region, 0, length, RUN_UNCHECKED);
        }
    }

  ide_highlight_engine_queue_work (self);

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
  guint offset;
  guint length;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_HIGHLIGHT_ENGINE (self));
  g_return_if_fail (gtk_text_iter_compare (begin, end) <= 0);

  offset = gtk_text_iter_get_offset (begin);
  length = gtk_text_iter_get_offset (end) - gtk_text_iter_get_offset (begin);

  if (length > 0)
    {
      _cjh_text_region_remove (self->region, offset, length);
      _cjh_text_region_insert (self->region, offset, length, RUN_UNCHECKED);
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
 * Returns: (transfer none): a #GtkTextTag.
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

  g_signal_group_block (self->signal_group);
}

void
ide_highlight_engine_unpause (IdeHighlightEngine *self)
{
  g_autoptr(IdeBuffer) buffer = NULL;

  g_return_if_fail (IDE_IS_HIGHLIGHT_ENGINE (self));
  g_return_if_fail (self->signal_group != NULL);

  g_signal_group_unblock (self->signal_group);

  buffer = g_weak_ref_get (&self->buffer_wref);

  if (buffer != NULL)
    {
      /* Notify of some blocked signals */
      ide_highlight_engine__notify_style_scheme_cb (self, NULL, buffer);
      ide_highlight_engine__notify_language_cb (self, NULL, buffer);

      ide_highlight_engine_reload (self);
    }
}
