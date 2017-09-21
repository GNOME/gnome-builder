/* gb-color-picker-editor-view-addin.c
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gb-color-picker-editor-view-addin"

#include "gb-color-picker-document-monitor.h"
#include "gb-color-picker-editor-view-addin.h"

struct _GbColorPickerEditorViewAddin
{
  GObject parent_instance;

  /* Unowned reference to the view */
  IdeEditorView *view;

  /* Our document monitor, or NULL */
  GbColorPickerDocumentMonitor *monitor;

  /* If we've been enabled by the user */
  guint enabled : 1;

  /* Re-entrancy check for color-found */
  guint in_color_found : 1;
};

enum {
  COLOR_FOUND,
  N_SIGNALS
};

enum {
  PROP_0,
  PROP_ENABLED,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static void
monitor_color_found (GbColorPickerEditorViewAddin *self,
                     GstyleColor                  *color,
                     GbColorPickerDocumentMonitor *monitor)
{
  g_assert (GB_IS_COLOR_PICKER_EDITOR_VIEW_ADDIN (self));
  g_assert (GSTYLE_IS_COLOR (color));
  g_assert (GB_IS_COLOR_PICKER_DOCUMENT_MONITOR (monitor));

  self->in_color_found = TRUE;
  g_signal_emit (self, signals [COLOR_FOUND], 0, color);
  self->in_color_found = FALSE;
}

void
gb_color_picker_editor_view_addin_set_enabled (GbColorPickerEditorViewAddin *self,
                                               gboolean                      enabled)
{
  g_return_if_fail (GB_IS_COLOR_PICKER_EDITOR_VIEW_ADDIN (self));

  enabled = !!enabled;

  if (enabled != self->enabled)
    {
      if (self->enabled)
        {
          self->enabled = FALSE;
          gb_color_picker_document_monitor_queue_uncolorize (self->monitor, NULL, NULL);
          gb_color_picker_document_monitor_set_buffer (self->monitor, NULL);
          g_clear_object (&self->monitor);
        }

      if (enabled)
        {
          IdeBuffer *buffer = ide_editor_view_get_buffer (self->view);

          self->enabled = TRUE;
          self->monitor = gb_color_picker_document_monitor_new (buffer);
          g_signal_connect_object (self->monitor,
                                   "color-found",
                                   G_CALLBACK (monitor_color_found),
                                   self,
                                   G_CONNECT_SWAPPED);
          gb_color_picker_document_monitor_queue_colorize (self->monitor, NULL, NULL);
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ENABLED]);
    }
}

gboolean
gb_color_picker_editor_view_addin_get_enabled (GbColorPickerEditorViewAddin *self)
{
  g_return_val_if_fail (GB_IS_COLOR_PICKER_EDITOR_VIEW_ADDIN (self), FALSE);

  return self->enabled;
}

static void
gb_color_picker_editor_view_addin_load (IdeEditorViewAddin *addin,
                                        IdeEditorView      *view)
{
  GbColorPickerEditorViewAddin *self = (GbColorPickerEditorViewAddin *)addin;
  g_autoptr(DzlPropertiesGroup) group = NULL;

  g_assert (GB_IS_COLOR_PICKER_EDITOR_VIEW_ADDIN (self));
  g_assert (IDE_IS_EDITOR_VIEW (view));

  self->view = view;

  group = dzl_properties_group_new (G_OBJECT (self));
  dzl_properties_group_add_all_properties (group);
  gtk_widget_insert_action_group (GTK_WIDGET (view), "color-picker", G_ACTION_GROUP (group));
}

static void
gb_color_picker_editor_view_addin_unload (IdeEditorViewAddin *addin,
                                          IdeEditorView      *view)
{
  GbColorPickerEditorViewAddin *self = (GbColorPickerEditorViewAddin *)addin;

  g_assert (GB_IS_COLOR_PICKER_EDITOR_VIEW_ADDIN (self));
  g_assert (IDE_IS_EDITOR_VIEW (view));

  if (self->monitor != NULL)
    {
      gb_color_picker_document_monitor_set_buffer (self->monitor, NULL);
      g_clear_object (&self->monitor);
    }

  gtk_widget_insert_action_group (GTK_WIDGET (view), "color-picker", NULL);

  self->view = NULL;
}

static void
editor_view_addin_iface_init (IdeEditorViewAddinInterface *iface)
{
  iface->load = gb_color_picker_editor_view_addin_load;
  iface->unload = gb_color_picker_editor_view_addin_unload;
}

G_DEFINE_TYPE_WITH_CODE (GbColorPickerEditorViewAddin, gb_color_picker_editor_view_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_EDITOR_VIEW_ADDIN, editor_view_addin_iface_init))

static void
gb_color_picker_editor_view_addin_get_property (GObject    *object,
                                                guint       prop_id,
                                                GValue     *value,
                                                GParamSpec *pspec)
{
  GbColorPickerEditorViewAddin *self = GB_COLOR_PICKER_EDITOR_VIEW_ADDIN (object);

  switch (prop_id)
    {
    case PROP_ENABLED:
      g_value_set_boolean (value, gb_color_picker_editor_view_addin_get_enabled (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_color_picker_editor_view_addin_set_property (GObject      *object,
                                                guint         prop_id,
                                                const GValue *value,
                                                GParamSpec   *pspec)
{
  GbColorPickerEditorViewAddin *self = GB_COLOR_PICKER_EDITOR_VIEW_ADDIN (object);

  switch (prop_id)
    {
    case PROP_ENABLED:
      gb_color_picker_editor_view_addin_set_enabled (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_color_picker_editor_view_addin_class_init (GbColorPickerEditorViewAddinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gb_color_picker_editor_view_addin_get_property;
  object_class->set_property = gb_color_picker_editor_view_addin_set_property;

  properties [PROP_ENABLED] =
    g_param_spec_boolean ("enabled", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [COLOR_FOUND] =
    g_signal_new ("color-found",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1, GSTYLE_TYPE_COLOR);
}

static void
gb_color_picker_editor_view_addin_init (GbColorPickerEditorViewAddin *self)
{
}

void
gb_color_picker_editor_view_addin_set_color (GbColorPickerEditorViewAddin *self,
                                             GstyleColor                  *color)
{
  g_return_if_fail (GB_IS_COLOR_PICKER_EDITOR_VIEW_ADDIN (self));
  g_return_if_fail (GSTYLE_IS_COLOR (color));

  if (self->monitor != NULL && !self->in_color_found)
    gb_color_picker_document_monitor_set_color_tag_at_cursor (self->monitor, color);
}
