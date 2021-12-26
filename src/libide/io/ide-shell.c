/* ide-shell.c
 *
 * Copyright 2021 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#include "ide-shell.h"

gboolean
ide_shell_supports_dash_c (const char *shell)
{
  if (shell == NULL)
    return FALSE;

  return strcmp (shell, "bash") == 0 || g_str_has_suffix (shell, "/bash") ||
         strcmp (shell, "fish") == 0 || g_str_has_suffix (shell, "/fish") ||
         strcmp (shell, "zsh") == 0 || g_str_has_suffix (shell, "/zsh") ||
         strcmp (shell, "sh") == 0 || g_str_has_suffix (shell, "/sh");
}

gboolean
ide_shell_supports_dash_login (const char *shell)
{
  if (shell == NULL)
    return FALSE;

  return strcmp (shell, "bash") == 0 || g_str_has_suffix (shell, "/bash") ||
         strcmp (shell, "fish") == 0 || g_str_has_suffix (shell, "/fish") ||
         strcmp (shell, "zsh") == 0 || g_str_has_suffix (shell, "/zsh") ||
         strcmp (shell, "sh") == 0 || g_str_has_suffix (shell, "/sh");
}
