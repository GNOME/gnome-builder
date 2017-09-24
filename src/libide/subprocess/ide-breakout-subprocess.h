/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright © 2012, 2013, 2016 Red Hat, Inc.
 * Copyright © 2012, 2013 Canonical Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 *
 * Authors: Christian Hergert <chergert@redhat.com>
 */

#pragma once

#include "subprocess/ide-subprocess.h"

G_BEGIN_DECLS

#define IDE_TYPE_BREAKOUT_SUBPROCESS (ide_breakout_subprocess_get_type())

G_DECLARE_FINAL_TYPE (IdeBreakoutSubprocess, ide_breakout_subprocess, IDE, BREAKOUT_SUBPROCESS, GObject)

G_END_DECLS
