/* ide-diagnostics-manager.c
 *
 * Copyright 2016 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-diagnostics-manager"

#include "config.h"

#include <gtksourceview/gtksource.h>

#include "ide-context.h"
#include "ide-debug.h"

#include "buffers/ide-buffer.h"
#include "buffers/ide-buffer-manager.h"
#include "diagnostics/ide-diagnostic.h"
#include "diagnostics/ide-diagnostic-provider.h"
#include "diagnostics/ide-diagnostics.h"
#include "diagnostics/ide-diagnostics-manager.h"
#include "plugins/ide-extension-set-adapter.h"

#define DEFAULT_DIAGNOSE_DELAY 333

typedef struct
{
  /*
   * Our reference count on the group, which is not atomic because we
   * require access to this structure to be accessed from the main thread
   * only. This is used so that providers can have access to the group even
   * after the group has been removed from the IdeDiagnosticsManager.
   */
  volatile gint ref_count;

  /*
   * This is our identifier for the diagnostics. We use this as the key in
   * the hash table so that we can quickly find the target buffer. If the
   * IdeBuffer:file property changes, we will have to fallback to the
   * buffer to clear old entries.
   */
  GFile *file;

  /*
   * If there is a buffer open for the file, then the buffer will be found
   * here. This is useful when we detect that a IdeBuffer:file property has
   * changed and we need to invalidate things. There is a weak reference
   * to the buffer here, but that is dropped in our ::buffer-unloaded
   * callback. It's not really necessary other than for some additional
   * insurance.
   */
  GWeakRef buffer_wr;

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


static void     initable_iface_init                       (GInitableIface        *iface);
static gboolean ide_diagnostics_manager_clear_by_provider (IdeDiagnosticsManager *self,
                                                           IdeDiagnosticProvider *provider);
static void     ide_diagnostics_manager_add_diagnostic    (IdeDiagnosticsManager *self,
                                                           IdeDiagnosticProvider *provider,
                                                           IdeDiagnostic         *diagnostic);
static void     ide_diagnostics_group_queue_diagnose      (IdeDiagnosticsGroup   *group,
                                                           IdeDiagnosticsManager *self);


static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];


G_DEFINE_TYPE_WITH_CODE (IdeDiagnosticsManager, ide_diagnostics_manager, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init))


static void
free_diagnostics (gpointer data)
{
  IdeDiagnostics *diagnostics = data;

  g_clear_pointer (&diagnostics, ide_diagnostics_unref);
}

static void
ide_diagnostics_group_free (gpointer data)
{
  IdeDiagnosticsGroup *group = data;

  g_assert (group != NULL);
  g_assert (group->ref_count == 0);

  g_clear_pointer (&group->diagnostics_by_provider, g_hash_table_unref);
  g_weak_ref_clear (&group->buffer_wr);
  g_clear_object (&group->adapter);
  g_clear_object (&group->file);
  g_slice_free (IdeDiagnosticsGroup, group);
}

static IdeDiagnosticsGroup *
ide_diagnostics_group_new (GFile *file)
{
  IdeDiagnosticsGroup *group;

  g_assert (G_IS_FILE (file));

  group = g_slice_new0 (IdeDiagnosticsGroup);
  group->ref_count = 1;
  group->file = g_object_ref (file);

  g_weak_ref_init (&group->buffer_wr, NULL);

  return group;
}

static IdeDiagnosticsGroup *
ide_diagnostics_group_ref (IdeDiagnosticsGroup *group)
{
  g_assert (group != NULL);
  g_assert (group->ref_count > 0);

  group->ref_count++;

  return group;
}

static void
ide_diagnostics_group_unref (IdeDiagnosticsGroup *group)
{
  g_assert (group != NULL);
  g_assert (group->ref_count > 0);

  group->ref_count--;

  if (group->ref_count == 0)
    ide_diagnostics_group_free (group);
}

static guint
ide_diagnostics_group_has_diagnostics (IdeDiagnosticsGroup *group)
{
  g_assert (group != NULL);

  if (group->diagnostics_by_provider != NULL)
    {
      GHashTableIter iter;
      gpointer value;

      g_hash_table_iter_init (&iter, group->diagnostics_by_provider);

      while (g_hash_table_iter_next (&iter, NULL, &value))
        {
          IdeDiagnostics *diagnostics = value;

          if (diagnostics != NULL && ide_diagnostics_get_size (diagnostics) > 0)
            return TRUE;
        }
    }

  return FALSE;
}

static gboolean
ide_diagnostics_group_can_dispose (IdeDiagnosticsGroup *group)
{
  g_autoptr(IdeBuffer) buffer = NULL;

  g_assert (group != NULL);

  /*
   * We can cleanup this group if we don't have a buffer loaded and
   * the adapters have been unloaded and there are no diagnostics
   * registered for the group.
   */

  buffer = g_weak_ref_get (&group->buffer_wr);

  return (buffer == NULL) &&
         (group->adapter == NULL) &&
         (group->has_diagnostics == FALSE);
}

static void
ide_diagnostics_group_add (IdeDiagnosticsGroup   *group,
                           IdeDiagnosticProvider *provider,
                           IdeDiagnostic         *diagnostic)
{
  IdeDiagnostics *diagnostics;

  g_assert (group != NULL);
  g_assert (IDE_IS_DIAGNOSTIC_PROVIDER (provider));
  g_assert (diagnostic != NULL);

  if (group->diagnostics_by_provider == NULL)
    group->diagnostics_by_provider = g_hash_table_new_full (NULL, NULL, NULL, free_diagnostics);

  diagnostics = g_hash_table_lookup (group->diagnostics_by_provider, provider);

  if (diagnostics == NULL)
    {
      diagnostics = ide_diagnostics_new (NULL);
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

  g_assert (IDE_IS_DIAGNOSTIC_PROVIDER (provider));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_DIAGNOSTICS_MANAGER (self));

  IDE_TRACE_MSG ("%s diagnosis completed", G_OBJECT_TYPE_NAME (provider));

  diagnostics = ide_diagnostic_provider_diagnose_finish (provider, result, &error);

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
      return;
    }

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
      guint length = ide_diagnostics_get_size (diagnostics);

      for (guint i = 0; i < length; i++)
        {
          IdeDiagnostic *diagnostic = ide_diagnostics_index (diagnostics, i);
          GFile *file = ide_diagnostic_get_file (diagnostic);

          if G_LIKELY (file != NULL)
            {
              if G_LIKELY (g_file_equal (file, group->file))
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
                                        PeasExtension          *exten,
                                        gpointer                user_data)
{
  IdeDiagnosticProvider *provider = (IdeDiagnosticProvider *)exten;
  IdeDiagnosticsManager *self = user_data;
  IdeDiagnosticsGroup *group;
  IdeContext *context;
  g_autoptr (IdeBuffer) buffer = NULL;
  g_autoptr(IdeFile) file = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_DIAGNOSTICS_MANAGER (self));
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (adapter));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_DIAGNOSTIC_PROVIDER (provider));

  group = g_object_get_data (G_OBJECT (provider), "IDE_DIAGNOSTICS_GROUP");
  group->in_diagnose++;

  context = ide_object_get_context (IDE_OBJECT (self));

  file = ide_file_new (context, group->file);

#ifdef IDE_ENABLE_TRACE
  {
    g_autofree gchar *uri = g_file_get_uri (group->file);
    IDE_TRACE_MSG ("Beginning diagnose on %s with provider %s",
                   uri, G_OBJECT_TYPE_NAME (provider));
  }
#endif

  buffer = g_weak_ref_get (&group->buffer_wr);
  ide_diagnostic_provider_diagnose_async (provider,
                                          file,
                                          buffer,
                                          NULL,
                                          ide_diagnostics_group_diagnose_cb,
                                          g_object_ref (self));
}

static void
ide_diagnostics_group_diagnose (IdeDiagnosticsGroup   *group,
                                IdeDiagnosticsManager *self)
{
  g_autoptr(IdeBuffer) buffer = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_DIAGNOSTICS_MANAGER (self));
  g_assert (group != NULL);
  g_assert (group->in_diagnose == FALSE);
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (group->adapter));

  group->needs_diagnose = FALSE;
  group->has_diagnostics = FALSE;

  /*
   * We need to ensure that all the diagnostic providers have access to the
   * proper data within the unsaved files. So sync the content once to avoid
   * all providers from having to do this manually.
   */
  if (NULL != (buffer = g_weak_ref_get (&group->buffer_wr)))
    ide_buffer_sync_to_unsaved_files (buffer);

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
                                                       g_object_ref (self),
                                                       g_object_unref);
}

static void
ide_diagnostics_manager_finalize (GObject *object)
{
  IdeDiagnosticsManager *self = (IdeDiagnosticsManager *)object;

  dzl_clear_source (&self->queued_diagnose_source);
  g_clear_pointer (&self->groups_by_file, g_hash_table_unref);

  G_OBJECT_CLASS (ide_diagnostics_manager_parent_class)->finalize (object);
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

  object_class->finalize = ide_diagnostics_manager_finalize;
  object_class->get_property = ide_diagnostics_manager_get_property;

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
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
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
      g_hash_table_insert (self->groups_by_file, group->file, group);
    }

  ide_diagnostics_group_add (group, provider, diagnostic);
}

static IdeDiagnosticsGroup *
ide_diagnostics_manager_find_group_from_buffer (IdeDiagnosticsManager *self,
                                                IdeBuffer             *buffer)
{
  IdeDiagnosticsGroup *group;
  IdeFile *ifile;
  GFile *gfile;

  g_assert (IDE_IS_DIAGNOSTICS_MANAGER (self));
  g_assert (IDE_IS_BUFFER (buffer));

  ifile = ide_buffer_get_file (buffer);
  gfile = ide_file_get_file (ifile);
  group = g_hash_table_lookup (self->groups_by_file, gfile);

  if (group == NULL)
    {
      group = ide_diagnostics_group_new (gfile);
      g_hash_table_insert (self->groups_by_file, group->file, group);
    }

  g_assert (group != NULL);

  return group;
}

static IdeDiagnosticsGroup *
ide_diagnostics_manager_find_group_from_adapter (IdeDiagnosticsManager  *self,
                                                 IdeExtensionSetAdapter *adapter)
{
  GHashTableIter iter;
  gpointer value;

  g_assert (IDE_IS_DIAGNOSTICS_MANAGER (self));
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (adapter));

  g_hash_table_iter_init (&iter, self->groups_by_file);

  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      IdeDiagnosticsGroup *group = value;

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
                                         PeasExtension          *exten,
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

  g_assert (IDE_IS_DIAGNOSTICS_MANAGER (self));
  g_assert (IDE_IS_DIAGNOSTIC_PROVIDER (provider));

  g_hash_table_iter_init (&iter, self->groups_by_file);

  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      IdeDiagnosticsGroup *group = value;

      if (group->diagnostics_by_provider != NULL)
        {
          g_hash_table_remove (group->diagnostics_by_provider, provider);

          /*
           * If we caused this hashtable to become empty, we can release the
           * hashtable. The hashtable is guaranteed to not be empty if there
           * are other providers loaded for this group.
           */
          if (g_hash_table_size (group->diagnostics_by_provider) == 0)
            g_clear_pointer (&group->diagnostics_by_provider, g_hash_table_unref);

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
                                           PeasExtension          *exten,
                                           gpointer                user_data)
{
  IdeDiagnosticProvider *provider = (IdeDiagnosticProvider *)exten;
  IdeDiagnosticsManager *self = user_data;

  IDE_ENTRY;

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

static void
ide_diagnostics_manager_buffer_changed (IdeDiagnosticsManager *self,
                                        IdeBuffer             *buffer)
{
  IdeDiagnosticsGroup *group;

  IDE_ENTRY;

  g_assert (IDE_IS_DIAGNOSTICS_MANAGER (self));
  g_assert (IDE_IS_BUFFER (buffer));

  group = ide_diagnostics_manager_find_group_from_buffer (self, buffer);
  ide_diagnostics_group_queue_diagnose (group, self);

  IDE_EXIT;
}

static void
ide_diagnostics_manager_buffer_notify_language (IdeDiagnosticsManager *self,
                                                GParamSpec            *pspec,
                                                IdeBuffer             *buffer)
{
  IdeDiagnosticsGroup *group;
  GtkSourceLanguage *language;
  const gchar *language_id = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_DIAGNOSTICS_MANAGER (self));
  g_assert (pspec != NULL);
  g_assert (g_str_equal (pspec->name, "language"));
  g_assert (IDE_IS_BUFFER (buffer));

  /*
   * The goal here is to get the new language_id for the buffer and
   * alter the set of loaded diagnostic providers to match those registered
   * for the particular language_id. IdeExtensionSetAdapter does most of
   * the hard work, we just need to update the "match value".
   */

  language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (buffer));
  if (language != NULL)
    language_id = gtk_source_language_get_id (language);
  group = ide_diagnostics_manager_find_group_from_buffer (self, buffer);

  g_assert (IDE_IS_DIAGNOSTICS_MANAGER (self));
  g_assert (IDE_IS_BUFFER (buffer));

  if (group->adapter != NULL)
    ide_extension_set_adapter_set_value (group->adapter, language_id);

  IDE_EXIT;
}

void
ide_diagnostics_manager_update_group_by_file (IdeDiagnosticsManager *self,
                                              IdeBuffer             *buffer,
                                              GFile                 *new_file)
{
  GHashTableIter iter;
  gpointer value;

  g_assert (IDE_IS_DIAGNOSTICS_MANAGER (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_FILE (new_file));

  IDE_ENTRY;

  /*
   * The goal here is to steal the group that is in the hash table using
   * the old GFile, and replace it with the new GFile. That means removing
   * the group from the hashtable, changing the file field, and then
   * reinserting with our new file key.
   */
  g_hash_table_iter_init (&iter, self->groups_by_file);

  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      IdeDiagnosticsGroup *group = value;
      g_autoptr(IdeBuffer) group_buffer = g_weak_ref_get (&group->buffer_wr);

      if (buffer == group_buffer)
        {
          if (!g_file_equal (new_file, group->file))
            {
              g_hash_table_steal (self->groups_by_file, group->file);
              g_set_object (&group->file, new_file);
              g_hash_table_insert (self->groups_by_file, group->file, group);
            }

          IDE_EXIT;
        }
    }

  IDE_EXIT;
}

static void
ide_diagnostics_manager_buffer_notify_file (IdeDiagnosticsManager *self,
                                            GParamSpec            *pspec,
                                            IdeBuffer             *buffer)
{
  IdeFile *ifile;
  GFile *gfile;

  g_assert (IDE_IS_DIAGNOSTICS_MANAGER (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (pspec != NULL);
  g_assert (g_str_equal (pspec->name, "file"));

  ifile = ide_buffer_get_file (buffer);
  gfile = ide_file_get_file (ifile);

  ide_diagnostics_manager_update_group_by_file (self, buffer, gfile);
}

static void
ide_diagnostics_manager_buffer_loaded (IdeDiagnosticsManager *self,
                                       IdeBuffer             *buffer,
                                       IdeBufferManager      *buffer_manager)
{
  IdeDiagnosticsGroup *group;
  IdeContext *context;
  IdeFile *ifile;
  GFile *gfile;

  IDE_ENTRY;

  g_assert (IDE_IS_DIAGNOSTICS_MANAGER (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));

  /*
   * The goal below is to setup all of our state needed for tracking
   * diagnostics during the lifetime of the buffer. That includes tracking
   * a few properties to update our providers at runtime, along with
   * lifecycle tracking. At the end, we fire off a diagnosis so that we
   * have up to date diagnostics as soon as we can.
   */

  context = ide_object_get_context (IDE_OBJECT (self));

  g_signal_connect_object (buffer,
                           "changed",
                           G_CALLBACK (ide_diagnostics_manager_buffer_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (buffer,
                           "notify::file",
                           G_CALLBACK (ide_diagnostics_manager_buffer_notify_file),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (buffer,
                           "notify::language",
                           G_CALLBACK (ide_diagnostics_manager_buffer_notify_language),
                           self,
                           G_CONNECT_SWAPPED);

  ifile = ide_buffer_get_file (buffer);
  gfile = ide_file_get_file (ifile);

  group = g_hash_table_lookup (self->groups_by_file, gfile);

  if (group == NULL)
    {
      group = ide_diagnostics_group_new (gfile);
      g_hash_table_insert (self->groups_by_file, group->file, group);
    }

  g_weak_ref_set (&group->buffer_wr, buffer);

  if (group->diagnostics_by_provider == NULL)
    {
      group->diagnostics_by_provider =
        g_hash_table_new_full (NULL, NULL, NULL, free_diagnostics);
    }

  if (group->adapter == NULL)
    {
      GtkSourceLanguage *language;
      const gchar *language_id = NULL;

      language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (buffer));
      if (language != NULL)
        language_id = gtk_source_language_get_id (language);

      group->adapter = ide_extension_set_adapter_new (context,
                                                      peas_engine_get_default (),
                                                      IDE_TYPE_DIAGNOSTIC_PROVIDER,
                                                      "Diagnostic-Provider-Languages",
                                                      language_id);

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

  g_assert (g_hash_table_lookup (self->groups_by_file, gfile) == group);

  ide_diagnostics_group_queue_diagnose (group, self);

  IDE_EXIT;
}

static void
ide_diagnostics_manager_buffer_unloaded (IdeDiagnosticsManager *self,
                                         IdeBuffer             *buffer,
                                         IdeBufferManager      *buffer_manager)
{
  IdeDiagnosticsGroup *group;
  gboolean has_diagnostics;

  IDE_ENTRY;

  g_assert (IDE_IS_DIAGNOSTICS_MANAGER (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));

  /*
   * The goal here is to cleanup everything we can about this group that
   * is part of a loaded buffer. We might want to keep the group around
   * in case it is useful from other providers.
   */

  group = ide_diagnostics_manager_find_group_from_buffer (self, buffer);

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
  g_clear_object (&group->adapter);

  /*
   * Even after unloading the diagnostic providers, we might still have
   * diagnostics that were created from other files (this could happen when
   * one diagnostic is created for a header from a source file). So we don't
   * want to wipe out the hashtable unless everything was unloaded. The other
   * provider will cleanup during its own destruction.
   */
  if (group->diagnostics_by_provider != NULL &&
      g_hash_table_size (group->diagnostics_by_provider) == 0)
    g_clear_pointer (&group->diagnostics_by_provider, g_hash_table_unref);

  g_signal_handlers_disconnect_by_func (buffer,
                                        G_CALLBACK (ide_diagnostics_manager_buffer_changed),
                                        self);

  g_signal_handlers_disconnect_by_func (buffer,
                                        G_CALLBACK (ide_diagnostics_manager_buffer_notify_file),
                                        self);

  g_signal_handlers_disconnect_by_func (buffer,
                                        G_CALLBACK (ide_diagnostics_manager_buffer_notify_language),
                                        self);

  g_weak_ref_set (&group->buffer_wr, NULL);

  group->has_diagnostics = has_diagnostics;

  IDE_EXIT;
}

static gboolean
ide_diagnostics_manager_initable_init (GInitable     *initable,
                                       GCancellable  *cancellable,
                                       GError       **error)
{
  IdeDiagnosticsManager *self = (IdeDiagnosticsManager *)initable;
  IdeBufferManager *buffer_manager;
  IdeContext *context;
  guint n_items;

  IDE_ENTRY;

  g_assert (IDE_IS_DIAGNOSTICS_MANAGER (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  buffer_manager = ide_context_get_buffer_manager (context);

  /* We can start processing things when the buffer is loaded,
   * but we do so after buffer-loaded because we don't really
   * care to be before other subscribers. Our plugins might be
   * dependent on other things that are buffer specific.
   */
  g_signal_connect_object (buffer_manager,
                           "buffer-loaded",
                           G_CALLBACK (ide_diagnostics_manager_buffer_loaded),
                           self,
                           G_CONNECT_SWAPPED | G_CONNECT_AFTER);

  g_signal_connect_object (buffer_manager,
                           "buffer-unloaded",
                           G_CALLBACK (ide_diagnostics_manager_buffer_unloaded),
                           self,
                           G_CONNECT_SWAPPED);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (buffer_manager));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(IdeBuffer) buffer = NULL;

      buffer = g_list_model_get_item (G_LIST_MODEL (buffer_manager), i);
      ide_diagnostics_manager_buffer_loaded (self, buffer, buffer_manager);
    }

  IDE_RETURN (TRUE);
}

static void
initable_iface_init (GInitableIface *iface)
{
  iface->init = ide_diagnostics_manager_initable_init;
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

  g_return_val_if_fail (IDE_IS_DIAGNOSTICS_MANAGER (self), FALSE);

  g_hash_table_iter_init (&iter, self->groups_by_file);

  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      IdeDiagnosticsGroup *group = value;

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

  g_return_val_if_fail (IDE_IS_DIAGNOSTICS_MANAGER (self), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  ret = ide_diagnostics_new (NULL);

  group = g_hash_table_lookup (self->groups_by_file, file);

  if (group != NULL && group->diagnostics_by_provider != NULL)
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

          length = ide_diagnostics_get_size (diagnostics);

          for (guint i = 0; i < length; i++)
            {
              IdeDiagnostic *diagnostic = ide_diagnostics_index (diagnostics, i);

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

  g_return_val_if_fail (IDE_IS_DIAGNOSTICS_MANAGER (self), 0);
  g_return_val_if_fail (G_IS_FILE (file), 0);

  group = g_hash_table_lookup (self->groups_by_file, file);

  if (group != NULL)
    return group->sequence;

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
 *
 * Internally, this is the same as @buffer emitting the #IdeBuffer::changed
 * signal.
 *
 * Since: 3.28
 */
void
ide_diagnostics_manager_rediagnose (IdeDiagnosticsManager *self,
                                    IdeBuffer             *buffer)
{
  g_return_if_fail (IDE_IS_DIAGNOSTICS_MANAGER (self));
  g_return_if_fail (IDE_IS_BUFFER (buffer));
  g_return_if_fail (ide_buffer_get_context (buffer) ==
                    ide_object_get_context (IDE_OBJECT (self)));

  ide_diagnostics_manager_buffer_changed (self, buffer);
}
