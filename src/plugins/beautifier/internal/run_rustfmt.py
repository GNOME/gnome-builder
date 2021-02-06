#!/bin/env python3

import os
import sys
import pathlib
import subprocess

# That's a terrible way to fix https://github.com/rust-lang/rustfmt/issues/4660
# This script wouldn't be needed at all if this bug wasn't a thing, and we could have called rustfmt directly from the config.ini

cwd = pathlib.Path(os.getcwd()).resolve()

cargo_home = pathlib.Path(os.environ['CARGO_HOME'] if 'CARGO_HOME' in os.environ else os.path.expanduser("~/.cargo"))

command = [cargo_home.joinpath('bin').joinpath('rustfmt'), '--quiet', '--emit', 'stdout']
while True:
    if cwd.joinpath(".rustfmt.toml").exists() or cwd.joinpath("rustfmt.toml").exists():
        command.append('--config-path=' + str(cwd))
        break
    cwd = cwd.parent
    if str(cwd) == str(cwd.anchor):
        break

command.append(sys.argv[1])

runned_process = subprocess.run(command, capture_output=True, text=True)

print(runned_process.stderr, file=sys.stderr)
# Because print() will always print at least a new line, don't
# output anything on stdout if rustfmt errored, otherwise the
# beautifier plugin will replace the whole selected text with a newline
if runned_process.returncode == 0:
    print(runned_process.stdout)

# propagate return code so it can know if rustfmt couldn't format
# the code, and hence ignore the outputted code and show the error message to the user
exit(runned_process.returncode)
