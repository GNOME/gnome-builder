/* egg-counter.c
 *
 * Copyright (C) 2013-2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#ifdef __linux__
# include <dlfcn.h>
#endif
#include <glib/gprintf.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "egg-counter.h"

G_DEFINE_BOXED_TYPE (EggCounterArena, egg_counter_arena, egg_counter_arena_ref, egg_counter_arena_unref)

#define NAME_FORMAT        "/EggCounters-%u"
#define MAGIC              0x71167125
#define COUNTER_MAX_SHM    (1024 * 1024 * 4)
#define COUNTERS_PER_GROUP 8
#define DATA_CELL_SIZE     64
#define CELLS_PER_INFO     (sizeof(CounterInfo) / DATA_CELL_SIZE)
#define CELLS_PER_HEADER   2
#define CELLS_PER_GROUP(ncpu)                             \
  (((sizeof (CounterInfo) * COUNTERS_PER_GROUP) +         \
    (sizeof(EggCounterValue) * (ncpu))) / DATA_CELL_SIZE)
#define EGG_MEMORY_BARRIER __sync_synchronize()

typedef struct
{
  guint cell : 29;       /* Counter groups starting cell */
  guint position : 3;    /* Index within counter group */
  gchar category[20];    /* Counter category name. */
  gchar name[32];        /* Counter name. */
  gchar description[72]; /* Counter description */
} CounterInfo __attribute__((aligned (DATA_CELL_SIZE)));

G_STATIC_ASSERT (sizeof (CounterInfo) == 128);
G_STATIC_ASSERT (CELLS_PER_INFO == 2);
G_STATIC_ASSERT (CELLS_PER_GROUP(1) == 17);
G_STATIC_ASSERT (CELLS_PER_GROUP(2) == 18);
G_STATIC_ASSERT (CELLS_PER_GROUP(4) == 20);
G_STATIC_ASSERT (CELLS_PER_GROUP(8) == 24);
G_STATIC_ASSERT (CELLS_PER_GROUP(16) == 32);

typedef struct
{
  gint64 values[8];
} DataCell __attribute__((aligned (DATA_CELL_SIZE)));

G_STATIC_ASSERT (sizeof (DataCell) == 64);

typedef struct
{
  guint32 magic;         /* Expected magic value */
  guint32 size;          /* Size of underlying shm file */
  guint32 ncpu;          /* Number of CPUs registered with */
  guint32 first_offset;  /* Offset to first counter info in cells */
  guint32 n_counters;    /* Number of CounterInfos */
  gchar   padding [108];
} ShmHeader __attribute__((aligned (DATA_CELL_SIZE)));

G_STATIC_ASSERT (sizeof(ShmHeader) == (DATA_CELL_SIZE * CELLS_PER_HEADER));

struct _EggCounterArena
{
  gint      ref_count;
  guint     arena_is_malloced : 1;
  guint     data_is_mmapped : 1;
  guint     is_local_arena : 1;
  gsize     n_cells;
  DataCell *cells;
  gsize     data_length;
  GPid      pid;
  guint     n_counters;
  GList    *counters;
};

G_LOCK_DEFINE_STATIC (reglock);

static void _egg_counter_init_getcpu (void) __attribute__ ((constructor));
static guint (*_egg_counter_getcpu_helper) (void);

gint64
egg_counter_get (EggCounter *counter)
{
  gint64 value = 0;
  guint ncpu;
  guint i;

  g_return_val_if_fail (counter, G_GINT64_CONSTANT (-1));

  ncpu = g_get_num_processors ();

  EGG_MEMORY_BARRIER;

  for (i = 0; i < ncpu; i++)
    value += counter->values [i].value;

  return value;
}

void
egg_counter_reset (EggCounter *counter)
{
  guint ncpu;
  guint i;

  g_return_if_fail (counter);

  ncpu = g_get_num_processors ();

  for (i = 0; i < ncpu; i++)
    counter->values [i].value = 0;

  EGG_MEMORY_BARRIER;
}

static void
_egg_counter_arena_atexit (void)
{
  gchar name [32];
  gint pid;

  pid = getpid ();
  g_snprintf (name, sizeof name, NAME_FORMAT, pid);
  shm_unlink (name);
}

static void
_egg_counter_arena_init_local (EggCounterArena *arena)
{
  ShmHeader *header;
  gpointer mem;
  unsigned pid;
  gsize size;
  gint page_size;
  gint fd;
  gchar name [32];

  page_size = sysconf (_SC_PAGE_SIZE);

  /* Implausible, but squashes warnings. */
  if (page_size < 4096)
    {
      page_size = 4096;
      size = page_size * 4;
      goto use_malloc;
    }

  /*
   * FIXME: https://bugzilla.gnome.org/show_bug.cgi?id=749280
   *
   * We have some very tricky work ahead of us to add unlimited numbers
   * of counters at runtime. We basically need to avoid placing counters
   * that could overlap a page.
   */
  size = page_size * 4;

  arena->ref_count = 1;
  arena->is_local_arena = TRUE;

  if (getenv ("EGG_COUNTER_DISABLE_SHM"))
    goto use_malloc;

  pid = getpid ();
  g_snprintf (name, sizeof name, NAME_FORMAT, pid);

  if (-1 == (fd = shm_open (name, O_CREAT|O_RDWR, S_IRUSR|S_IWUSR|S_IRGRP)))
    goto use_malloc;

  /*
   * ftruncate() will cause reads to be zero. Therefore, we don't need to
   * do write() of zeroes to initialize the shared memory area.
   */
  if (-1 == ftruncate (fd, size))
    goto failure;

  /*
   * Memory map the shared memory segement so that we can store our counters
   * within it. We need to layout the counters into the segment so that other
   * processes can traverse and read the values by loading the shared page.
   */
  mem = mmap (NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
  if (mem == MAP_FAILED)
    goto failure;

  close (fd);
  atexit (_egg_counter_arena_atexit);

  arena->data_is_mmapped = TRUE;
  arena->cells = mem;
  arena->n_cells = (size / DATA_CELL_SIZE);
  arena->data_length = size;

  header = mem;
  header->magic = MAGIC;
  header->ncpu = g_get_num_processors ();
  header->first_offset = CELLS_PER_HEADER;

  EGG_MEMORY_BARRIER;

  header->size = (guint32)arena->data_length;

  return;

failure:
  shm_unlink (name);
  close (fd);

use_malloc:
  g_warning ("Failed to allocate shared memory for counters. "
             "Counters will not be available to external processes.");

  arena->data_is_mmapped = FALSE;
  arena->cells = g_malloc0 (size << 1);
  arena->n_cells = (size / DATA_CELL_SIZE);
  arena->data_length = size;

  /*
   * Make sure that we have a properly aligned allocation back from
   * malloc. Since we are at least a page size, we should pretty much
   * be guaranteed this, but better to check with posix_memalign().
   */
  if (posix_memalign ((void *)&arena->cells, page_size, size << 1) != 0)
    {
      perror ("posix_memalign()");
      abort ();
    }

  header = (void *)arena->cells;
  header->magic = MAGIC;
  header->ncpu = g_get_num_processors ();
  header->first_offset = CELLS_PER_HEADER;

  EGG_MEMORY_BARRIER;

  header->size = (guint32)arena->data_length;
}

static gboolean
_egg_counter_arena_init_remote (EggCounterArena *arena,
                                GPid             pid)
{
  ShmHeader header;
  gssize count;
  gchar name [32];
  void *mem = NULL;
  guint ncpu;
  guint n_counters;
  int i;
  int fd = -1;

  g_assert (arena != NULL);

  ncpu = g_get_num_processors ();

  arena->ref_count = 1;
  arena->pid = pid;

  g_snprintf (name, sizeof name, NAME_FORMAT, (int)pid);

  fd = shm_open (name, O_RDONLY, 0);
  if (fd < 0)
    return FALSE;

  count = pread (fd, &header, sizeof header, 0);

  if ((count != sizeof header) ||
      (header.magic != MAGIC) ||
      (header.size > COUNTER_MAX_SHM) ||
      (header.ncpu > g_get_num_processors ()))
    goto failure;

  n_counters = header.n_counters;

  if (header.size <
      CELLS_PER_HEADER + (((n_counters / COUNTERS_PER_GROUP) + 1) * CELLS_PER_GROUP(header.ncpu)))
    goto failure;

  mem = mmap (NULL, header.size, PROT_READ, MAP_SHARED, fd, 0);

  if (mem == MAP_FAILED)
    goto failure;

  arena->is_local_arena = FALSE;
  arena->data_is_mmapped = TRUE;
  arena->cells = mem;
  arena->n_cells = header.size / DATA_CELL_SIZE;
  arena->data_length = header.size;
  arena->counters = NULL;

  /* Not strictly required, but helpful for now */
  if (header.first_offset != CELLS_PER_HEADER)
    goto failure;

  for (i = 0; i < n_counters; i++)
    {
      CounterInfo *info;
      EggCounter *counter;
      guint group_start_cell;
      guint group;
      guint position;

      group = i / COUNTERS_PER_GROUP;
      position = i % COUNTERS_PER_GROUP;
      group_start_cell = header.first_offset + (CELLS_PER_GROUP (ncpu) * group);

      if (group_start_cell + CELLS_PER_GROUP (ncpu) >= arena->n_cells)
        goto failure;

      info = &(((CounterInfo *)&arena->cells[group_start_cell])[position]);

      counter = g_new0 (EggCounter, 1);
      counter->category = g_strndup (info->category, sizeof info->category);
      counter->name = g_strndup (info->name, sizeof info->name);
      counter->description = g_strndup (info->description, sizeof info->description);
      counter->values = (EggCounterValue *)&arena->cells [info->cell].values[info->position];

#if 0
      g_print ("Counter discovered: cell=%u position=%u category=%s name=%s values=%p offset=%lu\n",
               info->cell, info->position, info->category, info->name, counter->values,
               (guint8*)counter->values - (guint8*)mem);
#endif

      arena->counters = g_list_prepend (arena->counters, counter);
    }

  close (fd);

  return TRUE;

failure:
  close (fd);

  if ((mem != NULL) && (mem != MAP_FAILED))
    munmap (mem, header.size);

  return FALSE;
}

EggCounterArena *
egg_counter_arena_new_for_pid (GPid pid)
{
  EggCounterArena *arena;

  arena = g_new0 (EggCounterArena, 1);

  if (!_egg_counter_arena_init_remote (arena, pid))
    {
      g_free (arena);
      return NULL;
    }

  return arena;
}

static void
_egg_counter_arena_destroy (EggCounterArena *arena)
{
  g_assert (arena != NULL);

  if (arena->data_is_mmapped)
    munmap (arena->cells, arena->data_length);
  else
    g_free (arena->cells);

  g_clear_pointer (&arena->counters, g_list_free);

  arena->cells = NULL;

  if (arena->arena_is_malloced)
    g_free (arena);
}

EggCounterArena *
egg_counter_arena_get_default (void)
{
  static EggCounterArena instance;
  static gsize initialized;

  if (G_UNLIKELY (g_once_init_enter (&initialized)))
    {
      _egg_counter_arena_init_local (&instance);
      g_once_init_leave (&initialized, 1);
    }

  return &instance;
}

EggCounterArena *
egg_counter_arena_ref (EggCounterArena *arena)
{
  g_return_val_if_fail (arena, NULL);
  g_return_val_if_fail (arena->ref_count > 0, NULL);

  g_atomic_int_inc (&arena->ref_count);

  return arena;
}

void
egg_counter_arena_unref (EggCounterArena *arena)
{
  g_return_if_fail (arena);
  g_return_if_fail (arena->ref_count);

  if (g_atomic_int_dec_and_test (&arena->ref_count))
    _egg_counter_arena_destroy (arena);
}

void
egg_counter_arena_foreach (EggCounterArena       *arena,
                           EggCounterForeachFunc  func,
                           gpointer               user_data)
{
  GList *iter;

  g_return_if_fail (arena != NULL);
  g_return_if_fail (func != NULL);

  for (iter = arena->counters; iter; iter = iter->next)
    func (iter->data, user_data);
}

void
egg_counter_arena_register (EggCounterArena *arena,
                            EggCounter      *counter)
{
  CounterInfo *info;
  guint group;
  guint ncpu;
  guint position;
  guint group_start_cell;

  g_return_if_fail (arena != NULL);
  g_return_if_fail (counter != NULL);

  if (!arena->is_local_arena)
    {
      g_warning ("Cannot add counters to a remote arena.");
      return;
    }

  ncpu = g_get_num_processors ();

  G_LOCK (reglock);

  /*
   * Get the counter group and position within the group of the counter.
   */
  group = arena->n_counters / COUNTERS_PER_GROUP;
  position = arena->n_counters % COUNTERS_PER_GROUP;

  /*
   * Get the starting cell for this group. Cells roughly map to cachelines.
   */
  group_start_cell = CELLS_PER_HEADER + (CELLS_PER_GROUP (ncpu) * group);
  info = &((CounterInfo *)&arena->cells [group_start_cell])[position];

  g_assert (position < COUNTERS_PER_GROUP);
  g_assert (group_start_cell < arena->n_cells);

  /*
   * Store information about the counter in the SHM area. Also, update
   * the counter values pointer to map to the right cell in the SHM zone.
   */
  info->cell = group_start_cell + (COUNTERS_PER_GROUP * CELLS_PER_INFO);
  info->position = position;
  g_snprintf (info->category, sizeof info->category, "%s", counter->category);
  g_snprintf (info->description, sizeof info->description, "%s", counter->description);
  g_snprintf (info->name, sizeof info->name, "%s", counter->name);
  counter->values = (EggCounterValue *)&arena->cells [info->cell].values[info->position];

#if 0
  g_print ("Counter registered: cell=%u position=%u category=%s name=%s\n",
           info->cell, info->position, info->category, info->name);
#endif

  /*
   * Track the counter address, so we can _foreach() them.
   */
  arena->counters = g_list_append (arena->counters, counter);
  arena->n_counters++;

  /*
   * Now notify remote processes of the counter.
   */
  EGG_MEMORY_BARRIER;
  ((ShmHeader *)&arena->cells[0])->n_counters++;

  G_UNLOCK (reglock);
}

#ifdef __linux__
static void *
_egg_counter_find_getcpu_in_vdso (void)
{
  static const gchar *vdso_names[] = {
    "linux-vdso.so.1",
    "linux-vdso32.so.1",
    "linux-vdso64.so.1",
    NULL
  };
  static const gchar *sym_names[] = {
    "__kernel_getcpu",
    "__vdso_getcpu",
    NULL
  };
  gint i;

  for (i = 0; vdso_names [i]; i++)
    {
      void *lib;
      gint j;

      lib = dlopen (vdso_names [i], RTLD_NOW | RTLD_GLOBAL);
      if (lib == NULL)
        continue;

      for (j = 0; sym_names [j]; j++)
        {
          void *sym;

          sym = dlsym (lib, sym_names [j]);
          if (!sym)
            continue;

          return sym;
        }

      dlclose (lib);
    }

  return NULL;
}

static guint (*_egg_counter_getcpu_vdso_raw) (int *cpu,
                                              int *node,
                                              void *tcache);

static guint
_egg_counter_getcpu_vdso_helper (void)
{
  int cpu;
  _egg_counter_getcpu_vdso_raw (&cpu, NULL, NULL);
  return cpu;
}
#endif

static guint
_egg_counter_getcpu_fallback (void)
{
  return 0;
}

#ifdef EGG_HAVE_RDTSCP
static guint
_egg_counter_getcpu_rdtscp (void)
{
  return egg_get_current_cpu_rdtscp ();
}
#endif

static void
_egg_counter_init_getcpu (void)
{

#ifdef EGG_HAVE_RDTSCP
  _egg_counter_getcpu_helper = _egg_counter_getcpu_rdtscp;
#endif

#ifdef __linux__
  _egg_counter_getcpu_vdso_raw = _egg_counter_find_getcpu_in_vdso ();

  if (_egg_counter_getcpu_vdso_raw)
    {
      _egg_counter_getcpu_helper = _egg_counter_getcpu_vdso_helper;
      return;
    }
#endif

#ifdef HAVE_SCHED_GETCPU
  _egg_counter_getcpu_helper = (guint (*) (void))sched_getcpu;
#endif

  _egg_counter_getcpu_helper = _egg_counter_getcpu_fallback;
}

guint
egg_get_current_cpu_call (void)
{
  return _egg_counter_getcpu_helper ();
}
