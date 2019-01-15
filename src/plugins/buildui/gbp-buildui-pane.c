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

#include "ide-pipeline-stage-private.h"

#include "gbp-buildui-pane.h"
#include "gbp-buildui-stage-row.h"

struct _GbpBuilduiPane
{
  IdePane              parent_instance;

  /* Owned references */
  GHashTable          *diags_hash;
  IdePipeline    *pipeline;
  DzlSignalGroup      *pipeline_signals;

  /* Template widgets */
  GtkLabel            *build_status_label;
  GtkLabel            *time_completed_label;
  GtkNotebook         *notebook;
  GtkScrolledWindow   *errors_page;
  IdeFancyTreeView    *errors_tree_view;
  GtkScrolledWindow   *warnings_page;
  IdeFancyTreeView    *warnings_tree_view;
  GtkListStore        *diagnostics_store;
  GtkListBox          *stages_list_box;

  guint                error_count;
  guint                warning_count;
};

G_DEFINE_TYPE (GbpBuilduiPane, gbp_buildui_pane, IDE_TYPE_PANE)

enum {
  COLUMN_DIAGNOSTIC,
  LAST_COLUMN
};

enum {
  PROP_0,
  PROP_PIPELINE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
set_warnings_label (GbpBuilduiPane *self,
                    const gchar   *label)
{
  gtk_container_child_set (GTK_CONTAINER (self->notebook), GTK_WIDGET (self->warnings_page),
                           "tab-label", label,
                           NULL);
}

static void
set_errors_label (GbpBuilduiPane *self,
                  const gchar   *label)
{
  gtk_container_child_set (GTK_CONTAINER (self->notebook), GTK_WIDGET (self->errors_page),
                           "tab-label", label,
                           NULL);
}

static void
gbp_buildui_pane_diagnostic (GbpBuilduiPane   *self,
                             IdeDiagnostic    *diagnostic,
                             IdePipeline *pipeline)
{
  IdeDiagnosticSeverity severity;
  guint hash;

  IDE_ENTRY;

  g_assert (GBP_IS_BUILDUI_PANE (self));
  g_assert (diagnostic != NULL);
  g_assert (IDE_IS_PIPELINE (pipeline));

  severity = ide_diagnostic_get_severity (diagnostic);

  if (severity == IDE_DIAGNOSTIC_WARNING)
    {
      g_autofree gchar *label = NULL;

      self->warning_count++;

      label = g_strdup_printf ("%s (%u)", _("Warnings"), self->warning_count);
      set_warnings_label (self, label);
    }
  else if (severity == IDE_DIAGNOSTIC_ERROR || severity == IDE_DIAGNOSTIC_FATAL)
    {
      g_autofree gchar *label = NULL;

      self->error_count++;

      label = g_strdup_printf ("%s (%u)", _("Errors"), self->error_count);
      set_errors_label (self, label);
    }
  else
    {
      /* TODO: Figure out design for "Others" Column like Notes? */
    }

  hash = ide_diagnostic_hash (diagnostic);

  if (g_hash_table_insert (self->diags_hash, GUINT_TO_POINTER (hash), NULL))
    {
      GtkTreeIter iter;

      dzl_gtk_list_store_insert_sorted (self->diagnostics_store,
                                        &iter,
                                        diagnostic,
                                        COLUMN_DIAGNOSTIC,
                                        (GCompareDataFunc)ide_diagnostic_compare,
                                        NULL);
      gtk_list_store_set (self->diagnostics_store, &iter,
                          COLUMN_DIAGNOSTIC, diagnostic,
                          -1);
    }

  IDE_EXIT;
}

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
      text = dzl_g_time_span_to_label (span);
      gtk_label_set_label (self->time_completed_label, text);
    }
  else
    gtk_label_set_label (self->time_completed_label, "—");
}

static void
gbp_buildui_pane_started (GbpBuilduiPane   *self,
                          IdePipelinePhase     phase,
                          IdePipeline *pipeline)
{
  IDE_ENTRY;

  g_assert (GBP_IS_BUILDUI_PANE (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  if (phase >= IDE_PIPELINE_PHASE_BUILD)
    {
      self->error_count = 0;
      self->warning_count = 0;

      set_warnings_label (self, _("Warnings"));
      set_errors_label (self, _("Errors"));

      gtk_list_store_clear (self->diagnostics_store);
      g_hash_table_remove_all (self->diags_hash);
    }

  IDE_EXIT;
}

static GtkWidget *
gbp_buildui_pane_create_stage_row_cb (gpointer data,
                                     gpointer user_data)
{
  IdePipelineStage *stage = data;

  g_assert (IDE_IS_PIPELINE_STAGE (stage));
  g_assert (GBP_IS_BUILDUI_PANE (user_data));

  return gbp_buildui_stage_row_new (stage);
}

static void
gbp_buildui_pane_bind_pipeline (GbpBuilduiPane   *self,
                                IdePipeline *pipeline,
                                DzlSignalGroup   *signals)
{
  g_assert (GBP_IS_BUILDUI_PANE (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (G_IS_LIST_MODEL (pipeline));
  g_assert (self->pipeline == NULL);
  g_assert (DZL_IS_SIGNAL_GROUP (signals));

  self->pipeline = g_object_ref (pipeline);
  self->error_count = 0;
  self->warning_count = 0;

  set_warnings_label (self, _("Warnings"));
  set_errors_label (self, _("Errors"));

  gtk_label_set_label (self->time_completed_label, "—");
  gtk_label_set_label (self->build_status_label, "—");

  gtk_list_box_bind_model (self->stages_list_box,
                           G_LIST_MODEL (pipeline),
                           gbp_buildui_pane_create_stage_row_cb,
                           self, NULL);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PIPELINE]);
}

static void
gbp_buildui_pane_unbind_pipeline (GbpBuilduiPane *self,
                                  DzlSignalGroup *signals)
{
  g_return_if_fail (GBP_IS_BUILDUI_PANE (self));
  g_return_if_fail (!self->pipeline || IDE_IS_PIPELINE (self->pipeline));

  g_clear_object (&self->pipeline);

  if (!gtk_widget_in_destruction (GTK_WIDGET (self)))
    {
      g_hash_table_remove_all (self->diags_hash);
      gtk_list_store_clear (self->diagnostics_store);
      gtk_container_foreach (GTK_CONTAINER (self->stages_list_box),
                             (GtkCallback) gtk_widget_destroy,
                             NULL);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PIPELINE]);
    }
}

void
gbp_buildui_pane_set_pipeline (GbpBuilduiPane   *self,
                               IdePipeline *pipeline)
{
  g_return_if_fail (GBP_IS_BUILDUI_PANE (self));
  g_return_if_fail (!pipeline || IDE_IS_PIPELINE (pipeline));

  if (self->pipeline_signals != NULL)
    dzl_signal_group_set_target (self->pipeline_signals, pipeline);
}

static void
gbp_buildui_pane_diagnostic_activated (GbpBuilduiPane    *self,
                                       GtkTreePath       *path,
                                       GtkTreeViewColumn *colun,
                                       GtkTreeView       *tree_view)
{
  g_autoptr(IdeDiagnostic) diagnostic = NULL;
  IdeWorkspace *workspace;
  IdeLocation *loc;
  GtkTreeModel *model;
  IdeSurface *surface;
  GtkTreeIter iter;

  IDE_ENTRY;

  g_assert (GBP_IS_BUILDUI_PANE (self));
  g_assert (path != NULL);
  g_assert (GTK_IS_TREE_VIEW_COLUMN (colun));
  g_assert (GTK_IS_TREE_VIEW (tree_view));

  model = gtk_tree_view_get_model (tree_view);
  if (!gtk_tree_model_get_iter (model, &iter, path))
    IDE_EXIT;

  gtk_tree_model_get (model, &iter,
                      COLUMN_DIAGNOSTIC, &diagnostic,
                      -1);

  if (diagnostic == NULL || !(loc = ide_diagnostic_get_location (diagnostic)))
    IDE_EXIT;

  workspace = ide_widget_get_workspace (GTK_WIDGET (self));
  surface = ide_workspace_get_surface_by_name (workspace, "editor");
  ide_editor_surface_focus_location (IDE_EDITOR_SURFACE (surface), loc);

  IDE_EXIT;
}

static void
gbp_buildui_pane_text_func (GtkCellLayout   *layout,
                            GtkCellRenderer *renderer,
                            GtkTreeModel    *model,
                            GtkTreeIter     *iter,
                            gpointer         user_data)
{
  IdeCellRendererFancy *fancy = (IdeCellRendererFancy *)renderer;
  g_autoptr(IdeDiagnostic) diagnostic = NULL;

  gtk_tree_model_get (model, iter,
                      COLUMN_DIAGNOSTIC, &diagnostic,
                      -1);

  if G_LIKELY (diagnostic != NULL)
    {
      g_autofree gchar *title = NULL;
      g_autofree gchar *name = NULL;
      IdeLocation *location;
      const gchar *text;
      GFile *file = NULL;
      guint line = 0;
      guint column = 0;

      location = ide_diagnostic_get_location (diagnostic);

      if (location != NULL)
        {
          if ((file = ide_location_get_file (location)))
            {
              name = g_file_get_basename (file);
              line = ide_location_get_line (location);
              column = ide_location_get_line_offset (location);
            }
        }

      title = g_strdup_printf ("%s:%u:%u", name ?: "", line + 1, column + 1);
      ide_cell_renderer_fancy_take_title (fancy, g_steal_pointer (&title));

      text = ide_diagnostic_get_text (diagnostic);
      ide_cell_renderer_fancy_set_body (fancy, text);
    }
  else
    {
      ide_cell_renderer_fancy_set_title (fancy, NULL);
      ide_cell_renderer_fancy_set_body (fancy, NULL);
    }
}

static void
gbp_buildui_pane_notify_message (GbpBuilduiPane  *self,
                                 GParamSpec      *pspec,
                                 IdeBuildManager *build_manager)
{
  g_autofree gchar *message = NULL;
  IdePipeline *pipeline;
  GtkStyleContext *style;

  g_assert (GBP_IS_BUILDUI_PANE (self));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  message = ide_build_manager_get_message (build_manager);
  pipeline = ide_build_manager_get_pipeline (build_manager);

  gtk_label_set_label (self->build_status_label, message);

  style = gtk_widget_get_style_context (GTK_WIDGET (self->build_status_label));

  if (ide_pipeline_get_phase (pipeline) == IDE_PIPELINE_PHASE_FAILED)
    gtk_style_context_add_class (style, GTK_STYLE_CLASS_ERROR);
  else
    gtk_style_context_remove_class (style, GTK_STYLE_CLASS_ERROR);
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

static gboolean
gbp_buildui_pane_diagnostic_tooltip (GbpBuilduiPane *self,
                                     gint            x,
                                     gint            y,
                                     gboolean        keyboard_mode,
                                     GtkTooltip     *tooltip,
                                     GtkTreeView    *tree_view)
{
  GtkTreeModel *model = NULL;
  GtkTreeIter iter;

  g_assert (GBP_IS_BUILDUI_PANE (self));
  g_assert (GTK_IS_TOOLTIP (tooltip));
  g_assert (GTK_IS_TREE_VIEW (tree_view));

  if (gtk_tree_view_get_tooltip_context (tree_view, &x, &y, keyboard_mode, &model, NULL, &iter))
    {
      g_autoptr(IdeDiagnostic) diag = NULL;

      gtk_tree_model_get (model, &iter,
                          COLUMN_DIAGNOSTIC, &diag,
                          -1);

      if (diag != NULL)
        {
          g_autofree gchar *text = ide_diagnostic_get_text_for_display (diag);

          gtk_tooltip_set_text (tooltip, text);

          return TRUE;
        }
    }

  return FALSE;
}

static gboolean
diagnostic_is_warning (GtkTreeModel *model,
                       GtkTreeIter  *iter,
                       gpointer      user_data)
{
  g_autoptr(IdeDiagnostic) diag = NULL;
  IdeDiagnosticSeverity severity = 0;

  gtk_tree_model_get (model, iter,
                      COLUMN_DIAGNOSTIC, &diag,
                      -1);

  if (diag != NULL)
    severity = ide_diagnostic_get_severity (diag);

  return severity <= IDE_DIAGNOSTIC_WARNING;
}

static gboolean
diagnostic_is_error (GtkTreeModel *model,
                     GtkTreeIter  *iter,
                     gpointer      user_data)
{
  g_autoptr(IdeDiagnostic) diag = NULL;
  IdeDiagnosticSeverity severity = 0;

  gtk_tree_model_get (model, iter,
                      COLUMN_DIAGNOSTIC, &diag,
                      -1);

  if (diag != NULL)
    severity = ide_diagnostic_get_severity (diag);

  return severity > IDE_DIAGNOSTIC_WARNING;
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

  phase = _ide_pipeline_stage_get_phase (stage);

  ide_pipeline_build_async (self->pipeline,
                                  phase & IDE_PIPELINE_PHASE_MASK,
                                  NULL, NULL, NULL);
}

static void
gbp_buildui_pane_destroy (GtkWidget *widget)
{
  GbpBuilduiPane *self = (GbpBuilduiPane *)widget;

  if (self->pipeline_signals != NULL)
    dzl_signal_group_set_target (self->pipeline_signals, NULL);

  g_clear_pointer (&self->diags_hash, g_hash_table_unref);

  g_clear_object (&self->pipeline_signals);
  g_clear_object (&self->pipeline);

  GTK_WIDGET_CLASS (gbp_buildui_pane_parent_class)->destroy (widget);
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

  widget_class->destroy = gbp_buildui_pane_destroy;

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
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiPane, notebook);
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiPane, errors_page);
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiPane, errors_tree_view);
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiPane, warnings_page);
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiPane, warnings_tree_view);
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiPane, diagnostics_store);
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiPane, stages_list_box);

  g_type_ensure (IDE_TYPE_CELL_RENDERER_FANCY);
  g_type_ensure (IDE_TYPE_DIAGNOSTIC);
  g_type_ensure (IDE_TYPE_FANCY_TREE_VIEW);
}

static void
gbp_buildui_pane_init (GbpBuilduiPane *self)
{
  GtkTreeModel *filter;

  gtk_widget_init_template (GTK_WIDGET (self));

  self->pipeline_signals = dzl_signal_group_new (IDE_TYPE_PIPELINE);

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

  dzl_signal_group_connect_object (self->pipeline_signals,
                                   "diagnostic",
                                   G_CALLBACK (gbp_buildui_pane_diagnostic),
                                   self,
                                   G_CONNECT_SWAPPED);

  dzl_signal_group_connect_object (self->pipeline_signals,
                                   "started",
                                   G_CALLBACK (gbp_buildui_pane_started),
                                   self,
                                   G_CONNECT_SWAPPED);

  self->diags_hash = g_hash_table_new (NULL, NULL);

  g_object_set (self, "title", _("Build Issues"), NULL);

  ide_widget_set_context_handler (self, gbp_buildui_pane_context_handler);

  g_signal_connect_swapped (self->warnings_tree_view,
                           "row-activated",
                           G_CALLBACK (gbp_buildui_pane_diagnostic_activated),
                           self);

  g_signal_connect_swapped (self->warnings_tree_view,
                           "query-tooltip",
                           G_CALLBACK (gbp_buildui_pane_diagnostic_tooltip),
                           self);

  g_signal_connect_swapped (self->errors_tree_view,
                           "row-activated",
                           G_CALLBACK (gbp_buildui_pane_diagnostic_activated),
                           self);

  g_signal_connect_swapped (self->errors_tree_view,
                           "query-tooltip",
                           G_CALLBACK (gbp_buildui_pane_diagnostic_tooltip),
                           self);

  filter = gtk_tree_model_filter_new (GTK_TREE_MODEL (self->diagnostics_store), NULL);
  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (filter),
                                          diagnostic_is_warning, NULL, NULL);
  gtk_tree_view_set_model (GTK_TREE_VIEW (self->warnings_tree_view), GTK_TREE_MODEL (filter));
  g_object_unref (filter);

  filter = gtk_tree_model_filter_new (GTK_TREE_MODEL (self->diagnostics_store), NULL);
  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (filter),
                                          diagnostic_is_error, NULL, NULL);
  gtk_tree_view_set_model (GTK_TREE_VIEW (self->errors_tree_view), GTK_TREE_MODEL (filter));
  g_object_unref (filter);

  ide_fancy_tree_view_set_data_func (IDE_FANCY_TREE_VIEW (self->warnings_tree_view),
                                     gbp_buildui_pane_text_func, self, NULL);

  ide_fancy_tree_view_set_data_func (IDE_FANCY_TREE_VIEW (self->errors_tree_view),
                                     gbp_buildui_pane_text_func, self, NULL);

  g_signal_connect_swapped (self->stages_list_box,
                            "row-activated",
                            G_CALLBACK (gbp_buildui_pane_stage_row_activated),
                            self);
}
