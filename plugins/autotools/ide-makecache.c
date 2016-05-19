/* ide-makecache.c
 *
 * Copyright (C) 2013 Jesse van den Kieboom <jessevdk@gnome.org>
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

#define G_LOG_DOMAIN "ide-makecache"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "egg-counter.h"
#include "egg-task-cache.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <unistd.h>
#include <ide.h>

#include "ide-makecache.h"
#include "ide-makecache-target.h"

#define FAKE_CC    "__LIBIDE_FAKE_CC__"
#define FAKE_CXX   "__LIBIDE_FAKE_CXX__"
#define FAKE_VALAC "__LIBIDE_FAKE_VALAC__"

struct _IdeMakecache
{
  IdeObject    parent_instance;

  GFile        *makefile;
  GFile        *parent;
  gchar        *llvm_flags;
  GMappedFile  *mapped;
  EggTaskCache *file_targets_cache;
  EggTaskCache *file_flags_cache;
};

typedef struct
{
  IdeMakecache *self;
  GFile        *file;
  GPtrArray    *targets;
  gchar        *relative_path;
} FileFlagsLookup;

typedef struct
{
  GMappedFile *mapped;
  gchar       *path;
} FileTargetsLookup;

G_DEFINE_TYPE (IdeMakecache, ide_makecache, IDE_TYPE_OBJECT)

EGG_DEFINE_COUNTER (instances, "IdeMakecache", "Instances", "The number of IdeMakecache")

enum {
  PROP_0,
  PROP_MAKEFILE,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static void
file_flags_lookup_free (gpointer data)
{
  FileFlagsLookup *lookup = data;

  g_clear_object (&lookup->self);
  g_clear_object (&lookup->file);
  g_clear_pointer (&lookup->targets, g_ptr_array_unref);
  g_clear_pointer (&lookup->relative_path, g_free);
  g_slice_free (FileFlagsLookup, lookup);
}

static void
file_targets_lookup_free (gpointer data)
{
  FileTargetsLookup *lookup = data;

  g_clear_pointer (&lookup->path, g_free);
  g_clear_pointer (&lookup->mapped, g_mapped_file_unref);
  g_slice_free (FileTargetsLookup, lookup);
}

static gboolean
file_is_clangable (GFile *file)
{
  g_autofree gchar *name = NULL;

  name = g_strreverse (g_file_get_basename (file));

  return (g_str_has_prefix (name, "c.") ||
          g_str_has_prefix (name, "h.") ||
          g_str_has_prefix (name, "cc.") ||
          g_str_has_prefix (name, "hh.") ||
          g_str_has_prefix (name, "ppc.") ||
          g_str_has_prefix (name, "pph.") ||
          g_str_has_prefix (name, "xxc.") ||
          g_str_has_prefix (name, "xxh."));
}

static gchar *
ide_makecache_get_relative_path (IdeMakecache *self,
                                 GFile        *file)
{
  IdeContext *context;
  IdeVcs *vcs;
  GFile *workdir;

  g_assert (IDE_IS_MAKECACHE (self));
  g_assert (G_IS_FILE (file));

  context = ide_object_get_context (IDE_OBJECT (self));
  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);

  return g_file_get_relative_path (workdir, file);
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
      g_assert (error != NULL);
      g_task_return_error (task, error);
      IDE_EXIT;
    }

  if (!g_subprocess_communicate_utf8 (subprocess, NULL, cancellable, &stdoutstr, NULL, &error))
    {
      g_assert (error != NULL);
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

  ide_thread_pool_push_task (IDE_THREAD_POOL_COMPILER,
                             task,
                             ide_makecache_discover_llvm_flags_worker);

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

/**
 * ide_makecache_get_file_targets_searched:
 *
 * Returns: (transfer container): A #GPtrArray of #IdeMakecacheTarget.
 */
static GPtrArray *
ide_makecache_get_file_targets_searched (GMappedFile *mapped,
                                         const gchar *path)
{
  g_autofree gchar *escaped = NULL;
  g_autofree gchar *name = NULL;
  g_autofree gchar *regexstr = NULL;
  g_autofree gchar *subdir = NULL;
  g_autoptr(GHashTable) found = NULL;
  g_autoptr(GMatchInfo) match_info = NULL;
  g_autoptr(GPtrArray) targets = NULL;
  g_autoptr(GRegex) regex = NULL;
  const gchar *content;
  const gchar *line;
  IdeLineReader rl;
  gsize len;
  gsize line_len;

  IDE_ENTRY;

  g_assert (path);

  /*
   * TODO:
   *
   * We can end up with the same filename in multiple subdirectories. We should be careful about
   * that later when we extract flags to choose the best match first.
   */
  name = g_path_get_basename (path);
  escaped = g_regex_escape_string (name, -1);
  regexstr = g_strdup_printf ("^([^:\n ]+):.*\\b(%s)\\b", escaped);

  regex = g_regex_new (regexstr, 0, 0, NULL);
  if (!regex)
    IDE_RETURN (NULL);

  content = g_mapped_file_get_contents (mapped);
  len = g_mapped_file_get_length (mapped);

  targets = g_ptr_array_new_with_free_func ((GDestroyNotify)ide_makecache_target_unref);
  found = g_hash_table_new (ide_makecache_target_hash, ide_makecache_target_equal);

#ifdef IDE_ENABLE_TRACE
  {
    gchar *fmtsize;

    fmtsize = g_format_size (len);
    IDE_TRACE_MSG ("Beginning regex lookup across %s of UTF-8 text", fmtsize);
    g_free (fmtsize);
  }
#endif

  if (len > G_MAXSSIZE)
    IDE_RETURN (NULL);

  ide_line_reader_init (&rl, (gchar *)content, len);

  while ((line = ide_line_reader_next (&rl, &line_len)))
    {
      /*
       * Keep track of "subdir = <dir>" changes so we know what directory
       * to launch make from.
       */
      if ((line_len > 9) && (memcmp (line, "subdir = ", 9) == 0))
        {
          g_free (subdir);
          subdir = g_strndup (line + 9, line_len - 9);
          continue;
        }

      if (g_regex_match_full (regex, line, line_len, 0, 0, &match_info, NULL))
        {
          while (g_match_info_matches (match_info))
            {
              g_autofree gchar *targetstr = NULL;

              targetstr = g_match_info_fetch (match_info, 1);

              if (is_target_interesting (targetstr))
                {
                  g_autoptr(IdeMakecacheTarget) target = NULL;

                  target = ide_makecache_target_new (subdir, targetstr);

                  if (!g_hash_table_contains (found, target))
                    {
                      g_hash_table_insert (found, target, NULL);
                      g_ptr_array_add (targets, g_steal_pointer (&target));
                    }
                }

              g_match_info_next (match_info, NULL);
            }
        }
    }

  IDE_TRACE_MSG ("Regex scan complete");

  if (targets->len > 0)
    {
#ifdef IDE_ENABLE_TRACE
      {
        GString *str;
        gsize i;

        str = g_string_new (NULL);

        for (i = 0; i < targets->len; i++)
          {
            const gchar *target_subdir;
            const gchar *target;
            IdeMakecacheTarget *cur;

            cur = g_ptr_array_index (targets, i);

            target_subdir = ide_makecache_target_get_subdir (cur);
            target = ide_makecache_target_get_target (cur);

            if (target_subdir != NULL)
              g_string_append_printf (str, " (%s of subdir %s)", target, target_subdir);
            else
              g_string_append_printf (str, " %s", target);
          }


        IDE_TRACE_MSG ("File \"%s\" found in targets: %s", path, str->str);
        g_string_free (str, TRUE);
      }
#endif

      g_ptr_array_ref (targets);

      IDE_RETURN (targets);
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

  g_debug ("Validating makecache");

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

  g_debug ("Using \"%s\" for makecache directory", directory);

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

  g_debug ("Creating temporary makecache at \"%s\"", path);

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
      g_assert (error != NULL);
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
  subprocess = g_subprocess_launcher_spawn (launcher, &error, GNU_MAKE_NAME, "-p", "-n", "-s", NULL);
  fdcopy = -1;

  if (!subprocess)
    {
      g_assert (error != NULL);
      g_task_return_error (task, error);
      close (fd);
      IDE_EXIT;
    }

  /*
   * Step 4, wait for the subprocess to complete.
   */
  if (!g_subprocess_wait (subprocess, cancellable, &error))
    {
      g_assert (error != NULL);
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
      g_assert (error != NULL);
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
      g_assert (error != NULL);
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
ide_makecache_parse_c_cxx_include (IdeMakecache *self,
                                   GPtrArray    *ret,
                                   const gchar  *relpath,
                                   const gchar  *part1,
                                   const gchar  *part2,
                                   const gchar  *subdir)
{
  static const gchar *dummy = "-I";
  gchar *adjusted = NULL;

  g_assert (self != NULL);
  g_assert (ret != NULL);
  g_assert (relpath != NULL);
  g_assert (part1 != NULL);
  g_assert (subdir != NULL);

  /*
   * We will get parts either like ("-Ifoo", NULL) or ("-I", "foo").
   * Canonicalize things so we are always dealing with something that looks
   * like ("-I", "foo") since we might need to mutate the path.
   */

  if (part2 == NULL)
    {
      g_assert (strlen (part1) > 2);
      part2 = &part1 [2];
      part1 = dummy;
    }

  g_assert (!ide_str_empty0 (part1));
  g_assert (!ide_str_empty0 (part2));

  /*
   * If the path is relative, then we need to adjust it to be relative to the
   * target file rather than relative to the makefile. Clang expects the
   * path information to be as such.
   */

  if (part2 [0] != '/')
    {
      gchar *parent;

      parent = g_file_get_path (self->parent);
      adjusted = g_build_filename (parent, subdir, part2, NULL);
      g_free (parent);

      part2 = adjusted;
    }

  g_ptr_array_add (ret, g_strdup_printf ("%s%s", part1, part2));

  g_free (adjusted);
}

static void
ide_makecache_parse_c_cxx (IdeMakecache *self,
                           const gchar  *line,
                           const gchar  *relpath,
                           const gchar  *subdir,
                           GPtrArray    *ret)
{
  gint argc = 0;
  g_auto(GStrv) argv = NULL;
  gboolean in_expand = FALSE;
  GError *error = NULL;
  gsize i;

  g_assert (line != NULL);
  g_assert (ret != NULL);
  g_assert (subdir != NULL);

  while (isspace (*line))
    line++;

  if (!g_shell_parse_argv (line, &argc, &argv, &error))
    {
      g_warning ("Failed to parse line: %s", error->message);
      g_clear_error (&error);
      return;
    }

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
          {
            const gchar *part1 = flag;
            const gchar *part2 = NULL;

            if ((strlen (flag) == 2) && (i < (argc - 1)))
              part2 = argv [++i];
            ide_makecache_parse_c_cxx_include (self, ret, relpath, part1, part2, subdir);
          }
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

static gchar *
build_path (const gchar *relpath,
            const gchar *subdir,
            const gchar *path)
{
  g_assert (relpath);
  g_assert (subdir);
  g_assert (path);

  if (g_path_is_absolute (path))
    return g_strdup (path);

  return g_build_filename (subdir, path, NULL);
}

static void
ide_makecache_parse_valac (IdeMakecache *self,
                           const gchar  *line,
                           const gchar  *relpath,
                           const gchar  *subdir,
                           GPtrArray    *ret)
{
  g_auto(GStrv) argv = NULL;
  gint argc = 0;

  g_assert (IDE_IS_MAKECACHE (self));
  g_assert (line != NULL);
  g_assert (relpath != NULL);
  g_assert (subdir != NULL);
  g_assert (ret != NULL);

  if (g_shell_parse_argv (line, &argc, &argv, NULL))
    {
      gint i;

      for (i = 0; i < argc; i++)
        {
          gchar *arg = argv[i];
          gchar *next_arg = i < (argc - 1) ? argv[i + 1] : NULL;

          if (g_str_has_prefix (arg, "--pkg=") ||
              g_str_has_prefix (arg, "--target-glib="))
            {
              g_ptr_array_add (ret, g_strdup (arg));
            }
          else if (g_str_has_prefix (arg, "--vapidir=") ||
                   g_str_has_prefix (arg, "--girdir=") ||
                   g_str_has_prefix (arg, "--metadatadir="))
            {
              gchar *tmp = strchr (arg, '=');

              *tmp = '\0';
              next_arg = ++tmp;

              g_ptr_array_add (ret, g_strdup (arg));
              g_ptr_array_add (ret, build_path (relpath, subdir, next_arg));
            }
          else if (next_arg &&
                   (g_str_has_prefix (arg, "--pkg") ||
                    g_str_has_prefix (arg, "--target-glib")))
            {
              g_ptr_array_add (ret, g_strdup (arg));
              g_ptr_array_add (ret, g_strdup (next_arg));
              i++;
            }
          else if (g_str_has_prefix (arg, "--vapidir") ||
                   g_str_has_prefix (arg, "--girdir") ||
                   g_str_has_prefix (arg, "--metadatadir"))
            {
              g_ptr_array_add (ret, g_strdup (arg));
              g_ptr_array_add (ret, build_path (relpath, subdir, next_arg));
              i++;
            }
          else if (g_str_has_prefix (arg, "--thread"))
            {
              g_ptr_array_add (ret, g_strdup (arg));
            }
          else if (strstr (arg, ".vapi") != NULL)
            {
              g_ptr_array_add (ret, g_strdup (arg));
            }
        }
    }

  g_ptr_array_add (ret, NULL);
}

static gchar **
ide_makecache_parse_line (IdeMakecache *self,
                          const gchar  *line,
                          const gchar  *relpath,
                          const gchar  *subdir)
{
  GPtrArray *ret;
  const gchar *pos;

  IDE_ENTRY;

  g_assert (IDE_IS_MAKECACHE (self));
  g_assert (line != NULL);
  g_assert (relpath != NULL);
  g_assert (subdir != NULL);

  ret = g_ptr_array_new_with_free_func (g_free);

  if ((pos = strstr (line, FAKE_CXX)))
    {
      gchar **strv;

      g_ptr_array_add (ret, g_strdup ("-xc++"));
      ide_makecache_parse_c_cxx (self, pos + strlen (FAKE_CXX), relpath, subdir, ret);
      strv = (gchar **)g_ptr_array_free (ret, FALSE);
      IDE_RETURN (strv);
    }
  else if ((pos = strstr (line, FAKE_CC)))
    {
      gchar **strv;

      ide_makecache_parse_c_cxx (self, pos + strlen(FAKE_CC), relpath, subdir, ret);
      strv = (gchar **)g_ptr_array_free (ret, FALSE);
      IDE_RETURN (strv);
    }
  else if ((pos = strstr (line, FAKE_VALAC)))
    {
      gchar **strv;

      ide_makecache_parse_valac (self, pos + strlen(FAKE_VALAC), relpath, subdir, ret);
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
  FileFlagsLookup *lookup = task_data;
  gsize i;
  gsize j;

  IDE_ENTRY;

  g_assert (EGG_IS_TASK_CACHE (source_object));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (G_IS_TASK (task));
  g_assert (lookup != NULL);
  g_assert (lookup->relative_path != NULL);
  g_assert (G_IS_FILE (lookup->file));
  g_assert (IDE_IS_MAKECACHE (lookup->self));
  g_assert (lookup->targets != NULL);

  for (j = 0; j < lookup->targets->len; j++)
    {
      IdeMakecacheTarget *target;
      g_autoptr(GSubprocessLauncher) launcher = NULL;
      g_autoptr(GSubprocess) subprocess = NULL;
      g_autoptr(GPtrArray) argv = NULL;
      g_autofree gchar *stdoutstr = NULL;
      g_autofree gchar *cwd = NULL;
      const gchar *subdir;
      const gchar *targetstr;
      const gchar *relpath;
      GError *error = NULL;
      gchar **lines;
      gchar **ret = NULL;
      gchar *tmp;

      if (g_cancellable_is_cancelled (cancellable))
        break;

      target = g_ptr_array_index (lookup->targets, j);

      subdir = ide_makecache_target_get_subdir (target);
      targetstr = ide_makecache_target_get_target (target);

      cwd = g_file_get_path (lookup->self->parent);

      if ((subdir != NULL) && g_str_has_prefix (lookup->relative_path, subdir))
        relpath = lookup->relative_path + strlen (subdir);
      else
        relpath = lookup->relative_path;

      while (*relpath == G_DIR_SEPARATOR)
        relpath++;

      argv = g_ptr_array_new ();
      g_ptr_array_add (argv, GNU_MAKE_NAME);
      g_ptr_array_add (argv, "-C");
      g_ptr_array_add (argv, (gchar *)(subdir ?: "."));
      g_ptr_array_add (argv, "-s");
      g_ptr_array_add (argv, "-i");
      g_ptr_array_add (argv, "-n");
      g_ptr_array_add (argv, "-W");
      g_ptr_array_add (argv, (gchar *)relpath);
      g_ptr_array_add (argv, (gchar *)targetstr);
      g_ptr_array_add (argv, "V=1");
      g_ptr_array_add (argv, "CC="FAKE_CC);
      g_ptr_array_add (argv, "CXX="FAKE_CXX);
      g_ptr_array_add (argv, "VALAC="FAKE_VALAC);
      g_ptr_array_add (argv, NULL);

#ifdef IDE_ENABLE_TRACE
      {
        gchar *cmdline;

        cmdline = g_strjoinv (" ", (gchar **)argv->pdata);
        IDE_TRACE_MSG ("subdir=%s %s", subdir ?: ".", cmdline);
        g_free (cmdline);
      }
#endif

      launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);
      g_subprocess_launcher_set_cwd (launcher, cwd);
      subprocess = g_subprocess_launcher_spawnv (launcher,
                                                 (const gchar * const *)argv->pdata,
                                                 &error);

      if (!subprocess)
        {
          g_assert (error != NULL);
          g_task_return_error (task, error);
          IDE_EXIT;
        }

      /* Don't let ourselves be cancelled from this operation */
      if (!g_subprocess_communicate_utf8 (subprocess, NULL, NULL, &stdoutstr, NULL, &error))
        {
          g_assert (error != NULL);
          g_task_return_error (task, error);
          IDE_EXIT;
        }

      /*
       * Replace escaped newlines with " " to simplify command parsing
       */
      tmp = stdoutstr;
      while (NULL != (tmp = strstr (tmp, "\\\n")))
        {
          tmp[0] = ' ';
          tmp[1] = ' ';
        }

      lines = g_strsplit (stdoutstr, "\n", 0);

      for (i = 0; lines [i]; i++)
        {
          gchar *line = lines [i];
          gsize linelen;

          if (line [0] == '\0')
            continue;

          linelen = strlen (line);

          if (line [linelen - 1] == '\\')
            line [linelen - 1] = '\0';

          if ((ret = ide_makecache_parse_line (lookup->self, line, relpath, subdir ?: ".")))
            break;
        }

      g_strfreev (lines);

      if (ret == NULL)
        continue;

      g_task_return_pointer (task, ret, (GDestroyNotify)g_strfreev);

      IDE_EXIT;
    }

  if (g_cancellable_is_cancelled (cancellable) && g_task_get_return_on_cancel (task))
    IDE_EXIT;

  g_task_return_new_error (task,
                           G_IO_ERROR,
                           G_IO_ERROR_FAILED,
                           "Failed to extract flags from make output");

  IDE_EXIT;
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

static gchar *
replace_suffix (const gchar *str,
                const gchar *replace)
{
  const gchar *suffix;
  GString *gs;

  g_assert (str != NULL);
  g_assert (replace != NULL);

  /*
   * In the common case, we could do this without GString.
   * But probably not worth the effort.
   */

  if (NULL == (suffix = strrchr (str, '.')))
    return g_strdup (str);

  gs = g_string_new (NULL);
  g_string_append_len (gs, str, suffix - str);
  g_string_append_printf (gs, ".%s", replace);

  return g_string_free (gs, FALSE);
}

static void
ide_makecache_get_file_targets_worker (GTask        *task,
                                       gpointer      source_object,
                                       gpointer      task_data,
                                       GCancellable *cancellable)
{
  g_autofree gchar *translated = NULL;
  g_autofree gchar *base = NULL;
  FileTargetsLookup *lookup = task_data;
  const gchar *path;
  GPtrArray *ret;

  IDE_ENTRY;

  g_assert (EGG_IS_TASK_CACHE (source_object));
  g_assert (G_IS_TASK (task));
  g_assert (lookup != NULL);
  g_assert (lookup->mapped != NULL);
  g_assert (lookup->path != NULL);

  path = lookup->path;

  /* Translate suffix to something we can find in a target */
  if (g_str_has_suffix (path, ".vala"))
    path = translated = replace_suffix (path, "c");

  base = g_path_get_basename (path);

  /* we use an empty GPtrArray to get negative cache hits. a bit heavy handed? sure. */
  if (!(ret = ide_makecache_get_file_targets_searched (lookup->mapped, path)))
    ret = g_ptr_array_new ();

  /* If we had a vala file, we might need to translate the target */
  if (translated != NULL)
    {
      guint i;

      for (i = 0; i < ret->len; i++)
        {
          IdeMakecacheTarget *target = g_ptr_array_index (ret, i);
          const gchar *name = ide_makecache_target_get_target (target);
          const gchar *slash = strrchr (name, G_DIR_SEPARATOR);
          const gchar *endptr;

          /* We might be using non-recursive automake, which means that the
           * source is maybe in a subdirectory, but the target is in the
           * current subdir (so no directory prefix).
           */
          if (slash != NULL)
            name = slash + 1;

          /*
           * It we got a target that looks like "foo.lo" and the filename was
           * "foo.vala", then they probably aren't using vala automake
           * integration but we can likely still extract flags.
           */
          if ((NULL != (endptr = strrchr (name, '.'))) &&
              (strcmp (endptr, ".lo") == 0) &&
              (strncmp (name, base, endptr - name) == 0))
            continue;

          /*
           * Follow the automake vala renaming rules the best I can decipher.
           * I mostly see _vala.stamp, but I have seen others. However, we
           * need to ship this product and our templates are generating
           * _vala.stamp with libraries, so at least make that work out of
           * the box.
           */
          if (NULL != (endptr = strchr (name, '-')))
            {
              GString *str = g_string_new (NULL);

              g_string_append_len (str, name, endptr - name);
              g_string_append (str, "_vala.stamp");
              ide_makecache_target_set_target (target, str->str);
              g_string_free (str, TRUE);
            }
        }
    }

  g_task_return_pointer (task, ret, (GDestroyNotify)g_ptr_array_unref);

  IDE_EXIT;
}

static void
ide_makecache_get_file_targets_dispatch (EggTaskCache  *cache,
                                         gconstpointer  key,
                                         GTask         *task,
                                         gpointer       user_data)
{
  IdeMakecache *self = user_data;
  FileTargetsLookup *lookup;
  GFile *file = (GFile *)key;

  g_assert (EGG_IS_TASK_CACHE (cache));
  g_assert (IDE_IS_MAKECACHE (self));
  g_assert (G_IS_FILE (file));
  g_assert (G_IS_TASK (task));

  lookup = g_slice_new0 (FileTargetsLookup);
  lookup->mapped = g_mapped_file_ref (self->mapped);

  if (!(lookup->path = ide_makecache_get_relative_path (self, file)) &&
      !(lookup->path = g_file_get_path (file)) &&
      !(lookup->path = g_file_get_basename (file)))
    {
      file_targets_lookup_free (lookup);
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_FILENAME,
                               "Failed to extract filename.");
      return;
    }

  g_task_set_task_data (task, lookup, file_targets_lookup_free);

  /* throttle via the compiler thread pool */
  ide_thread_pool_push_task (IDE_THREAD_POOL_COMPILER,
                             task,
                             ide_makecache_get_file_targets_worker);
}

static void
ide_makecache_get_file_flags__get_targets_cb (GObject      *object,
                                              GAsyncResult *result,
                                              gpointer      user_data)
{
  IdeMakecache *self = (IdeMakecache *)object;
  g_autoptr(GPtrArray) targets = NULL;
  g_autoptr(GTask) task = user_data;
  FileFlagsLookup *lookup;
  GError *error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAKECACHE (self));

  if (!(targets = ide_makecache_get_file_targets_finish (self, result, &error)))
    {
      g_assert (error != NULL);
      g_task_return_error (task, error);
      IDE_EXIT;
    }

  lookup = g_task_get_task_data (task);
  g_assert (IDE_IS_MAKECACHE (lookup->self));
  g_assert (G_IS_FILE (lookup->file));

  /*
   * If we didn't discover any targets for this file, try to apply the language
   * defaults based on the filetype.
   */
  if (targets->len == 0)
    {
      if (file_is_clangable (lookup->file))
        {
          gchar **ret;

          ret = g_new0 (gchar *, 2);
          ret [0] = g_strdup (self->llvm_flags);
          ret [1] = NULL;

          g_task_return_pointer (task, ret, (GDestroyNotify)g_strfreev);
          IDE_EXIT;
        }

      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_FILENAME,
                               "File is not included in an target.");
      IDE_EXIT;
    }

  lookup->targets = g_ptr_array_ref (targets);

  ide_thread_pool_push_task (IDE_THREAD_POOL_COMPILER,
                             task,
                             ide_makecache_get_file_flags_worker);

  IDE_EXIT;
}

static void
ide_makecache_get_file_flags_dispatch (EggTaskCache  *cache,
                                       gconstpointer  key,
                                       GTask         *task,
                                       gpointer       user_data)
{
  IdeMakecache *self = user_data;
  FileFlagsLookup *lookup;
  GFile *file = (GFile *)key;

  IDE_ENTRY;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_MAKECACHE (self));
  g_assert (G_IS_FILE (file));

  lookup = g_slice_new0 (FileFlagsLookup);
  lookup->self = g_object_ref (self);
  lookup->file = g_object_ref (file);

  if (!(lookup->relative_path = ide_makecache_get_relative_path (self, file)) &&
      !(lookup->relative_path = g_file_get_path (file)) &&
      !(lookup->relative_path = g_file_get_basename (file)))
    {
      file_flags_lookup_free (lookup);
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_FILENAME,
                               "Failed to extract filename.");
      return;
    }

  g_task_set_task_data (task, lookup, file_flags_lookup_free);

  ide_makecache_get_file_targets_async (self,
                                        file,
                                        g_task_get_cancellable (task),
                                        ide_makecache_get_file_flags__get_targets_cb,
                                        g_object_ref (task));

  IDE_EXIT;
}


static void
ide_makecache_finalize (GObject *object)
{
  IdeMakecache *self = (IdeMakecache *)object;

  g_clear_object (&self->makefile);
  g_clear_pointer (&self->mapped, g_mapped_file_unref);
  g_clear_object (&self->file_targets_cache);
  g_clear_object (&self->file_flags_cache);
  g_clear_pointer (&self->llvm_flags, g_free);

  G_OBJECT_CLASS (ide_makecache_parent_class)->finalize (object);

  EGG_COUNTER_DEC (instances);
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

  properties [PROP_MAKEFILE] =
    g_param_spec_object ("makefile",
                         "Makefile",
                         "The root makefile to be cached.",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
ide_makecache_init (IdeMakecache *self)
{
  EGG_COUNTER_INC (instances);

  self->file_targets_cache = egg_task_cache_new ((GHashFunc)g_file_hash,
                                                 (GEqualFunc)g_file_equal,
                                                 g_object_ref,
                                                 g_object_unref,
                                                 (GBoxedCopyFunc)g_ptr_array_ref,
                                                 (GBoxedFreeFunc)g_ptr_array_unref,
                                                 0,
                                                 ide_makecache_get_file_targets_dispatch,
                                                 self,
                                                 NULL);

  self->file_flags_cache = egg_task_cache_new ((GHashFunc)g_file_hash,
                                               (GEqualFunc)g_file_equal,
                                               g_object_ref,
                                               g_object_unref,
                                               (GBoxedCopyFunc)g_strdupv,
                                               (GBoxedFreeFunc)g_strfreev,
                                               0,
                                               ide_makecache_get_file_flags_dispatch,
                                               self,
                                               NULL);
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

  ide_thread_pool_push_task (IDE_THREAD_POOL_COMPILER,
                             task,
                             ide_makecache_new_worker);
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

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_CONTEXT (context));
  g_return_if_fail (G_IS_FILE (makefile));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

#ifdef IDE_ENABLE_TRACE
  {
    g_autofree gchar *path = g_file_get_path (makefile);
    IDE_TRACE_MSG ("Generating makecache for %s", path);
  }
#endif

  self = g_object_new (IDE_TYPE_MAKECACHE,
                       "context", context,
                       "makefile", makefile,
                       NULL);

  task = g_task_new (self, cancellable, callback, user_data);

  ide_makecache_discover_llvm_flags_async (self,
                                           cancellable,
                                           ide_makecache__discover_llvm_flags_cb,
                                           g_object_ref (task));

  IDE_EXIT;
}

IdeMakecache *
ide_makecache_new_for_makefile_finish (GAsyncResult  *result,
                                       GError       **error)
{
  GTask *task = (GTask *)result;
  IdeMakecache *ret;

  IDE_ENTRY;

  g_return_val_if_fail (G_IS_TASK (task), NULL);

  ret = g_task_propagate_pointer (task, error);

  IDE_RETURN (ret);
}

static void
ide_makecache_get_file_targets__task_cache_get_cb (GObject      *object,
                                                   GAsyncResult *result,
                                                   gpointer      user_data)
{
  EggTaskCache *cache = (EggTaskCache *)object;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;
  GPtrArray *ret;

  if (!(ret = egg_task_cache_get_finish (cache, result, &error)))
    {
      g_assert (error != NULL);
      g_task_return_error (task, error);
    }
  else
    g_task_return_pointer (task, ret, (GDestroyNotify)g_ptr_array_unref);
}

void
ide_makecache_get_file_targets_async (IdeMakecache        *self,
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

  egg_task_cache_get_async (self->file_targets_cache,
                            file,
                            FALSE,
                            cancellable,
                            ide_makecache_get_file_targets__task_cache_get_cb,
                            g_object_ref (task));

  IDE_EXIT;
}

/**
 * ide_makecache_get_file_targets_finish:
 *
 * Completes an asynchronous request to ide_makecache_get_file_flags_async().
 *
 * Returns: (transfer container) (element-type IdeMakecacheTarget): An array of targets.
 */
GPtrArray *
ide_makecache_get_file_targets_finish (IdeMakecache  *self,
                                       GAsyncResult  *result,
                                       GError       **error)
{
  GTask *task = (GTask *)result;
  GPtrArray *ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_MAKECACHE (self), NULL);
  g_return_val_if_fail (G_IS_TASK (task), NULL);

  ret = g_task_propagate_pointer (task, error);

  IDE_RETURN (ret);
}

static void
ide_makecache_get_file_flags__task_cache_get_cb (GObject      *object,
                                                 GAsyncResult *result,
                                                 gpointer      user_data)
{
  EggTaskCache *cache = (EggTaskCache *)object;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;
  gchar **ret;

  if (!(ret = egg_task_cache_get_finish (cache, result, &error)))
    {
      g_assert (error != NULL);
      g_task_return_error (task, error);
    }
  else
    g_task_return_pointer (task, ret, (GDestroyNotify)g_strfreev);
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

  egg_task_cache_get_async (self->file_flags_cache,
                            file,
                            FALSE,
                            cancellable,
                            ide_makecache_get_file_flags__task_cache_get_cb,
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
