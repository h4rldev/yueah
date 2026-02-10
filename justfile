set shell := ["bash", "-c"]
set quiet := true

src_dir := 'src'
out_dir := 'out'
bin_dir := 'bin'
lib_dir := 'lib'
include_dir := 'include'
h2o_include := lib_dir + '/include'
link_flags := '-lyyjson -lh2o -lssl -lcrypto -lz -luv -lm -lbrotlidec -lbrotlienc -lbsd -lsqlite3 -lsodium'
debug_compile_flags := '-ggdb -fsanitize=address -Wall -Wextra -pedantic -Wno-unused-parameter -Wno-variadic-macros -DYUEAH_DEBUG'
debug_link_flags := '-ggdb -fsanitize=address -static-libasan'
release_compile_flags := '-O2 -flto'
release_link_flags := '-O2 -flto'

default:
    just --list

ensure_h2o:
    #!/usr/bin/env bash
    TEMP=$(mktemp -d)

    [[ -d {{ lib_dir }} ]] || mkdir -p {{ lib_dir }}
    [[ -d {{ h2o_include }} ]] || mkdir -p {{ h2o_include }}
    git clone https://github.com/h2o/h2o.git $TEMP

    [[ -f {{ h2o_include }}/h2o.h ]] || cp $TEMP/include/h2o.h {{ h2o_include }}
    [[ -f {{ h2o_include }}/picotls.h ]] || cp $TEMP/deps/picotls/include/picotls.h {{ h2o_include }}
    [[ -f {{ h2o_include }}/quicly.h ]] || cp $TEMP/deps/quicly/include/quicly.h {{ h2o_include }}

    [[ -d {{ h2o_include }}/h2o ]] || cp -r $TEMP/include/h2o {{ h2o_include }}
    [[ -d {{ h2o_include }}/picotls ]] || cp -r $TEMP/deps/picotls/include/picotls {{ h2o_include }}
    [[ -d {{ h2o_include }}/quicly ]] || cp -r $TEMP/deps/quicly/include/quicly {{ h2o_include }}

    if ! [[ -f {{ lib_dir }}/libh2o.a ]]; then
      mkdir h2o_build_temp
      pushd h2o_build_temp
      cmake $TEMP
      make libh2o
      cp libh2o.a ../{{ lib_dir }}
      cp libh2o.pc ../{{ lib_dir }}
      popd
    fi

    rm -rf h2o_build_temp
    rm -rf $TEMP

compile type="debug":
    #!/usr/bin/env bash
    [[ -d {{ out_dir }} ]] || mkdir -p {{ out_dir }}
    [[ -d {{ h2o_include }} ]] || just ensure_h2o

    if [[ {{ type }} == "debug" ]]; then
        find {{ src_dir }} -name "*.c" -exec sh -c 'gcc -c "$1" -I {{ include_dir }} -I {{ h2o_include }} {{ debug_compile_flags }} -o "{{ out_dir }}/$(basename "${1%.c}")-debug.o"' sh {} \;
    else
        find {{ src_dir }} -name "*.c" -exec sh -c 'gcc -c "$1" -I {{ include_dir }} -I {{ h2o_include }} {{ release_compile_flags }} -o "{{ out_dir }}/$(basename "${1%.c}")-release.o"' sh {} \;
    fi

link type="debug":
    #!/usr/bin/env bash
    [[ -d {{ bin_dir }} ]] || mkdir -p {{ bin_dir }}
    [[ -f {{ lib_dir }}/libh2o.a ]] || just ensure_h2o

    if [[ {{ type }} == "debug" ]]; then
        gcc {{ out_dir }}/*-debug.o -L {{ lib_dir }} {{ link_flags }} {{ debug_link_flags }} -o {{ bin_dir }}/yueah-debug
    else
        gcc {{ out_dir }}/*-release.o -L {{ lib_dir }} {{ link_flags }} {{ release_link_flags }} -o {{ bin_dir }}/yueah
    fi

release:
    just compile release
    just link release

debug:
    just compile debug
    just link debug

migrate:
    just --justfile migrator/justfile migrate

bear:
    bear -- just compile
    sed -i 's|"/nix/store/[^"]*gcc[^"]*|\"gcc|g' compile_commands.json
