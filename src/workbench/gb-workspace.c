/* gb-workspace.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#include <glib/gi18n.h>

#include "gb-workspace.h"

struct _GbWorkspacePrivate
{
  gchar *icon_name;
  gchar *title;
};

enum {
  PROP_0,
  PROP_ICON_NAME,
  PROP_TITLE,
  LAST_PROP
};

enum {
  NEW_DOCUMENT,
  LAST_SIGNAL
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GbWorkspace, gb_workspace, GTK_TYPE_BIN)

static GParamSpec *gParamSpecs[LAST_PROP];
static guint       gSignals [LAST_SIGNAL];

/**
 * gb_workspace_get_actions:
 * @workspace: A #GbWorkspace.
 *
 * Fetch the actions for the workspace, to be added to the toplevel.
 * The actions will be added with the prefix for the workspace based on
 * the "name" property.
 *
 * Returns: (transfer none): A #GActionGroup or %NULL.
 */
GActionGroup *
gb_workspace_get_actions (GbWorkspace *workspace)
{
  g_return_val_if_fail (GB_IS_WORKSPACE (workspace), NULL);

  if (GB_WORKSPACE_GET_CLASS (workspace)->get_actions)
    return GB_WORKSPACE_GET_CLASS (workspace)->get_actions (workspace);

  return NULL;
}

const gchar *
gb_workspace_get_icon_name (GbWorkspace *workspace)
{
  g_return_val_if_fail (GB_IS_WORKSPACE (workspace), NULL);

  return workspace->priv->icon_name;
}

void
gb_workspace_set_icon_name (GbWorkspace *workspace,
                            const gchar *icon_name)
{
  g_return_if_fail (GB_IS_WORKSPACE (workspace));

  g_free (workspace->priv->icon_name);
  workspace->priv->icon_name = g_strdup (icon_name);
  g_object_notify_by_pspec (G_OBJECT (workspace),
                            gParamSpecs[PROP_ICON_NAME]);
}

const gchar *
gb_workspace_get_title (GbWorkspace *workspace)
{
  g_return_val_if_fail (GB_IS_WORKSPACE (workspace), NULL);

  return workspace->priv->title;
}

void
gb_workspace_set_title (GbWorkspace *workspace,
                        const gchar *title)
{
  g_return_if_fail (GB_IS_WORKSPACE (workspace));

  g_free (workspace->priv->title);
  workspace->priv->title = g_strdup (title);
  g_object_notify_by_pspec (G_OBJECT (workspace),
                            gParamSpecs[PROP_TITLE]);
}

void
gb_workspace_new_document (GbWorkspace *workspace)
{
  g_return_if_fail (GB_IS_WORKSPACE (workspace));

  g_signal_emit (workspace, gSignals [NEW_DOCUMENT], 0);
}

static void
gb_workspace_finalize (GObject *object)
{
  GbWorkspacePrivate *priv;

  priv = GB_WORKSPACE (object)->priv;

  g_clear_pointer (&priv->icon_name, g_free);
  g_clear_pointer (&priv->title, g_free);

  G_OBJECT_CLASS (gb_workspace_parent_class)->finalize (object);
}

static void
gb_workspace_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GbWorkspace *workspace = GB_WORKSPACE (object);

  switch (prop_id)
    {
    case PROP_ICON_NAME:
      g_value_set_string (value, gb_workspace_get_icon_name (workspace));
      break;

    case PROP_TITLE:
      g_value_set_string (value, gb_workspace_get_title (workspace));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_workspace_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GbWorkspace *workspace = GB_WORKSPACE (object);

  switch (prop_id)
    {
    case PROP_ICON_NAME:
      gb_workspace_set_icon_name (workspace, g_value_get_string (value));
      break;

    case PROP_TITLE:
      gb_workspace_set_title (workspace, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_workspace_class_init (GbWorkspaceClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = gb_workspace_finalize;
  object_class->get_property = gb_workspace_get_property;
  object_class->set_property = gb_workspace_set_property;

  gParamSpecs[PROP_TITLE] =
    g_param_spec_string ("title",
                         _("Title"),
                         _("The title of the workspace."),
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_TITLE,
                                   gParamSpecs[PROP_TITLE]);

  gParamSpecs[PROP_ICON_NAME] =
    g_param_spec_string ("icon-name",
                         _("Icon Name"),
                         _("The name of the icon to use."),
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_ICON_NAME,
                                   gParamSpecs[PROP_ICON_NAME]);

  gSignals [NEW_DOCUMENT] =
    g_signal_new ("new-document",
                  GB_TYPE_WORKSPACE,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GbWorkspaceClass, new_document),
                  NULL,
                  NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);
}

static void
gb_workspace_init (GbWorkspace *workspace)
{
  workspace->priv = gb_workspace_get_instance_private (workspace);
}
