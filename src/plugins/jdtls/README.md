You can get jdtls from somewhere like https://download.eclipse.org/jdtls/milestones/1.9.0/

Extract it and symlink to it from somewhere in your $PATH like:

 ln -s ~/Downloads/jdt-language-server-1.9.0-202203031534/bin/jdtls ~/.local/bin/jdtls

Then Builder (assuming `.local/bin` is in your $PATH) will find it and spawn it for .java files for you.
