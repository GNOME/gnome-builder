/* ide-directory-vcs.c
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

#define G_LOG_DOMAIN "ide-directory-vcs"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-threading.h>

#include "ide-context.h"

#include "ide-directory-vcs.h"

struct _IdeDirectoryVcs
{
  IdeObject  parent_instances;
  GFile     *workdir;
};

#define LOAD_MAX_FILES 5000

static void vcs_iface_init (IdeVcsInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdeDirectoryVcs, ide_directory_vcs, IDE_TYPE_OBJECT, G_TYPE_FLAG_FINAL,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_VCS, vcs_iface_init))

enum {
  PROP_0,
  N_PROPS,

  /* Override Properties */
  PROP_BRANCH_NAME,
  PROP_WORKDIR,
};

static gchar *
ide_directory_vcs_get_branch_name (IdeVcs *vcs)
{
  return g_strdup (_("unversioned"));
}

static GFile *
ide_directory_vcs_get_workdir (IdeVcs *vcs)
{
  IdeDirectoryVcs *self = (IdeDirectoryVcs *)vcs;

  g_return_val_if_fail (IDE_IS_DIRECTORY_VCS (vcs), NULL);

  /* Note: This function is expected to be thread-safe for
   *       those holding a reference to @vcs. So
   *       @workdir cannot be changed after creation
   *       and must be valid for the lifetime of @vcs.
   */

  return self->workdir;
}

static gboolean
ide_directory_vcs_is_ignored (IdeVcs  *vcs,
                              GFile   *file,
                              GError **error)
{
  g_autofree char *reversed = NULL;

  g_assert (IDE_IS_VCS (vcs));
  g_assert (G_IS_FILE (file));

  reversed = g_file_get_basename (file);

  /* Ignore .dot directories by default. The UI can choose to show them. */
  if (reversed[0] == '.')
    return TRUE;

  /* check suffixes, in reverse */
  reversed = g_strreverse (reversed);
  if ((reversed [0] == '~') ||
      (strncmp (reversed, "al.", 3) == 0) ||        /* .la */
      (strncmp (reversed, "ol.", 3) == 0) ||        /* .lo */
      (strncmp (reversed, "o.", 2) == 0) ||         /* .o */
      (strncmp (reversed, "pws.", 4) == 0) ||       /* .swp */
      (strncmp (reversed, "sped.", 5) == 0) ||      /* .deps */
      (strncmp (reversed, "sbil.", 5) == 0) ||      /* .libs */
      (strncmp (reversed, "cyp.", 4) == 0) ||       /* .pyc */
      (strncmp (reversed, "oyp.", 4) == 0) ||       /* .pyo */
      (strncmp (reversed, "omg.", 4) == 0) ||       /* .gmo */
      (strncmp (reversed, "tig.", 4) == 0) ||       /* .git */
      (strncmp (reversed, "rzb.", 4) == 0) ||       /* .bzr */
      (strncmp (reversed, "nvs.", 4) == 0) ||       /* .svn */
      (strncmp (reversed, "pmatsrid.", 9) == 0) ||  /* .dirstamp */
      (strncmp (reversed, "hcg.", 4) == 0))         /* .gch */
    return TRUE;

  return FALSE;
}

static void
ide_directory_vcs_destroy (IdeObject *object)
{
  IdeDirectoryVcs *self = (IdeDirectoryVcs *)object;

  g_clear_object (&self->workdir);

  IDE_OBJECT_CLASS (ide_directory_vcs_parent_class)->destroy (object);
}

static void
ide_directory_vcs_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  IdeDirectoryVcs *self = IDE_DIRECTORY_VCS (object);

  switch (prop_id)
    {
    case PROP_BRANCH_NAME:
      g_value_take_string (value, ide_directory_vcs_get_branch_name (IDE_VCS (self)));
      break;

    case PROP_WORKDIR:
      g_value_set_object (value, ide_directory_vcs_get_workdir (IDE_VCS (self)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_directory_vcs_class_init (IdeDirectoryVcsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  object_class->get_property = ide_directory_vcs_get_property;

  i_object_class->destroy = ide_directory_vcs_destroy;

  g_object_class_override_property (object_class, PROP_BRANCH_NAME, "branch-name");
  g_object_class_override_property (object_class, PROP_WORKDIR, "workdir");
}

static void
ide_directory_vcs_init (IdeDirectoryVcs *self)
{
}

static gint
ide_directory_vcs_get_priority (IdeVcs *vcs)
{
  return G_MAXINT;
}

static void
vcs_iface_init (IdeVcsInterface *iface)
{
  iface->get_workdir = ide_directory_vcs_get_workdir;
  iface->is_ignored = ide_directory_vcs_is_ignored;
  iface->get_priority = ide_directory_vcs_get_priority;
  iface->get_branch_name = ide_directory_vcs_get_branch_name;
}

IdeDirectoryVcs *
ide_directory_vcs_new (GFile *workdir)
{
  IdeDirectoryVcs *self = g_object_new (IDE_TYPE_DIRECTORY_VCS, NULL);
  self->workdir = g_file_dup (workdir);
  return self;
}
