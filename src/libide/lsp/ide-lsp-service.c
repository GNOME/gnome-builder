/* ide-lsp-service.c
 *
 * Copyright 2021 James Westman <james@jwestman.net>
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

#define G_LOG_DOMAIN "ide-lsp-service"

#include "config.h"

#include <glib/gi18n.h>

#include "ide-lsp-service.h"

/**
 * SECTION:ide-lsp-service
 * @title: IdeLspService
 * @short_description: Service integration for LSPs
 */

typedef struct
{
  IdeSubprocessSupervisor *supervisor;
  IdeLspClient *client;
  char *program;
  char **search_path;
  guint has_started : 1;
  guint inherit_stderr : 1;
  guint has_seen_autostart : 1;
} IdeLspServicePrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (IdeLspService, ide_lsp_service, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_CLIENT,
  PROP_INHERIT_STDERR,
  PROP_PROGRAM,
  PROP_SEARCH_PATH,
  PROP_SUPERVISOR,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
ide_lsp_service_stop (IdeLspService *self)
{
  IdeLspServicePrivate *priv = ide_lsp_service_get_instance_private (self);
  gboolean notify_client = FALSE;
  gboolean notify_supervisor = FALSE;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_LSP_SERVICE (self));

  if (priv->has_started)
    g_debug ("Stopping LSP client %s", G_OBJECT_TYPE_NAME (self));

  if (priv->client != NULL)
    {
      ide_lsp_client_stop (priv->client);
      ide_object_destroy (IDE_OBJECT (priv->client));
      priv->client = NULL;
      notify_client = TRUE;
    }

  if (priv->supervisor != NULL)
    {
      ide_subprocess_supervisor_stop (priv->supervisor);
      g_clear_object (&priv->supervisor);
      notify_supervisor = TRUE;
    }

  priv->has_started = FALSE;

  if (notify_client)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CLIENT]);

  if (notify_supervisor)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SUPERVISOR]);

  IDE_EXIT;
}


static void
ide_lsp_service_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeLspService *self = IDE_LSP_SERVICE (object);
  IdeLspServicePrivate *priv = ide_lsp_service_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_CLIENT:
      g_value_set_object (value, priv->client);
      break;

    case PROP_PROGRAM:
      g_value_set_string (value, priv->program);
      break;

    case PROP_SEARCH_PATH:
      g_value_set_boxed (value, priv->search_path);
      break;

    case PROP_SUPERVISOR:
      g_value_set_object (value, priv->supervisor);
      break;

    case PROP_INHERIT_STDERR:
      g_value_set_boolean (value, priv->inherit_stderr);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_lsp_service_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  IdeLspService *self = IDE_LSP_SERVICE (object);

  switch (prop_id)
    {
    case PROP_INHERIT_STDERR:
      ide_lsp_service_set_inherit_stderr (self, g_value_get_boolean (value));
      break;

    case PROP_PROGRAM:
      ide_lsp_service_set_program (self, g_value_get_string (value));
      break;

    case PROP_SEARCH_PATH:
      ide_lsp_service_set_search_path (self, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_lsp_service_destroy (IdeObject *object)
{
  IdeLspService *self = (IdeLspService *)object;
  IdeLspServicePrivate *priv = ide_lsp_service_get_instance_private (self);

  IDE_ENTRY;

  ide_lsp_service_stop (self);

  g_clear_object (&priv->supervisor);
  g_clear_object (&priv->client);
  g_clear_pointer (&priv->program, g_free);
  g_clear_pointer (&priv->search_path, g_strfreev);

  IDE_OBJECT_CLASS (ide_lsp_service_parent_class)->destroy (object);

  IDE_EXIT;
}

static void
ide_lsp_service_prepare_tooling (IdeLspService *self,
                                 IdeRunContext *run_context)
{
  IdeBuildSystem *build_system;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_LSP_SERVICE (self));
  g_assert (IDE_IS_RUN_CONTEXT (run_context));

  if ((context = ide_object_get_context (IDE_OBJECT (self))) &&
      ide_context_has_project (context) &&
      (build_system = ide_build_system_from_context (context)))
    ide_build_system_prepare_tooling (build_system, run_context);

  IDE_EXIT;
}

static IdeSubprocessLauncher *
ide_lsp_service_create_launcher (IdeLspService    *self,
                                 IdePipeline      *pipeline,
                                 GSubprocessFlags  flags)
{
  IdeLspServicePrivate *priv = ide_lsp_service_get_instance_private (self);
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeContext) context = NULL;
  const char *srcdir;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_LSP_SERVICE (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  if (priv->program == NULL)
    IDE_RETURN (NULL);

  context = ide_object_ref_context (IDE_OBJECT (self));
  srcdir = ide_pipeline_get_srcdir (pipeline);

  /* First try in the build environment */
  if (ide_pipeline_contains_program_in_path (pipeline, priv->program, NULL))
    {
      g_autoptr(IdeRunContext) run_context = ide_run_context_new ();

      ide_pipeline_prepare_run_context (pipeline, run_context);
      ide_lsp_service_prepare_tooling (self, run_context);
      ide_run_context_append_argv (run_context, priv->program);
      ide_run_context_set_cwd (run_context, srcdir);

      IDE_LSP_SERVICE_GET_CLASS (self)->prepare_run_context (self, pipeline, run_context);

      if ((launcher = ide_run_context_end (run_context, NULL)))
        {
          ide_subprocess_launcher_set_flags (launcher, flags);
          IDE_RETURN (g_steal_pointer (&launcher));
        }
    }

  /* Then try on the host if we find it there */
  if (launcher == NULL)
    {
      IdeRuntimeManager *runtime_manager = ide_runtime_manager_from_context (context);
      IdeRuntime *host = ide_runtime_manager_get_runtime (runtime_manager, "host");

      if (ide_runtime_contains_program_in_path (host, priv->program, NULL))
        {
          g_autoptr(IdeRunContext) run_context = NULL;

          run_context = ide_run_context_new ();
          ide_runtime_prepare_to_build (host, pipeline, run_context);
          ide_lsp_service_prepare_tooling (self, run_context);
          ide_run_context_append_argv (run_context, priv->program);
          ide_run_context_set_cwd (run_context, srcdir);

          IDE_LSP_SERVICE_GET_CLASS (self)->prepare_run_context (self, pipeline, run_context);

          if ((launcher = ide_run_context_end (run_context, NULL)))
            {
              ide_subprocess_launcher_set_flags (launcher, flags);
              IDE_RETURN (g_steal_pointer (&launcher));
            }
        }

      /* If we didn't find it in the host, we might have an alternate
       * search path we can try.
       */
      if (priv->search_path)
        {
          for (guint i = 0; priv->search_path[i]; i++)
            {
              g_autofree char *path = g_build_filename (priv->search_path[i], priv->program, NULL);

              if (g_file_test (path, G_FILE_TEST_IS_EXECUTABLE))
                {
                  g_autoptr(IdeRunContext) run_context = NULL;

                  run_context = ide_run_context_new ();
                  ide_runtime_prepare_to_build (host, pipeline, run_context);
                  ide_lsp_service_prepare_tooling (self, run_context);
                  ide_run_context_append_argv (run_context, path);
                  ide_run_context_set_cwd (run_context, srcdir);

                  IDE_LSP_SERVICE_GET_CLASS (self)->prepare_run_context (self, pipeline, run_context);

                  if ((launcher = ide_run_context_end (run_context, NULL)))
                    {
                      ide_subprocess_launcher_set_flags (launcher, flags);
                      IDE_RETURN (g_steal_pointer (&launcher));
                    }
                }
            }
        }
    }

  /* Finally fallback to Builder's execution runtime */
  if (launcher == NULL)
    {
      g_autofree char *path = NULL;

      if ((path = g_find_program_in_path (priv->program)))
        {
          g_autoptr(IdeRunContext) run_context = ide_run_context_new ();

          ide_lsp_service_prepare_tooling (self, run_context);
          ide_run_context_append_argv (run_context, path);
          ide_run_context_set_cwd (run_context, srcdir);

          IDE_LSP_SERVICE_GET_CLASS (self)->prepare_run_context (self, pipeline, run_context);

          if ((launcher = ide_run_context_end (run_context, NULL)))
            {
              ide_subprocess_launcher_set_flags (launcher, flags);
              IDE_RETURN (g_steal_pointer (&launcher));
            }
        }
    }

  IDE_RETURN (NULL);
}

G_NORETURN static void
ide_lsp_service_real_configure_client (IdeLspService *self,
                                       IdeLspClient  *client)
{
  g_assert (IDE_IS_LSP_SERVICE (self));
  g_assert (IDE_IS_LSP_CLIENT (client));

  g_assert_not_reached ();
}

static void
ide_lsp_service_real_prepare_run_context (IdeLspService *self,
                                          IdePipeline   *pipeline,
                                          IdeRunContext *run_context)
{
  IDE_ENTRY;

  g_assert (IDE_IS_LSP_SERVICE (self));
  g_assert (!pipeline || IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_RUN_CONTEXT (run_context));

  IDE_EXIT;
}

static void
ide_lsp_service_real_configure_supervisor (IdeLspService           *self,
                                           IdeSubprocessSupervisor *supervisor)
{
  IDE_ENTRY;

  g_assert (IDE_IS_LSP_SERVICE (self));
  g_assert (IDE_IS_SUBPROCESS_SUPERVISOR (supervisor));

  IDE_EXIT;
}

static void
ide_lsp_service_class_init (IdeLspServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *ide_object_class = IDE_OBJECT_CLASS (klass);
  IdeLspServiceClass *service_class = IDE_LSP_SERVICE_CLASS (klass);

  object_class->get_property = ide_lsp_service_get_property;
  object_class->set_property = ide_lsp_service_set_property;

  ide_object_class->destroy = ide_lsp_service_destroy;

  service_class->configure_client = ide_lsp_service_real_configure_client;
  service_class->configure_supervisor = ide_lsp_service_real_configure_supervisor;
  service_class->prepare_run_context = ide_lsp_service_real_prepare_run_context;

  /**
   * IdeLspService:client:
   *
   * The [class@LspClient] provided by the service, or %NULL if it has not been started yet.
   */
  properties[PROP_CLIENT] =
    g_param_spec_object ("client",
                         "Client",
                         "Client",
                         IDE_TYPE_LSP_CLIENT,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * IdeLspService:program:
   *
   * The "program" property contains the name of the executable to
   * launch. If this is set, the create-launcher signal will use it
   * to locate and execute the program if found.
   */
  properties[PROP_PROGRAM] =
    g_param_spec_string ("program",
                         "Program",
                         "The program executable name",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeLspService:search-path:
   *
   * An alternate search path to locate the program on the host.
   */
  properties[PROP_SEARCH_PATH] =
    g_param_spec_boxed ("search-path",
                        "Search Path",
                        "Search Path",
                        G_TYPE_STRV,
                        (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeLspService:supervisor:
   *
   * The [class@SubprocessSupervisor] that manages the language server process, or %NULL if the
   * service is not running.
   */
  properties[PROP_SUPERVISOR] =
    g_param_spec_object ("supervisor",
                         "Supervisor",
                         "Supervisor",
                         IDE_TYPE_LSP_CLIENT,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * IdeLspService:inherit-stderr:
   *
   * If inherit-stderr is enabled, the language server process's stderr is passed through to Builder's.
   */
  properties[PROP_INHERIT_STDERR] =
    g_param_spec_boolean ("inherit-stderr",
                          "Inherit stderr",
                          "Inherit stderr",
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_lsp_service_init (IdeLspService *self)
{
}

/**
 * ide_lsp_service_get_inherit_stderr:
 * @self: a [class@LspService]
 *
 * Gets whether the language server process's stderr output should be passed to Builder's.
 *
 * Returns: %TRUE if the subprocess inherits stderr, otherwise %FALSE
 */
gboolean
ide_lsp_service_get_inherit_stderr (IdeLspService *self)
{
  IdeLspServicePrivate *priv = ide_lsp_service_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (IDE_IS_LSP_SERVICE (self), FALSE);

  return priv->inherit_stderr;
}

/**
 * ide_lsp_service_set_inherit_stderr:
 * @self: a [class@LspService]
 * @inherit_stderr: %TRUE to enable stderr, %FALSE to disable it
 *
 * Gets whether the language server process's stderr output should be passed to Builder's.
 */
void
ide_lsp_service_set_inherit_stderr (IdeLspService *self,
                                    gboolean       inherit_stderr)
{
  IdeLspServicePrivate *priv = ide_lsp_service_get_instance_private (self);

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_LSP_SERVICE (self));

  inherit_stderr = !!inherit_stderr;

  if (priv->inherit_stderr != inherit_stderr)
    {
      priv->inherit_stderr = inherit_stderr;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_INHERIT_STDERR]);
    }

  IDE_EXIT;
}

static void
on_supervisor_exited_cb (IdeLspService           *self,
                         IdeSubprocess           *subprocess,
                         IdeSubprocessSupervisor *supervisor)
{
  IdeLspServicePrivate *priv = ide_lsp_service_get_instance_private (self);

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_LSP_SERVICE (self));
  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (IDE_IS_SUBPROCESS_SUPERVISOR (supervisor));

  ide_object_message (IDE_OBJECT (self),
                      /* translators: %s is replaced with the name of the language server */
                      _("Language server “%s” exited"),
                      priv->program);

  IDE_EXIT;
}

static void
on_supervisor_spawned_cb (IdeLspService           *self,
                          IdeSubprocess           *subprocess,
                          IdeSubprocessSupervisor *supervisor)
{
  IdeLspServicePrivate *priv = ide_lsp_service_get_instance_private (self);
  IdeLspServiceClass *klass;
  g_autoptr(GIOStream) iostream = NULL;
  g_autoptr(IdeLspClient) client = NULL;
  g_autoptr(GInputStream) to_stdout = NULL;
  g_autoptr(GOutputStream) to_stdin = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_LSP_SERVICE (self));
  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (IDE_IS_SUBPROCESS_SUPERVISOR (supervisor));

  klass = IDE_LSP_SERVICE_GET_CLASS (self);

  to_stdin = ide_subprocess_get_stdin_pipe (subprocess);
  to_stdout = ide_subprocess_get_stdout_pipe (subprocess);
  iostream = g_simple_io_stream_new (to_stdout, to_stdin);

  if (priv->client != NULL)
    {
      ide_lsp_client_stop (priv->client);
      ide_object_destroy (IDE_OBJECT (priv->client));
    }

  ide_object_message (IDE_OBJECT (self),
                      /* translators: the first %s is replaced with the language server name, second %s with PID */
                      _("Language server “%s” spawned as process %s"),
                      priv->program,
                      ide_subprocess_get_identifier (subprocess));

  client = ide_lsp_client_new (iostream);
  ide_object_append (IDE_OBJECT (self), IDE_OBJECT (client));

  klass->configure_client (self, client);

  ide_lsp_client_start (client);

  priv->client = g_steal_pointer (&client);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CLIENT]);

  IDE_EXIT;
}

static void
ensure_started (IdeLspService *self,
                IdeContext    *context)
{
  IdeLspServicePrivate *priv = ide_lsp_service_get_instance_private (self);
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocessSupervisor) supervisor = NULL;
  g_autoptr(GSettings) settings = NULL;
  IdeBuildManager *build_manager;
  IdeLspServiceClass *klass;
  IdePipeline *pipeline = NULL;
  GSubprocessFlags flags;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_LSP_SERVICE (self));
  g_assert (IDE_IS_CONTEXT (context));

  if (priv->has_started)
    IDE_EXIT;

  g_assert (priv->supervisor == NULL);
  g_assert (priv->client == NULL);

  klass = IDE_LSP_SERVICE_GET_CLASS (self);
  build_manager = ide_build_manager_from_context (context);
  pipeline = ide_build_manager_get_pipeline (build_manager);
  settings = g_settings_new ("org.gnome.builder");

  /* Delay until pipeline is ready */
  if (!ide_pipeline_is_ready (pipeline))
    IDE_EXIT;

  flags = G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE;
  if (!priv->inherit_stderr && !g_settings_get_boolean (settings, "lsp-inherit-stderr"))
    flags |= G_SUBPROCESS_FLAGS_STDERR_SILENCE;

  /* Create launcher by applying run context */
  if (!(launcher = ide_lsp_service_create_launcher (self, pipeline, flags)))
    IDE_EXIT;

  supervisor = ide_subprocess_supervisor_new ();
  ide_subprocess_supervisor_set_launcher (supervisor, launcher);
  g_signal_connect_object (supervisor,
                           "spawned",
                           G_CALLBACK (on_supervisor_spawned_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (supervisor,
                           "exited",
                           G_CALLBACK (on_supervisor_exited_cb),
                           self,
                           G_CONNECT_SWAPPED);

  priv->has_started = TRUE;

  klass->configure_supervisor (self, supervisor);
  ide_subprocess_supervisor_start (supervisor);
  priv->supervisor = g_steal_pointer (&supervisor);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SUPERVISOR]);

  IDE_EXIT;
}

/**
 * ide_lsp_service_restart:
 * @self: a [class@LspService]
 *
 * Restarts the service and its associated process.
 */
void
ide_lsp_service_restart (IdeLspService *self)
{
  IdeContext *context;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_LSP_SERVICE (self));
  g_return_if_fail (!ide_object_in_destruction (IDE_OBJECT (self)));

  g_debug ("Request to restart LSP service %s",
           G_OBJECT_TYPE_NAME (self));

  ide_lsp_service_stop (self);

  if ((context = ide_object_get_context (IDE_OBJECT (self))))
    ensure_started (self, context);

  IDE_EXIT;
}

static void
on_pipeline_loaded_cb (IdeLspService *self,
                       IdePipeline   *pipeline)
{
  IdeBuildManager *build_manager;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_LSP_SERVICE (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  if (!(context = ide_object_get_context (IDE_OBJECT (self))) ||
      !(build_manager = ide_build_manager_from_context (context)) ||
      pipeline != ide_build_manager_get_pipeline (build_manager) ||
      ide_pipeline_is_ready (pipeline))
    g_signal_handlers_disconnect_by_func (pipeline,
                                          G_CALLBACK (on_pipeline_loaded_cb),
                                          self);

  if (ide_pipeline_is_ready (pipeline))
    {
      g_debug ("Pipeline has completed loading, restarting LSP service %s",
               G_OBJECT_TYPE_NAME (self));
      ide_lsp_service_restart (self);
    }

  IDE_EXIT;
}

static void
on_notify_pipeline_cb (IdeLspService   *self,
                       GParamSpec      *pspec,
                       IdeBuildManager *build_manager)
{
  IdeLspServicePrivate *priv = ide_lsp_service_get_instance_private (self);
  IdePipeline *pipeline;

  IDE_ENTRY;

  g_assert (IDE_IS_LSP_SERVICE (self));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  /* If the service has not yet started, and there have been no
   * requests for providers which force starting of the service,
   * then just silently ignore this so we don't auto-spawn services
   * unnecessarily.
   */
  if (!priv->has_started && !priv->has_seen_autostart)
    IDE_EXIT;

  g_debug ("Pipeline changed, requesting LSP service %s restart",
           G_OBJECT_TYPE_NAME (self));

  ide_lsp_service_stop (self);

  if ((pipeline = ide_build_manager_get_pipeline (build_manager)))
    {
      if (!ide_pipeline_is_ready (pipeline))
        g_signal_connect_object (pipeline,
                                 "loaded",
                                 G_CALLBACK (on_pipeline_loaded_cb),
                                 self,
                                 G_CONNECT_SWAPPED);
      else
        ide_lsp_service_restart (self);
    }

  IDE_EXIT;
}

static void
ide_lsp_service_class_bind_client_internal (IdeLspServiceClass *klass,
                                            IdeObject          *provider,
                                            gboolean            autostart)
{
  IdeContext *context;
  GParamSpec *pspec;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_LSP_SERVICE_CLASS (klass));
  g_return_if_fail (IDE_IS_OBJECT (provider));

  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (provider), "client");
  g_return_if_fail (pspec != NULL && g_type_is_a (pspec->value_type, IDE_TYPE_LSP_CLIENT));

  context = ide_object_get_context (provider);
  g_return_if_fail (context != NULL);
  g_return_if_fail (IDE_IS_CONTEXT (context));

  /* If the context has a project (ie: not editor mode), then we
   * want to track changes to the pipeline so we can reload the
   * language server automatically.
   */
  if (ide_context_has_project (context))
    {
      IdeLspServicePrivate *priv;
      IdeBuildManager *build_manager = ide_build_manager_from_context (context);
      g_autoptr(IdeLspService) service = NULL;
      gboolean do_notify = FALSE;

      if (!(service = ide_object_get_child_typed (IDE_OBJECT (context), G_OBJECT_CLASS_TYPE (klass))))
        {
          service = ide_object_ensure_child_typed (IDE_OBJECT (context), G_OBJECT_CLASS_TYPE (klass));
          g_signal_connect_object (build_manager,
                                   "notify::pipeline",
                                   G_CALLBACK (on_notify_pipeline_cb),
                                   service,
                                   G_CONNECT_SWAPPED);
          do_notify = TRUE;
        }

      priv = ide_lsp_service_get_instance_private (service);
      priv->has_seen_autostart |= autostart;
      do_notify |= (autostart && !priv->has_started);

      if (do_notify)
        on_notify_pipeline_cb (service, NULL, build_manager);

      g_object_bind_property (service, "client", provider, "client", G_BINDING_SYNC_CREATE);
    }

  IDE_EXIT;
}

/**
 * ide_lsp_service_class_bind_client:
 * @klass: a [class@LspService] class structure
 * @provider: an [class@Object]
 *
 * Binds the "client" property of @property to its context's instance of
 * @klass. If the language server is not running yet, it will be started.
 */
void
ide_lsp_service_class_bind_client (IdeLspServiceClass *klass,
                                   IdeObject          *provider)
{
  ide_lsp_service_class_bind_client_internal (klass, provider, TRUE);
}

/**
 * ide_lsp_service_class_bind_client_lazy:
 * @klass: a [class@LspService] class structure
 * @provider: an [class@Object]
 *
 * Like ide_lsp_service_bind_client() but will not immediately spawn
 * the language server.
 */
void
ide_lsp_service_class_bind_client_lazy (IdeLspServiceClass *klass,
                                        IdeObject          *provider)
{
  ide_lsp_service_class_bind_client_internal (klass, provider, FALSE);
}

const char *
ide_lsp_service_get_program (IdeLspService *self)
{
  IdeLspServicePrivate *priv = ide_lsp_service_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_LSP_SERVICE (self), NULL);

  return priv->program;
}

void
ide_lsp_service_set_program (IdeLspService *self,
                             const char    *program)
{
  IdeLspServicePrivate *priv = ide_lsp_service_get_instance_private (self);

  g_return_if_fail (IDE_IS_LSP_SERVICE (self));

  if (g_set_str (&priv->program, program))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PROGRAM]);
}

const char * const *
ide_lsp_service_get_search_path (IdeLspService *self)
{
  IdeLspServicePrivate *priv = ide_lsp_service_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_LSP_SERVICE (self), NULL);

  return (const char * const *)priv->search_path;
}

/**
 * ide_lsp_service_set_search_path:
 * @self: a #IdeLspService
 * @search_path: (array zero-terminated=1) (element-type utf8) (nullable):
 *   a search path to apply when searching the host or %NULL.
 *
 * Sets an alternate search path to use when discovering programs on
 * the host system.
 */
void
ide_lsp_service_set_search_path (IdeLspService      *self,
                                 const char * const *search_path)
{
  IdeLspServicePrivate *priv = ide_lsp_service_get_instance_private (self);

  g_return_if_fail (IDE_IS_LSP_SERVICE (self));

  if (ide_set_strv (&priv->search_path, search_path))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SEARCH_PATH]);
}
