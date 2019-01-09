/* ide-tags-builder.h
 *
 * Copyright 2015-2019 Christian Hergert <chergert@redhat.com>
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

#include <gio/gio.h>

G_BEGIN_DECLS

#define IDE_TYPE_TAGS_BUILDER (ide_tags_builder_get_type ())

G_DECLARE_INTERFACE (IdeTagsBuilder, ide_tags_builder, IDE, TAGS_BUILDER, GObject)

struct _IdeTagsBuilderInterface
{
  GTypeInterface parent;

  void     (*build_async)  (IdeTagsBuilder       *self,
                            GFile                *directory_or_file,
                            gboolean              recursive,
                            GCancellable         *cancellable,
                            GAsyncReadyCallback   callback,
                            gpointer              user_data);
  gboolean (*build_finish) (IdeTagsBuilder       *self,
                            GAsyncResult         *result,
                            GError              **error);
};

void      ide_tags_builder_build_async  (IdeTagsBuilder       *self,
                                         GFile                *directory_or_file,
                                         gboolean              recursive,
                                         GCancellable         *cancellable,
                                         GAsyncReadyCallback   callback,
                                         gpointer              user_data);
gboolean  ide_tags_builder_build_finish (IdeTagsBuilder       *self,
                                         GAsyncResult         *result,
                                         GError              **error);

G_END_DECLS
