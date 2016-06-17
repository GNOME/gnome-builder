# Build Systems

Various build system machinery.

## ide-builder.c

This object manages a single build request. It contains things like a
configuration and environment, which can be used to setup the builder.

## ide-build-result-addin.c

This interface can be used by plugins to attach to an IdeBuildResult. They
might use the log signal to extract information from the build.

## ide-build-result.c

This represents a build result. It is created when the build starts and
the builder should append information to it as necessary.

## ide-build-system.c

The core interface for the build system integration. This needs to create new
IdeBuilder instances, help get file flags (such as CFLAGS for C files) and
other high-level operations.

## ide-configuration-manager.c

Manages all configurations for the project, and reads/writes the .buildconfig
file.

## ide-configuration.c

An individual configuration that was loaded/persisted to the .buildconfig file.

## ide-environment.c

Manages a collection of key/value pairs (IdeEnvironmentVariable). This is a
GListModel to make writing environment editors easier.

## ide-environment-variable.c

A single variable within the environment.
