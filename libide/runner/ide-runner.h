/* ide-runner.h
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

#ifndef IDE_RUNNER_H
#define IDE_RUNNER_H

#include "ide-types.h"

G_BEGIN_DECLS

typedef enum
{
  IDE_RUNNER_INVALID,
  IDE_RUNNER_READY,
  IDE_RUNNER_RUNNING,
  IDE_RUNNER_EXITED,
  IDE_RUNNER_FAILED,
} IdeRunnerState;

#define IDE_TYPE_RUNNER (ide_runner_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeRunner, ide_runner, IDE, RUNNER, IdeObject)

struct _IdeRunnerClass
{
  IdeObject parent_instance;

  void (*force_quit) (IdeRunner *self);
  void (*run)        (IdeRunner *self);
};

IdeRunner *ide_runner_new        (IdeContext *context);
void       ide_runner_force_quit (IdeRunner  *self);
void       ide_runner_run        (IdeRunner  *self);

G_END_DECLS

#endif /* IDE_RUNNER_H */
