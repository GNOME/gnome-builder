/*
 * Copyright (c) 2011 Red Hat, Inc.
 * Copyright (c) 2013 Ignacio Casal Quinteiro
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by 
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public 
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License 
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#ifndef __GD_TAGGED_ENTRY_H__
#define __GD_TAGGED_ENTRY_H__

#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GD_TYPE_TAGGED_ENTRY gd_tagged_entry_get_type()

G_DECLARE_FINAL_TYPE (GdTaggedEntry, gd_tagged_entry,
                      GD, TAGGED_ENTRY, GtkSearchEntry)

#define GD_TYPE_TAGGED_ENTRY_TAG gd_tagged_entry_tag_get_type()

G_DECLARE_FINAL_TYPE (GdTaggedEntryTag, gd_tagged_entry_tag,
                      GD, TAGGED_ENTRY_TAG, GObject)


GdTaggedEntry *gd_tagged_entry_new (void);

void     gd_tagged_entry_set_tag_button_visible (GdTaggedEntry *self,
                                                 gboolean       visible);
gboolean gd_tagged_entry_get_tag_button_visible (GdTaggedEntry *self);

gboolean gd_tagged_entry_insert_tag (GdTaggedEntry    *self,
                                     GdTaggedEntryTag *tag,
                                     gint              position);

gboolean gd_tagged_entry_add_tag (GdTaggedEntry    *self,
                                  GdTaggedEntryTag *tag);

gboolean gd_tagged_entry_remove_tag (GdTaggedEntry *self,
                                     GdTaggedEntryTag *tag);


GdTaggedEntryTag *gd_tagged_entry_tag_new (const gchar *label);

void gd_tagged_entry_tag_set_label (GdTaggedEntryTag *tag,
                                    const gchar *label);
const gchar *gd_tagged_entry_tag_get_label (GdTaggedEntryTag *tag);

void gd_tagged_entry_tag_set_has_close_button (GdTaggedEntryTag *tag,
                                               gboolean has_close_button);
gboolean gd_tagged_entry_tag_get_has_close_button (GdTaggedEntryTag *tag);

void gd_tagged_entry_tag_set_style (GdTaggedEntryTag *tag,
                                    const gchar *style);
const gchar *gd_tagged_entry_tag_get_style (GdTaggedEntryTag *tag);

gboolean gd_tagged_entry_tag_get_area (GdTaggedEntryTag      *tag,
                                       cairo_rectangle_int_t *rect);

G_END_DECLS

#endif /* __GD_TAGGED_ENTRY_H__ */
