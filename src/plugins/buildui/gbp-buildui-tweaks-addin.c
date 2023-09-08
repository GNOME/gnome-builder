/* gbp-buildui-tweaks-addin.c
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

#define G_LOG_DOMAIN "gbp-buildui-tweaks-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-foundry.h>

#include "gbp-buildui-environment-editor.h"
#include "gbp-buildui-tweaks-addin.h"

struct _GbpBuilduiTweaksAddin
{
  IdeTweaksAddin parent_instance;
  IdeContext *context;
};

G_DEFINE_FINAL_TYPE (GbpBuilduiTweaksAddin, gbp_buildui_tweaks_addin, IDE_TYPE_TWEAKS_ADDIN)

static gpointer
map_item_cb (gpointer item,
             gpointer user_data)
{
  g_autoptr(IdeRuntime) runtime = item;

  return g_object_new (IDE_TYPE_TWEAKS_CHOICE,
                       "title", ide_runtime_get_display_name (runtime),
                       "value", g_variant_new_string (ide_runtime_get_id (runtime)),
                       NULL);
}

static GtkWidget *
create_runtime_list_cb (GbpBuilduiTweaksAddin *self,
                        IdeTweaksItem         *item,
                        IdeTweaksWidget       *instance)
{
  g_autoptr(GtkMapListModel) mapped = NULL;
  g_autoptr(GListModel) model = NULL;
  g_autoptr(IdeConfig) config = NULL;
  IdeTweaksComboRow *row;
  IdeTweaksBinding *binding;
  const char *runtime_id;
  guint selected = 0;
  guint n_items;

  g_assert (GBP_IS_BUILDUI_TWEAKS_ADDIN (self));
  g_assert (IDE_IS_TWEAKS_WIDGET (item));
  g_assert (IDE_IS_TWEAKS_WIDGET (instance));

  if (!(binding = ide_tweaks_widget_get_binding (IDE_TWEAKS_WIDGET (item))))
    return NULL;

  config = IDE_CONFIG (ide_tweaks_property_dup_object (IDE_TWEAKS_PROPERTY (binding)));
  g_object_get (config, "supported-runtimes", &model, NULL);
  mapped = gtk_map_list_model_new (g_object_ref (model), map_item_cb, NULL, NULL);
  n_items = g_list_model_get_n_items (model);
  runtime_id = ide_config_get_runtime_id (config);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(IdeRuntime) runtime = g_list_model_get_item (model, i);

      if (ide_str_equal0 (runtime_id, ide_runtime_get_id (runtime)))
        {
          selected = i;
          break;
        }
    }

  /* TODO: It would be nice to show an error here if we don't find
   * the runtime. Currently, we would just select the first item which
   * would modify the existing configuration.
   */

  row = g_object_new (IDE_TYPE_TWEAKS_COMBO_ROW,
                      "title", _("Runtime"),
                      "subtitle", _("The container used to build and run your application"),
                      "binding", binding,
                      "model", mapped,
                      "selected", selected,
                      NULL);

  return GTK_WIDGET (row);
}

static GtkWidget *
create_environ_editor_cb (GbpBuilduiTweaksAddin *self,
                          IdeTweaksWidget       *widget,
                          IdeTweaksWidget       *instance)
{
  IdeTweaksBinding *binding;

  g_assert (GBP_IS_BUILDUI_TWEAKS_ADDIN (self));

  if (!(binding = ide_tweaks_widget_get_binding (widget)))
    return NULL;

  return gbp_buildui_environment_editor_new (binding);
}

static void
on_duplicate_cb (GtkButton *button,
                 IdeConfig *config)
{
  IdeConfigManager *config_manager;
  IdeTweaksWindow *window;
  IdeContext *context;

  g_assert (GTK_IS_BUTTON (button));
  g_assert (IDE_IS_CONFIG (config));

  window = IDE_TWEAKS_WINDOW (gtk_widget_get_root (GTK_WIDGET (button)));

  context = ide_object_get_context (IDE_OBJECT (config));
  config_manager = ide_config_manager_from_context (context);
  ide_config_manager_duplicate (config_manager, config);

  ide_tweaks_window_navigate_initial (window);
}

static void
on_delete_cb (GtkButton *button,
              IdeConfig *config)
{
  IdeConfigManager *config_manager;
  IdeTweaksWindow *window;
  IdeContext *context;

  g_assert (GTK_IS_BUTTON (button));
  g_assert (IDE_IS_CONFIG (config));

  window = IDE_TWEAKS_WINDOW (gtk_widget_get_root (GTK_WIDGET (button)));

  context = ide_object_get_context (IDE_OBJECT (config));
  config_manager = ide_config_manager_from_context (context);
  ide_config_manager_delete (config_manager, config);

  ide_tweaks_window_navigate_initial (window);
}

static void
on_make_active_cb (GtkButton *button,
                   IdeConfig *config)
{
  IdeConfigManager *config_manager;
  IdeContext *context;

  g_assert (GTK_IS_BUTTON (button));
  g_assert (IDE_IS_CONFIG (config));

  context = ide_object_get_context (IDE_OBJECT (config));
  config_manager = ide_config_manager_from_context (context);
  ide_config_manager_set_current (config_manager, config);
}

static GtkWidget *
create_config_buttons_cb (GbpBuilduiTweaksAddin *self,
                          IdeTweaksWidget       *widget,
                          IdeTweaksWidget       *instance)
{
  g_autoptr(IdeConfig) config = NULL;
  IdeTweaksBinding *binding;
  GtkButton *duplicate;
  GtkButton *delete;
  GtkButton *make_active;
  GtkBox *box;

  g_assert (GBP_IS_BUILDUI_TWEAKS_ADDIN (self));
  g_assert (IDE_IS_TWEAKS_WIDGET (widget));
  g_assert (IDE_IS_TWEAKS_WIDGET (instance));

  if (!(binding = ide_tweaks_widget_get_binding (widget)))
    return NULL;

  if (!(config = IDE_CONFIG (ide_tweaks_property_dup_object (IDE_TWEAKS_PROPERTY (binding)))))
    return NULL;

  box = g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_HORIZONTAL,
                      "spacing", 12,
                      "homogeneous", TRUE,
                      NULL);

  duplicate = g_object_new (GTK_TYPE_BUTTON,
                            "label", _("Duplicate"),
                            "tooltip-text", _("Duplicate into new configuration"),
                            "can-shrink", TRUE,
                            "hexpand", TRUE,
                            NULL);
  g_signal_connect_object (duplicate,
                           "clicked",
                           G_CALLBACK (on_duplicate_cb),
                           config,
                           0);
  gtk_box_append (GTK_BOX (box), GTK_WIDGET (duplicate));

  make_active = g_object_new (GTK_TYPE_BUTTON,
                              "label", _("Make Active"),
                              "tooltip-text", _("Make configuration active and reload build pipeline"),
                              "can-shrink", TRUE,
                              "hexpand", TRUE,
                              NULL);
  g_signal_connect_object (make_active,
                           "clicked",
                           G_CALLBACK (on_make_active_cb),
                           config,
                           0);
  gtk_box_append (GTK_BOX (box), GTK_WIDGET (make_active));

  delete = g_object_new (GTK_TYPE_BUTTON,
                         "css-classes", IDE_STRV_INIT ("destructive-action"),
                         "label", _("Delete"),
                         "tooltip-text", _("Delete configuration"),
                         "can-shrink", TRUE,
                         "hexpand", TRUE,
                         NULL);
  g_signal_connect_object (delete,
                           "clicked",
                           G_CALLBACK (on_delete_cb),
                           config,
                           0);
  gtk_box_append (GTK_BOX (box), GTK_WIDGET (delete));

  return GTK_WIDGET (box);
}

static void
gbp_buildui_tweaks_addin_load (IdeTweaksAddin *addin,
                               IdeTweaks      *tweaks)
{
  GbpBuilduiTweaksAddin *self = (GbpBuilduiTweaksAddin *)addin;

  g_assert (GBP_IS_BUILDUI_TWEAKS_ADDIN (self));
  g_assert (IDE_IS_TWEAKS (tweaks));

  if ((self->context = ide_tweaks_get_context (tweaks)))
    {
      IdeRuntimeManager *runtime_manager = ide_runtime_manager_from_context (self->context);
      IdeRunCommands *run_commands = ide_run_commands_from_context (self->context);

      ide_tweaks_expose_object (tweaks, "Runtimes", G_OBJECT (runtime_manager));
      ide_tweaks_expose_object (tweaks, "RunCommands", G_OBJECT (run_commands));
    }
  else
    {
      g_autoptr(GListStore) store = g_list_store_new (G_TYPE_OBJECT);

      ide_tweaks_expose_object (tweaks, "Runtimes", G_OBJECT (store));
      ide_tweaks_expose_object (tweaks, "RunCommands", G_OBJECT (store));
    }

  ide_tweaks_addin_bind_callback (IDE_TWEAKS_ADDIN (self), create_runtime_list_cb);
  ide_tweaks_addin_bind_callback (IDE_TWEAKS_ADDIN (self), create_environ_editor_cb);
  ide_tweaks_addin_bind_callback (IDE_TWEAKS_ADDIN (self), create_config_buttons_cb);
  ide_tweaks_addin_set_resource_paths (IDE_TWEAKS_ADDIN (self),
                                       IDE_STRV_INIT ("/plugins/buildui/tweaks.ui"));

  IDE_TWEAKS_ADDIN_CLASS (gbp_buildui_tweaks_addin_parent_class)->load (addin, tweaks);
}

static void
gbp_buildui_tweaks_addin_unload (IdeTweaksAddin *addin,
                                 IdeTweaks      *tweaks)
{
  GbpBuilduiTweaksAddin *self = (GbpBuilduiTweaksAddin *)addin;

  g_assert (GBP_IS_BUILDUI_TWEAKS_ADDIN (self));
  g_assert (IDE_IS_TWEAKS (tweaks));

  self->context = NULL;
}

static void
gbp_buildui_tweaks_addin_class_init (GbpBuilduiTweaksAddinClass *klass)
{
  IdeTweaksAddinClass *tweaks_addin_class = IDE_TWEAKS_ADDIN_CLASS (klass);

  tweaks_addin_class->load = gbp_buildui_tweaks_addin_load;
  tweaks_addin_class->unload = gbp_buildui_tweaks_addin_unload;
}

static void
gbp_buildui_tweaks_addin_init (GbpBuilduiTweaksAddin *self)
{
}
