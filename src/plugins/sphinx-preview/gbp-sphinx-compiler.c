/* gbp-sphinx-compiler.c
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

#define G_LOG_DOMAIN "gbp-sphinx-compiler"

#include "config.h"

#include <glib/gstdio.h>

#include <libide-io.h>
#include <libide-threading.h>

#include "gbp-sphinx-compiler.h"

struct _GbpSphinxCompiler
{
  GObject  parent_instance;
  GFile   *config_file;
  GFile   *basedir;
  GFile   *builddir;
};

G_DEFINE_FINAL_TYPE (GbpSphinxCompiler, gbp_sphinx_compiler, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_CONFIG_FILE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

GbpSphinxCompiler *
gbp_sphinx_compiler_new (GFile *config_file)
{
  g_return_val_if_fail (G_IS_FILE (config_file), NULL);

  return g_object_new (GBP_TYPE_SPHINX_COMPILER,
                       "config-file", config_file,
                       NULL);
}

static gboolean
remove_temporary_directory (GFile   *file,
                            GError **error)
{
  g_autoptr(IdeDirectoryReaper) reaper = NULL;

  g_assert (G_IS_FILE (file));
  g_assert (g_file_is_native (file));

  if (!g_file_query_exists (file, NULL))
    return TRUE;

  reaper = ide_directory_reaper_new ();
  ide_directory_reaper_add_directory (reaper, file, 0);
  if (!ide_directory_reaper_execute (reaper, NULL, error))
    return FALSE;

  if (!g_file_delete (file, NULL, error))
    return FALSE;

  return TRUE;
}

static GFile *
create_temporary_directory (GError **error)
{
  g_autofree char *path = NULL;

  if (!(path = g_dir_make_tmp ("gnome-builder-sphinx-XXXXXX", error)))
    return NULL;

  return g_file_new_for_path (path);
}

static void
gbp_sphinx_compiler_constructed (GObject *object)
{
  GbpSphinxCompiler *self = (GbpSphinxCompiler *)object;
  g_autoptr(GError) error = NULL;

  g_assert (GBP_IS_SPHINX_COMPILER (self));

  G_OBJECT_CLASS (gbp_sphinx_compiler_parent_class)->constructed (object);

  if (self->config_file == NULL)
    g_critical ("%s created without a config-file", G_OBJECT_TYPE_NAME (self));
  else if (!(self->basedir = g_file_get_parent (self->config_file)))
    g_critical ("Implausible GFile used as config-file");
  else if (!(self->builddir = create_temporary_directory (&error)))
    g_critical ("Failed to create build directory: %s", error->message);
}

static void
gbp_sphinx_compiler_finalize (GObject *object)
{
  GbpSphinxCompiler *self = (GbpSphinxCompiler *)object;

  if (self->builddir != NULL)
    {
      g_autoptr(GError) error = NULL;

      if (!remove_temporary_directory (self->builddir, &error))
        g_warning ("Failed to cleanup sphinx build directory: %s: %s",
                   g_file_peek_path (self->builddir),
                   error->message);

      g_clear_object (&self->builddir);
    }

  g_clear_object (&self->config_file);
  g_clear_object (&self->basedir);

  G_OBJECT_CLASS (gbp_sphinx_compiler_parent_class)->finalize (object);
}

static void
gbp_sphinx_compiler_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GbpSphinxCompiler *self = GBP_SPHINX_COMPILER (object);

  switch (prop_id)
    {
    case PROP_CONFIG_FILE:
      g_value_set_object (value, self->config_file);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_sphinx_compiler_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GbpSphinxCompiler *self = GBP_SPHINX_COMPILER (object);

  switch (prop_id)
    {
    case PROP_CONFIG_FILE:
      self->config_file = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_sphinx_compiler_class_init (GbpSphinxCompilerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gbp_sphinx_compiler_constructed;
  object_class->finalize = gbp_sphinx_compiler_finalize;
  object_class->get_property = gbp_sphinx_compiler_get_property;
  object_class->set_property = gbp_sphinx_compiler_set_property;

  properties [PROP_CONFIG_FILE] =
    g_param_spec_object ("config-file",
                         "Config File",
                         "Config File",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_sphinx_compiler_init (GbpSphinxCompiler *self)
{
}

static void
gbp_sphinx_compiler_load_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  GFile *file = (GFile *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autofree char *contents = NULL;
  gsize len;

  IDE_ENTRY;

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!g_file_load_contents_finish (file, result, &contents, &len, NULL, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task, g_steal_pointer (&contents), g_free);

  IDE_EXIT;
}

static void
gbp_sphinx_compiler_compile_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  GCancellable *cancellable;
  GFile *dest;

  IDE_ENTRY;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_subprocess_wait_check_finish (subprocess, result, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  dest = ide_task_get_task_data (task);
  cancellable = ide_task_get_cancellable (task);

  g_assert (G_IS_FILE (dest));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_file_load_contents_async (dest,
                              cancellable,
                              gbp_sphinx_compiler_load_cb,
                              g_steal_pointer (&task));

  IDE_EXIT;
}

static char *
replace_suffix (const char *str,
                const char *suffix)
{
  const char *dot = strrchr (str, '.');
  char *shortened;
  char *ret;

  if (dot == NULL)
    return g_strdup (str);

  shortened = g_strndup (str, dot - str);
  ret = g_strconcat (shortened, suffix, NULL);
  g_free (shortened);

  return ret;
}

static void
gbp_sphinx_compiler_purge_doctree (GbpSphinxCompiler *self,
                                   const char        *relpath)
{
  g_autofree char *doctreepath = NULL;
  g_autofree char *fullpath = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_SPHINX_COMPILER (self));
  g_assert (relpath != NULL);

  doctreepath = replace_suffix (relpath, ".doctree");
  fullpath = g_build_filename (g_file_peek_path (self->builddir),
                               ".doctrees",
                               doctreepath,
                               NULL);

  g_unlink (fullpath);

  IDE_EXIT;
}

void
gbp_sphinx_compiler_compile_async (GbpSphinxCompiler   *self,
                                   GFile               *file,
                                   const char          *contents,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) tu = NULL;
  g_autofree char *relpath = NULL;
  g_autofree char *htmlpath = NULL;
  const char *path;

  IDE_ENTRY;

  g_return_if_fail (GBP_IS_SPHINX_COMPILER (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (g_file_has_prefix (file, self->basedir));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_sphinx_compiler_compile_async);

  if (!g_file_is_native (file))
    {
      ide_task_return_unsupported_error (task);
      IDE_EXIT;
    }

  path = g_file_peek_path (file);
  relpath = g_file_get_relative_path (self->basedir, file);
  htmlpath = replace_suffix (relpath, ".html");
  tu = g_file_get_child (self->builddir, htmlpath);
  ide_task_set_task_data (task, g_steal_pointer (&tu), g_object_unref);

  gbp_sphinx_compiler_purge_doctree (self, relpath);

  g_assert (path != NULL);
  g_assert (G_IS_FILE (self->basedir));
  g_assert (G_IS_FILE (self->builddir));
  g_assert (g_file_peek_path (self->basedir) != NULL);
  g_assert (g_file_peek_path (self->builddir) != NULL);

  launcher = ide_subprocess_launcher_new (0);
  ide_subprocess_launcher_push_args (launcher, IDE_STRV_INIT ("sphinx-build", "-Q", "-b", "html"));
  ide_subprocess_launcher_push_argv (launcher, g_file_peek_path (self->basedir));
  ide_subprocess_launcher_push_argv (launcher, g_file_peek_path (self->builddir));
  ide_subprocess_launcher_push_argv (launcher, path);

  if (!(subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  ide_subprocess_wait_check_async (subprocess,
                                   cancellable,
                                   gbp_sphinx_compiler_compile_cb,
                                   g_steal_pointer (&task));

  IDE_EXIT;
}

char *
gbp_sphinx_compiler_compile_finish (GbpSphinxCompiler  *self,
                                    GAsyncResult       *result,
                                    GError            **error)
{
  g_return_val_if_fail (GBP_IS_SPHINX_COMPILER (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}
