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

#include <libpeas/peas.h>

#include <libide-gui.h>
#include <libide-plugins.h>
#include <libide-threading.h>

#include "gbp-buildui-targets-dialog.h"

struct _GbpBuilduiTargetsDialog
{
  AdwWindow               parent_instance;

  GtkListBox             *list_box;
  GtkSpinner             *spinner;
  GListStore             *store;
  IdeExtensionSetAdapter *set;

  guint                   busy_count;
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
  AdwActionRow *row;
  const char *name;
  GtkWidget *check;

  g_assert (IDE_IS_BUILD_TARGET (target));

  name = ide_build_target_get_name (target);
  namev = g_variant_take_ref (g_variant_new_string (name ? name : ""));

  check = g_object_new (GTK_TYPE_CHECK_BUTTON,
                        "action-name", "build-manager.default-build-target",
                        "action-target", namev,
                        "valign", GTK_ALIGN_CENTER,
                        "can-focus", FALSE,
                        NULL);
  row = g_object_new (ADW_TYPE_ACTION_ROW,
                      "title", ide_build_target_get_display_name (item),
                      "activatable-widget", check,
                      NULL);
  gtk_widget_add_css_class (check, "checkimage");
  adw_action_row_add_suffix (row, check);

  return GTK_WIDGET (row);
}

static void
gbp_buildui_targets_dialog_get_targets_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  IdeBuildTargetProvider *provider = (IdeBuildTargetProvider *)object;
  g_autoptr(GbpBuilduiTargetsDialog) self = user_data;
  g_autoptr(GPtrArray) targets = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUILD_TARGET_PROVIDER (provider));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_BUILDUI_TARGETS_DIALOG (self));

  targets = ide_build_target_provider_get_targets_finish (provider, result, &error);
  IDE_PTR_ARRAY_SET_FREE_FUNC (targets, g_object_unref);

  if (targets != NULL)
    {
      for (guint i = 0; i < targets->len; i++)
        {
          IdeBuildTarget *target = g_ptr_array_index (targets, i);

          g_list_store_append (self->store, target);
        }
    }

  self->busy_count--;

  if (self->busy_count == 0)
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUSY]);
}

static void
gbp_buildui_targets_dialog_foreach_cb (IdeExtensionSetAdapter *set,
                                       PeasPluginInfo         *plugin_info,
                                       PeasExtension          *exten,
                                       gpointer                user_data)
{
  IdeBuildTargetProvider *provider = (IdeBuildTargetProvider *)exten;
  GbpBuilduiTargetsDialog *self = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (IDE_IS_BUILD_TARGET_PROVIDER (provider));
  g_assert (GBP_IS_BUILDUI_TARGETS_DIALOG (self));

  self->busy_count++;

  ide_build_target_provider_get_targets_async (provider,
                                               NULL,
                                               gbp_buildui_targets_dialog_get_targets_cb,
                                               g_object_ref (self));

  if (self->busy_count == 1)
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUSY]);
}

static void
gbp_buildui_targets_dialog_set_context (GbpBuilduiTargetsDialog *self,
                                        IdeContext              *context)
{
  IdeBuildManager *build_manager;

  IDE_ENTRY;

  g_assert (GBP_IS_BUILDUI_TARGETS_DIALOG (self));
  g_assert (!context || IDE_IS_CONTEXT (context));
  g_assert (self->set == NULL);

  if (context == NULL)
    IDE_EXIT;

  build_manager = ide_build_manager_from_context (context);
  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  "build-manager",
                                  G_ACTION_GROUP (build_manager));

  self->set = ide_extension_set_adapter_new (IDE_OBJECT (context),
                                             peas_engine_get_default (),
                                             IDE_TYPE_BUILD_TARGET_PROVIDER,
                                             NULL, NULL);


  ide_extension_set_adapter_foreach (self->set,
                                     gbp_buildui_targets_dialog_foreach_cb,
                                     self);

  IDE_EXIT;
}

static void
gbp_buildui_targets_dialog_dispose (GObject *object)
{
  GbpBuilduiTargetsDialog *self = (GbpBuilduiTargetsDialog *)object;

  g_clear_object (&self->store);
  ide_clear_and_destroy_object (&self->set);

  G_OBJECT_CLASS (gbp_buildui_targets_dialog_parent_class)->dispose (object);
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
      g_value_set_boolean (value, self->busy_count > 0);
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

  object_class->dispose = gbp_buildui_targets_dialog_dispose;
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

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/buildui/gbp-buildui-targets-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiTargetsDialog, list_box);
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiTargetsDialog, spinner);
}

static void
gbp_buildui_targets_dialog_init (GbpBuilduiTargetsDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->store = g_list_store_new (IDE_TYPE_BUILD_TARGET);
  gtk_list_box_bind_model (self->list_box, G_LIST_MODEL (self->store), create_target_row, NULL, NULL);
}
