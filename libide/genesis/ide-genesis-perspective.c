/* ide-genesis-perspective.c
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
#include <libpeas/peas.h>

#include "ide-genesis-addin.h"
#include "ide-genesis-perspective.h"

struct _IdeGenesisPerspective
{
  GtkBin            parent_instance;

  PeasExtensionSet *addins;

  GtkHeaderBar     *header_bar;
  GtkListBox       *list_box;
  GtkStack         *stack;
};

static void perspective_iface_init (IdePerspectiveInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdeGenesisPerspective, ide_genesis_perspective, GTK_TYPE_BIN, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_PERSPECTIVE, perspective_iface_init))

static void
ide_genesis_perspective_addin_added (PeasExtensionSet *set,
                                     PeasPluginInfo   *plugin_info,
                                     PeasExtension    *exten,
                                     gpointer          user_data)
{
  IdeGenesisPerspective *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_GENESIS_ADDIN (exten));
  g_assert (IDE_IS_GENESIS_PERSPECTIVE (self));
}

static void
ide_genesis_perspective_addin_removed (PeasExtensionSet *set,
                                       PeasPluginInfo   *plugin_info,
                                       PeasExtension    *exten,
                                       gpointer          user_data)
{
  IdeGenesisPerspective *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_GENESIS_ADDIN (exten));
  g_assert (IDE_IS_GENESIS_PERSPECTIVE (self));
}

static void
ide_genesis_perspective_constructed (GObject *object)
{
  IdeGenesisPerspective *self = (IdeGenesisPerspective *)object;

  G_OBJECT_CLASS (ide_genesis_perspective_parent_class)->constructed (object);

  self->addins = peas_extension_set_new (peas_engine_get_default (),
                                         IDE_TYPE_GENESIS_ADDIN,
                                         NULL);

  peas_extension_set_foreach (self->addins, ide_genesis_perspective_addin_added, self);

  g_signal_connect (self->addins,
                    "extension-added",
                    G_CALLBACK (ide_genesis_perspective_addin_added),
                    self);
  g_signal_connect (self->addins,
                    "extension-rmeoved",
                    G_CALLBACK (ide_genesis_perspective_addin_removed),
                    self);
}

static void
ide_genesis_perspective_finalize (GObject *object)
{
  IdeGenesisPerspective *self = (IdeGenesisPerspective *)object;

  g_clear_object (&self->addins);

  G_OBJECT_CLASS (ide_genesis_perspective_parent_class)->finalize (object);
}

static void
ide_genesis_perspective_class_init (IdeGenesisPerspectiveClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = ide_genesis_perspective_constructed;
  object_class->finalize = ide_genesis_perspective_finalize;

  gtk_widget_class_set_css_name (widget_class, "genesisperspective");
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-genesis-perspective.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeGenesisPerspective, list_box);
  gtk_widget_class_bind_template_child (widget_class, IdeGenesisPerspective, stack);
  gtk_widget_class_bind_template_child (widget_class, IdeGenesisPerspective, header_bar);
}

static void
ide_genesis_perspective_init (IdeGenesisPerspective *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static gchar *
ide_genesis_perspective_get_id (IdePerspective *perspective)
{
  return g_strdup ("genesis");
}

static gboolean
ide_genesis_perspective_is_early (IdePerspective *perspective)
{
  return TRUE;
}

static GtkWidget *
ide_genesis_perspective_get_titlebar (IdePerspective *perspective)
{
  g_return_val_if_fail (IDE_IS_GENESIS_PERSPECTIVE (perspective), NULL);

  return GTK_WIDGET (IDE_GENESIS_PERSPECTIVE (perspective)->header_bar);
}

static void
perspective_iface_init (IdePerspectiveInterface *iface)
{
  iface->get_id = ide_genesis_perspective_get_id;
  iface->is_early = ide_genesis_perspective_is_early;
  iface->get_titlebar = ide_genesis_perspective_get_titlebar;
}
