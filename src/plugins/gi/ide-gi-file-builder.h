/* ide-gi-file-builder.h
 *
 * Copyright © 2017 Sebastien Lafargue <slafargue@gnome.org>
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

#include <gio/gio.h>
#include <glib-object.h>

#include "ide-gi-file-builder-result.h"
#include "ide-gi-version.h"

G_BEGIN_DECLS

#define IDE_TYPE_GI_FILE_BUILDER (ide_gi_file_builder_get_type())

G_DECLARE_FINAL_TYPE (IdeGiFileBuilder, ide_gi_file_builder, IDE, GI_FILE_BUILDER, GObject)

IdeGiFileBuilderResult *ide_gi_file_builder_generate           (IdeGiFileBuilder     *self,
                                                                GFile                *file,
                                                                GFile                *write_path,
                                                                gint                  version_count,
                                                                GError              **error);
void                    ide_gi_file_builder_generate_async     (IdeGiFileBuilder     *self,
                                                                GFile                *file,
                                                                GFile                *write_path,
                                                                gint                  version_count,
                                                                GCancellable         *cancellable,
                                                                GAsyncReadyCallback   callback,
                                                                gpointer              user_data);
IdeGiFileBuilderResult  *ide_gi_file_builder_generate_finish   (IdeGiFileBuilder     *self,
                                                                GAsyncResult         *result,
                                                                GError              **error);
IdeGiFileBuilder        *ide_gi_file_builder_new               (void);

G_END_DECLS
