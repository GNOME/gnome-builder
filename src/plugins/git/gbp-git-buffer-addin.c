/* gbp-git-buffer-addin.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-git-buffer-addin"

#include "config.h"

#include <libgit2-glib/ggit.h>
#include <libide-vcs.h>

#include "gbp-git-buffer-addin.h"
#include "gbp-git-buffer-change-monitor.h"
#include "gbp-git-vcs.h"

struct _GbpGitBufferAddin
{
  GObject                    parent_instance;
  GbpGitBufferChangeMonitor *monitor;
};

static void
gbp_git_buffer_addin_file_laoded (IdeBufferAddin *addin,
                                  IdeBuffer      *buffer,
                                  GFile          *file)
{
  GbpGitBufferAddin *self = (GbpGitBufferAddin *)addin;
  g_autoptr(GbpGitBufferChangeMonitor) monitor = NULL;
  g_autoptr(IdeContext) context = NULL;
  GgitRepository *repository;
  IdeObjectBox *box;
  IdeVcs *vcs;

  g_assert (GBP_IS_GIT_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_FILE (file));

  context = ide_buffer_ref_context (buffer);
  vcs = ide_context_peek_child_typed (context, IDE_TYPE_VCS);
  if (!GBP_IS_GIT_VCS (vcs))
    return;

  if (!(repository = gbp_git_vcs_get_repository (GBP_GIT_VCS (vcs))))
    return;

  self->monitor = g_object_new (GBP_TYPE_GIT_BUFFER_CHANGE_MONITOR,
                                "buffer", buffer,
                                "repository", repository,
                                NULL);

  box = ide_object_box_from_object (G_OBJECT (buffer));
  ide_object_append (IDE_OBJECT (box), IDE_OBJECT (self->monitor));

  ide_buffer_set_change_monitor (buffer, IDE_BUFFER_CHANGE_MONITOR (self->monitor));
}

static void
gbp_git_buffer_addin_file_saved (IdeBufferAddin *addin,
                                 IdeBuffer      *buffer,
                                 GFile          *file)
{
  GbpGitBufferAddin *self = (GbpGitBufferAddin *)addin;

  g_assert (GBP_IS_GIT_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_FILE (file));

  if (self->monitor != NULL)
    ide_buffer_change_monitor_reload (IDE_BUFFER_CHANGE_MONITOR (self->monitor));
}

static void
gbp_git_buffer_addin_unload (IdeBufferAddin *addin,
                             IdeBuffer      *buffer)
{
  GbpGitBufferAddin *self = (GbpGitBufferAddin *)addin;

  g_assert (GBP_IS_GIT_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  if (self->monitor != NULL)
    {
      ide_buffer_set_change_monitor (buffer, NULL);
      ide_clear_and_destroy_object (&self->monitor);
    }
}

static void
buffer_addin_iface_init (IdeBufferAddinInterface *iface)
{
  iface->file_loaded = gbp_git_buffer_addin_file_laoded;
  iface->file_saved = gbp_git_buffer_addin_file_saved;
  iface->unload = gbp_git_buffer_addin_unload;
}

G_DEFINE_TYPE_WITH_CODE (GbpGitBufferAddin, gbp_git_buffer_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_BUFFER_ADDIN, buffer_addin_iface_init))

static void
gbp_git_buffer_addin_class_init (GbpGitBufferAddinClass *klass)
{
}

static void
gbp_git_buffer_addin_init (GbpGitBufferAddin *self)
{
}
