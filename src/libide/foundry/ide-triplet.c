/* ide-triplet.c
 *
 * Copyright 2018 Corentin Noël <corentin.noel@collabora.com>
 * Copyright 2018 Collabora Ltd.
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

#define G_LOG_DOMAIN "ide-triplet"

#include "config.h"

#include "ide-triplet.h"

G_DEFINE_BOXED_TYPE (IdeTriplet, ide_triplet, ide_triplet_ref, ide_triplet_unref)

struct _IdeTriplet
{
  volatile gint ref_count;

  gchar *full_name;
  gchar *arch;
  gchar *vendor;
  gchar *kernel;
  gchar *operating_system;
};

static IdeTriplet *
_ide_triplet_construct (void)
{
  IdeTriplet *self;

  self = g_slice_new0 (IdeTriplet);
  self->ref_count = 1;
  self->full_name = NULL;
  self->arch = NULL;
  self->vendor = NULL;
  self->kernel = NULL;
  self->operating_system = NULL;

  return self;
}

/**
 * ide_triplet_new:
 * @full_name: The complete identifier of the machine
 *
 * Creates a new #IdeTriplet from a given identifier. This identifier
 * can be a simple architecture name, a duet of "arch-kernel" (like "m68k-coff"), a triplet
 * of "arch-kernel-os" (like "x86_64-linux-gnu") or a quadriplet of "arch-vendor-kernel-os"
 * (like "i686-pc-linux-gnu")
 *
 * Returns: (transfer full): An #IdeTriplet.
 */
IdeTriplet *
ide_triplet_new (const gchar *full_name)
{
  IdeTriplet *self;
  g_auto (GStrv) parts = NULL;
  guint parts_length = 0;

  g_return_val_if_fail (full_name != NULL, NULL);

  self = _ide_triplet_construct ();
  self->full_name = g_strdup (full_name);

  parts = g_strsplit (full_name, "-", 4);
  parts_length = g_strv_length (parts);
  /* Currently they can't have more than 4 parts */
  if (parts_length >= 4)
    {
      self->arch = g_strdup (parts[0]);
      self->vendor = g_strdup (parts[1]);
      self->kernel = g_strdup (parts[2]);
      self->operating_system = g_strdup (parts[3]);
    }
  else if (parts_length == 3)
    {
      self->arch = g_strdup (parts[0]);
      self->kernel = g_strdup (parts[1]);
      self->operating_system = g_strdup (parts[2]);
    }
  else if (parts_length == 2)
    {
      self->arch = g_strdup (parts[0]);
      self->kernel = g_strdup (parts[1]);
    }
  else if (parts_length == 1)
    self->arch = g_strdup (parts[0]);

  return self;
}

/**
 * ide_triplet_new_from_system:
 *
 * Creates a new #IdeTriplet from a the current system information
 *
 * Returns: (transfer full): An #IdeTriplet.
 */
IdeTriplet *
ide_triplet_new_from_system (void)
{
  static IdeTriplet *system_triplet;

  if (g_once_init_enter (&system_triplet))
    g_once_init_leave (&system_triplet, ide_triplet_new (ide_get_system_type ()));

  return ide_triplet_ref (system_triplet);
}

/**
 * ide_triplet_new_with_triplet:
 * @arch: The name of the architecture of the machine (like "x86_64")
 * @kernel: (nullable): The name of the kernel of the machine (like "linux")
 * @operating_system: (nullable): The name of the os of the machine
 * (like "gnuabi64")
 *
 * Creates a new #IdeTriplet from a given triplet of "arch-kernel-os"
 * (like "x86_64-linux-gnu")
 *
 * Returns: (transfer full): An #IdeTriplet.
 */
IdeTriplet *
ide_triplet_new_with_triplet (const gchar *arch,
                              const gchar *kernel,
                              const gchar *operating_system)
{
  IdeTriplet *self;
  g_autofree gchar *full_name = NULL;

  g_return_val_if_fail (arch != NULL, NULL);

  self = _ide_triplet_construct ();
  self->arch = g_strdup (arch);
  self->kernel = g_strdup (kernel);
  self->operating_system = g_strdup (operating_system);

  full_name = g_strdup (arch);
  if (kernel != NULL)
    {
      g_autofree gchar *start_full_name = full_name;
      full_name = g_strdup_printf ("%s-%s", start_full_name, kernel);
    }

  if (operating_system != NULL)
    {
      g_autofree gchar *start_full_name = full_name;
      full_name = g_strdup_printf ("%s-%s", start_full_name, operating_system);
    }

  self->full_name = g_steal_pointer (&full_name);

  return self;
}

/**
 * ide_triplet_new_with_quadruplet:
 * @arch: The name of the architecture of the machine (like "x86_64")
 * @vendor: (nullable): The name of the vendor of the machine (like "pc")
 * @kernel: (nullable): The name of the kernel of the machine (like "linux")
 * @operating_system: (nullable): The name of the os of the machine (like "gnuabi64")
 *
 * Creates a new #IdeTriplet from a given quadruplet of
 * "arch-vendor-kernel-os" (like "i686-pc-linux-gnu")
 *
 * Returns: (transfer full): An #IdeTriplet.
 */
IdeTriplet *
ide_triplet_new_with_quadruplet (const gchar *arch,
                                 const gchar *vendor,
                                 const gchar *kernel,
                                 const gchar *operating_system)
{
  IdeTriplet *self;
  g_autofree gchar *full_name = NULL;

  g_return_val_if_fail (arch != NULL, NULL);

  if (vendor == NULL)
    return ide_triplet_new_with_triplet (arch, kernel, operating_system);

  self = _ide_triplet_construct ();
  self->arch = g_strdup (arch);
  self->vendor = g_strdup (vendor);
  self->kernel = g_strdup (kernel);
  self->operating_system = g_strdup (operating_system);

  full_name = g_strdup_printf ("%s-%s", arch, vendor);
  if (kernel != NULL)
    {
      g_autofree gchar *start_full_name = full_name;
      full_name = g_strdup_printf ("%s-%s", start_full_name, kernel);
    }

  if (operating_system != NULL)
    {
      g_autofree gchar *start_full_name = full_name;
      full_name = g_strdup_printf ("%s-%s", start_full_name, operating_system);
    }

  self->full_name = g_steal_pointer (&full_name);

  return self;
}

static void
ide_triplet_finalize (IdeTriplet *self)
{
  g_free (self->full_name);
  g_free (self->arch);
  g_free (self->vendor);
  g_free (self->kernel);
  g_free (self->operating_system);
  g_slice_free (IdeTriplet, self);
}

/**
 * ide_triplet_ref:
 * @self: An #IdeTriplet
 *
 * Increases the reference count of @self
 *
 * Returns: (transfer none): An #IdeTriplet.
 */
IdeTriplet *
ide_triplet_ref (IdeTriplet *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count > 0, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

/**
 * ide_triplet_unref:
 * @self: An #IdeTriplet
 *
 * Decreases the reference count of @self
 * Once the reference count reaches 0, the object is freed.
 */
void
ide_triplet_unref (IdeTriplet *self)
{
  g_return_if_fail (self);
  g_return_if_fail (self->ref_count > 0);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    ide_triplet_finalize (self);
}

/**
 * ide_triplet_get_full_name:
 * @self: An #IdeTriplet
 *
 * Gets the full name of the machine configuration name (can be an architecture name,
 * a duet, a triplet or a quadruplet).
 *
 * Returns: (transfer none): The full name of the machine configuration name
 */
const gchar *
ide_triplet_get_full_name (IdeTriplet *self)
{
  g_return_val_if_fail (self, NULL);

  return self->full_name;
}

/**
 * ide_triplet_get_arch:
 * @self: An #IdeTriplet
 *
 * Gets the architecture name of the machine
 *
 * Returns: (transfer none): The architecture name of the machine
 */
const gchar *
ide_triplet_get_arch (IdeTriplet *self)
{
  g_return_val_if_fail (self, NULL);

  return self->arch;
}

/**
 * ide_triplet_get_vendor:
 * @self: An #IdeTriplet
 *
 * Gets the vendor name of the machine
 *
 * Returns: (transfer none) (nullable): The vendor name of the machine
 */
const gchar *
ide_triplet_get_vendor (IdeTriplet *self)
{
  g_return_val_if_fail (self, NULL);

  return self->vendor;
}

/**
 * ide_triplet_get_kernel:
 * @self: An #IdeTriplet
 *
 * Gets name of the kernel of the machine
 *
 * Returns: (transfer none) (nullable): The name of the kernel of the machine
 */
const gchar *
ide_triplet_get_kernel (IdeTriplet *self)
{
  g_return_val_if_fail (self, NULL);

  return self->kernel;
}

/**
 * ide_triplet_get_operating_system:
 * @self: An #IdeTriplet
 *
 * Gets name of the operating system of the machine
 *
 * Returns: (transfer none) (nullable): The name of the operating system of the machine
 */
const gchar *
ide_triplet_get_operating_system (IdeTriplet *self)
{
  g_return_val_if_fail (self, NULL);

  return self->operating_system;
}


/**
 * ide_triplet_is_system:
 * @self: An #IdeTriplet
 *
 * Gets whether this is the same architecture as the system
 *
 * Returns: %TRUE if this is the same architecture as the system, %FALSE otherwise
 */
gboolean
ide_triplet_is_system (IdeTriplet *self)
{
  g_autofree gchar *system_arch = ide_get_system_arch ();

  g_return_val_if_fail (self, FALSE);

  return g_strcmp0 (self->arch, system_arch) == 0;
}
