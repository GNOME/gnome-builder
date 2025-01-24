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

#include "config.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

#include <libide-foundry.h>

#include "ide-notification-addin.h"

#define GRACE_PERIOD_USEC (G_USEC_PER_SEC * 5)

struct _IdeNotificationAddin
{
  IdeObject         parent_instance;
  IdeNotification  *notif;
  char             *last_msg_body;
  char             *shell_notif_id;
  IdePipelinePhase  requested_phase;
  gint64            last_time;
  guint             suppress : 1;
  guint             did_first_build : 1;
};

static void addin_iface_init (IdePipelineAddinInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeNotificationAddin,
                               ide_notification_addin,
                               IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_PIPELINE_ADDIN, addin_iface_init))

static gboolean
title_with_default (GBinding     *binding,
                    const GValue *from_value,
                    GValue       *to_value,
                    gpointer      user_data)
{
  g_autofree char *str = g_strstrip (g_value_dup_string (from_value));

  if (ide_str_empty0 (str))
    g_value_set_static_string (to_value, _("Building…"));
  else
    g_value_take_string (to_value, g_steal_pointer (&str));

  return TRUE;
}

static gboolean
should_suppress_message (IdeNotificationAddin *self,
                         const char           *message)
{
  g_assert (IDE_IS_NOTIFICATION_ADDIN (self));
  g_assert (message != NULL);

  if (self->last_msg_body == NULL ||
      !ide_str_equal0 (self->last_msg_body, message) ||
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
  g_autoptr(GNotification) notification = NULL;
  g_autoptr(GIcon) icon = NULL;
  GtkApplication *app;
  const gchar *msg_title;
  IdeContext *context;
  GtkWindow *window;

  g_assert (IDE_IS_NOTIFICATION_ADDIN (self));

  if (self->suppress)
    return;

  app = GTK_APPLICATION (g_application_get_default ());

  if (!(window = gtk_application_get_active_window (app)))
    return;

  if (gtk_window_is_active (window))
    return;

  g_clear_pointer (&self->shell_notif_id, g_free);

  context = ide_object_get_context (IDE_OBJECT (self));
  project_name = ide_context_dup_title (context);
  self->shell_notif_id = ide_context_dup_project_id (context);

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

#ifdef DEVELOPMENT_BUILD
  icon = g_themed_icon_new ("org.gnome.Builder.Devel-symbolic");
#else
  icon = g_themed_icon_new ("org.gnome.Builder-symbolic");
#endif

  notification = g_notification_new (msg_title);
  g_notification_set_body (notification, msg_body);
  g_notification_set_priority (notification, G_NOTIFICATION_PRIORITY_NORMAL);
  g_notification_set_icon (notification, icon);

  if (!should_suppress_message (self, msg_body))
    g_application_send_notification (g_application_get_default (), self->shell_notif_id, notification);
}

static void
ide_notification_addin_pipeline_started (IdeNotificationAddin *self,
                                         IdePipelinePhase      requested_phase,
                                         IdePipeline          *pipeline)
{
  g_assert (IDE_IS_NOTIFICATION_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  if (self->notif != NULL)
    {
      ide_notification_withdraw (self->notif);
      g_clear_object (&self->notif);
    }

  /* Be certain we don't get before/after flags */
  g_assert ((requested_phase & IDE_PIPELINE_PHASE_MASK) == requested_phase);

  /*
   * We don't care about any build that is advancing to a phase before the
   * BUILD phase. We advanced to CONFIGURE a lot when extracting build flags.
   * However, we do want to show that at least once when the pipeline is
   * setting up so that the user knows something is happening (such as building
   * deps).
   */
  self->requested_phase = requested_phase;
  self->suppress = requested_phase < IDE_PIPELINE_PHASE_BUILD && self->did_first_build;
  self->did_first_build = TRUE;

  g_assert (self->notif == NULL);

  if (!self->suppress)
    {
      /* Setup new in-app notification */
      self->notif = ide_notification_new ();
      g_object_bind_property_full (pipeline, "message", self->notif, "title",
                                   G_BINDING_SYNC_CREATE,
                                   title_with_default, NULL, NULL, NULL);
      ide_notification_attach (self->notif, IDE_OBJECT (pipeline));

      /* Withdraw previous shell notification (it's now invalid) */
      if (self->shell_notif_id)
        g_application_withdraw_notification (g_application_get_default (), self->shell_notif_id);
    }
}

static void
ide_notification_addin_pipeline_finished (IdeNotificationAddin *self,
                                          gboolean              failed,
                                          IdePipeline          *pipeline)
{
  g_assert (IDE_IS_NOTIFICATION_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  if (self->notif)
    {
      if (!failed)
        {
          ide_notification_set_icon_name (self->notif, "builder-check-symbolic");

          if (self->requested_phase & IDE_PIPELINE_PHASE_BUILD)
            ide_notification_set_title (self->notif, _("Build succeeded"));
          else if (self->requested_phase & IDE_PIPELINE_PHASE_CONFIGURE)
            ide_notification_set_title (self->notif, _("Build configured"));
          else if (self->requested_phase & IDE_PIPELINE_PHASE_AUTOGEN)
            ide_notification_set_title (self->notif, _("Build bootstrapped"));
        }
      else
        {
          ide_notification_set_icon_name (self->notif, "dialog-error-symbolic");
          ide_notification_set_title (self->notif, _("Build failed"));
        }

      ide_notification_addin_notify (self, !failed);
    }
}

static void
ide_notification_addin_load (IdePipelineAddin *addin,
                             IdePipeline      *pipeline)
{
  IdeNotificationAddin *self = (IdeNotificationAddin *)addin;

  g_assert (IDE_IS_NOTIFICATION_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  g_signal_connect_object (pipeline,
                           "started",
                           G_CALLBACK (ide_notification_addin_pipeline_started),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (pipeline,
                           "finished",
                           G_CALLBACK (ide_notification_addin_pipeline_finished),
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

  g_signal_handlers_disconnect_by_func (pipeline,
                                        G_CALLBACK (ide_notification_addin_pipeline_started),
                                        self);
  g_signal_handlers_disconnect_by_func (pipeline,
                                        G_CALLBACK (ide_notification_addin_pipeline_finished),
                                        self);

  /* Release in-app notification */
  if (self->notif != NULL)
    {
      ide_notification_withdraw (self->notif);
      g_clear_object (&self->notif);
    }

  /* Release desktop notification */
  if (self->shell_notif_id)
    g_application_withdraw_notification (g_application_get_default (), self->shell_notif_id);

  g_clear_pointer (&self->last_msg_body, g_free);
  g_clear_pointer (&self->shell_notif_id, g_free);
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
