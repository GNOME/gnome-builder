/* gbp-buildui-pane.c
 *
 * Copyright 2015-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-buildui-pane"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-editor.h>
#include <libide-foundry.h>
#include <libide-gtk.h>

#include "ide-pipeline-stage-private.h"

#include "gbp-buildui-pane.h"
#include "gbp-buildui-stage-row.h"

struct _GbpBuilduiPane
{
  IdePane              parent_instance;

  /* Owned references */
  IdePipeline         *pipeline;
  GSignalGroup        *pipeline_signals;

  /* Template widgets */
  GtkLabel            *build_status_label;
  GtkLabel            *time_completed_label;
  GtkListBox          *stages_list_box;

  guint                shift_pressed : 1;
};

G_DEFINE_FINAL_TYPE (GbpBuilduiPane, gbp_buildui_pane, IDE_TYPE_PANE)

enum {
  PROP_0,
  PROP_PIPELINE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
gbp_buildui_pane_update_running_time (GbpBuilduiPane *self)
{
  g_autofree gchar *text = NULL;

  g_assert (GBP_IS_BUILDUI_PANE (self));

  if (self->pipeline != NULL)
    {
      IdeBuildManager *build_manager;
      IdeContext *context;
      GTimeSpan span;

      context = ide_widget_get_context (GTK_WIDGET (self));
      build_manager = ide_build_manager_from_context (context);

      span = ide_build_manager_get_running_time (build_manager);
      text = ide_g_time_span_to_label (span);
      gtk_label_set_label (self->time_completed_label, text);
    }
  else
    gtk_label_set_label (self->time_completed_label, "—");
}

static GtkWidget *
gbp_buildui_pane_create_stage_row_cb (gpointer data,
                                      gpointer user_data)
{
  return gbp_buildui_stage_row_new (IDE_PIPELINE_STAGE (data));
}

static void
gbp_buildui_pane_bind_pipeline (GbpBuilduiPane *self,
                                IdePipeline    *pipeline,
                                GSignalGroup   *signals)
{
  g_assert (GBP_IS_BUILDUI_PANE (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (G_IS_LIST_MODEL (pipeline));
  g_assert (self->pipeline == NULL);
  g_assert (G_IS_SIGNAL_GROUP (signals));

  self->pipeline = g_object_ref (pipeline);

  gtk_label_set_label (self->time_completed_label, "—");
  gtk_label_set_label (self->build_status_label, "—");

  gtk_list_box_bind_model (self->stages_list_box,
                           G_LIST_MODEL (pipeline),
                           gbp_buildui_pane_create_stage_row_cb,
                           NULL, NULL);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PIPELINE]);
}

static void
gbp_buildui_pane_unbind_pipeline (GbpBuilduiPane *self,
                                  GSignalGroup   *signals)
{
  IDE_ENTRY;

  g_return_if_fail (GBP_IS_BUILDUI_PANE (self));
  g_return_if_fail (!self->pipeline || IDE_IS_PIPELINE (self->pipeline));

  if (!gtk_widget_in_destruction (GTK_WIDGET (self)))
    {
      gtk_list_box_bind_model (self->stages_list_box, NULL, NULL, NULL, NULL);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PIPELINE]);
    }

  g_clear_object (&self->pipeline);

  IDE_EXIT;
}

void
gbp_buildui_pane_set_pipeline (GbpBuilduiPane   *self,
                               IdePipeline *pipeline)
{
  g_return_if_fail (GBP_IS_BUILDUI_PANE (self));
  g_return_if_fail (!pipeline || IDE_IS_PIPELINE (pipeline));

  if (self->pipeline_signals != NULL)
    g_signal_group_set_target (self->pipeline_signals, pipeline);
}

static void
gbp_buildui_pane_notify_message (GbpBuilduiPane  *self,
                                 GParamSpec      *pspec,
                                 IdeBuildManager *build_manager)
{
  g_autofree gchar *message = NULL;
  IdePipeline *pipeline;

  g_assert (GBP_IS_BUILDUI_PANE (self));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  message = ide_build_manager_get_message (build_manager);
  pipeline = ide_build_manager_get_pipeline (build_manager);

  gtk_label_set_label (self->build_status_label, message);

  if (ide_pipeline_get_phase (pipeline) == IDE_PIPELINE_PHASE_FAILED)
    gtk_widget_add_css_class (GTK_WIDGET (self->build_status_label), "error");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (self->build_status_label), "error");
}

static void
gbp_buildui_pane_context_handler (GtkWidget  *widget,
                                  IdeContext *context)
{
  GbpBuilduiPane *self = (GbpBuilduiPane *)widget;
  IdeBuildManager *build_manager;

  IDE_ENTRY;

  g_assert (GBP_IS_BUILDUI_PANE (self));
  g_assert (!context || IDE_IS_CONTEXT (context));

  if (context == NULL)
    IDE_EXIT;

  build_manager = ide_build_manager_from_context (context);

  g_signal_connect_object (build_manager,
                           "notify::message",
                           G_CALLBACK (gbp_buildui_pane_notify_message),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (build_manager,
                           "notify::running-time",
                           G_CALLBACK (gbp_buildui_pane_update_running_time),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (build_manager,
                           "build-started",
                           G_CALLBACK (gbp_buildui_pane_update_running_time),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (build_manager,
                           "build-finished",
                           G_CALLBACK (gbp_buildui_pane_update_running_time),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (build_manager,
                           "build-failed",
                           G_CALLBACK (gbp_buildui_pane_update_running_time),
                           self,
                           G_CONNECT_SWAPPED);

  IDE_EXIT;
}

static void
gbp_buildui_pane_stage_row_activated (GbpBuilduiPane     *self,
                                      GbpBuilduiStageRow *row,
                                      GtkListBox         *list_box)
{
  IdePipelineStage *stage;
  IdePipelinePhase phase;

  g_assert (GBP_IS_BUILDUI_PANE (self));
  g_assert (GBP_IS_BUILDUI_STAGE_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  if (self->pipeline == NULL)
    return;

  stage = gbp_buildui_stage_row_get_stage (row);
  g_assert (IDE_IS_PIPELINE_STAGE (stage));

  if (self->shift_pressed)
    ide_pipeline_stage_set_completed (stage, FALSE);

  phase = _ide_pipeline_stage_get_phase (stage);

  ide_pipeline_build_async (self->pipeline,
                            phase & IDE_PIPELINE_PHASE_MASK,
                            NULL, NULL, NULL);
}

static gboolean
key_modifiers_cb (GbpBuilduiPane        *self,
                  GdkModifierType        state,
                  GtkEventControllerKey *key)
{
  g_assert (GBP_IS_BUILDUI_PANE (self));
  g_assert (GTK_IS_EVENT_CONTROLLER_KEY (key));

  self->shift_pressed = !!(state & GDK_SHIFT_MASK);

  return FALSE;
}

static void
gbp_buildui_pane_dispose (GObject *object)
{
  GbpBuilduiPane *self = (GbpBuilduiPane *)object;

  g_signal_group_set_target (self->pipeline_signals, NULL);
  g_clear_object (&self->pipeline);

  G_OBJECT_CLASS (gbp_buildui_pane_parent_class)->dispose (object);
}

static void
gbp_buildui_pane_finalize (GObject *object)
{
  GbpBuilduiPane *self = (GbpBuilduiPane *)object;

  g_clear_object (&self->pipeline_signals);

  G_OBJECT_CLASS (gbp_buildui_pane_parent_class)->finalize (object);
}

static void
gbp_buildui_pane_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GbpBuilduiPane *self = GBP_BUILDUI_PANE (object);

  switch (prop_id)
    {
    case PROP_PIPELINE:
      g_value_set_object (value, self->pipeline);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_buildui_pane_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GbpBuilduiPane *self = GBP_BUILDUI_PANE (object);

  switch (prop_id)
    {
    case PROP_PIPELINE:
      gbp_buildui_pane_set_pipeline (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_buildui_pane_class_init (GbpBuilduiPaneClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gbp_buildui_pane_dispose;
  object_class->finalize = gbp_buildui_pane_finalize;
  object_class->get_property = gbp_buildui_pane_get_property;
  object_class->set_property = gbp_buildui_pane_set_property;

  properties [PROP_PIPELINE] =
    g_param_spec_object ("pipeline",
                         NULL,
                         NULL,
                         IDE_TYPE_PIPELINE,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/buildui/gbp-buildui-pane.ui");
  gtk_widget_class_set_css_name (widget_class, "buildpanel");
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiPane, build_status_label);
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiPane, time_completed_label);
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiPane, stages_list_box);
  gtk_widget_class_bind_template_callback (widget_class, key_modifiers_cb);

  g_type_ensure (IDE_TYPE_DIAGNOSTIC);
}

static void
gbp_buildui_pane_init (GbpBuilduiPane *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->pipeline_signals = g_signal_group_new (IDE_TYPE_PIPELINE);
  g_signal_connect_object (self->pipeline_signals,
                           "bind",
                           G_CALLBACK (gbp_buildui_pane_bind_pipeline),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->pipeline_signals,
                           "unbind",
                           G_CALLBACK (gbp_buildui_pane_unbind_pipeline),
                           self,
                           G_CONNECT_SWAPPED);

  panel_widget_set_title (PANEL_WIDGET (self), _("Build Pipeline"));

  ide_widget_set_context_handler (self, gbp_buildui_pane_context_handler);

  g_signal_connect_swapped (self->stages_list_box,
                            "row-activated",
                            G_CALLBACK (gbp_buildui_pane_stage_row_activated),
                            self);
}
