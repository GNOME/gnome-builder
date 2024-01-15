/* gbp-buildui-omni-bar-section.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-buildui-omni-bar-section"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-foundry.h>
#include <libide-gui.h>
#include <libide-vcs.h>

#include "gbp-buildui-omni-bar-section.h"

struct _GbpBuilduiOmniBarSection
{
  AdwBin          parent_instance;

  GSignalGroup   *build_manager_signals;

  GtkLabel       *config_ready_label;
  GtkLabel       *popover_branch_label;
  GtkLabel       *popover_build_message;
  GtkLabel       *popover_build_result_label;
  GtkLabel       *popover_config_label;
  GtkLabel       *popover_device_label;
  GtkLabel       *popover_errors_label;
  GtkLabel       *popover_last_build_time_label;
  GtkLabel       *popover_project_label;
  GtkLabel       *popover_runtime_label;
  GtkLabel       *popover_warnings_label;

  GtkRevealer    *popover_details_revealer;
};

G_DEFINE_FINAL_TYPE (GbpBuilduiOmniBarSection, gbp_buildui_omni_bar_section, ADW_TYPE_BIN)

static void
gbp_buildui_omni_bar_section_notify_can_build (GbpBuilduiOmniBarSection *self,
                                               GParamSpec               *pspec,
                                               IdeBuildManager          *build_manager)
{
  gboolean can_build;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDUI_OMNI_BAR_SECTION (self));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  can_build = ide_build_manager_get_can_build (build_manager);

  gtk_widget_set_visible (GTK_WIDGET (self->config_ready_label), !can_build);
}

static void
gbp_buildui_omni_bar_section_notify_pipeline (GbpBuilduiOmniBarSection *self,
                                              GParamSpec               *pspec,
                                              IdeBuildManager          *build_manager)
{
  IdePipeline *pipeline;
  const gchar *device_name = NULL;
  const gchar *runtime_name = NULL;
  const gchar *display_name = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDUI_OMNI_BAR_SECTION (self));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  if ((pipeline = ide_build_manager_get_pipeline (build_manager)))
    {
      IdeConfig *config = ide_pipeline_get_config (pipeline);
      IdeRuntime *runtime = ide_config_get_runtime (config);
      IdeDevice *device = ide_pipeline_get_device (pipeline);

      display_name = ide_config_get_display_name (config);

      if (runtime != NULL)
        {
          runtime_name = ide_runtime_get_display_name (runtime);
          if (runtime_name == NULL)
            runtime_name = ide_runtime_get_id (runtime);
        }

      if (device != NULL)
        device_name = ide_device_get_display_name (device);
    }

  gtk_label_set_label (self->popover_config_label, display_name);
  gtk_label_set_label (self->popover_device_label, device_name);

  if (runtime_name != NULL)
    {
      gtk_label_set_label (self->popover_runtime_label, runtime_name);
    }
  else
    {
      g_autofree gchar *markup = g_strdup_printf ("<b>%s</b>", _("Missing"));
      gtk_label_set_markup (self->popover_runtime_label, markup);
    }
}

static void
gbp_buildui_omni_bar_section_notify_error_count (GbpBuilduiOmniBarSection *self,
                                                 GParamSpec               *pspec,
                                                 IdeBuildManager          *build_manager)
{
  gchar str[12];
  guint count;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDUI_OMNI_BAR_SECTION (self));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  count = ide_build_manager_get_error_count (build_manager);
  g_snprintf (str, sizeof str, "%u", count);
  gtk_label_set_label (self->popover_errors_label, str);
}

static void
gbp_buildui_omni_bar_section_notify_warning_count (GbpBuilduiOmniBarSection *self,
                                                   GParamSpec               *pspec,
                                                   IdeBuildManager          *build_manager)
{
  gchar str[12];
  guint count;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDUI_OMNI_BAR_SECTION (self));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  count = ide_build_manager_get_warning_count (build_manager);
  g_snprintf (str, sizeof str, "%u", count);
  gtk_label_set_label (self->popover_warnings_label, str);
}

static void
gbp_buildui_omni_bar_section_notify_last_build_time (GbpBuilduiOmniBarSection *self,
                                                     GParamSpec               *pspec,
                                                     IdeBuildManager          *build_manager)
{
  g_autofree gchar *formatted = NULL;
  GDateTime *last_build_time;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDUI_OMNI_BAR_SECTION (self));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  if ((last_build_time = ide_build_manager_get_last_build_time (build_manager)))
    formatted = g_date_time_format (last_build_time, "%X");

  gtk_label_set_label (self->popover_last_build_time_label, formatted);
}

static void
gbp_buildui_omni_bar_section_notify_message (GbpBuilduiOmniBarSection *self,
                                             GParamSpec               *pspec,
                                             IdeBuildManager          *build_manager)
{
  g_autofree gchar *message = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDUI_OMNI_BAR_SECTION (self));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  message = ide_build_manager_get_message (build_manager);

  gtk_label_set_label (self->popover_build_message, message);
}

static void
gbp_buildui_omni_bar_section_build_started (GbpBuilduiOmniBarSection *self,
                                            IdePipeline         *pipeline,
                                            IdeBuildManager          *build_manager)
{
  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDUI_OMNI_BAR_SECTION (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  gtk_revealer_set_reveal_child (self->popover_details_revealer, TRUE);

  gtk_label_set_label (self->popover_build_result_label, _("Building…"));
  gtk_widget_remove_css_class (GTK_WIDGET (self->popover_build_result_label), "error");

  IDE_EXIT;
}

static void
gbp_buildui_omni_bar_section_build_failed (GbpBuilduiOmniBarSection *self,
                                           IdePipeline         *pipeline,
                                           IdeBuildManager          *build_manager)
{
  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDUI_OMNI_BAR_SECTION (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  gtk_label_set_label (self->popover_build_result_label, _("Failed"));
  gtk_widget_add_css_class (GTK_WIDGET (self->popover_build_result_label), "error");

  IDE_EXIT;
}

static void
gbp_buildui_omni_bar_section_build_finished (GbpBuilduiOmniBarSection *self,
                                             IdePipeline         *pipeline,
                                             IdeBuildManager          *build_manager)
{
  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDUI_OMNI_BAR_SECTION (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  gtk_label_set_label (self->popover_build_result_label, _("Success"));

  IDE_EXIT;
}

static void
gbp_buildui_omni_bar_section_bind_build_manager (GbpBuilduiOmniBarSection *self,
                                                 IdeBuildManager          *build_manager,
                                                 GSignalGroup             *signals)
{
  IdeContext *context;
  IdeVcs *vcs;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDUI_OMNI_BAR_SECTION (self));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));
  g_assert (G_IS_SIGNAL_GROUP (signals));

  gbp_buildui_omni_bar_section_notify_can_build (self, NULL, build_manager);
  gbp_buildui_omni_bar_section_notify_pipeline (self, NULL, build_manager);
  gbp_buildui_omni_bar_section_notify_message (self, NULL, build_manager);
  gbp_buildui_omni_bar_section_notify_error_count (self, NULL, build_manager);
  gbp_buildui_omni_bar_section_notify_warning_count (self, NULL, build_manager);
  gbp_buildui_omni_bar_section_notify_last_build_time (self, NULL, build_manager);

  context = ide_object_get_context (IDE_OBJECT (build_manager));
  vcs = ide_vcs_from_context (context);

  g_object_bind_property (context, "title",
                          self->popover_project_label, "label",
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property (vcs, "branch-name",
                          self->popover_branch_label, "label",
                          G_BINDING_SYNC_CREATE);
}

static void
gbp_buildui_omni_bar_section_dispose (GObject *object)
{
  GbpBuilduiOmniBarSection *self = (GbpBuilduiOmniBarSection *)object;

  g_assert (GBP_IS_BUILDUI_OMNI_BAR_SECTION (self));

  if (self->build_manager_signals)
    {
      g_signal_group_set_target (self->build_manager_signals, NULL);
      g_clear_object (&self->build_manager_signals);
    }

  G_OBJECT_CLASS (gbp_buildui_omni_bar_section_parent_class)->dispose (object);
}

static void
gbp_buildui_omni_bar_section_class_init (GbpBuilduiOmniBarSectionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gbp_buildui_omni_bar_section_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/buildui/gbp-buildui-omni-bar-section.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiOmniBarSection, config_ready_label);
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiOmniBarSection, popover_branch_label);
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiOmniBarSection, popover_build_message);
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiOmniBarSection, popover_build_result_label);
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiOmniBarSection, popover_config_label);
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiOmniBarSection, popover_details_revealer);
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiOmniBarSection, popover_device_label);
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiOmniBarSection, popover_errors_label);
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiOmniBarSection, popover_last_build_time_label);
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiOmniBarSection, popover_project_label);
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiOmniBarSection, popover_runtime_label);
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiOmniBarSection, popover_warnings_label);
}

static void
gbp_buildui_omni_bar_section_init (GbpBuilduiOmniBarSection *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->build_manager_signals = g_signal_group_new (IDE_TYPE_BUILD_MANAGER);
  g_signal_connect_object (self->build_manager_signals,
                           "bind",
                           G_CALLBACK (gbp_buildui_omni_bar_section_bind_build_manager),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_group_connect_object (self->build_manager_signals,
                                   "notify::can-build",
                                   G_CALLBACK (gbp_buildui_omni_bar_section_notify_can_build),
                                   self,
                                   G_CONNECT_SWAPPED);
  g_signal_group_connect_object (self->build_manager_signals,
                                   "notify::message",
                                   G_CALLBACK (gbp_buildui_omni_bar_section_notify_message),
                                   self,
                                   G_CONNECT_SWAPPED);
  g_signal_group_connect_object (self->build_manager_signals,
                                   "notify::pipeline",
                                   G_CALLBACK (gbp_buildui_omni_bar_section_notify_pipeline),
                                   self,
                                   G_CONNECT_SWAPPED);
  g_signal_group_connect_object (self->build_manager_signals,
                                   "notify::error-count",
                                   G_CALLBACK (gbp_buildui_omni_bar_section_notify_error_count),
                                   self,
                                   G_CONNECT_SWAPPED);
  g_signal_group_connect_object (self->build_manager_signals,
                                   "notify::warning-count",
                                   G_CALLBACK (gbp_buildui_omni_bar_section_notify_warning_count),
                                   self,
                                   G_CONNECT_SWAPPED);
  g_signal_group_connect_object (self->build_manager_signals,
                                   "notify::last-build-time",
                                   G_CALLBACK (gbp_buildui_omni_bar_section_notify_last_build_time),
                                   self,
                                   G_CONNECT_SWAPPED);
  g_signal_group_connect_object (self->build_manager_signals,
                                   "build-started",
                                   G_CALLBACK (gbp_buildui_omni_bar_section_build_started),
                                   self,
                                   G_CONNECT_SWAPPED);
  g_signal_group_connect_object (self->build_manager_signals,
                                   "build-failed",
                                   G_CALLBACK (gbp_buildui_omni_bar_section_build_failed),
                                   self,
                                   G_CONNECT_SWAPPED);
  g_signal_group_connect_object (self->build_manager_signals,
                                   "build-finished",
                                   G_CALLBACK (gbp_buildui_omni_bar_section_build_finished),
                                   self,
                                   G_CONNECT_SWAPPED);
}

void
gbp_buildui_omni_bar_section_set_context (GbpBuilduiOmniBarSection *self,
                                          IdeContext               *context)
{
  IdeBuildManager *build_manager;

  g_return_if_fail (GBP_IS_BUILDUI_OMNI_BAR_SECTION (self));
  g_return_if_fail (IDE_IS_CONTEXT (context));

  build_manager = ide_build_manager_from_context (context);
  g_signal_group_set_target (self->build_manager_signals, build_manager);
}
