/* gbp-owe-workbench-addin.c
 *
 * Copyright 2021 vanadiae <vanadiae35@gmail.com>
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

#define G_LOG_DOMAIN "gbp-owe-workbench-addin"

#include "config.h"

#include "gbp-owe-workbench-addin.h"

#include <libportal/portal.h>
#include <libportal-gtk4/portal-gtk4.h>

struct _GbpOweWorkbenchAddin
{
  IdeObject parent_instance;
  XdpPortal *portal;
  IdeWorkbench *workbench;
};

static void addin_iface_init (IdeWorkbenchAddinInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpOweWorkbenchAddin,
                               gbp_owe_workbench_addin,
                               IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, addin_iface_init))

static gboolean
gbp_owe_workbench_addin_can_open (IdeWorkbenchAddin     *addin,
                                  GFile                 *file,
                                  const gchar           *content_type,
                                  gint                  *priority)
{
  GbpOweWorkbenchAddin *self = (GbpOweWorkbenchAddin *)addin;
  GFileType filetype;

  g_assert (GBP_IS_OWE_WORKBENCH_ADDIN (self));
  g_assert (G_IS_FILE (file));
  g_assert (priority != NULL);

  filetype = g_file_query_file_type (file, G_FILE_QUERY_INFO_NONE, NULL);

  /* We want the addin to be used only as last resort, when none satisfied better.
   * See the ls plugin's workbench addin for an explanation on why not a full G_MAXINT.
   */
  *priority = G_MAXINT / 2;

  /* xdp_portal_open_uri() doesn't accept opening directories, and anyway there's
   * the Open Containing Folder entry for that purpose.
   */
  return filetype != G_FILE_TYPE_DIRECTORY;
}

static void
on_file_opened_cb (GObject      *source_object,
                   GAsyncResult *res,
                   gpointer      user_data)
{
  XdpPortal *portal = (XdpPortal *)source_object;
  g_autoptr(IdeTask) task = (IdeTask *)user_data;
  g_autoptr(GError) error = NULL;

  g_assert (XDP_IS_PORTAL (portal));
  g_assert (IDE_IS_TASK (task));

  if (!xdp_portal_open_uri_finish (portal, res, &error) &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    ide_task_return_new_error (task, error->domain, error->code,
                               "Couldn't open file with external program using libportal: %s", error->message);
  else
    ide_task_return_boolean (task, TRUE);
}

static void
gbp_owe_workbench_addin_open_async (IdeWorkbenchAddin   *addin,
                                    GFile               *file,
                                    const gchar         *content_type,
                                    int                  line,
                                    int                  line_offset,
                                    IdeBufferOpenFlags   flags,
                                    PanelPosition       *position,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  GbpOweWorkbenchAddin *self = (GbpOweWorkbenchAddin *)addin;
  g_autoptr(IdeTask) task = NULL;
  g_autofree gchar *uri = NULL;
  XdpParent *parent;
  GtkWindow *current_window;

  g_assert (GBP_IS_OWE_WORKBENCH_ADDIN (self));
  g_assert (G_IS_FILE (file));

  if (self->portal == NULL)
    self->portal = xdp_portal_new ();

  uri = g_file_get_uri (file);
  task = ide_task_new (self, cancellable, callback, user_data);

  current_window = GTK_WINDOW (ide_workbench_get_current_workspace (self->workbench));
  parent = xdp_parent_new_gtk (current_window);
  xdp_portal_open_uri (self->portal,
                       parent,
                       uri,
                       XDP_OPEN_URI_FLAG_ASK | XDP_OPEN_URI_FLAG_WRITABLE,
                       ide_task_get_cancellable (task),
                       on_file_opened_cb,
                       g_object_ref (task));
  xdp_parent_free (parent);
}

static gboolean
gbp_owe_workbench_addin_open_finish (IdeWorkbenchAddin     *addin,
                                     GAsyncResult          *result,
                                     GError               **error)
{
  GbpOweWorkbenchAddin *self = (GbpOweWorkbenchAddin *)addin;

  g_assert (GBP_IS_OWE_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
gbp_owe_workbench_addin_load (IdeWorkbenchAddin *addin,
                              IdeWorkbench      *workbench)
{
  GbpOweWorkbenchAddin *self = (GbpOweWorkbenchAddin *)addin;

  g_assert (GBP_IS_OWE_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = workbench;
}

static void
gbp_owe_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                IdeWorkbench      *workbench)
{
  GbpOweWorkbenchAddin *self = (GbpOweWorkbenchAddin *)addin;

  g_assert (GBP_IS_OWE_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  if (self->portal != NULL)
    g_clear_object (&self->portal);

  self->workbench = NULL;
}

static void
gbp_owe_workbench_addin_class_init (GbpOweWorkbenchAddinClass *klass)
{
}

static void
gbp_owe_workbench_addin_init (GbpOweWorkbenchAddin *self)
{
}

static void
addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gbp_owe_workbench_addin_load;
  iface->unload = gbp_owe_workbench_addin_unload;
  iface->can_open = gbp_owe_workbench_addin_can_open;
  iface->open_finish = gbp_owe_workbench_addin_open_finish;
  iface->open_async = gbp_owe_workbench_addin_open_async;
}
