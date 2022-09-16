/* ide-run-command.h
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#if !defined (IDE_FOUNDRY_INSIDE) && !defined (IDE_FOUNDRY_COMPILATION)
# error "Only <libide-foundry.h> can be included directly."
#endif

#include <libide-core.h>

#include "ide-foundry-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_RUN_COMMAND (ide_run_command_get_type())

typedef enum
{
  IDE_RUN_COMMAND_KIND_UNKNOWN = 0,
  IDE_RUN_COMMAND_KIND_APPLICATION,
  IDE_RUN_COMMAND_KIND_UTILITY,
  IDE_RUN_COMMAND_KIND_TEST,
  IDE_RUN_COMMAND_KIND_BENCHMARK,
  IDE_RUN_COMMAND_KIND_USER_DEFINED,
} IdeRunCommandKind;

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeRunCommand, ide_run_command, IDE, RUN_COMMAND, GObject)

struct _IdeRunCommandClass
{
  GObjectClass parent_class;

  void (*prepare_to_run) (IdeRunCommand *self,
                          IdeRunContext *run_context,
                          IdeContext    *context);
};

IDE_AVAILABLE_IN_ALL
IdeRunCommand      *ide_run_command_new              (void);
IDE_AVAILABLE_IN_ALL
const char         *ide_run_command_get_id           (IdeRunCommand      *self);
IDE_AVAILABLE_IN_ALL
void                ide_run_command_set_id           (IdeRunCommand      *self,
                                                      const char         *id);
IDE_AVAILABLE_IN_ALL
const char         *ide_run_command_get_cwd          (IdeRunCommand      *self);
IDE_AVAILABLE_IN_ALL
void                ide_run_command_set_cwd          (IdeRunCommand      *self,
                                                      const char         *cwd);
IDE_AVAILABLE_IN_ALL
const char         *ide_run_command_get_display_name (IdeRunCommand      *self);
IDE_AVAILABLE_IN_ALL
void                ide_run_command_set_display_name (IdeRunCommand      *self,
                                                      const char         *display_name);
IDE_AVAILABLE_IN_ALL
const char * const *ide_run_command_get_argv         (IdeRunCommand      *self);
IDE_AVAILABLE_IN_ALL
void                ide_run_command_set_argv         (IdeRunCommand      *self,
                                                      const char * const *argv);
IDE_AVAILABLE_IN_ALL
void                ide_run_command_append_args      (IdeRunCommand      *self,
                                                      const char * const *args);
IDE_AVAILABLE_IN_ALL
void                ide_run_command_append_argv      (IdeRunCommand      *self,
                                                      const char         *arg);
IDE_AVAILABLE_IN_ALL
void                ide_run_command_append_formatted (IdeRunCommand      *self,
                                                      const char         *format,
                                                      ...) G_GNUC_PRINTF (2, 3);
IDE_AVAILABLE_IN_ALL
gboolean            ide_run_command_append_parsed    (IdeRunCommand      *self,
                                                      const char         *args,
                                                      GError            **error);
IDE_AVAILABLE_IN_ALL
const char * const *ide_run_command_get_environ      (IdeRunCommand      *self);
IDE_AVAILABLE_IN_ALL
void                ide_run_command_set_environ      (IdeRunCommand      *self,
                                                      const char * const *environ);
IDE_AVAILABLE_IN_ALL
const char         *ide_run_command_getenv           (IdeRunCommand      *self,
                                                      const char         *key);
IDE_AVAILABLE_IN_ALL
void                ide_run_command_setenv           (IdeRunCommand      *self,
                                                      const char         *key,
                                                      const char         *value);
IDE_AVAILABLE_IN_ALL
int                 ide_run_command_get_priority     (IdeRunCommand      *self);
IDE_AVAILABLE_IN_ALL
void                ide_run_command_set_priority     (IdeRunCommand      *self,
                                                      int                 priority);
IDE_AVAILABLE_IN_ALL
IdeRunCommandKind   ide_run_command_get_kind         (IdeRunCommand      *self);
IDE_AVAILABLE_IN_ALL
void                ide_run_command_set_kind         (IdeRunCommand      *self,
                                                      IdeRunCommandKind   kind);
IDE_AVAILABLE_IN_ALL
const char * const *ide_run_command_get_languages    (IdeRunCommand      *self);
IDE_AVAILABLE_IN_ALL
void                ide_run_command_set_languages    (IdeRunCommand      *self,
                                                      const char * const *languages);
IDE_AVAILABLE_IN_ALL
gboolean            ide_run_command_get_can_default  (IdeRunCommand      *self);
IDE_AVAILABLE_IN_ALL
void                ide_run_command_set_can_default  (IdeRunCommand      *self,
                                                      gboolean            can_default);
IDE_AVAILABLE_IN_ALL
void                ide_run_command_prepare_to_run   (IdeRunCommand      *self,
                                                      IdeRunContext      *run_context,
                                                      IdeContext         *context);

G_END_DECLS
