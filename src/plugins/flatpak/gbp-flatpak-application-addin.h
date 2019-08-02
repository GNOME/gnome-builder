/* gbp-flatpak-application-addin.h
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#pragma once

#include <flatpak.h>
#include <libide-gui.h>

G_BEGIN_DECLS

#define GBP_TYPE_FLATPAK_APPLICATION_ADDIN (gbp_flatpak_application_addin_get_type())

G_DECLARE_FINAL_TYPE (GbpFlatpakApplicationAddin, gbp_flatpak_application_addin, GBP, FLATPAK_APPLICATION_ADDIN, GObject)

GbpFlatpakApplicationAddin *gbp_flatpak_application_addin_get_default            (void);
FlatpakInstalledRef        *gbp_flatpak_application_addin_find_extension         (GbpFlatpakApplicationAddin  *self,
                                                                                  const gchar                 *name);
GPtrArray                  *gbp_flatpak_application_addin_get_runtimes           (GbpFlatpakApplicationAddin  *self);
GPtrArray                  *gbp_flatpak_application_addin_get_installations      (GbpFlatpakApplicationAddin  *self);
gboolean                    gbp_flatpak_application_addin_has_runtime            (GbpFlatpakApplicationAddin  *self,
                                                                                  const gchar                 *id,
                                                                                  const gchar                 *arch,
                                                                                  const gchar                 *branch);
void                        gbp_flatpak_application_addin_check_sysdeps_async    (GbpFlatpakApplicationAddin  *self,
                                                                                  GCancellable                *cancellable,
                                                                                  GAsyncReadyCallback          callback,
                                                                                  gpointer                     user_data);
gboolean                    gbp_flatpak_application_addin_check_sysdeps_finish   (GbpFlatpakApplicationAddin  *self,
                                                                                  GAsyncResult                *result,
                                                                                  GError                     **error);
void                        gbp_flatpak_application_addin_install_runtime_async  (GbpFlatpakApplicationAddin  *self,
                                                                                  const gchar                 *runtime_id,
                                                                                  const gchar                 *arch,
                                                                                  const gchar                 *branch,
                                                                                  GCancellable                *cancellable,
                                                                                  IdeNotification            **progress,
                                                                                  GAsyncReadyCallback          callback,
                                                                                  gpointer                     user_data);
gboolean                    gbp_flatpak_application_addin_install_runtime_finish (GbpFlatpakApplicationAddin  *self,
                                                                                  GAsyncResult                *result,
                                                                                  GError                     **error);
void                        gbp_flatpak_application_addin_locate_sdk_async       (GbpFlatpakApplicationAddin  *self,
                                                                                  const gchar                 *runtime_id,
                                                                                  const gchar                 *arch,
                                                                                  const gchar                 *branch,
                                                                                  GCancellable                *cancellable,
                                                                                  GAsyncReadyCallback          callback,
                                                                                  gpointer                     user_data);
gboolean                    gbp_flatpak_application_addin_locate_sdk_finish      (GbpFlatpakApplicationAddin  *self,
                                                                                  GAsyncResult                *result,
                                                                                  gchar                      **sdk_id,
                                                                                  gchar                      **sdk_arch,
                                                                                  gchar                      **sdk_branch,
                                                                                  GError                     **error);
gchar                      *gbp_flatpak_application_addin_get_deploy_dir         (GbpFlatpakApplicationAddin  *self,
                                                                                  const gchar                 *id,
                                                                                  const gchar                 *arch,
                                                                                  const gchar                 *branch);

G_END_DECLS
