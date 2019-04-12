# language server client for java

Wrap [java-language-server](https://github.com/georgewfraser/java-language-server).

## Installing and testing

1. Install `java-language-server`

```
git clone https://github.com/georgewfraser/java-language-server
cd java-language-server
JAVA_HOME=/path/to/jdk11 scripts/link_mac.sh
```

2. Set up your environment

Create a launcher in /usr/bin

```
$ cat > /usr/bin/javals <<EOF
#!/bin/bash

/path/to/java-language-server/dist/mac/bin/launcher $@
```

Alternatively create a script in a custom path with `JAVALS_CMD` env:

```
export JAVALS_CMD="/custom/path/to/javals-script"
```

3. Launch Builder, open a java project
4. Right click on a method and click 'go to definition'

## Bugs:

Please report on IRC or gitlab
