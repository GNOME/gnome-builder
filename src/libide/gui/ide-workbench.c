/* ide-workbench.c
 *
 * Copyright 2014-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-workbench"

#include "config.h"

#include <glib/gi18n.h>

#include <libpeas.h>

#include <libide-debugger.h>
#include <libide-gtk.h>
#include <libide-threading.h>
#include <libide-tweaks.h>

#include "ide-build-private.h"
#include "ide-context-private.h"
#include "ide-foundry-init.h"
#include "ide-thread-private.h"
#include "ide-transfer-manager-private.h"

#include "ide-application.h"
#include "ide-application-private.h"
#include "ide-application-tweaks.h"
#include "ide-gui-global.h"
#include "ide-primary-workspace.h"
#include "ide-shortcut-manager-private.h"
#include "ide-workbench-addin.h"
#include "ide-workbench-private.h"
#include "ide-workspace-private.h"

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GActionGroup, g_object_unref)

/**
 * SECTION:ide-workbench
 * @title: IdeWorkbench
 * @short_description: window group for all windows within a project
 *
 * The #IdeWorkbench is a #GtkWindowGroup containing the #IdeContext (root
 * data-structure for a project) and all of the windows associated with the
 * project.
 *
 * Usually, windows within the #IdeWorkbench are an #IdeWorkspace. They can
 * react to changes in the #IdeContext or its descendants to represent the
 * project and it's state.
 */

struct _IdeWorkbench
{
  GtkWindowGroup    parent_instance;

  /* MRU of workspaces, link embedded in workspace */
  GQueue            mru_queue;

  /* Owned references */
  PeasExtensionSet    *addins;
  GCancellable        *cancellable;
  IdeContext          *context;
  IdeBuildSystem      *build_system;
  IdeProjectInfo      *project_info;
  IdeVcs              *vcs;
  IdeVcsMonitor       *vcs_monitor;
  IdeSearchEngine     *search_engine;
  IdeSession          *session;

  /* Various flags */
  guint                unloaded : 1;
};

typedef struct
{
  GPtrArray          *addins;
  IdeWorkbenchAddin  *preferred;
  GFile              *file;
  gchar              *hint;
  gchar              *content_type;
  PanelPosition      *position;
  IdeBufferOpenFlags  flags;
  gint                at_line;
  gint                at_line_offset;
} Open;

typedef struct
{
  IdeProjectInfo *project_info;
  GPtrArray      *addins;
  GType           workspace_type;
  gint64          present_time;
} LoadProject;

typedef struct
{
  GPtrArray *roots;
  gchar     *path;
} ResolveFile;

enum {
  PROP_0,
  PROP_BUILD_SYSTEM,
  PROP_CONFIG,
  PROP_CONTEXT,
  PROP_VCS,
  N_PROPS
};

static void ide_workbench_action_add_workspace (gpointer      instance,
                                                const char   *action_name,
                                                GVariant     *param);
static void ide_workbench_action_close         (gpointer      instance,
                                                const char   *action_name,
                                                GVariant     *param);
static void ide_workbench_action_open          (gpointer      instance,
                                                const char   *action_name,
                                                GVariant     *param);
static void ide_workbench_action_open_uri      (gpointer      instance,
                                                const char   *action_name,
                                                GVariant     *param);
static void ide_workbench_action_dump_tasks    (gpointer      instance,
                                                const char   *action_name,
                                                GVariant     *param);
static void ide_workbench_action_object_tree   (gpointer      instance,
                                                const char   *action_name,
                                                GVariant     *param);
static void ide_workbench_action_inspector     (gpointer      instance,
                                                const char   *action_name,
                                                GVariant     *param);
static void ide_workbench_action_reload_all    (gpointer      instance,
                                                const char   *action_name,
                                                GVariant     *param);
static void ide_workbench_action_global_search (gpointer      instance,
                                                const char   *action_name,
                                                GVariant     *param);
static void ide_workbench_action_configure     (gpointer      instance,
                                                const char   *action_name,
                                                GVariant     *param);
static void ide_workbench_action_tweaks        (gpointer      instance,
                                                const char   *action_name,
                                                GVariant     *param);

G_DEFINE_FINAL_TYPE (IdeWorkbench, ide_workbench, GTK_TYPE_WINDOW_GROUP)

static GParamSpec *properties [N_PROPS];
static IdeActionMixin action_mixin;

static void
load_project_free (LoadProject *lp)
{
  g_clear_object (&lp->project_info);
  g_clear_pointer (&lp->addins, g_ptr_array_unref);
  g_slice_free (LoadProject, lp);
}

static void
open_free (Open *o)
{
  g_clear_pointer (&o->addins, g_ptr_array_unref);
  g_clear_object (&o->position);
  g_clear_object (&o->preferred);
  g_clear_object (&o->file);
  g_clear_pointer (&o->hint, g_free);
  g_clear_pointer (&o->content_type, g_free);
  g_slice_free (Open, o);
}

static void
resolve_file_free (ResolveFile *rf)
{
  g_clear_pointer (&rf->roots, g_ptr_array_unref);
  g_clear_pointer (&rf->path, g_free);
  g_slice_free (ResolveFile, rf);
}

static gboolean
ignore_error (GError *error)
{
  return g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) ||
         g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
}

static IdeConfig *
ide_workbench_get_config (IdeWorkbench *self)
{
  IdeConfigManager *config_manager;

  g_return_val_if_fail (IDE_IS_WORKBENCH (self), NULL);

  if ((config_manager = ide_context_peek_child_typed (self->context, IDE_TYPE_CONFIG_MANAGER)))
    return ide_config_manager_get_current (config_manager);

  return NULL;
}

/**
 * ide_workbench_from_context:
 * @context: an #IdeContext
 *
 * Helper to get the #IdeWorkbench for a given context.
 *
 * Returns: (transfer none) (nullable): an #IdeWorkbench or %NULL
 */
IdeWorkbench *
ide_workbench_from_context (IdeContext *context)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  return IDE_WORKBENCH (g_object_get_data (G_OBJECT (context), "WORKBENCH"));
}

static void
ide_workbench_set_context (IdeWorkbench *self,
                           IdeContext   *context)
{
  g_autoptr(IdeContext) new_context = NULL;
  g_autoptr(IdeBufferManager) bufmgr = NULL;
  IdeBuildSystem *build_system;

  g_return_if_fail (IDE_IS_WORKBENCH (self));
  g_return_if_fail (!context || IDE_IS_CONTEXT (context));

  if (context == NULL)
    context = new_context = ide_context_new ();

  /* backpointer for the workbench */
  g_object_set_data (G_OBJECT (context), "WORKBENCH", self);

  g_set_object (&self->context, context);

  /* Make sure we have access to buffer manager early */
  bufmgr = ide_object_ensure_child_typed (IDE_OBJECT (context), IDE_TYPE_BUFFER_MANAGER);

  /* And use a fallback build system if one is not already available */
  if ((build_system = ide_context_peek_child_typed (context, IDE_TYPE_BUILD_SYSTEM)))
    self->build_system = g_object_ref (build_system);
  else
    self->build_system = ide_object_ensure_child_typed (IDE_OBJECT (context), IDE_TYPE_FALLBACK_BUILD_SYSTEM);
}

static void
ide_workbench_addin_added_workspace_cb (IdeWorkspace      *workspace,
                                        IdeWorkbenchAddin *addin)
{
  g_assert (IDE_IS_WORKSPACE (workspace));
  g_assert (IDE_IS_WORKBENCH_ADDIN (addin));

  ide_workbench_addin_workspace_added (addin, workspace);
}

static void
ide_workbench_addin_removed_workspace_cb (IdeWorkspace      *workspace,
                                          IdeWorkbenchAddin *addin)
{
  g_assert (IDE_IS_WORKSPACE (workspace));
  g_assert (IDE_IS_WORKBENCH_ADDIN (addin));

  ide_workbench_addin_workspace_removed (addin, workspace);
}

static void
ide_workbench_addin_added_cb (PeasExtensionSet *set,
                              PeasPluginInfo   *plugin_info,
                              GObject    *exten,
                              gpointer          user_data)
{
  IdeWorkbench *self = user_data;
  IdeWorkbenchAddin *addin = (IdeWorkbenchAddin *)exten;
  g_autoptr(GActionGroup) action_group = NULL;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_WORKBENCH_ADDIN (addin));
  g_assert (IDE_IS_WORKBENCH (self));

  ide_workbench_addin_load (addin, self);

  if (!(action_group = ide_workbench_addin_ref_action_group (addin)) &&
      G_IS_ACTION_GROUP (addin))
    action_group = g_object_ref (G_ACTION_GROUP (addin));

  if (action_group != NULL)
    {
      IdeActionMuxer *muxer = ide_action_mixin_get_action_muxer (self);
      ide_action_muxer_insert_action_group (muxer,
                                            peas_plugin_info_get_module_name (plugin_info),
                                            action_group);
    }

  /* Notify of the VCS system up-front */
  if (self->vcs != NULL)
    ide_workbench_addin_vcs_changed (addin, self->vcs);

  /*
   * If we already loaded a project, then give the plugin a
   * chance to handle that, even if it is delayed a bit.
   */

  if (self->project_info != NULL)
    ide_workbench_addin_load_project_async (addin, self->project_info, NULL, NULL, NULL);

  ide_workbench_foreach_workspace (self,
                                   (IdeWorkspaceCallback)ide_workbench_addin_added_workspace_cb,
                                   addin);
}

static void
ide_workbench_addin_removed_cb (PeasExtensionSet *set,
                                PeasPluginInfo   *plugin_info,
                                GObject    *exten,
                                gpointer          user_data)
{
  IdeWorkbench *self = user_data;
  IdeWorkbenchAddin *addin = (IdeWorkbenchAddin *)exten;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_WORKBENCH_ADDIN (addin));
  g_assert (IDE_IS_WORKBENCH (self));

  ide_action_muxer_insert_action_group (ide_action_mixin_get_action_muxer (self),
                                        peas_plugin_info_get_module_name (plugin_info),
                                        NULL);

  /* Notify of workspace removals so addins don't need to manually
   * track them for cleanup.
   */
  ide_workbench_foreach_workspace (self,
                                   (IdeWorkspaceCallback)ide_workbench_addin_removed_workspace_cb,
                                   addin);

  ide_workbench_addin_unload (addin, self);
}

static void
ide_workbench_notify_context_title (IdeWorkbench *self,
                                    GParamSpec   *pspec,
                                    IdeContext   *context)
{
  g_autofree gchar *formatted = NULL;
  g_autofree gchar *title = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_WORKBENCH (self));
  g_assert (IDE_IS_CONTEXT (context));

  title = ide_context_dup_title (context);
  formatted = g_strdup_printf (_("Builder — %s"), title);
  ide_workbench_foreach_workspace (self,
                                   (IdeWorkspaceCallback)gtk_window_set_title,
                                   formatted);
}

static void
ide_workbench_notify_context_workdir (IdeWorkbench *self,
                                      GParamSpec   *pspec,
                                      IdeContext   *context)
{
  g_autoptr(GFile) workdir = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_WORKBENCH (self));
  g_assert (IDE_IS_CONTEXT (context));

  workdir = ide_context_ref_workdir (context);
  ide_vcs_monitor_set_root (self->vcs_monitor, workdir);
}

static void
ide_workbench_constructed (GObject *object)
{
  IdeWorkbench *self = (IdeWorkbench *)object;
  g_autoptr(IdeActionMuxer) muxer = NULL;
  IdeActionMuxer *our_muxer;

  g_assert (IDE_IS_WORKBENCH (self));

  if (self->context == NULL)
    self->context = ide_context_new ();

  g_signal_connect_object (self->context,
                           "notify::title",
                           G_CALLBACK (ide_workbench_notify_context_title),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->context,
                           "notify::workdir",
                           G_CALLBACK (ide_workbench_notify_context_workdir),
                           self,
                           G_CONNECT_SWAPPED);

  G_OBJECT_CLASS (ide_workbench_parent_class)->constructed (object);

  ide_action_mixin_constructed (&action_mixin, object);
  ide_action_mixin_set_enabled (self, "configure", FALSE);

  /* Add various action groups to context muxer */
  muxer = ide_context_ref_action_muxer (self->context);
  our_muxer = ide_action_mixin_get_action_muxer (self);
  ide_action_muxer_insert_action_group (muxer, "workbench", G_ACTION_GROUP (our_muxer));
  ide_action_muxer_insert_action_group (muxer, "transfer-manager", _ide_transfer_manager_get_actions (NULL));

  self->vcs_monitor = g_object_new (IDE_TYPE_VCS_MONITOR,
                                    "parent", self->context,
                                    NULL);

  self->addins = peas_extension_set_new (peas_engine_get_default (),
                                         IDE_TYPE_WORKBENCH_ADDIN,
                                         NULL);
  g_signal_connect (self->addins,
                    "extension-added",
                    G_CALLBACK (ide_workbench_addin_added_cb),
                    self);
  g_signal_connect (self->addins,
                    "extension-removed",
                    G_CALLBACK (ide_workbench_addin_removed_cb),
                    self);
  peas_extension_set_foreach (self->addins,
                              ide_workbench_addin_added_cb,
                              self);

}

static void
ide_workbench_dispose (GObject *object)
{
  IdeWorkbench *self = (IdeWorkbench *)object;

  g_assert (IDE_IS_MAIN_THREAD ());

  if (self->context != NULL)
    g_object_set_data (G_OBJECT (self->context), "WORKBENCH", NULL);

  g_clear_object (&self->build_system);
  g_clear_object (&self->vcs);
  g_clear_object (&self->vcs_monitor);
  g_clear_object (&self->search_engine);
  g_clear_object (&self->project_info);
  g_clear_object (&self->cancellable);
  g_clear_object (&self->session);
  g_clear_object (&self->context);

  G_OBJECT_CLASS (ide_workbench_parent_class)->dispose (object);
}

static void
ide_workbench_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  IdeWorkbench *self = IDE_WORKBENCH (object);

  g_assert (IDE_IS_MAIN_THREAD ());

  switch (prop_id)
    {
    case PROP_BUILD_SYSTEM:
      g_value_set_object (value, ide_workbench_get_build_system (self));
      break;

    case PROP_CONTEXT:
      g_value_set_object (value, ide_workbench_get_context (self));
      break;

    case PROP_CONFIG:
      g_value_set_object (value, ide_workbench_get_config (self));
      break;

    case PROP_VCS:
      g_value_set_object (value, ide_workbench_get_vcs (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_workbench_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  IdeWorkbench *self = IDE_WORKBENCH (object);

  g_assert (IDE_IS_MAIN_THREAD ());

  switch (prop_id)
    {
    case PROP_CONTEXT:
      ide_workbench_set_context (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_workbench_class_init (IdeWorkbenchClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_workbench_constructed;
  object_class->dispose = ide_workbench_dispose;
  object_class->get_property = ide_workbench_get_property;
  object_class->set_property = ide_workbench_set_property;

  properties[PROP_BUILD_SYSTEM] =
    g_param_spec_object ("build-system", NULL, NULL,
                         IDE_TYPE_BUILD_SYSTEM,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_CONFIG] =
    g_param_spec_object ("config", NULL, NULL,
                         IDE_TYPE_CONFIG,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * IdeWorkbench:context:
   *
   * The "context" property is the #IdeContext for the project.
   *
   * The #IdeContext is the root #IdeObject used in the tree of
   * objects representing the project and the workings of the IDE.
   */
  properties [PROP_CONTEXT] =
    g_param_spec_object ("context",
                         "Context",
                         "The IdeContext for the workbench",
                         IDE_TYPE_CONTEXT,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeWorkbench:vcs:
   *
   * The "vcs" property contains an #IdeVcs that represents the version control
   * system that is currently loaded for the project.
   *
   * The #IdeVcs is registered by an #IdeWorkbenchAddin when loading a project.
   */
  properties [PROP_VCS] =
    g_param_spec_object ("vcs",
                         "Vcs",
                         "The version control system, if any",
                         IDE_TYPE_VCS,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  ide_action_mixin_init (&action_mixin, object_class);

  ide_action_mixin_install_action (&action_mixin, "workspace.new", NULL, ide_workbench_action_add_workspace);
  ide_action_mixin_install_action (&action_mixin, "close", NULL, ide_workbench_action_close);
  ide_action_mixin_install_action (&action_mixin, "open", NULL, ide_workbench_action_open);
  ide_action_mixin_install_action (&action_mixin, "open-uri", "s", ide_workbench_action_open_uri);
  ide_action_mixin_install_action (&action_mixin, "reload-files", NULL, ide_workbench_action_reload_all);
  ide_action_mixin_install_action (&action_mixin, "global-search", NULL, ide_workbench_action_global_search);
  ide_action_mixin_install_action (&action_mixin, "configure", NULL, ide_workbench_action_configure);
  ide_action_mixin_install_action (&action_mixin, "configure-page", "s", ide_workbench_action_configure);
  ide_action_mixin_install_action (&action_mixin, "tweaks", NULL, ide_workbench_action_tweaks);
  ide_action_mixin_install_action (&action_mixin, "-inspector", NULL, ide_workbench_action_inspector);
  ide_action_mixin_install_action (&action_mixin, "-object-tree", NULL, ide_workbench_action_object_tree);
  ide_action_mixin_install_action (&action_mixin, "-dump-tasks", NULL, ide_workbench_action_dump_tasks);
}

static void
ide_workbench_init (IdeWorkbench *self)
{
}

static void
collect_addins_cb (PeasExtensionSet *set,
                   PeasPluginInfo   *plugin_info,
                   GObject    *exten,
                   gpointer          user_data)
{
  GPtrArray *ar = user_data;
  g_ptr_array_add (ar, g_object_ref (exten));
}

static GPtrArray *
ide_workbench_collect_addins (IdeWorkbench *self)
{
  g_autoptr(GPtrArray) ar = NULL;

  g_assert (IDE_IS_WORKBENCH (self));

  ar = g_ptr_array_new_with_free_func (g_object_unref);
  if (self->addins != NULL)
    peas_extension_set_foreach (self->addins, collect_addins_cb, ar);
  return g_steal_pointer (&ar);
}

static IdeWorkbenchAddin *
ide_workbench_find_addin (IdeWorkbench *self,
                          const gchar  *hint)
{
  PeasEngine *engine;
  PeasPluginInfo *plugin_info;
  GObject *exten = NULL;

  g_return_val_if_fail (IDE_IS_WORKBENCH (self), NULL);
  g_return_val_if_fail (hint != NULL, NULL);

  engine = peas_engine_get_default ();

  if ((plugin_info = peas_engine_get_plugin_info (engine, hint)))
    exten = peas_extension_set_get_extension (self->addins, plugin_info);

  return exten ? g_object_ref (IDE_WORKBENCH_ADDIN (exten)) : NULL;
}

/**
 * ide_workbench_new:
 *
 * Creates a new #IdeWorkbench.
 *
 * This does not create any windows, you'll need to request that a workspace
 * be created based on the kind of workspace you want to display to the user.
 *
 * Returns: an #IdeWorkbench
 */
IdeWorkbench *
ide_workbench_new (void)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);

  return g_object_new (IDE_TYPE_WORKBENCH, NULL);
}

/**
 * ide_workbench_new_for_context:
 *
 * Creates a new #IdeWorkbench using @context for the #IdeWorkbench:context.
 *
 * Returns: (transfer full): an #IdeWorkbench
 */
IdeWorkbench *
ide_workbench_new_for_context (IdeContext *context)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  return g_object_new (IDE_TYPE_CONTEXT,
                       "visible", TRUE,
                       NULL);
}

/**
 * ide_workbench_get_context:
 * @self: an #IdeWorkbench
 *
 * Gets the #IdeContext for the workbench.
 *
 * Returns: (transfer none): an #IdeContext
 */
IdeContext *
ide_workbench_get_context (IdeWorkbench *self)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_WORKBENCH (self), NULL);

  return self->context;
}

/**
 * ide_workbench_from_widget:
 * @widget: a #GtkWidget
 *
 * Finds the #IdeWorkbench associated with a widget.
 *
 * Returns: (nullable) (transfer none): an #IdeWorkbench or %NULL
 */
IdeWorkbench *
ide_workbench_from_widget (GtkWidget *widget)
{
  GtkWindowGroup *group;
  GtkWidget *toplevel;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  /*
   * The workbench is a window group, and the workspaces belong to us. So we
   * just need to get the toplevel window group property, and cast.
   */

  if ((toplevel = GTK_WIDGET (gtk_widget_get_native (widget))) &&
      GTK_IS_WINDOW (toplevel) &&
      (group = gtk_window_get_group (GTK_WINDOW (toplevel))) &&
      IDE_IS_WORKBENCH (group))
    return IDE_WORKBENCH (group);

  return NULL;
}

/**
 * ide_workbench_foreach_workspace:
 * @self: an #IdeWorkbench
 * @callback: (scope call): a #IdeWorkspaceCallback to call for each #IdeWorkspace
 * @user_data: user data for @callback
 *
 * Iterates the available workspaces in the workbench. Workspaces are iterated
 * in most-recently-used order.
 */
void
ide_workbench_foreach_workspace (IdeWorkbench         *self,
                                 IdeWorkspaceCallback  callback,
                                 gpointer              user_data)
{
  GList *copy;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_WORKBENCH (self));
  g_return_if_fail (callback != NULL);

  /* Copy for re-entrancy safety */
  copy = g_list_copy (self->mru_queue.head);

  for (const GList *iter = copy; iter; iter = iter->next)
    {
      IdeWorkspace *workspace = iter->data;
      g_assert (IDE_IS_WORKSPACE (workspace));
      callback (workspace, user_data);
    }

  g_list_free (copy);
}

typedef struct
{
  IdePageCallback callback;
  gpointer user_data;
} ForeachPage;

static void
ide_workbench_foreach_page_cb (IdeWorkspace *workspace,
                               gpointer      user_data)
{
  ForeachPage *state = user_data;

  g_assert (IDE_IS_WORKSPACE (workspace));
  g_assert (state != NULL);
  g_assert (state->callback != NULL);

  ide_workspace_foreach_page (workspace, state->callback, state->user_data);
}


/**
 * ide_workbench_foreach_page:
 * @self: a #IdeWorkbench
 * @callback: (scope call): a callback to execute for each page
 * @user_data: closure data for @callback
 *
 * Calls @callback for every page loaded in the workbench, by iterating
 * workspaces in order of most-recently-used.
 */
void
ide_workbench_foreach_page (IdeWorkbench    *self,
                            IdePageCallback  callback,
                            gpointer         user_data)
{
  ForeachPage state = {callback, user_data};

  g_return_if_fail (IDE_IS_WORKBENCH (self));
  g_return_if_fail (callback != NULL);

  ide_workbench_foreach_workspace (self, ide_workbench_foreach_page_cb, &state);
}

static void
ide_workbench_workspace_is_active_cb (IdeWorkbench *self,
                                      GParamSpec   *pspec,
                                      IdeWorkspace *workspace)
{
  g_assert (IDE_IS_WORKBENCH (self));
  g_assert (IDE_IS_WORKSPACE (workspace));
  g_assert (gtk_window_get_group (GTK_WINDOW (workspace)) == GTK_WINDOW_GROUP (self));

  if (gtk_window_is_active (GTK_WINDOW (workspace)))
    {
      GList *mru_link = _ide_workspace_get_mru_link (workspace);

      g_queue_unlink (&self->mru_queue, mru_link);

      g_assert (mru_link->prev == NULL);
      g_assert (mru_link->next == NULL);
      g_assert (mru_link->data == (gpointer)workspace);

      g_queue_push_head_link (&self->mru_queue, mru_link);
    }
}

/**
 * ide_workbench_add_workspace:
 * @self: an #IdeWorkbench
 * @workspace: an #IdeWorkspace
 *
 * Adds @workspace to @workbench.
 */
void
ide_workbench_add_workspace (IdeWorkbench *self,
                             IdeWorkspace *workspace)
{
  g_autoptr(IdeActionMuxer) muxer = NULL;
  g_autoptr(GPtrArray) addins = NULL;
  IdeShortcutManager *shortcuts;
  GList *mru_link;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_WORKBENCH (self));
  g_return_if_fail (IDE_IS_WORKSPACE (workspace));

  /* Now add the window to the workspace (which takes no reference, as the
   * window will take a reference back to us.
   */
  if (gtk_window_get_group (GTK_WINDOW (workspace)) != GTK_WINDOW_GROUP (self))
    gtk_window_group_add_window (GTK_WINDOW_GROUP (self), GTK_WINDOW (workspace));

  g_assert (gtk_window_has_group (GTK_WINDOW (workspace)));
  g_assert (gtk_window_get_group (GTK_WINDOW (workspace)) == GTK_WINDOW_GROUP (self));

  /* Now place the workspace into our MRU tracking */
  mru_link = _ide_workspace_get_mru_link (workspace);

  /* New workspaces are expected to be displayed right away, so we can
   * just push the window onto the head.
   */
  g_queue_push_head_link (&self->mru_queue, mru_link);

  /* Update the context for the workspace, even if we're not loaded,
   * this IdeContext will be updated later.
   */
  _ide_workspace_set_context (workspace, self->context);

  /* Connect context actions to the workspace */
  muxer = ide_context_ref_action_muxer (self->context);
  gtk_widget_insert_action_group (GTK_WIDGET (workspace),
                                  "context",
                                  G_ACTION_GROUP (muxer));

  /* Setup capture shortcut controller for workspace */
  shortcuts = ide_shortcut_manager_from_context (self->context);
  _ide_workspace_set_shortcut_model (workspace, G_LIST_MODEL (shortcuts));

  /* Track toplevel focus changes to maintain a most-recently-used queue. */
  g_signal_connect_object (workspace,
                           "notify::is-active",
                           G_CALLBACK (ide_workbench_workspace_is_active_cb),
                           self,
                           G_CONNECT_SWAPPED);

  /* Notify all the addins about the new workspace. */
  if ((addins = ide_workbench_collect_addins (self)))
    {
      for (guint i = 0; i < addins->len; i++)
        {
          IdeWorkbenchAddin *addin = g_ptr_array_index (addins, i);
          ide_workbench_addin_workspace_added (addin, workspace);
        }
    }

  if (!gtk_window_get_title (GTK_WINDOW (workspace)))
    {
      g_autofree gchar *title = NULL;
      g_autofree gchar *formatted = NULL;

      title = ide_context_dup_title (self->context);
      formatted = g_strdup_printf (_("Builder — %s"), title);
      gtk_window_set_title (GTK_WINDOW (workspace), formatted);
    }
}

/**
 * ide_workbench_remove_workspace:
 * @self: an #IdeWorkbench
 * @workspace: an #IdeWorkspace
 *
 * Removes @workspace from @workbench.
 */
void
ide_workbench_remove_workspace (IdeWorkbench *self,
                                IdeWorkspace *workspace)
{
  g_autoptr(GPtrArray) addins = NULL;
  GList *list;
  GList *mru_link;
  guint count = 0;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_WORKBENCH (self));
  g_return_if_fail (IDE_IS_WORKSPACE (workspace));

  /* Stop tracking MRU changes */
  mru_link = _ide_workspace_get_mru_link (workspace);
  g_queue_unlink (&self->mru_queue, mru_link);
  g_signal_handlers_disconnect_by_func (workspace,
                                        G_CALLBACK (ide_workbench_workspace_is_active_cb),
                                        self);

  /* Notify all the addins about losing the workspace. */
  if ((addins = ide_workbench_collect_addins (self)))
    {
      for (guint i = 0; i < addins->len; i++)
        {
          IdeWorkbenchAddin *addin = g_ptr_array_index (addins, i);
          ide_workbench_addin_workspace_removed (addin, workspace);
        }
    }

  /* Clear our action groups (which drops an additional back-reference) */
  gtk_widget_insert_action_group (GTK_WIDGET (workspace), "context", NULL);

  /* Only cleanup the group if it hasn't already been removed */
  if (gtk_window_has_group (GTK_WINDOW (workspace)))
    gtk_window_group_remove_window (GTK_WINDOW_GROUP (self), GTK_WINDOW (workspace));

  /*
   * If this is our last workspace being closed, then we want to
   * try to cleanup the workbench and shut things down.
   */

  list = gtk_window_group_list_windows (GTK_WINDOW_GROUP (self));
  for (const GList *iter = list; iter; iter = iter->next)
    {
      GtkWindow *window = iter->data;

      if (IDE_IS_WORKSPACE (window) && workspace != IDE_WORKSPACE (window))
        count++;
    }
  g_list_free (list);

  /*
   * If there are no more workspaces left, then we will want to also
   * unload the workbench opportunistically, so that the application
   * can exit cleanly.
   */
  if (count == 0 && self->unloaded == FALSE)
    ide_workbench_unload_async (self, NULL, NULL, NULL);
}

/**
 * ide_workbench_focus_workspace:
 * @self: an #IdeWorkbench
 * @workspace: an #IdeWorkspace
 *
 * Requests that @workspace be raised in the windows of @self, and
 * displayed to the user.
 */
void
ide_workbench_focus_workspace (IdeWorkbench *self,
                               IdeWorkspace *workspace)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_WORKBENCH (self));
  g_return_if_fail (IDE_IS_WORKSPACE (workspace));

  gtk_window_present (GTK_WINDOW (workspace));
}

static void
ide_workbench_project_loaded_foreach_cb (PeasExtensionSet *set,
                                         PeasPluginInfo   *plugin_info,
                                         GObject    *exten,
                                         gpointer          user_data)
{
  IdeWorkbenchAddin *addin = (IdeWorkbenchAddin *)exten;
  IdeWorkbench *self = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (IDE_IS_WORKBENCH_ADDIN (addin));
  g_assert (IDE_IS_WORKBENCH (self));
  g_assert (IDE_IS_PROJECT_INFO (self->project_info));

  ide_workbench_addin_project_loaded (addin, self->project_info);
}

static void
ide_workbench_restore_workspace_session_cb (IdeWorkspace *workspace,
                                            gpointer      user_data)
{
  IdeSession *session = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_WORKSPACE (workspace));
  g_assert (IDE_IS_SESSION (session));

  _ide_workspace_restore_session (workspace, session);
}

static gboolean
ide_workbench_can_restore_session (IdeWorkbench *self)
{
  IdeApplication *app = IDE_APPLICATION_DEFAULT;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_APPLICATION (app));
  g_assert (IDE_IS_WORKBENCH (self));

  if (self->session == NULL)
    return FALSE;

  /* Wish we could rename this setting, maybe in 44 */
  return g_settings_get_boolean (app->settings, "restore-previous-files");
}

static void
ide_workbench_load_project_completed (IdeWorkbench *self,
                                      IdeTask      *task)
{
  IdeBuildManager *build_manager;
  LoadProject *lp;

  g_assert (IDE_IS_WORKBENCH (self));
  g_assert (IDE_IS_TASK (task));

  lp = ide_task_get_task_data (task);

  g_assert (lp != NULL);
  g_assert (lp->addins != NULL);
  g_assert (lp->addins->len == 0);

  /* If we did not get a VCS as part of the loading process, set the
   * fallback VCS implementation.
   */
  if (self->vcs == NULL)
    {
      g_autoptr(GFile) workdir = ide_context_ref_workdir (self->context);
      g_autoptr(IdeDirectoryVcs) vcs = ide_directory_vcs_new (workdir);

      ide_workbench_set_vcs (self, IDE_VCS (vcs));
    }

  /* Create the search engine up-front */
  if (self->search_engine == NULL)
    self->search_engine = ide_object_ensure_child_typed (IDE_OBJECT (self->context),
                                                         IDE_TYPE_SEARCH_ENGINE);

  /* Allow addins to restore session, which might bypass the need to create
   * a workspace manually below.
   */
  if (ide_workbench_can_restore_session (self))
    {
      /* Restore workspaces, and cancel our request to create a new one
       * if the workspace was likely created already.
       */
      if (_ide_workbench_restore_workspaces (self,
                                             self->session,
                                             lp->present_time,
                                             lp->workspace_type))
        lp->workspace_type = G_TYPE_INVALID;

      _ide_workbench_addins_restore_session (self, self->addins, self->session);
    }

  if (lp->workspace_type != G_TYPE_INVALID)
    {
      IdeWorkspace *workspace;

      workspace = g_object_new (lp->workspace_type,
                                "application", IDE_APPLICATION_DEFAULT,
                                NULL);
      ide_workbench_add_workspace (self, workspace);
      gtk_window_present_with_time (GTK_WINDOW (workspace), lp->present_time);
    }

  /* Notify addins that projects have loaded */
  peas_extension_set_foreach (self->addins,
                              ide_workbench_project_loaded_foreach_cb,
                              self);

  /* Now that we have a workspace window for the project, we can allow
   * the build manager to start.
   */
  build_manager = ide_build_manager_from_context (self->context);

  /* Enable actions that are available to projects */
  ide_action_mixin_set_enabled (self, "configure", TRUE);
  ide_action_mixin_set_enabled (self, "configure-page", TRUE);

  /* Now restore the workspace sessions */
  if (ide_workbench_can_restore_session (self))
    {
      ide_workbench_foreach_workspace (self,
                                       ide_workbench_restore_workspace_session_cb,
                                       self->session);
      g_clear_object (&self->session);
    }

  ide_object_message (self->context, _("Project loaded"));

  _ide_build_manager_start (build_manager);

  ide_task_return_boolean (task, TRUE);
}

static void
ide_workbench_load_project_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  IdeWorkbenchAddin *addin = (IdeWorkbenchAddin *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeWorkbench *self;
  LoadProject *lp;

  g_assert (IDE_IS_WORKBENCH_ADDIN (addin));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  lp = ide_task_get_task_data (task);

  g_assert (IDE_IS_WORKBENCH (self));
  g_assert (lp != NULL);
  g_assert (IDE_IS_PROJECT_INFO (lp->project_info));
  g_assert (lp->addins != NULL);
  g_assert (lp->addins->len > 0);

  if (!ide_workbench_addin_load_project_finish (addin, result, &error))
    {
      if (!ignore_error (error))
        g_warning ("%s addin failed to load project: %s",
                   G_OBJECT_TYPE_NAME (addin), error->message);
    }

  g_ptr_array_remove (lp->addins, addin);

  if (lp->addins->len == 0)
    ide_workbench_load_project_completed (self, task);
}

static void
ide_workbench_config_changed_cb (IdeWorkbench     *self,
                                 GParamSpec       *pspec,
                                 IdeConfigManager *config_manager)
{
  g_assert (IDE_IS_WORKBENCH (self));
  g_assert (IDE_IS_CONFIG_MANAGER (config_manager));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CONFIG]);
}

static void
ide_workbench_init_foundry_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeConfigManager *config_manager;
  IdeWorkbench *self;
  GCancellable *cancellable;
  LoadProject *lp;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!_ide_foundry_init_finish (result, &error))
    g_critical ("Failed to initialize foundry: %s", error->message);

  cancellable = ide_task_get_cancellable (task);
  self = ide_task_get_source_object (task);
  lp = ide_task_get_task_data (task);

  g_assert (IDE_IS_WORKBENCH (self));
  g_assert (lp != NULL);
  g_assert (lp->addins != NULL);
  g_assert (IDE_IS_PROJECT_INFO (lp->project_info));

  if ((config_manager = ide_context_peek_child_typed (self->context, IDE_TYPE_CONFIG_MANAGER)))
    g_signal_connect_object (config_manager,
                             "notify::current",
                             G_CALLBACK (ide_workbench_config_changed_cb),
                             self,
                             G_CONNECT_SWAPPED);

  /* Now, we need to notify all of the workbench addins that we're
   * opening the project. Once they have all completed, we'll create the
   * new workspace window and attach it. That saves us the work of
   * rendering various frames of the during the intensive load process.
   */
  for (guint i = 0; i < lp->addins->len; i++)
    {
      IdeWorkbenchAddin *addin = g_ptr_array_index (lp->addins, i);

      ide_workbench_addin_load_project_async (addin,
                                              lp->project_info,
                                              cancellable,
                                              ide_workbench_load_project_cb,
                                              g_object_ref (task));
    }

  if (lp->addins->len == 0)
    ide_workbench_load_project_completed (self, task);
}

/**
 * ide_workbench_load_project_async:
 * @self: a #IdeWorkbench
 * @project_info: an #IdeProjectInfo describing the project to open
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: (nullable): a #GAsyncReadyCallback to execute upon completion
 * @user_data: user data for @callback
 *
 * Requests that a project be opened in the workbench.
 *
 * @project_info should contain enough information to discover and load the
 * project. Depending on the various fields of the #IdeProjectInfo,
 * different plugins may become active as part of loading the project.
 *
 * Note that this may only be called once for an #IdeWorkbench. If you need
 * to open a second project, you need to create and register a second
 * workbench first, and then open using that secondary workbench.
 *
 * @callback should call ide_workbench_load_project_finish() to obtain the
 * result of the open request.
 */
void
ide_workbench_load_project_async (IdeWorkbench        *self,
                                  IdeProjectInfo      *project_info,
                                  GType                workspace_type,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GFile) parent = NULL;
  g_autofree gchar *name = NULL;
  const char *project_id;
  const char *project_name;
  LoadProject *lp;
  GFile *directory;
  GFile *file;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_WORKBENCH (self));
  g_return_if_fail (IDE_IS_PROJECT_INFO (project_info));
  g_return_if_fail (workspace_type != IDE_TYPE_WORKSPACE);
  g_return_if_fail (workspace_type == G_TYPE_INVALID ||
                    g_type_is_a (workspace_type, IDE_TYPE_WORKSPACE));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (self->unloaded == FALSE);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_workbench_load_project_async);

  if (self->project_info != NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "Cannot load project, a project is already loaded");
      IDE_EXIT;
    }

  _ide_context_set_has_project (self->context);

  g_set_object (&self->project_info, project_info);

  /* Update context project-id based on project-info */
  if ((project_id = ide_project_info_get_id (project_info)))
    {
      g_autofree gchar *generated = ide_create_project_id (project_id);
      ide_context_set_project_id (self->context, generated);
    }

  if (!(project_name = ide_project_info_get_name (project_info)))
    project_name = project_id;

  ide_object_message (self->context,
                      /* translators: %s is replaced with the project name */
                      _("Loading project “%s”"),
                      project_name);

  if (!ide_project_info_get_directory (project_info) &&
      !ide_project_info_get_file (project_info))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_FOUND,
                                 "No file or directory provided to load as project");
      IDE_EXIT;
    }

  /* Fallback to using directory as file if necessary */
  if (!(file = ide_project_info_get_file (project_info)))
    {
      file = ide_project_info_get_directory (project_info);
      g_assert (G_IS_FILE (file));

      ide_project_info_set_file (project_info, file);
    }

  /*
   * Track the directory root based on project info. If we didn't get a
   * directory set, then take the parent of the project file.
   */

  if ((directory = ide_project_info_get_directory (project_info)))
    {
      ide_context_set_workdir (self->context, directory);
    }
  else
    {
      if (g_file_query_file_type (file, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL) == G_FILE_TYPE_DIRECTORY)
        {
          ide_context_set_workdir (self->context, file);
          directory = file;
        }
      else
        {
          ide_context_set_workdir (self->context, (parent = g_file_get_parent (file)));
          directory = parent;
        }

      ide_project_info_set_directory (project_info, directory);
    }

  g_assert (G_IS_FILE (directory));

  name = g_file_get_basename (directory);
  ide_context_set_title (self->context, name);

  {
    GFile *pdir = ide_project_info_get_directory (project_info);
    GFile *pfile = ide_project_info_get_file (project_info);
    const gchar *pident = ide_project_info_get_id (project_info);
    const gchar *pname = ide_project_info_get_name (project_info);

    /* Log some information to help track down project loading issues. */
    g_debug ("Loading project");
    g_debug ("    id = %s", pname);
    g_debug ("  name = %s", pident);
    g_debug ("   dir = %s", g_file_peek_path (pdir));
    g_debug ("  file = %s", g_file_peek_path (pfile));
  }

  /* If there has not been a project name set, make the default matching
   * the directory name. A plugin may update the name with more information
   * based on .doap files, etc.
   */
  if (!ide_project_info_get_name (project_info))
    ide_project_info_set_name (project_info, name);

  /* Setup some information we're going to need later on when loading the
   * individual workbench addins (and then creating the workspace).
   */
  lp = g_slice_new0 (LoadProject);
  lp->project_info = g_object_ref (project_info);
  /* HACK: Workaround for lack of last event time */
  lp->present_time = g_get_monotonic_time () / 1000L;
  lp->addins = ide_workbench_collect_addins (self);
  lp->workspace_type = workspace_type;
  ide_task_set_task_data (task, lp, load_project_free);

  /*
   * Before we load any addins, we want to register the Foundry subsystems
   * such as the device manager, diagnostics engine, configurations, etc.
   * This makes sure that we have some basics setup before addins load.
   */
  _ide_foundry_init_async (self->context,
                           cancellable,
                           ide_workbench_init_foundry_cb,
                           g_steal_pointer (&task));

  IDE_EXIT;
}

/**
 * ide_workbench_load_project_finish:
 * @self: a #IdeWorkbench
 *
 * Completes an asynchronous request to open a project using
 * ide_workbench_load_project_async().
 *
 * Returns: %TRUE if the project was successfully opened; otherwise %FALSE
 *   and @error is set.
 */
gboolean
ide_workbench_load_project_finish (IdeWorkbench  *self,
                                   GAsyncResult  *result,
                                   GError       **error)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (IDE_IS_WORKBENCH (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
print_object_tree (IdeObject *object,
                   gpointer   depthptr)
{
  gint depth = GPOINTER_TO_INT (depthptr);
  g_autofree gchar *space = g_strnfill (depth * 2, ' ');
  g_autofree gchar *info = ide_object_repr (object);

  g_print ("%s%s\n", space, info);
  ide_object_foreach (object,
                      (GFunc)print_object_tree,
                      GINT_TO_POINTER (depth + 1));
}

static void
ide_workbench_action_object_tree (gpointer    instance,
                                  const char *action_name,
                                  GVariant   *param)
{
  IdeWorkbench *self = instance;

  g_assert (IDE_IS_WORKBENCH (self));

  print_object_tree (IDE_OBJECT (self->context), NULL);
}

static void
ide_workbench_action_dump_tasks (gpointer    instance,
                                 const char *action_name,
                                 GVariant   *param)
{
  IdeWorkbench *self = instance;

  g_assert (IDE_IS_WORKBENCH (self));

  _ide_dump_tasks ();
}

static void
ide_workbench_action_inspector (gpointer    instance,
                                const char *action_name,
                                GVariant   *param)
{
  gtk_window_set_interactive_debugging (TRUE);
}

static void
ide_workbench_action_close_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  IdeWorkbench *self = (IdeWorkbench *)object;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_WORKBENCH (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (user_data == NULL);

  if (ide_workbench_unload_finish (self, result, NULL))
    {
      IdeApplication *app = IDE_APPLICATION_DEFAULT;
      GtkWindow *active;

      if (!(active = gtk_application_get_active_window (GTK_APPLICATION (app))))
        g_application_activate (G_APPLICATION (app));
      else
        ide_gtk_window_present (active);
    }
}

static void
ide_workbench_action_close (gpointer    instance,
                            const char *action_name,
                            GVariant   *param)
{
  IdeWorkbench *self = instance;

  g_assert (IDE_IS_WORKBENCH (self));
  g_assert (param == NULL);

  if (self->unloaded == FALSE)
    ide_workbench_unload_async (self,
                                NULL,
                                ide_workbench_action_close_cb,
                                NULL);
}

static void
ide_workbench_action_add_workspace (gpointer    instance,
                                    const char *action_name,
                                    GVariant   *param)
{
  IdeWorkbench *self = instance;
  IdeWorkspace *workspace;

  IDE_ENTRY;

  g_assert (IDE_IS_WORKBENCH (self));

  workspace = _ide_workbench_create_secondary (self);
  gtk_window_present (GTK_WINDOW (workspace));

  IDE_EXIT;
}

static void
ide_workbench_action_reload_all_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  IdeBufferManager *bufmgr = (IdeBufferManager *)object;
  g_autoptr(IdeWorkbench) self = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER_MANAGER (bufmgr));
  g_assert (IDE_IS_WORKBENCH (self));

  if (!ide_buffer_manager_reload_all_finish (bufmgr, result, &error))
    g_warning ("Failed to reload buffers that changed on disk: %s", error->message);
  else
    g_debug ("All buffers changed on disk were reloaded");

  IDE_EXIT;
}

static void
ide_workbench_action_reload_all (gpointer    instance,
                                 const char *action_name,
                                 GVariant   *param)
{
  IdeWorkbench *self = instance;
  IdeBufferManager *bufmgr;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_WORKBENCH (self));
  g_assert (param == NULL);

  g_debug ("Reloading all files which have changed on disk");

  context = ide_workbench_get_context (self);
  bufmgr = ide_buffer_manager_from_context (context);
  ide_buffer_manager_reload_all_async (bufmgr,
                                       NULL,
                                       ide_workbench_action_reload_all_cb,
                                       g_object_ref (self));

  IDE_EXIT;
}

static void
ide_workbench_action_open_response_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  GtkFileDialog *dialog = (GtkFileDialog *)object;
  g_autoptr(IdeWorkbench) self = user_data;
  g_autoptr(GListModel) model = NULL;
  g_autoptr(GError) error = NULL;
  guint n_items;

  g_assert (GTK_IS_FILE_DIALOG (dialog));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_WORKBENCH (self));

  if (!(model = gtk_file_dialog_open_multiple_finish (dialog, result, &error)))
    return;

  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(GFile) file = g_list_model_get_item (model, i);

      g_assert (G_IS_FILE (file));

      ide_workbench_open_async (self, file, NULL, 0, NULL, NULL, NULL, NULL);
    }
}

static void
ide_workbench_action_open (gpointer    instance,
                           const char *action_name,
                           GVariant   *param)
{
  g_autoptr(GtkFileDialog) dialog = NULL;
  IdeWorkbench *self = instance;
  IdeWorkspace *workspace;

  g_assert (IDE_IS_WORKBENCH (self));
  g_assert (param == NULL);

  workspace = ide_workbench_get_current_workspace (self);

  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (dialog, _("Open File…"));
  gtk_file_dialog_set_accept_label (dialog, _("Open"));
  gtk_file_dialog_set_modal (dialog, FALSE);

  gtk_file_dialog_open_multiple (dialog,
                                 GTK_WINDOW (workspace),
                                 NULL,
                                 ide_workbench_action_open_response_cb,
                                 g_object_ref (self));
}

static void
ide_workbench_action_open_uri (gpointer    instance,
                               const char *action_name,
                               GVariant   *param)
{
  IdeWorkbench *self = instance;
  g_autoptr(GFile) file = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_WORKBENCH (self));
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_STRING));

  file = g_file_new_for_uri (g_variant_get_string (param, NULL));
  ide_workbench_open_async (self, file, NULL, 0, NULL, NULL, NULL, NULL);

  IDE_EXIT;
}

static void
ide_workbench_action_global_search (gpointer    instance,
                                    const char *action_name,
                                    GVariant   *param)
{
  IdeWorkbench *self = instance;

  IDE_ENTRY;

  g_assert (IDE_IS_WORKBENCH (self));
  g_assert (param == NULL);

  for (const GList *iter = self->mru_queue.head; iter; iter = iter->next)
    {
      IdeWorkspace *workspace = iter->data;

      if (_ide_workspace_can_search (workspace))
        {
          _ide_workspace_begin_global_search (workspace);
          IDE_EXIT;
        }
    }

  IDE_EXIT;
}

/**
 * ide_workbench_get_search_engine:
 * @self: a #IdeWorkbench
 *
 * Gets the search engine for the workbench, if any.
 *
 * Returns: (transfer none): an #IdeSearchEngine
 */
IdeSearchEngine *
ide_workbench_get_search_engine (IdeWorkbench *self)
{
  g_return_val_if_fail (IDE_IS_WORKBENCH (self), NULL);
  g_return_val_if_fail (self->context != NULL, NULL);

  if (self->search_engine == NULL)
    self->search_engine = ide_object_ensure_child_typed (IDE_OBJECT (self->context),
                                                         IDE_TYPE_SEARCH_ENGINE);

  return self->search_engine;
}

/**
 * ide_workbench_get_project_info:
 * @self: a #IdeWorkbench
 *
 * Gets the #IdeProjectInfo for the workbench, if a project has been or is
 * currently, loading.
 *
 * Returns: (transfer none) (nullable): an #IdeProjectInfo or %NULL
 */
IdeProjectInfo *
ide_workbench_get_project_info (IdeWorkbench *self)
{
  g_return_val_if_fail (IDE_IS_WORKBENCH (self), NULL);

  return self->project_info;
}

static void
ide_workbench_unload_foundry_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeWorkbench *self;

  IDE_ENTRY;

  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);

  if (!_ide_foundry_unload_finish (result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  if (self->context != NULL)
    {
      ide_object_destroy (IDE_OBJECT (self->context));
      g_clear_object (&self->context);
    }

  IDE_EXIT;
}

static void
ide_workbench_unload_project_completed (IdeWorkbench *self,
                                        IdeTask      *task)
{
  GList *copy;

  IDE_ENTRY;

  g_assert (IDE_IS_WORKBENCH (self));
  g_assert (IDE_IS_TASK (task));

  g_clear_object (&self->addins);

  copy = g_list_copy_deep (self->mru_queue.head, (GCopyFunc)g_object_ref, NULL);
  for (const GList *iter = copy; iter; iter = iter->next)
    {
      IdeWorkspace *workspace = iter->data;
      g_assert (IDE_IS_WORKSPACE (workspace));
      gtk_window_destroy (GTK_WINDOW (workspace));
    }
  g_list_free_full (copy, g_object_unref);

  _ide_foundry_unload_async (self->context,
                             ide_task_get_cancellable (task),
                             ide_workbench_unload_foundry_cb,
                             g_object_ref (task));

  IDE_EXIT;
}
static void
ide_workbench_unload_project_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  IdeWorkbenchAddin *addin = (IdeWorkbenchAddin *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeWorkbench *self;
  GPtrArray *addins;

  IDE_ENTRY;

  g_assert (IDE_IS_WORKBENCH_ADDIN (addin));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  addins = ide_task_get_task_data (task);

  g_assert (IDE_IS_WORKBENCH (self));
  g_assert (addins != NULL);
  g_assert (addins->len > 0);

  if (!ide_workbench_addin_unload_project_finish (addin, result, &error))
    {
      if (!ignore_error (error))
        g_warning ("%s failed to unload project: %s",
                   G_OBJECT_TYPE_NAME (addin), error->message);
    }

  g_ptr_array_remove (addins, addin);

  if (addins->len == 0)
    ide_workbench_unload_project_completed (self, task);

  IDE_EXIT;
}

/**
 * ide_workbench_unload_async:
 * @self: an #IdeWorkbench
 * @cancellable: (nullable): a #GCancellable
 * @callback: a #GAsyncReadyCallback to execute upon completion
 * @user_data: closure data for @callback
 *
 * Asynchronously unloads the workbench.
 *
 * All #IdeWorkspace windows will be closed after calling this
 * function.
 */
void
ide_workbench_unload_async (IdeWorkbench        *self,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GPtrArray) addins = NULL;
  g_autoptr(IdeSession) session = NULL;
  GApplication *app;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_WORKBENCH (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_workbench_unload_async);

  if (self->unloaded)
    {
      ide_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  self->unloaded = TRUE;

  /* Keep the GApplication alive for the lifetime of the task */
  app = g_application_get_default ();
  g_signal_connect_object (task,
                           "notify::completed",
                           G_CALLBACK (g_application_release),
                           app,
                           G_CONNECT_SWAPPED);
  g_application_hold (app);

  /* Collect addins up front, we'll need them a couple times */
  addins = ide_workbench_collect_addins (self);

  /* Create a session object to store project state */
  session = ide_session_new ();

  /* We always request to save session state, but we only persist
   * it to disk for projects. That may change in the future though
   * so we always call that API for symmetry.
   */
  for (guint i = 0; i < addins->len; i++)
    {
      IdeWorkbenchAddin *addin = g_ptr_array_index (addins, i);
      ide_workbench_addin_save_session (addin, session);
    }

  /* Now let the workspaces capture themselves as that is a
   * more convenient session API for some addins.
   */
  ide_workbench_foreach_workspace (self,
                                   (IdeWorkspaceCallback)_ide_workspace_save_session,
                                   session);

  /* Release the search engine early to help it cleanup */
  ide_clear_and_destroy_object (&self->search_engine);

  /*
   * Remove our workbench from the application, so that no new
   * open-file requests can keep us alive while we're shutting
   * down.
   */
  ide_application_remove_workbench (IDE_APPLICATION (app), self);

  /* If we haven't loaded a project, then there is nothing to
   * do right now, just let ide_workbench_addin_unload() be called
   * when the workbench disposes.
   */
  if (self->project_info == NULL)
    {
      ide_workbench_unload_project_completed (self, task);
      IDE_EXIT;
    }

  /* Keep the addins around to call during stages of unload */
  ide_task_set_task_data (task,
                          g_ptr_array_ref (addins),
                          g_ptr_array_unref);

  if (addins->len == 0)
    {
      ide_workbench_unload_project_completed (self, task);
      IDE_EXIT;
    }

  for (guint i = 0; i < addins->len; i++)
    {
      IdeWorkbenchAddin *addin = g_ptr_array_index (addins, i);

      ide_workbench_addin_unload_project_async (addin,
                                                self->project_info,
                                                ide_task_get_cancellable (task),
                                                ide_workbench_unload_project_cb,
                                                g_object_ref (task));
    }

  IDE_EXIT;
}

/**
 * ide_workbench_unload_finish:
 * @self: an #IdeWorkbench
 * @result: a #GAsyncResult provided to callback
 * @error: a location for a #GError, or %NULL

 * Completes a request to unload the workbench.
 *
 * Returns: %TRUE if the workbench was unloaded successfully,
 *   otherwise %FALSE and @error is set.
 */
gboolean
ide_workbench_unload_finish (IdeWorkbench *self,
                             GAsyncResult *result,
                             GError **error)
{
  g_return_val_if_fail (IDE_IS_WORKBENCH (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
ide_workbench_open_all_cb (GObject      *object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  IdeWorkbench *self = (IdeWorkbench *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  gint *n_active;

  g_assert (IDE_IS_WORKBENCH (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_workbench_open_finish (self, result, &error))
    g_message ("Failed to open file: %s", error->message);

  n_active = ide_task_get_task_data (task);
  g_assert (n_active != NULL);

  (*n_active)--;

  if (*n_active == 0)
    ide_task_return_boolean (task, TRUE);
}

/**
 * ide_workbench_open_all_async:
 * @self: an #IdeWorkbench
 * @files: (array length=n_files): an array of #GFile
 * @n_files: number of #GFiles contained in @files
 * @hint: (nullable): an optional hint about what addin to use
 * @cancellable: (nullable): a #GCancellable
 * @callback: a #GAsyncReadyCallback to execute upon completion
 * @user_data: closure data for @callback
 *
 * Requests that the workbench open all of the #GFile denoted by @files.
 *
 * If @hint is provided, that will be used to determine what workbench
 * addin to use when opening the file. The @hint name should match the
 * module name of the plugin.
 *
 * Call ide_workbench_open_finish() from @callback to complete this
 * operation.
 */
void
ide_workbench_open_all_async (IdeWorkbench         *self,
                              GFile               **files,
                              guint                 n_files,
                              const gchar          *hint,
                              GCancellable         *cancellable,
                              GAsyncReadyCallback   callback,
                              gpointer              user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GPtrArray) ar = NULL;
  gint *n_active;

  g_return_if_fail (IDE_IS_WORKBENCH (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_workbench_open_all_async);

  if (n_files == 0)
    {
      ide_task_return_boolean (task, TRUE);
      return;
    }

  ar = g_ptr_array_new_full (n_files, g_object_unref);
  for (guint i = 0; i < n_files; i++)
    g_ptr_array_add (ar, g_object_ref (files[i]));

  n_active = g_new0 (gint, 1);
  *n_active = ar->len;
  ide_task_set_task_data (task, n_active, g_free);

  for (guint i = 0; i < ar->len; i++)
    {
      GFile *file = g_ptr_array_index (ar, i);

      ide_workbench_open_async (self,
                                file,
                                hint,
                                IDE_BUFFER_OPEN_FLAGS_NONE,
                                NULL,
                                cancellable,
                                ide_workbench_open_all_cb,
                                g_object_ref (task));
    }
}

/**
 * ide_workbench_open_async:
 * @self: an #IdeWorkbench
 * @file: a #GFile
 * @hint: (nullable): an optional hint about what addin to use
 * @flags: optional flags when opening the file
 * @position: (nullable): a position to open the page
 * @cancellable: (nullable): a #GCancellable
 * @callback: a #GAsyncReadyCallback to execute upon completion
 * @user_data: closure data for @callback
 *
 * Requests that the workbench open @file.
 *
 * If @hint is provided, that will be used to determine what workbench
 * addin to use when opening the file. The @hint name should match the
 * module name of the plugin.
 *
 * @flags may be ignored by some backends.
 */
void
ide_workbench_open_async (IdeWorkbench        *self,
                          GFile               *file,
                          const gchar         *hint,
                          IdeBufferOpenFlags   flags,
                          PanelPosition       *position,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
  g_return_if_fail (IDE_IS_WORKBENCH (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  ide_workbench_open_at_async (self,
                               file,
                               hint,
                               -1,
                               -1,
                               flags,
                               position,
                               cancellable,
                               callback,
                               user_data);
}

static void
ide_workbench_open_cb (GObject      *object,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  IdeWorkbenchAddin *addin = (IdeWorkbenchAddin *)object;
  IdeWorkbenchAddin *next;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  GCancellable *cancellable;
  Open *o;

  g_assert (IDE_IS_WORKBENCH_ADDIN (addin));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  cancellable = ide_task_get_cancellable (task);
  o = ide_task_get_task_data (task);

  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (o != NULL);
  g_assert (o->addins != NULL);
  g_assert (o->addins->len > 0);

  if (ide_workbench_addin_open_finish (addin, result, &error))
    {
      ide_task_return_boolean (task, TRUE);
      return;
    }

  g_debug ("%s did not open the file, trying next.",
           G_OBJECT_TYPE_NAME (addin));

  g_ptr_array_remove (o->addins, addin);

  /*
   * We failed to open the file, try the next addin that is
   * left which said it supported the content-type.
   */

  if (o->addins->len == 0)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "Failed to locate addin supporting file");
      return;
    }

  next = g_ptr_array_index (o->addins, 0);

  ide_workbench_addin_open_async (next,
                                  o->file,
                                  o->content_type,
                                  o->at_line,
                                  o->at_line_offset,
                                  o->flags,
                                  o->position,
                                  cancellable,
                                  ide_workbench_open_cb,
                                  g_steal_pointer (&task));
}

static gint
sort_by_priority (gconstpointer a,
                  gconstpointer b,
                  gpointer      user_data)
{
  IdeWorkbenchAddin *addin_a = *(IdeWorkbenchAddin **)a;
  IdeWorkbenchAddin *addin_b = *(IdeWorkbenchAddin **)b;
  Open *o = user_data;
  gint prio_a = 0;
  gint prio_b = 0;

  if (!ide_workbench_addin_can_open (addin_a, o->file, o->content_type, &prio_a))
    return 1;

  if (!ide_workbench_addin_can_open (addin_b, o->file, o->content_type, &prio_b))
    return -1;

  if (prio_a < prio_b)
    return -1;
  else if (prio_a > prio_b)
    return 1;
  else
    return 0;
}

static void
ide_workbench_open_query_info_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  GFile *file = (GFile *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GFileInfo) info = NULL;
  g_autoptr(GError) error = NULL;
  IdeWorkbenchAddin *first;
  GCancellable *cancellable;
  Open *o;

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  cancellable = ide_task_get_cancellable (task);
  o = ide_task_get_task_data (task);

  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (o != NULL);
  g_assert (o->addins != NULL);
  g_assert (o->addins->len > 0);

  if ((info = g_file_query_info_finish (file, result, &error)))
    o->content_type = g_strdup (g_file_info_get_content_type (info));

  /* Remove unsupported addins while iterating backwards so that
   * we can preserve the ordering of the array as we go.
   */
  for (guint i = o->addins->len; i > 0; i--)
    {
      IdeWorkbenchAddin *addin = g_ptr_array_index (o->addins, i - 1);
      gint prio = G_MAXINT;

      if (!ide_workbench_addin_can_open (addin, o->file, o->content_type, &prio))
        {
          g_ptr_array_remove_index_fast (o->addins, i - 1);
          if (o->preferred == addin)
            g_clear_object (&o->preferred);
        }
    }

  if (o->addins->len == 0)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "No addins can open the file");
      return;
    }

  /*
   * Now sort the addins by priority, so that we can attempt to load them
   * in the preferred ordering.
   */
  g_ptr_array_sort_with_data (o->addins, sort_by_priority, o);

  /*
   * Ensure that we place the preferred at the head of the array, so
   * that it gets preference over default priorities.
   */
  if (o->preferred != NULL)
    {
      g_ptr_array_insert (o->addins, 0, g_object_ref (o->preferred));

      for (guint i = 1; i < o->addins->len; i++)
        {
          if (g_ptr_array_index (o->addins, i) == (gpointer)o->preferred)
            {
              g_ptr_array_remove_index (o->addins, i);
              break;
            }
        }
    }

  /* Now start requesting that addins attempt to load the file. */

  first = g_ptr_array_index (o->addins, 0);

  ide_workbench_addin_open_async (first,
                                  o->file,
                                  o->content_type,
                                  o->at_line,
                                  o->at_line_offset,
                                  o->flags,
                                  o->position,
                                  cancellable,
                                  ide_workbench_open_cb,
                                  g_steal_pointer (&task));
}

/**
 * ide_workbench_open_at_async:
 * @self: an #IdeWorkbench
 * @file: a #GFile
 * @hint: (nullable): an optional hint about what addin to use
 * @at_line: the line number to open at, or -1 to ignore
 * @at_line_offset: the line offset to open at, or -1 to ignore
 * @flags: optional #IdeBufferOpenFlags
 * @cancellable: (nullable): a #GCancellable
 * @callback: a #GAsyncReadyCallback to execute upon completion
 * @user_data: closure data for @callback
 *
 * Like ide_workbench_open_async(), this allows opening a file
 * within the workbench. However, it also allows specifying a
 * line and column offset within the file to focus. Usually, this
 * only makes sense for files that can be opened in an editor.
 *
 * @at_line and @at_line_offset may be < 0 to ignore the parameters.
 *
 * @flags may be ignored by some backends
 *
 * Use ide_workbench_open_finish() to receive teh result of this
 * asynchronous operation.
 */
void
ide_workbench_open_at_async (IdeWorkbench        *self,
                             GFile               *file,
                             const gchar         *hint,
                             gint                 at_line,
                             gint                 at_line_offset,
                             IdeBufferOpenFlags   flags,
                             PanelPosition       *position,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  g_autoptr(PanelPosition) local_position = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GPtrArray) addins = NULL;
  IdeWorkbench *other;
  Open *o;

  g_return_if_fail (IDE_IS_WORKBENCH (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (self->unloaded == FALSE);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (position == NULL)
    position = local_position = panel_position_new ();

  /* Possibly re-route opening the file to another workbench if we
   * discover the file is a better fit over there.
   */
  other = ide_application_find_workbench_for_file (IDE_APPLICATION_DEFAULT, file);

  if (other != NULL && other != self)
    {
      ide_workbench_open_at_async (other,
                                   file,
                                   hint,
                                   at_line,
                                   at_line_offset,
                                   flags,
                                   NULL,
                                   cancellable,
                                   callback,
                                   user_data);
      return;
    }

  /* Canonicalize parameters */
  if (at_line < 0)
    at_line = -1;
  if (at_line_offset < 0)
    at_line_offset = -1;

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_workbench_open_at_async);

  /*
   * Make sure we might have an addin to load after discovering
   * the files content-type.
   */
  if (!(addins = ide_workbench_collect_addins (self)) || addins->len == 0)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "No addins could open the file");
      return;
    }

  o = g_slice_new0 (Open);
  o->addins = g_ptr_array_ref (addins);
  if (hint != NULL)
    o->preferred = ide_workbench_find_addin (self, hint);
  o->file = g_object_ref (file);
  o->hint = g_strdup (hint);
  o->flags = flags;
  o->at_line = at_line;
  o->at_line_offset = at_line_offset;
  o->position = g_object_ref (position);
  ide_task_set_task_data (task, o, open_free);

  g_file_query_info_async (file,
                           G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                           G_FILE_QUERY_INFO_NONE,
                           G_PRIORITY_DEFAULT,
                           cancellable,
                           ide_workbench_open_query_info_cb,
                           g_steal_pointer (&task));
}

/**
 * ide_workbench_open_finish:
 * @self: an #IdeWorkbench
 * @result: a #GAsyncResult provided to callback
 * @error: a location for a #GError, or %NULL
 *
 * Completes a request to open a file using either
 * ide_workbench_open_async() or ide_workbench_open_at_async().
 *
 * Returns: %TRUE if the file was successfully opened; otherwise
 *   %FALSE and @error is set.
 */
gboolean
ide_workbench_open_finish (IdeWorkbench  *self,
                           GAsyncResult  *result,
                           GError       **error)
{
  g_return_val_if_fail (IDE_IS_WORKBENCH (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

gboolean
ide_workbench_open_at_finish (IdeWorkbench  *self,
                              GAsyncResult  *result,
                              GError       **error)
{
  return ide_workbench_open_finish (self, result, error);
}

gboolean
ide_workbench_open_all_finish (IdeWorkbench  *self,
                               GAsyncResult  *result,
                               GError       **error)
{
  return ide_workbench_open_finish (self, result, error);
}

/**
 * ide_workbench_get_current_workspace:
 * @self: a #IdeWorkbench
 *
 * Gets the most recently focused workspace, which may be used to
 * deliver events such as opening new pages.
 *
 * Returns: (transfer none) (nullable): an #IdeWorkspace or %NULL
 */
IdeWorkspace *
ide_workbench_get_current_workspace (IdeWorkbench *self)
{
  g_return_val_if_fail (IDE_IS_WORKBENCH (self), NULL);

  if (self->mru_queue.length > 0)
    return IDE_WORKSPACE (self->mru_queue.head->data);

  return NULL;
}

/**
 * ide_workbench_activate:
 * @self: a #IdeWorkbench
 *
 * This function will attempt to raise the most recently focused workspace.
 */
void
ide_workbench_activate (IdeWorkbench *self)
{
  IdeWorkspace *workspace;

  g_return_if_fail (IDE_IS_WORKBENCH (self));

  if ((workspace = ide_workbench_get_current_workspace (self)))
    ide_workbench_focus_workspace (self, workspace);
}

static void
ide_workbench_propagate_vcs_cb (PeasExtensionSet *set,
                                PeasPluginInfo   *plugin_info,
                                GObject    *exten,
                                gpointer          user_data)
{
  IdeWorkbenchAddin *addin = (IdeWorkbenchAddin *)exten;
  IdeVcs *vcs = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_WORKBENCH_ADDIN (addin));
  g_assert (!vcs || IDE_IS_VCS (vcs));

  ide_workbench_addin_vcs_changed (addin, vcs);
}

/**
 * ide_workbench_get_vcs:
 * @self: a #IdeWorkbench
 *
 * Gets the #IdeVcs that has been loaded for the workbench, if any.
 *
 * Returns: (transfer none) (nullable): an #IdeVcs or %NULL
 */
IdeVcs *
ide_workbench_get_vcs (IdeWorkbench *self)
{
  g_return_val_if_fail (IDE_IS_WORKBENCH (self), NULL);

  return self->vcs;
}

/**
 * ide_workbench_get_vcs_monitor:
 * @self: a #IdeWorkbench
 *
 * Gets the #IdeVcsMonitor for the workbench, if any.
 *
 * Returns: (transfer none) (nullable): an #IdeVcsMonitor or %NULL
 */
IdeVcsMonitor *
ide_workbench_get_vcs_monitor (IdeWorkbench *self)
{
  g_return_val_if_fail (IDE_IS_WORKBENCH (self), NULL);

  return self->vcs_monitor;
}

static void
remove_non_matching_vcs_cb (IdeObject *child,
                            IdeVcs    *vcs)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_OBJECT (child));
  g_assert (IDE_IS_VCS (vcs));

  if (IDE_IS_VCS (child) && IDE_VCS (child) != vcs)
    ide_object_destroy (child);
}

static void
ide_workbench_vcs_notify_branch_name_cb (IdeWorkbench *self,
                                         GParamSpec   *pspec,
                                         IdeVcs       *vcs)
{
  IdeBuildManager *build_manager;

  IDE_ENTRY;

  g_assert (IDE_IS_WORKBENCH (self));
  g_assert (IDE_IS_VCS (vcs));

  if (!ide_workbench_has_project (self))
    IDE_EXIT;

  build_manager = ide_build_manager_from_context (self->context);
  ide_build_manager_invalidate (build_manager);

  IDE_EXIT;
}

/**
 * ide_workbench_set_vcs:
 * @self: a #IdeWorkbench
 * @vcs: (nullable): an #IdeVcs
 *
 * Sets the #IdeVcs for the workbench.
 */
void
ide_workbench_set_vcs (IdeWorkbench *self,
                       IdeVcs       *vcs)
{
  g_autoptr(IdeVcs) local_vcs = NULL;
  g_autoptr(GFile) local_workdir = NULL;
  g_autoptr(GFile) workdir = NULL;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_WORKBENCH (self));
  g_return_if_fail (!vcs || IDE_IS_VCS (vcs));

  if (self->vcs != NULL && vcs == self->vcs)
    return;

  if (vcs == NULL)
    {
      local_workdir = ide_context_ref_workdir (self->context);
      vcs = local_vcs = IDE_VCS (ide_directory_vcs_new (local_workdir));
    }

  g_set_object (&self->vcs, vcs);
  ide_object_append (IDE_OBJECT (self->context), IDE_OBJECT (vcs));
  ide_object_foreach (IDE_OBJECT (self->context),
                      (GFunc)remove_non_matching_vcs_cb,
                      vcs);

  if ((workdir = ide_vcs_get_workdir (vcs)))
    ide_context_set_workdir (self->context, workdir);

  ide_vcs_monitor_set_vcs (self->vcs_monitor, self->vcs);

  peas_extension_set_foreach (self->addins,
                              ide_workbench_propagate_vcs_cb,
                              self->vcs);

  g_signal_connect_object (vcs,
                           "notify::branch-name",
                           G_CALLBACK (ide_workbench_vcs_notify_branch_name_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_VCS]);
}

/**
 * ide_workbench_get_build_system:
 * @self: a #IdeWorkbench
 *
 * Gets the #IdeBuildSystem for the workbench, if any.
 *
 * Returns: (transfer none) (nullable): an #IdeBuildSystem or %NULL
 */
IdeBuildSystem *
ide_workbench_get_build_system (IdeWorkbench *self)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_WORKBENCH (self), NULL);

  return self->build_system;
}

static void
remove_non_matching_build_systems_cb (IdeObject      *child,
                                      IdeBuildSystem *build_system)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_OBJECT (child));
  g_assert (IDE_IS_BUILD_SYSTEM (build_system));

  if (IDE_IS_BUILD_SYSTEM (child) && IDE_BUILD_SYSTEM (child) != build_system)
    ide_object_destroy (child);
}

/**
 * ide_workbench_set_build_system:
 * @self: a #IdeWorkbench
 * @build_system: (nullable): an #IdeBuildSystem or %NULL
 *
 * Sets the #IdeBuildSystem for the workbench.
 *
 * If @build_system is %NULL, then a fallback build system will be used
 * instead. It does not provide building capabilities, but allows for some
 * components that require a build system to continue functioning.
 */
void
ide_workbench_set_build_system (IdeWorkbench   *self,
                                IdeBuildSystem *build_system)
{
  g_autoptr(IdeBuildSystem) local_build_system = NULL;
  IdeConfigManager *config_manager;
  IdeBuildManager *build_manager;

  g_return_if_fail (IDE_IS_WORKBENCH (self));
  g_return_if_fail (!build_system || IDE_IS_BUILD_SYSTEM (build_system));

  if (build_system == self->build_system)
    return;

  /* We want there to always be a build system available so that various
   * plugins don't need lots of extra code to handle the %NULL case. So
   * if @build_system is %NULL, then we'll create a fallback build system
   * and assign that instead.
   */

  if (build_system == NULL)
    build_system = local_build_system = ide_fallback_build_system_new ();

  /* We want to add our new build system before removing the old build
   * system to ensure there is always an #IdeBuildSystem child of the
   * IdeContext.
   */
  g_set_object (&self->build_system, build_system);
  ide_object_append (IDE_OBJECT (self->context), IDE_OBJECT (build_system));

  /* Now remove any previous build-system from the context */
  ide_object_foreach (IDE_OBJECT (self->context),
                      (GFunc)remove_non_matching_build_systems_cb,
                      build_system);

  /* Ask the config manager to invalidate active configs which
   * might rely on the active build system.
   */
  if ((config_manager = ide_context_peek_child_typed (self->context, IDE_TYPE_CONFIG_MANAGER)))
    ide_config_manager_invalidate (config_manager);

  /* Ask the build-manager to setup a new pipeline */
  if ((build_manager = ide_context_peek_child_typed (self->context, IDE_TYPE_BUILD_MANAGER)))
    ide_build_manager_invalidate (build_manager);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BUILD_SYSTEM]);
}

/**
 * ide_workbench_get_workspace_by_type:
 * @self: a #IdeWorkbench
 * @type: a #GType of a subclass of #IdeWorkspace
 *
 * Gets the most-recently-used workspace that matches @type.
 *
 * Returns: (transfer none) (nullable): an #IdeWorkspace or %NULL
 */
IdeWorkspace *
ide_workbench_get_workspace_by_type (IdeWorkbench *self,
                                     GType         type)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_WORKBENCH (self), NULL);
  g_return_val_if_fail (g_type_is_a (type, IDE_TYPE_WORKSPACE), NULL);

  for (const GList *iter = self->mru_queue.head; iter; iter = iter->next)
    {
      if (G_TYPE_CHECK_INSTANCE_TYPE (iter->data, type))
        return IDE_WORKSPACE (iter->data);
    }

  return NULL;
}

gboolean
_ide_workbench_is_last_workspace (IdeWorkbench *self,
                                  IdeWorkspace *workspace)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (IDE_IS_WORKBENCH (self), FALSE);

  /* Always close when primary workspace is closed */
  if (IDE_IS_PRIMARY_WORKSPACE (workspace))
    return TRUE;

  return self->mru_queue.length == 1 &&
         g_queue_peek_head (&self->mru_queue) == (gpointer)workspace;
}

/**
 * ide_workbench_has_project:
 * @self: a #IdeWorkbench
 *
 * Returns %TRUE if a project is loaded (or currently loading) in the
 * workbench.
 *
 * Returns: %TRUE if the workbench has a project
 */
gboolean
ide_workbench_has_project (IdeWorkbench *self)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (IDE_IS_WORKBENCH (self), FALSE);

  return self->project_info != NULL;
}

/**
 * ide_workbench_addin_find_by_module_name:
 * @workbench: an #IdeWorkbench
 * @module_name: the name of the addin module
 *
 * Finds the addin (if any) matching the plugin's @module_name.
 *
 * Returns: (transfer none) (nullable): an #IdeWorkbenchAddin or %NULL
 */
IdeWorkbenchAddin *
ide_workbench_addin_find_by_module_name (IdeWorkbench *workbench,
                                         const gchar  *module_name)
{
  PeasPluginInfo *plugin_info;
  GObject *ret = NULL;
  PeasEngine *engine;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_WORKBENCH (workbench), NULL);
  g_return_val_if_fail (module_name != NULL, NULL);

  if (workbench->addins == NULL)
    return NULL;

  engine = peas_engine_get_default ();

  if ((plugin_info = peas_engine_get_plugin_info (engine, module_name)))
    ret = peas_extension_set_get_extension (workbench->addins, plugin_info);

  return IDE_WORKBENCH_ADDIN (ret);
}

static void
ide_workbench_resolve_file_worker (IdeTask      *task,
                                   gpointer      source_object,
                                   gpointer      task_data,
                                   GCancellable *cancellable)
{
  ResolveFile *rf = task_data;
  g_autofree gchar *basename = NULL;

  g_assert (IDE_IS_TASK (task));
  g_assert (IDE_IS_WORKBENCH (source_object));
  g_assert (rf != NULL);
  g_assert (rf->roots != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  for (guint i = 0; i < rf->roots->len; i++)
    {
      GFile *root = g_ptr_array_index (rf->roots, i);
      g_autoptr(GFile) child = g_file_get_child (root, rf->path);

      if (g_file_query_exists (child, cancellable))
        {
          ide_task_return_pointer (task, g_steal_pointer (&child), g_object_unref);
          return;
        }
    }

  basename = g_path_get_basename (rf->path);

  for (guint i = 0; i < rf->roots->len; i++)
    {
      GFile *root = g_ptr_array_index (rf->roots, i);
      g_autoptr(GPtrArray) found = NULL;

      found = ide_g_file_find_with_depth (root, basename, 0, cancellable);
      IDE_PTR_ARRAY_SET_FREE_FUNC (found, g_object_unref);

      if (found != NULL && found->len > 0)
        {
          GFile *match = g_ptr_array_index (found, 0);
          ide_task_return_pointer (task, g_object_ref (match), g_object_unref);
          return;
        }
    }

  ide_task_return_new_error (task,
                             G_IO_ERROR,
                             G_IO_ERROR_NOT_FOUND,
                             "Failed to locate file %s",
                             basename);
}

/**
 * ide_workbench_resolve_file_async:
 * @self: a #IdeWorkbench
 * @filename: the filename to discover
 *
 * This function will try to locate a given file based on the filename,
 * possibly resolving it from a build directory, or source directory.
 *
 * If no file was discovered, some attempt will be made to locate a file
 * that matches appropriately.
 */
void
ide_workbench_resolve_file_async (IdeWorkbench        *self,
                                  const gchar         *filename,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  ResolveFile *rf;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_WORKBENCH (self));
  g_return_if_fail (filename != NULL);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_workbench_resolve_file_async);

  rf = g_slice_new0 (ResolveFile);
  rf->roots = g_ptr_array_new_with_free_func (g_object_unref);
  rf->path = g_strdup (filename);

  g_ptr_array_add (rf->roots, ide_context_ref_workdir (self->context));

  if (ide_workbench_has_project (self))
    {
      IdeBuildManager *build_manager = ide_build_manager_from_context (self->context);
      IdePipeline *pipeline = ide_build_manager_get_pipeline (build_manager);

      if (pipeline != NULL)
        {
          const gchar *builddir = ide_pipeline_get_builddir (pipeline);

          g_ptr_array_add (rf->roots, g_file_new_for_path (builddir));
        }
    }

  ide_task_set_task_data (task, rf, resolve_file_free);
  ide_task_run_in_thread (task, ide_workbench_resolve_file_worker);

  IDE_EXIT;
}

/**
 * ide_workbench_resolve_file_finish:
 * @self: a #IdeWorkbench
 * @result: a #GAsyncResult
 * @error: a location for a #GError
 *
 * Completes an asynchronous request to ide_workbench_resolve_file_async().
 *
 * Returns: (transfer full): a #GFile, or %NULL and @error is set
 */
GFile *
ide_workbench_resolve_file_finish (IdeWorkbench  *self,
                                   GAsyncResult  *result,
                                   GError       **error)
{
  GFile *ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_WORKBENCH (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  IDE_RETURN (g_steal_pointer (&ret));
}

static void
ide_workbench_action_tweaks (gpointer    instance,
                             const char *action_name,
                             GVariant   *param)
{
  IdeWorkbench *self = instance;
  g_autoptr(IdeTweaks) tweaks = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) tweaks_file = NULL;
  IdeTweaksWindow *window;
  const char *project_id;

  IDE_ENTRY;

  g_assert (IDE_IS_WORKBENCH (self));

  tweaks = ide_tweaks_new ();

  /* We are in project mode, so expose the project-id */
  project_id = ide_context_dup_project_id (self->context);
  ide_tweaks_set_project_id (tweaks, project_id);

  /* Ensure access to the context and workbench objects */
  ide_tweaks_expose_object (tweaks, "context", G_OBJECT (self->context));
  ide_tweaks_expose_object (tweaks, "workbench", G_OBJECT (self));

  /* Load our base tweaks scaffolding */
  tweaks_file = g_file_new_for_uri ("resource:///org/gnome/libide-gui/tweaks.ui");
  ide_tweaks_load_from_file (tweaks, tweaks_file, NULL, &error);
  g_assert_no_error (error);

  /* Now display window */
  window = g_object_new (IDE_TYPE_TWEAKS_WINDOW,
                         "tweaks", tweaks,
                         NULL);
  gtk_window_group_add_window (GTK_WINDOW_GROUP (self), GTK_WINDOW (window));
  gtk_window_present (GTK_WINDOW (window));

  IDE_EXIT;
}

static void
ide_workbench_action_configure (gpointer    instance,
                                const char *action_name,
                                GVariant   *param)
{
  IdeWorkbench *self = instance;
  const char *page = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_WORKBENCH (self));

  if (param && g_variant_is_of_type (param, G_VARIANT_TYPE_STRING))
    page = g_variant_get_string (param, NULL);

  ide_show_tweaks (self->context, page);
}

IdeWorkspace *
_ide_workbench_create_secondary (IdeWorkbench *self)
{
  IdeWorkspace *workspace;
  GType secondary_type;

  g_return_val_if_fail (IDE_IS_WORKBENCH (self), NULL);

  /* TODO: allow secondary workspace type to be set so we don't
   *       have this layering violation.
   */

  secondary_type = g_type_from_name ("IdeEditorWorkspace");
  g_return_val_if_fail (secondary_type != G_TYPE_INVALID, NULL);

  workspace = g_object_new (secondary_type,
                            "application", IDE_APPLICATION_DEFAULT,
                            NULL);
  ide_workbench_add_workspace (self, IDE_WORKSPACE (workspace));

  return workspace;
}

void
_ide_workbench_set_session (IdeWorkbench *self,
                            IdeSession   *session)
{
  g_return_if_fail (IDE_IS_WORKBENCH (self));

  g_set_object (&self->session, session);
}
