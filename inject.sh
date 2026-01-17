#!/bin/bash

if [[ ! $(id -u) -eq 0 ]]; then
    echo "This script must be ran as root!"
    exit 1
fi

pid=$(pidof -s "tf_linux64")
libpath=$(realpath "$PWD/b/libtf2aeth.so")


if [[ ! -f "$libpath" ]]; then
    echo "ERROR: inject.sh: Compile cheat first!"
    exit 1
fi

if [[ -z "$pid" ]] || [[ "$pid" == "" ]]; then
   echo "ERROR: inject.sh: process not running."
   exit 1
fi

old_tmp_libpath_txt="/tmp/tf_linux64_${pid}_aeth.txt"
if [[ -f "$old_tmp_libpath_txt" ]]; then
    old_tmp_libpath=$(head -n 1 "$old_tmp_libpath_txt")
    if [[ -z "$old_tmp_libpath" && "$old_tmp_libpath" != "" ]]; then
        echo "inject.sh: old_tmp_libpath: $old_tmp_libpath"
    fi
fi

tmp_libpath=""
if [[ "$1" == "unload" ]] && [[ ! -z "$2" ]]; then
    tmp_libpath="$2"
else
    tmp_libpath="/tmp/libgl"
    tmp_libpath+=$(head /dev/urandom | tr -dc 'a-z0-9' | head -c 4)
    tmp_libpath+=".so"
    cp -p "$libpath" "$tmp_libpath"
fi
echo "inject.sh: Our cheat library is located at $tmp_libpath"

if [[ "$1" == "unload" ]] && [[ ! -z "$tmp_libpath" ]]; then
    gdb -n -q -batch                                        \
         -ex "attach $pid"                                  \
         -ex "set \$dlopen = (void* (*)(char*, int))dlopen" \
         -ex "set \$dlclose = (int (*)(void*))dlclose"      \
         -ex "set \$dlerror =  (char* (*)(void))dlerror"    \
                                                            \
         -ex "set \$self = \$dlopen(\"$tmp_libpath\", 6)"   \
         -ex "call \$dlclose(\$self)"                       \
         -ex "call \$dlclose(\$self)"                       \
                                                            \
         -ex "call \$dlerror()"                             \
         -ex "detach"                                       \
         -ex "quit"
    rm "$old_tmp_libpath_txt"
elif [[ "$1" == "debug" ]]; then
   echo "Launching in debug mode."
   gdb -n -q                                               \
        -ex "attach $pid"                                  \
        -ex "set \$dlopen = (void* (*)(char*, int))dlopen" \
        -ex "set \$dlerror =  (char* (*)(void))dlerror"    \
        -ex "call \$dlopen(\"$tmp_libpath\", 2)"           \
        -ex "call \$dlerror()"                             \
        -ex "continue"

    echo -e "\locked and loaded. Unload cheat using:"
    echo -e "$0 unload $tmp_libpath"

    rm "$tmp_libpath"
    echo "$tmp_libpath" > "$old_tmp_libpath_txt"
else
    if [[ ! -z "$old_tmp_libpath" ]] && [[ "$old_tmp_libpath" != "" ]] && grep -q "$old_tmp_libpath" "/proc/$pid/maps"; then
        echo -e "already loaded. Reloading...\n";
        gdb -n -q -batch                                         \
            -ex "attach $pid"                                    \
            -ex "set \$dlopen = (void* (*)(char*, int))dlopen"   \
            -ex "set \$dlclose = (int (*)(void*))dlclose"        \
            -ex "set \$dlerror =  (char* (*)(void))dlerror"      \
                                                                 \
            -ex "set \$self = \$dlopen(\"$old_tmp_libpath\", 6)" \
            -ex "call \$dlclose(\$self)"                         \
            -ex "call \$dlclose(\$self)"                         \
                                                                 \
            -ex "call \$dlopen(\"$tmp_libpath\", 2)"             \
            -ex "call \$dlerror()"                               \
            -ex "detach"                                         \
            -ex "quit"
    else
        gdb -n -q -batch                                       \
            -ex "attach $pid"                                  \
            -ex "set \$dlopen = (void* (*)(char*, int))dlopen" \
            -ex "set \$dlerror =  (char* (*)(void))dlerror"    \
            -ex "call \$dlopen(\"$tmp_libpath\", 2)"           \
            -ex "call \$dlerror()"                             \
            -ex "detach"                                       \
            -ex "quit"
    fi

    echo -e "\locked and loaded. Unload cheat using:"
    echo -e "$0 unload $tmp_libpath"

    rm "$tmp_libpath"
    echo "$tmp_libpath" > "$old_tmp_libpath_txt"
fi

set +x
