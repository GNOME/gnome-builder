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
  g_autofree gchar *icon_name = NULL;
  g_autofree gchar *title = NULL;
  GtkListBoxRow *row;
  GtkBox *box;
  GtkImage *image;
  GtkLabel *label;
  GtkWidget *widget;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_GENESIS_ADDIN (exten));
  g_assert (IDE_IS_GENESIS_PERSPECTIVE (self));

  icon_name = ide_genesis_addin_get_icon_name (IDE_GENESIS_ADDIN (exten));
  title = ide_genesis_addin_get_title (IDE_GENESIS_ADDIN (exten));
  widget = ide_genesis_addin_get_widget (IDE_GENESIS_ADDIN (exten));

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      "visible", TRUE,
                      NULL);
  box = g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_HORIZONTAL,
                      "spacing", 18,
                      "visible", TRUE,
                      NULL);
  image = g_object_new (GTK_TYPE_IMAGE,
                        "hexpand", FALSE,
                        "icon-name", icon_name,
                        "pixel-size", 32,
                        "visible", TRUE,
                        NULL);
  label = g_object_new (GTK_TYPE_LABEL,
                        "label", title,
                        "valign", GTK_ALIGN_BASELINE,
                        "visible", TRUE,
                        "wrap", TRUE,
                        "xalign", 0.0f,
                        NULL);

  g_object_set_data (G_OBJECT (row), "IDE_GENESIS_ADDIN", exten);

  gtk_container_add (GTK_CONTAINER (row), GTK_WIDGET (box));
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (image));
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (label));
  gtk_container_add (GTK_CONTAINER (self->list_box), GTK_WIDGET (row));

  if (widget != NULL)
    gtk_container_add (GTK_CONTAINER (self->stack), widget);
}

static void
ide_genesis_perspective_addin_removed (PeasExtensionSet *set,
                                       PeasPluginInfo   *plugin_info,
                                       PeasExtension    *exten,
                                       gpointer          user_data)
{
  IdeGenesisPerspective *self = user_data;
  GtkWidget *widget;
  GList *children;
  GList *iter;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_GENESIS_ADDIN (exten));
  g_assert (IDE_IS_GENESIS_PERSPECTIVE (self));

  children = gtk_container_get_children (GTK_CONTAINER (self->list_box));

  for (iter = children; iter; iter = iter->next)
    {
      gpointer data = g_object_get_data (iter->data, "IDE_GENESIS_ADDIN");

      if (data == (gpointer)exten)
        {
          gtk_container_remove (GTK_CONTAINER (self->list_box), iter->data);
          break;
        }
    }

  g_list_free (children);

  widget = ide_genesis_addin_get_widget (IDE_GENESIS_ADDIN (exten));
  if (widget != NULL)
    gtk_container_remove (GTK_CONTAINER (self->stack), widget);
}

static void
ide_genesis_perspective_row_activated (IdeGenesisPerspective *self,
                                       GtkListBoxRow         *row,
                                       GtkListBox            *list_box)
{
  IdeGenesisAddin *addin;
  GtkWidget *child;

  g_assert (GTK_IS_LIST_BOX (list_box));
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (IDE_IS_GENESIS_PERSPECTIVE (self));

  addin = g_object_get_data (G_OBJECT (row), "IDE_GENESIS_ADDIN");
  if (addin == NULL)
    return;

  child = ide_genesis_addin_get_widget (addin);
  if (child == NULL)
    return;

  gtk_stack_set_visible_child (self->stack, child);
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

  g_signal_connect_object (self->list_box,
                           "row-activated",
                           G_CALLBACK (ide_genesis_perspective_row_activated),
                           self,
                           G_CONNECT_SWAPPED);
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
