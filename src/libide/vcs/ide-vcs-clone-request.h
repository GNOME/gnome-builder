/* ide-vcs-clone-request.h
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#if !defined (IDE_VCS_INSIDE) && !defined (IDE_VCS_COMPILATION)
# error "Only <libide-vcs.h> can be included directly."
#endif

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_VCS_CLONE_REQUEST (ide_vcs_clone_request_get_type())

typedef enum
{
  IDE_VCS_CLONE_REQUEST_VALID           = 0,
  IDE_VCS_CLONE_REQUEST_INVAL_URI       = 1 << 0,
  IDE_VCS_CLONE_REQUEST_INVAL_DIRECTORY = 1 << 1,
  IDE_VCS_CLONE_REQUEST_INVAL_EMAIL     = 1 << 2,
} IdeVcsCloneRequestValidation;

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeVcsCloneRequest, ide_vcs_clone_request, IDE, VCS_CLONE_REQUEST, IdeObject)

IDE_AVAILABLE_IN_ALL
IdeVcsCloneRequest           *ide_vcs_clone_request_new               (void);
IDE_AVAILABLE_IN_ALL
const char                   *ide_vcs_clone_request_get_module_name   (IdeVcsCloneRequest   *self);
IDE_AVAILABLE_IN_ALL
void                          ide_vcs_clone_request_set_module_name   (IdeVcsCloneRequest   *self,
                                                                       const char           *module_name);
IDE_AVAILABLE_IN_ALL
const char                   *ide_vcs_clone_request_get_author_name   (IdeVcsCloneRequest   *self);
IDE_AVAILABLE_IN_ALL
void                          ide_vcs_clone_request_set_author_name   (IdeVcsCloneRequest   *self,
                                                                       const char           *author_name);
IDE_AVAILABLE_IN_ALL
const char                   *ide_vcs_clone_request_get_author_email  (IdeVcsCloneRequest   *self);
IDE_AVAILABLE_IN_ALL
void                          ide_vcs_clone_request_set_author_email  (IdeVcsCloneRequest   *self,
                                                                       const char           *author_email);
GListModel                   *ide_vcs_clone_request_get_branch_model  (IdeVcsCloneRequest   *self);
IDE_AVAILABLE_IN_ALL
const char                   *ide_vcs_clone_request_get_branch_name   (IdeVcsCloneRequest   *self);
IDE_AVAILABLE_IN_ALL
void                          ide_vcs_clone_request_set_branch_name   (IdeVcsCloneRequest   *self,
                                                                       const char           *branch_name);
IDE_AVAILABLE_IN_ALL
const char                   *ide_vcs_clone_request_get_uri           (IdeVcsCloneRequest   *self);
IDE_AVAILABLE_IN_ALL
void                          ide_vcs_clone_request_set_uri           (IdeVcsCloneRequest   *self,
                                                                       const char           *uri);
IDE_AVAILABLE_IN_ALL
GFile                        *ide_vcs_clone_request_get_directory     (IdeVcsCloneRequest   *self);
IDE_AVAILABLE_IN_ALL
void                          ide_vcs_clone_request_set_directory     (IdeVcsCloneRequest   *self,
                                                                       GFile                *directory);
IDE_AVAILABLE_IN_ALL
void                          ide_vcs_clone_request_populate_branches (IdeVcsCloneRequest   *self);
IDE_AVAILABLE_IN_ALL
IdeVcsCloneRequestValidation  ide_vcs_clone_request_validate          (IdeVcsCloneRequest   *self);
IDE_AVAILABLE_IN_44
void                          ide_vcs_clone_request_clone_async       (IdeVcsCloneRequest   *self,
                                                                       IdeNotification      *notif,
                                                                       int                   pty_fd,
                                                                       GCancellable         *cancellable,
                                                                       GAsyncReadyCallback   callback,
                                                                       gpointer              user_data);
IDE_AVAILABLE_IN_ALL
GFile                        *ide_vcs_clone_request_clone_finish      (IdeVcsCloneRequest   *self,
                                                                       GAsyncResult         *result,
                                                                       GError              **error);

G_END_DECLS
