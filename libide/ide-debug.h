/* ide-debug.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#ifndef IDE_DEBUG_H
#define IDE_DEBUG_H

#include <glib.h>

G_BEGIN_DECLS

#ifndef G_LOG_LEVEL_TRACE
# define G_LOG_LEVEL_TRACE (1 << G_LOG_LEVEL_USER_SHIFT)
#endif

#ifndef IDE_DISABLE_DEBUG
# define IDE_DEBUG(...)                                                \
   g_log(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, ##__VA_ARGS__)
#else
# define IDE_DEBUG(...)
#endif

#ifndef IDE_DISABLE_TRACE
# define IDE_TRACE_MSG(fmt, ...)                                       \
   g_log(G_LOG_DOMAIN, G_LOG_LEVEL_TRACE, "  MSG: %s():%d: "fmt,       \
         G_STRFUNC, __LINE__, ##__VA_ARGS__)
# define IDE_PROBE                                                     \
   g_log(G_LOG_DOMAIN, G_LOG_LEVEL_TRACE, "PROBE: %s():%d",            \
         G_STRFUNC, __LINE__)
# define IDE_TODO(_msg)                                                \
   g_log(G_LOG_DOMAIN, G_LOG_LEVEL_TRACE, " TODO: %s():%d: %s",        \
         G_STRFUNC, __LINE__, _msg)
# define IDE_ENTRY                                                     \
   g_log(G_LOG_DOMAIN, G_LOG_LEVEL_TRACE, "ENTRY: %s():%d",            \
         G_STRFUNC, __LINE__)
# define IDE_EXIT                                                      \
   G_STMT_START {                                                      \
      g_log(G_LOG_DOMAIN, G_LOG_LEVEL_TRACE, " EXIT: %s():%d",         \
            G_STRFUNC, __LINE__);                                      \
      return;                                                          \
   } G_STMT_END
# define IDE_GOTO(_l)                                                  \
   G_STMT_START {                                                      \
      g_log(G_LOG_DOMAIN, G_LOG_LEVEL_TRACE, " GOTO: %s():%d ("#_l")", \
            G_STRFUNC, __LINE__);                                      \
      goto _l;                                                         \
   } G_STMT_END
# define IDE_RETURN(_r)                                                \
   G_STMT_START {                                                      \
      g_log(G_LOG_DOMAIN, G_LOG_LEVEL_TRACE, " EXIT: %s():%d ",        \
            G_STRFUNC, __LINE__);                                      \
      return _r;                                                       \
   } G_STMT_END
#else
# define IDE_TODO(_msg)
# define IDE_PROBE
# define IDE_TRACE_MSG(fmt, ...)
# define IDE_ENTRY
# define IDE_GOTO(_l)   goto _l
# define IDE_EXIT       return
# define IDE_RETURN(_r) return _r
#endif

G_END_DECLS

#endif /* IDE_DEBUG_H */
