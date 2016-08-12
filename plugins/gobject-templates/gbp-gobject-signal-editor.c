/* gbp-gobject-signal-editor.c
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

#define G_LOG_DOMAIN "gbp-gobject-signal-editor"

#include <glib/gi18n.h>

#include "gbp-gobject-signal-editor.h"

struct _GbpGobjectSignalEditor
{
  GtkBin parent_instance;

  GbpGobjectSignal *signal;
};

enum {
  PROP_0,
  PROP_SIGNAL,
  N_PROPS
};

G_DEFINE_TYPE (GbpGobjectSignalEditor, gbp_gobject_signal_editor, GTK_TYPE_BIN)

static GParamSpec *properties [N_PROPS];

GbpGobjectSignal *
gbp_gobject_signal_editor_get_signal (GbpGobjectSignalEditor *self)
{
  g_return_val_if_fail (GBP_IS_GOBJECT_SIGNAL_EDITOR (self), NULL);

  return self->signal;
}

void
gbp_gobject_signal_editor_set_signal (GbpGobjectSignalEditor *self,
                                      GbpGobjectSignal       *signal)
{
  g_return_if_fail (GBP_IS_GOBJECT_SIGNAL_EDITOR (self));
  g_return_if_fail (!signal || GBP_IS_GOBJECT_SIGNAL (signal));

  if (g_set_object (&self->signal, signal))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SIGNAL]);
    }
}

static void
gbp_gobject_signal_editor_finalize (GObject *object)
{
  GbpGobjectSignalEditor *self = (GbpGobjectSignalEditor *)object;

  g_clear_object (&self->signal);

  G_OBJECT_CLASS (gbp_gobject_signal_editor_parent_class)->finalize (object);
}

static void
gbp_gobject_signal_editor_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  GbpGobjectSignalEditor *self = GBP_GOBJECT_SIGNAL_EDITOR (object);

  switch (prop_id)
    {
    case PROP_SIGNAL:
      g_value_set_object (value, self->signal);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_gobject_signal_editor_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  GbpGobjectSignalEditor *self = GBP_GOBJECT_SIGNAL_EDITOR (object);

  switch (prop_id)
    {
    case PROP_SIGNAL:
      gbp_gobject_signal_editor_set_signal (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_gobject_signal_editor_class_init (GbpGobjectSignalEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gbp_gobject_signal_editor_finalize;
  object_class->get_property = gbp_gobject_signal_editor_get_property;
  object_class->set_property = gbp_gobject_signal_editor_set_property;

  properties [PROP_SIGNAL] =
    g_param_spec_object ("signal",
                         "Signal",
                         "The signal to be edited",
                         GBP_TYPE_GOBJECT_SIGNAL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/gobject-templates/gbp-gobject-signal-editor.ui");
}

static void
gbp_gobject_signal_editor_init (GbpGobjectSignalEditor *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
