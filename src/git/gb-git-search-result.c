/* gb-git-search-result.c
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

#include "gb-document-manager.h"
#include "gb-editor-document.h"
#include "gb-editor-workspace.h"
#include "gb-git-search-result.h"
#include "gb-widget.h"
#include "gb-workbench.h"

struct _GbGitSearchResultPrivate
{
  gchar *path;

  GtkLabel *label;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbGitSearchResult, gb_git_search_result,
                            GB_TYPE_SEARCH_RESULT)

enum {
  PROP_0,
  PROP_PATH,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

GtkWidget *
gb_git_search_result_new (const gchar *path)
{
  return g_object_new (GB_TYPE_GIT_SEARCH_RESULT, "path", path, NULL);
}

const gchar *
gb_git_search_result_get_path (GbGitSearchResult *result)
{
  g_return_val_if_fail (GB_IS_GIT_SEARCH_RESULT (result), NULL);

  return result->priv->path;
}

static void
gb_git_search_result_set_path (GbGitSearchResult *result,
                               const gchar       *path)
{
  g_return_if_fail (GB_IS_GIT_SEARCH_RESULT (result));

  if (path != result->priv->path)
    {
      g_free (result->priv->path);
      result->priv->path = g_strdup (path);
      g_object_notify_by_pspec (G_OBJECT (result), gParamSpecs [PROP_PATH]);
    }
}

static void
gb_git_search_result_activate (GbSearchResult *result)
{
  GbGitSearchResult *self = (GbGitSearchResult *)result;
  GbWorkspace *workspace;
  GbWorkbench *workbench;
  GFile *file;

  g_return_if_fail (GB_IS_GIT_SEARCH_RESULT (self));

  workbench = gb_widget_get_workbench (GTK_WIDGET (result));
  workspace = gb_workbench_get_workspace (workbench, GB_TYPE_EDITOR_WORKSPACE);

  /* TODO: probably should store the GFile so we can compare it to the
   * root of the repository for proper display paths */
  file = g_file_new_for_path (self->priv->path);
  gb_editor_workspace_open (GB_EDITOR_WORKSPACE (workspace), file);
  g_clear_object (&file);
}

static void
gb_git_search_result_constructed (GObject *object)
{
  GbGitSearchResult *self = (GbGitSearchResult *)object;

  G_OBJECT_CLASS (gb_git_search_result_parent_class)->constructed (object);

  g_object_bind_property (self, "path", self->priv->label, "label",
                          G_BINDING_SYNC_CREATE);
}

static void
gb_git_search_result_finalize (GObject *object)
{
  GbGitSearchResultPrivate *priv = GB_GIT_SEARCH_RESULT (object)->priv;

  g_clear_pointer (&priv->path, g_free);

  G_OBJECT_CLASS (gb_git_search_result_parent_class)->finalize (object);
}

static void
gb_git_search_result_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GbGitSearchResult *self = GB_GIT_SEARCH_RESULT (object);

  switch (prop_id)
    {
    case PROP_PATH:
      g_value_set_string (value, gb_git_search_result_get_path (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_git_search_result_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GbGitSearchResult *self = GB_GIT_SEARCH_RESULT (object);

  switch (prop_id)
    {
    case PROP_PATH:
      gb_git_search_result_set_path (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_git_search_result_class_init (GbGitSearchResultClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GbSearchResultClass *result_class = GB_SEARCH_RESULT_CLASS (klass);

  object_class->constructed = gb_git_search_result_constructed;
  object_class->finalize = gb_git_search_result_finalize;
  object_class->get_property = gb_git_search_result_get_property;
  object_class->set_property = gb_git_search_result_set_property;

  result_class->activate = gb_git_search_result_activate;

  gParamSpecs [PROP_PATH] =
    g_param_spec_string ("path",
                         _("Path"),
                         _("The path to the resulting file."),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_PATH,
                                   gParamSpecs [PROP_PATH]);

  GB_WIDGET_CLASS_TEMPLATE (klass, "gb-git-search-result.ui");
  GB_WIDGET_CLASS_BIND (klass, GbGitSearchResult, label);
}

static void
gb_git_search_result_init (GbGitSearchResult *self)
{
  self->priv = gb_git_search_result_get_instance_private (self);
  gtk_widget_init_template (GTK_WIDGET (self));
}
