/* gbp-spell-buffer-addin.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#include "gbp-spell-buffer-addin.h"

struct _GbpSpellBufferAddin
{
  GObject        parent_instance;

  /* Unowned reference to buffer */
  IdeBuffer     *buffer;
  GtkTextTag    *misspelled_tag;

  /* Owned spellchecker instance */
  GspellChecker *spellchecker;

  /* To allow for dynamic enabling of the inline spellcheck, we keep
   * track of how many views need it. We will enable the feature in
   * the buffer if it has manually been enabled (see @enabled) or if
   * this value is >= 1.
   */
  gint           count;

  /* Manual enabling of inline checking */
  guint          enabled : 1;
};

enum {
  PROP_0,
  PROP_ENABLED,
  N_PROPS
};

static gboolean
gbp_spell_buffer_addin_get_enabled (GbpSpellBufferAddin *self)
{
  g_assert (GBP_IS_SPELL_BUFFER_ADDIN (self));

  return self->enabled || self->count > 0;
}

static void
gbp_spell_buffer_addin_apply (GbpSpellBufferAddin *self)
{
  GspellTextBuffer *spell_buffer;

  IDE_ENTRY;

  g_assert (GBP_IS_SPELL_BUFFER_ADDIN (self));

  /* We might be disposed */
  if (self->buffer == NULL)
    return;

  spell_buffer = gspell_text_buffer_get_from_gtk_text_buffer (GTK_TEXT_BUFFER (self->buffer));

  if (!gbp_spell_buffer_addin_get_enabled (self))
    {
      GtkTextIter begin;
      GtkTextIter end;

      gspell_text_buffer_set_spell_checker (spell_buffer, NULL);
      g_clear_object (&self->spellchecker);

      gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (self->buffer), &begin, &end);
      gtk_text_buffer_remove_tag (GTK_TEXT_BUFFER (self->buffer),
                                  self->misspelled_tag, &begin, &end);

      return;
    }

  if (self->spellchecker == NULL)
    {

      /* Setup the spell checker for the buffer. We retrain the spellchecker
       * instance so that we can add words/modify the dictionary at runtime.
       */
      self->spellchecker = gspell_checker_new (NULL);
      gspell_text_buffer_set_spell_checker (spell_buffer, self->spellchecker);
    }

  IDE_EXIT;
}

static void
update_style_scheme (GbpSpellBufferAddin *self,
                     GParamSpec          *pspec,
                     IdeBuffer           *buffer)
{
  GtkSourceStyleScheme *scheme;

  g_assert (GBP_IS_SPELL_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  scheme = gtk_source_buffer_get_style_scheme (GTK_SOURCE_BUFFER (buffer));

  if (!ide_source_style_scheme_apply_style (scheme, "misspelled-match", self->misspelled_tag))
    g_object_set (self->misspelled_tag,
                  "underline", PANGO_UNDERLINE_SINGLE,
                  NULL);
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

  self->misspelled_tag = gtk_text_buffer_create_tag (GTK_TEXT_BUFFER (buffer), NULL, NULL);
  g_signal_connect_swapped (self->buffer,
                            "notify::style-scheme",
                            G_CALLBACK (update_style_scheme),
                            self);
  update_style_scheme (self, NULL, self->buffer);

  gbp_spell_buffer_addin_apply (self);

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

  g_signal_handlers_disconnect_by_func (buffer,
                                        G_CALLBACK (update_style_scheme),
                                        self);
  gtk_text_tag_table_remove (gtk_text_buffer_get_tag_table (GTK_TEXT_BUFFER (buffer)),
                             self->misspelled_tag);
  self->misspelled_tag = NULL;

  self->buffer = NULL;
  gbp_spell_buffer_addin_apply (self);

  IDE_EXIT;
}

static void
buffer_addin_iface_init (IdeBufferAddinInterface *iface)
{
  iface->load = gbp_spell_buffer_addin_load;
  iface->unload = gbp_spell_buffer_addin_unload;
}

G_DEFINE_TYPE_WITH_CODE (GbpSpellBufferAddin, gbp_spell_buffer_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_BUFFER_ADDIN, buffer_addin_iface_init))

static GParamSpec *properties [N_PROPS];

static void
gbp_spell_buffer_addin_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GbpSpellBufferAddin *self = GBP_SPELL_BUFFER_ADDIN (object);

  switch (prop_id)
    {
    case PROP_ENABLED:
      g_value_set_boolean (value, gbp_spell_buffer_addin_get_enabled (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_spell_buffer_addin_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GbpSpellBufferAddin *self = GBP_SPELL_BUFFER_ADDIN (object);

  switch (prop_id)
    {
    case PROP_ENABLED:
      self->enabled = g_value_get_boolean (value);
      gbp_spell_buffer_addin_apply (self);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_spell_buffer_addin_class_init (GbpSpellBufferAddinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gbp_spell_buffer_addin_get_property;
  object_class->set_property = gbp_spell_buffer_addin_set_property;

  properties [PROP_ENABLED] =
    g_param_spec_boolean ("enabled",
                          "Enabled",
                          "If the spellchecker is enabled",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_spell_buffer_addin_init (GbpSpellBufferAddin *self)
{
}

/**
 * gbp_spell_buffer_addin_get_checker:
 * @self: a #GbpSpellBufferAddin
 *
 * Gets the #GspellChecker used by the underlying buffer, or %NULL if
 * no spellchecker is active.
 *
 * Returns: (transfer none): a #GspellChecker
 *
 * Since: 3.26
 */
GspellChecker *
gbp_spell_buffer_addin_get_checker (GbpSpellBufferAddin *self)
{
  g_return_val_if_fail (GBP_IS_SPELL_BUFFER_ADDIN (self), NULL);

  return self->spellchecker;
}

/**
 * gbp_spell_buffer_addin_begin_checking:
 * @self: a #GbpSpellBufferAddin
 *
 * Views should call this function when they begin their spellchecking
 * process. It dynamically enables various features on the buffer that
 * are necessary for spellchecking.
 *
 * When done, the consumer MUST call gbp_spell_buffer_addin_end_checking()
 * to complete the process. If no more views are active, spellchecking
 * may be disabled on the buffer.
 *
 * Since: 3.26
 */
void
gbp_spell_buffer_addin_begin_checking (GbpSpellBufferAddin *self)
{
  gboolean before_state;
  gboolean after_state;

  g_return_if_fail (GBP_IS_SPELL_BUFFER_ADDIN (self));
  g_return_if_fail (self->count >= 0);

  before_state = gbp_spell_buffer_addin_get_enabled (self);
  self->count++;
  after_state = gbp_spell_buffer_addin_get_enabled (self);

  if (before_state != after_state)
    {
      gbp_spell_buffer_addin_apply (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ENABLED]);
    }
}

/**
 * gbp_spell_buffer_addin_end_checking:
 * @self: a #GbpSpellBufferAddin
 *
 * Completes a spellcheck operation. The buffer will return to it's original
 * state. Thay may mean inline checking is disabled.
 */
void
gbp_spell_buffer_addin_end_checking (GbpSpellBufferAddin *self)
{
  gboolean before_state;
  gboolean after_state;

  g_return_if_fail (GBP_IS_SPELL_BUFFER_ADDIN (self));
  g_return_if_fail (self->count >= 0);

  before_state = gbp_spell_buffer_addin_get_enabled (self);
  self->count--;
  after_state = gbp_spell_buffer_addin_get_enabled (self);

  if (before_state != after_state)
    {
      gbp_spell_buffer_addin_apply (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ENABLED]);
    }
}

/**
 * gbp_spell_buffer_addin_get_misspelled_tag:
 * @self: a #GbpSpellBufferAddin
 *
 * Gets the tag to use for the current misspelled word.
 *
 * Returns: (nullable) (transfer none): a #GtkTextTag or %NULL.
 */
GtkTextTag *
gbp_spell_buffer_addin_get_misspelled_tag (GbpSpellBufferAddin *self)
{
  g_return_val_if_fail (GBP_IS_SPELL_BUFFER_ADDIN (self), NULL);

  return self->misspelled_tag;
}
