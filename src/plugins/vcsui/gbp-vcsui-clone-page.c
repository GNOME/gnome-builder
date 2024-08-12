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

#include <libide-greeter.h>
#include <libide-gtk.h>
#include <libide-gui.h>
#include <libide-projects.h>

#include "gbp-vcsui-clone-page.h"

struct _GbpVcsuiClonePage
{
  AdwNavigationPage   parent_instance;

  AdwEntryRow        *author_email_row;
  AdwEntryRow        *author_name_row;
  GtkMenuButton      *branch_button;
  GtkLabel           *branch_label;
  AdwEntryRow        *location_row;
  GtkWidget          *main;
  GtkStack           *stack;
  VteTerminal        *terminal;
  AdwEntryRow        *uri_row;
  IdeProgressIcon    *progress;
  GtkLabel           *failure_message;
  GtkLabel           *error_label;

  IdeVcsCloneRequest *request;
};

G_DEFINE_FINAL_TYPE (GbpVcsuiClonePage, gbp_vcsui_clone_page, ADW_TYPE_NAVIGATION_PAGE)

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
select_folder_response_cb (GObject      *object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  GtkFileDialog *dialog = (GtkFileDialog *)object;
  g_autoptr(GbpVcsuiClonePage) self = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) file = NULL;
  g_autofree char *path = NULL;

  g_assert (GTK_IS_FILE_DIALOG (dialog));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_VCSUI_CLONE_PAGE (self));

  if (!(file = gtk_file_dialog_select_folder_finish (dialog, result, &error)))
    return;

  path = ide_path_collapse (g_file_peek_path (file));
  gtk_editable_set_text (GTK_EDITABLE (self->location_row), path);
}

static void
select_folder_action (GtkWidget  *widget,
                      const char *action_name,
                      GVariant   *param)
{
  GbpVcsuiClonePage *self = (GbpVcsuiClonePage *)widget;
  g_autoptr(GtkFileDialog) dialog = NULL;
  GtkRoot *root;

  g_assert (GBP_IS_VCSUI_CLONE_PAGE (self));

  root = gtk_widget_get_root (widget);

  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (dialog, _("Select Location"));
  gtk_file_dialog_set_accept_label (dialog, _("Select"));
  gtk_file_dialog_set_initial_folder (dialog, ide_vcs_clone_request_get_directory (self->request));

  gtk_file_dialog_select_folder (dialog,
                                 GTK_WINDOW (root),
                                 NULL,
                                 select_folder_response_cb,
                                 g_object_ref (self));
}

static void
gbp_vcsui_clone_page_clone_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  IdeVcsCloneRequest *request = (IdeVcsCloneRequest *)object;
  g_autoptr(GbpVcsuiClonePage) self = user_data;
  IdeGreeterWorkspace *greeter;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) directory = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_VCS_CLONE_REQUEST (request));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_VCSUI_CLONE_PAGE (self));

  greeter = IDE_GREETER_WORKSPACE (ide_widget_get_workspace (GTK_WIDGET (self)));

  gtk_widget_hide (GTK_WIDGET (self->progress));

  if (!(directory = ide_vcs_clone_request_clone_finish (request, result, &error)))
    {
      g_message ("Failed to clone repository: %s", error->message);
      gtk_stack_set_visible_child_name (self->stack, "details");
      gtk_label_set_label (self->failure_message,
                           _("A failure occurred while cloning the repository."));
      gtk_label_set_label (self->error_label, error->message);
      IDE_GOTO (failure);
    }
  else
    {
      g_autoptr(IdeProjectInfo) project_info = NULL;

      g_debug ("Clone request complete");

      project_info = ide_project_info_new ();
      ide_project_info_set_file (project_info, directory);
      ide_project_info_set_directory (project_info, directory);

      ide_greeter_workspace_open_project (greeter, project_info);
    }

failure:
  ide_greeter_workspace_end (greeter);

  IDE_EXIT;
}

static void
notify_progress_cb (IdeNotification *notif,
                    GParamSpec      *pspec,
                    IdeProgressIcon *icon)
{
  IdeAnimation *anim;
  double progress;

  g_assert (IDE_IS_NOTIFICATION (notif));
  g_assert (IDE_IS_PROGRESS_ICON (icon));

  anim = g_object_get_data (G_OBJECT (icon), "ANIMATION");
  if (anim != NULL)
    ide_animation_stop (anim);

  progress = ide_notification_get_progress (notif);
  anim = ide_object_animate (icon,
                             IDE_ANIMATION_LINEAR,
                             200,
                             NULL,
                             "progress", progress,
                             NULL);
  g_object_set_data_full (G_OBJECT (icon),
                          "ANIMATION",
                          g_object_ref (anim),
                          g_object_unref);
}

static void
notify_body_cb (IdeNotification *notif,
                GParamSpec      *pspec,
                VteTerminal     *terminal)
{
  g_autofree char *body = NULL;

  g_assert (IDE_IS_NOTIFICATION (notif));
  g_assert (VTE_IS_TERMINAL (terminal));

  /* TODO: we need to plumb something better than IdeNotification to pass
   * essentially PTY data between the worker and the UI process. but this
   * will be fine for now until we can get to it.
   */

  if ((body = ide_notification_dup_body (notif)))
    vte_terminal_feed (terminal, body, -1);
}

static void
clone_action (GtkWidget  *widget,
              const char *action_name,
              GVariant   *param)
{
  GbpVcsuiClonePage *self = (GbpVcsuiClonePage *)widget;
  g_autoptr(IdeNotification) notif = NULL;
  IdeGreeterWorkspace *greeter;
  VtePty *pty;
  int fd, pty_fd;

  IDE_ENTRY;

  g_assert (GBP_IS_VCSUI_CLONE_PAGE (self));

  gtk_stack_set_visible_child_name (self->stack, "progress");
  gtk_widget_show (GTK_WIDGET (self->progress));

  notif = ide_notification_new ();
  g_signal_connect_object (notif,
                           "notify::progress",
                           G_CALLBACK (notify_progress_cb),
                           self->progress,
                           0);
  g_signal_connect_object (notif,
                           "notify::body",
                           G_CALLBACK (notify_body_cb),
                           self->terminal,
                           0);

  greeter = IDE_GREETER_WORKSPACE (ide_widget_get_workspace (widget));
  ide_greeter_workspace_begin (greeter);
  gtk_widget_action_set_enabled (widget, "clone-page.clone", FALSE);

  gtk_label_set_label (self->failure_message, NULL);
  gtk_label_set_label (self->error_label, NULL);

  pty = vte_terminal_get_pty (self->terminal);
  fd = vte_pty_get_fd (pty);
  pty_fd = ide_pty_intercept_create_producer (fd, TRUE);

  ide_vcs_clone_request_clone_async (self->request,
                                     notif,
                                     pty_fd,
                                     NULL,
                                     gbp_vcsui_clone_page_clone_cb,
                                     g_object_ref (self));

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
request_notify_cb (GbpVcsuiClonePage  *self,
                   GParamSpec         *pspec,
                   IdeVcsCloneRequest *request)
{
  IdeVcsCloneRequestValidation flags = 0;

  g_assert (GBP_IS_VCSUI_CLONE_PAGE (self));
  g_assert (IDE_IS_VCS_CLONE_REQUEST (request));

  flags = ide_vcs_clone_request_validate (request);

  if (flags & IDE_VCS_CLONE_REQUEST_INVAL_URI)
    gtk_widget_add_css_class (GTK_WIDGET (self->uri_row), "error");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (self->uri_row), "error");

  if (flags & IDE_VCS_CLONE_REQUEST_INVAL_DIRECTORY)
    gtk_widget_add_css_class (GTK_WIDGET (self->location_row), "error");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (self->location_row), "error");

  if (flags & IDE_VCS_CLONE_REQUEST_INVAL_EMAIL)
    gtk_widget_add_css_class (GTK_WIDGET (self->author_email_row), "error");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (self->author_email_row), "error");

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "clone-page.clone", flags == 0);
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
  gtk_widget_class_bind_template_child (widget_class, GbpVcsuiClonePage, error_label);
  gtk_widget_class_bind_template_child (widget_class, GbpVcsuiClonePage, failure_message);
  gtk_widget_class_bind_template_child (widget_class, GbpVcsuiClonePage, location_row);
  gtk_widget_class_bind_template_child (widget_class, GbpVcsuiClonePage, main);
  gtk_widget_class_bind_template_child (widget_class, GbpVcsuiClonePage, progress);
  gtk_widget_class_bind_template_child (widget_class, GbpVcsuiClonePage, request);
  gtk_widget_class_bind_template_child (widget_class, GbpVcsuiClonePage, stack);
  gtk_widget_class_bind_template_child (widget_class, GbpVcsuiClonePage, terminal);
  gtk_widget_class_bind_template_child (widget_class, GbpVcsuiClonePage, uri_row);

  gtk_widget_class_bind_template_callback (widget_class, location_row_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, branch_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, branch_name_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, branch_popover_show_cb);
  gtk_widget_class_bind_template_callback (widget_class, request_notify_cb);

  gtk_widget_class_install_action (widget_class, "clone-page.select-folder", NULL, select_folder_action);
  gtk_widget_class_install_action (widget_class, "clone-page.clone", NULL, clone_action);

  g_type_ensure (IDE_TYPE_PROGRESS_ICON);
  g_type_ensure (VTE_TYPE_TERMINAL);
  g_type_ensure (IDE_TYPE_VCS_CLONE_REQUEST);
}

static void
gbp_vcsui_clone_page_init (GbpVcsuiClonePage *self)
{
  g_autofree char *projects_dir = ide_path_collapse (ide_get_projects_dir ());
  g_autoptr(VtePty) pty = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_editable_set_text (GTK_EDITABLE (self->location_row), projects_dir);
  gtk_editable_set_text (GTK_EDITABLE (self->author_name_row), g_get_real_name ());

  pty = vte_pty_new_sync (VTE_PTY_DEFAULT, NULL, NULL);
  vte_terminal_set_pty (self->terminal, pty);

  gtk_widget_remove_css_class (GTK_WIDGET (self->uri_row), "error");
}

void
gbp_vcsui_clone_page_set_uri (GbpVcsuiClonePage *self,
                              const char        *uri)
{
  g_return_if_fail (GBP_IS_VCSUI_CLONE_PAGE (self));

  if (uri == NULL)
    uri = "";

  gtk_editable_set_text (GTK_EDITABLE (self->uri_row), uri);
}
