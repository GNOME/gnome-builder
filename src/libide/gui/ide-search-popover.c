/* ide-search-popover.c
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

#define G_LOG_DOMAIN "ide-search-popover"

#include "config.h"

#include "ide-search-popover-private.h"
#include "ide-search-resources.h"

struct _IdeSearchPopover
{
  GtkPopover       parent_instance;

  IdeSearchEngine *search_engine;

  GtkBox          *nav_box;
  GtkSearchEntry  *entry;
};

enum {
  PROP_0,
  PROP_SEARCH_ENGINE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeSearchPopover, ide_search_popover, GTK_TYPE_POPOVER)

static GParamSpec *properties [N_PROPS];

static void
ide_search_popover_hide_action (GtkWidget  *widget,
                                const char *action_Name,
                                GVariant   *param)
{
  gtk_popover_popdown (GTK_POPOVER (widget));
}

static void
ide_search_popover_set_search_engine (IdeSearchPopover *self,
                                      IdeSearchEngine  *search_engine)
{
  g_assert (IDE_IS_SEARCH_POPOVER (self));
  g_assert (IDE_IS_SEARCH_ENGINE (search_engine));

  if (g_set_object (&self->search_engine, search_engine))
    {
      /* TODO: Setup addins */
    }
}

static void
ide_search_popover_show (GtkWidget *widget)
{
  IdeSearchPopover *self = (IdeSearchPopover *)widget;

  g_assert (IDE_IS_SEARCH_POPOVER (self));

  GTK_WIDGET_CLASS (ide_search_popover_parent_class)->show (widget);

  gtk_widget_grab_focus (GTK_WIDGET (self->entry));
}

static gboolean
ide_search_popover_grab_focus (GtkWidget *widget)
{
  IdeSearchPopover *self = (IdeSearchPopover *)widget;

  g_assert (IDE_IS_SEARCH_POPOVER (self));

  return gtk_widget_grab_focus (GTK_WIDGET (self->entry));
}

static void
ide_search_popover_dispose (GObject *object)
{
  IdeSearchPopover *self = (IdeSearchPopover *)object;

  g_clear_object (&self->search_engine);

  G_OBJECT_CLASS (ide_search_popover_parent_class)->dispose (object);
}

static void
ide_search_popover_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  IdeSearchPopover *self = IDE_SEARCH_POPOVER (object);

  switch (prop_id)
    {
    case PROP_SEARCH_ENGINE:
      g_value_set_object (value, self->search_engine);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_search_popover_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  IdeSearchPopover *self = IDE_SEARCH_POPOVER (object);

  switch (prop_id)
    {
    case PROP_SEARCH_ENGINE:
      ide_search_popover_set_search_engine (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_search_popover_class_init (IdeSearchPopoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ide_search_popover_dispose;
  object_class->get_property = ide_search_popover_get_property;
  object_class->set_property = ide_search_popover_set_property;

  widget_class->grab_focus = ide_search_popover_grab_focus;
  widget_class->show = ide_search_popover_show;

  properties [PROP_SEARCH_ENGINE] =
    g_param_spec_object ("search-engine",
                         "Search Engine",
                         "The search engine for the popover",
                         IDE_TYPE_SEARCH_ENGINE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  g_resources_register (ide_search_get_resource ());

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-gui/ui/ide-search-popover.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeSearchPopover, entry);
  gtk_widget_class_bind_template_child (widget_class, IdeSearchPopover, nav_box);

  gtk_widget_class_install_action (widget_class, "search.hide", NULL, ide_search_popover_hide_action);
}

static void
ide_search_popover_init (IdeSearchPopover *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
ide_search_popover_new (IdeSearchEngine *search_engine)
{
  g_return_val_if_fail (IDE_IS_SEARCH_ENGINE (search_engine), NULL);

  return g_object_new (IDE_TYPE_SEARCH_POPOVER,
                       "search-engine", search_engine,
                       NULL);
}

void
ide_search_popover_present (IdeSearchPopover *self,
                            int               parent_width,
                            int               parent_height)
{
  GdkRectangle point;
  int min_width;
  int nat_width;

  g_return_if_fail (IDE_IS_SEARCH_POPOVER (self));

  gtk_widget_measure (GTK_WIDGET (self->nav_box),
                      GTK_ORIENTATION_HORIZONTAL,
                      -1,
                      &min_width, &nat_width, NULL, NULL);

  point = (GdkRectangle) { (parent_width - min_width) / 2, 100, 1, 1 };
  gtk_popover_set_pointing_to (GTK_POPOVER (self), &point);
  gtk_popover_present (GTK_POPOVER (self));
}
