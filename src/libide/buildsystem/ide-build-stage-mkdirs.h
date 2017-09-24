/* ide-build-stage-mkdirs.h
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#include "ide-build-stage.h"

G_BEGIN_DECLS

#define IDE_TYPE_BUILD_STAGE_MKDIRS (ide_build_stage_mkdirs_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeBuildStageMkdirs, ide_build_stage_mkdirs, IDE, BUILD_STAGE_MKDIRS, IdeBuildStage)

struct _IdeBuildStageMkdirsClass
{
  IdeBuildStageClass parent_class;

  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
};

IdeBuildStage *ide_build_stage_mkdirs_new      (IdeContext          *context);
void           ide_build_stage_mkdirs_add_path (IdeBuildStageMkdirs *self,
                                                const gchar         *path,
                                                gboolean             with_parents,
                                                gint                 mode,
                                                gboolean             remove_on_rebuild);

G_END_DECLS
