# Application Components

This directory contains the components that manage the application at runtime.

## ide-application.*

The IdeApplication is our primary component. Almost every Gtk application
has one. Nothing special. However, it also manages a bunch of application-wide
moving parts depending on the mode it is in.

The IdeApplication can run in one of three modes:

 * UI Application
 * Worker process (running plugin code)
 * CLI (the command line tool)
 * Unit test mode (used by our unit tests for machinery)

It also manages setting up theme helpers, skeleton directories, default
settings, and a bunch of other stuff.

## ide-application-addin.*

This interface is used by plugins to hook into the application once-per UI
process. They will be loaded at startup, and shutdown with the application.

## ide-application-command-line.c

This file handles the machinery when we are in command-line mode. This includes
finding the proper plugin, and parsing arguments for the user.

## ide-application-tests.c

When running our unit tests from the toplevel tests/ directory, we need to
have a mostly working IdeApplication instance. This helps make that work
without running the whole program.

## ide-application-credits.h

Various contributors to the project. Add yourself here in the proper place.

## ide-application-actions.c

This file contains the implementation of various actions available with the
"app." action prefix.
