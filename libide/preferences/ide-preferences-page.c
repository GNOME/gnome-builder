/* ide-preferences-page.c
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

#include <glib/gi18n.h>

#include "ide-preferences-group.h"
#include "ide-preferences-group-private.h"
#include "ide-preferences-page.h"
#include "ide-preferences-page-private.h"

struct _IdePreferencesPage
{
  GtkBin      parent_instance;

  gint        priority;

  GtkBox     *box;
  GHashTable *groups_by_name;
};

enum {
  PROP_0,
  PROP_PRIORITY,
  LAST_PROP
};

G_DEFINE_TYPE (IdePreferencesPage, ide_preferences_page, GTK_TYPE_BIN)

static GParamSpec *properties [LAST_PROP];

static void
ide_preferences_page_finalize (GObject *object)
{
  IdePreferencesPage *self = (IdePreferencesPage *)object;

  g_clear_pointer (&self->groups_by_name, g_hash_table_unref);

  G_OBJECT_CLASS (ide_preferences_page_parent_class)->finalize (object);
}

static void
ide_preferences_page_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  IdePreferencesPage *self = IDE_PREFERENCES_PAGE (object);

  switch (prop_id)
    {
    case PROP_PRIORITY:
      g_value_set_int (value, self->priority);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_preferences_page_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  IdePreferencesPage *self = IDE_PREFERENCES_PAGE (object);

  switch (prop_id)
    {
    case PROP_PRIORITY:
      self->priority = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_preferences_page_class_init (IdePreferencesPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ide_preferences_page_finalize;
  object_class->get_property = ide_preferences_page_get_property;
  object_class->set_property = ide_preferences_page_set_property;

  properties [PROP_PRIORITY] =
    g_param_spec_int ("priority",
                      "Priority",
                      "Priority",
                      G_MININT,
                      G_MAXINT,
                      0,
                      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-preferences-page.ui");
  gtk_widget_class_bind_template_child (widget_class, IdePreferencesPage, box);
}

static void
ide_preferences_page_init (IdePreferencesPage *self)
{
  self->groups_by_name = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  gtk_widget_init_template (GTK_WIDGET (self));
}

void
ide_preferences_page_add_group (IdePreferencesPage  *self,
                                IdePreferencesGroup *group)
{
  gchar *name = NULL;
  gint position = -1;

  g_return_if_fail (IDE_IS_PREFERENCES_PAGE (self));
  g_return_if_fail (IDE_IS_PREFERENCES_GROUP (group));

  g_object_get (group, "name", &name, NULL);

  if (g_hash_table_contains (self->groups_by_name, name))
    {
      g_free (name);
      return;
    }

  g_hash_table_insert (self->groups_by_name, name, group);

  gtk_container_add_with_properties (GTK_CONTAINER (self->box), GTK_WIDGET (group),
                                     "position", position,
                                     NULL);
}

IdePreferencesGroup *
ide_preferences_page_get_group (IdePreferencesPage *self,
                                const gchar        *name)
{
  g_return_val_if_fail (IDE_IS_PREFERENCES_PAGE (self), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  return g_hash_table_lookup (self->groups_by_name, name);
}

void
_ide_preferences_page_set_map (IdePreferencesPage *self,
                               GHashTable         *map)
{
  IdePreferencesGroup *group;
  GHashTableIter iter;

  g_return_if_fail (IDE_IS_PREFERENCES_PAGE (self));

  g_hash_table_iter_init (&iter, self->groups_by_name);

  while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&group))
    _ide_preferences_group_set_map (group, map);
}
