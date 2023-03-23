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

#include <glib/gi18n.h>

#include <adwaita.h>
#include <libpeas.h>

#include <libide-core.h>
#include <libide-threading.h>

#include "ide-application-private.h"
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
  guint             use_tabbar : 1;
  guint             constructed : 1;
};

G_DEFINE_TYPE (IdeFrame, ide_frame, PANEL_TYPE_FRAME)

enum {
  PROP_0,
  PROP_USE_TABBAR,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
ide_frame_notify_addin_of_page (PeasExtensionSet *set,
                                PeasPluginInfo   *plugin_info,
                                GObject    *exten,
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

  visible_child = panel_frame_get_visible_child (PANEL_FRAME (self));

  if (self->addins != NULL)
    peas_extension_set_foreach (self->addins,
                                ide_frame_notify_addin_of_page,
                                visible_child);
}

static void
ide_frame_addin_added (PeasExtensionSet *set,
                       PeasPluginInfo   *plugin_info,
                       GObject    *exten,
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
                         GObject    *exten,
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
ide_frame_reload_addins (IdeFrame *self)
{
  IDE_ENTRY;

  g_assert (IDE_IS_FRAME (self));

  g_clear_object (&self->addins);
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
  peas_extension_set_foreach (self->addins, ide_frame_addin_added, self);

  IDE_EXIT;
}

static void
status_page_pressed_cb (IdeFrame        *self,
                        double           x,
                        double           y,
                        int              n_press,
                        GtkGestureClick *click)
{
  GtkRoot *root;

  g_assert (IDE_IS_FRAME (self));
  g_assert (GTK_IS_GESTURE_CLICK (click));

  root = gtk_widget_get_root (GTK_WIDGET (self));
  gtk_root_set_focus (root, NULL);
}

static gboolean
interface_style_to_tabbar (GValue *value,
                           GVariant *variant,
                           gpointer user_data)
{
  g_value_set_boolean (value, ide_str_equal0 (g_variant_get_string (variant, NULL), "tab-bar"));
  return TRUE;
}

static void
ide_frame_constructed (GObject *object)
{
  IdeFrame *self = (IdeFrame *)object;
  PanelFrameHeader *header;

  g_assert (IDE_IS_FRAME (self));

  G_OBJECT_CLASS (ide_frame_parent_class)->constructed (object);

  g_settings_bind_with_mapping (IDE_APPLICATION_DEFAULT->settings, "interface-style",
                                self, "use-tabbar",
                                G_SETTINGS_BIND_GET,
                                interface_style_to_tabbar, NULL, NULL, NULL);

  self->constructed = TRUE;

  if (self->use_tabbar)
    header = PANEL_FRAME_HEADER (panel_frame_tab_bar_new ());
  else
    header = PANEL_FRAME_HEADER (panel_frame_header_bar_new ());
  panel_frame_set_header (PANEL_FRAME (self), header);
  ide_frame_reload_addins (self);
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
ide_frame_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  IdeFrame *self = IDE_FRAME (object);

  switch (prop_id)
    {
    case PROP_USE_TABBAR:
      g_value_set_boolean (value, ide_frame_get_use_tabbar (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_frame_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  IdeFrame *self = IDE_FRAME (object);

  switch (prop_id)
    {
    case PROP_USE_TABBAR:
      ide_frame_set_use_tabbar (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_frame_class_init (IdeFrameClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = ide_frame_constructed;
  object_class->dispose = ide_frame_dispose;
  object_class->get_property = ide_frame_get_property;
  object_class->set_property = ide_frame_set_property;

  properties [PROP_USE_TABBAR] =
    g_param_spec_boolean ("use-tabbar",
                          "Use Tabbar",
                          "If tabs should be used",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-gui/ui/ide-frame.ui");
  gtk_widget_class_bind_template_callback (widget_class, status_page_pressed_cb);
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
  GObject *ret = NULL;
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

/**
 * ide_frame_get_position:
 * @self: a #IdeFrame
 *
 * Gets the position in the grid of a frame.
 *
 * Returns: (transfer full): a new #PanelPosition
 */
PanelPosition *
ide_frame_get_position (IdeFrame *self)
{
  return panel_frame_get_position (PANEL_FRAME (self));
}

gboolean
ide_frame_get_use_tabbar (IdeFrame *self)
{
  g_return_val_if_fail (IDE_IS_FRAME (self), FALSE);

  return self->use_tabbar;
}

void
ide_frame_set_use_tabbar (IdeFrame *self,
                          gboolean  use_tabbar)
{
  g_return_if_fail (IDE_IS_FRAME (self));

  use_tabbar = !!use_tabbar;

  if (use_tabbar != self->use_tabbar)
    {
      PanelFrameHeader *header;

      self->use_tabbar = use_tabbar;

      if (self->constructed)
        {
          if (self->use_tabbar)
            header = PANEL_FRAME_HEADER (panel_frame_tab_bar_new ());
          else
            header = PANEL_FRAME_HEADER (panel_frame_header_bar_new ());

          panel_frame_set_header (PANEL_FRAME (self), header);

          ide_frame_reload_addins (self);
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_USE_TABBAR]);
    }
}
