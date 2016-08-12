/* gbp-gobject-workbench-addin.c
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

#define G_LOG_DOMAIN "gbp-gobject-workbench-addin"

#include <glib/gi18n.h>
#include <tmpl-glib.h>

#include "gbp-gobject-dialog.h"
#include "gbp-gobject-spec.h"
#include "gbp-gobject-template.h"
#include "gbp-gobject-workbench-addin.h"

struct _GbpGobjectWorkbenchAddin
{
  GObject parent_instance;

  IdeWorkbench *workbench;
};

static void workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface);

G_DEFINE_TYPE_EXTENDED (GbpGobjectWorkbenchAddin, gbp_gobject_workbench_addin, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN,
                                               workbench_addin_iface_init))

static void
gbp_gobject_workbench_addin_class_init (GbpGobjectWorkbenchAddinClass *klass)
{
}

static void
gbp_gobject_workbench_addin_init (GbpGobjectWorkbenchAddin *self)
{
}

static void
dialog_hide_cb (GbpGobjectWorkbenchAddin *self,
                GbpGobjectDialog         *dialog)
{
  g_assert (GBP_IS_GOBJECT_WORKBENCH_ADDIN (self));
  g_assert (GBP_IS_GOBJECT_DIALOG (dialog));

  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
expand_all_cb (GObject      *object,
               GAsyncResult *result,
               gpointer      user_data)
{
  GbpGobjectTemplate *template = (GbpGobjectTemplate *)object;
  g_autoptr(GbpGobjectWorkbenchAddin) self = user_data;
  g_autoptr(GError) error = NULL;
  IdeContext *context = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_GOBJECT_TEMPLATE (template));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (self->workbench != NULL)
    context = ide_workbench_get_context (self->workbench);

  if (!ide_template_base_expand_all_finish (IDE_TEMPLATE_BASE (template), result, &error))
    {
      if (context != NULL)
        ide_context_warning (context, "%s", error->message);
      else
        g_warning ("%s", error->message);
    }

  IDE_EXIT;
}

static void
generate_from_spec (GbpGobjectWorkbenchAddin *self,
                    GbpGobjectSpec           *spec,
                    GFile                    *directory)
{
  g_autoptr(GbpGobjectTemplate) template = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_GOBJECT_WORKBENCH_ADDIN (self));
  g_assert (GBP_IS_GOBJECT_SPEC (spec));
  g_assert (G_IS_FILE (directory));

  template = gbp_gobject_template_new ();
  gbp_gobject_template_set_spec (template, spec);
  gbp_gobject_template_set_directory (template, directory);
  gbp_gobject_template_set_language (template, GBP_GOBJECT_LANGUAGE_C);

  ide_template_base_expand_all_async (IDE_TEMPLATE_BASE (template),
                                      NULL,
                                      expand_all_cb,
                                      g_object_ref (self));

  IDE_EXIT;
}

static void
dialog_apply_cb (GbpGobjectWorkbenchAddin *self,
                 GbpGobjectDialog         *dialog)
{
  g_autoptr(GFile) directory = NULL;
  GbpGobjectSpec *spec;

  IDE_ENTRY;

  g_assert (GBP_IS_GOBJECT_WORKBENCH_ADDIN (self));
  g_assert (GBP_IS_GOBJECT_DIALOG (dialog));

  spec = gbp_gobject_dialog_get_spec (dialog);
  directory = gbp_gobject_dialog_get_directory (dialog);

  g_assert (GBP_IS_GOBJECT_SPEC (spec));
  g_assert (G_IS_FILE (directory));

  if (gbp_gobject_spec_get_ready (spec))
    generate_from_spec (self, spec, directory);

  gtk_widget_destroy (GTK_WIDGET (dialog));

  IDE_EXIT;
}

static void
new_gobject_activate (GSimpleAction *action,
                      GVariant      *param,
                      gpointer       user_data)
{
  GbpGobjectWorkbenchAddin *self = user_data;
  GbpGobjectDialog *dialog;
  IdeContext *context;
  IdeVcs *vcs;
  GFile *workdir;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (param == NULL);
  g_assert (GBP_IS_GOBJECT_WORKBENCH_ADDIN (self));

  context = ide_workbench_get_context (self->workbench);
  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);

  dialog = g_object_new (GBP_TYPE_GOBJECT_DIALOG,
                         "directory", workdir,
                         "modal", TRUE,
                         "transient-for", self->workbench,
                         "title", _("New Class"),
                         NULL);

  g_signal_connect_object (dialog,
                           "close",
                           G_CALLBACK (dialog_apply_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (dialog,
                           "cancel",
                           G_CALLBACK (dialog_hide_cb),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_window_present (GTK_WINDOW (dialog));
}

static void
gbp_gobject_workbench_addin_load (IdeWorkbenchAddin *addin,
                                  IdeWorkbench      *workbench)
{
  GbpGobjectWorkbenchAddin *self = (GbpGobjectWorkbenchAddin *)addin;
  g_autoptr(GSimpleActionGroup) group = NULL;
  static const GActionEntry entries[] = {
    { "new-gobject", new_gobject_activate },
  };

  g_assert (IDE_IS_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = workbench;

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group),
                                   entries,
                                   G_N_ELEMENTS (entries),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (workbench),
                                  "gobject-templates",
                                  G_ACTION_GROUP (group));
}

static void
gbp_gobject_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                    IdeWorkbench      *workbench)
{
  GbpGobjectWorkbenchAddin *self = (GbpGobjectWorkbenchAddin *)addin;

  g_assert (IDE_IS_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = NULL;

  gtk_widget_insert_action_group (GTK_WIDGET (workbench), "gobject-templates", NULL);
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gbp_gobject_workbench_addin_load;
  iface->unload = gbp_gobject_workbench_addin_unload;
}
