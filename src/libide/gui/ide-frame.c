/* ide-frame.c
 *
 * Copyright 2017-2021 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-frame"

#include "config.h"

#include <adwaita.h>
#include <glib/gi18n.h>
#include <libpeas/peas.h>

#include <libide-core.h>
#include <libide-threading.h>

#include "ide-frame.h"
#include "ide-frame-addin.h"

/**
 * SECTION:ide-frame
 * @title: IdeFrame
 * @short_description: A stack of #IdePage
 *
 * This widget is used to represent a stack of #IdePage widgets.  it
 * includes an #IdeFrameHeader at the top, and then a stack of pages
 * below.
 *
 * If there are no #IdePage visibile, then an empty state widget is
 * displayed with some common information for the user.
 */

struct _IdeFrame
{
  PanelFrame        parent_instance;
  PeasExtensionSet *addins;
};

G_DEFINE_TYPE (IdeFrame, ide_frame, PANEL_TYPE_FRAME)

static void
ide_frame_notify_addin_of_page (PeasExtensionSet *set,
                                PeasPluginInfo   *plugin_info,
                                PeasExtension    *exten,
                                gpointer          user_data)
{
  IdeFrameAddin *addin = (IdeFrameAddin *)exten;
  IdePage *page = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_FRAME_ADDIN (addin));
  g_assert (!page || IDE_IS_PAGE (page));

  ide_frame_addin_set_page (addin, page);
}

static void
ide_frame_notify_visible_child (IdeFrame   *self,
                                GParamSpec *pspec)
{
  PanelWidget *visible_child;

  g_assert (IDE_IS_FRAME (self));

#if 0
  /* FIXME: We can probably this differently in GTK 4
   *
   * Mux/Proxy actions to our level so that they also be activated
   * from the header bar without any weirdness by the View.
   */
  dzl_gtk_widget_mux_action_groups (GTK_WIDGET (self), visible_child,
                                    "IDE_FRAME_MUXED_ACTION");
#endif

  visible_child = panel_frame_get_visible_child (PANEL_FRAME (self));

  if (self->addins != NULL)
    peas_extension_set_foreach (self->addins,
                                ide_frame_notify_addin_of_page,
                                visible_child);
}

static void
ide_frame_addin_added (PeasExtensionSet *set,
                       PeasPluginInfo   *plugin_info,
                       PeasExtension    *exten,
                       gpointer          user_data)
{
  IdeFrameAddin *addin = (IdeFrameAddin *)exten;
  IdeFrame *self = user_data;
  IdePage *visible_child;

  g_assert (IDE_IS_FRAME (self));
  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_FRAME_ADDIN (addin));

  ide_frame_addin_load (addin, self);

  visible_child = IDE_PAGE (panel_frame_get_visible_child (PANEL_FRAME (self)));

  if (visible_child != NULL)
    ide_frame_addin_set_page (addin, visible_child);
}

static void
ide_frame_addin_removed (PeasExtensionSet *set,
                         PeasPluginInfo   *plugin_info,
                         PeasExtension    *exten,
                         gpointer          user_data)
{
  IdeFrameAddin *addin = (IdeFrameAddin *)exten;
  IdeFrame *self = user_data;

  g_assert (IDE_IS_FRAME (self));
  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_FRAME_ADDIN (addin));

  ide_frame_addin_set_page (addin, NULL);
  ide_frame_addin_unload (addin, self);
}

static void
ide_frame_constructed (GObject *object)
{
  IdeFrame *self = (IdeFrame *)object;

  g_assert (IDE_IS_FRAME (self));

  G_OBJECT_CLASS (ide_frame_parent_class)->constructed (object);

  self->addins = peas_extension_set_new (peas_engine_get_default (),
                                         IDE_TYPE_FRAME_ADDIN,
                                         NULL);

  g_signal_connect (self->addins,
                    "extension-added",
                    G_CALLBACK (ide_frame_addin_added),
                    self);

  g_signal_connect (self->addins,
                    "extension-removed",
                    G_CALLBACK (ide_frame_addin_removed),
                    self);

  peas_extension_set_foreach (self->addins,
                              ide_frame_addin_added,
                              self);
}

static void
ide_frame_dispose (GObject *object)
{
  IdeFrame *self = (IdeFrame *)object;

  g_assert (IDE_IS_FRAME (self));

  g_clear_object (&self->addins);

  G_OBJECT_CLASS (ide_frame_parent_class)->dispose (object);
}

static void
ide_frame_class_init (IdeFrameClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ide_frame_dispose;
  object_class->constructed = ide_frame_constructed;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-gui/ui/ide-frame.ui");
}

static void
ide_frame_init (IdeFrame *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect (self,
                    "notify::visible-child",
                    G_CALLBACK (ide_frame_notify_visible_child),
                    NULL);
}

GtkWidget *
ide_frame_new (void)
{
  return g_object_new (IDE_TYPE_FRAME, NULL);
}

/**
 * ide_frame_addin_find_by_module_name:
 * @frame: An #IdeFrame
 * @module_name: the module name which provides the addin
 *
 * This function will locate the #IdeFrameAddin that was registered by
 * the plugin named @module_name (which should match the "Module" field
 * provided in the .plugin file).
 *
 * If no module was found or that module does not implement the
 * #IdeFrameAddinInterface, then %NULL is returned.
 *
 * Returns: (transfer none) (nullable): An #IdeFrameAddin or %NULL
 */
IdeFrameAddin *
ide_frame_addin_find_by_module_name (IdeFrame    *frame,
                                     const gchar *module_name)
{
  PeasExtension *ret = NULL;
  PeasPluginInfo *plugin_info;

  g_return_val_if_fail (IDE_IS_FRAME (frame), NULL);
  g_return_val_if_fail (frame->addins != NULL, NULL);
  g_return_val_if_fail (module_name != NULL, NULL);

  plugin_info = peas_engine_get_plugin_info (peas_engine_get_default (), module_name);

  if (plugin_info != NULL)
    ret = peas_extension_set_get_extension (frame->addins, plugin_info);
  else
    g_warning ("No addin could be found matching module \"%s\"", module_name);

  return ret ? IDE_FRAME_ADDIN (ret) : NULL;
}
