/* ide-runtime.c
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-runtime"

#include "config.h"

#include <glib/gi18n.h>
#include <string.h>

#include <libide-io.h>
#include <libide-threading.h>

#define IDE_TERMINAL_INSIDE
# include "../terminal/ide-terminal-util.h"
#undef IDE_TERMINAL_INSIDE

#include "ide-build-manager.h"
#include "ide-build-target.h"
#include "ide-config.h"
#include "ide-config-manager.h"
#include "ide-pipeline.h"
#include "ide-run-context.h"
#include "ide-runtime.h"
#include "ide-toolchain.h"
#include "ide-triplet.h"

typedef struct
{
  char *id;
  char *short_id;
  char *category;
  char *name;
  char *display_name;
  char *icon_name;
} IdeRuntimePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeRuntime, ide_runtime, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_ICON_NAME,
  PROP_ID,
  PROP_SHORT_ID,
  PROP_CATEGORY,
  PROP_DISPLAY_NAME,
  PROP_NAME,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static gboolean
ide_runtime_real_contains_program_in_path (IdeRuntime   *self,
                                           const char   *program,
                                           GCancellable *cancellable)
{
  g_autofree char *path = NULL;

  g_assert (IDE_IS_RUNTIME (self));
  g_assert (program != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  path = g_find_program_in_path (program);

  IDE_TRACE_MSG ("Locating program %s => %s", program, path ? path : "missing");

  return path != NULL;
}

gboolean
ide_runtime_contains_program_in_path (IdeRuntime   *self,
                                      const gchar  *program,
                                      GCancellable *cancellable)
{
  g_return_val_if_fail (IDE_IS_RUNTIME (self), FALSE);
  g_return_val_if_fail (program != NULL, FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  return IDE_RUNTIME_GET_CLASS (self)->contains_program_in_path (self, program, cancellable);
}

static void
ide_runtime_real_prepare_configuration (IdeRuntime *self,
                                        IdeConfig  *config)
{
  IdeRuntimePrivate *priv = ide_runtime_get_instance_private (self);

  g_assert (IDE_IS_RUNTIME (self));
  g_assert (IDE_IS_CONFIG (config));

  if (!ide_config_get_prefix_set (config))
    {
      g_autoptr(IdeContext) context = NULL;
      g_autofree char *install_path = NULL;
      g_autofree char *project_id = NULL;
      g_autofree char *id = NULL;

      context = ide_object_ref_context (IDE_OBJECT (self));
      project_id = ide_context_dup_project_id (context);
      id = g_strdelimit (g_strdup (priv->id), "@:/", '-');

      install_path = ide_context_cache_filename (context, "install", id, NULL);

      ide_config_set_prefix (config, install_path);
      ide_config_set_prefix_set (config, FALSE);
    }
}

static GFile *
ide_runtime_null_translate_file (IdeRuntime *self,
                                 GFile      *file)
{
  return NULL;
}

static GFile *
ide_runtime_flatpak_translate_file (IdeRuntime *self,
                                    GFile      *file)
{
  g_autofree gchar *path = NULL;

  g_assert (IDE_IS_RUNTIME (self));
  g_assert (G_IS_FILE (file));
  g_assert (ide_is_flatpak ());

  /* Only deal with native files */
  if (!g_file_is_native (file) || NULL == (path = g_file_get_path (file)))
    return NULL;

  /* If this is /usr or /etc, then translate to /run/host/$dir,
   * as that is where flatpak 0.10.1 and greater will mount them
   * when --filesystem=host.
   */
  if (g_str_has_prefix (path, "/usr/") || g_str_has_prefix (path, "/etc/"))
    return g_file_new_build_filename ("/run/host/", path, NULL);

  return NULL;
}

static gchar *
ide_runtime_repr (IdeObject *object)
{
  IdeRuntime *self = (IdeRuntime *)object;
  IdeRuntimePrivate *priv = ide_runtime_get_instance_private (self);

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_RUNTIME (self));

  return g_strdup_printf ("%s id=\"%s\" display-name=\"%s\"",
                          G_OBJECT_TYPE_NAME (self),
                          priv->id ?: "",
                          priv->display_name ?: "");
}

static void
ide_runtime_destroy (IdeObject *object)
{
  IdeRuntime *self = (IdeRuntime *)object;
  IdeRuntimePrivate *priv = ide_runtime_get_instance_private (self);

  g_clear_pointer (&priv->id, g_free);
  g_clear_pointer (&priv->short_id, g_free);
  g_clear_pointer (&priv->category, g_free);
  g_clear_pointer (&priv->name, g_free);
  g_clear_pointer (&priv->display_name, g_free);
  g_clear_pointer (&priv->icon_name, g_free);

  IDE_OBJECT_CLASS (ide_runtime_parent_class)->destroy (object);
}

static void
ide_runtime_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  IdeRuntime *self = IDE_RUNTIME (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_string (value, ide_runtime_get_id (self));
      break;

    case PROP_SHORT_ID:
      g_value_set_string (value, ide_runtime_get_short_id (self));
      break;

    case PROP_CATEGORY:
      g_value_set_string (value, ide_runtime_get_category (self));
      break;

    case PROP_DISPLAY_NAME:
      g_value_set_string (value, ide_runtime_get_display_name (self));
      break;

    case PROP_NAME:
      g_value_set_string (value, ide_runtime_get_name (self));
      break;

    IDE_GET_PROPERTY_STRING (ide_runtime, icon_name, ICON_NAME);
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_runtime_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  IdeRuntime *self = IDE_RUNTIME (object);

  switch (prop_id)
    {
    case PROP_ID:
      ide_runtime_set_id (self, g_value_get_string (value));
      break;

    case PROP_SHORT_ID:
      ide_runtime_set_short_id (self, g_value_get_string (value));
      break;

    case PROP_CATEGORY:
      ide_runtime_set_category (self, g_value_get_string (value));
      break;

    case PROP_DISPLAY_NAME:
      ide_runtime_set_display_name (self, g_value_get_string (value));
      break;

    case PROP_NAME:
      ide_runtime_set_name (self, g_value_get_string (value));
      break;

    IDE_SET_PROPERTY_STRING (ide_runtime, icon_name, ICON_NAME);
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_runtime_class_init (IdeRuntimeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  object_class->get_property = ide_runtime_get_property;
  object_class->set_property = ide_runtime_set_property;

  i_object_class->destroy = ide_runtime_destroy;
  i_object_class->repr = ide_runtime_repr;

  klass->contains_program_in_path = ide_runtime_real_contains_program_in_path;
  klass->prepare_configuration = ide_runtime_real_prepare_configuration;

  if (ide_is_flatpak ())
    klass->translate_file = ide_runtime_flatpak_translate_file;
  else
    klass->translate_file = ide_runtime_null_translate_file;

  properties [PROP_ID] =
    g_param_spec_string ("id",
                         "Id",
                         "The runtime identifier",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  properties [PROP_SHORT_ID] =
    g_param_spec_string ("short-id",
                         "Short Id",
                         "The short runtime identifier",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_CATEGORY] =
    g_param_spec_string ("category",
                         "Category",
                         "The runtime's category",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  properties [PROP_DISPLAY_NAME] =
    g_param_spec_string ("display-name",
                         "Display Name",
                         "Display Name",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  properties [PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "Name",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  IDE_DEFINE_STRING_PROPERTY ("icon-name", NULL, G_PARAM_READWRITE, ICON_NAME);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_runtime_init (IdeRuntime *self)
{
}

IDE_DEFINE_STRING_GETTER_PRIVATE (ide_runtime, IdeRuntime, IDE_TYPE_RUNTIME, icon_name)
IDE_DEFINE_STRING_SETTER_PRIVATE (ide_runtime, IdeRuntime, IDE_TYPE_RUNTIME, icon_name, ICON_NAME)

const gchar *
ide_runtime_get_id (IdeRuntime  *self)
{
  IdeRuntimePrivate *priv = ide_runtime_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_RUNTIME (self), NULL);

  return priv->id;
}

void
ide_runtime_set_id (IdeRuntime  *self,
                    const gchar *id)
{
  IdeRuntimePrivate *priv = ide_runtime_get_instance_private (self);

  g_return_if_fail (IDE_IS_RUNTIME (self));
  g_return_if_fail (id != NULL);

  if (g_set_str (&priv->id, id))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ID]);
}

const gchar *
ide_runtime_get_short_id (IdeRuntime  *self)
{
  IdeRuntimePrivate *priv = ide_runtime_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_RUNTIME (self), NULL);

  return priv->short_id ? priv->short_id : priv->id;
}

void
ide_runtime_set_short_id (IdeRuntime  *self,
                          const gchar *short_id)
{
  IdeRuntimePrivate *priv = ide_runtime_get_instance_private (self);

  g_return_if_fail (IDE_IS_RUNTIME (self));
  g_return_if_fail (short_id != NULL);

  if (g_set_str (&priv->short_id, short_id))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SHORT_ID]);
}

const gchar *
ide_runtime_get_category (IdeRuntime  *self)
{
  IdeRuntimePrivate *priv = ide_runtime_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_RUNTIME (self), NULL);
  g_return_val_if_fail (priv->category != NULL, "Host System");

  return priv->category;
}

void
ide_runtime_set_category (IdeRuntime  *self,
                          const gchar *category)
{
  IdeRuntimePrivate *priv = ide_runtime_get_instance_private (self);

  g_return_if_fail (IDE_IS_RUNTIME (self));

  if (category == NULL)
    category = _("Host System");

  if (g_set_str (&priv->category, category))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CATEGORY]);
}

const gchar *
ide_runtime_get_name (IdeRuntime *self)
{
  IdeRuntimePrivate *priv = ide_runtime_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_RUNTIME (self), NULL);

  return priv->name ? priv->name : priv->display_name;
}

void
ide_runtime_set_name (IdeRuntime  *self,
                      const gchar *name)
{
  IdeRuntimePrivate *priv = ide_runtime_get_instance_private (self);

  g_return_if_fail (IDE_IS_RUNTIME (self));

  if (g_set_str(&priv->name, name))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_NAME]);
    }
}

const gchar *
ide_runtime_get_display_name (IdeRuntime *self)
{
  IdeRuntimePrivate *priv = ide_runtime_get_instance_private (self);
  gchar *ret;

  g_return_val_if_fail (IDE_IS_RUNTIME (self), NULL);

  if (!(ret = priv->display_name))
    {
      if (!(ret = priv->name))
        ret = priv->id;
    }

  return ret;
}

void
ide_runtime_set_display_name (IdeRuntime  *self,
                              const gchar *display_name)
{
  IdeRuntimePrivate *priv = ide_runtime_get_instance_private (self);

  g_return_if_fail (IDE_IS_RUNTIME (self));

  if (g_set_str (&priv->display_name, display_name))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DISPLAY_NAME]);
}

IdeRuntime *
ide_runtime_new (const gchar *id,
                 const gchar *display_name)
{
  g_return_val_if_fail (id != NULL, NULL);
  g_return_val_if_fail (display_name != NULL, NULL);

  return g_object_new (IDE_TYPE_RUNTIME,
                       "id", id,
                       "display-name", display_name,
                       NULL);
}

void
ide_runtime_prepare_configuration (IdeRuntime       *self,
                                   IdeConfig *configuration)
{
  g_return_if_fail (IDE_IS_RUNTIME (self));
  g_return_if_fail (IDE_IS_CONFIG (configuration));

  IDE_RUNTIME_GET_CLASS (self)->prepare_configuration (self, configuration);
}

GQuark
ide_runtime_error_quark (void)
{
  static GQuark quark = 0;

  if G_UNLIKELY (quark == 0)
    quark = g_quark_from_static_string ("ide_runtime_error_quark");

  return quark;
}

/**
 * ide_runtime_translate_file:
 * @self: An #IdeRuntime
 * @file: a #GFile
 *
 * Translates the file from a path within the runtime to a path that can
 * be accessed from the host system.
 *
 * Returns: (transfer full) (not nullable): a #GFile.
 */
GFile *
ide_runtime_translate_file (IdeRuntime *self,
                            GFile      *file)
{
  GFile *ret = NULL;

  g_return_val_if_fail (IDE_IS_RUNTIME (self), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  if (IDE_RUNTIME_GET_CLASS (self)->translate_file)
    ret = IDE_RUNTIME_GET_CLASS (self)->translate_file (self, file);

  if (ret == NULL)
    ret = g_object_ref (file);

  return ret;
}

/**
 * ide_runtime_get_system_include_dirs:
 * @self: a #IdeRuntime
 *
 * Gets the system include dirs for the runtime. Usually, this is just
 * "/usr/include", but more complex runtimes may include additional.
 *
 * Returns: (transfer full) (array zero-terminated=1): A newly allocated
 *   string containing the include dirs.
 */
gchar **
ide_runtime_get_system_include_dirs (IdeRuntime *self)
{
  static const gchar *basic[] = { "/usr/include", NULL };

  g_return_val_if_fail (IDE_IS_RUNTIME (self), NULL);

  if (IDE_RUNTIME_GET_CLASS (self)->get_system_include_dirs)
    return IDE_RUNTIME_GET_CLASS (self)->get_system_include_dirs (self);

  return g_strdupv ((gchar **)basic);
}

/**
 * ide_runtime_get_triplet:
 * @self: a #IdeRuntime
 *
 * Gets the architecture triplet of the runtime.
 *
 * This can be used to ensure we're compiling for the right architecture
 * given the current device.
 *
 * Returns: (transfer full) (not nullable): the architecture triplet the runtime
 * will build for.
 */
IdeTriplet *
ide_runtime_get_triplet (IdeRuntime *self)
{
  IdeTriplet *ret = NULL;

  g_return_val_if_fail (IDE_IS_RUNTIME (self), NULL);

  if (IDE_RUNTIME_GET_CLASS (self)->get_triplet)
    ret = IDE_RUNTIME_GET_CLASS (self)->get_triplet (self);

  if (ret == NULL)
    ret = ide_triplet_new_from_system ();

  return ret;
}

/**
 * ide_runtime_get_arch:
 * @self: a #IdeRuntime
 *
 * Gets the architecture of the runtime.
 *
 * This can be used to ensure we're compiling for the right architecture
 * given the current device.
 *
 * This is strictly equivalent to calling #ide_triplet_get_arch on the result
 * of #ide_runtime_get_triplet.
 *
 * Returns: (transfer full) (not nullable): the name of the architecture
 * the runtime will build for.
 */
gchar *
ide_runtime_get_arch (IdeRuntime *self)
{
  gchar *ret = NULL;
  g_autoptr(IdeTriplet) triplet = NULL;

  g_return_val_if_fail (IDE_IS_RUNTIME (self), NULL);

  triplet = ide_runtime_get_triplet (self);
  ret = g_strdup (ide_triplet_get_arch (triplet));

  return ret;
}

/**
 * ide_runtime_supports_toolchain:
 * @self: a #IdeRuntime
 * @toolchain: the #IdeToolchain to check
 *
 * Informs wether a toolchain is supported by this.
 *
 * Returns: %TRUE if the toolchain is supported
 */
gboolean
ide_runtime_supports_toolchain (IdeRuntime   *self,
                                IdeToolchain *toolchain)
{
  const gchar *toolchain_id;

  g_return_val_if_fail (IDE_IS_RUNTIME (self), FALSE);
  g_return_val_if_fail (IDE_IS_TOOLCHAIN (toolchain), FALSE);

  toolchain_id = ide_toolchain_get_id (toolchain);
  if (g_strcmp0 (toolchain_id, "default") == 0)
    return TRUE;

  if (IDE_RUNTIME_GET_CLASS (self)->supports_toolchain)
    return IDE_RUNTIME_GET_CLASS (self)->supports_toolchain (self, toolchain);

  return TRUE;
}

/**
 * ide_runtime_prepare_to_run:
 * @self: a #IdeRuntime
 * @pipeline: (nullable): an #IdePipeline or %NULL for the current
 * @run_context: an #IdeRunContext
 *
 * Prepares a run context to run an application.
 *
 * The virtual function implementation should add to the run context anything
 * necessary to be able to run within the runtime.
 *
 * That might include pushing a new layer so that the command will run within
 * a subcommand such as "flatpak", "jhbuild", or "podman".
 *
 * This is meant to be able to run applications, so additional work is expected
 * of runtimes to ensure access to things like graphical displays.
 */
void
ide_runtime_prepare_to_run (IdeRuntime    *self,
                            IdePipeline   *pipeline,
                            IdeRunContext *run_context)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_RUNTIME (self));
  g_return_if_fail (!pipeline || IDE_IS_PIPELINE (pipeline));
  g_return_if_fail (IDE_IS_RUN_CONTEXT (run_context));

  if (IDE_RUNTIME_GET_CLASS (self)->prepare_to_run == NULL)
    IDE_EXIT;

  if (pipeline == NULL)
    {
      IdeContext *context = ide_object_get_context (IDE_OBJECT (self));
      IdeBuildManager *build_manager = ide_build_manager_from_context (context);

      pipeline = ide_build_manager_get_pipeline (build_manager);
    }

  g_return_if_fail (IDE_IS_PIPELINE (pipeline));
  g_return_if_fail (ide_pipeline_get_runtime (pipeline) == self);

  IDE_RUNTIME_GET_CLASS (self)->prepare_to_run (self, pipeline, run_context);

  /* Give the run_context access to some environment */
  ide_run_context_add_minimal_environment (run_context);

  IDE_EXIT;
}

/**
 * ide_runtime_prepare_to_build:
 * @self: a #IdeRuntime
 * @pipeline: (nullable): an #IdePipeline or %NULL for the current
 * @run_context: an #IdeRunContext
 *
 * Prepares a run context for running a build command.
 *
 * The virtual function implementation should add to the run context anything
 * necessary to be able to run within the runtime.
 *
 * That might include pushing a new layer so that the command will run within
 * a subcommand such as "flatpak", "jhbuild", or "podman".
 *
 * This is meant to be able to run a build command, so it may not require
 * access to some features like network or graphical displays.
 */
void
ide_runtime_prepare_to_build (IdeRuntime    *self,
                              IdePipeline   *pipeline,
                              IdeRunContext *run_context)
{
  IdeRuntime *expected;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_RUNTIME (self));
  g_return_if_fail (!pipeline || IDE_IS_PIPELINE (pipeline));
  g_return_if_fail (IDE_IS_RUN_CONTEXT (run_context));

  if (IDE_RUNTIME_GET_CLASS (self)->prepare_to_build == NULL)
    IDE_EXIT;

  if (pipeline == NULL)
    {
      IdeContext *context = ide_object_get_context (IDE_OBJECT (self));
      IdeBuildManager *build_manager = ide_build_manager_from_context (context);

      pipeline = ide_build_manager_get_pipeline (build_manager);
    }

  g_return_if_fail (IDE_IS_PIPELINE (pipeline));

  expected = ide_pipeline_get_runtime (pipeline);

  if (self != expected)
    g_debug ("Preparing run context for build using non-native runtime. \"%s\" instead of \"%s\".",
             ide_runtime_get_id (self),
             expected ? ide_runtime_get_id (expected) : "(null)");

  IDE_RUNTIME_GET_CLASS (self)->prepare_to_build (self, pipeline, run_context);

  IDE_EXIT;
}
