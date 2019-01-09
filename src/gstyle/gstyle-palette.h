/* gstyle-palette.h
 *
 * Copyright 2016 sebastien lafargue <slafargue@gnome.org>
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

#pragma once

#include <glib-object.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include <libxml/xmlreader.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "gstyle-color.h"

G_BEGIN_DECLS

#define GSTYLE_PALETTE_ERROR (gstyle_palette_error_quark())

#define GSTYLE_TYPE_PALETTE (gstyle_palette_get_type())

G_DECLARE_FINAL_TYPE (GstylePalette, gstyle_palette, GSTYLE, PALETTE, GObject)

typedef enum
{
  GSTYLE_PALETTE_ERROR_DUP_COLOR_NAME,
  GSTYLE_PALETTE_ERROR_EMPTY,
  GSTYLE_PALETTE_ERROR_FILE,
  GSTYLE_PALETTE_ERROR_FORMAT,
  GSTYLE_PALETTE_ERROR_PARSE
} GstylePaletteError;

GQuark              gstyle_palette_error_quark           (void);

GstylePalette      *gstyle_palette_new                   (void);
GstylePalette      *gstyle_palette_new_from_buffer       (GtkTextBuffer  *buffer,
                                                          GtkTextIter    *begin,
                                                          GtkTextIter    *end,
                                                          GCancellable   *cancellable,
                                                          GError        **error);
GstylePalette      *gstyle_palette_new_from_file         (GFile          *file,
                                                          GCancellable   *cancellable,
                                                          GError        **error);
gboolean            gstyle_palette_add                   (GstylePalette  *self,
                                                          GstyleColor    *color,
                                                          GError        **error);
gboolean            gstyle_palette_add_at_index          (GstylePalette  *self,
                                                          GstyleColor    *color,
                                                          gint            position,
                                                          GError        **error);
gboolean            gstyle_palette_get_changed           (GstylePalette  *self);
GPtrArray          *gstyle_palette_get_colors            (GstylePalette  *self);
const GstyleColor  *gstyle_palette_get_color_at_index    (GstylePalette  *self,
                                                          guint           index);
gint                gstyle_palette_get_index             (GstylePalette  *self,
                                                          GstyleColor    *color);
const gchar        *gstyle_palette_get_id                (GstylePalette  *self);
guint               gstyle_palette_get_len               (GstylePalette  *self);
const gchar        *gstyle_palette_get_name              (GstylePalette  *self);
GFile              *gstyle_palette_get_file              (GstylePalette  *self);
GPtrArray          *gstyle_palette_lookup                (GstylePalette  *self,
                                                          const gchar    *name);
gboolean            gstyle_palette_remove                (GstylePalette  *self,
                                                          GstyleColor    *color);
gboolean            gstyle_palette_remove_at_index       (GstylePalette  *self,
                                                          gint            position);
gboolean            gstyle_palette_save_to_xml           (GstylePalette  *self,
                                                          GFile          *file,
                                                          GError        **error);
void                gstyle_palette_set_changed           (GstylePalette  *self,
                                                          gboolean        changed);
void                gstyle_palette_set_name              (GstylePalette  *self,
                                                          const gchar    *name);
void                gstyle_palette_set_id                (GstylePalette  *self,
                                                          const gchar    *id);

G_END_DECLS
