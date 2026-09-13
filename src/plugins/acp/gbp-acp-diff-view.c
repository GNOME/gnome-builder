/* gbp-acp-diff-view.c
 *
 * Copyright 2026 GNOME Builder contributors
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

#define G_LOG_DOMAIN "gbp-acp-diff-view"

#include "config.h"

#include <glib/gi18n.h>

#include "gbp-acp-diff-view.h"

#define DIFF_MAX_LINES 800

struct _GbpAcpDiffView
{
  GtkBox parent_instance;
};

G_DEFINE_FINAL_TYPE (GbpAcpDiffView, gbp_acp_diff_view, GTK_TYPE_BOX)

static GtkCssProvider *diff_css;

static void
ensure_css (void)
{
  if (diff_css != NULL)
    return;

  diff_css = gtk_css_provider_new ();
  gtk_css_provider_load_from_string (diff_css,
    ".acp-diff {"
    "  font-size: 0.9em;"
    "}"
    ".acp-diff textview {"
    "  background: transparent;"
    "}");
}

static void
append_line (GtkTextBuffer *buffer,
             const gchar   *prefix,
             const gchar   *line,
             GtkTextTag    *tag)
{
  GtkTextIter iter;

  gtk_text_buffer_get_end_iter (buffer, &iter);

  if (tag != NULL)
    gtk_text_buffer_insert_with_tags (buffer, &iter, prefix, -1, tag, NULL);
  else
    gtk_text_buffer_insert (buffer, &iter, prefix, -1);

  gtk_text_buffer_get_end_iter (buffer, &iter);

  if (tag != NULL)
    gtk_text_buffer_insert_with_tags (buffer, &iter, line, -1, tag, NULL);
  else
    gtk_text_buffer_insert (buffer, &iter, line, -1);

  gtk_text_buffer_get_end_iter (buffer, &iter);
  gtk_text_buffer_insert (buffer, &iter, "\n", -1);
}

static void
render_plain_diff (GtkTextBuffer *buffer,
                   GtkTextTag    *removed_tag,
                   GtkTextTag    *added_tag,
                   const gchar   *old_text,
                   const gchar   *new_text)
{
  g_auto(GStrv) old_lines = NULL;
  g_auto(GStrv) new_lines = NULL;

  if (old_text != NULL)
    {
      old_lines = g_strsplit (old_text, "\n", -1);
      for (guint i = 0; old_lines[i] != NULL && i < DIFF_MAX_LINES; i++)
        append_line (buffer, "- ", old_lines[i], removed_tag);
    }

  if (new_text != NULL)
    {
      new_lines = g_strsplit (new_text, "\n", -1);
      for (guint i = 0; new_lines[i] != NULL && i < DIFF_MAX_LINES; i++)
        append_line (buffer, "+ ", new_lines[i], added_tag);
    }
}

static void
render_diff (GtkTextBuffer *buffer,
             GtkTextTag    *removed_tag,
             GtkTextTag    *added_tag,
             const gchar   *old_text,
             const gchar   *new_text)
{
  g_auto(GStrv) old_lines = NULL;
  g_auto(GStrv) new_lines = NULL;
  g_autofree gint *dp = NULL;
  guint n;
  guint m;

  if (old_text == NULL)
    {
      render_plain_diff (buffer, removed_tag, added_tag, NULL, new_text);
      return;
    }

  old_lines = g_strsplit (old_text, "\n", -1);
  new_lines = g_strsplit (new_text ?: "", "\n", -1);
  n = g_strv_length (old_lines);
  m = g_strv_length (new_lines);

  if (n > DIFF_MAX_LINES || m > DIFF_MAX_LINES)
    {
      render_plain_diff (buffer, removed_tag, added_tag, old_text, new_text);
      return;
    }

  dp = g_new0 (gint, (n + 1) * (m + 1));

  for (gint i = n - 1; i >= 0; i--)
    {
      for (gint j = m - 1; j >= 0; j--)
        {
          if (g_strcmp0 (old_lines[i], new_lines[j]) == 0)
            dp[i * (m + 1) + j] = dp[(i + 1) * (m + 1) + (j + 1)] + 1;
          else
            dp[i * (m + 1) + j] = MAX (dp[(i + 1) * (m + 1) + j],
                                       dp[i * (m + 1) + (j + 1)]);
        }
    }

  {
    guint i = 0;
    guint j = 0;

    while (i < n && j < m)
      {
        if (g_strcmp0 (old_lines[i], new_lines[j]) == 0)
          {
            append_line (buffer, "  ", old_lines[i], NULL);
            i++;
            j++;
          }
        else if (dp[(i + 1) * (m + 1) + j] >= dp[i * (m + 1) + (j + 1)])
          {
            append_line (buffer, "- ", old_lines[i], removed_tag);
            i++;
          }
        else
          {
            append_line (buffer, "+ ", new_lines[j], added_tag);
            j++;
          }
      }

    while (i < n)
      append_line (buffer, "- ", old_lines[i++], removed_tag);

    while (j < m)
      append_line (buffer, "+ ", new_lines[j++], added_tag);
  }
}

static void
gbp_acp_diff_view_class_init (GbpAcpDiffViewClass *klass)
{
}

static void
gbp_acp_diff_view_init (GbpAcpDiffView *self)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);
  gtk_box_set_spacing (GTK_BOX (self), 6);
  gtk_widget_add_css_class (GTK_WIDGET (self), "acp-diff");
}

GtkWidget *
gbp_acp_diff_view_new (const gchar *path,
                       const gchar *old_text,
                       const gchar *new_text)
{
  GbpAcpDiffView *self;
  GtkTextBuffer *buffer;
  GtkTextTag *removed_tag;
  GtkTextTag *added_tag;
  GtkWidget *header;
  GtkWidget *scroller;
  GtkWidget *text_view;
  g_autofree gchar *title = NULL;
  g_autofree gchar *basename = NULL;
  GdkRGBA removed = { .red = 0.85, .green = 0.25, .blue = 0.25, .alpha = 0.25 };
  GdkRGBA added = { .red = 0.25, .green = 0.75, .blue = 0.35, .alpha = 0.25 };

  g_return_val_if_fail (path != NULL, NULL);

  ensure_css ();

  self = g_object_new (GBP_TYPE_ACP_DIFF_VIEW, NULL);

  basename = g_path_get_basename (path);
  title = g_strdup_printf (_("Changes to %s"), basename);

  header = gtk_label_new (title);
  gtk_widget_set_halign (header, GTK_ALIGN_START);
  gtk_label_set_ellipsize (GTK_LABEL (header), PANGO_ELLIPSIZE_MIDDLE);
  gtk_widget_set_tooltip_text (header, path);
  gtk_widget_add_css_class (header, "heading");
  gtk_box_append (GTK_BOX (self), header);

  text_view = gtk_text_view_new ();
  gtk_text_view_set_editable (GTK_TEXT_VIEW (text_view), FALSE);
  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (text_view), FALSE);
  gtk_text_view_set_monospace (GTK_TEXT_VIEW (text_view), TRUE);
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (text_view), GTK_WRAP_NONE);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));

  removed_tag = gtk_text_buffer_create_tag (buffer, "removed", NULL);
  g_object_set (removed_tag, "paragraph-background-rgba", &removed, NULL);
  added_tag = gtk_text_buffer_create_tag (buffer, "added", NULL);
  g_object_set (added_tag, "paragraph-background-rgba", &added, NULL);

  render_diff (buffer, removed_tag, added_tag, old_text, new_text);

  scroller = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (scroller), 320);
  gtk_scrolled_window_set_propagate_natural_height (GTK_SCROLLED_WINDOW (scroller), TRUE);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scroller), text_view);
  gtk_widget_add_css_class (scroller, "card");
  gtk_box_append (GTK_BOX (self), scroller);

  return GTK_WIDGET (self);
}
