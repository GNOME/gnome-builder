/* ide-golang-build-system.c
 *
 * Copyright 2018 Loïc BLOT <loic.blot@unix-experience.fr>
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

#define G_LOG_DOMAIN "ide-golang-build-system"

#include "config.h"

#include <gio/gio.h>
#include <ide.h>
#include <string.h>

#include "ide-golang-build-system.h"

struct _IdeGolangBuildSystem
{
  IdeObject  parent_instance;
  gchar      *goroot;
  gchar      *gopath;
};

static void async_initable_iface_init (GAsyncInitableIface *iface);
static void build_system_iface_init (IdeBuildSystemInterface *iface);

G_DEFINE_TYPE_WITH_CODE (IdeGolangBuildSystem,
                         ide_golang_build_system,
                         IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, async_initable_iface_init)
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_SYSTEM, build_system_iface_init))

enum {
  PROP_0,
  PROP_GOROOT,
  PROP_GOPATH,
  LAST_PROP,
};

static GParamSpec *properties [LAST_PROP];

static void
ide_golang_build_system_constructed (GObject *object)
{
  IdeGolangBuildSystem *self = (IdeGolangBuildSystem *)object;
  IdeBufferManager *buffer_manager;
  IdeContext *context;

  G_OBJECT_CLASS (ide_golang_build_system_parent_class)->constructed (object);

  context = ide_object_get_context (IDE_OBJECT (self));
  g_assert (IDE_IS_CONTEXT (context));

  buffer_manager = ide_context_get_buffer_manager (context);
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));
}

static void
ide_golang_build_system_finalize (GObject *object)
{
  IdeGolangBuildSystem *self = (IdeGolangBuildSystem *)object;

  g_clear_pointer (&self->goroot, g_free);
  g_clear_pointer (&self->gopath, g_free);
  G_OBJECT_CLASS (ide_golang_build_system_parent_class)->finalize (object);
}

static void
ide_golang_build_system_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  IdeGolangBuildSystem *self = IDE_GOLANG_BUILD_SYSTEM (object);

  switch (prop_id)
    {
    case PROP_GOROOT:
      g_value_set_string (value, self->goroot);
      break;

    case PROP_GOPATH:
      g_value_set_string (value, self->gopath);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_golang_build_system_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  IdeGolangBuildSystem *self = IDE_GOLANG_BUILD_SYSTEM (object);

  switch (prop_id)
    {
    case PROP_GOROOT:
      g_free (self->goroot);
      self->goroot = g_value_dup_string (value);
      break;

    case PROP_GOPATH:
      g_free (self->gopath);
      self->gopath = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static gchar *
ide_golang_build_system_get_id (IdeBuildSystem *build_system)
{
  return g_strdup ("golang");
}

static gchar *
ide_golang_build_system_get_display_name (IdeBuildSystem *build_system)
{
  return g_strdup ("Golang");
}

static gint
ide_golang_build_system_get_priority (IdeBuildSystem *system)
{
  return 0;
}

static void
ide_golang_build_system_get_build_flags_async (IdeBuildSystem      *build_system,
                                                  IdeFile             *file,
                                                  GCancellable        *cancellable,
                                                  GAsyncReadyCallback  callback,
                                                  gpointer             user_data)
{
  IdeGolangBuildSystem *self = (IdeGolangBuildSystem *)build_system;
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_GOLANG_BUILD_SYSTEM (self));
  g_assert (IDE_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_golang_build_system_get_build_flags_async);

  IDE_EXIT;
}

static gchar **
ide_golang_build_system_get_build_flags_finish (IdeBuildSystem  *build_system,
                                                   GAsyncResult    *result,
                                                   GError         **error)
{
  g_assert (IDE_IS_GOLANG_BUILD_SYSTEM (build_system));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
ide_golang_build_system_init (IdeGolangBuildSystem *self)
{
}

static void
ide_golang_build_system_init_async (GAsyncInitable      *initable,
                                       gint                 io_priority,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  IdeGolangBuildSystem *system = (IdeGolangBuildSystem *)initable;
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_GOLANG_BUILD_SYSTEM (system));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (initable, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_golang_build_system_init_async);
  ide_task_set_priority (task, G_PRIORITY_HIGH);
  ide_task_return_boolean (task, TRUE);
}

static gboolean
ide_golang_build_system_init_finish (GAsyncInitable  *initable,
                                        GAsyncResult    *result,
                                        GError         **error)
{
  IdeTask *task = (IdeTask *)result;

  g_return_val_if_fail (IDE_IS_GOLANG_BUILD_SYSTEM (initable), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (task), FALSE);

  return ide_task_propagate_boolean (task, error);
}

static void
ide_golang_build_system_class_init (IdeGolangBuildSystemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_golang_build_system_constructed;
  object_class->finalize = ide_golang_build_system_finalize;
  object_class->get_property = ide_golang_build_system_get_property;
  object_class->set_property = ide_golang_build_system_set_property;

  properties [PROP_GOROOT] =
    g_param_spec_string ("project-goroot",
                         "Project GOROOT",
                         "The name of the project tarball GGOOOO.",
                         "/usr/lib/go",
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  properties [PROP_GOPATH] =
    g_param_spec_string ("project-gopath",
                         "Project GOPATH",
                         "The path of the project file BURP.",
                         "~/go",
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
build_system_iface_init (IdeBuildSystemInterface *iface)
{
  iface->get_priority = ide_golang_build_system_get_priority;
  iface->get_build_flags_async = ide_golang_build_system_get_build_flags_async;
  iface->get_build_flags_finish = ide_golang_build_system_get_build_flags_finish;
  iface->get_id = ide_golang_build_system_get_id;
  iface->get_display_name = ide_golang_build_system_get_display_name;
}

static void
async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = ide_golang_build_system_init_async;
  iface->init_finish = ide_golang_build_system_init_finish;
}
