/* ipc-flatpak-util.h
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

#include <gio/gio.h>

G_BEGIN_DECLS

#define RUNTIME_VARIANT_STRING       "(sssssssb)"
#define RUNTIME_VARIANT_TYPE         G_VARIANT_TYPE(RUNTIME_VARIANT_STRING)
#define RUNTIME_ARRAY_VARIANT_STRING "a"RUNTIME_VARIANT_STRING
#define RUNTIME_ARRAY_VARIANT_TYPE   G_VARIANT_TYPE("a" RUNTIME_VARIANT_STRING)

static inline gboolean
complete_wrapped_error (GDBusMethodInvocation *invocation,
                        const GError          *error)
{
  g_autoptr(GError) wrapped = NULL;

  wrapped = g_error_new (G_IO_ERROR,
                         G_IO_ERROR_FAILED,
                         "The operation failed. The original error was \"%s\"",
                         error->message);
  g_dbus_method_invocation_return_gerror (invocation, wrapped);

  return TRUE;
}

static inline GVariant *
runtime_variant_new (const char *name,
                     const char *arch,
                     const char *branch,
                     const char *sdk_name,
                     const char *sdk_branch,
                     const char *deploy_dir,
                     const char *metadata,
                     gboolean    is_extension)
{
  return g_variant_take_ref (g_variant_new (RUNTIME_VARIANT_STRING,
                                            name,
                                            arch,
                                            branch,
                                            sdk_name,
                                            sdk_branch,
                                            deploy_dir,
                                            metadata,
                                            is_extension));
}

static inline gboolean
runtime_variant_parse (GVariant    *variant,
                       const char **name,
                       const char **arch,
                       const char **branch,
                       const char **sdk_name,
                       const char **sdk_branch,
                       const char **deploy_dir,
                       const char **metadata,
                       gboolean    *is_extension)
{
  if (variant == NULL)
    return FALSE;

  if (!g_variant_is_of_type (variant, RUNTIME_VARIANT_TYPE))
    return FALSE;

  g_variant_get (variant, "(&s&s&s&s&s&s&sb)",
                 name, arch, branch, sdk_name, sdk_branch, deploy_dir, metadata, is_extension);

  return TRUE;
}

G_END_DECLS
