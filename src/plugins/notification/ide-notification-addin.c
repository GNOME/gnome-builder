/* gbp-notification-addin.c
 *
 * Copyright 2017 Lucie Charvat <luci.charvat@gmail.com>
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

#define G_LOG_DOMAIN "gbp-notification-addin"

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <libide-foundry.h>

#include "ide-notification-addin.h"

#define GRACE_PERIOD_USEC (G_USEC_PER_SEC * 5)

struct _IdeNotificationAddin
{
  IdeObject        parent_instance;
  IdeNotification *notif;
  gchar           *last_msg_body;
  IdePipelinePhase    requested_phase;
  gint64           last_time;
  guint            supress : 1;
};

static void addin_iface_init (IdePipelineAddinInterface *iface);

G_DEFINE_TYPE_WITH_CODE (IdeNotificationAddin,
                         ide_notification_addin,
                         IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_PIPELINE_ADDIN, addin_iface_init))

static gboolean
should_supress_message (IdeNotificationAddin *self,
                        const gchar          *message)
{
  g_assert (IDE_IS_NOTIFICATION_ADDIN (self));
  g_assert (message != NULL);

  if (self->last_msg_body == NULL ||
      !dzl_str_equal0 (self->last_msg_body, message) ||
      self->last_time + GRACE_PERIOD_USEC < g_get_monotonic_time ())
    {
      g_free (self->last_msg_body);
      self->last_msg_body = g_strdup (message);
      self->last_time = g_get_monotonic_time ();
      return FALSE;
    }

  return TRUE;
}

static void
ide_notification_addin_notify (IdeNotificationAddin *self,
                               gboolean              success)
{
  g_autofree gchar *msg_body = NULL;
  g_autofree gchar *project_name = NULL;
  g_autofree gchar *id = NULL;
  g_autoptr(GNotification) notification = NULL;
  g_autoptr(GIcon) icon = NULL;
  GtkApplication *app;
  const gchar *msg_title;
  IdeContext *context;
  GtkWindow *window;

  g_assert (IDE_IS_NOTIFICATION_ADDIN (self));

  if (self->supress)
    return;

  app = GTK_APPLICATION (g_application_get_default ());

  if (!(window = gtk_application_get_active_window (app)))
    return;

  if (gtk_window_is_active (window))
    return;

  context = ide_object_get_context (IDE_OBJECT (self));
  project_name = ide_context_dup_title (context);
  id = ide_context_dup_project_id (context);

  if (success)
    {
      msg_title = _("Build successful");
      msg_body = g_strdup_printf (_("Project “%s” has completed building"), project_name);
    }
  else
    {
      msg_title = _("Build failed");
      msg_body = g_strdup_printf (_("Project “%s” failed to build"), project_name);
    }

  icon = g_themed_icon_new ("org.gnome.Builder");

  notification = g_notification_new (msg_title);
  g_notification_set_body (notification, msg_body);
  g_notification_set_priority (notification, G_NOTIFICATION_PRIORITY_NORMAL);
  g_notification_set_icon (notification, icon);

  if (!should_supress_message (self, msg_body))
    g_application_send_notification (g_application_get_default (), id, notification);
}

static void
ide_notification_addin_build_started (IdeNotificationAddin *self,
                                      IdePipeline     *pipeline,
                                      IdeBuildManager      *build_manager)
{
  IdePipelinePhase phase;

  g_assert (IDE_IS_NOTIFICATION_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  if (self->notif != NULL)
    {
      ide_notification_withdraw (self->notif);
      g_clear_object (&self->notif);
    }

  /* We don't care about any build that is advancing to a phase
   * before the BUILD phase. We advanced to CONFIGURE a lot when
   * extracting build flags.
   */

  phase = ide_pipeline_get_requested_phase (pipeline);
  g_assert ((phase & IDE_PIPELINE_PHASE_MASK) == phase);

  self->requested_phase = phase;
  self->supress = phase < IDE_PIPELINE_PHASE_BUILD;

  if (self->requested_phase)
    {
      self->notif = ide_notification_new ();
      g_object_bind_property (pipeline, "message", self->notif, "title", G_BINDING_SYNC_CREATE);
      ide_notification_attach (self->notif, IDE_OBJECT (self));
    }
}

static void
ide_notification_addin_build_failed (IdeNotificationAddin *self,
                                     IdePipeline     *pipeline,
                                     IdeBuildManager      *build_manager)
{
  g_assert (IDE_IS_NOTIFICATION_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  if (self->notif)
    {
      ide_notification_set_icon_name (self->notif, "dialog-error-symbolic");
      ide_notification_set_title (self->notif, _("Build failed"));
    }

  ide_notification_addin_notify (self, FALSE);
}

static void
ide_notification_addin_build_finished (IdeNotificationAddin *self,
                                       IdePipeline     *pipeline,
                                       IdeBuildManager      *build_manager)
{
  g_assert (IDE_IS_NOTIFICATION_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  if (self->notif)
    {
      ide_notification_set_icon_name (self->notif, "emblem-ok-symbolic");

      if (self->requested_phase & IDE_PIPELINE_PHASE_BUILD)
        ide_notification_set_title (self->notif, _("Build succeeded"));
      else if (self->requested_phase & IDE_PIPELINE_PHASE_CONFIGURE)
        ide_notification_set_title (self->notif, _("Build configured"));
      else if (self->requested_phase & IDE_PIPELINE_PHASE_AUTOGEN)
        ide_notification_set_title (self->notif, _("Build bootstrapped"));
    }

  ide_notification_addin_notify (self, TRUE);
}

static void
ide_notification_addin_load (IdePipelineAddin *addin,
                             IdePipeline      *pipeline)
{
  IdeNotificationAddin *self = (IdeNotificationAddin *)addin;
  IdeBuildManager *build_manager;
  IdeContext *context;

  g_assert (IDE_IS_NOTIFICATION_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  context = ide_object_get_context (IDE_OBJECT (addin));
  g_assert (IDE_IS_CONTEXT (context));

  build_manager = ide_build_manager_from_context (context);
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  g_signal_connect_object (build_manager,
                           "build-started",
                           G_CALLBACK (ide_notification_addin_build_started),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (build_manager,
                           "build-finished",
                           G_CALLBACK (ide_notification_addin_build_finished),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (build_manager,
                           "build-failed",
                           G_CALLBACK (ide_notification_addin_build_failed),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
ide_notification_addin_unload (IdePipelineAddin *addin,
                               IdePipeline      *pipeline)
{
  IdeNotificationAddin *self = (IdeNotificationAddin *)addin;

  g_assert (IDE_IS_NOTIFICATION_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  if (self->notif != NULL)
    {
      ide_notification_withdraw (self->notif);
      g_clear_object (&self->notif);
    }

  g_clear_pointer (&self->last_msg_body, g_free);
}

static void
ide_notification_addin_class_init (IdeNotificationAddinClass *klass)
{
}

static void
ide_notification_addin_init (IdeNotificationAddin *self)
{
}

static void
addin_iface_init (IdePipelineAddinInterface *iface)
{
  iface->load = ide_notification_addin_load;
  iface->unload = ide_notification_addin_unload;
}
