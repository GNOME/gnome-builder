/* ide-omni-bar.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-omni-bar"

#include "ide-context.h"
#include "ide-debug.h"

#include "projects/ide-project.h"
#include "util/ide-gtk.h"
#include "vcs/ide-vcs.h"
#include "workbench/ide-omni-bar.h"

struct _IdeOmniBar
{
  GtkBox    parent_instance;

  GtkLabel *branch_label;
  GtkLabel *project_label;
};

G_DEFINE_TYPE (IdeOmniBar, ide_omni_bar, GTK_TYPE_BOX)

static void
ide_omni_bar_update (IdeOmniBar *self)
{
  g_autofree gchar *branch_name = NULL;
  const gchar *project_name = NULL;
  IdeContext *context;

  g_assert (IDE_IS_OMNI_BAR (self));

  context = ide_widget_get_context (GTK_WIDGET (self));

  if (IDE_IS_CONTEXT (context))
    {
      IdeProject *project;
      IdeVcs *vcs;

      project = ide_context_get_project (context);
      project_name = ide_project_get_name (project);

      vcs = ide_context_get_vcs (context);
      branch_name = ide_vcs_get_branch_name (vcs);
    }

  gtk_label_set_label (self->project_label, project_name);
  gtk_label_set_label (self->branch_label, branch_name);
}

static void
ide_omni_bar_context_set (GtkWidget  *widget,
                          IdeContext *context)
{
  IdeOmniBar *self = (IdeOmniBar *)widget;

  IDE_ENTRY;

  g_assert (IDE_IS_OMNI_BAR (self));
  g_assert (!context || IDE_IS_CONTEXT (context));

  ide_omni_bar_update (self);

  if (context != NULL)
    {
      IdeVcs *vcs = ide_context_get_vcs (context);

      g_signal_connect_object (vcs,
                               "changed",
                               G_CALLBACK (ide_omni_bar_update),
                               self,
                               G_CONNECT_SWAPPED);
    }

  IDE_EXIT;
}

static void
ide_omni_bar_finalize (GObject *object)
{
  G_OBJECT_CLASS (ide_omni_bar_parent_class)->finalize (object);
}

static void
ide_omni_bar_class_init (IdeOmniBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ide_omni_bar_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-omni-bar.ui");
  gtk_widget_class_set_css_name (widget_class, "omnibar");
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, branch_label);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, project_label);
}

static void
ide_omni_bar_init (IdeOmniBar *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  ide_widget_set_context_handler (self, ide_omni_bar_context_set);
}

GtkWidget *
ide_omni_bar_new (void)
{
  return g_object_new (IDE_TYPE_OMNI_BAR, NULL);
}
