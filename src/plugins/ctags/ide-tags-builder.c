/* ide-tags-builder.c
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

#define G_LOG_DOMAIN "ide-tags-builder"

#include "config.h"

#include "ide-tags-builder.h"

G_DEFINE_INTERFACE (IdeTagsBuilder, ide_tags_builder, G_TYPE_OBJECT)


void
ide_tags_builder_build_async (IdeTagsBuilder      *self,
                              GFile               *directory_or_file,
                              gboolean             recursive,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  g_return_if_fail (IDE_IS_TAGS_BUILDER (self));
  g_return_if_fail (!directory_or_file || G_IS_FILE (directory_or_file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_TAGS_BUILDER_GET_IFACE (self)->build_async (self, directory_or_file, recursive, cancellable, callback, user_data);
}

gboolean
ide_tags_builder_build_finish (IdeTagsBuilder  *self,
                               GAsyncResult    *result,
                               GError         **error)
{
  g_return_val_if_fail (IDE_IS_TAGS_BUILDER (self), FALSE);

  return IDE_TAGS_BUILDER_GET_IFACE (self)->build_finish (self, result, error);
}

static void
ide_tags_builder_default_init (IdeTagsBuilderInterface *iface)
{
}
