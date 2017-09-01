/* gbp-notification-addin.c
 *
 * Copyright (C) 2017 Lucie Charvat <luci.charvat@gmail.com>
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

#define NOTIFY_TIMEOUT (10000)

struct _IdeNotificationAddin
{
  IdeObject   parent_instance;
  GDBusProxy *proxy;
  guint       notify_id;
};

static void addin_iface_init (IdeBuildPipelineAddinInterface *iface);
static guint last_notify_id;

G_DEFINE_TYPE_EXTENDED (IdeNotificationAddin,
                        ide_notification_addin,
                        IDE_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_PIPELINE_ADDIN,
                                               addin_iface_init))

static void
ide_notification_addin_notify (IdeNotificationAddin *self,
                               gboolean              success)
{
  g_autoptr(GVariant) result = NULL;
  g_autofree gchar *msg_body = NULL;
  GVariantBuilder actions_builder;
  GVariantBuilder hints_builder;
  GtkApplication *app;
  GtkWindow *window;
  IdeContext *context;
  IdeProject *project;
  const gchar *project_name;
  const gchar *msg_title;

  g_assert (IDE_IS_NOTIFICATION_ADDIN (self));

  if (self->proxy == NULL)
    return;

  app = GTK_APPLICATION (g_application_get_default ());
  window = gtk_application_get_active_window (app);
  if(gtk_window_has_toplevel_focus (window))
    return;

  context = ide_object_get_context (IDE_OBJECT (self));
  g_assert (IDE_IS_CONTEXT (context));

  project = ide_context_get_project (context);
  g_assert (IDE_IS_PROJECT (project));

  project_name = ide_project_get_name (project);
  if (project_name == NULL)
    return;

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

  g_variant_builder_init (&actions_builder, G_VARIANT_TYPE ("as"));
  g_variant_builder_init (&hints_builder, G_VARIANT_TYPE ("a{sv}"));

  /*
   * We use self->notify_id so that notifications simply overwrite
   * the previous state. This helps keep things from getting out of
   * hand with lots of notifications for the same project.
   */

  result = g_dbus_proxy_call_sync (self->proxy,
                                   "Notify",
                                   g_variant_new ("(susssasa{sv}i)",
                                                  "org.gnome.Builder", self->notify_id, "",
                                                  msg_title,
                                                  msg_body, &actions_builder,
                                                  &hints_builder, -1),
                                   G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL);

#if 0
   "/data/icons/hicolor/16x16/apps/org.gnome.Builder.png",
   GNotification *notification;
   notification = g_notification_new ("Test");
   g_notification_set_body (notification, "body");
   g_application_send_notification (G_APPLICATION (app), "test1", notification);
   g_object_unref (notification);
#endif
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
  g_assert (IDE_IS_NOTIFICATION_ADDIN (self));
  g_assert (IDE_IS_BUILD_PIPELINE (build_pipeline));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  ide_notification_addin_notify (self, TRUE);
}

static void
ide_notification_addin_load (IdeBuildPipelineAddin *addin,
                             IdeBuildPipeline      *pipeline)
{
  IdeNotificationAddin *self = (IdeNotificationAddin *)addin;
  IdeContext* context;
  IdeBuildManager *build_manager;

  g_assert (IDE_IS_NOTIFICATION_ADDIN (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));

  context = ide_object_get_context (IDE_OBJECT (addin));
  g_assert (IDE_IS_CONTEXT (context));

  build_manager = ide_context_get_build_manager (context);
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  self->proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                               G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                               NULL,
                                               "org.freedesktop.Notifications",
                                               "/org/freedesktop/Notifications",
                                               "org.freedesktop.Notifications",
                                               NULL,
                                               NULL);

  if (self->proxy == NULL)
    {
      g_message ("Failed to locate org.freedesktop.Notifications");
      return;
    }

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

  g_clear_object (&self->proxy);
}
static void
ide_notification_addin_class_init (IdeNotificationAddinClass *klass)
{
}

static void
ide_notification_addin_init (IdeNotificationAddin *self)
{
  self->notify_id = ++last_notify_id;
}

static void
addin_iface_init (IdeBuildPipelineAddinInterface *iface)
{
  iface->load = ide_notification_addin_load;
  iface->unload = ide_notification_addin_unload;
}
