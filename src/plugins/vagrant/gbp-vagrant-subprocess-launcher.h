/* gbp-vagrant-subprocess-launcher.h
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#include <libide-foundry.h>

G_BEGIN_DECLS

#define GBP_TYPE_VAGRANT_SUBPROCESS_LAUNCHER (gbp_vagrant_subprocess_launcher_get_type())
#define GBP_VAGRANT_SUBPROCESS_LAUNCHER_C_OPT "@@VAGRANT_C_OPT@@"

G_DECLARE_FINAL_TYPE (GbpVagrantSubprocessLauncher, gbp_vagrant_subprocess_launcher, GBP, VAGRANT_SUBPROCESS_LAUNCHER, IdeSubprocessLauncher)

IdeSubprocessLauncher *gbp_vagrant_subprocess_launcher_new (const gchar *dir);

G_END_DECLS
