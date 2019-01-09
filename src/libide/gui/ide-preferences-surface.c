/* ide-preferences-surface.c
 *
 * Copyright 2015-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-preferences-surface"

#include "config.h"

#include <dazzle.h>
#include <glib/gi18n.h>
#include <libpeas/peas.h>

#include "ide-preferences-addin.h"
#include "ide-preferences-builtin-private.h"
#include "ide-preferences-surface.h"
#include "ide-surface.h"

struct _IdePreferencesSurface
{
  IdeSurface          parent_instance;
  DzlPreferencesView *view;
  PeasExtensionSet   *extensions;
};

G_DEFINE_TYPE (IdePreferencesSurface, ide_preferences_surface, IDE_TYPE_SURFACE)

static void
ide_preferences_surface_addin_added_cb (PeasExtensionSet *set,
                                        PeasPluginInfo   *plugin_info,
                                        PeasExtension    *extension,
                                        gpointer          user_data)
{
  IdePreferencesSurface *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_PREFERENCES_ADDIN (extension));
  g_assert (IDE_IS_PREFERENCES_SURFACE (self));

  ide_preferences_addin_load (IDE_PREFERENCES_ADDIN (extension), DZL_PREFERENCES (self->view));
  dzl_preferences_view_reapply_filter (self->view);
}

static void
ide_preferences_surface_addin_removed_cb (PeasExtensionSet *set,
                                          PeasPluginInfo   *plugin_info,
                                          PeasExtension    *extension,
                                          gpointer          user_data)
{
  IdePreferencesSurface *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_PREFERENCES_ADDIN (extension));
  g_assert (IDE_IS_PREFERENCES_SURFACE (self));

  ide_preferences_addin_unload (IDE_PREFERENCES_ADDIN (extension), DZL_PREFERENCES (self->view));
  dzl_preferences_view_reapply_filter (self->view);
}

static void
ide_preferences_surface_destroy (GtkWidget *widget)
{
  IdePreferencesSurface *self = (IdePreferencesSurface *)widget;

  g_clear_object (&self->extensions);

  GTK_WIDGET_CLASS (ide_preferences_surface_parent_class)->destroy (widget);
}

static void
ide_preferences_surface_constructed (GObject *object)
{
  IdePreferencesSurface *self = (IdePreferencesSurface *)object;

  G_OBJECT_CLASS (ide_preferences_surface_parent_class)->constructed (object);

  _ide_preferences_builtin_register (DZL_PREFERENCES (self->view));

  self->extensions = peas_extension_set_new (peas_engine_get_default (),
                                             IDE_TYPE_PREFERENCES_ADDIN,
                                             NULL);

  g_signal_connect (self->extensions,
                    "extension-added",
                    G_CALLBACK (ide_preferences_surface_addin_added_cb),
                    self);

  g_signal_connect (self->extensions,
                    "extension-removed",
                    G_CALLBACK (ide_preferences_surface_addin_removed_cb),
                    self);

  peas_extension_set_foreach (self->extensions,
                              ide_preferences_surface_addin_added_cb,
                              self);
}

static void
ide_preferences_surface_class_init (IdePreferencesSurfaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = ide_preferences_surface_constructed;

  widget_class->destroy = ide_preferences_surface_destroy;
}

static void
ide_preferences_surface_init (IdePreferencesSurface *self)
{
  ide_surface_set_icon_name (IDE_SURFACE (self), "preferences-system-symbolic");
  gtk_widget_set_name (GTK_WIDGET (self), "preferences");

  self->view = g_object_new (DZL_TYPE_PREFERENCES_VIEW,
                             "visible", TRUE,
                             NULL);
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->view));
}
