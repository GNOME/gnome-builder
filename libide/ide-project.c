/* ide-project.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>

#include "ide-project.h"
#include "ide-project-item.h"

typedef struct
{
  IdeProjectItem *root;
  gchar *name;
} IdeProjectPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeProject, ide_project, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_NAME,
  PROP_ROOT,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

const gchar *
ide_project_get_name (IdeProject *project)
{
  IdeProjectPrivate *priv = ide_project_get_instance_private (project);

  g_return_val_if_fail (IDE_IS_PROJECT (project), NULL);

  return priv->name;
}

void
ide_project_set_name (IdeProject  *project,
                      const gchar *name)
{
  IdeProjectPrivate *priv = ide_project_get_instance_private (project);

  g_return_if_fail (IDE_IS_PROJECT (project));

  if (priv->name != name)
    {
      g_free (priv->name);
      priv->name = g_strdup (name);
      g_object_notify_by_pspec (G_OBJECT (project), gParamSpecs [PROP_NAME]);
    }
}

IdeProjectItem *
ide_project_get_root (IdeProject *project)
{
  IdeProjectPrivate *priv = ide_project_get_instance_private (project);

  g_return_val_if_fail (IDE_IS_PROJECT (project),  NULL);

  return priv->root;
}

static void
ide_project_set_root (IdeProject     *project,
                      IdeProjectItem *root)
{
  IdeProjectPrivate *priv = ide_project_get_instance_private (project);
  g_autoptr(IdeProjectItem) allocated = NULL;
  IdeContext *context;

  g_return_if_fail (IDE_IS_PROJECT (project));
  g_return_if_fail (!root || IDE_IS_PROJECT_ITEM (root));

  context = ide_object_get_context (IDE_OBJECT (project));

  if (!root)
    {
      allocated = g_object_new (IDE_TYPE_PROJECT_ITEM,
                                "context", context,
                                NULL);
      root = allocated;
    }

  if (g_set_object (&priv->root, root))
    g_object_notify_by_pspec (G_OBJECT (project), gParamSpecs [PROP_ROOT]);
}

static void
ide_project_finalize (GObject *object)
{
  IdeProject *self = (IdeProject *)object;
  IdeProjectPrivate *priv = ide_project_get_instance_private (self);

  g_clear_object (&priv->root);
  g_clear_pointer (&priv->name, g_free);

  G_OBJECT_CLASS (ide_project_parent_class)->finalize (object);
}

static void
ide_project_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  IdeProject *self = IDE_PROJECT (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, ide_project_get_name (self));
      break;

    case PROP_ROOT:
      g_value_set_object (value, ide_project_get_root (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_project_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  IdeProject *self = IDE_PROJECT (object);

  switch (prop_id)
    {
    case PROP_NAME:
      ide_project_set_name (self, g_value_get_string (value));
      break;

    case PROP_ROOT:
      ide_project_set_root (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_project_class_init (IdeProjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_project_finalize;
  object_class->get_property = ide_project_get_property;
  object_class->set_property = ide_project_set_property;

  gParamSpecs [PROP_NAME] =
    g_param_spec_string ("name",
                         _("Name"),
                         _("The name of the project."),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_NAME,
                                   gParamSpecs [PROP_NAME]);

  gParamSpecs [PROP_ROOT] =
    g_param_spec_object ("root",
                         _("Root"),
                         _("The root object for the project."),
                         IDE_TYPE_PROJECT_ITEM,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_ROOT,
                                   gParamSpecs [PROP_ROOT]);
}

static void
ide_project_init (IdeProject *self)
{
}
