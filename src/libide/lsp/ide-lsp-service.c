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
#include "ide-lsp-service.h"

typedef struct
{
  IdeSubprocessSupervisor *supervisor;
  IdeLspClient *client;
  guint has_started : 1;
  guint inherit_stderr : 1;
} IdeLspServicePrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (IdeLspService, ide_lsp_service, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_CLIENT,
  PROP_SUPERVISOR,
  PROP_INHERIT_STDERR,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

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

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_lsp_service_destroy (IdeObject *object)
{
  IdeLspService *self = (IdeLspService *)object;
  IdeLspServicePrivate *priv = ide_lsp_service_get_instance_private (self);

  ide_lsp_service_stop (self);

  g_clear_object (&priv->supervisor);
  g_clear_object (&priv->client);

  IDE_OBJECT_CLASS (ide_lsp_service_parent_class)->destroy (object);
}

G_NORETURN static void
ide_lsp_service_real_configure_client (IdeLspService *self,
                                       IdeLspClient  *client)
{
  g_assert_not_reached ();
}

static void
ide_lsp_service_real_configure_launcher (IdeLspService         *self,
                                         IdeSubprocessLauncher *client)
{
}

static void
ide_lsp_service_real_configure_supervisor (IdeLspService           *self,
                                           IdeSubprocessSupervisor *client)
{
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
  service_class->configure_launcher = ide_lsp_service_real_configure_launcher;
  service_class->configure_supervisor = ide_lsp_service_real_configure_supervisor;

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

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_LSP_SERVICE (self));

  if (priv->inherit_stderr == !!inherit_stderr)
    return;

  priv->inherit_stderr = !!inherit_stderr;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_INHERIT_STDERR]);
}

static void
on_supervisor_spawned (IdeLspService           *self,
                       IdeSubprocess           *subprocess,
                       IdeSubprocessSupervisor *supervisor)
{
  IdeLspServicePrivate *priv = ide_lsp_service_get_instance_private (self);
  IdeLspServiceClass *klass = IDE_LSP_SERVICE_GET_CLASS (self);
  g_autoptr(GIOStream) iostream = NULL;
  g_autoptr(IdeLspClient) client = NULL;
  g_autoptr(GInputStream) to_stdout = NULL;
  g_autoptr(GOutputStream) to_stdin = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_LSP_SERVICE (self));
  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (IDE_IS_SUBPROCESS_SUPERVISOR (supervisor));

  to_stdin = ide_subprocess_get_stdin_pipe (subprocess);
  to_stdout = ide_subprocess_get_stdout_pipe (subprocess);
  iostream = g_simple_io_stream_new (to_stdout, to_stdin);

  if (priv->client != NULL)
    {
      ide_lsp_client_stop (priv->client);
      ide_object_destroy (IDE_OBJECT (priv->client));
    }

  client = ide_lsp_client_new (iostream);
  ide_object_append (IDE_OBJECT (self), IDE_OBJECT (client));

  klass->configure_client (self, client);

  ide_lsp_client_start (client);

  priv->client = g_steal_pointer (&client);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CLIENT]);
}

static void
ensure_started (IdeLspService *self,
                IdeContext    *context)
{
  IdeLspServicePrivate *priv = ide_lsp_service_get_instance_private (self);
  IdeLspServiceClass *klass = IDE_LSP_SERVICE_GET_CLASS (self);
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  GSubprocessFlags flags;
  g_autoptr(GFile) workdir = NULL;
  g_autofree char *workdir_path = NULL;
  g_autoptr(IdeSubprocessSupervisor) supervisor = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_LSP_SERVICE (self));
  g_assert (IDE_IS_CONTEXT (context));

  if (priv->has_started)
    return;

  priv->has_started = TRUE;

  g_assert (priv->supervisor == NULL);
  g_assert (priv->client == NULL);

  flags = G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE;
  if (!priv->inherit_stderr)
    flags |= G_SUBPROCESS_FLAGS_STDERR_SILENCE;
  launcher = ide_subprocess_launcher_new (flags);

  ide_subprocess_launcher_set_clear_env (launcher, FALSE);

  workdir = ide_context_ref_workdir (context);
  workdir_path = g_file_get_path (workdir);
  ide_subprocess_launcher_set_cwd (launcher, workdir_path);

  klass->configure_launcher (self, launcher);

  supervisor = ide_subprocess_supervisor_new ();
  ide_subprocess_supervisor_set_launcher (supervisor, launcher);
  g_signal_connect_object (supervisor,
                           "spawned",
                           G_CALLBACK (on_supervisor_spawned),
                           self,
                           G_CONNECT_SWAPPED);

  klass->configure_supervisor (self, supervisor);

  ide_subprocess_supervisor_start (supervisor);

  priv->supervisor = g_steal_pointer (&supervisor);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SUPERVISOR]);
}

/**
 * ide_lsp_service_stop:
 * @self: a [class@LspService]
 *
 * Stops the service and its associated process, if any. This will set [property@LspService:client]
 * to %NULL.
 */
void
ide_lsp_service_stop (IdeLspService *self)
{
  gboolean notify_client = FALSE;
  gboolean notify_supervisor = FALSE;
  IdeLspServicePrivate *priv = ide_lsp_service_get_instance_private (self);

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_LSP_SERVICE (self));

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
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_LSP_SERVICE (self));
  g_return_if_fail (!ide_object_in_destruction (IDE_OBJECT (self)));

  ide_lsp_service_stop (self);
  ensure_started (self, ide_object_get_context (IDE_OBJECT (self)));
}

/**
 * ide_lsp_service_class_bind_client:
 * @klass: a [class@LspService] class structure
 * @provider: an [class@Object]
 *
 * Binds the "client" property of @property to its context's instance of @klass. If the language
 * server is not running yet, it will be started.
 */
void
ide_lsp_service_class_bind_client (IdeLspServiceClass *klass,
                                   IdeObject          *provider)
{
  IdeContext *context;
  g_autoptr(IdeLspService) service = NULL;
  GParamSpec *pspec;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_LSP_SERVICE_CLASS (klass));
  g_return_if_fail (IDE_IS_OBJECT (provider));

  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (provider), "client");
  g_return_if_fail (pspec != NULL && g_type_is_a (pspec->value_type, IDE_TYPE_LSP_CLIENT));

  context = ide_object_get_context (provider);
  g_return_if_fail (IDE_IS_CONTEXT (context));
  service = ide_object_ensure_child_typed (IDE_OBJECT (context), G_OBJECT_CLASS_TYPE (klass));

  ensure_started (service, context);

  g_object_bind_property (service, "client", provider, "client", G_BINDING_SYNC_CREATE);
}
