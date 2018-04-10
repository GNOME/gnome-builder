/* ide-git-clone-widget.c
 *
 * Copyright 2015 Christian Hergert <chergert@redhat.com>
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

#include <dazzle.h>
#include <glib/gi18n.h>
#include <libgit2-glib/ggit.h>
#include <ide.h>

#include "ide-git-clone-widget.h"
#include "ide-git-remote-callbacks.h"

#define ANIMATION_DURATION_MSEC 250

struct _IdeGitCloneWidget
{
  GtkBin                parent_instance;

  gchar                *child_name;

  DzlFileChooserEntry  *clone_location_entry;
  GtkEntry             *clone_uri_entry;
  GtkLabel             *clone_error_label;
  GtkProgressBar       *clone_progress;
  GtkLabel             *destination_label;

  guint                 is_ready : 1;
};

typedef struct
{
  IdeVcsUri *uri;
  GFile     *location;
  GFile     *project_file;
} CloneRequest;

enum {
  PROP_0,
  PROP_IS_READY,
  LAST_PROP
};

G_DEFINE_TYPE (IdeGitCloneWidget, ide_git_clone_widget, GTK_TYPE_BIN)

static void
clone_request_free (gpointer data)
{
  CloneRequest *req = data;

  if (req != NULL)
    {
      g_clear_pointer (&req->uri, ide_vcs_uri_unref);
      g_clear_object (&req->location);
      g_clear_object (&req->project_file);
      g_slice_free (CloneRequest, req);
    }
}

static CloneRequest *
clone_request_new (IdeVcsUri *uri,
                   GFile     *location)
{
  CloneRequest *req;

  g_assert (uri);
  g_assert (location);

  req = g_slice_new0 (CloneRequest);
  req->uri = ide_vcs_uri_ref (uri);
  req->location = g_object_ref (location);
  req->project_file = NULL;

  return req;
}

static void
ide_git_clone_widget_uri_changed (IdeGitCloneWidget *self,
                                  GtkEntry          *entry)
{
  g_autoptr(IdeVcsUri) uri = NULL;
  g_autoptr(GString) str = NULL;
  const gchar *text;
  gboolean is_ready = FALSE;
  gboolean matches = TRUE;

  g_assert (IDE_IS_GIT_CLONE_WIDGET (self));
  g_assert (GTK_IS_ENTRY (entry));

  g_clear_pointer (&self->child_name, g_free);

  text = gtk_entry_get_text (entry);
  str = g_string_new (NULL);

  for (const gchar *ptr = text; *ptr; ptr = g_utf8_next_char (ptr))
    {
      gunichar ch = g_utf8_get_char (ptr);

      if (!g_unichar_isspace (ch))
        {
          g_string_append_unichar (str, ch);
          matches = FALSE;
        }
    }

  if (!matches)
    {
      g_signal_handlers_block_by_func (entry, G_CALLBACK (ide_git_clone_widget_uri_changed), self);
      text = str->str;
      gtk_entry_set_text (entry, text);
      g_signal_handlers_unblock_by_func (entry, G_CALLBACK (ide_git_clone_widget_uri_changed), self);
    }

  uri = ide_vcs_uri_new (text);

  if (uri != NULL)
    {
      const gchar *path;

      g_object_set (self->clone_uri_entry,
                    "secondary-icon-tooltip-text", "",
                    "secondary-icon-name", NULL,
                    NULL);

      path = ide_vcs_uri_get_path (uri);

      if (!dzl_str_empty0 (path))
        {
          g_autofree gchar *name = g_path_get_basename (path);

          if (g_str_has_suffix (name, ".git"))
            *(strrchr (name, '.')) = '\0';

          if (!g_str_equal (name, "/") && !g_str_equal (name, "~"))
            self->child_name = g_steal_pointer (&name);

          is_ready = TRUE;
        }
    }

  if (!is_ready)
    {
      g_object_set (self->clone_uri_entry,
                    "secondary-icon-name", "dialog-warning-symbolic",
                    "secondary-icon-tooltip-text", _("A valid Git URL is required"),
                    NULL);
    }

  if (self->child_name != NULL)
    {
      g_autofree gchar *formatted = NULL;
      g_autoptr(GFile) file = dzl_file_chooser_entry_get_file (self->clone_location_entry);
      g_autoptr(GFile) child = g_file_get_child (file, self->child_name);
      g_autofree gchar *path = g_file_get_path (child);
      g_autofree gchar *collapsed = ide_path_collapse (path);

      /* translators: %s is replaced with the path to the project */
      formatted = g_strdup_printf (_("Your project will be created at %s"), collapsed);
      gtk_label_set_label (self->destination_label, formatted);
    }
  else
    gtk_label_set_label (self->destination_label, NULL);

  if (is_ready != self->is_ready)
    {
      self->is_ready = is_ready;
      g_object_notify (G_OBJECT (self), "is-ready");
    }
}

static void
ide_git_clone_widget_finalize (GObject *object)
{
  IdeGitCloneWidget *self = (IdeGitCloneWidget *)object;

  g_clear_pointer (&self->child_name, g_free);

  G_OBJECT_CLASS (ide_git_clone_widget_parent_class)->finalize (object);
}

static void
ide_git_clone_widget_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  IdeGitCloneWidget *self = IDE_GIT_CLONE_WIDGET(object);

  switch (prop_id)
    {
    case PROP_IS_READY:
      g_value_set_boolean (value, self->is_ready);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
ide_git_clone_widget_class_init (IdeGitCloneWidgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ide_git_clone_widget_finalize;
  object_class->get_property = ide_git_clone_widget_get_property;

  g_object_class_install_property (object_class,
                                   PROP_IS_READY,
                                   g_param_spec_boolean ("is-ready",
                                                         "Is Ready",
                                                         "If the widget is ready to continue.",
                                                         FALSE,
                                                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

  gtk_widget_class_set_css_name (widget_class, "gitclonewidget");
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/git-plugin/ide-git-clone-widget.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeGitCloneWidget, clone_error_label);
  gtk_widget_class_bind_template_child (widget_class, IdeGitCloneWidget, clone_location_entry);
  gtk_widget_class_bind_template_child (widget_class, IdeGitCloneWidget, clone_progress);
  gtk_widget_class_bind_template_child (widget_class, IdeGitCloneWidget, clone_uri_entry);
  gtk_widget_class_bind_template_child (widget_class, IdeGitCloneWidget, destination_label);
}

static void
ide_git_clone_widget_init (IdeGitCloneWidget *self)
{
  g_autoptr(GFile) projects_dir = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  projects_dir = ide_application_get_projects_directory (IDE_APPLICATION_DEFAULT);
  g_assert (G_IS_FILE (projects_dir));

  dzl_file_chooser_entry_set_file (self->clone_location_entry, projects_dir);

  g_signal_connect_object (self->clone_uri_entry,
                           "changed",
                           G_CALLBACK (ide_git_clone_widget_uri_changed),
                           self,
                           G_CONNECT_SWAPPED);
}

static gboolean
open_after_timeout (gpointer user_data)
{
  IdeTask *task = user_data;
  IdeGitCloneWidget *self;
  IdeWorkbench *workbench;
  g_autoptr(GError) error = NULL;
  CloneRequest *req;

  IDE_ENTRY;

  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  req = ide_task_get_task_data (task);
  workbench = ide_widget_get_workbench (GTK_WIDGET (self));

  g_assert (req != NULL);
  g_assert (IDE_IS_GIT_CLONE_WIDGET (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  if (error)
    {
      g_warning ("%s", error->message);
      gtk_label_set_label (self->clone_error_label, error->message);
      gtk_widget_show (GTK_WIDGET (self->clone_error_label));
    }
  else
    {
      ide_workbench_open_project_async (workbench, req->project_file, NULL, NULL, NULL);
    }

  IDE_RETURN (G_SOURCE_REMOVE);
}

static gboolean
finish_animation_in_idle (gpointer data)
{
  IdeTask *task = data;
  IdeGitCloneWidget *self;

  IDE_ENTRY;

  g_assert (IDE_IS_TASK (task));
  self = ide_task_get_source_object (task);
  g_assert (IDE_IS_GIT_CLONE_WIDGET (self));

  dzl_object_animate_full (self->clone_progress,
                           DZL_ANIMATION_EASE_IN_OUT_QUAD,
                           ANIMATION_DURATION_MSEC,
                           NULL,
                           (GDestroyNotify) dzl_gtk_widget_hide_with_fade,
                           self->clone_progress,
                           "fraction", 1.0,
                           NULL);

  /*
   * Wait for a second so animations can complete before opening
   * the project. Otherwise, it's pretty jarring to the user.
   */
  g_timeout_add_full (G_PRIORITY_LOW,
                      ANIMATION_DURATION_MSEC,
                      open_after_timeout,
                      g_object_ref (task),
                      g_object_unref);

  IDE_RETURN (G_SOURCE_REMOVE);
}

static void
ide_git_clone_widget_worker (IdeTask      *task,
                             gpointer      source_object,
                             gpointer      task_data,
                             GCancellable *cancellable)
{
  g_autofree gchar *uristr = NULL;
  IdeGitCloneWidget *self = source_object;
  g_autoptr(GError) error = NULL;
  GgitRepository *repository;
  GgitCloneOptions *clone_options;
  GgitFetchOptions *fetch_options;
  GgitRemoteCallbacks *callbacks;
  CloneRequest *req = task_data;
  IdeProgress *progress;

  g_assert (IDE_IS_TASK (task));
  g_assert (IDE_IS_GIT_CLONE_WIDGET (self));
  g_assert (req != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  callbacks = g_object_new (IDE_TYPE_GIT_REMOTE_CALLBACKS, NULL);
  progress = ide_git_remote_callbacks_get_progress (IDE_GIT_REMOTE_CALLBACKS (callbacks));
  g_object_bind_property (progress, "fraction", self->clone_progress, "fraction", 0);
  g_signal_connect_object (cancellable,
                           "cancelled",
                           G_CALLBACK (ide_git_remote_callbacks_cancel),
                           callbacks,
                           G_CONNECT_SWAPPED);

  fetch_options = ggit_fetch_options_new ();
  ggit_fetch_options_set_remote_callbacks (fetch_options, callbacks);

  clone_options = ggit_clone_options_new ();
  ggit_clone_options_set_is_bare (clone_options, FALSE);
  ggit_clone_options_set_checkout_branch (clone_options, "master");
  ggit_clone_options_set_fetch_options (clone_options, fetch_options);
  g_clear_pointer (&fetch_options, ggit_fetch_options_free);

  uristr = ide_vcs_uri_to_string (req->uri);

  repository = ggit_repository_clone (uristr, req->location, clone_options, &error);

  g_clear_object (&callbacks);
  g_clear_object (&clone_options);

  if (repository == NULL)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if (ide_task_return_error_if_cancelled (task))
    return;

  req->project_file = ggit_repository_get_workdir (repository);
  gdk_threads_add_idle_full (G_PRIORITY_LOW,
                             finish_animation_in_idle,
                             g_object_ref (task),
                             g_object_unref);

  /* We must complete the task in this worker, as IdeTask does not support
   * completing the task asynchronously after work completes. Another option
   * would be to fix things to use mutliple async steps.
   */
  ide_task_return_boolean (task, TRUE);

  g_clear_object (&repository);
}

void
ide_git_clone_widget_clone_async (IdeGitCloneWidget   *self,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GFile) location = NULL;
  g_autoptr(IdeVcsUri) uri = NULL;
  g_autofree gchar *uristr = NULL;
  CloneRequest *req;

  g_return_if_fail (IDE_IS_GIT_CLONE_WIDGET (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_git_clone_widget_clone_async);

  /*
   * ggit_repository_clone() will block and we don't have a good way to
   * cancel it. So we need to return immediately (even though the clone
   * will continue in the background for now).
   *
   * FIXME: Find Ggit API to cancel clone. We might need access to the
   *    GgitRemote so we can ggit_remote_disconnect().
   */
  ide_task_set_return_on_cancel (task, TRUE);

  gtk_label_set_label (self->clone_error_label, NULL);

  uristr = g_strstrip (g_strdup (gtk_entry_get_text (self->clone_uri_entry)));
  location = dzl_file_chooser_entry_get_file (DZL_FILE_CHOOSER_ENTRY (self->clone_location_entry));

  uri = ide_vcs_uri_new (uristr);

  if (uri == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVAL,
                                 _("A valid Git URL is required"));
      return;
    }

  if (self->child_name)
    {
      g_autoptr(GFile) child = g_file_get_child (location, self->child_name);

      req = clone_request_new (uri, child);
    }
  else
    {
      req = clone_request_new (uri, location);
    }

  gtk_widget_set_sensitive (GTK_WIDGET (self->clone_location_entry), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->clone_uri_entry), FALSE);

  gtk_progress_bar_set_fraction (self->clone_progress, 0.0);
  gtk_widget_show (GTK_WIDGET (self->clone_progress));

  ide_task_set_task_data (task, req, clone_request_free);
  ide_task_set_release_on_propagate (task, FALSE);
  ide_task_run_in_thread (task, ide_git_clone_widget_worker);
}

gboolean
ide_git_clone_widget_clone_finish (IdeGitCloneWidget  *self,
                                   GAsyncResult       *result,
                                   GError            **error)
{
  g_autoptr(GError) local_error = NULL;
  gboolean ret;

  g_return_val_if_fail (IDE_IS_GIT_CLONE_WIDGET (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  ret = ide_task_propagate_boolean (IDE_TASK (result), &local_error);

  /* Only hide progress if we were cancelled */
  if (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    gtk_widget_hide (GTK_WIDGET (self->clone_progress));

  gtk_widget_set_sensitive (GTK_WIDGET (self->clone_location_entry), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->clone_uri_entry), TRUE);

  if (local_error != NULL)
    g_propagate_error (error, g_steal_pointer (&local_error));

  return ret;
}

void
ide_git_clone_widget_set_uri (IdeGitCloneWidget *self,
                              IdeVcsUri         *uri)
{
  g_autofree gchar *str = NULL;

  g_return_if_fail (IDE_IS_GIT_CLONE_WIDGET (self));
  g_return_if_fail (uri != NULL);

  str = ide_vcs_uri_to_string (uri);
  gtk_entry_set_text (self->clone_uri_entry, str);
}
