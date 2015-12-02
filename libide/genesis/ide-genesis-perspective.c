/* ide-genesis-perspective.c
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

#include <glib/gi18n.h>
#include <libpeas/peas.h>

#include "ide-genesis-addin.h"
#include "ide-genesis-perspective.h"
#include "ide-gtk.h"
#include "ide-macros.h"
#include "ide-workbench.h"

struct _IdeGenesisPerspective
{
  GtkBin            parent_instance;

  GActionGroup     *actions;
  PeasExtensionSet *addins;
  GBinding         *continue_binding;
  IdeGenesisAddin  *current_addin;

  GtkHeaderBar     *header_bar;
  GtkListBox       *list_box;
  GtkWidget        *main_page;
  GtkStack         *stack;
  GtkButton        *continue_button;
  GtkButton        *cancel_button;
};

static void perspective_iface_init (IdePerspectiveInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdeGenesisPerspective, ide_genesis_perspective, GTK_TYPE_BIN, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_PERSPECTIVE, perspective_iface_init))

static void
ide_genesis_perspective_addin_added (PeasExtensionSet *set,
                                     PeasPluginInfo   *plugin_info,
                                     PeasExtension    *exten,
                                     gpointer          user_data)
{
  IdeGenesisPerspective *self = user_data;
  g_autofree gchar *icon_name = NULL;
  g_autofree gchar *title = NULL;
  GtkListBoxRow *row;
  GtkBox *box;
  GtkImage *image;
  GtkLabel *label;
  GtkWidget *widget;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_GENESIS_ADDIN (exten));
  g_assert (IDE_IS_GENESIS_PERSPECTIVE (self));

  icon_name = ide_genesis_addin_get_icon_name (IDE_GENESIS_ADDIN (exten));
  title = ide_genesis_addin_get_title (IDE_GENESIS_ADDIN (exten));
  widget = ide_genesis_addin_get_widget (IDE_GENESIS_ADDIN (exten));

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      "visible", TRUE,
                      NULL);
  box = g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_HORIZONTAL,
                      "spacing", 18,
                      "visible", TRUE,
                      NULL);
  image = g_object_new (GTK_TYPE_IMAGE,
                        "hexpand", FALSE,
                        "icon-name", icon_name,
                        "pixel-size", 32,
                        "visible", TRUE,
                        NULL);
  label = g_object_new (GTK_TYPE_LABEL,
                        "label", title,
                        "valign", GTK_ALIGN_BASELINE,
                        "visible", TRUE,
                        "wrap", TRUE,
                        "xalign", 0.0f,
                        NULL);

  g_object_set_data (G_OBJECT (row), "IDE_GENESIS_ADDIN", exten);

  gtk_container_add (GTK_CONTAINER (row), GTK_WIDGET (box));
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (image));
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (label));
  gtk_container_add (GTK_CONTAINER (self->list_box), GTK_WIDGET (row));

  if (widget != NULL)
    gtk_container_add (GTK_CONTAINER (self->stack), widget);
}

static void
ide_genesis_perspective_addin_removed (PeasExtensionSet *set,
                                       PeasPluginInfo   *plugin_info,
                                       PeasExtension    *exten,
                                       gpointer          user_data)
{
  IdeGenesisPerspective *self = user_data;
  GtkWidget *widget;
  GList *children;
  GList *iter;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_GENESIS_ADDIN (exten));
  g_assert (IDE_IS_GENESIS_PERSPECTIVE (self));

  children = gtk_container_get_children (GTK_CONTAINER (self->list_box));

  for (iter = children; iter; iter = iter->next)
    {
      gpointer data = g_object_get_data (iter->data, "IDE_GENESIS_ADDIN");

      if (data == NULL)
        continue;

      if (data == (gpointer)exten)
        {
          gtk_container_remove (GTK_CONTAINER (self->list_box), iter->data);
          break;
        }
    }

  g_list_free (children);

  widget = ide_genesis_addin_get_widget (IDE_GENESIS_ADDIN (exten));
  if (widget != NULL)
    gtk_container_remove (GTK_CONTAINER (self->stack), widget);
}

static void
ide_genesis_perspective_row_activated (IdeGenesisPerspective *self,
                                       GtkListBoxRow         *row,
                                       GtkListBox            *list_box)
{
  IdeGenesisAddin *addin;
  GtkWidget *child;
  GBinding *binding;

  g_assert (GTK_IS_LIST_BOX (list_box));
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (IDE_IS_GENESIS_PERSPECTIVE (self));

  addin = g_object_get_data (G_OBJECT (row), "IDE_GENESIS_ADDIN");
  if (addin == NULL)
    return;

  child = ide_genesis_addin_get_widget (addin);
  if (child == NULL)
    return;

  binding = g_object_bind_property (addin, "is-ready",
                                    self->continue_button, "sensitive",
                                    G_BINDING_SYNC_CREATE);
  ide_set_weak_pointer (&self->continue_binding, binding);

  gtk_widget_show (GTK_WIDGET (self->continue_button));
  gtk_header_bar_set_show_close_button (self->header_bar, FALSE);

  gtk_stack_set_visible_child (self->stack, child);

  self->current_addin = addin;
}

static void
ide_genesis_perspective_run_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  IdeGenesisAddin *addin = (IdeGenesisAddin *)object;
  g_autoptr(IdeGenesisPerspective) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_GENESIS_ADDIN (addin));
  g_assert (IDE_IS_GENESIS_PERSPECTIVE (self));

  if (!ide_genesis_addin_run_finish (addin, result, &error))
    {
      GtkWidget *dialog;

      dialog = gtk_message_dialog_new (NULL,
                                       GTK_DIALOG_USE_HEADER_BAR,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE,
                                       _("Failed to load the project"));
      g_object_set (dialog,
                    "secondary-text", error->message,
                    NULL);

      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);

      /*
       * TODO: Destroy workbench.
       */
    }
}

static void
ide_genesis_perspective_continue_clicked (IdeGenesisPerspective *self,
                                          GtkButton             *button)
{
  g_assert (IDE_IS_GENESIS_PERSPECTIVE (self));
  g_assert (GTK_IS_BUTTON (button));
  g_assert (self->current_addin != NULL);

  ide_genesis_addin_run_async (self->current_addin,
                               NULL,
                               ide_genesis_perspective_run_cb,
                               g_object_ref (self));
}

static void
ide_genesis_perspective_constructed (GObject *object)
{
  IdeGenesisPerspective *self = (IdeGenesisPerspective *)object;

  G_OBJECT_CLASS (ide_genesis_perspective_parent_class)->constructed (object);

  self->addins = peas_extension_set_new (peas_engine_get_default (),
                                         IDE_TYPE_GENESIS_ADDIN,
                                         NULL);

  peas_extension_set_foreach (self->addins, ide_genesis_perspective_addin_added, self);

  g_signal_connect (self->addins,
                    "extension-added",
                    G_CALLBACK (ide_genesis_perspective_addin_added),
                    self);

  g_signal_connect (self->addins,
                    "extension-removed",
                    G_CALLBACK (ide_genesis_perspective_addin_removed),
                    self);

  g_signal_connect_object (self->continue_button,
                           "clicked",
                           G_CALLBACK (ide_genesis_perspective_continue_clicked),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
ide_genesis_perspective_destroy (GtkWidget *widget)
{
  IdeGenesisPerspective *self = (IdeGenesisPerspective *)widget;

  g_clear_object (&self->actions);
  g_clear_object (&self->addins);

  GTK_WIDGET_CLASS (ide_genesis_perspective_parent_class)->destroy (widget);
}

static void
ide_genesis_perspective_class_init (IdeGenesisPerspectiveClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = ide_genesis_perspective_constructed;

  widget_class->destroy = ide_genesis_perspective_destroy;

  gtk_widget_class_set_css_name (widget_class, "genesisperspective");
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-genesis-perspective.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeGenesisPerspective, cancel_button);
  gtk_widget_class_bind_template_child (widget_class, IdeGenesisPerspective, continue_button);
  gtk_widget_class_bind_template_child (widget_class, IdeGenesisPerspective, header_bar);
  gtk_widget_class_bind_template_child (widget_class, IdeGenesisPerspective, list_box);
  gtk_widget_class_bind_template_child (widget_class, IdeGenesisPerspective, main_page);
  gtk_widget_class_bind_template_child (widget_class, IdeGenesisPerspective, stack);
}

static void
ide_genesis_perspective_init (IdeGenesisPerspective *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self->list_box,
                           "row-activated",
                           G_CALLBACK (ide_genesis_perspective_row_activated),
                           self,
                           G_CONNECT_SWAPPED);
}

static gchar *
ide_genesis_perspective_get_id (IdePerspective *perspective)
{
  return g_strdup ("genesis");
}

static gboolean
ide_genesis_perspective_is_early (IdePerspective *perspective)
{
  return TRUE;
}

static GtkWidget *
ide_genesis_perspective_get_titlebar (IdePerspective *perspective)
{
  g_assert (IDE_IS_GENESIS_PERSPECTIVE (perspective));

  return GTK_WIDGET (IDE_GENESIS_PERSPECTIVE (perspective)->header_bar);
}

static void
go_previous (GSimpleAction *action,
             GVariant      *variant,
             gpointer       user_data)
{
  IdeGenesisPerspective *self = user_data;
  IdeWorkbench *workbench;
  GtkWidget *visible_child;

  g_assert (IDE_IS_GENESIS_PERSPECTIVE (self));

  if (self->continue_binding)
    {
      g_binding_unbind (self->continue_binding);
      ide_clear_weak_pointer (&self->continue_binding);
    }

  gtk_widget_hide (GTK_WIDGET (self->continue_button));
  gtk_header_bar_set_show_close_button (self->header_bar, TRUE);

  visible_child = gtk_stack_get_visible_child (self->stack);

  if (visible_child != self->main_page)
    {
      gtk_stack_set_visible_child (self->stack, self->main_page);
      return;
    }

  workbench = ide_widget_get_workbench (GTK_WIDGET (self));
  ide_workbench_set_visible_perspective_name (workbench, "greeter");
}

static GActionGroup *
ide_genesis_perspective_get_actions (IdePerspective *perspective)
{
  IdeGenesisPerspective *self = (IdeGenesisPerspective *)perspective;

  g_assert (IDE_IS_GENESIS_PERSPECTIVE (self));

  if (self->actions == NULL)
    {
      const GActionEntry entries[] = {
        { "go-previous", go_previous },
      };

      self->actions = G_ACTION_GROUP (g_simple_action_group_new ());
      g_action_map_add_action_entries (G_ACTION_MAP (self->actions),
                                       entries, G_N_ELEMENTS (entries), self);
    }

  g_assert (G_IS_ACTION_GROUP (self->actions));

  return self->actions;
}

static void
perspective_iface_init (IdePerspectiveInterface *iface)
{
  iface->get_id = ide_genesis_perspective_get_id;
  iface->is_early = ide_genesis_perspective_is_early;
  iface->get_titlebar = ide_genesis_perspective_get_titlebar;
  iface->get_actions = ide_genesis_perspective_get_actions;
}
