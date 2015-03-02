/* ide-makecache.c
 *
 * Copyright (C) 2013 Jesse van den Kieboom <jessevdk@gnome.org>
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is based upon makefileintegration.py from gnome-code-assistance.
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

#define G_LOG_DOMAIN "ide-makecache"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <unistd.h>

#include "ide-context.h"
#include "ide-debug.h"
#include "ide-global.h"
#include "ide-makecache.h"
#include "ide-project.h"

#define FAKE_CC  "__LIBIDE_FAKE_CC__"
#define FAKE_CXX "__LIBIDE_FAKE_CXX__"

struct _IdeMakecache
{
  IdeObject parent_instance;

  /* Immutable after instance creation */
  GFile       *makefile;
  GFile       *parent;
  gchar       *llvm_flags;
  GMappedFile *mapped;

  /* Mutable, but only available from main thread */
  GHashTable  *file_targets_cache;

  /* Mutable, any thread, lock before reading or writing */
  GMutex       mutex;
  GHashTable  *file_flags_cache;
  GHashTable  *file_targets_neg_cache;
};

typedef struct
{
  gchar **targets;
  gchar  *relative_path;
} FileFlagsLookup;

G_DEFINE_TYPE (IdeMakecache, ide_makecache, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_MAKEFILE,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

static void
file_flags_lookup_free (gpointer data)
{
  FileFlagsLookup *lookup = data;

  if (data)
    {
      g_strfreev (lookup->targets);
      g_free (lookup->relative_path);
      g_free (lookup);
    }
}

static void
ide_makecache_discover_llvm_flags_worker (GTask        *task,
                                          gpointer      source_object,
                                          gpointer      task_data,
                                          GCancellable *cancellable)
{
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autofree gchar *stdoutstr = NULL;
  gchar *include_path = NULL;
  GError *error = NULL;

  IDE_ENTRY;

  g_assert (G_IS_TASK (task));

  IDE_TRACE_MSG ("Spawning 'clang -print-file-name=include'");

  subprocess = g_subprocess_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE,
                                 &error,
                                 "clang",
                                 "-print-file-name=include",
                                 NULL);

  if (!subprocess)
    {
      g_task_return_error (task, error);
      IDE_EXIT;
    }

  if (!g_subprocess_communicate_utf8 (subprocess, NULL, cancellable, &stdoutstr, NULL, &error))
    {
      g_task_return_error (task, error);
      IDE_EXIT;
    }

  g_strstrip (stdoutstr);

  IDE_TRACE_MSG ("Clang Result: %s", stdoutstr);

  if (g_str_equal (stdoutstr, "include"))
    {
      g_task_return_pointer (task, NULL, NULL);
      IDE_EXIT;
    }

  include_path = g_strdup_printf ("-I%s", stdoutstr);
  g_task_return_pointer (task, include_path, g_free);

  IDE_EXIT;
}

static void
ide_makecache_discover_llvm_flags_async (IdeMakecache        *self,
                                         GCancellable        *cancellable,
                                         GAsyncReadyCallback  callback,
                                         gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAKECACHE (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_run_in_thread (task, ide_makecache_discover_llvm_flags_worker);

  IDE_EXIT;
}

static gchar *
ide_makecache_discover_llvm_flags_finish (IdeMakecache  *self,
                                          GAsyncResult  *result,
                                          GError       **error)
{
  GTask *task = (GTask *)result;
  gchar *ret;

  IDE_ENTRY;

  g_assert (IDE_IS_MAKECACHE (self));
  g_assert (G_IS_TASK (task));

  ret = g_task_propagate_pointer (task, error);

  IDE_RETURN (ret);
}

static gboolean
is_target_interesting (const gchar *target)
{
  return ((target [0] != '#') &&
          (target [0] != '.') &&
          (g_str_has_suffix (target, ".lo") ||
           g_str_has_suffix (target, ".o")));
}

static const gchar * const *
ide_makecache_get_file_targets_cached (IdeMakecache *self,
                                       const gchar  *path)
{
  const gchar * const *ret;

  IDE_ENTRY;

  g_assert (IDE_IS_MAKECACHE (self));
  g_assert (path);

  ret = g_hash_table_lookup (self->file_targets_cache, path);

  IDE_DEBUG ("File targets cache %s for %s.", ret ? "hit" : "miss", path);

  IDE_RETURN (ret);
}

static const gchar * const *
ide_makecache_get_file_targets_searched (IdeMakecache *self,
                                         const gchar  *path)
{
  const gchar *content;
  g_autoptr(GRegex) regex = NULL;
  g_autofree gchar *escaped = NULL;
  g_autofree gchar *regexstr = NULL;
  g_autoptr(GMatchInfo) match_info = NULL;
  g_autoptr(GHashTable) found = NULL;
  g_autoptr(GPtrArray) targets = NULL;
  gsize len;

  IDE_ENTRY;

  g_assert (IDE_IS_MAKECACHE (self));
  g_assert (path);

  escaped = g_regex_escape_string (path, -1);
  regexstr = g_strdup_printf ("^([^:\n ]+):.*\\b(%s)\\b", escaped);

  regex = g_regex_new (regexstr, G_REGEX_MULTILINE, 0, NULL);
  if (!regex)
    IDE_RETURN (NULL);

  content = g_mapped_file_get_contents (self->mapped);
  len = g_mapped_file_get_length (self->mapped);

  targets = g_ptr_array_new_with_free_func (g_free);
  found = g_hash_table_new (g_str_hash, g_str_equal);

#ifndef IDE_DISABLE_TRACE
  {
    gchar *fmtsize;

    fmtsize = g_format_size (len);
    IDE_TRACE_MSG ("Beginning regex lookup across %s of UTF-8 text", fmtsize);
    g_free (fmtsize);
  }
#endif

  if (g_regex_match_full (regex, content, len, 0, 0, &match_info, NULL))
    {
      while (g_match_info_matches (match_info))
        {
          g_autofree gchar *target = NULL;

          target = g_match_info_fetch (match_info, 1);

          if (is_target_interesting (target) && !g_hash_table_contains (found, target))
            {
              g_hash_table_insert (found, target, NULL);
              g_ptr_array_add (targets, target);
              target = NULL;
            }

          g_match_info_next (match_info, NULL);
        }
    }

  IDE_TRACE_MSG ("Regex scan complete");

  if (targets->len)
    {
      gchar **ret;

      g_ptr_array_add (targets, NULL);
      ret = (gchar **)g_ptr_array_free (targets, FALSE);

#ifndef IDE_DISABLE_TRACE
      {
        gchar *targetsstr;

        targetsstr = g_strjoinv (" ", ret);
        IDE_TRACE_MSG ("File \"%s\" found in targets: %s", path, targetsstr);
        g_free (targetsstr);
      }
#endif

      g_hash_table_insert (self->file_targets_cache, g_strdup (path), ret);

      IDE_RETURN ((const gchar * const *)ret);
    }

  IDE_RETURN (NULL);
}

static gboolean
ide_makecache_validate_mapped_file (GMappedFile  *mapped,
                                    GError      **error)
{
  const gchar *contents;
  gsize len;

  IDE_ENTRY;

  g_assert (mapped);
  g_assert (error);
  g_assert (!*error);

  IDE_DEBUG ("Validating makecache");

  contents = g_mapped_file_get_contents (mapped);

  if (contents == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "GMappedFile returned NULL contents");
      IDE_RETURN (FALSE);
    }

  len = g_mapped_file_get_length (mapped);

  if (len == 0)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "GMappedFile returned zero length");
      IDE_RETURN (FALSE);
    }

  if (!g_utf8_validate (contents, len, NULL))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "mapped file contains invalid UTF-8");
      IDE_RETURN (FALSE);
    }

  IDE_RETURN (TRUE);
}

static int
ide_makecache_open_temp (IdeMakecache  *self,
                         gchar        **name_used,
                         GError       **error)
{
  IdeContext *context;
  IdeProject *project;
  const gchar *project_name;
  g_autofree gchar *name = NULL;
  g_autofree gchar *path = NULL;
  g_autofree gchar *directory = NULL;
  time_t now;
  int fd;

  IDE_ENTRY;

  g_assert (IDE_IS_MAKECACHE (self));
  g_assert (name_used);
  g_assert (error);
  g_assert (!*error);

  context = ide_object_get_context (IDE_OBJECT (self));
  project = ide_context_get_project (context);
  project_name = ide_project_get_name (project);

  directory = g_build_filename (g_get_user_cache_dir (),
                                ide_get_program_name (),
                                "makecache",
                                NULL);

  IDE_DEBUG ("Using \"%s\" for makecache directory", directory);

  if (g_mkdir_with_parents (directory, 0700) != 0)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   g_io_error_from_errno (errno),
                   "Failed to create makecache directory");
      IDE_RETURN (-1);
    }

  now = time (NULL);
  name = g_strdup_printf ("%s.makecache.tmp-%u", project_name, (guint)now);
  path = g_build_filename (directory, name, NULL);

  IDE_DEBUG ("Creating temporary makecache at \"%s\"", path);

  fd = g_open (path, O_CREAT|O_RDWR, 0600);

  if (fd == -1)
    {
      *name_used = NULL;
      g_set_error (error,
                   G_IO_ERROR,
                   g_io_error_from_errno (errno),
                   "Failed to open temporary file: %s",
                   g_strerror (errno));
      IDE_RETURN (-1);
    }

  *name_used = g_strdup (path);

  IDE_RETURN (fd);
}

static void
ide_makecache_new_worker (GTask        *task,
                          gpointer      source_object,
                          gpointer      task_data,
                          GCancellable *cancellable)
{
  IdeMakecache *self = source_object;
  IdeContext *context;
  IdeProject *project;
  const gchar *project_name;
  g_autofree gchar *name_used = NULL;
  g_autofree gchar *name = NULL;
  g_autofree gchar *cache_path = NULL;
  g_autoptr(GFile) parent = NULL;
  g_autofree gchar *workdir = NULL;
  g_autoptr(GMappedFile) mapped = NULL;
  g_autoptr(GSubprocessLauncher) launcher = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  GError *error = NULL;
  int fdcopy;
  int fd;

  IDE_ENTRY;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_MAKECACHE (self));

  if (!self->makefile || !(parent = g_file_get_parent (self->makefile)))
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_FILENAME,
                               "No makefile was specified.");
      IDE_EXIT;
    }

  workdir = g_file_get_path (parent);

  if (!workdir)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_FILENAME,
                               "Makefile must be accessable on local filesystem.");
      IDE_EXIT;
    }

  context = ide_object_get_context (IDE_OBJECT (self));
  project = ide_context_get_project (context);
  project_name = ide_project_get_name (project);
  name = g_strdup_printf ("%s.makecache", project_name);
  cache_path = g_build_filename (g_get_user_cache_dir (),
                                 ide_get_program_name (),
                                 "makecache",
                                 name,
                                 NULL);

 /*
  * NOTE:
  *
  * The makecache file is a file that contains all of the output from `make -p -n -s` on an
  * automake project. This contains everything we need to determine what make targets a file
  * "belongs to".
  *
  * The process is as follows.
  *
  * 1) Open a new temporary file to contain the output. This needs to be in the same directory
  *    as the target so that we can rename() it into place.
  * 2) dup() the fd to pass to the child process.
  * 3) Spawn `make -p -n -s` using the temporary file as stdout.
  * 4) Wait for the subprocess to complete. It would be nice if we could do this asynchronously,
  *    but we'd need to break this whole thing into more tasks.
  * 5) Move the temporary file into position at ~/.cache/<prgname>/<project>.makecache
  * 6) mmap() the cache file using g_mapped_file_new_from_fd().
  * 7) Close the fd. This does NOT cause the mmap() region to be unmapped.
  * 8) Validate the mmap() contents with g_utf8_validate().
  */

  /*
   * Step 1, open our temporary file.
   */
  fd = ide_makecache_open_temp (self, &name_used, &error);

  if (fd == -1)
    {
      g_task_return_error (task, error);
      IDE_EXIT;
    }

  /*
   * Step 2, make an extra fd to be passed to the child process.
   */
  fdcopy = dup (fd);

  if (fdcopy == -1)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               g_io_error_from_errno (errno),
                               "Failed to open temporary file: %s",
                               g_strerror (errno));
      close (fd);
      IDE_EXIT;
    }

  /*
   * Step 3,
   *
   * Spawn `make -p -n -s` in the directory containing our makefile.
   */
  launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_NONE);
  g_subprocess_launcher_set_cwd (launcher, workdir);
  g_subprocess_launcher_take_stdout_fd (launcher, fdcopy);
  subprocess = g_subprocess_launcher_spawn (launcher, &error, "make", "-p", "-n", "-s", NULL);

  if (!subprocess)
    {
      g_task_return_error (task, error);
      close (fd);
      IDE_EXIT;
    }

  /*
   * Step 4, wait for the subprocess to complete.
   */
  if (!g_subprocess_wait (subprocess, cancellable, &error))
    {
      g_task_return_error (task, error);
      close (fd);
      IDE_EXIT;
    }

  /*
   * Step 5, move the file into location at the cache path.
   *
   * TODO:
   *
   * If we can switch to O_TMPFILE and use renameat2(), that would be neat. I'm not sure that is
   * possible though since the O_TMPFILE will not have a filename associated with it.
   */
  if (0 != g_rename (name_used, cache_path))
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               g_io_error_from_errno (errno),
                               "Failed to move makecache into target directory: %s",
                               g_strerror (errno));
      close (fd);
      IDE_EXIT;
    }

  /*
   * Step 6, map the makecache file into memory.
   */
  mapped = g_mapped_file_new_from_fd (fd, FALSE, &error);

  if (!mapped)
    {
      g_task_return_error (task, error);
      close (fd);
      IDE_EXIT;
    }

  /*
   * Step 7, we are done with fd, so close it now. Note that this does not have an effect on the
   * mmap() region.
   */
  close (fd);

  /*
   * Step 8, validate the contents of the mmap region.
   */
  if (!ide_makecache_validate_mapped_file (mapped, &error))
    {
      g_task_return_error (task, error);
      IDE_EXIT;
    }

  /*
   * Step 9, save the mmap for future use.
   */
  self->mapped = g_mapped_file_ref (mapped);

  g_task_return_pointer (task, g_object_ref (self), g_object_unref);

  IDE_EXIT;
}

static void
ide_makecache_parse_c_cxx (IdeMakecache *self,
                           const gchar  *line,
                           GPtrArray    *ret)
{
  gint argc = 0;
  gchar **argv = NULL;
  gboolean in_expand = FALSE;
  gsize i;

  g_assert (line);
  g_assert (ret);

  while (isspace (*line))
    line++;

  if (!g_shell_parse_argv (line, &argc, &argv, NULL))
    return;

  g_ptr_array_add (ret, g_strdup (self->llvm_flags));

  for (i = 0; i < argc; i++)
    {
      const gchar *flag = argv [i];

      if (strchr (flag, '`'))
        in_expand = !in_expand;

      if (in_expand || strlen (flag) < 2)
        continue;

      switch (flag [1])
        {
        case 'I': /* -I./includes/ -I ./includes/ */
          g_ptr_array_add (ret, g_strdup (flag));
          if ((strlen (flag) == 2) && (i < (argc - 1)))
            g_ptr_array_add (ret, g_strdup (argv [++i]));
          break;

        case 'f': /* -fPIC... */
        case 'W': /* -Werror... */
        case 'm': /* -m64 -mtune=native */
          g_ptr_array_add (ret, g_strdup (flag));
          break;

        case 'D': /* -Dfoo -D foo */
        case 'x': /* -xc++ */
          g_ptr_array_add (ret, g_strdup (flag));
          if ((strlen (flag) == 2) && (i < (argc - 1)))
            g_ptr_array_add (ret, g_strdup (argv [++i]));
          break;

        default:
          if (g_str_has_prefix (flag, "-std="))
            g_ptr_array_add (ret, g_strdup (flag));
          break;
        }
    }

  g_ptr_array_add (ret, NULL);
}

static gchar **
ide_makecache_parse_line (IdeMakecache *self,
                          const gchar  *line)
{
  GPtrArray *ret = NULL;
  const gchar *pos;

  IDE_ENTRY;

  g_assert (line);

  ret = g_ptr_array_new ();

  if ((pos = strstr (line, FAKE_CXX)))
    {
      gchar **strv;

      ide_makecache_parse_c_cxx (self, pos + strlen(FAKE_CXX), ret);
      g_ptr_array_add (ret, "-xc++");
      strv = (gchar **)g_ptr_array_free (ret, FALSE);
      IDE_RETURN (strv);
    }
  else if ((pos = strstr (line, FAKE_CC)))
    {
      gchar **strv;

      ide_makecache_parse_c_cxx (self, pos + strlen(FAKE_CC), ret);
      strv = (gchar **)g_ptr_array_free (ret, FALSE);
      IDE_RETURN (strv);
    }

  g_ptr_array_unref (ret);
  IDE_RETURN (NULL);
}

static void
ide_makecache_get_file_flags_worker (GTask        *task,
                                     gpointer      source_object,
                                     gpointer      task_data,
                                     GCancellable *cancellable)
{
  IdeMakecache *self = source_object;
  FileFlagsLookup *lookup = task_data;
  g_autoptr(GSubprocessLauncher) launcher = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autofree gchar *cwd = NULL;
  g_autoptr(GPtrArray) argv = NULL;
  g_autofree gchar *stdoutstr = NULL;
  GError *error = NULL;
  gchar **lines;
  gchar **ret = NULL;
  gsize i;

  IDE_ENTRY;

  g_assert (IDE_IS_MAKECACHE (self));
  g_assert (G_IS_TASK (task));
  g_assert (lookup);
  g_assert (lookup->relative_path);
  g_assert (lookup->targets);

  cwd = g_file_get_path (self->parent);

  argv = g_ptr_array_new ();
  g_ptr_array_add (argv, "make");
  g_ptr_array_add (argv, "-s");
  g_ptr_array_add (argv, "-i");
  g_ptr_array_add (argv, "-n");
  g_ptr_array_add (argv, "-W");
  g_ptr_array_add (argv, lookup->relative_path);
  for (i = 0; lookup->targets [i]; i++)
    g_ptr_array_add (argv, lookup->targets [i]);
  g_ptr_array_add (argv, "V=1");
  g_ptr_array_add (argv, "CC="FAKE_CC);
  g_ptr_array_add (argv, "CXX="FAKE_CXX);
  g_ptr_array_add (argv, NULL);

#ifndef IDE_DISABLE_TRACE
  {
    gchar *cmdline;

    cmdline = g_strjoinv (" ", (gchar **)argv->pdata);
    IDE_TRACE_MSG ("%s", cmdline);
    g_free (cmdline);
  }
#endif

  launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);
  g_subprocess_launcher_set_cwd (launcher, cwd);
  subprocess = g_subprocess_launcher_spawnv (launcher, (const gchar * const *)argv->pdata, &error);

  if (!subprocess)
    {
      g_task_return_error (task, error);
      return;
    }

  if (!g_subprocess_communicate_utf8 (subprocess, NULL, cancellable, &stdoutstr, NULL, &error))
    {
      g_task_return_error (task, error);
      return;
    }

  lines = g_strsplit (stdoutstr, "\n", 0);

  for (i = 0; lines [i]; i++)
    {
      const gchar *line = lines [i];

      if ((ret = ide_makecache_parse_line (self, line)))
        break;
    }

  g_strfreev (lines);

  if (!ret)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Failed to extract flags from make output");
      return;
    }

  g_mutex_lock (&self->mutex);
  g_hash_table_replace (self->file_flags_cache, g_strdup (lookup->relative_path), g_strdupv (ret));
  g_mutex_unlock (&self->mutex);

  g_task_return_pointer (task, ret, (GDestroyNotify)g_strfreev);
}

static void
ide_makecache_set_makefile (IdeMakecache *self,
                            GFile        *makefile)
{
  g_autoptr(GFile) parent = NULL;

  g_return_if_fail (IDE_IS_MAKECACHE (self));
  g_return_if_fail (G_IS_FILE (makefile));

  parent = g_file_get_parent (makefile);

  if (!parent)
    {
      g_warning (_("Invalid makefile provided, ignoring."));
      return;
    }

  g_set_object (&self->makefile, makefile);
  g_set_object (&self->parent, parent);
}

static void
ide_makecache_finalize (GObject *object)
{
  IdeMakecache *self = (IdeMakecache *)object;

  g_mutex_clear (&self->mutex);
  g_clear_object (&self->makefile);
  g_clear_pointer (&self->mapped, g_mapped_file_unref);
  g_clear_pointer (&self->file_targets_cache, g_hash_table_unref);
  g_clear_pointer (&self->file_targets_neg_cache, g_hash_table_unref);
  g_clear_pointer (&self->file_flags_cache, g_hash_table_unref);
  g_clear_pointer (&self->llvm_flags, g_free);

  G_OBJECT_CLASS (ide_makecache_parent_class)->finalize (object);
}

static void
ide_makecache_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  IdeMakecache *self = IDE_MAKECACHE (object);

  switch (prop_id)
    {
    case PROP_MAKEFILE:
      g_value_set_object (value, ide_makecache_get_makefile (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_makecache_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  IdeMakecache *self = IDE_MAKECACHE (object);

  switch (prop_id)
    {
    case PROP_MAKEFILE:
      ide_makecache_set_makefile (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_makecache_class_init (IdeMakecacheClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_makecache_finalize;
  object_class->get_property = ide_makecache_get_property;
  object_class->set_property = ide_makecache_set_property;

  gParamSpecs [PROP_MAKEFILE] =
    g_param_spec_object ("makefile",
                         _("Makefile"),
                         _("The root makefile to be cached."),
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_MAKEFILE,
                                   gParamSpecs [PROP_MAKEFILE]);
}

static void
ide_makecache_init (IdeMakecache *self)
{
  g_mutex_init (&self->mutex);
  self->file_targets_cache = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                                    (GDestroyNotify)g_strfreev);
  self->file_targets_neg_cache = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                                        (GDestroyNotify)g_strfreev);
  self->file_flags_cache = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                                  (GDestroyNotify)g_strfreev);
}

GFile *
ide_makecache_get_makefile (IdeMakecache *self)
{
  g_return_val_if_fail (IDE_IS_MAKECACHE (self), NULL);

  return self->makefile;
}

static void
ide_makecache__discover_llvm_flags_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  IdeMakecache *self = (IdeMakecache *)object;
  g_autoptr(GTask) task = user_data;
  gchar *flags;
  GError *error = NULL;

  flags = ide_makecache_discover_llvm_flags_finish (self, result, &error);

  if (error)
    {
      g_task_return_error (task, error);
      return;
    }

  self->llvm_flags = flags;

  g_task_run_in_thread (task, ide_makecache_new_worker);
}

void
ide_makecache_new_for_makefile_async (IdeContext          *context,
                                      GFile               *makefile,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  g_autoptr(IdeMakecache) self = NULL;

  g_return_if_fail (IDE_IS_CONTEXT (context));
  g_return_if_fail (G_IS_FILE (makefile));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  self = g_object_new (IDE_TYPE_MAKECACHE,
                       "context", context,
                       "makefile", makefile,
                       NULL);

  task = g_task_new (self, cancellable, callback, user_data);

  ide_makecache_discover_llvm_flags_async (self,
                                           cancellable,
                                           ide_makecache__discover_llvm_flags_cb,
                                           g_object_ref (task));
}

IdeMakecache *
ide_makecache_new_for_makefile_finish (GAsyncResult  *result,
                                       GError       **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (G_IS_TASK (task), NULL);

  return g_task_propagate_pointer (task, error);
}

static void
ide_makecache_get_file_targets_worker (GTask        *task,
                                       gpointer      source_object,
                                       gpointer      task_data,
                                       GCancellable *cancellable)
{
  IdeMakecache *self = source_object;
  const gchar *path = task_data;
  const gchar * const *ret;

  IDE_ENTRY;

  g_assert (IDE_IS_MAKECACHE (self));
  g_assert (G_IS_TASK (task));
  g_assert (path);

  ret = ide_makecache_get_file_targets_searched (self, path);

  if (!ret)
    {
      g_mutex_lock (&self->mutex);
      g_hash_table_insert (self->file_targets_neg_cache, g_strdup (path), NULL);
      g_mutex_unlock (&self->mutex);

      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_FOUND,
                               "target was not found in project");
      IDE_EXIT;
    }

  g_task_return_pointer (task, g_strdupv ((gchar **)ret), (GDestroyNotify)g_strfreev);

  IDE_EXIT;
}

void
ide_makecache_get_file_targets_async (IdeMakecache        *self,
                                      GFile               *file,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  const gchar * const *ret;
  gchar *path = NULL;
  gboolean neg_hit;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAKECACHE (self));
  g_return_if_fail (G_IS_FILE (file));

  task = g_task_new (self, cancellable, callback, user_data);

  path = g_file_get_relative_path (self->parent, file);
  g_task_set_task_data (task, path, g_free);

  if (!path)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_FILENAME,
                               "File must be in the project path.");
      IDE_EXIT;
    }

  g_mutex_lock (&self->mutex);
  neg_hit = g_hash_table_contains (self->file_targets_neg_cache, path);
  g_mutex_unlock (&self->mutex);

  if (neg_hit)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_FOUND,
                               "target could not be found");
      IDE_EXIT;
    }

  ret = ide_makecache_get_file_targets_cached (self, path);

  if (ret)
    {
      g_task_return_pointer (task, g_strdupv ((gchar **)ret), (GDestroyNotify)g_strfreev);
      IDE_EXIT;
    }

  g_task_run_in_thread (task, ide_makecache_get_file_targets_worker);

  IDE_EXIT;
}

gchar **
ide_makecache_get_file_targets_finish (IdeMakecache  *self,
                                       GAsyncResult  *result,
                                       GError       **error)
{
  GTask *task = (GTask *)result;
  gchar **ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_MAKECACHE (self), NULL);
  g_return_val_if_fail (G_IS_TASK (task), NULL);

  ret = g_task_propagate_pointer (task, error);

  IDE_RETURN (ret);
}

static void
ide_makecache__get_targets_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  IdeMakecache *self = (IdeMakecache *)object;
  g_autoptr(GTask) task = user_data;
  gchar **targets;
  GError *error = NULL;
  GFile *file;
  FileFlagsLookup *lookup;
  g_autofree gchar *path = NULL;
  g_autofree gchar *relative_path = NULL;
  gchar **argv;

  IDE_ENTRY;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_MAKECACHE (self));

  targets = ide_makecache_get_file_targets_finish (self, result, &error);

  if (!targets)
    {
      if (TRUE) /* TODO: Check for C/C++/Obj-C */
        {
          argv = g_new0 (gchar *, 2);
          argv [0] = g_strdup (self->llvm_flags);

          g_task_return_pointer (task, argv, (GDestroyNotify)g_strfreev);
          IDE_EXIT;
        }

      g_task_return_error (task, error);
      IDE_EXIT;
    }

  file = g_task_get_task_data (task);
  path = g_file_get_path (file);
  relative_path = g_file_get_relative_path (self->parent, file);

  g_mutex_lock (&self->mutex);
  argv = g_strdupv (g_hash_table_lookup (self->file_flags_cache, relative_path));
  g_mutex_unlock (&self->mutex);

  if (argv)
    {
      g_strfreev (targets);
      g_task_return_pointer (task, argv, (GDestroyNotify)g_strfreev);
      IDE_EXIT;
    }

  lookup = g_new0 (FileFlagsLookup, 1);
  lookup->targets = targets;
  lookup->relative_path = g_strdup (relative_path);

  g_task_set_task_data (task, lookup, file_flags_lookup_free);
  g_task_run_in_thread (task, ide_makecache_get_file_flags_worker);

  IDE_EXIT;
}

void
ide_makecache_get_file_flags_async (IdeMakecache        *self,
                                    GFile               *file,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAKECACHE (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (task, g_object_ref (file), g_object_unref);

  ide_makecache_get_file_targets_async (self,
                                        file,
                                        g_task_get_cancellable (task),
                                        ide_makecache__get_targets_cb,
                                        g_object_ref (task));

  IDE_EXIT;
}

gchar **
ide_makecache_get_file_flags_finish (IdeMakecache  *self,
                                     GAsyncResult  *result,
                                     GError       **error)
{
  GTask *task = (GTask *)result;
  gchar **ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_MAKECACHE (self), NULL);
  g_return_val_if_fail (G_IS_TASK (task), NULL);

  ret = g_task_propagate_pointer (task, error);

  IDE_RETURN (ret);
}
