/* gbp-devhelp-session-addin.c
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


#define G_LOG_DOMAIN "gbp-devhelp-session-addin"

#include "config.h"

#include <libide-threading.h>

#include "gbp-devhelp-session-addin.h"

#include "gbp-devhelp-page.h"

struct _GbpDevhelpSessionAddin
{
  IdeObject parent_instance;
};

static void
gbp_devhelp_session_addin_save_page_async (IdeSessionAddin     *addin,
                                           IdePage             *page,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  GVariantDict state_dict;
  const char *page_uri;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_DEVHELP_SESSION_ADDIN (addin));
  g_assert (GBP_IS_DEVHELP_PAGE (page));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (addin, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_devhelp_session_addin_save_page_async);

  page_uri = gbp_devhelp_page_get_uri (GBP_DEVHELP_PAGE (page));
  g_variant_dict_init (&state_dict, NULL);
  g_variant_dict_insert (&state_dict, "uri", "s", page_uri ? page_uri : "");

  IDE_TRACE_MSG ("Saving devhelp page URI \"%s\"", page_uri);

  ide_task_return_pointer (task, g_variant_take_ref (g_variant_dict_end (&state_dict)), g_variant_unref);

  IDE_EXIT;
}

static GVariant *
gbp_devhelp_session_addin_save_page_finish (IdeSessionAddin  *self,
                                            GAsyncResult     *result,
                                            GError          **error)
{
  g_assert (GBP_IS_DEVHELP_SESSION_ADDIN (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
gbp_devhelp_session_addin_restore_page_async (IdeSessionAddin     *addin,
                                              GVariant            *state,
                                              GCancellable        *cancellable,
                                              GAsyncReadyCallback  callback,
                                              gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  GVariantDict state_dict;
  const char *uri;

  g_assert (GBP_IS_DEVHELP_SESSION_ADDIN (addin));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (g_variant_is_of_type (state, G_VARIANT_TYPE_VARDICT));

  task = ide_task_new (addin, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_devhelp_session_addin_restore_page_async);

  g_variant_dict_init (&state_dict, state);
  g_variant_dict_lookup (&state_dict, "uri", "&s", &uri);
  g_assert (uri != NULL);

  IDE_TRACE_MSG ("Restoring devhelp page URI \"%s\"", uri);

  ide_task_return_pointer (task,
                           g_object_new (GBP_TYPE_DEVHELP_PAGE,
                                         "uri", uri,
                                         "visible", TRUE,
                                         NULL),
                           g_object_unref);
}

static IdePage *
gbp_devhelp_session_addin_restore_page_finish (IdeSessionAddin  *self,
                                               GAsyncResult     *result,
                                               GError          **error)
{
  g_assert (GBP_IS_DEVHELP_SESSION_ADDIN (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static gboolean
gbp_devhelp_session_addin_can_save_page (IdeSessionAddin *addin,
                                        IdePage         *page)
{
  return GBP_IS_DEVHELP_PAGE (page);
}

static char **
gbp_devhelp_session_addin_get_autosave_properties (IdeSessionAddin *addin)
{
  GStrvBuilder *builder = NULL;

  g_assert (GBP_IS_DEVHELP_SESSION_ADDIN (addin));

  builder = g_strv_builder_new ();
  g_strv_builder_add (builder, "uri");
  return g_strv_builder_end (builder);
}

static void
session_addin_iface_init (IdeSessionAddinInterface *iface)
{
  iface->save_page_async = gbp_devhelp_session_addin_save_page_async;
  iface->save_page_finish = gbp_devhelp_session_addin_save_page_finish;
  iface->restore_page_async = gbp_devhelp_session_addin_restore_page_async;
  iface->restore_page_finish = gbp_devhelp_session_addin_restore_page_finish;
  iface->can_save_page = gbp_devhelp_session_addin_can_save_page;
  iface->get_autosave_properties = gbp_devhelp_session_addin_get_autosave_properties;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpDevhelpSessionAddin, gbp_devhelp_session_addin, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_SESSION_ADDIN, session_addin_iface_init))

static void
gbp_devhelp_session_addin_class_init (GbpDevhelpSessionAddinClass *klass)
{
}

static void
gbp_devhelp_session_addin_init (GbpDevhelpSessionAddin *self)
{
}
