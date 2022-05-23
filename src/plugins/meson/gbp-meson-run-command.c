/* gbp-meson-run-command.c
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

#define G_LOG_DOMAIN "gbp-meson-run-command"

#include "config.h"

#include "gbp-meson-run-command.h"

struct _GbpMesonRunCommand
{
  IdeRunCommand parent_instance;
};

G_DEFINE_FINAL_TYPE (GbpMesonRunCommand, gbp_meson_run_command, IDE_TYPE_RUN_COMMAND)

static char **
gbp_meson_run_command_get_arguments (IdeRunCommand      *run_command,
                                     const char * const *wrapper)
{
  g_assert (GBP_IS_MESON_RUN_COMMAND (run_command));

  /* TODO: use --wrapper when 'meson test' */

  return IDE_RUN_COMMAND_CLASS (gbp_meson_run_command_parent_class)->get_arguments (run_command, wrapper);
}

static void
gbp_meson_run_command_class_init (GbpMesonRunCommandClass *klass)
{
  IdeRunCommandClass *run_command_class = IDE_RUN_COMMAND_CLASS (klass);

  run_command_class->get_arguments = gbp_meson_run_command_get_arguments;
}

static void
gbp_meson_run_command_init (GbpMesonRunCommand *self)
{
}
