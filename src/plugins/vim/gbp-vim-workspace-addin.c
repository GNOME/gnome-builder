/* gbp-vim-workspace-addin.c
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

#define G_LOG_DOMAIN "gbp-vim-workspace-addin"

#include "config.h"

#include <gtk/gtk.h>
#include <libpanel.h>

#include <libide-editor.h>
#include <libide-gui.h>

#include "gbp-vim-workspace-addin.h"

struct _GbpVimWorkspaceAddin
{
  GObject parent_instance;

  GSettings *editor_settings;

  GtkLabel *command_bar;
  GtkLabel *command;

  guint active : 1;
};

enum {
  PROP_0,
  PROP_ACTIVE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
on_keybindings_changed_cb (GbpVimWorkspaceAddin *self,
                           const char           *key,
                           GSettings            *editor_settings)
{
  g_autofree char *keybindings = NULL;
  gboolean active;

  g_assert (GBP_IS_VIM_WORKSPACE_ADDIN (self));
  g_assert (g_strcmp0 (key, "keybindings") == 0);
  g_assert (G_IS_SETTINGS (editor_settings));

  keybindings = g_settings_get_string (editor_settings, "keybindings");
  active = g_strcmp0 (keybindings, "vim") == 0;

  if (active != self->active)
    {
      self->active = active;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTIVE]);
    }
}

static void
gbp_vim_workspace_addin_load (IdeWorkspaceAddin *addin,
                              IdeWorkspace      *workspace)
{
  GbpVimWorkspaceAddin *self = (GbpVimWorkspaceAddin *)addin;
  g_autofree char *keybindings = NULL;
  PanelStatusbar *statusbar;
  PangoAttrList *attrs;

  IDE_ENTRY;

  g_assert (IDE_IS_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  self->editor_settings = g_settings_new ("org.gnome.builder.editor");
  keybindings = g_settings_get_string (self->editor_settings, "keybindings");
  g_signal_connect_object (self->editor_settings,
                           "changed::keybindings",
                           G_CALLBACK (on_keybindings_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  self->active = g_strcmp0 (keybindings, "vim") == 0;

  if (!self->active)
    g_debug ("Vim plugin loaded but inactive as keybindings are currently \"%s\"",
             keybindings);

  attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_family_new ("Monospace"));

  self->command_bar = g_object_new (GTK_TYPE_LABEL,
                                    "attributes", attrs,
                                    "hexpand", TRUE,
                                    "selectable", TRUE,
                                    "visible", FALSE,
                                    "xalign", .0f,
                                    NULL);
  self->command = g_object_new (GTK_TYPE_LABEL,
                                "attributes", attrs,
                                "visible", FALSE,
                                "xalign", 1.f,
                                NULL);

  statusbar = ide_workspace_get_statusbar (workspace);

  panel_statusbar_add_prefix (statusbar, 10000, GTK_WIDGET (self->command_bar));
  panel_statusbar_add_suffix (statusbar, 21000, GTK_WIDGET (self->command));

  pango_attr_list_unref (attrs);

  IDE_EXIT;
}

static void
gbp_vim_workspace_addin_unload (IdeWorkspaceAddin *addin,
                                IdeWorkspace      *workspace)
{
  GbpVimWorkspaceAddin *self = (GbpVimWorkspaceAddin *)addin;
  PanelStatusbar *statusbar;

  IDE_ENTRY;

  g_assert (IDE_IS_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  statusbar = ide_workspace_get_statusbar (workspace);

  panel_statusbar_remove (statusbar, GTK_WIDGET (self->command_bar));
  panel_statusbar_remove (statusbar, GTK_WIDGET (self->command));

  self->command_bar = NULL;
  self->command = NULL;
  self->active = FALSE;

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ACTIVE]);

  g_clear_object (&self->editor_settings);

  IDE_EXIT;
}

static void
gbp_vim_workspace_addin_page_changed (IdeWorkspaceAddin *addin,
                                      IdePage           *page)
{
  GbpVimWorkspaceAddin *self = (GbpVimWorkspaceAddin *)addin;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_VIM_WORKSPACE_ADDIN (self));
  g_assert (!page || IDE_IS_PAGE (page));

  if (!IDE_IS_EDITOR_PAGE (page))
    {
      gbp_vim_workspace_addin_set_command_bar (self, NULL);
      gbp_vim_workspace_addin_set_command (self, NULL);
    }

  IDE_EXIT;
}

static void
workspace_addin_iface_init (IdeWorkspaceAddinInterface *iface)
{
  iface->load = gbp_vim_workspace_addin_load;
  iface->unload = gbp_vim_workspace_addin_unload;
  iface->page_changed = gbp_vim_workspace_addin_page_changed;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpVimWorkspaceAddin, gbp_vim_workspace_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKSPACE_ADDIN, workspace_addin_iface_init))

static void
gbp_vim_workspace_addin_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  GbpVimWorkspaceAddin *self = GBP_VIM_WORKSPACE_ADDIN (object);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, self->active);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_vim_workspace_addin_class_init (GbpVimWorkspaceAddinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gbp_vim_workspace_addin_get_property;

  /**
   * GbpVimWorkspaceAddin:active:
   *
   * The "active" property is bound by other vim plugin hooks so that they
   * may all enable/disable together based on the current keybindings setting
   * in org.gnome.builder.editor.
   */
  properties [PROP_ACTIVE] =
    g_param_spec_boolean ("active",
                          "Active",
                          "If vim is the active keybindings for the application",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_vim_workspace_addin_init (GbpVimWorkspaceAddin *self)
{
}

void
gbp_vim_workspace_addin_set_command (GbpVimWorkspaceAddin *self,
                                     const char           *command)
{
  g_return_if_fail (GBP_IS_VIM_WORKSPACE_ADDIN (self));

  if (self->command != NULL)
    {
      gtk_label_set_label (self->command, command);
      gtk_widget_set_visible (GTK_WIDGET (self->command), !ide_str_empty0 (command));
    }
}

void
gbp_vim_workspace_addin_set_command_bar (GbpVimWorkspaceAddin *self,
                                         const char           *command_bar)
{
  g_return_if_fail (GBP_IS_VIM_WORKSPACE_ADDIN (self));

  if (self->command_bar != NULL)
    {
      gtk_label_set_label (self->command_bar, command_bar);
      gtk_widget_set_visible (GTK_WIDGET (self->command_bar), !ide_str_empty0 (command_bar));
    }
}

gboolean
gbp_vim_workspace_addin_get_active (GbpVimWorkspaceAddin *self)
{
  g_return_val_if_fail (GBP_IS_VIM_WORKSPACE_ADDIN (self), FALSE);

  return self->active;
}
