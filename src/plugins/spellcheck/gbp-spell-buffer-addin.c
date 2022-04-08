/* gbp-spell-buffer-addin.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-spell-buffer-addin"

#include "config.h"

#include <libide-code.h>

#include "gbp-spell-buffer-addin.h"

#include "editor-spell-checker.h"
#include "editor-text-buffer-spell-adapter.h"

#define METADATA_SPELLING "metadata::gte-spelling"

struct _GbpSpellBufferAddin
{
  GObject parent_instance;
  IdeBuffer *buffer;
  EditorSpellChecker *checker;
  EditorTextBufferSpellAdapter *adapter;
  GPropertyAction *enabled_action;
};

static void
check_error (GObject      *object,
             GAsyncResult *result,
             gpointer      user_data)
{
  GFile *file = (GFile *)object;
  g_autoptr(GError) error = NULL;

  if (!g_file_set_attributes_finish (file, result, NULL, &error))
    g_warning ("Failed to persist metadata: %s", error->message);
}

static void
checker_notify_language_cb (GbpSpellBufferAddin *self,
                            GParamSpec          *pspec,
                            EditorSpellChecker  *spell_checker)
{
  g_autoptr(GFileInfo) info = NULL;
  const char *language_id;
  GFile *file;

  g_assert (GBP_IS_SPELL_BUFFER_ADDIN (self));
  g_assert (EDITOR_IS_SPELL_CHECKER (spell_checker));
  g_assert (IDE_IS_BUFFER (self->buffer));

  /* Only persist the metadata if we have a backing file */
  if (!(file = ide_buffer_get_file (self->buffer)) || !g_file_is_native (file))
    return;

  /* Ignore if there is nothing to set */
  if (!(language_id = editor_spell_checker_get_language (spell_checker)))
    return;

  info = g_file_info_new ();
  g_file_info_set_attribute_string (info, METADATA_SPELLING, language_id);
  g_file_set_attributes_async (file, info, G_FILE_QUERY_INFO_NONE,
                               G_PRIORITY_DEFAULT, NULL, check_error, NULL);
}

static void
gbp_spell_buffer_addin_load (IdeBufferAddin *addin,
                             IdeBuffer      *buffer)
{
  GbpSpellBufferAddin *self = (GbpSpellBufferAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_SPELL_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  self->buffer = buffer;
  self->checker = editor_spell_checker_new (NULL, NULL);
  self->adapter = editor_text_buffer_spell_adapter_new (GTK_TEXT_BUFFER (buffer), self->checker);

  g_signal_connect_object (self->checker,
                           "notify::language",
                           G_CALLBACK (checker_notify_language_cb),
                           self,
                           G_CONNECT_SWAPPED);

  self->enabled_action = g_property_action_new ("enabled", self->adapter, "enabled");

  editor_text_buffer_spell_adapter_set_enabled (self->adapter, TRUE);

  IDE_EXIT;
}

static void
gbp_spell_buffer_addin_unload (IdeBufferAddin *addin,
                               IdeBuffer      *buffer)
{
  GbpSpellBufferAddin *self = (GbpSpellBufferAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_SPELL_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  g_signal_handlers_disconnect_by_func (self->checker,
                                        G_CALLBACK (checker_notify_language_cb),
                                        self);

  g_clear_object (&self->checker);
  g_clear_object (&self->adapter);
  g_clear_object (&self->enabled_action);

  self->buffer = NULL;

  IDE_EXIT;
}

static void
buffer_addin_iface_init (IdeBufferAddinInterface *iface)
{
  iface->load = gbp_spell_buffer_addin_load;
  iface->unload = gbp_spell_buffer_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpSpellBufferAddin, gbp_spell_buffer_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_BUFFER_ADDIN, buffer_addin_iface_init))

static void
gbp_spell_buffer_addin_class_init (GbpSpellBufferAddinClass *klass)
{
}

static void
gbp_spell_buffer_addin_init (GbpSpellBufferAddin *self)
{
}

void
gbp_spell_buffer_addin_add_word (GbpSpellBufferAddin *self,
                                 const char          *word)
{
  IDE_ENTRY;

  g_return_if_fail (GBP_IS_SPELL_BUFFER_ADDIN (self));

  if (word == NULL || word[0] == 0)
    IDE_EXIT;

  editor_spell_checker_add_word (self->checker, word);
  editor_text_buffer_spell_adapter_invalidate_all (self->adapter);

  IDE_EXIT;
}

void
gbp_spell_buffer_addin_ignore_word (GbpSpellBufferAddin *self,
                                    const char          *word)
{
  IDE_ENTRY;

  g_return_if_fail (GBP_IS_SPELL_BUFFER_ADDIN (self));

  if (word == NULL || word[0] == 0)
    IDE_EXIT;

  editor_spell_checker_ignore_word (self->checker, word);
  editor_text_buffer_spell_adapter_invalidate_all (self->adapter);

  IDE_EXIT;
}

gboolean
gbp_spell_buffer_addin_check_spelling (GbpSpellBufferAddin *self,
                                       const char          *word)
{
  g_return_val_if_fail (GBP_IS_SPELL_BUFFER_ADDIN (self), FALSE);

  if (self->checker != NULL && word != NULL)
    return editor_spell_checker_check_word (self->checker, word, -1);

  return TRUE;
}

char **
gbp_spell_buffer_addin_list_corrections (GbpSpellBufferAddin *self,
                                         const char          *word)
{
  g_return_val_if_fail (GBP_IS_SPELL_BUFFER_ADDIN (self), NULL);

  if (self->checker != NULL && word != NULL)
    return editor_spell_checker_list_corrections (self->checker, word);

  return NULL;
}

GAction *
gbp_spell_buffer_addin_get_enabled_action (GbpSpellBufferAddin *self)
{
  g_return_val_if_fail (GBP_IS_SPELL_BUFFER_ADDIN (self), NULL);

  return G_ACTION (self->enabled_action);
}
