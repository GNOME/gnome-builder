/* gbp-notification-addin.c
 *
 * Copyright © 2017 Lucie Charvat <luci.charvat@gmail.com>
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

#define G_LOG_DOMAIN "gbp-notification-addin"

#include <glib/gi18n.h>
#include <gio/gio.h>

#include "ide-notification-addin.h"

#define GRACE_PERIOD_USEC (G_USEC_PER_SEC * 5)

struct _IdeNotificationAddin
{
  IdeObject  parent_instance;

  gchar     *last_msg_body;
  gint64     last_time;
};

static void addin_iface_init (IdeBuildPipelineAddinInterface *iface);

G_DEFINE_TYPE_WITH_CODE (IdeNotificationAddin,
                         ide_notification_addin,
                         IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_PIPELINE_ADDIN, addin_iface_init))

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
  g_autoptr(GNotification) notification = NULL;
  g_autoptr(GIcon) icon = NULL;
  GtkApplication *app;
  const gchar *project_name;
  const gchar *msg_title;
  const gchar *id;
  IdeContext *context;
  IdeProject *project;
  GtkWindow *window;

  g_assert (IDE_IS_NOTIFICATION_ADDIN (self));

  app = GTK_APPLICATION (g_application_get_default ());

  if (!(window = gtk_application_get_active_window (app)))
    return;

  if (gtk_window_is_active (window))
    return;

  context = ide_object_get_context (IDE_OBJECT (self));
  project = ide_context_get_project (context);
  project_name = ide_project_get_name (project);
  id = ide_project_get_id (project);

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
ide_notification_addin_build_failed (IdeNotificationAddin *self,
                                     IdeBuildPipeline     *build_pipeline,
                                     IdeBuildManager      *build_manager)
{
  g_assert (IDE_IS_NOTIFICATION_ADDIN (self));
  g_assert (IDE_IS_BUILD_PIPELINE (build_pipeline));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  ide_notification_addin_notify (self, FALSE);
}

static void
ide_notification_addin_build_finished (IdeNotificationAddin *self,
                                       IdeBuildPipeline     *build_pipeline,
                                       IdeBuildManager      *build_manager)
{
  IdeBuildPhase phase;

  g_assert (IDE_IS_NOTIFICATION_ADDIN (self));
  g_assert (IDE_IS_BUILD_PIPELINE (build_pipeline));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  /* Only notify if we were advancing to a build phase */
  phase = ide_build_pipeline_get_phase (build_pipeline);
  if (phase >= IDE_BUILD_PHASE_BUILD)
    ide_notification_addin_notify (self, TRUE);
}

static void
ide_notification_addin_load (IdeBuildPipelineAddin *addin,
                             IdeBuildPipeline      *pipeline)
{
  IdeNotificationAddin *self = (IdeNotificationAddin *)addin;
  IdeBuildManager *build_manager;
  IdeContext *context;

  g_assert (IDE_IS_NOTIFICATION_ADDIN (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));

  context = ide_object_get_context (IDE_OBJECT (addin));
  g_assert (IDE_IS_CONTEXT (context));

  build_manager = ide_context_get_build_manager (context);
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

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
ide_notification_addin_unload (IdeBuildPipelineAddin *addin,
                               IdeBuildPipeline      *pipeline)
{
  IdeNotificationAddin *self = (IdeNotificationAddin *)addin;

  g_assert (IDE_IS_NOTIFICATION_ADDIN (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));

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
addin_iface_init (IdeBuildPipelineAddinInterface *iface)
{
  iface->load = ide_notification_addin_load;
  iface->unload = ide_notification_addin_unload;
}
