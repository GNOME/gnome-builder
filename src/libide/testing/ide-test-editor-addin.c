/* ide-test-editor-addin.c
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-test-editor-addin"

#include <glib/gi18n.h>

#include "ide-context.h"

#include "editor/ide-editor-addin.h"
#include "editor/ide-editor-perspective.h"
#include "editor/ide-editor-sidebar.h"
#include "testing/ide-test-editor-addin.h"
#include "testing/ide-test-panel.h"
#include "util/ide-gtk.h"

struct _IdeTestEditorAddin
{
  GObject       parent_instance;

  IdeTestPanel *panel;
};

static void
ide_test_editor_addin_load (IdeEditorAddin       *addin,
                            IdeEditorPerspective *editor)
{
  IdeTestEditorAddin *self = (IdeTestEditorAddin *)addin;
  IdeEditorSidebar *sidebar;
  IdeTestManager *manager;
  IdeContext *context;

  g_assert (IDE_IS_TEST_EDITOR_ADDIN (self));
  g_assert (IDE_IS_EDITOR_PERSPECTIVE (editor));

  context = ide_widget_get_context (GTK_WIDGET (editor));
  manager = ide_context_get_test_manager (context);

  self->panel = g_object_new (IDE_TYPE_TEST_PANEL,
                              "manager", manager,
                              "visible", TRUE,
                              NULL);
  g_signal_connect (self->panel,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroy),
                    &self->panel);

  sidebar = ide_editor_perspective_get_sidebar (editor);

  ide_editor_sidebar_add_section (sidebar,
                                  "tests",
                                  _("Unit Tests"),
                                  "builder-unit-tests-symbolic",
                                  NULL,
                                  NULL,
                                  GTK_WIDGET (self->panel),
                                  400);
}

static void
ide_test_editor_addin_unload (IdeEditorAddin       *addin,
                              IdeEditorPerspective *editor)
{
  IdeTestEditorAddin *self = (IdeTestEditorAddin *)addin;

  g_assert (IDE_IS_TEST_EDITOR_ADDIN (self));
  g_assert (IDE_IS_EDITOR_PERSPECTIVE (editor));

  if (self->panel)
    gtk_widget_destroy (GTK_WIDGET (self->panel));
}

static void
editor_addin_iface_init (IdeEditorAddinInterface *iface)
{
  iface->load = ide_test_editor_addin_load;
  iface->unload = ide_test_editor_addin_unload;
}

G_DEFINE_TYPE_WITH_CODE (IdeTestEditorAddin, ide_test_editor_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_EDITOR_ADDIN, editor_addin_iface_init))

static void
ide_test_editor_addin_class_init (IdeTestEditorAddinClass *klass)
{
}

static void
ide_test_editor_addin_init (IdeTestEditorAddin *self)
{
}
