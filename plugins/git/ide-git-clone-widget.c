/* ide-git-clone-widget.c
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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

#include "ide-git-clone-widget.h"

struct _IdeGitCloneWidget
{
  GtkBin parent_instance;
};

G_DEFINE_TYPE (IdeGitCloneWidget, ide_git_clone_widget, GTK_TYPE_BIN)

enum {
  PROP_0,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static void
ide_git_clone_widget_finalize (GObject *object)
{
  IdeGitCloneWidget *self = (IdeGitCloneWidget *)object;

  G_OBJECT_CLASS (ide_git_clone_widget_parent_class)->finalize (object);
}

static void
ide_git_clone_widget_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  IdeGitCloneWidget *self = IDE_GIT_CLONE_WIDGET (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_git_clone_widget_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  IdeGitCloneWidget *self = IDE_GIT_CLONE_WIDGET (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_git_clone_widget_class_init (IdeGitCloneWidgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ide_git_clone_widget_finalize;
  object_class->get_property = ide_git_clone_widget_get_property;
  object_class->set_property = ide_git_clone_widget_set_property;

  gtk_widget_class_set_css_name (widget_class, "gitclonewidget");
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/git/ide-git-clone-widget.ui");
}

static void
ide_git_clone_widget_init (IdeGitCloneWidget *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
