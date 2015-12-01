/* ide-git-clone-widget.c
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
#include <libgit2-glib/ggit.h>
#include <ide.h>

#include "egg-animation.h"

#include "ide-git-clone-widget.h"
#include "ide-git-remote-callbacks.h"

#define ANIMATION_DURATION_MSEC 250

struct _IdeGitCloneWidget
{
  GtkBin                parent_instance;

  GtkFileChooserButton *clone_location_button;
  GtkEntry             *clone_location_entry;
  GtkEntry             *clone_uri_entry;
  GtkButton            *clone_button;
  GtkLabel             *clone_error_label;
  GtkProgressBar       *clone_progress;
  GtkSpinner           *clone_spinner;
};

typedef struct
{
  gchar *uri;
  GFile *location;
} CloneRequest;

G_DEFINE_TYPE (IdeGitCloneWidget, ide_git_clone_widget, GTK_TYPE_BIN)

static void ide_git_clone_widget_begin_clone (IdeGitCloneWidget *self);

static void
clone_request_free (gpointer data)
{
  CloneRequest *req = data;

  if (req != NULL)
    {
      g_clear_pointer (&req->uri, g_free);
      g_clear_object (&req->location);
      g_slice_free (CloneRequest, req);
    }
}

static CloneRequest *
clone_request_new (const gchar *uri,
                   GFile       *location)
{
  CloneRequest *req;

  g_assert (uri);
  g_assert (location);

  req = g_slice_new0 (CloneRequest);
  req->uri = g_strdup (uri);
  req->location = g_object_ref (location);

  return req;
}

static void
ide_git_clone_widget_uri_changed (IdeGitCloneWidget *self,
                                  GtkEntry          *entry)
{
  g_autoptr(IdeVcsUri) uri = NULL;
  const gchar *text;

  g_assert (IDE_IS_GIT_CLONE_WIDGET (self));
  g_assert (GTK_IS_ENTRY (entry));

  text = gtk_entry_get_text (entry);
  uri = ide_vcs_uri_new (text);

  if (uri != NULL)
    {
      const gchar *path;
      gchar *name = NULL;

      g_object_set (self->clone_uri_entry,
                    "secondary-icon-name", NULL,
                    "secondary-icon-tooltip-text", NULL,
                    NULL);

      path = ide_vcs_uri_get_path (uri);

      if (path != NULL)
        {
          name = g_path_get_basename (path);
          if (g_str_has_suffix (name, ".git"))
            *(strrchr (name, '.')) = '\0';
          if (!g_str_equal (name, "/"))
            gtk_entry_set_text (self->clone_location_entry, name);
          g_free (name);
        }
    }
  else
    {
      g_object_set (self->clone_uri_entry,
                    "secondary-icon-name", "dialog-warning-symbolic",
                    "secondary-icon-tooltip-text", _("A valid Git URL is required"),
                    NULL);
    }
}

static void
ide_git_clone_widget_clone_button_clicked (IdeGitCloneWidget *self,
                                           GtkButton         *button)
{
  g_assert (IDE_IS_GIT_CLONE_WIDGET (self));
  g_assert (GTK_IS_BUTTON (button));

  ide_git_clone_widget_begin_clone (self);
}

static void
ide_git_clone_widget_finalize (GObject *object)
{
  G_OBJECT_CLASS (ide_git_clone_widget_parent_class)->finalize (object);
}

static void
ide_git_clone_widget_class_init (IdeGitCloneWidgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ide_git_clone_widget_finalize;

  gtk_widget_class_set_css_name (widget_class, "gitclonewidget");
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/git/ide-git-clone-widget.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeGitCloneWidget, clone_button);
  gtk_widget_class_bind_template_child (widget_class, IdeGitCloneWidget, clone_error_label);
  gtk_widget_class_bind_template_child (widget_class, IdeGitCloneWidget, clone_location_button);
  gtk_widget_class_bind_template_child (widget_class, IdeGitCloneWidget, clone_location_entry);
  gtk_widget_class_bind_template_child (widget_class, IdeGitCloneWidget, clone_progress);
  gtk_widget_class_bind_template_child (widget_class, IdeGitCloneWidget, clone_spinner);
  gtk_widget_class_bind_template_child (widget_class, IdeGitCloneWidget, clone_uri_entry);
}

static void
ide_git_clone_widget_init (IdeGitCloneWidget *self)
{
  gchar *projects_dir;

  gtk_widget_init_template (GTK_WIDGET (self));

  projects_dir = g_build_filename (g_get_home_dir (), _("Projects"), NULL);
  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (self->clone_location_button),
                                       projects_dir);
  g_free (projects_dir);

  g_signal_connect_object (self->clone_uri_entry,
                           "changed",
                           G_CALLBACK (ide_git_clone_widget_uri_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->clone_button,
                           "clicked",
                           G_CALLBACK (ide_git_clone_widget_clone_button_clicked),
                           self,
                           G_CONNECT_SWAPPED);
}

static gboolean
open_after_timeout (gpointer user_data)
{
  IdeGitCloneWidget *self;
  IdeWorkbench *workbench;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  workbench = ide_widget_get_workbench (GTK_WIDGET (self));

  g_assert (IDE_IS_GIT_CLONE_WIDGET (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  gtk_widget_hide (GTK_WIDGET (self->clone_spinner));

  file = g_task_propagate_pointer (task, &error);

  if (error)
    {
      g_warning ("%s", error->message);
      gtk_label_set_label (self->clone_error_label, error->message);
      gtk_widget_show (GTK_WIDGET (self->clone_error_label));
    }
  else
    {
      ide_workbench_open_project_async (workbench, file, NULL, NULL, NULL);
    }

  return G_SOURCE_REMOVE;
}

static void
ide_git_clone_widget_clone_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  IdeGitCloneWidget *self = (IdeGitCloneWidget *)object;
  GTask *task = (GTask *)result;

  g_assert (IDE_IS_GIT_CLONE_WIDGET (self));
  g_assert (G_IS_TASK (task));

  egg_object_animate_full (self->clone_progress,
                           EGG_ANIMATION_EASE_IN_OUT_QUAD,
                           ANIMATION_DURATION_MSEC,
                           NULL,
                           (GDestroyNotify)ide_widget_hide_with_fade,
                           self->clone_progress,
                           "fraction", 1.0,
                           NULL);

  /*
   * Wait for a second so animations can complete before opening
   * the project. Otherwise, it's pretty jarring to the user.
   */
  g_timeout_add (ANIMATION_DURATION_MSEC, open_after_timeout, g_object_ref (task));
}

static void
ide_git_clone_widget_worker (GTask        *task,
                             gpointer      source_object,
                             gpointer      task_data,
                             GCancellable *cancellable)
{
  IdeGitCloneWidget *self = source_object;
  GgitRepository *repository;
  g_autoptr(GFile) workdir = NULL;
  CloneRequest *req = task_data;
  GgitCloneOptions *clone_options;
  GgitFetchOptions *fetch_options;
  GgitRemoteCallbacks *callbacks;
  IdeProgress *progress;
  GError *error = NULL;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_GIT_CLONE_WIDGET (self));
  g_assert (req != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  callbacks = g_object_new (IDE_TYPE_GIT_REMOTE_CALLBACKS, NULL);
  progress = ide_git_remote_callbacks_get_progress (IDE_GIT_REMOTE_CALLBACKS (callbacks));
  g_object_bind_property (progress, "fraction", self->clone_progress, "fraction", 0);

  fetch_options = ggit_fetch_options_new ();
  ggit_fetch_options_set_remote_callbacks (fetch_options, callbacks);

  clone_options = ggit_clone_options_new ();
  ggit_clone_options_set_is_bare (clone_options, FALSE);
  ggit_clone_options_set_checkout_branch (clone_options, "master");
  ggit_clone_options_set_fetch_options (clone_options, fetch_options);
  g_clear_pointer (&fetch_options, ggit_fetch_options_free);

  repository = ggit_repository_clone (req->uri, req->location, clone_options, &error);

  g_clear_object (&callbacks);
  g_clear_object (&clone_options);

  if (repository == NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  workdir = ggit_repository_get_workdir (repository);
  g_task_return_pointer (task, g_object_ref (workdir), g_object_unref);

  g_clear_object (&repository);
}

static void
ide_git_clone_widget_begin_clone (IdeGitCloneWidget *self)
{
  g_autoptr(GTask) task = NULL;
  g_autoptr(GFile) location = NULL;
  g_autoptr(GFile) child = NULL;
  CloneRequest *req;
  const gchar *uri;
  const gchar *child_name;

  g_assert (IDE_IS_GIT_CLONE_WIDGET (self));

  gtk_widget_set_sensitive (GTK_WIDGET (self->clone_button), FALSE);
  gtk_label_set_label (self->clone_error_label, NULL);
  gtk_widget_show (GTK_WIDGET (self->clone_spinner));

  uri = gtk_entry_get_text (self->clone_uri_entry);
  child_name = gtk_entry_get_text (self->clone_location_entry);
  location = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (self->clone_location_button));

  if (child_name != NULL)
    {
      child = g_file_get_child (location, child_name);
      req = clone_request_new (uri, child);
    }
  else
    {
      req = clone_request_new (uri, location);
    }

  task = g_task_new (self, NULL, ide_git_clone_widget_clone_cb, self);
  g_task_set_task_data (task, req, clone_request_free);
  g_task_run_in_thread (task, ide_git_clone_widget_worker);
}
