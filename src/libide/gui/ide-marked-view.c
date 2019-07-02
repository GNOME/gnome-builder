/* ide-marked-view.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-marked-view"

#include "config.h"

#include <webkit2/webkit2.h>

#include "gs-markdown-private.h"
#include "ide-marked-view.h"

struct _IdeMarkedView
{
  GtkBin parent_instance;
};

G_DEFINE_TYPE (IdeMarkedView, ide_marked_view, GTK_TYPE_BIN)

static void
ide_marked_view_class_init (IdeMarkedViewClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_css_name (widget_class, "markedview");
}

static void
ide_marked_view_init (IdeMarkedView *self)
{
}

GtkWidget *
ide_marked_view_new (IdeMarkedContent *content)
{
  g_autofree gchar *markup = NULL;
  GtkWidget *child = NULL;
  IdeMarkedView *self;
  IdeMarkedKind kind;

  g_return_val_if_fail (content != NULL, NULL);

  self = g_object_new (IDE_TYPE_MARKED_VIEW, NULL);
  kind = ide_marked_content_get_kind (content);
  markup = ide_marked_content_as_string (content);

  switch (kind)
    {
    default:
    case IDE_MARKED_KIND_PLAINTEXT:
    case IDE_MARKED_KIND_PANGO:
      child = g_object_new (GTK_TYPE_LABEL,
                            "max-width-chars", 80,
                            "selectable", TRUE,
                            "wrap", TRUE,
                            "xalign", 0.0f,
                            "visible", TRUE,
                            "use-markup", kind == IDE_MARKED_KIND_PANGO,
                            "label", markup,
                            NULL);
      break;

    case IDE_MARKED_KIND_HTML:
      child = g_object_new (WEBKIT_TYPE_WEB_VIEW,
                            "visible", TRUE,
                            NULL);
      webkit_web_view_load_html (WEBKIT_WEB_VIEW (child), markup, NULL);
      break;

    case IDE_MARKED_KIND_MARKDOWN:
      {
        g_autoptr(GsMarkdown) md = gs_markdown_new (GS_MARKDOWN_OUTPUT_PANGO);
        g_autofree gchar *parsed = NULL;

        gs_markdown_set_smart_quoting (md, TRUE);
        gs_markdown_set_autocode (md, TRUE);
        gs_markdown_set_autolinkify (md, TRUE);

        if ((parsed = gs_markdown_parse (md, markup)))
          child = g_object_new (GTK_TYPE_LABEL,
                                "max-width-chars", 80,
                                "selectable", TRUE,
                                "wrap", TRUE,
                                "xalign", 0.0f,
                                "visible", TRUE,
                                "use-markup", TRUE,
                                "label", parsed,
                                NULL);
      }
      break;
    }

  if (child != NULL)
    gtk_container_add (GTK_CONTAINER (self), child);

  return GTK_WIDGET (self);
}
