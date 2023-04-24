/* gbp-buildui-status-popover.c
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

#define G_LOG_DOMAIN "gbp-buildui-status-popover"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-editor.h>
#include <libide-foundry.h>
#include <libide-gui.h>

#include "gbp-buildui-status-popover.h"

struct _GbpBuilduiStatusPopover
{
  GtkPopover       parent_instance;

  /* Owned references */
  GSignalGroup    *pipeline_signals;
  GHashTable      *deduplicator;

  /* Template references */
  GListStore      *diagnostics;
  GtkStack        *stack;
  GtkCustomFilter *error_filter;
  GtkCustomFilter *warning_filter;
  GtkStackPage    *errors;
  GListModel      *errors_model;
  GtkStackPage    *warnings;
  GListModel      *warnings_model;
};

G_DEFINE_FINAL_TYPE (GbpBuilduiStatusPopover, gbp_buildui_status_popover, GTK_TYPE_POPOVER)

static gboolean
warnings_title_cb (GBinding     *binding,
                   const GValue *from_value,
                   GValue       *to_value,
                   gpointer      user_data)
{
  guint n_items = g_value_get_uint (from_value);
  g_value_take_string (to_value,
                       /* translators: %u is replaced with the number of warnings */
                       g_strdup_printf (_("Warnings (%u)"), n_items));
  return TRUE;
}

static gboolean
errors_title_cb (GBinding     *binding,
                 const GValue *from_value,
                 GValue       *to_value,
                 gpointer      user_data)
{
  guint n_items = g_value_get_uint (from_value);
  g_value_take_string (to_value,
                       /* translators: %u is replaced with the number of errors */
                       g_strdup_printf (_("Errors (%u)"), n_items));
  return TRUE;
}

static void
gbp_buildui_status_popover_clear (GbpBuilduiStatusPopover *self)
{
  g_assert (GBP_IS_BUILDUI_STATUS_POPOVER (self));

  if (self->diagnostics != NULL)
    g_list_store_remove_all (self->diagnostics);

  if (self->deduplicator != NULL)
    g_hash_table_remove_all (self->deduplicator);
}

static void
gbp_buildui_status_popover_add_diagnsotic (GbpBuilduiStatusPopover *self,
                                           IdeDiagnostic           *diagnostic,
                                           IdePipeline             *pipeline)
{
  g_assert (GBP_IS_BUILDUI_STATUS_POPOVER (self));
  g_assert (IDE_IS_DIAGNOSTIC (diagnostic));
  g_assert (IDE_IS_PIPELINE (pipeline));

  if (g_hash_table_contains (self->deduplicator, diagnostic))
    return;

  g_hash_table_insert (self->deduplicator, g_object_ref (diagnostic), NULL);
  g_list_store_insert_sorted (self->diagnostics,
                              diagnostic,
                              (GCompareDataFunc)ide_diagnostic_compare,
                              NULL);
}

static void
gbp_buildui_status_popover_bind_pipeline (GbpBuilduiStatusPopover *self,
                                          IdePipeline             *pipeline,
                                          GSignalGroup            *signal_group)
{
  IDE_ENTRY;

  g_assert (GBP_IS_BUILDUI_STATUS_POPOVER (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (G_IS_SIGNAL_GROUP (signal_group));

  gbp_buildui_status_popover_clear (self);

  IDE_EXIT;
}

static void
gbp_buildui_status_popover_unbind_pipeline (GbpBuilduiStatusPopover *self,
                                            GSignalGroup            *signal_group)
{
  IDE_ENTRY;

  g_assert (GBP_IS_BUILDUI_STATUS_POPOVER (self));
  g_assert (G_IS_SIGNAL_GROUP (signal_group));

  IDE_EXIT;
}

static void
gbp_buildui_status_popover_activate_cb (GbpBuilduiStatusPopover *self,
                                        guint                    item_position,
                                        GtkListView             *list_view)
{
  g_autoptr(PanelPosition) position = NULL;
  g_autoptr(IdeDiagnostic) diagnostic = NULL;
  IdeWorkspace *workspace;
  IdeLocation *location;
  GListModel *model;

  IDE_ENTRY;

  g_assert (GBP_IS_BUILDUI_STATUS_POPOVER (self));
  g_assert (GTK_IS_LIST_VIEW (list_view));

  if (!(model = G_LIST_MODEL (gtk_list_view_get_model (list_view))) ||
      !(diagnostic = g_list_model_get_item (model, item_position)) ||
      !IDE_IS_DIAGNOSTIC (diagnostic) ||
      !(location = ide_diagnostic_get_location (diagnostic)))
    IDE_EXIT;

  workspace = ide_widget_get_workspace (GTK_WIDGET (self));
  position = panel_position_new ();
  ide_editor_focus_location (workspace, position, location);

  gtk_popover_popdown (GTK_POPOVER (self));

  IDE_EXIT;
}

static gboolean
is_warning (gpointer data,
            gpointer user_data)
{
  return ide_diagnostic_get_severity (data) == IDE_DIAGNOSTIC_WARNING;
}

static gboolean
is_error (gpointer data,
          gpointer user_data)
{
  return ide_diagnostic_get_severity (data) == IDE_DIAGNOSTIC_ERROR;
}

static void
gbp_buildui_status_popover_closed (GtkPopover *popover)
{
  GbpBuilduiStatusPopover *self = (GbpBuilduiStatusPopover *)popover;
  IdeWorkspace *workspace;
  IdePage *page;

  g_assert (GBP_IS_BUILDUI_STATUS_POPOVER (self));

  if ((workspace = ide_widget_get_workspace (GTK_WIDGET (self))) &&
      (page = ide_workspace_get_most_recent_page (workspace)))
    gtk_widget_grab_focus (GTK_WIDGET (page));
}

static void
gbp_buildui_status_popover_dispose (GObject *object)
{
  GbpBuilduiStatusPopover *self = (GbpBuilduiStatusPopover *)object;

  g_clear_object (&self->pipeline_signals);
  g_clear_pointer (&self->deduplicator, g_hash_table_unref);

  G_OBJECT_CLASS (gbp_buildui_status_popover_parent_class)->dispose (object);
}

static void
gbp_buildui_status_popover_class_init (GbpBuilduiStatusPopoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkPopoverClass *popover_class = GTK_POPOVER_CLASS (klass);

  object_class->dispose = gbp_buildui_status_popover_dispose;

  popover_class->closed = gbp_buildui_status_popover_closed;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/buildui/gbp-buildui-status-popover.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiStatusPopover, diagnostics);
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiStatusPopover, error_filter);
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiStatusPopover, errors);
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiStatusPopover, errors_model);
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiStatusPopover, stack);
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiStatusPopover, warning_filter);
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiStatusPopover, warnings);
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiStatusPopover, warnings_model);
  gtk_widget_class_bind_template_callback (widget_class, gbp_buildui_status_popover_activate_cb);

  g_type_ensure (IDE_TYPE_DIAGNOSTIC);
  g_type_ensure (IDE_TYPE_LOCATION);
}

static void
gbp_buildui_status_popover_init (GbpBuilduiStatusPopover *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_custom_filter_set_filter_func (self->warning_filter, is_warning, NULL, NULL);
  gtk_custom_filter_set_filter_func (self->error_filter, is_error, NULL, NULL);

  self->deduplicator = g_hash_table_new_full ((GHashFunc)ide_diagnostic_hash,
                                              (GEqualFunc)ide_diagnostic_equal,
                                              g_object_unref,
                                              NULL);
  self->pipeline_signals = g_signal_group_new (IDE_TYPE_PIPELINE);
  g_signal_connect_object (self->pipeline_signals,
                           "bind",
                           G_CALLBACK (gbp_buildui_status_popover_bind_pipeline),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->pipeline_signals,
                           "unbind",
                           G_CALLBACK (gbp_buildui_status_popover_unbind_pipeline),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_group_connect_object (self->pipeline_signals,
                                   "diagnostic",
                                   G_CALLBACK (gbp_buildui_status_popover_add_diagnsotic),
                                   self,
                                   G_CONNECT_SWAPPED);
  g_signal_group_connect_object (self->pipeline_signals,
                                   "started",
                                   G_CALLBACK (gbp_buildui_status_popover_clear),
                                   self,
                                   G_CONNECT_SWAPPED);

  g_object_bind_property_full (self->warnings_model, "n-items",
                               self->warnings, "title",
                               G_BINDING_SYNC_CREATE,
                               warnings_title_cb, NULL, NULL, NULL);
  g_object_bind_property_full (self->errors_model, "n-items",
                               self->errors, "title",
                               G_BINDING_SYNC_CREATE,
                               errors_title_cb, NULL, NULL, NULL);
}

static void
gbp_buildui_status_popover_connect (GbpBuilduiStatusPopover *self,
                                    IdeContext              *context)
{
  IdeBuildManager *build_manager;

  IDE_ENTRY;

  g_assert (GBP_IS_BUILDUI_STATUS_POPOVER (self));
  g_assert (IDE_IS_CONTEXT (context));

  build_manager = ide_build_manager_from_context (context);
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  g_object_bind_property (build_manager, "pipeline",
                          self->pipeline_signals, "target",
                          G_BINDING_SYNC_CREATE);

  IDE_EXIT;
}

GbpBuilduiStatusPopover *
gbp_buildui_status_popover_new (IdeContext *context)
{
  GbpBuilduiStatusPopover *self;

  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  self = g_object_new (GBP_TYPE_BUILDUI_STATUS_POPOVER, NULL);
  gbp_buildui_status_popover_connect (self, context);

  return self;
}

void
gbp_buildui_status_popover_set_page (GbpBuilduiStatusPopover *self,
                                     const char              *page)
{
  GtkWidget *visible_child;

  g_return_if_fail (GBP_IS_BUILDUI_STATUS_POPOVER (self));
  g_return_if_fail (page != NULL);

  gtk_stack_set_visible_child_name (self->stack, page);

  if ((visible_child = gtk_stack_get_visible_child (self->stack)))
    gtk_widget_child_focus (visible_child, GTK_DIR_TAB_FORWARD);
}
