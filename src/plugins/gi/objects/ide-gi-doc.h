/* ide-gi-doc.h
 *
 * Copyright © 2018 Sebastien Lafargue <slafargue@gnome.org>
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

#pragma once

#include <glib.h>
#include <glib-object.h>

#include "./../ide-gi-blob.h"
#include "./../ide-gi-types.h"
#include "./../ide-gi-namespace.h"
#include "./../ide-gi-namespace-private.h"

G_BEGIN_DECLS

struct _IdeGiDoc
{
  volatile gint   ref_count;

  IdeGiNamespace *ns;
  IdeGiDocBlob   *blob;
  gint32          offset;
};

void              ide_gi_doc_dump                     (IdeGiDoc       *self);
const gchar      *ide_gi_doc_get_doc                  (IdeGiDoc       *self);
const gchar      *ide_gi_doc_get_version              (IdeGiDoc       *self);
const gchar      *ide_gi_doc_get_deprecated_version   (IdeGiDoc       *self);
const gchar      *ide_gi_doc_get_stability            (IdeGiDoc       *self);

IdeGiDoc         *ide_gi_doc_new                      (IdeGiNamespace *ns,
                                                       gint32          offset);
IdeGiDoc         *ide_gi_doc_ref                      (IdeGiDoc       *self);
void              ide_gi_doc_unref                    (IdeGiDoc       *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeGiDoc, ide_gi_doc_unref)

G_END_DECLS
