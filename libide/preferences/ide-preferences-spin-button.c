/* ide-preferences-spin-button.c
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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

#include "ide-preferences-spin-button.h"

struct _IdePreferencesSpinButton
{
  GtkBin         parent_instance;

  GtkSpinButton *spin_button;
};

G_DEFINE_TYPE (IdePreferencesSpinButton, ide_preferences_spin_button, GTK_TYPE_BIN)

enum {
  PROP_0,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static void
ide_preferences_spin_button_finalize (GObject *object)
{
  IdePreferencesSpinButton *self = (IdePreferencesSpinButton *)object;

  G_OBJECT_CLASS (ide_preferences_spin_button_parent_class)->finalize (object);
}

static void
ide_preferences_spin_button_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  IdePreferencesSpinButton *self = IDE_PREFERENCES_SPIN_BUTTON (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_preferences_spin_button_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  IdePreferencesSpinButton *self = IDE_PREFERENCES_SPIN_BUTTON (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_preferences_spin_button_class_init (IdePreferencesSpinButtonClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ide_preferences_spin_button_finalize;
  object_class->get_property = ide_preferences_spin_button_get_property;
  object_class->set_property = ide_preferences_spin_button_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-preferences-spin-button.ui");
  gtk_widget_class_bind_template_child (widget_class, IdePreferencesSpinButton, spin_button);
}

static void
ide_preferences_spin_button_init (IdePreferencesSpinButton *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
