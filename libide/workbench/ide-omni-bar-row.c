/* ide-omni-bar-row.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-omni-bar-row"

#include <glib/gi18n.h>

#include "devices/ide-device.h"
#include "runtimes/ide-runtime.h"
#include "workbench/ide-omni-bar-row.h"

struct _IdeOmniBarRow
{
  GtkListBoxRow     parent_instance;

  IdeConfiguration *item;

  GtkLabel         *title;
  GtkLabel         *device_title;
  GtkLabel         *runtime_title;
  GtkGrid          *grid;
  GtkImage         *checked;
  GtkButton        *button;
};

enum {
  PROP_0,
  PROP_ITEM,
  N_PROPS
};

G_DEFINE_TYPE (IdeOmniBarRow, ide_omni_bar_row, GTK_TYPE_LIST_BOX_ROW)

static GParamSpec *properties [N_PROPS];

/**
 * ide_omni_bar_row_get_item:
 *
 * Returns: (transfer none): An #IdeConfiguration.
 */
IdeConfiguration *
ide_omni_bar_row_get_item (IdeOmniBarRow *self)
{
  g_return_val_if_fail (IDE_IS_OMNI_BAR_ROW (self), NULL);

  return self->item;
}

static void
on_runtime_changed (IdeOmniBarRow    *self,
                    GParamSpec       *pspec,
                    IdeConfiguration *config)
{
  g_autofree gchar *freeme = NULL;
  const gchar *display_name = NULL;
  IdeRuntime *runtime;

  g_assert (IDE_IS_OMNI_BAR_ROW (self));
  g_assert (IDE_IS_CONFIGURATION (config));

  if (NULL != (runtime = ide_configuration_get_runtime (config)))
    display_name = ide_runtime_get_display_name (runtime);
  else
    display_name = freeme = g_strdup_printf ("%s (%s)",
                                             ide_configuration_get_runtime_id (config),
                                             /* Translators, missing means we could not locate the runtime */
                                             _("missing"));

  gtk_label_set_label (self->runtime_title, display_name);
}

static void
on_device_changed (IdeOmniBarRow    *self,
                   GParamSpec       *pspec,
                   IdeConfiguration *config)
{
  const gchar *display_name = NULL;
  IdeDevice *device;

  g_assert (IDE_IS_OMNI_BAR_ROW (self));
  g_assert (IDE_IS_CONFIGURATION (config));

  if (NULL != (device = ide_configuration_get_device (config)))
    display_name = ide_device_get_display_name (device);

  gtk_label_set_label (self->device_title, display_name);
}

static void
ide_omni_bar_row_set_item (IdeOmniBarRow    *self,
                           IdeConfiguration *item)
{
  g_return_if_fail (IDE_IS_OMNI_BAR_ROW (self));
  g_return_if_fail (IDE_IS_CONFIGURATION (item));

  if (g_set_object (&self->item, item))
    {
      g_object_bind_property (self->item, "display-name",
                              self->title, "label",
                              G_BINDING_SYNC_CREATE);

      g_signal_connect_object (self->item,
                               "notify::runtime",
                               G_CALLBACK (on_runtime_changed),
                               self,
                               G_CONNECT_SWAPPED);

      g_signal_connect_object (self->item,
                               "notify::device",
                               G_CALLBACK (on_device_changed),
                               self,
                               G_CONNECT_SWAPPED);

      on_runtime_changed (self, NULL, item);
      on_device_changed (self, NULL, item);
    }
}

static void
ide_omni_bar_row_finalize (GObject *object)
{
  IdeOmniBarRow *self = (IdeOmniBarRow *)object;

  g_clear_object (&self->item);

  G_OBJECT_CLASS (ide_omni_bar_row_parent_class)->finalize (object);
}

static void
ide_omni_bar_row_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  IdeOmniBarRow *self = IDE_OMNI_BAR_ROW (object);

  switch (prop_id)
    {
    case PROP_ITEM:
      g_value_set_object (value, self->item);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_omni_bar_row_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  IdeOmniBarRow *self = IDE_OMNI_BAR_ROW (object);

  switch (prop_id)
    {
    case PROP_ITEM:
      ide_omni_bar_row_set_item (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_omni_bar_row_class_init (IdeOmniBarRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ide_omni_bar_row_finalize;
  object_class->get_property = ide_omni_bar_row_get_property;
  object_class->set_property = ide_omni_bar_row_set_property;

  properties [PROP_ITEM] =
    g_param_spec_object ("item",
                         "Item",
                         "The configuration item to view",
                         IDE_TYPE_CONFIGURATION,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-omni-bar-row.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBarRow, button);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBarRow, checked);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBarRow, grid);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBarRow, title);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBarRow, device_title);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBarRow, runtime_title);
}

static void
ide_omni_bar_row_init (IdeOmniBarRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
ide_omni_bar_row_new (IdeConfiguration *item)
{
  g_return_val_if_fail (IDE_IS_CONFIGURATION (item), NULL);

  return g_object_new (IDE_TYPE_OMNI_BAR_ROW,
                       "item", item,
                       NULL);
}

/**
 * ide_omni_bar_row_set_active:
 *
 * Sets this row as the currently active build configuration.
 * Doing so will expand extra information on the row.
 */
void
ide_omni_bar_row_set_active (IdeOmniBarRow *self,
                             gboolean       active)
{
  g_return_if_fail (IDE_IS_OMNI_BAR_ROW (self));

  active = !!active;

  gtk_widget_set_visible (GTK_WIDGET (self->grid), active);
  gtk_widget_set_visible (GTK_WIDGET (self->button), active);
  gtk_widget_set_visible (GTK_WIDGET (self->checked), active);
}
