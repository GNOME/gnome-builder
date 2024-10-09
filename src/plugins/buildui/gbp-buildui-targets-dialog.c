/*
 * gbp-buildui-targets-dialog.c
 *
 * Copyright 2022 Christian Hergert <>
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

#define G_LOG_DOMAIN "gbp-buildui-targets-dialog"

#include "config.h"

#include <glib/gi18n.h>
#include <libpeas.h>

#include <libide-gui.h>

#include "gbp-buildui-targets-dialog.h"

struct _GbpBuilduiTargetsDialog
{
  AdwWindow           parent_instance;
  AdwPreferencesPage *page;
  GtkListBox         *list_box;
  guint               busy : 1;
};

G_DEFINE_FINAL_TYPE (GbpBuilduiTargetsDialog, gbp_buildui_targets_dialog, ADW_TYPE_WINDOW)

enum {
  PROP_0,
  PROP_BUSY,
  PROP_CONTEXT,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static GtkWidget *
create_target_row (gpointer item,
                   gpointer user_data)
{
  IdeBuildTarget *target = item;
  g_autoptr(GVariant) namev = NULL;
  IdeArtifactKind kind;
  AdwActionRow *row;
  const char *name;
  GtkWidget *check;
  const char *pill_label;

  g_assert (IDE_IS_BUILD_TARGET (target));

  name = ide_build_target_get_name (target);
  namev = g_variant_take_ref (g_variant_new_string (name ? name : ""));

  check = g_object_new (GTK_TYPE_CHECK_BUTTON,
                        "action-name", "context.build-manager.default-build-target",
                        "css-classes", IDE_STRV_INIT ("checkimage"),
                        "action-target", namev,
                        "valign", GTK_ALIGN_CENTER,
                        "can-focus", FALSE,
                        NULL);
  row = g_object_new (ADW_TYPE_ACTION_ROW,
                      "title", ide_build_target_get_display_name (item),
                      "activatable-widget", check,
                      NULL);

  kind = ide_build_target_get_kind (target);

  switch (kind)
    {
    case IDE_ARTIFACT_KIND_SHARED_LIBRARY:
      pill_label = _("Shared");
      break;

    case IDE_ARTIFACT_KIND_STATIC_LIBRARY:
      pill_label = _("Static");
      break;

    case IDE_ARTIFACT_KIND_EXECUTABLE:
      pill_label = _("Executable");
      break;

    case IDE_ARTIFACT_KIND_FILE:
    case IDE_ARTIFACT_KIND_NONE:
    default:
      pill_label = NULL;
      break;
    }

  if (pill_label != NULL)
    adw_action_row_add_suffix (row,
                               g_object_new (GTK_TYPE_LABEL,
                                             "label", pill_label,
                                             "css-name", "button",
                                             "css-classes", IDE_STRV_INIT ("pill", "small"),
                                             "valign", GTK_ALIGN_CENTER,
                                             NULL));
  adw_action_row_add_suffix (row, check);

  return GTK_WIDGET (row);
}

static void
gbp_buildui_targets_dialog_list_targets_cb (GObject      *object,
                                            GAsyncResult *result,
                                            gpointer      user_data)
{
  IdeBuildManager *build_manager = (IdeBuildManager *)object;
  g_autoptr(GbpBuilduiTargetsDialog) self = user_data;
  g_autoptr(GListModel) model = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_MANAGER (build_manager));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_BUILDUI_TARGETS_DIALOG (self));

  self->busy = FALSE;
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUSY]);

  if (!(model = ide_build_manager_list_targets_finish (build_manager, result, &error)))
    {
      if (!ide_error_ignore (error))
        ide_object_warning (build_manager,
                            /* translators: %s is replaced with the error message */
                            _("Failed to list build targets: %s"),
                            error->message);
      IDE_EXIT;
    }

  gtk_list_box_bind_model (self->list_box, model, create_target_row, NULL, NULL);

  IDE_EXIT;
}

static void
gbp_buildui_targets_dialog_set_context (GbpBuilduiTargetsDialog *self,
                                        IdeContext              *context)
{
  g_autoptr(IdeActionMuxer) muxer = NULL;
  IdeBuildManager *build_manager;

  IDE_ENTRY;

  g_assert (GBP_IS_BUILDUI_TARGETS_DIALOG (self));
  g_assert (!context || IDE_IS_CONTEXT (context));

  if (context == NULL)
    IDE_EXIT;

  self->busy = TRUE;

  muxer = ide_context_ref_action_muxer (context);
  build_manager = ide_build_manager_from_context (context);
  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  "context",
                                  G_ACTION_GROUP (muxer));
  ide_build_manager_list_targets_async (build_manager,
                                        NULL,
                                        gbp_buildui_targets_dialog_list_targets_cb,
                                        g_object_ref (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUSY]);

  IDE_EXIT;
}

static gboolean
close_request_cb (GtkWindow *window)
{
  g_assert (GBP_IS_BUILDUI_TARGETS_DIALOG (window));

  gtk_widget_insert_action_group (GTK_WIDGET (window), "build-manager", NULL);
  gtk_window_destroy (window);

  return TRUE;
}

static void
gbp_buildui_targets_dialog_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  GbpBuilduiTargetsDialog *self = GBP_BUILDUI_TARGETS_DIALOG (object);

  switch (prop_id)
    {
    case PROP_BUSY:
      g_value_set_boolean (value, self->busy);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_buildui_targets_dialog_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  GbpBuilduiTargetsDialog *self = GBP_BUILDUI_TARGETS_DIALOG (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      gbp_buildui_targets_dialog_set_context (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_buildui_targets_dialog_class_init (GbpBuilduiTargetsDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gbp_buildui_targets_dialog_get_property;
  object_class->set_property = gbp_buildui_targets_dialog_set_property;

  properties [PROP_BUSY] =
    g_param_spec_boolean ("busy", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_CONTEXT] =
    g_param_spec_object ("context", NULL, NULL,
                         IDE_TYPE_CONTEXT,
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "window.close", NULL);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/buildui/gbp-buildui-targets-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiTargetsDialog, page);
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiTargetsDialog, list_box);
}

static void
gbp_buildui_targets_dialog_init (GbpBuilduiTargetsDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect (self, "close-request", G_CALLBACK (close_request_cb), NULL);

#ifdef DEVELOPMENT_BUILD
  gtk_widget_add_css_class (GTK_WIDGET (self), "devel");
#endif
}
