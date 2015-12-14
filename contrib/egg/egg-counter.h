/* egg-counter.h
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
 *
 * Additionally, this file does not claim copyright over the expansion
 * of macros in your source program.
 */

#ifndef EGG_COUNTER_H
#define EGG_COUNTER_H

#include <glib-object.h>

/*
 * History
 * =======
 *
 * EggCounter is a performance counter based on ideas from previous work
 * on high performance counters. They are not guaranteed to be 100%
 * correct, but they approach that with no synchronization given new
 * enough hardware. In particular, we use %ecx from rdtscp (the core id)
 * to determine which cachline to increment the counter within.
 *
 * Given a counter, the value will be split up int NCPU cachelines where
 * NCPU is the number of cores returned from get_nprocs() (on Linux).
 *
 * Updating the counter is very cheap, reading back the counter requires
 * a volatile read of each cacheline. Again, no correctness is guaranteed.
 *
 * In practice, very few values are lost even during tight competing loops.
 * A loss can happen when the thread is pre-empted between the %rdtscp
 * instruction and the addq increment (on x86_64).
 *
 *
 * Using EggCounter
 * ================
 *
 * To define a counter, you must have support for constructor attributes.
 *
 *   EGG_DEFINE_COUNTER (Symbol, "Category", "Name", "Description")
 *
 * To increment the counter in a function of your choice (but within the
 * same module), use EGG_COUNTER_ADD, EGG_COUNTER_INC, EGG_COUNTER_DEC.
 *
 *   EGG_COUNTER_ADD (Symbol);
 *
 *
 * Architecture Support
 * ====================
 *
 * If you are not on x86_64, or are missing the rdtscp instruction, a 64-bit
 * atomic will be performed using __sync_fetch_and_add8(). Clearly, we can
 * do some more work here to abstract which implementation is used, but we
 * only support GCC and Clang today, which both have that intrinsic. Some
 * architectures may not have it (such as 32-bit PPC), but I'm not too
 * concerned about that at the moment.
 *
 * The counters are mapped into a shared memory zone using shm_open() and
 * mmap(). An external program can then discover the available counters
 * and print them without blocking the target program. It simply must
 * perform the reads in a volatile manner just like the target process
 * would need to do for readback.
 *
 * EggCounterArena provides a helper to walk through the counters in the
 * shared memory zone. egg_counter_arena_foreach().
 *
 * You cannot remove a counter once it has been registered.
 *
 *
 * Accessing Counters Remotely
 * ===========================
 *
 * You can access the counters from out of process. By opening the SHM zone
 * and reading the contents from each cachline, you can get the approximate
 * state of the target application without blocking it.
 *
 * EggCounterArena provides a helper for you to do this.
 *
 *   EggCounterArena *arena;
 *
 *   arena = egg_counter_arena_new_for_pid (other_process_pid);
 *   egg_counter_arena_foreach (arena, my_counter_callback, user_data);
 *
 *
 * Data Layout
 * ===========
 *
 * The layout of the shared memory zone is broken into "cells". Each cell
 * is an approximate cacheline (64-bytes) on modern Intel hardware. Indexes
 * to data locations are represented in cells to simplify the math and
 * allow the compiler to know we are working with properly aligned structures.
 *
 * The base pointer in EggCounter.values is not 64-byte aligned! It is 8-byte
 * aligned and points to the offset within the cacheline for that counter.
 * We pack 8 64-bit counters into a single cacheline. This allows us to avoid
 * an extra MOV instruction when incrementing since we only need to perform
 * the offset from the base pointer.
 *
 * The first two cells are the header which contain information about the
 * underlying shm file and how large the mmap() range should be.
 *
 * After that, begin the counters.
 *
 * The counters are layed out in groups of 8 counters.
 *
 *  [8 CounterInfo Structs (128-bytes each)][N_CPU Data Zones (64-byte each)]
 *
 * See egg-counter.c for more information on the contents of these structures.
 *
 *
 * Build System Requirements
 * =========================
 *
 * We need to know if rdtscp is available at compile time. In an effort
 * to keep the headers as portable as possible (if that matters here?) we
 * require that you define EGG_HAVE_RDTSCP if the instruction is supported.
 *
 * An example for autoconf might be similar to the following:
 *
 *   AC_MSG_CHECKING([for fast counters with rdtscp])
 *   AC_RUN_IFELSE(
 *     [AC_LANG_SOURCE([[
 *      #include <x86intrin.h>
 *      int main (int argc, char *argv[]) { int cpu; __builtin_ia32_rdtscp (&cpu); return 0; }]])],
 *     [have_rdtscp=yes],
 *     [have_rdtscp=no])
 *   AC_MSG_RESULT([$have_rdtscp])
 *   AS_IF([test "$have_rdtscp" = "yes"],
 *         [CFLAGS="$CFLAGS -DEGG_HAVE_RDTSCP"])
 */

G_BEGIN_DECLS

#ifdef EGG_HAVE_RDTSCP
# include <x86intrin.h>
  static inline guint
  egg_get_current_cpu_rdtscp (void)
  {
    /*
     * This extracts the IA32_TSC_AUX into the ecx register. On Linux,
     * that value contains a value with the bottom 12 bits being the
     * cpu identifier, and the next 10 bits being the node group.
     */
    guint aux;
    __builtin_ia32_rdtscp (&aux);
    return aux & 0xFFF;
  }
# define egg_get_current_cpu() egg_get_current_cpu_rdtscp()
#elif defined(__linux__)
# define egg_get_current_cpu() egg_get_current_cpu_call()
#else
# define egg_get_current_cpu() 0
# define EGG_COUNTER_REQUIRES_ATOMIC 1
#endif

/**
 * EGG_DEFINE_COUNTER:
 * @Identifier: The symbol name of the counter
 * @Category: A string category for the counter.
 * @Name: A string name for the counter.
 * @Description: A string description for the counter.
 *
 * |[<!-- language="C" -->
 * EGG_DEFINE_COUNTER (my_counter, "My", "Counter", "My Counter Description");
 * ]|
 */
#define EGG_DEFINE_COUNTER(Identifier, Category, Name, Description)                 \
 static EggCounter Identifier##_ctr = { NULL, Category, Name, Description };        \
 static void Identifier##_ctr_init (void) __attribute__((constructor));             \
 static void                                                                        \
 Identifier##_ctr_init (void)                                                       \
 {                                                                                  \
   egg_counter_arena_register (egg_counter_arena_get_default(), &Identifier##_ctr); \
 }

/**
 * EGG_COUNTER_INC:
 * @Identifier: The identifier of the counter.
 *
 * Increments the counter @Identifier by 1.
 */
#define EGG_COUNTER_INC(Identifier) EGG_COUNTER_ADD(Identifier, G_GINT64_CONSTANT(1))

/**
 * EGG_COUNTER_DEC:
 * @Identifier: The identifier of the counter.
 *
 * Decrements the counter @Identifier by 1.
 */
#define EGG_COUNTER_DEC(Identifier) EGG_COUNTER_SUB(Identifier, G_GINT64_CONSTANT(1))

/**
 * EGG_COUNTER_SUB:
 * @Identifier: The identifier of the counter.
 * @Count: the amount to subtract.
 *
 * Subtracts from the counter identified by @Identifier by @Count.
 */
#define EGG_COUNTER_SUB(Identifier, Count) EGG_COUNTER_ADD(Identifier, (-(Count)))

/**
 * EGG_COUNTER_ADD:
 * @Identifier: The identifier of the counter.
 * @Count: the amount to add to the counter.
 *
 * Adds @Count to @Identifier.
 *
 * This operation is not guaranteed to have full correctness. It tries to find
 * a happy medium between fast, and accurate. When possible, the %rdtscp
 * instruction is used to get a cacheline owned by the executing CPU, to avoid
 * collisions. However, this is not guaranteed as the thread could be swapped
 * between the calls to %rdtscp and %addq (on 64-bit Intel).
 *
 * Other platforms have fallbacks which may give different guarantees, such as
 * using atomic operations (and therefore, memory barriers).
 *
 * See #EggCounter for more information.
 */
#ifdef EGG_COUNTER_REQUIRES_ATOMIC
# define EGG_COUNTER_ADD(Identifier, Count)                                          \
  G_STMT_START {                                                                     \
    __sync_add_and_fetch ((gint64 *)&Identifier##_ctr.values[0], ((gint64)(Count))); \
  } G_STMT_END
#else
# define EGG_COUNTER_ADD(Identifier, Count)                                    \
  G_STMT_START {                                                               \
    Identifier##_ctr.values[egg_get_current_cpu()].value += ((gint64)(Count)); \
  } G_STMT_END
#endif

typedef struct _EggCounter      EggCounter;
typedef struct _EggCounterArena EggCounterArena;
typedef struct _EggCounterValue EggCounterValue;

/**
 * EggCounterForeachFunc:
 * @counter: the counter.
 * @user_data: data supplied to egg_counter_arena_foreach().
 *
 * Function prototype for callbacks provided to egg_counter_arena_foreach().
 */
typedef void (*EggCounterForeachFunc) (EggCounter *counter,
                                       gpointer    user_data);

struct _EggCounter
{
  /*< Private >*/
  EggCounterValue *values;
  const gchar     *category;
  const gchar     *name;
  const gchar     *description;
} __attribute__ ((aligned(8)));

struct _EggCounterValue
{
  volatile gint64 value;
  gint64          padding [7];
} __attribute__ ((aligned(8)));

GType            egg_counter_arena_get_type     (void);
guint            egg_get_current_cpu_call       (void);
EggCounterArena *egg_counter_arena_get_default  (void);
EggCounterArena *egg_counter_arena_new_for_pid  (GPid                   pid);
EggCounterArena *egg_counter_arena_ref          (EggCounterArena       *arena);
void             egg_counter_arena_unref        (EggCounterArena       *arena);
void             egg_counter_arena_register     (EggCounterArena       *arena,
                                                 EggCounter            *counter);
void             egg_counter_arena_foreach      (EggCounterArena       *arena,
                                                 EggCounterForeachFunc  func,
                                                 gpointer               user_data);
void             egg_counter_reset              (EggCounter            *counter);
gint64           egg_counter_get                (EggCounter            *counter);

G_END_DECLS

#endif /* EGG_COUNTER_H */
