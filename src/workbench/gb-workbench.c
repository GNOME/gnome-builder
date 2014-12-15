/* gb-workbench.c
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

#define G_LOG_DOMAIN "workbench"

#include <glib/gi18n.h>
#include <libgit2-glib/ggit.h>

#include "gb-command-bar.h"
#include "gb-command-gaction-provider.h"
#include "gb-command-manager.h"
#include "gb-command-vim-provider.h"
#include "gb-close-confirmation-dialog.h"
#include "gb-credits-widget.h"
#include "gb-document-manager.h"
#include "gb-editor-workspace.h"
#include "gb-git-search-provider.h"
#include "gb-glib.h"
#include "gb-log.h"
#include "gb-search-box.h"
#include "gb-search-manager.h"
#include "gb-widget.h"
#include "gb-workbench.h"
#include "gedit-menu-stack-switcher.h"

struct _GbWorkbenchPrivate
{
  GbCommandManager       *command_manager;
  GbDocumentManager      *document_manager;
  GbNavigationList       *navigation_list;
  GbSearchManager        *search_manager;

  guint                   search_timeout;

  GbWorkspace            *active_workspace;
  GbCommandBar           *command_bar;
  GbCreditsWidget        *credits;
  GbWorkspace            *editor;
  GeditMenuStackSwitcher *gear_menu_button;
  GtkHeaderBar           *header_bar;
  GtkButton              *run_button;
  GbSearchBox            *search_box;
  GtkStack               *stack;
};

typedef struct
{
  GCancellable *cancellable;
  gint          outstanding;
} SavedState;

enum {
  PROP_0,
  PROP_COMMAND_MANAGER,
  PROP_NAVIGATION_LIST,
  LAST_PROP
};

enum {
  WORKSPACE_CHANGED,
  LAST_SIGNAL
};

G_DEFINE_TYPE_WITH_PRIVATE (GbWorkbench, gb_workbench,
                            GTK_TYPE_APPLICATION_WINDOW)

static GParamSpec *gParamSpecs [LAST_PROP];
static guint       gSignals [LAST_SIGNAL];

/**
 * gb_workbench_get_command_manager:
 *
 * Retrieves the command manager for the workspace.
 *
 * Returns: (transfer none) (type GbCommandManager*): A #GbCommandManager.
 */
GbCommandManager *
gb_workbench_get_command_manager (GbWorkbench *workbench)
{
  g_return_val_if_fail (GB_IS_WORKBENCH (workbench), NULL);

  return workbench->priv->command_manager;
}

/**
 * gb_workbench_get_document_manager:
 * @workbench: A #GbWorkbench
 *
 * Retrieves the document manager for the workbench.
 *
 * Returns: (transfer none): A #GbDocumentManager.
 */
GbDocumentManager *
gb_workbench_get_document_manager (GbWorkbench *workbench)
{
  g_return_val_if_fail (GB_IS_WORKBENCH (workbench), NULL);

  return workbench->priv->document_manager;
}

/**
 * gb_workbench_get_navigation_list:
 *
 * Fetches the navigation list for the workbench. This can be used to move
 * between edit points between workspaces.
 *
 * Returns: (transfer none): A #GbNavigationlist.
 */
GbNavigationList *
gb_workbench_get_navigation_list (GbWorkbench *workbench)
{
  g_return_val_if_fail (GB_IS_WORKBENCH (workbench), NULL);

  return workbench->priv->navigation_list;
}

GbSearchManager *
gb_workbench_get_search_manager (GbWorkbench *workbench)
{
  GbWorkbenchPrivate *priv;

  g_return_val_if_fail (GB_IS_WORKBENCH (workbench), NULL);

  priv = workbench->priv;

  if (!priv->search_manager)
    {
      GbSearchProvider *provider;
      GgitRepository *repository;
      GFile *file;

      priv->search_manager = gb_search_manager_new ();

      /* TODO: Keep repository in sync with loaded project */
      file = g_file_new_for_path (".");
      repository = ggit_repository_open (file, NULL);
      provider = g_object_new (GB_TYPE_GIT_SEARCH_PROVIDER,
                               "repository", repository,
                               NULL);
      gb_search_manager_add_provider (priv->search_manager, provider);
      g_clear_object (&file);
      g_clear_object (&repository);
      g_clear_object (&provider);
    }

  return priv->search_manager;
}

/**
 * gb_workbench_get_active_workspace:
 *
 * Retrieves the active workspace.
 *
 * Returns: (transfer none): A #GbWorkbench.
 */
GbWorkspace *
gb_workbench_get_active_workspace (GbWorkbench *workbench)
{
   GtkWidget *child;

   g_return_val_if_fail (GB_IS_WORKBENCH (workbench), NULL);

   child = gtk_stack_get_visible_child (workbench->priv->stack);

   return GB_WORKSPACE (child);
}

/**
 * gb_workbench_get_workspace:
 * @type: A #GType descending from #GbWorkspace
 *
 * Retrieves the workspace of type @type.
 *
 * Returns: (transfer none) (nullable): A #GbWorkspace or %NULL.
 */
GbWorkspace *
gb_workbench_get_workspace (GbWorkbench *workbench,
                            GType        type)
{
  g_return_val_if_fail (GB_IS_WORKBENCH (workbench), NULL);
  g_return_val_if_fail (g_type_is_a (type, GB_TYPE_WORKSPACE), NULL);

  if (type == GB_TYPE_EDITOR_WORKSPACE)
    return GB_WORKSPACE (workbench->priv->editor);

  return NULL;
}

static void
gb_workbench_roll_credits (GbWorkbench *workbench)
{
  g_return_if_fail (GB_IS_WORKBENCH (workbench));

  gb_credits_widget_start (workbench->priv->credits);
}

static void
gb_workbench_workspace_changed (GbWorkbench *workbench,
                                GbWorkspace *workspace)
{
  GbWorkbenchPrivate *priv;

  ENTRY;

  g_return_if_fail (GB_IS_WORKBENCH (workbench));
  g_return_if_fail (GB_IS_WORKSPACE (workspace));

  priv = workbench->priv;

  gb_clear_weak_pointer (&priv->active_workspace);

  if (workspace)
    {
      gb_set_weak_pointer (workspace, &priv->active_workspace);
      gtk_widget_grab_focus (GTK_WIDGET (workspace));
    }

  EXIT;
}

static void
gb_workbench_stack_child_changed (GbWorkbench *workbench,
                                  GParamSpec  *pspec,
                                  GtkStack    *stack)
{
  GtkWidget *child;

  g_return_if_fail (GB_IS_WORKBENCH (workbench));
  g_return_if_fail (GTK_IS_STACK (stack));

  child = gtk_stack_get_visible_child (stack);
  g_assert (!child || GB_IS_WORKSPACE (child));

  if (GB_IS_WORKSPACE (child))
    {
      GActionGroup *action_group;

      /*
       * Some actions need to be propagated from the workspace to the
       * toplevel. This way the header bar can activate them.
       */
      action_group = gtk_widget_get_action_group (child, "workspace");
      gtk_widget_insert_action_group (GTK_WIDGET (workbench),
                                      "workspace", action_group);
    }

  if (child)
    g_signal_emit (workbench, gSignals[WORKSPACE_CHANGED], 0, child);
}

static void
gb_workbench_realize (GtkWidget *widget)
{
  GbWorkbench *workbench = (GbWorkbench *)widget;

  if (GTK_WIDGET_CLASS (gb_workbench_parent_class)->realize)
    GTK_WIDGET_CLASS (gb_workbench_parent_class)->realize (widget);

  gtk_widget_grab_focus (GTK_WIDGET (workbench->priv->editor));
}

static void
gb_workbench_action_go_forward (GSimpleAction *action,
                                GVariant      *variant,
                                gpointer       user_data)
{
  GbWorkbench *workbench = user_data;

  g_return_if_fail (GB_IS_WORKBENCH (workbench));

  if (gb_navigation_list_get_can_go_forward (workbench->priv->navigation_list))
    gb_navigation_list_go_forward (workbench->priv->navigation_list);
}

static void
gb_workbench_action_go_backward (GSimpleAction *action,
                                 GVariant      *variant,
                                 gpointer       user_data)
{
  GbWorkbench *workbench = user_data;

  g_return_if_fail (GB_IS_WORKBENCH (workbench));

  if (gb_navigation_list_get_can_go_backward (workbench->priv->navigation_list))
    gb_navigation_list_go_backward (workbench->priv->navigation_list);
}

static void
gb_workbench_action_toggle_command_bar (GSimpleAction *action,
                                        GVariant      *parameters,
                                        gpointer       user_data)
{
  GbWorkbench *workbench = user_data;
  gboolean show = TRUE;

  g_return_if_fail (GB_IS_WORKBENCH (workbench));

  show = g_variant_get_boolean (parameters);

  if (show)
    gb_command_bar_show (workbench->priv->command_bar);
  else
    gb_command_bar_hide (workbench->priv->command_bar);
}

static void
gb_workbench_action_show_command_bar (GSimpleAction *action,
                                      GVariant      *parameters,
                                      gpointer       user_data)
{
  GVariant *b;

  g_return_if_fail (GB_IS_WORKBENCH (user_data));

  b = g_variant_new_boolean (TRUE);
  gb_workbench_action_toggle_command_bar (NULL, b, user_data);
  g_variant_unref (b);
}

static void
gb_workbench_action_global_search (GSimpleAction *action,
                                   GVariant      *parameters,
                                   gpointer       user_data)
{
  GbWorkbench *workbench = user_data;

  g_return_if_fail (GB_IS_WORKBENCH (workbench));

  gtk_widget_grab_focus (GTK_WIDGET (workbench->priv->search_box));
}

static void
gb_workbench_action_roll_credits (GSimpleAction *action,
                                  GVariant      *parameters,
                                  gpointer       user_data)
{
  GbWorkbench *workbench = user_data;

  g_return_if_fail (GB_IS_WORKBENCH (workbench));

  gb_workbench_roll_credits (workbench);
}

static void
gb_workbench_action_save_all (GSimpleAction *action,
                              GVariant      *parameters,
                              gpointer       user_data)
{
  GbWorkbench *workbench = user_data;
  GList *list;
  GList *iter;

  g_return_if_fail (GB_IS_WORKBENCH (workbench));

  list = gb_document_manager_get_documents (workbench->priv->document_manager);

  for (iter = list; iter; iter = iter->next)
    {
      GbDocument *document = GB_DOCUMENT (iter->data);

      /* This will not save files which do not have location set */
      if (gb_document_get_modified (document))
        gb_document_save_async (document, NULL, NULL, NULL);
    }

  g_list_free (list);
}

static void
on_command_bar_notify_child_revealed (GbCommandBar *command_bar,
                                      GParamSpec   *pspec,
                                      GbWorkbench  *workbench)
{
  gboolean reveal_child;

  g_return_if_fail (GB_IS_COMMAND_BAR (command_bar));
  g_return_if_fail (GB_IS_WORKBENCH (workbench));

  reveal_child = gtk_revealer_get_reveal_child (GTK_REVEALER (command_bar));

  if (!reveal_child && workbench->priv->active_workspace)
    gtk_widget_grab_focus (GTK_WIDGET (workbench->priv->active_workspace));
}

static void
gb_workbench_navigation_changed (GbWorkbench      *workbench,
                                 GParamSpec       *pspec,
                                 GbNavigationList *list)
{
  GbWorkbenchPrivate *priv;
  GbNavigationItem *item;
  GbWorkspace *workspace;

  g_return_if_fail (GB_IS_WORKBENCH (workbench));
  g_return_if_fail (GB_IS_NAVIGATION_LIST (list));

  priv = workbench->priv;

  item = gb_navigation_list_get_current_item (list);

  if (item)
    {
      workspace = gb_navigation_item_get_workspace (item);
      if (workspace)
        gtk_stack_set_visible_child (priv->stack, GTK_WIDGET (workspace));
      gb_navigation_item_activate (item);
    }
}

static void
gb_workbench_save_cb (GObject      *object,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  SavedState *state = user_data;

  GbDocument *document = (GbDocument *)object;

  gb_document_save_finish (document, result, NULL);

  state->outstanding--;
}

static void
gb_workbench_save_as_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  SavedState *state = user_data;

  GbDocument *document = (GbDocument *)object;

  gb_document_save_as_finish (document, result, NULL);

  state->outstanding--;
}

static void
gb_workbench_begin_save (GbWorkbench *workbench,
                         GbDocument  *document,
                         SavedState  *state)
{
  g_return_if_fail (GB_IS_WORKBENCH (workbench));
  g_return_if_fail (GB_IS_DOCUMENT (document));
  g_return_if_fail (state);

  state->outstanding++;

  gb_document_save_async (document,
                          state->cancellable,
                          gb_workbench_save_cb,
                          state);
}

static void
gb_workbench_begin_save_as (GbWorkbench *workbench,
                            GbDocument  *document,
                            SavedState  *state)
{
  g_return_if_fail (GB_IS_WORKBENCH (workbench));
  g_return_if_fail (GB_IS_DOCUMENT (document));
  g_return_if_fail (state);

  state->outstanding++;

  gb_document_save_as_async (document,
                             GTK_WIDGET (workbench),
                             state->cancellable,
                             gb_workbench_save_as_cb,
                             state);
}

static void
gb_workbench_wait_for_saved (GbWorkbench *workbench,
                             GtkDialog   *dialog,
                             SavedState  *state)
{
  g_return_if_fail (GB_IS_WORKBENCH (workbench));
  g_return_if_fail (GTK_IS_DIALOG (dialog));
  g_return_if_fail (state);

  gtk_widget_set_sensitive (GTK_WIDGET (dialog), FALSE);
  while (state->outstanding)
    gtk_main_iteration_do (TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (dialog), TRUE);
}

static gboolean
gb_workbench_confirm_close (GbWorkbench *workbench)
{
  GbDocumentManager *document_manager;
  gboolean ret = FALSE;
  GList *unsaved = NULL;

  g_return_val_if_fail (GB_IS_WORKBENCH (workbench), FALSE);

  document_manager = gb_workbench_get_document_manager (workbench);
  unsaved = gb_document_manager_get_unsaved_documents (document_manager);

  if (unsaved)
    {
      GbCloseConfirmationDialog *close;
      SavedState state = { 0 };
      GtkWidget *dialog;
      GList *selected;
      GList *iter;
      gint response_id;

      dialog = gb_close_confirmation_dialog_new (GTK_WINDOW (workbench),
                                                 unsaved);
      close = GB_CLOSE_CONFIRMATION_DIALOG (dialog);
      response_id = gtk_dialog_run (GTK_DIALOG (dialog));
      selected = gb_close_confirmation_dialog_get_selected_documents (close);

      switch (response_id)
        {
        case GTK_RESPONSE_YES:
          state.cancellable = g_cancellable_new ();

          for (iter = selected; iter; iter = iter->next)
            {
              GbDocument *document = GB_DOCUMENT (iter->data);

              if (gb_document_is_untitled (document))
                gb_workbench_begin_save_as (workbench, document, &state);
              else
                gb_workbench_begin_save (workbench, document, &state);
            }

          gb_workbench_wait_for_saved (workbench, GTK_DIALOG (dialog), &state);
          g_clear_object (&state.cancellable);
          break;

        case GTK_RESPONSE_NO:
          break;

        case GTK_RESPONSE_DELETE_EVENT:
        case GTK_RESPONSE_CANCEL:
          ret = TRUE;
          break;

        default:
          g_assert_not_reached ();
        }

      g_list_free (selected);
      gtk_widget_hide (dialog);
      gtk_widget_destroy (dialog);
    }

  g_list_free (unsaved);

  return ret;
}

static gboolean
gb_workbench_delete_event (GtkWidget   *widget,
                           GdkEventAny *event)
{
  GbWorkbench *workbench = (GbWorkbench *)widget;

  g_return_val_if_fail (GB_IS_WORKBENCH (workbench), FALSE);

  if (!gb_workbench_confirm_close (workbench))
    {
      if (GTK_WIDGET_CLASS (gb_workbench_parent_class)->delete_event)
        return GTK_WIDGET_CLASS (gb_workbench_parent_class)->delete_event (widget, event);
      return FALSE;
    }

  return TRUE;
}

static void
gb_workbench_add_command_provider (GbWorkbench *workbench,
                                   GType        type)
{
  GbCommandProvider *provider;

  g_return_if_fail (GB_IS_WORKBENCH (workbench));
  g_return_if_fail (g_type_is_a (type, GB_TYPE_COMMAND_PROVIDER));

  provider = g_object_new (type, "workbench", workbench, NULL);
  gb_command_manager_add_provider (workbench->priv->command_manager, provider);
}

static void
gb_workbench_set_focus (GtkWindow *window,
                        GtkWidget *widget)
{
  GbWorkbench *workbench = (GbWorkbench *)window;

  g_return_if_fail (GB_IS_WORKBENCH (workbench));

  /*
   * The goal here is to focus the current workspace if we are trying to
   * clear the workbench focus (from something like the global search).
   */

  GTK_WINDOW_CLASS (gb_workbench_parent_class)->set_focus (window, widget);

  if (!widget)
    {
      GbWorkspace *workspace;

      /*
       * Sadly we can't just set @widget before calling the parent set_focus()
       * implementation. It doesn't actually do anything. So instead we grab
       * the focus of the active workspace directly. We might need to check
       * for reentrancy later, but if that happens, we are probably doing
       * something else wrong.
       */
      workspace = gb_workbench_get_active_workspace (workbench);
      if (workspace)
        gtk_widget_grab_focus (GTK_WIDGET (workspace));
    }
}

static void
gb_workbench_constructed (GObject *object)
{
  static const GActionEntry actions[] = {
    { "global-search",      gb_workbench_action_global_search },
    { "go-backward",        gb_workbench_action_go_backward },
    { "go-forward",         gb_workbench_action_go_forward },
    { "show-command-bar",   gb_workbench_action_show_command_bar },
    { "toggle-command-bar", gb_workbench_action_toggle_command_bar, "b" },
    { "save-all",           gb_workbench_action_save_all },
    { "about",              gb_workbench_action_roll_credits },
  };
  GbWorkbenchPrivate *priv;
  GbWorkbench *workbench = (GbWorkbench *)object;
  GbSearchManager *search_manager;
  GtkApplication *app;
  GAction *action;
  GMenu *menu;

  g_assert (GB_IS_WORKBENCH (workbench));

  ENTRY;

  priv = workbench->priv;

  G_OBJECT_CLASS (gb_workbench_parent_class)->constructed (object);

  app = GTK_APPLICATION (g_application_get_default ());
  menu = gtk_application_get_menu_by_id (app, "gear-menu");
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (priv->gear_menu_button),
                                  G_MENU_MODEL (menu));

  g_signal_connect_object (priv->stack,
                           "notify::visible-child",
                           G_CALLBACK (gb_workbench_stack_child_changed),
                           workbench,
                           (G_CONNECT_SWAPPED | G_CONNECT_AFTER));

  g_action_map_add_action_entries (G_ACTION_MAP (workbench), actions,
                                   G_N_ELEMENTS (actions), workbench);

  g_signal_connect_object (priv->navigation_list,
                           "notify::current-item",
                           G_CALLBACK (gb_workbench_navigation_changed),
                           workbench,
                           G_CONNECT_SWAPPED);

  action = g_action_map_lookup_action (G_ACTION_MAP (workbench), "go-backward");
  g_object_bind_property (priv->navigation_list, "can-go-backward",
                          action, "enabled", G_BINDING_SYNC_CREATE);

  action = g_action_map_lookup_action (G_ACTION_MAP (workbench), "go-forward");
  g_object_bind_property (priv->navigation_list, "can-go-forward",
                          action, "enabled", G_BINDING_SYNC_CREATE);

  g_signal_connect (priv->command_bar, "notify::child-revealed",
                    G_CALLBACK (on_command_bar_notify_child_revealed),
                    workbench);

  search_manager = gb_workbench_get_search_manager (workbench);
  gb_search_box_set_search_manager (workbench->priv->search_box,
                                    search_manager);

  gb_workbench_stack_child_changed (workbench, NULL, priv->stack);

  EXIT;
}

static void
gb_workbench_dispose (GObject *object)
{
  GbWorkbenchPrivate *priv = GB_WORKBENCH (object)->priv;

  ENTRY;

  if (priv->search_timeout)
    {
      g_source_remove (priv->search_timeout);
      priv->search_timeout = 0;
    }

  g_clear_object (&priv->command_manager);
  g_clear_object (&priv->document_manager);
  g_clear_object (&priv->navigation_list);
  g_clear_object (&priv->search_manager);

  G_OBJECT_CLASS (gb_workbench_parent_class)->dispose (object);

  EXIT;
}

static void
gb_workbench_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GbWorkbench *self = (GbWorkbench *)object;

  switch (prop_id)
    {
    case PROP_COMMAND_MANAGER:
      g_value_set_object (value, gb_workbench_get_command_manager (self));
      break;

    case PROP_NAVIGATION_LIST:
      g_value_set_object (value, gb_workbench_get_navigation_list (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_workbench_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_workbench_class_init (GbWorkbenchClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkWindowClass *window_class = GTK_WINDOW_CLASS (klass);

  object_class->constructed = gb_workbench_constructed;
  object_class->dispose = gb_workbench_dispose;
  object_class->get_property = gb_workbench_get_property;
  object_class->set_property = gb_workbench_set_property;

  widget_class->realize = gb_workbench_realize;
  widget_class->delete_event = gb_workbench_delete_event;

  window_class->set_focus = gb_workbench_set_focus;

  klass->workspace_changed = gb_workbench_workspace_changed;

  gParamSpecs [PROP_COMMAND_MANAGER] =
    g_param_spec_object ("command-manager",
                         _("Command Manager"),
                         _("The command manager for the workspace."),
                         GB_TYPE_COMMAND_MANAGER,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_COMMAND_MANAGER,
                                   gParamSpecs [PROP_COMMAND_MANAGER]);

  gParamSpecs [PROP_NAVIGATION_LIST] =
    g_param_spec_object ("navigation-list",
                         _("Navigation List"),
                         _("The navigation list for the workbench."),
                         GB_TYPE_NAVIGATION_LIST,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_NAVIGATION_LIST,
                                   gParamSpecs [PROP_NAVIGATION_LIST]);

  gSignals [WORKSPACE_CHANGED] =
    g_signal_new ("workspace-changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GbWorkbenchClass, workspace_changed),
                  NULL,
                  NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1,
                  GB_TYPE_WORKSPACE);

  GB_WIDGET_CLASS_TEMPLATE (klass, "gb-workbench.ui");
  GB_WIDGET_CLASS_BIND (klass, GbWorkbench, command_bar);
  GB_WIDGET_CLASS_BIND (klass, GbWorkbench, editor);
  GB_WIDGET_CLASS_BIND (klass, GbWorkbench, gear_menu_button);
  GB_WIDGET_CLASS_BIND (klass, GbWorkbench, header_bar);
  GB_WIDGET_CLASS_BIND (klass, GbWorkbench, run_button);
  GB_WIDGET_CLASS_BIND (klass, GbWorkbench, search_box);
  GB_WIDGET_CLASS_BIND (klass, GbWorkbench, stack);

  g_type_ensure (GB_TYPE_COMMAND_BAR);
  g_type_ensure (GB_TYPE_CREDITS_WIDGET);
  g_type_ensure (GB_TYPE_EDITOR_WORKSPACE);
  g_type_ensure (GB_TYPE_SEARCH_BOX);
  g_type_ensure (GEDIT_TYPE_MENU_STACK_SWITCHER);
}

static void
gb_workbench_init (GbWorkbench *workbench)
{
  workbench->priv = gb_workbench_get_instance_private (workbench);

  workbench->priv->document_manager = gb_document_manager_new ();
  workbench->priv->command_manager = gb_command_manager_new ();
  workbench->priv->navigation_list = gb_navigation_list_new (workbench);

  gtk_widget_init_template (GTK_WIDGET (workbench));

  gb_workbench_add_command_provider (workbench,
                                     GB_TYPE_COMMAND_GACTION_PROVIDER);
  gb_workbench_add_command_provider (workbench,
                                     GB_TYPE_COMMAND_VIM_PROVIDER);
}
