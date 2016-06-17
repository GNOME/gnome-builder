/* ide-build-result.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#ifndef IDE_BUILD_RESULT_H
#define IDE_BUILD_RESULT_H

#include <gio/gio.h>

#include "ide-object.h"

#include "diagnostics/ide-diagnostic.h"

G_BEGIN_DECLS

#define IDE_TYPE_BUILD_RESULT (ide_build_result_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeBuildResult, ide_build_result, IDE, BUILD_RESULT, IdeObject)

typedef enum
{
  IDE_BUILD_RESULT_LOG_STDOUT,
  IDE_BUILD_RESULT_LOG_STDERR,
} IdeBuildResultLog;

struct _IdeBuildResultClass
{
  IdeObjectClass parent;

  void (*diagnostic) (IdeBuildResult    *self,
                      IdeDiagnostic     *diagnostic);
  void (*log)        (IdeBuildResult    *self,
                      IdeBuildResultLog  log,
                      const gchar       *message);
};

GInputStream  *ide_build_result_get_stdout_stream (IdeBuildResult *result);
GInputStream  *ide_build_result_get_stderr_stream (IdeBuildResult *result);
void           ide_build_result_log_subprocess    (IdeBuildResult *result,
                                                   GSubprocess    *subprocess);
GTimeSpan      ide_build_result_get_running_time  (IdeBuildResult *self);
gboolean       ide_build_result_get_running       (IdeBuildResult *self);
void           ide_build_result_set_running       (IdeBuildResult *self,
                                                   gboolean        running);
void           ide_build_result_emit_diagnostic   (IdeBuildResult *self,
                                                   IdeDiagnostic  *diagnostic);
gchar         *ide_build_result_get_mode          (IdeBuildResult *self);
void           ide_build_result_set_mode          (IdeBuildResult *self,
                                                   const gchar    *mode);
void           ide_build_result_log_stdout        (IdeBuildResult *result,
                                                   const gchar    *format,
                                                   ...) G_GNUC_PRINTF (2, 3);
void           ide_build_result_log_stderr        (IdeBuildResult *result,
                                                   const gchar    *format,
                                                   ...) G_GNUC_PRINTF (2, 3);

G_END_DECLS

#endif /* IDE_BUILD_RESULT_H */
