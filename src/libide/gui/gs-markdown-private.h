/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright 2008-2013 Richard Hughes <richard@hughsie.com>
 * Copyright 2015 Kalev Lember <klember@redhat.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __GS_MARKDOWN_H
#define __GS_MARKDOWN_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GS_TYPE_MARKDOWN (gs_markdown_get_type ())

G_DECLARE_FINAL_TYPE (GsMarkdown, gs_markdown, GS, MARKDOWN, GObject)

typedef enum {
        GS_MARKDOWN_OUTPUT_TEXT,
        GS_MARKDOWN_OUTPUT_PANGO,
        GS_MARKDOWN_OUTPUT_HTML,
        GS_MARKDOWN_OUTPUT_LAST
} GsMarkdownOutputKind;

GsMarkdown      *gs_markdown_new                        (GsMarkdownOutputKind    output);
void             gs_markdown_set_max_lines              (GsMarkdown             *self,
                                                         gint                    max_lines);
void             gs_markdown_set_smart_quoting          (GsMarkdown             *self,
                                                         gboolean                smart_quoting);
void             gs_markdown_set_escape                 (GsMarkdown             *self,
                                                         gboolean                escape);
void             gs_markdown_set_autocode               (GsMarkdown             *self,
                                                         gboolean                autocode);
void             gs_markdown_set_autolinkify            (GsMarkdown             *self,
                                                         gboolean                autolinkify);
gchar           *gs_markdown_parse                      (GsMarkdown             *self,
                                                         const gchar            *text);

G_END_DECLS

#endif /* __GS_MARKDOWN_H */

