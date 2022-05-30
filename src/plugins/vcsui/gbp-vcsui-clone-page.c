/* gbp-vcsui-clone-page.c
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

#define G_LOG_DOMAIN "gbp-vcsui-clone-page"

#include "config.h"

#include <glib/gi18n.h>

#include <adwaita.h>
#include <vte/vte.h>

#include <libide-projects.h>

#include "gbp-vcsui-clone-page.h"

struct _GbpVcsuiClonePage
{
  GtkWidget    parent_instance;

  GtkWidget   *main;
  AdwEntryRow *location_row;
  AdwEntryRow *author_name_row;
  AdwEntryRow *author_email_row;
  VteTerminal *terminal;
};

G_DEFINE_FINAL_TYPE (GbpVcsuiClonePage, gbp_vcsui_clone_page, GTK_TYPE_WIDGET)

static void
location_row_changed_cb (GbpVcsuiClonePage *self,
                         GtkEditable       *editable)
{
  g_autofree char *expanded = NULL;
  g_autoptr(GFile) directory = NULL;
  const char *text;

  g_assert (GBP_IS_VCSUI_CLONE_PAGE (self));
  g_assert (GTK_IS_EDITABLE (editable));

  text = gtk_editable_get_text (editable);
  expanded = ide_path_expand (text);
  directory = g_file_new_for_path (expanded);

  /* TODO: set value for clone input */
}

static void
select_folder_response_cb (GbpVcsuiClonePage    *self,
                           int                   response_id,
                           GtkFileChooserNative *native)
{
  g_assert (GBP_IS_VCSUI_CLONE_PAGE (self));
  g_assert (GTK_IS_FILE_CHOOSER_NATIVE (native));

  if (response_id == GTK_RESPONSE_ACCEPT)
    {
      g_autoptr(GFile) file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (native));
      g_autofree char *path = ide_path_collapse (g_file_peek_path (file));

      gtk_editable_set_text (GTK_EDITABLE (self->location_row), path);
    }

  gtk_native_dialog_destroy (GTK_NATIVE_DIALOG (native));
}

static void
select_folder_action (GtkWidget  *widget,
                      const char *action_name,
                      GVariant   *param)
{
  GbpVcsuiClonePage *self = (GbpVcsuiClonePage *)widget;
  GtkFileChooserNative *native;
  GtkRoot *root;

  g_assert (GBP_IS_VCSUI_CLONE_PAGE (self));

  root = gtk_widget_get_root (widget);
  native = gtk_file_chooser_native_new (_("Select Location"),
                                        GTK_WINDOW (root),
                                        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                        _("Select"),
                                        _("Cancel"));
  /* TODO: Apply folder from clone input */
#if 0
  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (native),
                                       ide_template_input_get_directory (self->input),
                                       NULL);
#endif
  g_signal_connect_object (native,
                           "response",
                           G_CALLBACK (select_folder_response_cb),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_native_dialog_show (GTK_NATIVE_DIALOG (native));
}

static void
gbp_vcsui_clone_page_dispose (GObject *object)
{
  GbpVcsuiClonePage *self = (GbpVcsuiClonePage *)object;

  g_clear_pointer (&self->main, gtk_widget_unparent);

  G_OBJECT_CLASS (gbp_vcsui_clone_page_parent_class)->dispose (object);
}

static void
gbp_vcsui_clone_page_class_init (GbpVcsuiClonePageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gbp_vcsui_clone_page_dispose;

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/vcsui/gbp-vcsui-clone-page.ui");

  gtk_widget_class_bind_template_child (widget_class, GbpVcsuiClonePage, main);
  gtk_widget_class_bind_template_child (widget_class, GbpVcsuiClonePage, location_row);
  gtk_widget_class_bind_template_child (widget_class, GbpVcsuiClonePage, author_name_row);
  gtk_widget_class_bind_template_child (widget_class, GbpVcsuiClonePage, author_email_row);
  gtk_widget_class_bind_template_child (widget_class, GbpVcsuiClonePage, terminal);

  gtk_widget_class_bind_template_callback (widget_class, location_row_changed_cb);

  gtk_widget_class_install_action (widget_class, "clone-page.select-folder", NULL, select_folder_action);

  g_type_ensure (VTE_TYPE_TERMINAL);
}

static void
gbp_vcsui_clone_page_init (GbpVcsuiClonePage *self)
{
  g_autofree char *projects_dir = ide_path_collapse (ide_get_projects_dir ());
  static GdkRGBA transparent = {0, 0, 0, 0};

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_editable_set_text (GTK_EDITABLE (self->location_row), projects_dir);
  gtk_editable_set_text (GTK_EDITABLE (self->author_name_row), g_get_real_name ());

  vte_terminal_set_colors (self->terminal, NULL, &transparent, NULL, 0);
  vte_terminal_feed (self->terminal, "Cloning git repository…\r\n", -1);
}
