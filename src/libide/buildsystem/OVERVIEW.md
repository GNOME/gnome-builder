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

Manages all configurations for the project, which can be provided by plugins
that implement the IdeConfigurationProvider interface.

## ide-configuration.c

An individual configuration that has information about how the project should
be built, such as what options to pass to configure and which runtime to use.

## ide-configuration-provider.*

This is the interface used for loading and unloading configurations into the
IdeConfigurationManager. The flatpak plugin is one place it's implemented.

## ide-environment.c

Manages a collection of key/value pairs (IdeEnvironmentVariable). This is a
GListModel to make writing environment editors easier.

## ide-environment-variable.c

A single variable within the environment.

## ide-build-manager.c

The BuildManager provides a convenient place to manage a single build process
for the context. Many build systems do not allow concurrent builds, so this
is a good way to ensure that two plugins do not race to use the build system.
