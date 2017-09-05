/* ide-omni-pausable-row.c
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

#define G_LOG_DOMAIN "ide-omni-pausable-row"

#include "ide-pausable.h"

#include "workbench/ide-omni-pausable-row.h"

struct _IdeOmniPausableRow
{
  DzlListBoxRow    parent_instance;

  DzlBindingGroup *group;
  IdePausable     *pausable;

  GtkToggleButton *button;
  GtkLabel        *title;
  GtkLabel        *subtitle;
};

enum {
  PROP_0,
  PROP_PAUSABLE,
  N_PROPS
};

G_DEFINE_TYPE (IdeOmniPausableRow, ide_omni_pausable_row, DZL_TYPE_LIST_BOX_ROW)

static GParamSpec *properties [N_PROPS];

static void
ide_omni_pausable_row_dispose (GObject *object)
{
  IdeOmniPausableRow *self = (IdeOmniPausableRow *)object;

  g_clear_object (&self->group);
  g_clear_object (&self->pausable);

  G_OBJECT_CLASS (ide_omni_pausable_row_parent_class)->dispose (object);
}

static void
ide_omni_pausable_row_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  IdeOmniPausableRow *self = IDE_OMNI_PAUSABLE_ROW (object);

  switch (prop_id)
    {
    case PROP_PAUSABLE:
      g_value_set_object (value, ide_omni_pausable_row_get_pausable (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_omni_pausable_row_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  IdeOmniPausableRow *self = IDE_OMNI_PAUSABLE_ROW (object);

  switch (prop_id)
    {
    case PROP_PAUSABLE:
      ide_omni_pausable_row_set_pausable (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_omni_pausable_row_class_init (IdeOmniPausableRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ide_omni_pausable_row_dispose;
  object_class->get_property = ide_omni_pausable_row_get_property;
  object_class->set_property = ide_omni_pausable_row_set_property;

  properties [PROP_PAUSABLE] =
    g_param_spec_object ("pausable", NULL, NULL,
                         IDE_TYPE_PAUSABLE,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-omni-pausable-row.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeOmniPausableRow, button);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniPausableRow, title);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniPausableRow, subtitle);
}

static void
ide_omni_pausable_row_init (IdeOmniPausableRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->group = dzl_binding_group_new ();

  dzl_binding_group_bind (self->group, "title", self->title, "label", 0);
  dzl_binding_group_bind (self->group, "subtitle", self->subtitle, "label", 0);
  dzl_binding_group_bind (self->group, "paused", self->button, "active",
                          G_BINDING_BIDIRECTIONAL);
}

GtkWidget *
ide_omni_pausable_row_new (IdePausable *pausable)
{
  g_return_val_if_fail (!pausable || IDE_IS_PAUSABLE (pausable), NULL);

  return g_object_new (IDE_TYPE_OMNI_PAUSABLE_ROW,
                       "pausable", pausable,
                       NULL);
}

/**
 * ide_omni_pausable_row_get_pausable:
 * @self: a #IdeOmniPausableRow
 *
 * Returns: (transfer none): An #IdePausable or %NULL
 */
IdePausable *
ide_omni_pausable_row_get_pausable (IdeOmniPausableRow *self)
{
  g_return_val_if_fail (IDE_IS_OMNI_PAUSABLE_ROW (self), NULL);

  return self->pausable;
}

void
ide_omni_pausable_row_set_pausable (IdeOmniPausableRow *self,
                                    IdePausable        *pausable)
{
  g_return_if_fail (IDE_IS_OMNI_PAUSABLE_ROW (self));
  g_return_if_fail (!pausable || IDE_IS_PAUSABLE (pausable));

  if (g_set_object (&self->pausable, pausable))
    {
      dzl_binding_group_set_source (self->group, pausable);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PAUSABLE]);
    }
}
