/* ide-build-result.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef IDE_BUILD_RESULT_H
#define IDE_BUILD_RESULT_H

#include <gio/gio.h>

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_BUILD_RESULT (ide_build_result_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeBuildResult, ide_build_result, IDE, BUILD_RESULT,
                          IdeObject)

struct _IdeBuildResultClass
{
  IdeObjectClass parent;
};

GInputStream  *ide_build_result_get_stdout_stream (IdeBuildResult *result);
GInputStream  *ide_build_result_get_stderr_stream (IdeBuildResult *result);
void           ide_build_result_log_subprocess    (IdeBuildResult *result,
                                                   GSubprocess    *subprocess);
void           ide_build_result_log_stdout        (IdeBuildResult *result,
                                                   const gchar    *format,
                                                   ...) G_GNUC_PRINTF (2, 3);
void           ide_build_result_log_stderr        (IdeBuildResult *result,
                                                   const gchar    *format,
                                                   ...) G_GNUC_PRINTF (2, 3);

G_END_DECLS

#endif /* IDE_BUILD_RESULT_H */
