/* gbp-flatpak-download-stage.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-flatpak-download-stage"

#include <glib/gi18n.h>
#include <libide-gui.h>

#include "gbp-flatpak-download-stage.h"
#include "gbp-flatpak-manifest.h"
#include "gbp-flatpak-util.h"

struct _GbpFlatpakDownloadStage
{
  IdePipelineStageLauncher parent_instance;

  gchar *state_dir;

  guint invalid : 1;
  guint force_update : 1;
};

G_DEFINE_TYPE (GbpFlatpakDownloadStage, gbp_flatpak_download_stage, IDE_TYPE_PIPELINE_STAGE_LAUNCHER)

enum {
  PROP_0,
  PROP_STATE_DIR,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
gbp_flatpak_download_stage_query (IdePipelineStage    *stage,
                                  IdePipeline *pipeline,
                                  GPtrArray        *targets,
                                  GCancellable     *cancellable)
{
  GbpFlatpakDownloadStage *self = (GbpFlatpakDownloadStage *)stage;
  IdeConfig *config;
  g_autofree gchar *staging_dir = NULL;
  g_autofree gchar *manifest_path = NULL;
  g_autofree gchar *stop_at_option = NULL;
  const gchar *src_dir;
  const gchar *primary_module;

  g_assert (GBP_IS_FLATPAK_DOWNLOAD_STAGE (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  /* Ignore downloads if there is no connection */
  if (!ide_application_has_network (IDE_APPLICATION_DEFAULT))
    {
      ide_pipeline_stage_log (stage,
                           IDE_BUILD_LOG_STDERR,
                           _("Network is not available, skipping downloads"),
                           -1);
      ide_pipeline_stage_set_completed (stage, TRUE);
      return;
    }

  config = ide_pipeline_get_config (pipeline);
  g_assert (!config || IDE_IS_CONFIG (config));

  if (!GBP_IS_FLATPAK_MANIFEST (config))
    {
      ide_pipeline_stage_set_completed (stage, TRUE);
      return;
    }

  if (self->invalid)
    {
      g_autoptr(IdeSubprocessLauncher) launcher = NULL;
      g_autofree gchar *arch = NULL;
      g_autofree gchar *arch_param = NULL;
      IdeRuntime *runtime;

      primary_module = gbp_flatpak_manifest_get_primary_module (GBP_FLATPAK_MANIFEST (config));
      manifest_path = gbp_flatpak_manifest_get_path (GBP_FLATPAK_MANIFEST (config));
      staging_dir = gbp_flatpak_get_staging_dir (pipeline);
      src_dir = ide_pipeline_get_srcdir (pipeline);

      launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                              G_SUBPROCESS_FLAGS_STDERR_PIPE);

      ide_subprocess_launcher_set_cwd (launcher, src_dir);
      ide_subprocess_launcher_set_run_on_host (launcher, FALSE);

      if (ide_is_flatpak ())
        {
          g_autofree gchar *user_dir = NULL;

          user_dir = g_build_filename (g_get_home_dir (), ".local", "share", "flatpak", NULL);
          ide_subprocess_launcher_setenv (launcher, "FLATPAK_USER_DIR", user_dir, TRUE);
          ide_subprocess_launcher_setenv (launcher, "XDG_RUNTIME_DIR", g_get_user_runtime_dir (), TRUE);
        }

      runtime = ide_pipeline_get_runtime (pipeline);
      arch = ide_runtime_get_arch (runtime);
      arch_param = g_strdup_printf ("--arch=%s", arch);

      ide_subprocess_launcher_push_argv (launcher, "flatpak-builder");
      ide_subprocess_launcher_push_argv (launcher, arch_param);
      ide_subprocess_launcher_push_argv (launcher, "--ccache");
      ide_subprocess_launcher_push_argv (launcher, "--force-clean");

      if (!dzl_str_empty0 (self->state_dir))
        {
          ide_subprocess_launcher_push_argv (launcher, "--state-dir");
          ide_subprocess_launcher_push_argv (launcher, self->state_dir);
        }

      ide_subprocess_launcher_push_argv (launcher, "--download-only");

      if (!self->force_update)
        ide_subprocess_launcher_push_argv (launcher, "--disable-updates");

      stop_at_option = g_strdup_printf ("--stop-at=%s", primary_module);
      ide_subprocess_launcher_push_argv (launcher, stop_at_option);

      ide_subprocess_launcher_push_argv (launcher, staging_dir);
      ide_subprocess_launcher_push_argv (launcher, manifest_path);

      ide_pipeline_stage_launcher_set_launcher (IDE_PIPELINE_STAGE_LAUNCHER (self), launcher);
      ide_pipeline_stage_set_completed (stage, FALSE);

      self->invalid = FALSE;
      self->force_update = FALSE;
    }
}

static void
gbp_flatpak_download_stage_finalize (GObject *object)
{
  GbpFlatpakDownloadStage *self = (GbpFlatpakDownloadStage *)object;

  g_assert (GBP_IS_FLATPAK_DOWNLOAD_STAGE (self));

  g_clear_pointer (&self->state_dir, g_free);

  G_OBJECT_CLASS (gbp_flatpak_download_stage_parent_class)->finalize (object);
}

static void
gbp_flatpak_download_stage_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  GbpFlatpakDownloadStage *self = GBP_FLATPAK_DOWNLOAD_STAGE (object);

  switch (prop_id)
    {
    case PROP_STATE_DIR:
      self->state_dir = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_flatpak_download_stage_class_init (GbpFlatpakDownloadStageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdePipelineStageClass *stage_class = IDE_PIPELINE_STAGE_CLASS (klass);

  object_class->finalize = gbp_flatpak_download_stage_finalize;
  object_class->set_property = gbp_flatpak_download_stage_set_property;

  stage_class->query = gbp_flatpak_download_stage_query;

  /**
   * GbpFlatpakDownloadStage:state-dir:
   *
   * The "state-dir" is the flatpak-builder state directory, to be used
   * as a parameter to "flatpak-builder --state-dir".
   *
   * Since: 3.28
   */
  properties [PROP_STATE_DIR] =
    g_param_spec_string ("state-dir",
                         "State Dir",
                         "The flatpak-builder state directory",
                         NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_flatpak_download_stage_init (GbpFlatpakDownloadStage *self)
{
  self->invalid = TRUE;

  /* Allow downloads to fail in case we can still make progress */
  ide_pipeline_stage_launcher_set_ignore_exit_status (IDE_PIPELINE_STAGE_LAUNCHER (self), TRUE);
}

void
gbp_flatpak_download_stage_force_update (GbpFlatpakDownloadStage *self)
{
  g_return_if_fail (GBP_IS_FLATPAK_DOWNLOAD_STAGE (self));

  self->force_update = TRUE;
  self->invalid = TRUE;
}
