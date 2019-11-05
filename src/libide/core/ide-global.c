/* ide-global.c
 *
 * Copyright 2014-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-global"

#include "config.h"

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <string.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "../../gconstructor.h"

#include "ide-debug.h"
#include "ide-global.h"
#include "ide-macros.h"
#include "ide-private.h"

static GThread *main_thread;
static const gchar *application_id = "org.gnome.Builder";
static IdeProcessKind kind = IDE_PROCESS_KIND_HOST;

#if defined (G_HAS_CONSTRUCTORS)
# ifdef G_DEFINE_CONSTRUCTOR_NEEDS_PRAGMA
#  pragma G_DEFINE_CONSTRUCTOR_PRAGMA_ARGS(ide_init_ctor)
# endif
G_DEFINE_CONSTRUCTOR(ide_init_ctor)
#else
# error Your platform/compiler is missing constructor support
#endif

static void
ide_init_ctor (void)
{
  main_thread = g_thread_self ();

  if (g_file_test ("/.flatpak-info", G_FILE_TEST_EXISTS))
    kind = IDE_PROCESS_KIND_FLATPAK;
}

/**
 * ide_get_main_thread
 *
 * Gets #GThread of the main thread.
 *
 * Generally this is used by macros to determine what thread they code is
 * currently running within.
 *
 * Returns: (transfer none): a #GThread
 *
 * Since: 3.32
 */
GThread *
ide_get_main_thread (void)
{
  return main_thread;
}

/**
 * ide_get_process_kind:
 *
 * Gets the kind of process we're running as.
 *
 * Returns: an #IdeProcessKind
 *
 * Since: 3.32
 */
IdeProcessKind
ide_get_process_kind (void)
{
  return kind;
}

const gchar *
ide_get_application_id (void)
{
  return application_id;
}

/**
 * ide_set_application_id:
 * @app_id: the application id
 *
 * Sets the application id that will be used.
 *
 * This must be set at application startup before any GApplication
 * has connected to the D-Bus.
 *
 * The default is "org.gnome.Builder".
 *
 * Since: 3.32
 */
void
ide_set_application_id (const gchar *app_id)
{
  g_return_if_fail (app_id != NULL);

  application_id = g_intern_string (app_id);
}

const gchar *
ide_get_program_name (void)
{
  return "gnome-builder";
}

gchar *
ide_create_host_triplet (const gchar *arch,
                         const gchar *kernel,
                         const gchar *system)
{
  if (arch == NULL || kernel == NULL)
    return g_strdup (ide_get_system_type ());
  else if (system == NULL)
    return g_strdup_printf ("%s-%s", arch, kernel);
  else
    return g_strdup_printf ("%s-%s-%s", arch, kernel, system);
}

const gchar *
ide_get_system_type (void)
{
  static gchar *system_type;
  g_autofree gchar *os_lower = NULL;
  const gchar *machine = NULL;
  struct utsname u;

  if (system_type != NULL)
    return system_type;

  if (uname (&u) < 0)
    return g_strdup ("unknown");

  os_lower = g_utf8_strdown (u.sysname, -1);

  /* config.sub doesn't accept amd64-OS */
  machine = strcmp (u.machine, "amd64") ? u.machine : "x86_64";

  /*
   * TODO: Clearly we want to discover "gnu", but that should be just fine
   *       for a default until we try to actually run on something non-gnu.
   *       Which seems unlikely at the moment. If you run FreeBSD, you can
   *       probably fix this for me :-) And while you're at it, make the
   *       uname() call more portable.
   */

#ifdef __GLIBC__
  system_type = g_strdup_printf ("%s-%s-%s", machine, os_lower, "gnu");
#else
  system_type = g_strdup_printf ("%s-%s", machine, os_lower);
#endif

  return system_type;
}

gchar *
ide_get_system_arch (void)
{
  static GHashTable *remap;
  const char *machine;
  struct utsname u;

  if (uname (&u) < 0)
    return g_strdup ("unknown");

  if (g_once_init_enter (&remap))
    {
      GHashTable *mapping;

      mapping = g_hash_table_new (g_str_hash, g_str_equal);
      g_hash_table_insert (mapping, (gchar *)"amd64", (gchar *)"x86_64");
      g_hash_table_insert (mapping, (gchar *)"armv7l", (gchar *)"aarch64");
      g_hash_table_insert (mapping, (gchar *)"i686", (gchar *)"i386");

      g_once_init_leave (&remap, mapping);
    }

  if (g_hash_table_lookup_extended (remap, u.machine, NULL, (gpointer *)&machine))
    return g_strdup (machine);
  else
    return g_strdup (u.machine);
}

gsize
ide_get_system_page_size (void)
{
  return sysconf (_SC_PAGE_SIZE);
}

static gchar *
get_base_path (const gchar *name)
{
  g_autoptr(GKeyFile) keyfile = g_key_file_new ();

  if (g_key_file_load_from_file (keyfile, "/.flatpak-info", 0, NULL))
    return g_key_file_get_string (keyfile, "Instance", name, NULL);

  return NULL;
}

/**
 * ide_get_relocatable_path:
 * @path: a relocatable path
 *
 * Gets the path to a resource that may be relocatable at runtime.
 *
 * Returns: (transfer full): a new string containing the path
 *
 * Since: 3.32
 */
gchar *
ide_get_relocatable_path (const gchar *path)
{
  static gchar *base_path;

  if G_UNLIKELY (base_path == NULL)
    base_path = get_base_path ("app-path");

  return g_build_filename (base_path, path, NULL);
}

const gchar *
ide_gettext (const gchar *message)
{
  if (message != NULL)
    return g_dgettext (GETTEXT_PACKAGE, message);
  return NULL;
}

static IdeTraceVTable trace_vtable;

void
_ide_trace_init (IdeTraceVTable *vtable)
{
  trace_vtable = *vtable;
  if (trace_vtable.load)
    trace_vtable.load ();
}

void
_ide_trace_shutdown (void)
{
  if (trace_vtable.unload)
    trace_vtable.unload ();
  memset (&trace_vtable, 0, sizeof trace_vtable);
}

void
ide_trace_function (const gchar *strfunc,
                    gint64       begin_time_usec,
                    gint64       end_time_usec)
{
  /* In case our clock is not reliable */
  if (end_time_usec < begin_time_usec)
    end_time_usec = begin_time_usec;

  if (trace_vtable.function)
    trace_vtable.function (strfunc, begin_time_usec, end_time_usec);
}

void
_ide_trace_log (GLogLevelFlags  log_level,
                const gchar    *domain,
                const gchar    *message)
{
  if (trace_vtable.log)
    trace_vtable.log (log_level, domain, message);
}
