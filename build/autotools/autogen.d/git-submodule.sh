if [ -d .git ]; then
    git submodule init
    git submodule update
fi
