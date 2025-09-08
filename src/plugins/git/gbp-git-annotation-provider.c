/*
 * gbp-git-annotation-provider.c
 *
 * Copyright 2025 Nokse <nokse@posteo.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#define G_LOG_DOMAIN "gbp-git-annotation-provider"

#include "config.h"

#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>
#include <libide-code.h>
#include <libide-gui.h>
#include <libide-vcs.h>

#include "daemon/ipc-git-service.h"
#include "daemon/ipc-git-repository.h"
#include "daemon/ipc-git-blame.h"

#include "gbp-git-vcs.h"
#include "gbp-git-annotation-provider.h"

struct _GbpGitAnnotationProvider
{
  GtkSourceAnnotationProvider parent_instance;

  IdeBuffer                  *buffer;

  GCancellable               *blame_cancellable;
  GCancellable               *update_cancellable;
  IpcGitBlame                *blame_service;

  guint                       last_line;
  char                       *commit_hash;
  char                       *short_hash;
  char                       *author_name;
  char                       *author_email;
  char                       *commit_message;
  char                       *commit_date;
  char                       *natural_time;
  char                       *precise_date;

  guint                       content_updated : 1;
};

G_DEFINE_TYPE (GbpGitAnnotationProvider, gbp_git_annotation_provider, GTK_SOURCE_TYPE_ANNOTATION_PROVIDER)

static void
gbp_git_annotation_provider_add_annotation (GbpGitAnnotationProvider *self,
                                            const char               *blame_text)
{
  GtkSourceAnnotation *annotation;

  IDE_ENTRY;

  annotation = gtk_source_annotation_new (g_strdup (blame_text),
                                          g_icon_new_for_string ("commit-symbolic", NULL),
                                          self->last_line,
                                          GTK_SOURCE_ANNOTATION_STYLE_NONE);

  gtk_source_annotation_provider_remove_all (GTK_SOURCE_ANNOTATION_PROVIDER (self));
  gtk_source_annotation_provider_add_annotation (GTK_SOURCE_ANNOTATION_PROVIDER (self), annotation);

  IDE_EXIT;
}

static char *
format_relative_time (char *time_past_str)
{
  g_autoptr (GDateTime) time_now = NULL;
  g_autoptr (GDateTime) time_past = NULL;
  g_autoptr (GTimeZone) local_timezone = NULL;
  GTimeSpan time_diff;
  int64_t seconds, minutes, hours, days, weeks, months, years;
  char *result = NULL;

  IDE_ENTRY;

  local_timezone = g_time_zone_new_local ();
  time_past = g_date_time_new_from_iso8601 (time_past_str, local_timezone);
  time_now = g_date_time_new_now_local ();
  time_diff = g_date_time_difference (time_now, time_past);

  if (time_diff < 0)
    IDE_RETURN (g_strdup (_("in the future")));

  seconds = time_diff / G_TIME_SPAN_SECOND;
  minutes = seconds / 60;
  hours = minutes / 60;
  days = hours / 24;
  weeks = days / 7;
  months = days / 30;
  years = days / 365;

  if (seconds < 60)
    result = g_strdup(_("just now"));
  else if (minutes < 60)
    result = g_strdup_printf (ngettext("%ld minute ago",
                                       "%ld minutes ago",
                                       (long)minutes),
                              (long)minutes);
  else if (hours < 24)
    result = g_strdup_printf (ngettext("%ld hour ago",
                                       "%ld hours ago",
                                       (long)hours),
                              (long)hours);
  else if (days < 7)
    result = g_strdup_printf (ngettext("%ld day ago",
                                       "%ld days ago",
                                       (long)days),
                              (long)days);
  else if (weeks < 4)
    result = g_strdup_printf (ngettext("%ld week ago",
                                       "%ld weeks ago",
                                       (long)weeks),
                              (long)weeks);
  else if (months < 12)
    result = g_strdup_printf (ngettext("%ld month ago",
                                       "%ld months ago",
                                       (long)months),
                              (long)months);
  else
    result = g_strdup_printf (ngettext("%ld year ago",
                                       "%ld years ago",
                                       (long)years),
                              (long)years);

  IDE_RETURN (result);
}

static char *
format_precise_time (const char *date_string)
{
  g_autoptr (GDateTime) commit_datetime = NULL;
  g_autoptr (GDateTime) local_datetime = NULL;
  char *result = NULL;

  IDE_ENTRY;

  if (!date_string || !*date_string)
    IDE_RETURN (NULL);

  commit_datetime = g_date_time_new_from_iso8601 (date_string, NULL);
  if (!commit_datetime)
    IDE_RETURN (NULL);

  local_datetime = g_date_time_to_local (commit_datetime);
  if (!local_datetime)
    IDE_RETURN (NULL);

  result = g_date_time_format (local_datetime, "%c");

  IDE_RETURN (result);
}

static void
gbp_git_annotation_provider_query_line_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  IpcGitBlame *blame_service = (IpcGitBlame *)object;
  GbpGitAnnotationProvider *self = user_data;
  g_autoptr(GError) error = NULL;
  g_autofree char *commit_hash = NULL;
  g_autofree char *author_name = NULL;
  g_autofree char *author_email = NULL;
  g_autofree char *commit_message = NULL;
  g_autofree char *commit_date = NULL;
  gboolean is_valid_commit = FALSE;
  guint line_in_commit = 0;

  IDE_ENTRY;

  g_assert (IPC_IS_GIT_BLAME (blame_service));
  g_assert (GBP_IS_GIT_ANNOTATION_PROVIDER (self));

  if (!ipc_git_blame_call_query_line_finish (blame_service,
                                             &commit_hash,
                                             &author_name,
                                             &author_email,
                                             &commit_message,
                                             &commit_date,
                                             &line_in_commit,
                                             result,
                                             &error))
    {
      g_debug ("Error while querying line blame: %s", error->message);
      IDE_EXIT;
    }

  is_valid_commit = (commit_hash != NULL &&
                     *commit_hash != '\0' &&
                     author_name != NULL &&
                     *author_name != '\0');

  if (is_valid_commit)
    {
      g_autofree char *blame_text = NULL;

      g_strchomp(commit_message);

      self->author_name = g_strdup (author_name);
      self->author_email = g_strdup (author_email);
      self->commit_message = g_strdup (commit_message);
      self->commit_date = g_strdup (commit_date);
      self->commit_hash = g_strdup (commit_hash);
      self->short_hash = g_strndup (commit_hash, 8);

      self->precise_date = format_precise_time (self->commit_date);
      self->natural_time = format_relative_time (self->commit_date);

      blame_text = g_strdup_printf (_("%s, %s"),
                                    self->author_name,
                                    self->natural_time);

      gbp_git_annotation_provider_add_annotation (self, blame_text);
    }

  IDE_EXIT;
}

static void
gbp_git_annotation_provider_query_line (GbpGitAnnotationProvider *self)
{
  IDE_ENTRY;

  g_assert (GBP_IS_GIT_ANNOTATION_PROVIDER (self));

  if (self->blame_service == NULL)
    IDE_EXIT;

  if (self->blame_cancellable)
    g_cancellable_cancel (self->blame_cancellable);

  g_clear_pointer (&self->author_name, g_free);
  g_clear_pointer (&self->author_email, g_free);
  g_clear_pointer (&self->commit_message, g_free);
  g_clear_pointer (&self->commit_date, g_free);
  g_clear_pointer (&self->commit_hash, g_free);
  g_clear_pointer (&self->short_hash, g_free);
  g_clear_pointer (&self->natural_time, g_free);
  g_clear_pointer (&self->precise_date, g_free);

  self->blame_cancellable = g_cancellable_new ();

  ipc_git_blame_call_query_line (self->blame_service,
                                 self->last_line,
                                 self->blame_cancellable,
                                 gbp_git_annotation_provider_query_line_cb,
                                 self);

  IDE_EXIT;
}

static void
gbp_git_annotation_provider_update_content (GbpGitAnnotationProvider *self,
                                            IdeBuffer                *buffer)
{
  g_autoptr(GBytes) bytes = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_GIT_ANNOTATION_PROVIDER (self));
  g_assert (IDE_IS_BUFFER (buffer));

  gtk_source_annotation_provider_remove_all (GTK_SOURCE_ANNOTATION_PROVIDER (self));

  if (self->update_cancellable)
    g_cancellable_cancel (self->update_cancellable);

  self->update_cancellable = g_cancellable_new ();

  if (self->blame_service == NULL)
    IDE_EXIT;

  bytes = ide_buffer_dup_content (buffer);

  ipc_git_blame_call_update_content (self->blame_service,
                                     (const char *)g_bytes_get_data (bytes, NULL),
                                     self->update_cancellable,
                                     NULL, NULL);

  IDE_EXIT;
}

static void
gbp_git_annotation_provider_update_blame_service (GbpGitAnnotationProvider *self)
{
  g_autoptr(IpcGitBlame) proxy = NULL;
  g_autoptr(GError) error = NULL;
  IpcGitRepository *repository;
  g_autofree char *relative_path = NULL;
  g_autofree char *obj_path = NULL;
  GDBusConnection *connection;
  IdeContext *context;
  IdeVcs *vcs;
  IdeWorkbench *workbench;
  GFile *file;

  IDE_ENTRY;

  g_assert (GBP_IS_GIT_ANNOTATION_PROVIDER (self));

  if (self->buffer == NULL)
    IDE_EXIT;

  context = ide_buffer_ref_context (self->buffer);
  file = ide_buffer_get_file (self->buffer);
  relative_path = g_file_get_path (file);

  workbench = ide_workbench_from_context (context);
  if (workbench == NULL)
    IDE_EXIT;

  vcs = ide_workbench_get_vcs (workbench);
  if (!GBP_IS_GIT_VCS (vcs))
    IDE_EXIT;

  repository = gbp_git_vcs_get_repository (GBP_GIT_VCS (vcs));
  if (repository == NULL)
    IDE_EXIT;

  if (!ipc_git_repository_call_blame_sync (repository,
                                           relative_path,
                                           &obj_path,
                                           NULL,
                                           &error))
    IDE_EXIT;

  connection = g_dbus_proxy_get_connection (G_DBUS_PROXY (repository));

  if (!(proxy = ipc_git_blame_proxy_new_sync (connection,
                                              G_DBUS_PROXY_FLAGS_NONE,
                                              NULL,
                                              obj_path,
                                              NULL,
                                              &error)))
    IDE_EXIT;

  g_clear_object (&self->blame_service);
  self->blame_service = g_steal_pointer (&proxy);

  gbp_git_annotation_provider_update_content (self, self->buffer);

  IDE_EXIT;
}

static void
gbp_git_annotation_provider_clear (GbpGitAnnotationProvider *self)
{
  self->last_line = 0;
  g_clear_pointer (&self->author_name, g_free);
  g_clear_pointer (&self->author_email, g_free);
  g_clear_pointer (&self->commit_message, g_free);
  g_clear_pointer (&self->commit_date, g_free);
  g_clear_pointer (&self->commit_hash, g_free);
  g_clear_pointer (&self->short_hash, g_free);
  g_clear_pointer (&self->natural_time, g_free);
  g_clear_pointer (&self->precise_date, g_free);
}

static void
gbp_git_annotation_provider_buffer_cursor_moved_cb (GbpGitAnnotationProvider *self,
                                                    IdeBuffer                *buffer)
{
  GtkTextIter iter;
  guint line_number;

  IDE_ENTRY;

  g_assert (GBP_IS_GIT_ANNOTATION_PROVIDER (self));
  g_assert (IDE_IS_BUFFER (buffer));

  gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer),
                                   &iter,
                                   gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (buffer)));
  line_number = gtk_text_iter_get_line (&iter);

  if (line_number != self->last_line)
    {
      IdeBufferChangeMonitor *monitor = NULL;

      gbp_git_annotation_provider_clear (self);
      gtk_source_annotation_provider_remove_all (GTK_SOURCE_ANNOTATION_PROVIDER (self));

      if ((monitor = ide_buffer_get_change_monitor (buffer)) &&
          ide_buffer_change_monitor_get_change (monitor, line_number - 1) == IDE_BUFFER_LINE_CHANGE_NONE &&
          self->content_updated)
        {
          self->last_line = line_number;
          gbp_git_annotation_provider_query_line (self);
        }
    }

  IDE_EXIT;
}

static void
gbp_git_annotation_provider_buffer_changed_cb (GbpGitAnnotationProvider *self,
                                               IdeBuffer                *buffer)
{
  IDE_ENTRY;

  g_assert (GBP_IS_GIT_ANNOTATION_PROVIDER (self));
  g_assert (IDE_IS_BUFFER (buffer));

  self->content_updated = FALSE;

  IDE_EXIT;
}

static void
gbp_git_annotation_provider_buffer_change_settled_cb (GbpGitAnnotationProvider *self,
                                                      IdeBuffer                *buffer)
{
  IDE_ENTRY;

  g_assert (GBP_IS_GIT_ANNOTATION_PROVIDER (self));
  g_assert (IDE_IS_BUFFER (buffer));

  gbp_git_annotation_provider_update_content (self, buffer);

  self->content_updated = TRUE;

  IDE_EXIT;
}

void
gbp_git_annotation_provider_populate_hover_async (GtkSourceAnnotationProvider  *provider,
                                                  GtkSourceAnnotation          *annotation,
                                                  GtkSourceHoverDisplay        *display,
                                                  GCancellable                 *cancellable,
                                                  GAsyncReadyCallback           callback,
                                                  gpointer                      user_data)
{
  GbpGitAnnotationProvider *self = GBP_GIT_ANNOTATION_PROVIDER (provider);
  g_autoptr(GTask) task = NULL;
  GtkWidget *top_box;
  GtkWidget *bottom_box;

  IDE_ENTRY;

  g_assert (GTK_SOURCE_IS_ANNOTATION_PROVIDER (provider));
  g_assert (GTK_SOURCE_IS_HOVER_DISPLAY (display));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (provider, cancellable, callback, user_data);
  g_task_set_source_tag (task, gbp_git_annotation_provider_populate_hover_async);

  if (self->author_name == NULL || self->commit_message == NULL)
    {
      g_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  top_box = g_object_new (GTK_TYPE_BOX,
                          "orientation", GTK_ORIENTATION_HORIZONTAL,
                          "spacing", 12,
                          "margin-start", 12,
                          "margin-end", 12,
                          "margin-top", 6,
                          "margin-bottom", 6,
                          "hexpand", TRUE,
                          NULL);
  bottom_box = g_object_new (GTK_TYPE_BOX,
                             "orientation", GTK_ORIENTATION_HORIZONTAL,
                             "spacing", 12,
                             "margin-start", 12,
                             "margin-end", 12,
                             "margin-top", 6,
                             "margin-bottom", 6,
                             "hexpand", TRUE,
                          NULL);

  gtk_box_append (GTK_BOX (top_box),
                  g_object_new (GTK_TYPE_LABEL,
                                "label", self->author_name,
                                "css-classes", IDE_STRV_INIT ("heading"),
                                "hexpand", TRUE,
                                "xalign", 0.0,
                                NULL));

  if (self->author_email != NULL)
    gtk_box_append (GTK_BOX (top_box),
                    g_object_new (GTK_TYPE_LABEL,
                                  "label", self->author_email,
                                  "css-classes", IDE_STRV_INIT ("dimmed"),
                                  "hexpand", TRUE,
                                  "xalign", 1.0,
                                  NULL));

  if (self->precise_date != NULL)
    gtk_box_append (GTK_BOX (bottom_box),
                    g_object_new (GTK_TYPE_LABEL,
                                  "label", self->precise_date,
                                  "hexpand", TRUE,
                                  "xalign", 0.0,
                                  NULL));

  if (self->short_hash != NULL)
    gtk_box_append (GTK_BOX (bottom_box),
                    g_object_new (GTK_TYPE_LABEL,
                                  "label", self->short_hash,
                                  "selectable", TRUE,
                                  "css-classes", IDE_STRV_INIT ("monospace", "dimmed"),
                                  "hexpand", TRUE,
                                  "xalign", 1.0,
                                  NULL));

  gtk_source_hover_display_append (display, top_box);
  gtk_source_hover_display_append (display, gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
  gtk_source_hover_display_append (display,
                                   g_object_new (GTK_TYPE_LABEL,
                                                 "label", self->commit_message,
                                                 "margin-start", 12,
                                                 "margin-end", 12,
                                                 "margin-top", 6,
                                                 "margin-bottom", 6,
                                                 "selectable", TRUE,
                                                 "xalign", 0.0,
                                                 "yalign", 0.0,
                                                 NULL));
  gtk_source_hover_display_append (display, gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
  gtk_source_hover_display_append (display, bottom_box);

  g_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

gboolean
gbp_git_annotation_provider_populate_hover_finish (GtkSourceAnnotationProvider  *provider,
                                                   GAsyncResult                 *result,
                                                   GError                      **error)
{
  IDE_ENTRY;

  g_return_val_if_fail (GTK_SOURCE_IS_ANNOTATION_PROVIDER (provider), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  IDE_RETURN (g_task_propagate_boolean (G_TASK (result), error));
}

GbpGitAnnotationProvider *
gbp_git_annotation_provider_new (IdeBuffer *buffer)
{
  GbpGitAnnotationProvider *self;

  IDE_ENTRY;

  g_return_val_if_fail (!buffer || IDE_IS_BUFFER (buffer), NULL);

  self = g_object_new (GBP_TYPE_GIT_ANNOTATION_PROVIDER, NULL);

  if (g_set_object (&self->buffer, buffer))
    {
      g_signal_connect_object (self->buffer,
                               "cursor-moved",
                               G_CALLBACK (gbp_git_annotation_provider_buffer_cursor_moved_cb),
                               self,
                               G_CONNECT_SWAPPED);
      g_signal_connect_object (self->buffer,
                               "changed",
                               G_CALLBACK (gbp_git_annotation_provider_buffer_changed_cb),
                               self,
                               G_CONNECT_SWAPPED);
      g_signal_connect_object (self->buffer,
                               "change-settled",
                               G_CALLBACK (gbp_git_annotation_provider_buffer_change_settled_cb),
                               self,
                               G_CONNECT_SWAPPED);

      if (buffer != NULL)
        gbp_git_annotation_provider_update_blame_service (self);
    }

  IDE_RETURN (self);
}

static void
gbp_git_annotation_provider_dispose (GObject *object)
{
  GbpGitAnnotationProvider *self = GBP_GIT_ANNOTATION_PROVIDER (object);

  if (self->blame_cancellable)
    g_cancellable_cancel (self->blame_cancellable);

  if (self->update_cancellable)
    g_cancellable_cancel (self->update_cancellable);

  g_clear_object (&self->blame_cancellable);
  g_clear_object (&self->update_cancellable);
  g_clear_object (&self->buffer);
  g_clear_object (&self->blame_service);

  gbp_git_annotation_provider_clear (self);

  G_OBJECT_CLASS (gbp_git_annotation_provider_parent_class)->dispose (object);
}

static void
gbp_git_annotation_provider_class_init (GbpGitAnnotationProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkSourceAnnotationProviderClass *provider_class = GTK_SOURCE_ANNOTATION_PROVIDER_CLASS (klass);

  object_class->dispose = gbp_git_annotation_provider_dispose;
  provider_class->populate_hover_async = gbp_git_annotation_provider_populate_hover_async;
  provider_class->populate_hover_finish = gbp_git_annotation_provider_populate_hover_finish;
}

static void
gbp_git_annotation_provider_init (GbpGitAnnotationProvider *self)
{
  self->last_line = 0;
}
