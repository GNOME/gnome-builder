/* ide-config-view.c
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#define G_LOG_DOMAIN "ide-config-view"

#include <glib/gi18n.h>
#include <libpeas/peas.h>

#include "config/ide-config-view.h"
#include "config/ide-config-view-addin.h"
#include "util/ide-gtk.h"

struct _IdeConfigView
{
  GtkBin            parent_instance;

  DzlPreferences   *preferences;

  /* Owned references */
  IdeConfiguration *config;
  PeasExtensionSet *addins;
  GCancellable     *cancellable;
};

static void ide_config_view_connect    (IdeConfigView    *self,
                                        IdeConfiguration *config);
static void ide_config_view_disconnect (IdeConfigView    *self);

G_DEFINE_TYPE (IdeConfigView, ide_config_view, GTK_TYPE_BIN)

enum {
  PROP_0,
  PROP_CONFIG,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

/**
 * ide_config_view_new:
 *
 * Create a new #IdeConfigView.
 *
 * Returns: (transfer full): a newly created #IdeConfigView
 */
GtkWidget *
ide_config_view_new (void)
{
  return g_object_new (IDE_TYPE_CONFIG_VIEW, NULL);
}

static void
ide_config_view_destroy (GtkWidget *widget)
{
  IdeConfigView *self = (IdeConfigView *)widget;

  g_assert (IDE_IS_CONFIG_VIEW (self));

  if (self->config)
    ide_config_view_disconnect (self);

  GTK_WIDGET_CLASS (ide_config_view_parent_class)->destroy (widget);
}

static void
ide_config_view_finalize (GObject *object)
{
  IdeConfigView *self = (IdeConfigView *)object;

  g_assert (self->addins == NULL);
  g_assert (self->config == NULL);
  g_assert (self->cancellable == NULL);

  G_OBJECT_CLASS (ide_config_view_parent_class)->finalize (object);
}

static void
ide_config_view_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeConfigView *self = IDE_CONFIG_VIEW (object);

  switch (prop_id)
    {
    case PROP_CONFIG:
      g_value_set_object (value, ide_config_view_get_config (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_config_view_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  IdeConfigView *self = IDE_CONFIG_VIEW (object);

  switch (prop_id)
    {
    case PROP_CONFIG:
      ide_config_view_set_config (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_config_view_class_init (IdeConfigViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ide_config_view_finalize;
  object_class->get_property = ide_config_view_get_property;
  object_class->set_property = ide_config_view_set_property;

  widget_class->destroy = ide_config_view_destroy;

  properties [PROP_CONFIG] =
    g_param_spec_object ("config",
                         "Configuration",
                         "The configuration to be displayed",
                         IDE_TYPE_CONFIGURATION,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_config_view_init (IdeConfigView *self)
{
}

static void
ide_config_view_load_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  IdeConfigViewAddin *addin = (IdeConfigViewAddin *)object;
  g_autoptr(IdeConfigView) self = user_data;
  g_autoptr(GError) error = NULL;
  IdeContext *context;

  g_assert (IDE_IS_CONFIG_VIEW_ADDIN (addin));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_CONFIG_VIEW (self));

  context = ide_widget_get_context (GTK_WIDGET (self));

  if (!ide_config_view_addin_load_finish (addin, result, &error))
    ide_context_warning (context, "%s: %s", G_OBJECT_TYPE_NAME (addin), error->message);
}

static void
ide_config_view_addin_added_cb (PeasExtensionSet *set,
                                PeasPluginInfo   *plugin_info,
                                PeasExtension    *exten,
                                gpointer          user_data)
{
  IdeConfigViewAddin *addin = (IdeConfigViewAddin *)exten;
  IdeConfigView *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_CONFIG_VIEW_ADDIN (addin));
  g_assert (IDE_IS_CONFIG_VIEW (self));
  g_assert (DZL_IS_PREFERENCES (self->preferences));

  ide_config_view_addin_load_async (addin,
                                    self->preferences,
                                    self->config,
                                    self->cancellable,
                                    ide_config_view_load_cb,
                                    g_object_ref (self));
}

static void
ide_config_view_addin_removed_cb (PeasExtensionSet *set,
                                  PeasPluginInfo   *plugin_info,
                                  PeasExtension    *exten,
                                  gpointer          user_data)
{
  IdeConfigViewAddin *addin = (IdeConfigViewAddin *)exten;
  IdeConfigView *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_CONFIG_VIEW_ADDIN (addin));
  g_assert (IDE_IS_CONFIG_VIEW (self));
  g_assert (DZL_IS_PREFERENCES (self->preferences));

  ide_config_view_addin_unload (addin, self->preferences, self->config);
}

static void
ide_config_view_disconnect (IdeConfigView *self)
{
  g_assert (IDE_IS_CONFIG_VIEW (self));
  g_assert (IDE_IS_CONFIGURATION (self->config));

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  g_clear_object (&self->addins);
  g_clear_object (&self->config);

  if (self->preferences != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->preferences));
}

static void
ide_config_view_connect (IdeConfigView    *self,
                         IdeConfiguration *config)
{
  IdeContext *context;

  g_assert (IDE_IS_CONFIG_VIEW (self));
  g_assert (IDE_IS_CONFIGURATION (config));
  g_assert (self->cancellable == NULL);
  g_assert (self->config == NULL);

  context = ide_object_get_context (IDE_OBJECT (config));

  self->preferences = g_object_new (DZL_TYPE_PREFERENCES_VIEW,
                                    "use-sidebar", FALSE,
                                    "visible", TRUE,
                                    NULL);
  g_signal_connect (self->preferences,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->preferences);
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->preferences));

  self->cancellable = g_cancellable_new ();
  self->config = g_object_ref (config);
  self->addins = peas_extension_set_new (peas_engine_get_default (),
                                         IDE_TYPE_CONFIG_VIEW_ADDIN,
                                         "context", context,
                                         NULL);
  g_signal_connect (self->addins,
                    "extension-added",
                    G_CALLBACK (ide_config_view_addin_added_cb),
                    self);
  g_signal_connect (self->addins,
                    "extension-removed",
                    G_CALLBACK (ide_config_view_addin_removed_cb),
                    self);
  peas_extension_set_foreach (self->addins,
                              ide_config_view_addin_added_cb,
                              self);
}

/**
 * ide_config_view_get_config:
 *
 * Gets the #IdeConfigView:config property.
 *
 * Returns: (nullable) (transfer none): an #IdeConfiguration.
 *
 * Since: 3.32
 */
IdeConfiguration *
ide_config_view_get_config (IdeConfigView *self)
{
  g_return_val_if_fail (IDE_IS_CONFIG_VIEW (self), NULL);

  return self->config;
}

void
ide_config_view_set_config (IdeConfigView    *self,
                            IdeConfiguration *config)
{
  g_return_if_fail (IDE_IS_CONFIG_VIEW (self));
  g_return_if_fail (!config || IDE_IS_CONFIGURATION (config));

  if (self->config != config)
    {
      if (self->config)
        ide_config_view_disconnect (self);

      if (config)
        ide_config_view_connect (self, config);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CONFIG]);
    }
}
