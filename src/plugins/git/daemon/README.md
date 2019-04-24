# gnome-builder-git

This is a GPLv2.0+ licensed daemon that wraps libgit2-glib and libgit2.
It provides some of the services that are necessary for an IDE using Git.

The design is similar to Language Server Protocol, in that RPCs are performed over stdin/stdout to a subprocess daemon.
However, we use the D-Bus serialization format instead of JSON/JSONRPC for various reasons including efficiency and design.
For example, we can extend our implementation to trivially include FD passing for future extensions.
Unlike JSONRPC, we have gdbus-codegen to generate the IPC stubs giving us a clean, minimal daemon implementation and convenient APIs for the consumer.
All of the RPCs are defined in the various D-Bus interface description XML files.
