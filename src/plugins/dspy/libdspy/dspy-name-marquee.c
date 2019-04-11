/* dspy-name-marquee.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#define G_LOG_DOMAIN "dspy-name-marquee"

#include "config.h"

#include <dazzle.h>

#include "dspy-name-marquee.h"

struct _DspyNameMarquee
{
  GtkBin           parent_instance;

  DspyName        *name;
  DzlBindingGroup *name_bindings;

  GtkLabel        *label_bus;
  GtkLabel        *label_name;
  GtkLabel        *label_owner;
  GtkLabel        *label_pid;
};

G_DEFINE_TYPE (DspyNameMarquee, dspy_name_marquee, GTK_TYPE_BIN)

enum {
  PROP_0,
  PROP_NAME,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

/**
 * dspy_name_marquee_new:
 *
 * Create a new #DspyNameMarquee.
 *
 * Returns: (transfer full): a newly created #DspyNameMarquee
 */
GtkWidget *
dspy_name_marquee_new (void)
{
  return g_object_new (DSPY_TYPE_NAME_MARQUEE, NULL);
}

static void
dspy_name_marquee_finalize (GObject *object)
{
  DspyNameMarquee *self = (DspyNameMarquee *)object;

  dzl_binding_group_set_source (self->name_bindings, NULL);
  g_clear_object (&self->name_bindings);
  g_clear_object (&self->name);

  G_OBJECT_CLASS (dspy_name_marquee_parent_class)->finalize (object);
}

static void
dspy_name_marquee_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  DspyNameMarquee *self = DSPY_NAME_MARQUEE (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_object (value, dspy_name_marquee_get_name (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
dspy_name_marquee_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  DspyNameMarquee *self = DSPY_NAME_MARQUEE (object);

  switch (prop_id)
    {
    case PROP_NAME:
      dspy_name_marquee_set_name (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
dspy_name_marquee_class_init (DspyNameMarqueeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = dspy_name_marquee_finalize;
  object_class->get_property = dspy_name_marquee_get_property;
  object_class->set_property = dspy_name_marquee_set_property;

  properties [PROP_NAME] =
    g_param_spec_object ("name",
                         "Name",
                         "The DspyName to display on the marquee",
                         DSPY_TYPE_NAME,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/dspy/dspy-name-marquee.ui");
  gtk_widget_class_bind_template_child (widget_class, DspyNameMarquee, label_bus);
  gtk_widget_class_bind_template_child (widget_class, DspyNameMarquee, label_name);
  gtk_widget_class_bind_template_child (widget_class, DspyNameMarquee, label_owner);
  gtk_widget_class_bind_template_child (widget_class, DspyNameMarquee, label_pid);
}

static void
dspy_name_marquee_init (DspyNameMarquee *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->name_bindings = dzl_binding_group_new ();

  dzl_binding_group_bind (self->name_bindings, "pid", self->label_pid, "label", 0);
  dzl_binding_group_bind (self->name_bindings, "name", self->label_name, "label", 0);
  dzl_binding_group_bind (self->name_bindings, "owner", self->label_owner, "label", 0);
}

/**
 * dspy_name_marquee_get_name:
 *
 * Gets the name on the marquee
 *
 * Returns: (nullable) (transfer none): a #DspyName or %NULL
 */
DspyName *
dspy_name_marquee_get_name (DspyNameMarquee *self)
{
  g_return_val_if_fail (DSPY_IS_NAME_MARQUEE (self), NULL);

  return self->name;
}

void
dspy_name_marquee_set_name (DspyNameMarquee *self,
                            DspyName        *name)
{
  g_return_if_fail (DSPY_IS_NAME_MARQUEE (self));
  g_return_if_fail (!name || DSPY_IS_NAME (name));

  if (g_set_object (&self->name, name))
    {
      const gchar *address = NULL;

      if (name != NULL)
        address = dspy_connection_get_address (dspy_name_get_connection (name));

      dzl_binding_group_set_source (self->name_bindings, name);
      gtk_label_set_label (self->label_bus, address);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_NAME]);
    }
}
