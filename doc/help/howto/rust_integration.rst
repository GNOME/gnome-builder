################
Rust integration
################

First install `rustup`. Use the installer in `Preferencees->SDKs` or the instructions at rustup_.
This will install all the components into `~/.cargo`.
Then go to `Preferencees->SDKs->Rustup Toolchains` and press `+` to add a new Rust channel and use `nightly` as the name.

After that, open a terminal and install the Rust language-server:

.. code-block:: bash

    $ ~/.cargo/bin/rustup component add rls-preview --toolchain nightly
    $ ~/.cargo/bin/rustup component add rust-analysis --toolchain nightly
    $ ~/.cargo/bin/rustup component add rust-src --toolchain nightly

These instructions are taken from the rls-repo_, where you can check for up-to-date instructions.

.. _rustup: https://www.rustup.rs/
.. _rls-repo: https://github.com/rust-lang-nursery/rls
