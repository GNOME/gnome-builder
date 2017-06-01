/* ide-build-panel.c
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-build-panel"

#include <glib/gi18n.h>
#include <ide.h>

#include "ide-build-panel.h"

struct _IdeBuildPanel
{
  DzlDockWidget        parent_instance;

  GHashTable          *diags_hash;
  IdeBuildPipeline    *pipeline;

  GtkListStore        *diagnostics_store;
  GtkCellRendererText *diagnostics_text;
  GtkTreeViewColumn   *diagnostics_column;
  GtkTreeView         *diagnostics_tree_view;
  GtkLabel            *errors_label;
  GtkLabel            *running_time_label;
  GtkStack            *stack;
  GtkRevealer         *status_revealer;
  GtkLabel            *status_label;
  GtkLabel            *warnings_label;

  guint                error_count;
  guint                warning_count;
};

G_DEFINE_TYPE (IdeBuildPanel, ide_build_panel, DZL_TYPE_DOCK_WIDGET)

enum {
  COLUMN_DIAGNOSTIC,
  COLUMN_TEXT,
  LAST_COLUMN
};

enum {
  PROP_0,
  PROP_PIPELINE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
ide_build_panel_diagnostic (IdeBuildPanel    *self,
                            IdeDiagnostic    *diagnostic,
                            IdeBuildPipeline *pipeline)
{
  IdeDiagnosticSeverity severity;
  guint hash;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_PANEL (self));
  g_assert (diagnostic != NULL);
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));

  severity = ide_diagnostic_get_severity (diagnostic);

  if (severity == IDE_DIAGNOSTIC_WARNING)
    {
      g_autofree gchar *str = NULL;

      self->warning_count++;

      str = g_strdup_printf (ngettext ("%d warning", "%d warnings", self->warning_count), self->warning_count);
      gtk_label_set_label (self->warnings_label, str);
    }
  else if (severity == IDE_DIAGNOSTIC_ERROR || severity == IDE_DIAGNOSTIC_FATAL)
    {
      g_autofree gchar *str = NULL;

      self->error_count++;

      str = g_strdup_printf (ngettext ("%d error", "%d errors", self->error_count), self->error_count);
      gtk_label_set_label (self->errors_label, str);
    }

  hash = ide_diagnostic_hash (diagnostic);

  if (g_hash_table_insert (self->diags_hash, GUINT_TO_POINTER (hash), NULL))
    {
      GtkTreeModel *model = GTK_TREE_MODEL (self->diagnostics_store);
      GtkTreeIter iter;
      gint left = 0;
      gint right = gtk_tree_model_iter_n_children (model, NULL) - 1;
      gint middle = 0;
      gint cmpval = 1;

      /* Binary search to locate the target position. */
      while (left <= right)
        {
          g_autoptr(IdeDiagnostic) item = NULL;

          middle = (left + right) / 2;

          gtk_tree_model_iter_nth_child (model, &iter, NULL, middle);
          gtk_tree_model_get (model, &iter,
                              COLUMN_DIAGNOSTIC, &item,
                              -1);

          cmpval = ide_diagnostic_compare (item, diagnostic);

          if (cmpval < 0)
            left = middle + 1;
          else if (cmpval > 0)
            right = middle - 1;
          else
            break;
        }

      /* If we binary searched and middle was compared previous
       * to our new diagnostic, advance one position.
       */
      if (cmpval < 0)
        middle++;

      gtk_list_store_insert (self->diagnostics_store, &iter, middle);
      gtk_list_store_set (self->diagnostics_store, &iter,
                          COLUMN_DIAGNOSTIC, diagnostic,
                          COLUMN_TEXT, ide_diagnostic_get_text (diagnostic),
                          -1);
    }

  IDE_EXIT;
}

static void
ide_build_panel_update_running_time (IdeBuildPanel *self)
{
  g_autofree gchar *text = NULL;

  g_assert (IDE_IS_BUILD_PANEL (self));

  if (self->pipeline != NULL)
    {
      IdeBuildManager *build_manager;
      IdeContext *context;
      GTimeSpan span;

      context = ide_widget_get_context (GTK_WIDGET (self));
      build_manager = ide_context_get_build_manager (context);

      span = ide_build_manager_get_running_time (build_manager);
      text = ide_g_time_span_to_label (span);
    }

  gtk_label_set_label (self->running_time_label, text);
}

static void
ide_build_panel_started (IdeBuildPanel    *self,
                         IdeBuildPhase     phase,
                         IdeBuildPipeline *pipeline)
{
  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_PANEL (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));

  if (phase >= IDE_BUILD_PHASE_BUILD)
    {
      self->error_count = 0;
      self->warning_count = 0;

      gtk_label_set_label (self->warnings_label, "—");
      gtk_label_set_label (self->errors_label, "—");

      gtk_list_store_clear (self->diagnostics_store);
      g_hash_table_remove_all (self->diags_hash);
    }

  IDE_EXIT;
}

static void
ide_build_panel_connect (IdeBuildPanel    *self,
                         IdeBuildPipeline *pipeline)
{
  g_return_if_fail (IDE_IS_BUILD_PANEL (self));
  g_return_if_fail (IDE_IS_BUILD_PIPELINE (pipeline));
  g_return_if_fail (self->pipeline == NULL);

  self->pipeline = g_object_ref (pipeline);
  self->error_count = 0;
  self->warning_count = 0;

  gtk_label_set_label (self->warnings_label, "—");
  gtk_label_set_label (self->errors_label, "—");

  g_signal_connect_object (pipeline,
                           "diagnostic",
                           G_CALLBACK (ide_build_panel_diagnostic),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (pipeline,
                           "started",
                           G_CALLBACK (ide_build_panel_started),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_revealer_set_reveal_child (self->status_revealer, TRUE);

  gtk_stack_set_visible_child_name (self->stack, "diagnostics");
}

static void
ide_build_panel_disconnect (IdeBuildPanel *self)
{
  g_return_if_fail (IDE_IS_BUILD_PANEL (self));
  g_return_if_fail (IDE_IS_BUILD_PIPELINE (self->pipeline));

  g_signal_handlers_disconnect_by_func (self->pipeline,
                                        G_CALLBACK (ide_build_panel_diagnostic),
                                        self);
  g_clear_object (&self->pipeline);

  gtk_revealer_set_reveal_child (self->status_revealer, FALSE);

  g_hash_table_remove_all (self->diags_hash);
  gtk_list_store_clear (self->diagnostics_store);
  gtk_stack_set_visible_child_name (self->stack, "empty-state");
}

void
ide_build_panel_set_pipeline (IdeBuildPanel    *self,
                              IdeBuildPipeline *pipeline)
{
  g_return_if_fail (IDE_IS_BUILD_PANEL (self));
  g_return_if_fail (!pipeline || IDE_IS_BUILD_PIPELINE (pipeline));

  if (pipeline != self->pipeline)
    {
      if (self->pipeline)
        ide_build_panel_disconnect (self);

      if (pipeline)
        ide_build_panel_connect (self, pipeline);
    }
}

static void
ide_build_panel_diagnostic_activated (IdeBuildPanel     *self,
                                      GtkTreePath       *path,
                                      GtkTreeViewColumn *colun,
                                      GtkTreeView       *tree_view)
{
  g_autoptr(IdeUri) uri = NULL;
  IdeSourceLocation *loc;
  IdeDiagnostic *diagnostic = NULL;
  IdeWorkbench *workbench;
  GtkTreeModel *model;
  GtkTreeIter iter;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_PANEL (self));
  g_assert (path != NULL);
  g_assert (GTK_IS_TREE_VIEW_COLUMN (colun));
  g_assert (GTK_IS_TREE_VIEW (tree_view));

  model = gtk_tree_view_get_model (tree_view);
  if (!gtk_tree_model_get_iter (model, &iter, path))
    IDE_EXIT;

  gtk_tree_model_get (model, &iter,
                      COLUMN_DIAGNOSTIC, &diagnostic,
                      -1);

  if (diagnostic == NULL)
    IDE_EXIT;

  loc = ide_diagnostic_get_location (diagnostic);
  if (loc == NULL)
    IDE_EXIT;

  uri = ide_source_location_get_uri (loc);
  if (uri == NULL)
    IDE_EXIT;

  workbench = ide_widget_get_workbench (GTK_WIDGET (self));

  ide_workbench_open_uri_async (workbench,
                                uri,
                                "editor",
                                IDE_WORKBENCH_OPEN_FLAGS_NONE,
                                NULL,
                                NULL,
                                NULL);

  IDE_EXIT;
}

static void
ide_build_panel_text_func (GtkCellLayout   *layout,
                           GtkCellRenderer *renderer,
                           GtkTreeModel    *model,
                           GtkTreeIter     *iter,
                           gpointer         user_data)
{
  g_autoptr(IdeDiagnostic) diagnostic = NULL;
  g_auto(GValue) value = { 0 };

  gtk_tree_model_get (model, iter,
                      COLUMN_DIAGNOSTIC, &diagnostic,
                      -1);

  g_value_init (&value, G_TYPE_STRING);

  if G_LIKELY (diagnostic != NULL)
    {
      GString *str;
      const gchar *text;
      g_autofree gchar *name = NULL;
      IdeSourceLocation *location;
      GFile *gfile = NULL;
      guint line = 0;
      guint column = 0;

      location = ide_diagnostic_get_location (diagnostic);

      if (location != NULL)
        {
          IdeFile *file;

          if (NULL != (file = ide_source_location_get_file (location)))
            {
              if (NULL != (gfile = ide_file_get_file (file)))
                {
                  name = g_file_get_basename (gfile);
                  line = ide_source_location_get_line (location);
                  column = ide_source_location_get_line_offset (location);
                }
            }

        }

      str = g_string_new (NULL);

      if (name != NULL)
        g_string_append_printf (str, "<b>%s:%u:%u</b>\n",
                                name, line + 1, column + 1);

      text = ide_diagnostic_get_text (diagnostic);

      if (text != NULL)
        g_string_append (str, text);

      g_value_take_string (&value, g_string_free (str, FALSE));
      g_object_set_property (G_OBJECT (renderer), "markup", &value);

      return;
    }

  g_object_set_property (G_OBJECT (renderer), "text", &value);
}

static void
ide_build_panel_context_handler (GtkWidget  *widget,
                                 IdeContext *context)
{
  IdeBuildPanel *self = (IdeBuildPanel *)widget;
  IdeBuildManager *build_manager;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_PANEL (self));
  g_assert (!context || IDE_IS_CONTEXT (context));

  if (context == NULL)
    IDE_EXIT;

  build_manager = ide_context_get_build_manager (context);

  g_object_bind_property (build_manager, "message",
                          self->status_label, "label",
                          G_BINDING_SYNC_CREATE);

  g_signal_connect_object (build_manager,
                           "notify::running-time",
                           G_CALLBACK (ide_build_panel_update_running_time),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (build_manager,
                           "build-started",
                           G_CALLBACK (ide_build_panel_update_running_time),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (build_manager,
                           "build-finished",
                           G_CALLBACK (ide_build_panel_update_running_time),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (build_manager,
                           "build-failed",
                           G_CALLBACK (ide_build_panel_update_running_time),
                           self,
                           G_CONNECT_SWAPPED);

  IDE_EXIT;
}

static void
ide_build_panel_destroy (GtkWidget *widget)
{
  IdeBuildPanel *self = (IdeBuildPanel *)widget;

  if (self->pipeline != NULL)
    ide_build_panel_disconnect (self);

  g_clear_pointer (&self->diags_hash, g_hash_table_unref);

  GTK_WIDGET_CLASS (ide_build_panel_parent_class)->destroy (widget);
}

static void
ide_build_panel_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeBuildPanel *self = IDE_BUILD_PANEL (object);

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
ide_build_panel_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  IdeBuildPanel *self = IDE_BUILD_PANEL (object);

  switch (prop_id)
    {
    case PROP_PIPELINE:
      ide_build_panel_set_pipeline (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_build_panel_class_init (IdeBuildPanelClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  widget_class->destroy = ide_build_panel_destroy;

  object_class->get_property = ide_build_panel_get_property;
  object_class->set_property = ide_build_panel_set_property;

  properties [PROP_PIPELINE] =
    g_param_spec_object ("pipeline",
                         NULL,
                         NULL,
                         IDE_TYPE_BUILD_PIPELINE,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/buildui/ide-build-panel.ui");
  gtk_widget_class_set_css_name (widget_class, "buildpanel");
  gtk_widget_class_bind_template_child (widget_class, IdeBuildPanel, diagnostics_column);
  gtk_widget_class_bind_template_child (widget_class, IdeBuildPanel, diagnostics_store);
  gtk_widget_class_bind_template_child (widget_class, IdeBuildPanel, diagnostics_text);
  gtk_widget_class_bind_template_child (widget_class, IdeBuildPanel, diagnostics_tree_view);
  gtk_widget_class_bind_template_child (widget_class, IdeBuildPanel, errors_label);
  gtk_widget_class_bind_template_child (widget_class, IdeBuildPanel, running_time_label);
  gtk_widget_class_bind_template_child (widget_class, IdeBuildPanel, stack);
  gtk_widget_class_bind_template_child (widget_class, IdeBuildPanel, status_label);
  gtk_widget_class_bind_template_child (widget_class, IdeBuildPanel, status_revealer);
  gtk_widget_class_bind_template_child (widget_class, IdeBuildPanel, warnings_label);

  g_type_ensure (IDE_TYPE_DIAGNOSTIC);
}

static void
ide_build_panel_init (IdeBuildPanel *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->diags_hash = g_hash_table_new (NULL, NULL);

  g_object_set (self, "title", _("Build"), NULL);

  ide_widget_set_context_handler (self, ide_build_panel_context_handler);

  g_signal_connect_object (self->diagnostics_tree_view,
                           "row-activated",
                           G_CALLBACK (ide_build_panel_diagnostic_activated),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (self->diagnostics_column),
                                      GTK_CELL_RENDERER (self->diagnostics_text),
                                      ide_build_panel_text_func,
                                      self, NULL);
}
