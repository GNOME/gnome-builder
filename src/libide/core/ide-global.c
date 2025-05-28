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

#include <glib/gi18n.h>
#include <string.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <wordexp.h>

#include <gio/gio.h>
#define G_SETTINGS_ENABLE_BACKEND
#include <gio/gsettingsbackend.h>

#include "../../gconstructor.h"

#include "ide-debug.h"
#include "ide-global.h"
#include "ide-macros.h"
#include "ide-private.h"

static GThread *main_thread;
static const char *application_id = "org.gnome.Builder";
static IdeProcessKind kind = IDE_PROCESS_KIND_HOST;
static GSettings *g_settings;
static G_LOCK_DEFINE (projects_directory);
static char *projects_directory;

#if defined (G_HAS_CONSTRUCTORS)
# ifdef G_DEFINE_CONSTRUCTOR_NEEDS_PRAGMA
#  pragma G_DEFINE_CONSTRUCTOR_PRAGMA_ARGS(ide_init_ctor)
# endif
G_DEFINE_CONSTRUCTOR(ide_init_ctor)
#else
# error Your platform/compiler is missing constructor support
#endif

static void  on_projects_directory_changed_cb (GSettings   *settings,
                                               const gchar *key,
                                               gpointer     user_data);
static char *dup_projects_dir                 (GSettings   *settings);

static gboolean
has_schema_installed (const char *schema_id)
{
  GSettingsSchemaSource *source = g_settings_schema_source_get_default ();
  g_autoptr(GSettingsSchema) schema = g_settings_schema_source_lookup (source, schema_id, TRUE);

  return !!schema;
}

static void
ide_init_ctor (void)
{
  main_thread = g_thread_self ();

  if (g_file_test ("/.flatpak-info", G_FILE_TEST_EXISTS))
    kind = IDE_PROCESS_KIND_FLATPAK;

  /* Get projects directory on main-thread at startup so that we
   * can be certain GSettings is created on main-thread.
   *
   * Skip this if the GSettings is not yet installed as will be
   * the case if we're doing introspection discovery.
   */
  if (has_schema_installed ("org.gnome.builder"))
    {
      g_settings = g_settings_new ("org.gnome.builder");
      g_signal_connect (g_settings,
                        "changed::projects-directory",
                        G_CALLBACK (on_projects_directory_changed_cb),
                        NULL);
      projects_directory = dup_projects_dir (g_settings);
    }
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

#ifdef IDE_ENABLE_TRACE
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
#endif

void
_ide_trace_log (GLogLevelFlags  log_level,
                const gchar    *domain,
                const gchar    *message)
{
  if (trace_vtable.log)
    trace_vtable.log (log_level, domain, message);
}

static gchar **
get_environ_from_stdout (GSubprocess *subprocess)
{
  g_autofree gchar *stdout_buf = NULL;

  if (g_subprocess_communicate_utf8 (subprocess, NULL, NULL, &stdout_buf, NULL, NULL))
    {
      g_auto(GStrv) lines = g_strsplit (stdout_buf, "\n", 0);
      g_autoptr(GPtrArray) env = g_ptr_array_new_with_free_func (g_free);

      for (guint i = 0; lines[i]; i++)
        {
          const char *line = lines[i];

          if (!g_ascii_isalpha (*line) && *line != '_')
            continue;

          for (const char *iter = line; *iter; iter = g_utf8_next_char (iter))
            {
              if (*iter == '=')
                {
                  g_ptr_array_add (env, g_strdup (line));
                  break;
                }

              if (!g_ascii_isalnum (*iter) && *iter != '_')
                break;
            }
        }

      if (env->len > 0)
        {
          g_ptr_array_add (env, NULL);
          return (gchar **)g_ptr_array_free (g_steal_pointer (&env), FALSE);
        }
    }

  return NULL;
}

const gchar * const *
_ide_host_environ (void)
{
  static gchar **host_environ;

  if (host_environ == NULL)
    {
      if (ide_is_flatpak ())
        {
          g_autoptr(GSubprocessLauncher) launcher = NULL;
          g_autoptr(GSubprocess) subprocess = NULL;
          g_autoptr(GError) error = NULL;

          launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);
          subprocess = g_subprocess_launcher_spawn (launcher, &error,
                                                    "flatpak-spawn", "--host", "printenv", NULL);
          if (subprocess != NULL)
            host_environ = get_environ_from_stdout (subprocess);
        }

      if (host_environ == NULL)
        host_environ = g_get_environ ();
    }

  return (const char * const *)host_environ;
}

/**
 * ide_path_expand:
 *
 * This function will expand various "shell-like" features of the provided
 * path using the POSIX wordexp(3) function. Command substitution will
 * not be enabled, but path features such as ~user will be expanded.
 *
 * Returns: (transfer full): A newly allocated string containing the
 *   expansion. A copy of the input string upon failure to expand.
 */
gchar *
ide_path_expand (const gchar *path)
{
  wordexp_t state = { 0 };
  char *replace_home = NULL;
  char *ret = NULL;
  char *escaped;
  int r;

  if (path == NULL)
    return NULL;

  /* Special case some path prefixes */
  if (path[0] == '~')
    {
      if (path[1] == 0)
        path = g_get_home_dir ();
      else if (path[1] == G_DIR_SEPARATOR)
        path = replace_home = g_strdup_printf ("%s%s", g_get_home_dir (), &path[1]);
    }
  else if (strncmp (path, "$HOME", 5) == 0)
    {
      if (path[5] == 0)
        path = g_get_home_dir ();
      else if (path[5] == G_DIR_SEPARATOR)
        path = replace_home = g_strdup_printf ("%s%s", g_get_home_dir (), &path[5]);
    }

  escaped = g_shell_quote (path);
  r = wordexp (escaped, &state, WRDE_NOCMD);
  if (r == 0 && state.we_wordc > 0)
    ret = g_strdup (state.we_wordv [0]);
  wordfree (&state);

  if (!g_path_is_absolute (ret))
    {
      g_autofree gchar *freeme = ret;

      ret = g_build_filename (g_get_home_dir (), freeme, NULL);
    }

  g_free (replace_home);
  g_free (escaped);

  return ret;
}

/**
 * ide_path_collapse:
 *
 * This function will collapse a path that starts with the users home
 * directory into a shorthand notation using ~/ for the home directory.
 *
 * If the path does not have the home directory as a prefix, it will
 * simply return a copy of @path.
 *
 * Returns: (transfer full): A new path, possibly collapsed.
 */
gchar *
ide_path_collapse (const gchar *path)
{
  g_autofree gchar *expanded = NULL;

  if (path == NULL)
    return NULL;

  expanded = ide_path_expand (path);

  if (g_str_has_prefix (expanded, g_get_home_dir ()))
    return g_build_filename ("~",
                             expanded + strlen (g_get_home_dir ()),
                             NULL);

  return g_steal_pointer (&expanded);
}

static char *
dup_projects_dir (GSettings *settings)
{
  g_autofree char *dir = NULL;
  g_autofree char *expanded = NULL;
  g_autofree char *projects = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (G_IS_SETTINGS (settings));

  dir = g_settings_get_string (g_settings, "projects-directory");
  expanded = ide_path_expand (dir);

  if (g_file_test (expanded, G_FILE_TEST_IS_DIR))
    return g_steal_pointer (&expanded);

  projects = g_build_filename (g_get_home_dir (), "Projects", NULL);

  if (g_file_test (projects, G_FILE_TEST_IS_DIR))
    return g_steal_pointer (&projects);

  if (!ide_str_empty0 (dir) && !ide_str_empty0 (expanded))
    return g_steal_pointer (&expanded);

  return g_build_filename (g_get_home_dir (), _("Projects"), NULL);
}

static void
on_projects_directory_changed_cb (GSettings   *settings,
                                  const gchar *key,
                                  gpointer     user_data)
{
  g_autofree char *new_dir = NULL;

  g_assert (G_IS_SETTINGS (settings));
  g_assert (key != NULL);

  new_dir = dup_projects_dir (settings);

  G_LOCK (projects_directory);
  g_set_str (&projects_directory, new_dir);
  G_UNLOCK (projects_directory);
}

/**
 * ide_dup_projects_dir:
 *
 * Like ide_get_projects_dir() but may be called from threads.
 *
 * Gets the directory to store projects within.
 *
 * First, this checks GSettings for a directory. If that directory exists,
 * it is returned.
 *
 * If not, it then checks for the non-translated name "Projects" in the
 * users home directory. If it exists, that is returned.
 *
 * If that does not exist, and a GSetting path existed, but was non-existant
 * that is returned.
 *
 * If the GSetting was empty, the translated name "Projects" is returned.
 *
 * Returns: (transfer full): a string containing the projects dir
 */
char *
ide_dup_projects_dir (void)
{
  char *copy;

  G_LOCK (projects_directory);
  g_assert (projects_directory != NULL);
  copy = g_strdup (projects_directory);
  G_UNLOCK (projects_directory);

  g_assert (copy != NULL);

  return copy;
}

/**
 * ide_get_projects_dir:
 *
 * Gets the directory to store projects within.
 *
 * First, this checks GSettings for a directory. If that directory exists,
 * it is returned.
 *
 * If not, it then checks for the non-translated name "Projects" in the
 * users home directory. If it exists, that is returned.
 *
 * If that does not exist, and a GSetting path existed, but was non-existant
 * that is returned.
 *
 * If the GSetting was empty, the translated name "Projects" is returned.
 *
 * Returns: (not nullable) (transfer full): a #GFile
 */
const char *
ide_get_projects_dir (void)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (G_IS_SETTINGS (g_settings), NULL);

  return projects_directory;
}

static char *
ide_dup_default_cache_dir_internal (void)
{
  g_autoptr(GSettings) settings = g_settings_new_with_path ("org.gnome.builder.project", "/org/gnome/builder/projects/");
  g_autofree char *cache_dir = g_settings_get_string (settings, "cache-root");
  g_autofree char *projects_dir = ide_dup_projects_dir ();

  if (!ide_str_empty0 (cache_dir))
    return g_steal_pointer (&cache_dir);

  return g_build_filename (projects_dir, ".gnome-builder", NULL);
}

char *
ide_dup_default_cache_dir (void)
{
  char *default_cache_dir = ide_dup_default_cache_dir_internal ();
  static gboolean initialized;

  if (!initialized)
    {
      g_autofree char *cachedir_tag = NULL;

      if (!g_file_test (default_cache_dir, G_FILE_TEST_EXISTS))
        g_mkdir_with_parents (default_cache_dir, 0750);

      /* Tell backup systems to ignore this directory */
      cachedir_tag = g_build_filename (default_cache_dir, "CACHEDIR.TAG", NULL);
      g_file_set_contents (cachedir_tag,
                           "Signature: 8a477f597d28d172789f06886806bc55\n",
                           -1,
                           NULL);

      initialized = TRUE;
    }

  return default_cache_dir;
}

/**
 * ide_get_gir_repository: (skip)
 *
 * Returns: (transfer none):
 */
GIRepository *
ide_get_gir_repository (void)
{
  static GIRepository *instance;

  if (instance == NULL)
#if GLIB_CHECK_VERSION(2, 85, 0)
    instance = gi_repository_dup_default ();
#else
    instance = gi_repository_new ();
#endif

  return instance;
}
