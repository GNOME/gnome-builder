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

#include <libide-gui.h>
#include <libide-projects.h>

#include "gbp-vcsui-clone-page.h"

struct _GbpVcsuiClonePage
{
  GtkWidget           parent_instance;

  AdwEntryRow        *author_email_row;
  AdwEntryRow        *author_name_row;
  GtkMenuButton      *branch_button;
  GtkLabel           *branch_label;
  AdwEntryRow        *location_row;
  GtkWidget          *main;
  GtkStack           *stack;
  VteTerminal        *terminal;

  IdeVcsCloneRequest *request;
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

  ide_vcs_clone_request_set_directory (self->request, directory);
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
clone_action (GtkWidget  *widget,
              const char *action_name,
              GVariant   *param)
{
  GbpVcsuiClonePage *self = (GbpVcsuiClonePage *)widget;
  static guint count;

  IDE_ENTRY;

  g_assert (GBP_IS_VCSUI_CLONE_PAGE (self));

  if (count++ % 2 == 0)
  {
    gtk_stack_set_visible_child_name (self->stack, "progress");
  }
  else
  {
    gtk_stack_set_visible_child_name (self->stack, "details");
  }

  IDE_EXIT;
}

static void
branch_activated_cb (GbpVcsuiClonePage *self,
                     guint              position,
                     GtkListView       *list_view)
{
  g_autoptr(IdeVcsBranch) branch = NULL;
  g_autofree char *branch_id = NULL;
  GListModel *model;

  IDE_ENTRY;

  g_assert (GBP_IS_VCSUI_CLONE_PAGE (self));
  g_assert (GTK_IS_LIST_VIEW (list_view));

  model = G_LIST_MODEL (gtk_list_view_get_model (list_view));
  branch = g_list_model_get_item (model, position);
  branch_id = ide_vcs_branch_dup_id (branch);

  ide_vcs_clone_request_set_branch_name (self->request, branch_id);

  gtk_menu_button_popdown (self->branch_button);

  IDE_EXIT;
}

static void
branch_popover_show_cb (GbpVcsuiClonePage *self,
                        GtkPopover        *popover)
{
  IDE_ENTRY;

  g_assert (GBP_IS_VCSUI_CLONE_PAGE (self));
  g_assert (GTK_IS_POPOVER (popover));

  ide_vcs_clone_request_populate_branches (self->request);

  IDE_EXIT;
}

static void
branch_name_changed_cb (GbpVcsuiClonePage  *self,
                        GParamSpec         *pspec,
                        IdeVcsCloneRequest *request)
{
  const char *branch_name;
  gboolean empty;

  g_assert (GBP_IS_VCSUI_CLONE_PAGE (self));
  g_assert (IDE_IS_VCS_CLONE_REQUEST (request));

  branch_name = ide_vcs_clone_request_get_branch_name (request);

  /* Very much a git-ism, but that's all we support right now */
  if (branch_name != NULL && g_str_has_prefix (branch_name, "refs/heads/"))
    branch_name += strlen ("refs/heads/");

  empty = ide_str_empty0 (branch_name);

  gtk_widget_set_tooltip_text (GTK_WIDGET (self->branch_label),
                               empty ? NULL : branch_name);
  gtk_label_set_label (self->branch_label, branch_name);
  gtk_widget_set_visible (GTK_WIDGET (self->branch_label), !empty);
}

static void
gbp_vcsui_clone_page_root (GtkWidget *widget)
{
  GbpVcsuiClonePage *self = (GbpVcsuiClonePage *)widget;
  IdeContext *context;

  g_assert (GBP_IS_VCSUI_CLONE_PAGE (self));

  GTK_WIDGET_CLASS (gbp_vcsui_clone_page_parent_class)->root (widget);

  if ((context = ide_widget_get_context (widget)))
    ide_object_append (IDE_OBJECT (context), IDE_OBJECT (self->request));
}

static void
gbp_vcsui_clone_page_dispose (GObject *object)
{
  GbpVcsuiClonePage *self = (GbpVcsuiClonePage *)object;

  ide_object_destroy (IDE_OBJECT (self->request));
  g_clear_pointer (&self->main, gtk_widget_unparent);

  G_OBJECT_CLASS (gbp_vcsui_clone_page_parent_class)->dispose (object);
}

static void
gbp_vcsui_clone_page_class_init (GbpVcsuiClonePageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gbp_vcsui_clone_page_dispose;

  widget_class->root = gbp_vcsui_clone_page_root;

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/vcsui/gbp-vcsui-clone-page.ui");

  gtk_widget_class_bind_template_child (widget_class, GbpVcsuiClonePage, author_email_row);
  gtk_widget_class_bind_template_child (widget_class, GbpVcsuiClonePage, author_name_row);
  gtk_widget_class_bind_template_child (widget_class, GbpVcsuiClonePage, branch_button);
  gtk_widget_class_bind_template_child (widget_class, GbpVcsuiClonePage, branch_label);
  gtk_widget_class_bind_template_child (widget_class, GbpVcsuiClonePage, location_row);
  gtk_widget_class_bind_template_child (widget_class, GbpVcsuiClonePage, main);
  gtk_widget_class_bind_template_child (widget_class, GbpVcsuiClonePage, request);
  gtk_widget_class_bind_template_child (widget_class, GbpVcsuiClonePage, stack);
  gtk_widget_class_bind_template_child (widget_class, GbpVcsuiClonePage, terminal);

  gtk_widget_class_bind_template_callback (widget_class, location_row_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, branch_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, branch_name_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, branch_popover_show_cb);

  gtk_widget_class_install_action (widget_class, "clone-page.select-folder", NULL, select_folder_action);
  gtk_widget_class_install_action (widget_class, "clone-page.clone", NULL, clone_action);

  g_type_ensure (VTE_TYPE_TERMINAL);
  g_type_ensure (IDE_TYPE_VCS_CLONE_REQUEST);
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
