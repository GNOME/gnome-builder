/* ide-simple-builder.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-simple-builder"

#include <gtksourceview/gtksource.h>

#include "buildsystem/ide-simple-builder.h"
#include "files/ide-file.h"

G_DEFINE_TYPE (IdeSimpleBuilder, ide_simple_builder, IDE_TYPE_BUILDER)

static void
ide_simple_builder_build_async (IdeBuilder            *builder,
                                IdeBuilderBuildFlags   flags,
                                IdeBuildResult       **build_result,
                                GCancellable          *cancellable,
                                GAsyncReadyCallback    callback,
                                gpointer               user_data)
{
  g_assert (IDE_IS_SIMPLE_BUILDER (builder));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  /* TODO: we can use the prebuild/postbuild commands at least? */

  g_task_report_new_error (builder,
                           callback,
                           user_data,
                           ide_simple_builder_build_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "%s does not support building",
                           G_OBJECT_TYPE_NAME (builder));
}

static IdeBuildResult *
ide_simple_builder_build_finish (IdeBuilder    *builder,
                                 GAsyncResult  *result,
                                 GError       **error)
{
  g_assert (IDE_IS_SIMPLE_BUILDER (builder));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
ide_simple_builder_install_async (IdeBuilder           *builder,
                                  IdeBuildResult      **build_result,
                                  GCancellable         *cancellable,
                                  GAsyncReadyCallback   callback,
                                  gpointer              user_data)
{
  /* TODO: we can use the prebuild/postbuild commands at least? */

  g_task_report_new_error (builder,
                           callback,
                           user_data,
                           ide_simple_builder_install_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "%s does not support installing",
                           G_OBJECT_TYPE_NAME (builder));
}

static IdeBuildResult *
ide_simple_builder_install_finish (IdeBuilder    *builder,
                                   GAsyncResult  *result,
                                   GError       **error)
{
  g_assert (IDE_IS_SIMPLE_BUILDER (builder));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
ide_simple_builder_get_build_flags_async (IdeBuilder          *builder,
                                          IdeFile             *file,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data)
{
  IdeSimpleBuilder *self = (IdeSimpleBuilder *)builder;
  g_autoptr(GTask) task = NULL;
  IdeConfiguration *config;
  GtkSourceLanguage *language;
  const gchar *env = NULL;
  const gchar *id;

  g_assert (IDE_IS_SIMPLE_BUILDER (self));
  g_assert (IDE_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_simple_builder_get_build_flags_async);

  language = ide_file_get_language (file);

  config = ide_builder_get_configuration (builder);

  if (config == NULL || language == NULL)
    goto failure;

  id = gtk_source_language_get_id (language);

  if (ide_str_equal0 (id, "c") || ide_str_equal0 (id, "chdr"))
    env = ide_configuration_getenv (config, "CFLAGS");
  else if (ide_str_equal0 (id, "cpp") || ide_str_equal0 (id, "cpphdr"))
    env = ide_configuration_getenv (config, "CXXFLAGS");
  else if (ide_str_equal0 (id, "vala"))
    env = ide_configuration_getenv (config, "VALAFLAGS");

  if (env != NULL)
    {
      gchar **flags = NULL;
      gint argc;

      if (g_shell_parse_argv (env, &argc, &flags, NULL))
        {
          g_task_return_pointer (task, flags, (GDestroyNotify)g_strfreev);
          return;
        }
    }

failure:
  g_task_return_pointer (task, g_new0 (gchar*, 1), (GDestroyNotify)g_strfreev);
}

static gchar **
ide_simple_builder_get_build_flags_finish (IdeBuilder    *builder,
                                           GAsyncResult  *result,
                                           GError       **error)
{
  g_assert (IDE_IS_SIMPLE_BUILDER (builder));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
ide_simple_builder_class_init (IdeSimpleBuilderClass *klass)
{
  IdeBuilderClass *builder_class = IDE_BUILDER_CLASS (klass);

  builder_class->get_build_flags_async = ide_simple_builder_get_build_flags_async;
  builder_class->get_build_flags_finish = ide_simple_builder_get_build_flags_finish;
  builder_class->build_async = ide_simple_builder_build_async;
  builder_class->build_finish = ide_simple_builder_build_finish;
  builder_class->install_async = ide_simple_builder_install_async;
  builder_class->install_finish = ide_simple_builder_install_finish;
}

static void
ide_simple_builder_init (IdeSimpleBuilder *self)
{
}
