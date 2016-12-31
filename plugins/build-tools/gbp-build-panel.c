/* gbp-build-panel.c
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

#include <glib/gi18n.h>
#include <ide.h>

#include "egg-binding-group.h"
#include "egg-signal-group.h"

#include "gbp-build-panel.h"

struct _GbpBuildPanel
{
  PnlDockWidget        parent_instance;

  IdeBuildResult      *result;
  EggSignalGroup      *signals;
  EggBindingGroup     *bindings;
  GHashTable          *diags_hash;

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

G_DEFINE_TYPE (GbpBuildPanel, gbp_build_panel, PNL_TYPE_DOCK_WIDGET)

enum {
  PROP_0,
  PROP_RESULT,
  LAST_PROP
};

enum {
  COLUMN_DIAGNOSTIC,
  COLUMN_TEXT,
  LAST_COLUMN
};

static GParamSpec *properties [LAST_PROP];

static void
gbp_build_panel_diagnostic (GbpBuildPanel  *self,
                            IdeDiagnostic  *diagnostic,
                            IdeBuildResult *result)
{
  IdeDiagnosticSeverity severity;
  guint hash;

  IDE_ENTRY;

  g_assert (GBP_IS_BUILD_PANEL (self));
  g_assert (diagnostic != NULL);
  g_assert (IDE_IS_BUILD_RESULT (result));

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
gbp_build_panel_update_running_time (GbpBuildPanel *self)
{
  g_assert (GBP_IS_BUILD_PANEL (self));

  if (self->result != NULL)
    {
      GTimeSpan span;
      guint hours;
      guint minutes;
      guint seconds;
      gchar *text;

      span = ide_build_result_get_running_time (self->result);

      hours = span / G_TIME_SPAN_HOUR;
      minutes = (span % G_TIME_SPAN_HOUR) / G_TIME_SPAN_MINUTE;
      seconds = (span % G_TIME_SPAN_MINUTE) / G_TIME_SPAN_SECOND;

      text = g_strdup_printf ("%02u:%02u:%02u", hours, minutes, seconds);
      gtk_label_set_label (self->running_time_label, text);
      g_free (text);
    }
  else
    {
      gtk_label_set_label (self->running_time_label, NULL);
    }
}

static void
gbp_build_panel_connect (GbpBuildPanel  *self,
                         IdeBuildResult *result)
{
  g_return_if_fail (GBP_IS_BUILD_PANEL (self));
  g_return_if_fail (IDE_IS_BUILD_RESULT (result));
  g_return_if_fail (self->result == NULL);

  self->result = g_object_ref (result);
  self->error_count = 0;
  self->warning_count = 0;

  gtk_label_set_label (self->warnings_label, "—");
  gtk_label_set_label (self->errors_label, "—");

  egg_signal_group_set_target (self->signals, result);
  egg_binding_group_set_source (self->bindings, result);

  gtk_revealer_set_reveal_child (self->status_revealer, TRUE);

  gtk_stack_set_visible_child_name (self->stack, "diagnostics");
}

static void
gbp_build_panel_disconnect (GbpBuildPanel *self)
{
  g_return_if_fail (GBP_IS_BUILD_PANEL (self));

  gtk_revealer_set_reveal_child (self->status_revealer, FALSE);

  egg_signal_group_set_target (self->signals, NULL);
  egg_binding_group_set_source (self->bindings, NULL);
  g_clear_object (&self->result);
  g_hash_table_remove_all (self->diags_hash);
  gtk_list_store_clear (self->diagnostics_store);
  gtk_stack_set_visible_child_name (self->stack, "empty-state");
}

void
gbp_build_panel_set_result (GbpBuildPanel  *self,
                            IdeBuildResult *result)
{
  g_return_if_fail (GBP_IS_BUILD_PANEL (self));
  g_return_if_fail (!result || IDE_IS_BUILD_RESULT (result));

  if (result != self->result)
    {
      if (self->result)
        gbp_build_panel_disconnect (self);

      if (result)
        gbp_build_panel_connect (self, result);
    }
}

static void
gbp_build_panel_notify_running (GbpBuildPanel  *self,
                                GParamSpec     *pspec,
                                IdeBuildResult *result)
{
  g_assert (GBP_IS_BUILD_PANEL (self));
  g_assert (IDE_IS_BUILD_RESULT (result));

  gbp_build_panel_update_running_time (self);
}

static void
gbp_build_panel_notify_running_time (GbpBuildPanel  *self,
                                     GParamSpec     *pspec,
                                     IdeBuildResult *result)
{
  g_assert (GBP_IS_BUILD_PANEL (self));
  g_assert (IDE_IS_BUILD_RESULT (result));

  gbp_build_panel_update_running_time (self);
}

static void
gbp_build_panel_diagnostic_activated (GbpBuildPanel     *self,
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

  g_assert (GBP_IS_BUILD_PANEL (self));
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
gbp_build_panel_text_func (GtkCellLayout   *layout,
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
gbp_build_panel_destroy (GtkWidget *widget)
{
  GbpBuildPanel *self = (GbpBuildPanel *)widget;

  if (self->result)
    gbp_build_panel_disconnect (self);

  g_clear_object (&self->bindings);
  g_clear_object (&self->signals);
  g_clear_pointer (&self->diags_hash, g_hash_table_unref);

  GTK_WIDGET_CLASS (gbp_build_panel_parent_class)->destroy (widget);
}

static void
gbp_build_panel_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GbpBuildPanel *self = GBP_BUILD_PANEL(object);

  switch (prop_id)
    {
    case PROP_RESULT:
      g_value_set_object (value, self->result);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
gbp_build_panel_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GbpBuildPanel *self = GBP_BUILD_PANEL(object);

  switch (prop_id)
    {
    case PROP_RESULT:
      gbp_build_panel_set_result (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
gbp_build_panel_class_init (GbpBuildPanelClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gbp_build_panel_get_property;
  object_class->set_property = gbp_build_panel_set_property;

  widget_class->destroy = gbp_build_panel_destroy;

  properties [PROP_RESULT] =
    g_param_spec_object ("result",
                         "Result",
                         "Result",
                         IDE_TYPE_BUILD_RESULT,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/build-tools-plugin/gbp-build-panel.ui");
  gtk_widget_class_set_css_name (widget_class, "buildpanel");
  gtk_widget_class_bind_template_child (widget_class, GbpBuildPanel, diagnostics_column);
  gtk_widget_class_bind_template_child (widget_class, GbpBuildPanel, diagnostics_store);
  gtk_widget_class_bind_template_child (widget_class, GbpBuildPanel, diagnostics_text);
  gtk_widget_class_bind_template_child (widget_class, GbpBuildPanel, diagnostics_tree_view);
  gtk_widget_class_bind_template_child (widget_class, GbpBuildPanel, errors_label);
  gtk_widget_class_bind_template_child (widget_class, GbpBuildPanel, running_time_label);
  gtk_widget_class_bind_template_child (widget_class, GbpBuildPanel, stack);
  gtk_widget_class_bind_template_child (widget_class, GbpBuildPanel, status_label);
  gtk_widget_class_bind_template_child (widget_class, GbpBuildPanel, status_revealer);
  gtk_widget_class_bind_template_child (widget_class, GbpBuildPanel, warnings_label);

  g_type_ensure (IDE_TYPE_DIAGNOSTIC);
}

static void
gbp_build_panel_init (GbpBuildPanel *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->diags_hash = g_hash_table_new (NULL, NULL);

  g_object_set (self, "title", _("Build"), NULL);

  self->signals = egg_signal_group_new (IDE_TYPE_BUILD_RESULT);

  egg_signal_group_connect_object (self->signals,
                                   "diagnostic",
                                   G_CALLBACK (gbp_build_panel_diagnostic),
                                   self,
                                   G_CONNECT_SWAPPED);

  egg_signal_group_connect_object (self->signals,
                                   "notify::running",
                                   G_CALLBACK (gbp_build_panel_notify_running),
                                   self,
                                   G_CONNECT_SWAPPED);

  egg_signal_group_connect_object (self->signals,
                                   "notify::running-time",
                                   G_CALLBACK (gbp_build_panel_notify_running_time),
                                   self,
                                   G_CONNECT_SWAPPED);

  g_signal_connect_object (self->diagnostics_tree_view,
                           "row-activated",
                           G_CALLBACK (gbp_build_panel_diagnostic_activated),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (self->diagnostics_column),
                                      GTK_CELL_RENDERER (self->diagnostics_text),
                                      gbp_build_panel_text_func,
                                      self, NULL);


  self->bindings = egg_binding_group_new ();

  egg_binding_group_bind (self->bindings, "mode", self->status_label, "label",
                          G_BINDING_SYNC_CREATE);
}
