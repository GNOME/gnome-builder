/* ide-machine-config-name.c
 *
 * Copyright (C) 2018 Corentin Noël <corentin.noel@collabora.com>
 * Copyright (C) 2018 Collabora Ltd.
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

#define G_LOG_DOMAIN "ide-machine-config-name"

#include "config.h"

#include "util/ide-machine-config-name.h"
#include "util/ide-posix.h"

G_DEFINE_BOXED_TYPE (IdeMachineConfigName, ide_machine_config_name, ide_machine_config_name_ref, ide_machine_config_name_unref)

struct _IdeMachineConfigName
{
  volatile gint ref_count;

  gchar *full_name;
  gchar *cpu;
  gchar *vendor;
  gchar *kernel;
  gchar *operating_system;
};

static IdeMachineConfigName *
_ide_machine_config_name_construct (void)
{
  IdeMachineConfigName *self;

  self = g_new0 (IdeMachineConfigName, 1);
  self->ref_count = 1;
  self->full_name = NULL;
  self->cpu = NULL;
  self->vendor = NULL;
  self->kernel = NULL;
  self->operating_system = NULL;

  return self;
}

/**
 * ide_machine_config_name_new:
 * @full_name: The complete identifier of the machine
 *
 * Creates a new #IdeMachineConfigName from a given identifier. This identifier
  * can be a simple cpu architecture name, a duet of "cpu-kernel" (like "m68k-coff"), a triplet
  * of "cpu-kernel-os" (like "x86_64-linux-gnu") or a quadriplet of "cpu-vendor-kernel-os"
  * (like "i686-pc-linux-gnu")
 *
 * Returns: (transfer none): An #IdeMachineConfigName.
 *
 * Since: 3.30
 */
IdeMachineConfigName *
ide_machine_config_name_new (const gchar *full_name)
{
  IdeMachineConfigName *self;
  g_auto (GStrv) parts = NULL;
  guint parts_length = 0;

  self = _ide_machine_config_name_construct ();
  self->full_name = g_strdup (full_name);

  parts = g_strsplit (full_name, "-", -1);
  parts_length = g_strv_length (parts);
  /* Currently they can't have more than 4 parts */
  if (parts_length >= 4)
    {
      self->cpu = g_strdup (parts[0]);
      self->vendor = g_strdup (parts[1]);
      self->kernel = g_strdup (parts[2]);
      self->operating_system = g_strdup (parts[3]);
    }
  else if (parts_length == 3)
    {
      self->cpu = g_strdup (parts[0]);
      self->kernel = g_strdup (parts[1]);
      self->operating_system = g_strdup (parts[2]);
    }
  else if (parts_length == 2)
    {
      self->cpu = g_strdup (parts[0]);
      self->kernel = g_strdup (parts[1]);
    }
  else if (parts_length == 1)
    self->cpu = g_strdup (parts[0]);

  return self;
}

/**
 * ide_machine_config_name_new_from_system:
 *
 * Creates a new #IdeMachineConfigName from a the current system information
 *
 * Returns: (transfer none): An #IdeMachineConfigName.
 *
 * Since: 3.30
 */
IdeMachineConfigName *
ide_machine_config_name_new_from_system ()
{
  static IdeMachineConfigName *system_machine_config_name;

  if (system_machine_config_name != NULL)
    return ide_machine_config_name_ref (system_machine_config_name);

  system_machine_config_name = ide_machine_config_name_new (ide_get_system_type ());

  return ide_machine_config_name_ref (system_machine_config_name);
}

/**
 * ide_machine_config_name_new_with_triplet:
 * @cpu: The name of the cpu of the machine (like "x86_64")
 * @kernel: (allow-none): The name of the kernel of the machine (like "linux")
 * @operating_system: (allow-none): The name of the cpu of the machine
 * (like "gnuabi64")
 *
 * Creates a new #IdeMachineConfigName from a given triplet of "cpu-kernel-os"
 * (like "x86_64-linux-gnu")
 *
 * Returns: (transfer none): An #IdeMachineConfigName.
 *
 * Since: 3.30
 */
IdeMachineConfigName *
ide_machine_config_name_new_with_triplet (const gchar *cpu,
                                          const gchar *kernel,
                                          const gchar *operating_system)
{
  IdeMachineConfigName *self;
  g_autofree gchar *full_name = NULL;

  g_return_val_if_fail (cpu != NULL, NULL);

  self = _ide_machine_config_name_construct ();
  self->cpu = g_strdup (cpu);
  self->kernel = g_strdup (kernel);
  self->operating_system = g_strdup (operating_system);

  full_name = g_strdup (cpu);
  if (kernel != NULL)
    full_name = g_strdup_printf ("%s-%s", full_name, kernel);

  if (operating_system != NULL)
    full_name = g_strdup_printf ("%s-%s", full_name, operating_system);

  self->full_name = g_steal_pointer (&full_name);

  return self;
}

/**
 * ide_machine_config_name_new_with_quadruplet:
 * @cpu: The name of the cpu of the machine (like "x86_64")
 * @vendor: (allow-none): The name of the vendor of the machine (like "pc")
 * @kernel: (allow-none): The name of the kernel of the machine (like "linux")
 * @operating_system: (allow-none): The name of the cpu of the machine 
 * (like "gnuabi64")
 *
 * Creates a new #IdeMachineConfigName from a given quadruplet of 
 * "cpu-vendor-kernel-os" (like "i686-pc-linux-gnu")
 *
 * Returns: (transfer none): An #IdeMachineConfigName.
 *
 * Since: 3.30
 */
IdeMachineConfigName *
ide_machine_config_name_new_with_quadruplet (const gchar *cpu,
                                             const gchar *vendor,
                                             const gchar *kernel,
                                             const gchar *operating_system)
{
  IdeMachineConfigName *self;
  g_autofree gchar *full_name = NULL;

  g_return_val_if_fail (cpu != NULL, NULL);

  if (vendor == NULL)
    return ide_machine_config_name_new_with_triplet (cpu, kernel, operating_system);

  self = _ide_machine_config_name_construct ();
  self->ref_count = 1;
  self->cpu = g_strdup (cpu);
  self->vendor = g_strdup (vendor);
  self->kernel = g_strdup (kernel);
  self->operating_system = g_strdup (operating_system);

  full_name = g_strdup_printf ("%s-%s", cpu, vendor);
  if (kernel != NULL)
    full_name = g_strdup_printf ("%s-%s", full_name, kernel);

  if (operating_system != NULL)
    full_name = g_strdup_printf ("%s-%s", full_name, operating_system);

  self->full_name = g_steal_pointer (&full_name);

  return self;
}

static void
ide_machine_config_name_finalize (IdeMachineConfigName *self)
{
  g_free (self->full_name);
  g_free (self->cpu);
  g_free (self->vendor);
  g_free (self->kernel);
  g_free (self->operating_system);
  g_free (self);
}

/**
 * ide_machine_config_name_ref:
 * @self: An #IdeMachineConfigName
 *
 * Increases the reference count of @self
 *
 * Returns: (transfer none): An #IdeMachineConfigName.
 *
 * Since: 3.30
 */
IdeMachineConfigName *
ide_machine_config_name_ref (IdeMachineConfigName *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count > 0, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

/**
 * ide_machine_config_name_unref:
 * @self: An #IdeMachineConfigName
 *
 * Decreases the reference count of @self
 * Once the reference count reaches 0, the object is freed.
 *
 * Since: 3.30
 */
void
ide_machine_config_name_unref (IdeMachineConfigName *self)
{
  g_return_if_fail (self);
  g_return_if_fail (self->ref_count > 0);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    ide_machine_config_name_finalize (self);
}

/**
 * ide_machine_config_name_get_full_name:
 * @self: An #IdeMachineConfigName
 *
 * Gets the full name of the machine configuration name (can be a cpu name,
 * a duet, a triplet or a quadruplet).
 *
 * Returns: (transfer none): The full name of the machine configuration name
 *
 * Since: 3.30
 */
const gchar *
ide_machine_config_name_get_full_name (IdeMachineConfigName *self)
{
  g_return_val_if_fail (self, NULL);

  return self->full_name;
}

/**
 * ide_machine_config_name_get_cpu:
 * @self: An #IdeMachineConfigName
 *
 * Gets the cpu name of the machine
 *
 * Returns: (transfer none): The cpu name of the machine
 */
const gchar *
ide_machine_config_name_get_cpu (IdeMachineConfigName *self)
{
  g_return_val_if_fail (self, NULL);

  return self->cpu;
}

/**
 * ide_machine_config_name_get_vendor:
 * @self: An #IdeMachineConfigName
 *
 * Gets the vendor name of the machine
 *
 * Returns: (transfer none) (nullable): The vendor name of the machine
 *
 * Since: 3.30
 */
const gchar *
ide_machine_config_name_get_vendor (IdeMachineConfigName *self)
{
  g_return_val_if_fail (self, NULL);

  return self->vendor;
}

/**
 * ide_machine_config_name_get_kernel:
 * @self: An #IdeMachineConfigName
 *
 * Gets name of the kernel of the machine
 *
 * Returns: (transfer none) (nullable): The name of the kernel of the machine
 *
 * Since: 3.30
 */
const gchar *
ide_machine_config_name_get_kernel (IdeMachineConfigName *self)
{
  g_return_val_if_fail (self, NULL);

  return self->kernel;
}

/**
 * ide_machine_config_name_get_operating_system:
 * @self: An #IdeMachineConfigName
 *
 * Gets name of the operating system of the machine
 *
 * Returns: (transfer none) (nullable): The name of the operating system of the machine
 *
 * Since: 3.30
 */
const gchar *
ide_machine_config_name_get_operating_system (IdeMachineConfigName *self)
{
  g_return_val_if_fail (self, NULL);

  return self->operating_system;
}


/**
 * ide_machine_config_name_is_system:
 * @self: An #IdeMachineConfigName
 *
 * Gets whether this is the same architecture as the system
 *
 * Returns: %TRUE if this is the same architecture as the system, %FALSE otherwise
 *
 * Since: 3.30
 */
gboolean
ide_machine_config_name_is_system (IdeMachineConfigName *self)
{
  g_autofree gchar *system_arch = ide_get_system_arch ();

  g_return_val_if_fail (self, FALSE);

  return g_strcmp0 (self->cpu, system_arch) == 0;
}
