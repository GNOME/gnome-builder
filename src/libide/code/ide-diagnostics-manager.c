/* ide-diagnostics-manager.c
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-diagnostics-manager"

#include "config.h"

#include <gtksourceview/gtksource.h>

#include <libide-plugins.h>

#include "ide-marshal.h"

#include "ide-buffer.h"
#include "ide-buffer-manager.h"
#include "ide-buffer-private.h"
#include "ide-diagnostic.h"
#include "ide-diagnostic-provider.h"
#include "ide-diagnostics.h"
#include "ide-diagnostics-manager.h"
#include "ide-diagnostics-manager-private.h"

#define DEFAULT_DIAGNOSE_DELAY 333
#define DIAG_GROUP_MAGIC       0xF1282727
#define IS_DIAGNOSTICS_GROUP(g) ((g) && (g)->magic == DIAG_GROUP_MAGIC)

typedef struct
{
  /*
   * Used to give ourself a modicum of assurance our structure hasn't
   * been miss-used.
   */
  guint magic;

  /*
   * This is our identifier for the diagnostics. We use this as the key in
   * the hash table so that we can quickly find the target buffer. If the
   * IdeBuffer:file property changes, we will have to fallback to the
   * buffer to clear old entries.
   */
  GFile *file;

  /*
   * This hash table uses the given provider as the key and the last
   * reported IdeDiagnostics as the value.
   */
  GHashTable *diagnostics_by_provider;

  /*
   * This extension set adapter is used to update the providers that are
   * available based on the buffers current language. They may change
   * at runtime due to the buffers language changing. When that happens
   * we purge items from @diagnostics_by_provider and queue a diagnose
   * request of the new provider.
   */
  IdeExtensionSetAdapter *adapter;

  /* The most recent bytes we received for a future diagnosis. */
  GBytes *contents;

  /* The last language id we were notified about */
  const gchar *lang_id;

  /*
   * This is our sequence number for diagnostics. It is monotonically
   * increasing with every diagnostic discovered.
   */
  guint sequence;

  /*
   * If we are currently diagnosing, then this will be set to a
   * number greater than zero.
   */
  guint in_diagnose;

  /*
   * If we need a diagnose this bit will be set. If we complete a
   * diagnosis and this bit is set, then we will automatically queue
   * another diagnose upon completion.
   */
  guint needs_diagnose : 1;

  /*
   * This bit is set if we know the file or buffer has diagnostics. This
   * is useful when we've cleaned up our extensions and no longer have
   * the diagnostics loaded in memory, but we know that it previously
   * had diagnostics which have not been rectified.
   */
  guint has_diagnostics : 1;

  /*
   * This bit is set when the group has been removed from the
   * IdeDiagnosticsManager. That allows the providers to cleanup
   * as necessary when their async operations complete.
   */
  guint was_removed : 1;

} IdeDiagnosticsGroup;

struct _IdeDiagnosticsManager
{
  IdeObject parent_instance;

  /*
   * This hashtable contains a mapping of GFile to the IdeDiagnosticsGroup
   * for the file. When a buffer is renamed (the IdeBuffer:file property
   * is changed) we need to update this entry so it reflects the new
   * location.
   */
  GHashTable *groups_by_file;

  /*
   * If any group has a queued diagnose in process, this will be set so
   * we can coalesce the dispatch of everything at the same time.
   */
  guint queued_diagnose_source;
};

enum {
  PROP_0,
  PROP_BUSY,
  N_PROPS
};

enum {
  CHANGED,
  N_SIGNALS
};


static gboolean ide_diagnostics_manager_clear_by_provider (IdeDiagnosticsManager *self,
                                                           IdeDiagnosticProvider *provider);
static void     ide_diagnostics_manager_add_diagnostic    (IdeDiagnosticsManager *self,
                                                           IdeDiagnosticProvider *provider,
                                                           IdeDiagnostic         *diagnostic);
static void     ide_diagnostics_group_queue_diagnose      (IdeDiagnosticsGroup   *group,
                                                           IdeDiagnosticsManager *self);


static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

G_DEFINE_FINAL_TYPE (IdeDiagnosticsManager, ide_diagnostics_manager, IDE_TYPE_OBJECT)

static void
free_diagnostics (gpointer data)
{
  IdeDiagnostics *diagnostics = data;

  g_clear_object (&diagnostics);
}

static inline guint
diagnostics_get_size (IdeDiagnostics *diags)
{
  return diags ? g_list_model_get_n_items (G_LIST_MODEL (diags)) : 0;
}

static void
ide_diagnostics_group_finalize (IdeDiagnosticsGroup *group)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (group != NULL);
  g_assert (IS_DIAGNOSTICS_GROUP (group));

  group->magic = 0;

  g_clear_pointer (&group->diagnostics_by_provider, g_hash_table_unref);
  g_clear_pointer (&group->contents, g_bytes_unref);
  ide_clear_and_destroy_object (&group->adapter);
  g_clear_object (&group->file);
}

static IdeDiagnosticsGroup *
ide_diagnostics_group_new (GFile *file)
{
  IdeDiagnosticsGroup *group;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (G_IS_FILE (file));

  group = g_rc_box_new0 (IdeDiagnosticsGroup);
  group->magic = DIAG_GROUP_MAGIC;
  group->file = g_object_ref (file);
  group->diagnostics_by_provider = g_hash_table_new_full (NULL, NULL, NULL, free_diagnostics);

  return group;
}

static IdeDiagnosticsGroup *
ide_diagnostics_group_ref (IdeDiagnosticsGroup *group)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (group != NULL);
  g_assert (IS_DIAGNOSTICS_GROUP (group));

  return g_rc_box_acquire (group);
}

static void
ide_diagnostics_group_unref (IdeDiagnosticsGroup *group)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (group != NULL);
  g_assert (IS_DIAGNOSTICS_GROUP (group));

  g_rc_box_release_full (group, (GDestroyNotify)ide_diagnostics_group_finalize);
}

static guint
ide_diagnostics_group_has_diagnostics (IdeDiagnosticsGroup *group)
{
  GHashTableIter iter;
  gpointer value;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (group != NULL);
  g_assert (IS_DIAGNOSTICS_GROUP (group));

  g_hash_table_iter_init (&iter, group->diagnostics_by_provider);

  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      IdeDiagnostics *diagnostics = value;

      if (diagnostics_get_size (diagnostics) > 0)
        return TRUE;
    }

  return FALSE;
}

static gboolean
ide_diagnostics_group_can_dispose (IdeDiagnosticsGroup *group)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (group != NULL);
  g_assert (IS_DIAGNOSTICS_GROUP (group));

  /*
   * We can cleanup this group if we don't have a buffer loaded and
   * the adapters have been unloaded and there are no diagnostics
   * registered for the group.
   */

  return group->adapter == NULL &&
         group->has_diagnostics == FALSE;
}

static void
ide_diagnostics_group_add (IdeDiagnosticsGroup   *group,
                           IdeDiagnosticProvider *provider,
                           IdeDiagnostic         *diagnostic)
{
  IdeDiagnostics *diagnostics;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (group != NULL);
  g_assert (IS_DIAGNOSTICS_GROUP (group));
  g_assert (IDE_IS_DIAGNOSTIC_PROVIDER (provider));
  g_assert (diagnostic != NULL);

  diagnostics = g_hash_table_lookup (group->diagnostics_by_provider, provider);

  if (diagnostics == NULL)
    {
      diagnostics = ide_diagnostics_new ();
      g_hash_table_insert (group->diagnostics_by_provider, provider, diagnostics);
    }

  ide_diagnostics_add (diagnostics, diagnostic);

  group->has_diagnostics = TRUE;
  group->sequence++;
}

static void
ide_diagnostics_group_diagnose_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  IdeDiagnosticProvider *provider = (IdeDiagnosticProvider *)object;
  g_autoptr(IdeDiagnosticsManager) self = user_data;
  g_autoptr(IdeDiagnostics) diagnostics = NULL;
  g_autoptr(GError) error = NULL;
  IdeDiagnosticsGroup *group;
  gboolean changed;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_DIAGNOSTIC_PROVIDER (provider));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_DIAGNOSTICS_MANAGER (self));

  diagnostics = ide_diagnostic_provider_diagnose_finish (provider, result, &error);

  IDE_TRACE_MSG ("%s diagnosis completed (%s)",
                 G_OBJECT_TYPE_NAME (provider),
                 error ? error->message : "success");

  if (error != NULL &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED))
    g_debug ("%s", error->message);

  /*
   * This fetches the group our provider belongs to. Since the group is
   * reference counted (and we only release it when our provider is
   * finalized), we should be guaranteed we have a valid group.
   */
  group = g_object_get_data (G_OBJECT (provider), "IDE_DIAGNOSTICS_GROUP");

  if (group == NULL)
    {
      /* Warning and bail if we failed to get the diagnostic group.
       * This shouldn't be happening, but I have definitely seen it
       * so it is probably related to disposal.
       */
      g_warning ("Failed to locate group, possibly disposed.");
      IDE_EXIT;
    }

  g_assert (IS_DIAGNOSTICS_GROUP (group));

  /*
   * Clear all of our old diagnostics no matter where they ended up.
   */
  changed = ide_diagnostics_manager_clear_by_provider (self, provider);

  /*
   * The following adds diagnostics to the appropriate group, but tries the
   * group we belong to first as our fast path. That will almost always be
   * the case, except when a diagnostic came up for a header or something
   * while parsing a given file.
   */
  if (diagnostics != NULL)
    {
      guint length = diagnostics_get_size (diagnostics);

      for (guint i = 0; i < length; i++)
        {
          g_autoptr(IdeDiagnostic) diagnostic = g_list_model_get_item (G_LIST_MODEL (diagnostics), i);
          GFile *file = ide_diagnostic_get_file (diagnostic);

          if (file != NULL)
            {
              if (g_file_equal (file, group->file))
                ide_diagnostics_group_add (group, provider, diagnostic);
              else
                ide_diagnostics_manager_add_diagnostic (self, provider, diagnostic);
            }
        }

      if (length > 0)
        changed = TRUE;
    }

  group->in_diagnose--;

  /*
   * Ensure we increment our sequence number even when no diagnostics were
   * reported. This ensures that the gutter gets cleared and line-flags
   * cache updated.
   */
  group->sequence++;

  /*
   * Since the individual groups have sequence numbers associated with changes,
   * it's okay to emit this for every provider completion. That allows the UIs
   * to update faster as each provider completes at the expensive of a little
   * more CPU activity.
   */
  if (changed)
    g_signal_emit (self, signals [CHANGED], 0);

  /*
   * If there are no more diagnostics providers active and the group needs
   * another diagnosis, then we can start the next one now.
   *
   * If we are completing this diagnosis and the buffer was already released
   * (and other diagnose providers have unloaded), we might be able to clean
   * up the group and be done with things.
   */
  if (group->was_removed == FALSE && group->in_diagnose == 0 && group->needs_diagnose)
    {
      ide_diagnostics_group_queue_diagnose (group, self);
    }
  else if (ide_diagnostics_group_can_dispose (group))
    {
      group->was_removed = TRUE;
      g_hash_table_remove (self->groups_by_file, group->file);
      IDE_EXIT;
    }

  IDE_EXIT;
}

static void
ide_diagnostics_group_diagnose_foreach (IdeExtensionSetAdapter *adapter,
                                        PeasPluginInfo         *plugin_info,
                                        GObject          *exten,
                                        gpointer                user_data)
{
  IdeDiagnosticProvider *provider = (IdeDiagnosticProvider *)exten;
  IdeDiagnosticsManager *self = user_data;
  IdeDiagnosticsGroup *group;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_DIAGNOSTICS_MANAGER (self));
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (adapter));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_DIAGNOSTIC_PROVIDER (provider));

  group = g_object_get_data (G_OBJECT (provider), "IDE_DIAGNOSTICS_GROUP");

  g_assert (group != NULL);
  g_assert (IS_DIAGNOSTICS_GROUP (group));

  group->in_diagnose++;

#ifdef IDE_ENABLE_TRACE
  {
    g_autofree gchar *uri = g_file_get_uri (group->file);
    IDE_TRACE_MSG ("Beginning diagnose on %s with provider %s",
                   uri, G_OBJECT_TYPE_NAME (provider));
  }
#endif

  ide_diagnostic_provider_diagnose_async (provider,
                                          group->file,
                                          group->contents,
                                          group->lang_id,
                                          NULL,
                                          ide_diagnostics_group_diagnose_cb,
                                          g_object_ref (self));

  IDE_EXIT;
}

static void
ide_diagnostics_group_diagnose (IdeDiagnosticsGroup   *group,
                                IdeDiagnosticsManager *self)
{
  IDE_ENTRY;

  g_assert (IDE_IS_DIAGNOSTICS_MANAGER (self));
  g_assert (group != NULL);
  g_assert (group->in_diagnose == FALSE);
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (group->adapter));

  group->needs_diagnose = FALSE;
  group->has_diagnostics = FALSE;

  if (group->contents == NULL)
    group->contents = g_bytes_new ("", 0);

  ide_extension_set_adapter_foreach (group->adapter,
                                     ide_diagnostics_group_diagnose_foreach,
                                     self);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUSY]);

  IDE_EXIT;
}

static gboolean
ide_diagnostics_manager_begin_diagnose (gpointer data)
{
  IdeDiagnosticsManager *self = data;
  GHashTableIter iter;
  gpointer value;

  IDE_ENTRY;

  g_assert (IDE_IS_DIAGNOSTICS_MANAGER (self));

  self->queued_diagnose_source = 0;

  g_hash_table_iter_init (&iter, self->groups_by_file);

  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      IdeDiagnosticsGroup *group = value;

      g_assert (group != NULL);
      g_assert (IS_DIAGNOSTICS_GROUP (group));

      if (group->needs_diagnose && group->adapter != NULL && group->in_diagnose == 0)
        ide_diagnostics_group_diagnose (group, self);
    }

  IDE_RETURN (G_SOURCE_REMOVE);
}

static void
ide_diagnostics_group_queue_diagnose (IdeDiagnosticsGroup   *group,
                                      IdeDiagnosticsManager *self)
{
  g_assert (group != NULL);
  g_assert (IS_DIAGNOSTICS_GROUP (group));
  g_assert (IDE_IS_DIAGNOSTICS_MANAGER (self));

  /*
   * This checks to see if we are diagnosing and if not queues a diagnose.
   * If a diagnosis is already running, we don't need to do anything now
   * because the completion of the diagnose will tick off the next diagnose
   * upon seening group->needs_diagnose==TRUE.
   */

  group->needs_diagnose = TRUE;

  if (group->in_diagnose == 0 && self->queued_diagnose_source == 0)
    self->queued_diagnose_source = g_timeout_add_full (G_PRIORITY_LOW,
                                                       DEFAULT_DIAGNOSE_DELAY,
                                                       ide_diagnostics_manager_begin_diagnose,
                                                       self, NULL);
}

static void
ide_diagnostics_manager_destroy (IdeObject *object)
{
  IdeDiagnosticsManager *self = (IdeDiagnosticsManager *)object;

  g_clear_handle_id (&self->queued_diagnose_source, g_source_remove);
  g_clear_pointer (&self->groups_by_file, g_hash_table_unref);

  IDE_OBJECT_CLASS (ide_diagnostics_manager_parent_class)->destroy (object);
}

static void
ide_diagnostics_manager_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  IdeDiagnosticsManager *self = (IdeDiagnosticsManager *)object;

  switch (prop_id)
    {
    case PROP_BUSY:
      g_value_set_boolean (value, ide_diagnostics_manager_get_busy (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_diagnostics_manager_class_init (IdeDiagnosticsManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  object_class->get_property = ide_diagnostics_manager_get_property;

  i_object_class->destroy = ide_diagnostics_manager_destroy;

  properties [PROP_BUSY] =
    g_param_spec_boolean ("busy",
                          "Busy",
                          "If the diagnostics manager is busy",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * IdeDiagnosticsManager::changed:
   * @self: an #IdeDiagnosticsManager
   *
   * This signal is emitted when the diagnostics have changed for any
   * file managed by the IdeDiagnosticsManager.
   */
  signals [CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  ide_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  g_signal_set_va_marshaller (signals [CHANGED],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__VOIDv);
}

static void
ide_diagnostics_manager_init (IdeDiagnosticsManager *self)
{
  self->groups_by_file = g_hash_table_new_full (g_file_hash,
                                                (GEqualFunc)g_file_equal,
                                                NULL,
                                                (GDestroyNotify)ide_diagnostics_group_unref);
}

static void
ide_diagnostics_manager_add_diagnostic (IdeDiagnosticsManager *self,
                                        IdeDiagnosticProvider *provider,
                                        IdeDiagnostic         *diagnostic)
{
  IdeDiagnosticsGroup *group;
  GFile *file;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_DIAGNOSTICS_MANAGER (self));
  g_assert (IDE_IS_DIAGNOSTIC_PROVIDER (provider));
  g_assert (diagnostic != NULL);

  /*
   * This is our slow path for adding a diagnostic to the system. We have
   * to locate the proper group for the diagnostic and then insert it
   * into that group.
   */

  if (NULL == (file = ide_diagnostic_get_file (diagnostic)))
    return;

  group = g_hash_table_lookup (self->groups_by_file, file);

  if (group == NULL)
    {
      group = ide_diagnostics_group_new (file);
      g_hash_table_replace (self->groups_by_file, group->file, group);
    }

  g_assert (group != NULL);
  g_assert (IS_DIAGNOSTICS_GROUP (group));

  ide_diagnostics_group_add (group, provider, diagnostic);
}

static IdeDiagnosticsGroup *
ide_diagnostics_manager_find_group (IdeDiagnosticsManager *self,
                                    GFile                 *file)
{
  IdeDiagnosticsGroup *group;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_DIAGNOSTICS_MANAGER (self));
  g_assert (G_IS_FILE (file));

  if (!(group = g_hash_table_lookup (self->groups_by_file, file)))
    {
      group = ide_diagnostics_group_new (file);
      g_hash_table_replace (self->groups_by_file, group->file, group);
    }

  g_assert (group != NULL);
  g_assert (IS_DIAGNOSTICS_GROUP (group));

  return group;
}

static IdeDiagnosticsGroup *
ide_diagnostics_manager_find_group_from_adapter (IdeDiagnosticsManager  *self,
                                                 IdeExtensionSetAdapter *adapter)
{
  GHashTableIter iter;
  gpointer value;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_DIAGNOSTICS_MANAGER (self));
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (adapter));

  g_hash_table_iter_init (&iter, self->groups_by_file);

  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      IdeDiagnosticsGroup *group = value;

      g_assert (group != NULL);
      g_assert (IS_DIAGNOSTICS_GROUP (group));

      if (group->adapter == adapter)
        return group;
    }

  g_assert_not_reached ();

  return NULL;
}

static void
ide_diagnostics_manager_provider_invalidated (IdeDiagnosticsManager *self,
                                              IdeDiagnosticProvider *provider)
{
  IdeDiagnosticsGroup *group;

  IDE_ENTRY;

  g_assert (IDE_IS_DIAGNOSTICS_MANAGER (self));
  g_assert (IDE_IS_DIAGNOSTIC_PROVIDER (provider));

  group = g_object_get_data (G_OBJECT (provider), "IDE_DIAGNOSTICS_GROUP");

  ide_diagnostics_group_queue_diagnose (group, self);

  IDE_EXIT;
}

static void
ide_diagnostics_manager_extension_added (IdeExtensionSetAdapter *adapter,
                                         PeasPluginInfo         *plugin_info,
                                         GObject          *exten,
                                         gpointer                user_data)
{
  IdeDiagnosticProvider *provider = (IdeDiagnosticProvider *)exten;
  IdeDiagnosticsManager *self = user_data;
  IdeDiagnosticsGroup *group;

  IDE_ENTRY;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (adapter));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_DIAGNOSTIC_PROVIDER (provider));
  g_assert (IDE_IS_DIAGNOSTICS_MANAGER (self));

  group = ide_diagnostics_manager_find_group_from_adapter (self, adapter);

  g_assert (IS_DIAGNOSTICS_GROUP (group));

  /*
   * We will need access to the group upon completion of the diagnostics,
   * so we add a reference to the group and allow it to be automatically
   * cleaned up when the provider finalizes.
   */
  g_object_set_data_full (G_OBJECT (provider),
                          "IDE_DIAGNOSTICS_GROUP",
                          ide_diagnostics_group_ref (group),
                          (GDestroyNotify)ide_diagnostics_group_unref);

  /*
   * We insert a dummy entry into the hashtable upon creation so
   * that when an async diagnosis completes we can use the presence
   * of this key to know if we've been unloaded.
   */
  g_hash_table_insert (group->diagnostics_by_provider, provider, NULL);

  /*
   * We need to keep track of when the provider has been invalidated so
   * that we can queue another request to fetch the diagnostics.
   */
  g_signal_connect_object (provider,
                           "invalidated",
                           G_CALLBACK (ide_diagnostics_manager_provider_invalidated),
                           self,
                           G_CONNECT_SWAPPED);

  ide_diagnostic_provider_load (provider);

  ide_diagnostics_group_queue_diagnose (group, self);

  IDE_EXIT;
}

static gboolean
ide_diagnostics_manager_clear_by_provider (IdeDiagnosticsManager *self,
                                           IdeDiagnosticProvider *provider)
{
  GHashTableIter iter;
  gpointer value;
  gboolean changed = FALSE;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_DIAGNOSTICS_MANAGER (self));
  g_assert (IDE_IS_DIAGNOSTIC_PROVIDER (provider));

  g_hash_table_iter_init (&iter, self->groups_by_file);

  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      IdeDiagnosticsGroup *group = value;

      g_assert (group != NULL);
      g_assert (IS_DIAGNOSTICS_GROUP (group));

      if (g_hash_table_contains (group->diagnostics_by_provider, provider))
        {
          g_hash_table_remove (group->diagnostics_by_provider, provider);

          /*
           * TODO: If this provider is not part of this group, we can possibly
           *       dispose of the group if there are no diagnostics.
           */

          changed = TRUE;
        }
    }

  return changed;
}

static void
ide_diagnostics_manager_extension_removed (IdeExtensionSetAdapter *adapter,
                                           PeasPluginInfo         *plugin_info,
                                           GObject          *exten,
                                           gpointer                user_data)
{
  IdeDiagnosticProvider *provider = (IdeDiagnosticProvider *)exten;
  IdeDiagnosticsManager *self = user_data;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (adapter));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_DIAGNOSTIC_PROVIDER (provider));
  g_assert (IDE_IS_DIAGNOSTICS_MANAGER (self));

  g_signal_handlers_disconnect_by_func (provider,
                                        G_CALLBACK (ide_diagnostics_manager_provider_invalidated),
                                        self);

  /*
   * The goal of the following is to reomve our diagnostics from any file
   * that has been loaded. It is possible for diagnostic providers to effect
   * files outside the buffer they are loaded for and this ensures that we
   * clean those up.
   */
  ide_diagnostics_manager_clear_by_provider (self, provider);

  /* Clear the diagnostics group */
  g_object_set_data (G_OBJECT (provider), "IDE_DIAGNOSTICS_GROUP", NULL);

  IDE_EXIT;
}

/**
 * ide_diagnostics_manager_get_busy:
 *
 * Gets if the diagnostics manager is currently executing a diagnosis.
 *
 * Returns: %TRUE if the #IdeDiagnosticsManager is busy diagnosing.
 */
gboolean
ide_diagnostics_manager_get_busy (IdeDiagnosticsManager *self)
{
  GHashTableIter iter;
  gpointer value;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (IDE_IS_DIAGNOSTICS_MANAGER (self), FALSE);

  g_hash_table_iter_init (&iter, self->groups_by_file);

  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      IdeDiagnosticsGroup *group = value;

      g_assert (group != NULL);
      g_assert (IS_DIAGNOSTICS_GROUP (group));

      if (group->in_diagnose > 0)
        return TRUE;
    }

  return FALSE;
}

/**
 * ide_diagnostics_manager_get_diagnostics_for_file:
 * @self: An #IdeDiagnosticsManager
 * @file: a #GFile to retrieve diagnostics for
 *
 * This function collects all of the diagnostics that have been collected
 * for @file and returns them as a new #IdeDiagnostics to the caller.
 *
 * The #IdeDiagnostics structure will contain zero items if there are
 * no diagnostics discovered. Therefore, this function will never return
 * a %NULL value.
 *
 * Returns: (transfer full): A new #IdeDiagnostics.
 */
IdeDiagnostics *
ide_diagnostics_manager_get_diagnostics_for_file (IdeDiagnosticsManager *self,
                                                  GFile                 *file)
{
  g_autoptr(IdeDiagnostics) ret = NULL;
  IdeDiagnosticsGroup *group;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_DIAGNOSTICS_MANAGER (self), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  ret = ide_diagnostics_new ();

  group = g_hash_table_lookup (self->groups_by_file, file);

  g_assert (group == NULL || IS_DIAGNOSTICS_GROUP (group));

  if (group != NULL)
    {
      GHashTableIter iter;
      gpointer value;

      g_hash_table_iter_init (&iter, group->diagnostics_by_provider);

      while (g_hash_table_iter_next (&iter, NULL, &value))
        {
          IdeDiagnostics *diagnostics = value;
          guint length;

          if (diagnostics == NULL)
            continue;

          length = g_list_model_get_n_items (G_LIST_MODEL (diagnostics));

          for (guint i = 0; i < length; i++)
            {
              g_autoptr(IdeDiagnostic) diagnostic = NULL;

              diagnostic = g_list_model_get_item (G_LIST_MODEL (diagnostics), i);
              ide_diagnostics_add (ret, diagnostic);
            }
        }
    }

  return g_steal_pointer (&ret);
}

guint
ide_diagnostics_manager_get_sequence_for_file (IdeDiagnosticsManager *self,
                                               GFile                 *file)
{
  IdeDiagnosticsGroup *group;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), 0);
  g_return_val_if_fail (IDE_IS_DIAGNOSTICS_MANAGER (self), 0);
  g_return_val_if_fail (G_IS_FILE (file), 0);

  group = g_hash_table_lookup (self->groups_by_file, file);

  if (group != NULL)
    {
      g_assert (IS_DIAGNOSTICS_GROUP (group));
      g_assert (G_IS_FILE (group->file));
      g_assert (g_file_equal (group->file, file));

      return group->sequence;
    }

  return 0;
}

/**
 * ide_diagnostics_manager_rediagnose:
 * @self: an #IdeDiagnosticsManager
 * @buffer: an #IdeBuffer
 *
 * Requests that the diagnostics be reloaded for @buffer.
 *
 * You may want to call this if you changed something that a buffer depends on,
 * and want to seamlessly update its diagnostics with that updated information.
 */
void
ide_diagnostics_manager_rediagnose (IdeDiagnosticsManager *self,
                                    IdeBuffer             *buffer)
{
  IdeDiagnosticsGroup *group;
  GFile *file;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_DIAGNOSTICS_MANAGER (self));
  g_return_if_fail (IDE_IS_BUFFER (buffer));

  file = ide_buffer_get_file (buffer);
  group = ide_diagnostics_manager_find_group (self, file);

  g_assert (group != NULL);
  g_assert (IS_DIAGNOSTICS_GROUP (group));

  ide_diagnostics_group_queue_diagnose (group, self);
}

/**
 * ide_diagnostics_manager_from_context:
 * @context: an #IdeContext
 *
 * Gets the diagnostics manager for the context.
 *
 * Returns: (transfer none): an #IdeDiagnosticsManager
 */
IdeDiagnosticsManager *
ide_diagnostics_manager_from_context (IdeContext *context)
{
  IdeDiagnosticsManager *self;

  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  ide_object_lock (IDE_OBJECT (context));
  if (!(self = ide_context_peek_child_typed (context, IDE_TYPE_DIAGNOSTICS_MANAGER)))
    {
      g_autoptr(IdeDiagnosticsManager) created = NULL;
      created = ide_object_ensure_child_typed (IDE_OBJECT (context),
                                               IDE_TYPE_DIAGNOSTICS_MANAGER);
      self = ide_context_peek_child_typed (context, IDE_TYPE_DIAGNOSTICS_MANAGER);
    }
  ide_object_unlock (IDE_OBJECT (context));

  return self;
}

void
_ide_diagnostics_manager_file_closed (IdeDiagnosticsManager *self,
                                      GFile                 *file)
{
  IdeDiagnosticsGroup *group;
  gboolean has_diagnostics;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_DIAGNOSTICS_MANAGER (self));
  g_return_if_fail (G_IS_FILE (file));

  /*
   * The goal here is to cleanup everything we can about this group that
   * is part of a loaded buffer. We might want to keep the group around
   * in case it is useful from other providers.
   */

  group = ide_diagnostics_manager_find_group (self, file);

  g_assert (group != NULL);
  g_assert (IS_DIAGNOSTICS_GROUP (group));

  /* Clear some state we've been tracking */
  g_clear_pointer (&group->contents, g_bytes_unref);
  group->lang_id = NULL;
  group->needs_diagnose = FALSE;

  /*
   * We track if we have diagnostics now so that after we unload the
   * the providers, we can save that bit for later.
   */
  has_diagnostics = ide_diagnostics_group_has_diagnostics (group);

  /*
   * Force our diagnostic providers to unload. This will cause them
   * extension-removed signal to be called for each provider which
   * in turn will perform per-provider cleanup including the removal
   * of its diagnostics from all groups. (A provider can in practice
   * affect another group since a .c file could create a diagnostic
   * for a .h).
   */
  ide_clear_and_destroy_object (&group->adapter);

  group->has_diagnostics = has_diagnostics;

  IDE_EXIT;
}

void
_ide_diagnostics_manager_file_changed (IdeDiagnosticsManager *self,
                                       GFile                 *file,
                                       GBytes                *contents,
                                       const gchar           *lang_id)
{
  IdeDiagnosticsGroup *group;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_DIAGNOSTICS_MANAGER (self));
  g_return_if_fail (G_IS_FILE (file));

  group = ide_diagnostics_manager_find_group (self, file);

  g_assert (group != NULL);
  g_assert (IS_DIAGNOSTICS_GROUP (group));

  g_clear_pointer (&group->contents, g_bytes_unref);

  group->lang_id = g_intern_string (lang_id);
  group->contents = contents ? g_bytes_ref (contents) : NULL;

  ide_diagnostics_group_queue_diagnose (group, self);
}

void
_ide_diagnostics_manager_language_changed (IdeDiagnosticsManager *self,
                                           GFile                 *file,
                                           const gchar           *lang_id)
{
  IdeDiagnosticsGroup *group;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_DIAGNOSTICS_MANAGER (self));

  group = ide_diagnostics_manager_find_group (self, file);

  g_assert (group != NULL);
  g_assert (IS_DIAGNOSTICS_GROUP (group));

  group->lang_id = g_intern_string (lang_id);

  if (group->adapter != NULL)
    ide_extension_set_adapter_set_value (group->adapter, lang_id);

  ide_diagnostics_group_queue_diagnose (group, self);
}

void
_ide_diagnostics_manager_file_opened (IdeDiagnosticsManager *self,
                                      GFile                 *file,
                                      const gchar           *lang_id)
{
  IdeDiagnosticsGroup *group;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_DIAGNOSTICS_MANAGER (self));
  g_assert (G_IS_FILE (file));

  group = ide_diagnostics_manager_find_group (self, file);

  g_assert (group != NULL);
  g_assert (IS_DIAGNOSTICS_GROUP (group));

  group->lang_id = g_intern_string (lang_id);

  if (group->adapter == NULL)
    {
      group->adapter = ide_extension_set_adapter_new (IDE_OBJECT (self),
                                                      peas_engine_get_default (),
                                                      IDE_TYPE_DIAGNOSTIC_PROVIDER,
                                                      "Diagnostic-Provider-Languages",
                                                      lang_id);

      g_signal_connect_object (group->adapter,
                               "extension-added",
                               G_CALLBACK (ide_diagnostics_manager_extension_added),
                               self,
                               0);

      g_signal_connect_object (group->adapter,
                               "extension-removed",
                               G_CALLBACK (ide_diagnostics_manager_extension_removed),
                               self,
                               0);

      ide_extension_set_adapter_foreach (group->adapter,
                                         ide_diagnostics_manager_extension_added,
                                         self);
    }

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (group->adapter));
  g_assert (g_hash_table_lookup (self->groups_by_file, file) == group);

  ide_diagnostics_group_queue_diagnose (group, self);

  IDE_EXIT;
}

