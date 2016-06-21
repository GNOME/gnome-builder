/* ide-omni-bar.c
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

#include "ide-omni-bar.h"

struct _IdeOmniBar
{
  GtkBox parent_instance;
};

G_DEFINE_TYPE (IdeOmniBar, ide_omni_bar, GTK_TYPE_BOX)

enum {
  PROP_0,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
ide_omni_bar_finalize (GObject *object)
{
  IdeOmniBar *self = (IdeOmniBar *)object;

  G_OBJECT_CLASS (ide_omni_bar_parent_class)->finalize (object);
}

static void
ide_omni_bar_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  IdeOmniBar *self = IDE_OMNI_BAR (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_omni_bar_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  IdeOmniBar *self = IDE_OMNI_BAR (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_omni_bar_class_init (IdeOmniBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ide_omni_bar_finalize;
  object_class->get_property = ide_omni_bar_get_property;
  object_class->set_property = ide_omni_bar_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-omni-bar.ui");
  gtk_widget_class_set_css_name (widget_class, "omnibar");
}

static void
ide_omni_bar_init (IdeOmniBar *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
ide_omni_bar_new (void)
{
  return g_object_new (IDE_TYPE_OMNI_BAR, NULL);
}
