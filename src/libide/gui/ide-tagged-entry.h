/*
 * Copyright 2011 Red Hat, Inc.
 * Copyright 2013 Ignacio Casal Quinteiro
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef __IDE_TAGGED_ENTRY_H__
#define __IDE_TAGGED_ENTRY_H__

#include <gtk/gtk.h>
#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_TAGGED_ENTRY ide_tagged_entry_get_type()
#define IDE_TAGGED_ENTRY(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), IDE_TYPE_TAGGED_ENTRY, IdeTaggedEntry))
#define IDE_TAGGED_ENTRY_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), IDE_TYPE_TAGGED_ENTRY, IdeTaggedEntryClass))
#define IDE_IS_TAGGED_ENTRY(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), IDE_TYPE_TAGGED_ENTRY))
#define IDE_IS_TAGGED_ENTRY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), IDE_TYPE_TAGGED_ENTRY))
#define IDE_TAGGED_ENTRY_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), IDE_TYPE_TAGGED_ENTRY, IdeTaggedEntryClass))

typedef struct _IdeTaggedEntry IdeTaggedEntry;
typedef struct _IdeTaggedEntryClass IdeTaggedEntryClass;
typedef struct _IdeTaggedEntryPrivate IdeTaggedEntryPrivate;

typedef struct _IdeTaggedEntryTag IdeTaggedEntryTag;
typedef struct _IdeTaggedEntryTagClass IdeTaggedEntryTagClass;
typedef struct _IdeTaggedEntryTagPrivate IdeTaggedEntryTagPrivate;

struct _IdeTaggedEntry
{
  GtkSearchEntry parent;

  IdeTaggedEntryPrivate *priv;
};

struct _IdeTaggedEntryClass
{
  GtkSearchEntryClass parent_class;
};

#define IDE_TYPE_TAGGED_ENTRY_TAG ide_tagged_entry_tag_get_type()
#define IDE_TAGGED_ENTRY_TAG(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), IDE_TYPE_TAGGED_ENTRY_TAG, IdeTaggedEntryTag))
#define IDE_TAGGED_ENTRY_TAG_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), IDE_TYPE_TAGGED_ENTRY_TAG, IdeTaggedEntryTagClass))
#define IDE_IS_TAGGED_ENTRY_TAG(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), IDE_TYPE_TAGGED_ENTRY_TAG))
#define IDE_IS_TAGGED_ENTRY_TAG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), IDE_TYPE_TAGGED_ENTRY_TAG))
#define IDE_TAGGED_ENTRY_TAG_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), IDE_TYPE_TAGGED_ENTRY_TAG, IdeTaggedEntryTagClass))

struct _IdeTaggedEntryTag
{
  GObject parent;

  IdeTaggedEntryTagPrivate *priv;
};

struct _IdeTaggedEntryTagClass
{
  GObjectClass parent_class;
};

IDE_AVAILABLE_IN_3_32
GType              ide_tagged_entry_get_type                 (void) G_GNUC_CONST;
IDE_AVAILABLE_IN_3_32
IdeTaggedEntry    *ide_tagged_entry_new                      (void);
IDE_AVAILABLE_IN_3_32
void               ide_tagged_entry_set_tag_button_visible   (IdeTaggedEntry        *self,
                                                              gboolean               visible);
IDE_AVAILABLE_IN_3_32
gboolean           ide_tagged_entry_get_tag_button_visible   (IdeTaggedEntry        *self);
IDE_AVAILABLE_IN_3_32
gboolean           ide_tagged_entry_insert_tag               (IdeTaggedEntry        *self,
                                                              IdeTaggedEntryTag     *tag,
                                                              gint                   position);
IDE_AVAILABLE_IN_3_32
gboolean           ide_tagged_entry_add_tag                  (IdeTaggedEntry        *self,
                                                              IdeTaggedEntryTag     *tag);
IDE_AVAILABLE_IN_3_32
gboolean           ide_tagged_entry_remove_tag               (IdeTaggedEntry        *self,
                                                              IdeTaggedEntryTag     *tag);
IDE_AVAILABLE_IN_3_32
GType              ide_tagged_entry_tag_get_type             (void) G_GNUC_CONST;
IDE_AVAILABLE_IN_3_32
IdeTaggedEntryTag *ide_tagged_entry_tag_new                  (const gchar           *label);
IDE_AVAILABLE_IN_3_32
void               ide_tagged_entry_tag_set_label            (IdeTaggedEntryTag     *tag,
                                                              const gchar           *label);
IDE_AVAILABLE_IN_3_32
const gchar       *ide_tagged_entry_tag_get_label            (IdeTaggedEntryTag     *tag);
IDE_AVAILABLE_IN_3_32
void               ide_tagged_entry_tag_set_has_close_button (IdeTaggedEntryTag     *tag,
                                                              gboolean               has_close_button);
IDE_AVAILABLE_IN_3_32
gboolean           ide_tagged_entry_tag_get_has_close_button (IdeTaggedEntryTag     *tag);
IDE_AVAILABLE_IN_3_32
void               ide_tagged_entry_tag_set_style            (IdeTaggedEntryTag     *tag,
                                                              const gchar           *style);
IDE_AVAILABLE_IN_3_32
const gchar       *ide_tagged_entry_tag_get_style            (IdeTaggedEntryTag     *tag);
IDE_AVAILABLE_IN_3_32
gboolean           ide_tagged_entry_tag_get_area             (IdeTaggedEntryTag     *tag,
                                                              cairo_rectangle_int_t *rect);

G_END_DECLS

#endif /* __IDE_TAGGED_ENTRY_H__ */
