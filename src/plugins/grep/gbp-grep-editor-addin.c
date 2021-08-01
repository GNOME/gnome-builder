/* gbp-grep-editor-addin.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-grep-editor-addin"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-editor.h>
#include <libide-foundry.h>
#include <libide-gui.h>

#include "gbp-grep-editor-addin.h"
#include "gbp-grep-panel.h"

#define I_(s) g_intern_static_string(s)

struct _GbpGrepEditorAddin
{
  GObject    parent_instance;

  GtkWidget *panel;
};

static void
gbp_grep_editor_page_addin_show_project_panel_action (GSimpleAction *action,
                                                      GVariant      *variant,
                                                      gpointer       user_data)
{
  GbpGrepEditorAddin *self = GBP_GREP_EDITOR_ADDIN (user_data);

  ide_widget_reveal_and_grab (self->panel);
}

static const DzlShortcutEntry grep_shortcut_entries[] = {
  { "org.gnome.builder.panel",
    0, NULL,
    N_("Editor shortcuts"),
    N_("Panels"),
    N_("Show Find in Project panel") },
};

static const GActionEntry actions[] = {
  { "show-project-panel", gbp_grep_editor_page_addin_show_project_panel_action },
};

static void
gbp_grep_editor_addin_load (IdeEditorAddin   *addin,
                            IdeEditorSurface *editor_surface)
{
  GbpGrepEditorAddin *self = (GbpGrepEditorAddin *)addin;
  GtkWidget *utilities;
  g_autoptr(GSimpleActionGroup) group = NULL;
  DzlShortcutController *controller;

  g_assert (GBP_IS_GREP_EDITOR_ADDIN (self));

  utilities = ide_editor_surface_get_utilities (IDE_EDITOR_SURFACE (editor_surface));

  self->panel = gbp_grep_panel_new ();
  gtk_container_add (GTK_CONTAINER (utilities), self->panel);
  gtk_widget_show (self->panel);

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (editor_surface), "grep", G_ACTION_GROUP (group));

  controller = dzl_shortcut_controller_find (GTK_WIDGET (editor_surface));
  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.panel"),
                                              I_("<Primary><Shift>f"),
                                              DZL_SHORTCUT_PHASE_GLOBAL,
                                              I_("grep.show-project-panel"));

  dzl_shortcut_manager_add_shortcut_entries (NULL,
                                             grep_shortcut_entries,
                                             G_N_ELEMENTS (grep_shortcut_entries),
                                             GETTEXT_PACKAGE);
}

static void
gbp_grep_editor_addin_unload (IdeEditorAddin   *addin,
                              IdeEditorSurface *editor_surface)
{
  GbpGrepEditorAddin *self = (GbpGrepEditorAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GREP_EDITOR_ADDIN (self));

  gtk_widget_insert_action_group (GTK_WIDGET (editor_surface), "grep", NULL);

  g_clear_pointer (&self->panel, gtk_widget_destroy);
}

static void
editor_addin_iface_init (IdeEditorAddinInterface *iface)
{
  iface->load = gbp_grep_editor_addin_load;
  iface->unload = gbp_grep_editor_addin_unload;
}

G_DEFINE_TYPE_WITH_CODE (GbpGrepEditorAddin, gbp_grep_editor_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_EDITOR_ADDIN, editor_addin_iface_init))

static void
gbp_grep_editor_addin_class_init (GbpGrepEditorAddinClass *klass)
{
}

static void
gbp_grep_editor_addin_init (GbpGrepEditorAddin *self)
{
}
