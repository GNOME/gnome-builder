# Builder

Develop software for GNOME

## Helpful Links

 * [Read a Book on Contributing to Builder (PDF)](https://gitlab.gnome.org/chergert/builder-a-developers-notebook/-/raw/main/builder-a-developers-notebook.pdf?ref_type=heads)
 * [Read the documentation](https://builder.readthedocs.io/)
 * [API Reference](https://devsuite.app/docs/libide/)
 * [File a Bug in GitLab](https://gitlab.gnome.org/GNOME/gnome-builder/issues)
 * [Download a Release Tarball](https://download.gnome.org/sources/gnome-builder/)
 * [Browse source code in Git version control](https://gitlab.gnome.org/GNOME/gnome-builder)
 * [Learn more about Builder](https://apps.gnome.org/Builder)
 * [Chat with the developers](irc://irc.gnome.org/#gnome-builder) in #gnome-builder on irc.gnome.org

----

Builder aims to be an IDE for writing GNOME-based software.
We believe this focus will help us build something great for our community.

If you would like to help in this effort, join our IRC channel and we will help you find something to work on.

Builder is primarily written in C and Python.
Some aspects of Builder may be written in another language when it makes sense.

Builder is developed in conjunction with GNOME releases.
This means that we often contribute to, and rely on, features being developed in other GNOME modules such as Gtk.

----

Builder is built using meson:

```sh
meson --prefix=/usr build
ninja -C build
sudo ninja -C build install
```

For more information on building and installing Builder,
[read our installation guide](https://builder.readthedocs.io/installation.html)
to help you through the process.

----

Builder is licensed under the GNU General Public License as published by the
Free Software Foundation, either version 3 of the License, or (at your option)
any later version. Some files are individually licensed under alternative
licenses such as LGPL-2.1+ and LGPL-3.0.

## Supported Language Servers

Builder comes with support for a number of language servers builtin. It
automatically locates the language server within your build environment and
runs it there (possibly in a container). If it can find it elsewhere (such
as on the host) that will be used as a fallback.

 * bash-language-server (Bash)
 * blueprint (Blueprint)
 * clangd (C, C++, Objective-C, Objective-C++)
 * glsl-language-server (GLSL)
 * gopls (Go)
 * intelephense (PHP)
 * jdtls (Java)
 * jedi-language-server (Python)
 * lua-language-server (Lua)
 * serve-d (D)
 * python-lsp-server (Python)
 * rust-analyzer (Rust)
 * ts-language-server (Javascript, Typescript)
 * vala-language-server (Vala)
 * zls (Zig)

More are being added all the time and do not require writing code if you'd
like to add support. See `src/plugins/` for examples of language server
integration.

