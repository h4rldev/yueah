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
debug_link_flags := '-ggdb -fsanitize=address -static-libasan -ldotenv-debug'
release_compile_flags := '-O2 -flto'
release_link_flags := '-O2 -flto -ldotenv'
color_reset := "\\033[0m"
color_red := "\\033[31m"
color_green := "\\033[32m"
color_yellow := "\\033[33m"
color_blue := "\\033[34m"
color_cyan := "\\033[36m"

default:
    just --list

ensure_h2o threads=num_cpus():
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
      pushd h2o_build_temp >/dev/null
      cmake $TEMP
      make libh2o -j{{ threads }}
      cp libh2o.a ../{{ lib_dir }}
      cp libh2o.pc ../{{ lib_dir }}
      popd >/dev/null
    fi

    rm -rf h2o_build_temp
    rm -rf $TEMP

ensure_dotenv type="debug" threads=num_cpus():
    just --justfile dotenv/justfile {{ type }}

compile type="debug" force="false" threads=num_cpus():
    #!/usr/bin/env bash
    shopt -s globstar

    [[ -d {{ out_dir }} ]] || mkdir -p {{ out_dir }}
    [[ -d {{ h2o_include }} ]] || just ensure_h2o

    WILL_COMPILE=0
    if [[ {{ type }} == "debug" ]]; then
        for file in {{ src_dir }}/**/*.c; do
            if [[ "$file" -nt {{ out_dir }}/$(basename "${file%.c}")-debug.o ]]; then
                WILL_COMPILE=1
            fi
        done
    else
        for file in {{ src_dir }}/**/*.c; do
            if [[ "$file" -nt {{ out_dir }}/$(basename "${file%.c}")-release.o ]]; then
                WILL_COMPILE=1
            fi
        done
    fi

    if [[ {{ force }} == "true" ]]; then
        echo -e "Compile: Forcing.."
        WILL_COMPILE=1
    fi

    if [[ $WILL_COMPILE -eq 0 ]]; then
        echo -e "Compile: Nothing to do.."
        exit 0
    fi

    echo -e "Using {{ color_red }}{{ threads }}{{ color_reset }} threads"
    echo -e "Target: {{ color_green }}{{ type }}{{ color_reset }}\n"
    if [[ {{ type }} == "debug" ]]; then
        find {{ src_dir }} -name "*.c" -print0 | xargs -0 -P{{ threads }} -n1 \
            sh -c 'if [[ $1 -nt {{ out_dir }}/$(basename "${1%.c}")-debug.o || {{ force }} == "true" ]]; then echo -e "Compiling {{ color_green }}$1{{ color_reset }}..."; gcc -c "$1" -I {{ include_dir }} -I {{ h2o_include }} {{ debug_compile_flags }} -o "{{ out_dir }}/$(basename "${1%.c}")-debug.o"; fi' sh
    else
        find {{ src_dir }} -name "*.c" -print0 | xargs -0 -P{{ num_cpus() }} -n1 \
            sh -c 'if [[ $1 -nt {{ out_dir }}/$(basename "${1%.c}")-release.o || {{ force }} == "true" ]]; then echo -e "Compiling {{ color_green }}$1{{ color_reset }}..."; gcc -c "$1" -I {{ include_dir }} -I {{ h2o_include }} {{ release_compile_flags }} -o "{{ out_dir }}/$(basename "${1%.c}")-release.o"; fi' sh
    fi

link type="debug":
    #!/usr/bin/env bash
    [[ -d {{ bin_dir }} ]] || mkdir -p {{ bin_dir }}
    [[ -f {{ lib_dir }}/libh2o.a ]] || just ensure_h2o

    if [[ {{ type }} == "debug" ]]; then
        [[ -f {{ lib_dir }}/libdotenv-debug.a ]] || just ensure_dotenv debug
    else
        [[ -f {{ lib_dir }}/libdotenv.a ]] || just ensure_dotenv release
    fi

    WILL_LINK=0
    if [[ {{ type }} == "debug" ]]; then
        for file in {{ out_dir }}/*-debug.o; do
            if [[ "$file" -nt {{ bin_dir }}/yueah-debug ]]; then
                WILL_LINK=1
            fi
        done
    else
        for file in {{ out_dir }}/*-release.o; do
            if [[ "$file" -nt {{ bin_dir }}/yueah ]]; then
                WILL_LINK=1
            fi
        done
    fi

    if [[ $WILL_LINK -eq 0 ]]; then
        echo -e "Link: Nothing to do.."
        exit 0
    fi

    echo -e "\nLinking..."
    if [[ {{ type }} == "debug" ]]; then
        gcc {{ out_dir }}/*-debug.o -L {{ lib_dir }} {{ link_flags }} {{ debug_link_flags }} -o {{ bin_dir }}/yueah-debug
    else
        gcc {{ out_dir }}/*-release.o -L {{ lib_dir }} {{ link_flags }} {{ release_link_flags }} -o {{ bin_dir }}/yueah
    fi

release threads=num_cpus():
    just compile release {{ threads }}
    just link release

debug threads=num_cpus():
    just compile debug {{ threads }}
    just link debug

migrate:
    just --justfile migrator/justfile migrate

bear:
    bear -- just compile debug true
    sed -i 's|"/nix/store/[^"]*gcc[^"]*|\"gcc|g' compile_commands.json
