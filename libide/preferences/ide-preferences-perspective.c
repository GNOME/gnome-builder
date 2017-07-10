/* ide-preferences-perspective.c
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

#define G_LOG_DOMAIN "ide-preferences-perspective"

#include <glib/gi18n.h>
#include <libpeas/peas.h>

#include "preferences/ide-preferences-addin.h"
#include "preferences/ide-preferences-builtin.h"
#include "preferences/ide-preferences-perspective.h"
#include "workbench/ide-perspective.h"

struct _IdePreferencesPerspective
{
  DzlPreferencesView  parent_instance;
  PeasExtensionSet   *extensions;
};

static void ide_perspective_iface_init (IdePerspectiveInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdePreferencesPerspective, ide_preferences_perspective, DZL_TYPE_PREFERENCES_VIEW, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_PERSPECTIVE, ide_perspective_iface_init))

static void
ide_preferences_perspective_extension_added (PeasExtensionSet *set,
                                             PeasPluginInfo   *plugin_info,
                                             PeasExtension    *extension,
                                             gpointer          user_data)
{
  IdePreferencesPerspective *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_PREFERENCES_ADDIN (extension));
  g_assert (IDE_IS_PREFERENCES_PERSPECTIVE (self));

  ide_preferences_addin_load (IDE_PREFERENCES_ADDIN (extension), DZL_PREFERENCES (self));
  dzl_preferences_view_reapply_filter (DZL_PREFERENCES_VIEW (self));
}

static void
ide_preferences_perspective_extension_removed (PeasExtensionSet *set,
                                               PeasPluginInfo   *plugin_info,
                                               PeasExtension    *extension,
                                               gpointer          user_data)
{
  IdePreferencesPerspective *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_PREFERENCES_ADDIN (extension));
  g_assert (IDE_IS_PREFERENCES_PERSPECTIVE (self));

  ide_preferences_addin_unload (IDE_PREFERENCES_ADDIN (extension), DZL_PREFERENCES (self));
  dzl_preferences_view_reapply_filter (DZL_PREFERENCES_VIEW (self));
}

static void
ide_preferences_perspective_constructed (GObject *object)
{
  IdePreferencesPerspective *self = (IdePreferencesPerspective *)object;

  G_OBJECT_CLASS (ide_preferences_perspective_parent_class)->constructed (object);

  _ide_preferences_builtin_register (DZL_PREFERENCES (self));

  self->extensions = peas_extension_set_new (peas_engine_get_default (),
                                             IDE_TYPE_PREFERENCES_ADDIN,
                                             NULL);

  g_signal_connect (self->extensions,
                    "extension-added",
                    G_CALLBACK (ide_preferences_perspective_extension_added),
                    self);

  g_signal_connect (self->extensions,
                    "extension-removed",
                    G_CALLBACK (ide_preferences_perspective_extension_removed),
                    self);

  peas_extension_set_foreach (self->extensions,
                              ide_preferences_perspective_extension_added,
                              self);
}

static void
ide_preferences_perspective_class_init (IdePreferencesPerspectiveClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_preferences_perspective_constructed;
}

static void
ide_preferences_perspective_init (IdePreferencesPerspective *self)
{
}

static gchar *
ide_preferences_perspective_get_id (IdePerspective *perspective)
{
  return g_strdup ("preferences");
}

static gchar *
ide_preferences_perspective_get_title (IdePerspective *perspective)
{
  return g_strdup (_("Preferences"));
}

static gchar *
ide_preferences_perspective_get_icon_name (IdePerspective *perspective)
{
  return g_strdup ("preferences-system-symbolic");
}

static gchar *
ide_preferences_perspective_get_accelerator (IdePerspective *perspective)
{
  return g_strdup ("<ctrl>comma");
}

static void
ide_perspective_iface_init (IdePerspectiveInterface *iface)
{
  iface->get_id = ide_preferences_perspective_get_id;
  iface->get_icon_name = ide_preferences_perspective_get_icon_name;
  iface->get_title = ide_preferences_perspective_get_title;
  iface->get_accelerator = ide_preferences_perspective_get_accelerator;
}
