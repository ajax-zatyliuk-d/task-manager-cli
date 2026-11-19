#!/bin/bash
# Build helper for task-manager-cli, adapted from the mcu_communicator autobuild.sh.
# Build directories live next to the sources: ../<project>-<preset>/out/bin

RED='\033[31;1m'
YEL='\033[33;1m'
GREEN='\033[32;1m'
NC='\033[0m' # No Color

# Directories that are never formatted or checked
EXCLUDE_DIRS=("build" "docs" ".git" ".claude" ".vscode")
EXCLUDE_FILES=()

SRC_DIR_NAME=$(basename "${PWD}")
APP_NAME="task_manager"
TESTS=("test_task" "test_task_manager" "test_cli")

ACTION_ARG="$1"
SECOND_ARG="$2"

# Preset: debug (ASan + UBSan, default) or release (no sanitizers, for valgrind)
PRESET="${SECOND_ARG:-debug}"

set_build_paths() {
    BUILD_DIR_PATH="../${SRC_DIR_NAME}-$1"
    BIN_DIR_PATH="${BUILD_DIR_PATH}/out/bin"
    APP_BIN_PATH="${BIN_DIR_PATH}/${APP_NAME}"
}

check_preset() {
    if [[ "$PRESET" != "debug" && "$PRESET" != "release" ]]; then
        echo -e "${RED}ERR: Unknown preset: $PRESET (expected debug or release)${NC}"
        exit 1
    fi
    set_build_paths "$PRESET"
}

require_tool() {
    command -v "$1" >/dev/null || {
        echo -e "${RED}Missing tool: $1${NC}"
        [ -n "$2" ] && echo "$2"
        exit 1
    }
}

configure() {
    check_preset
    require_tool cmake
    echo "Configuring preset: $PRESET"
    cmake --preset "$PRESET" "$@" || {
        echo -e "${RED}ERR: cmake configure failed for preset [$PRESET]${NC}"
        exit 1
    }
}

build() {
    check_preset
    require_tool cmake
    [ -d "$BUILD_DIR_PATH" ] || configure

    echo "Building preset: $PRESET"
    cmake --build --preset "$PRESET" -- -j"$(nproc)" || {
        echo -e "${RED}ERR: cmake build failed for preset [$PRESET]${NC}"
        exit 1
    }

    if [ -f "$APP_BIN_PATH" ]; then
        echo -e "${GREEN}Build Done:${NC}${YEL} [$PRESET]${NC} -> $BIN_DIR_PATH"
    else
        echo -e "${RED}ERR: no binary found at $APP_BIN_PATH${NC}"
        exit 1
    fi
}

run_tests() {
    build
    ctest --preset "$PRESET" || {
        echo -e "${RED}ERR: tests failed for preset [$PRESET]${NC}"
        exit 1
    }
    echo -e "${GREEN}Tests passed${NC}${YEL} [$PRESET]${NC}"
}

run_valgrind() {
    PRESET="release"
    require_tool valgrind "On Ubuntu/Debian: sudo apt-get install valgrind"
    build

    local failed=0
    for test_name in "${TESTS[@]}"; do
        echo "valgrind: $test_name"
        # -q: print only errors and leaks; definite/possible leaks count as errors
        valgrind -q --leak-check=full --error-exitcode=1 "${BIN_DIR_PATH}/${test_name}" >/dev/null || failed=1
    done

    if [ "$failed" -ne 0 ]; then
        echo -e "${RED}ERR: valgrind reported errors or leaks${NC}"
        exit 1
    fi
    echo -e "${GREEN}valgrind: no errors${NC}"
}

run_lint() {
    check_preset
    require_tool cppcheck "On Ubuntu/Debian: sudo apt-get install cppcheck"
    require_tool pkg-config "On Ubuntu/Debian: sudo apt-get install pkg-config"
    require_tool include-what-you-use "On Ubuntu/Debian: sudo apt-get install iwyu"
    require_tool iwyu_tool "On Ubuntu/Debian: sudo apt-get install iwyu"

    configure -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

    local compile_db="$BUILD_DIR_PATH/compile_commands.json"
    if [[ ! -s "$compile_db" ]]; then
        echo -e "${RED}ERR: missing compilation database: $compile_db (use a Makefiles or Ninja generator)${NC}" >&2
        return 1
    fi

    local iwyu_mapping="$PWD/.iwyu.imp"
    if [[ ! -s "$iwyu_mapping" ]]; then
        echo "ERR: $iwyu_mapping is missing or empty. Use the project mapping or a JSON array []." >&2
        return 1
    fi

    local glib_cflags
    local -a glib_includes
    glib_cflags=$(pkg-config --cflags-only-I glib-2.0) || return 1
    read -r -a glib_includes <<< "$glib_cflags"

    local lint_log="$BUILD_DIR_PATH/lint.log"
    local failed=0

    if ! : > "$lint_log"; then
        return 1
    fi

    {
        echo "Checking C code with Cppcheck..."
        cppcheck --project="$compile_db" "${glib_includes[@]}" \
            -i "$PWD/tests/" \
            --enable=style --error-exitcode=1 || failed=1

        echo "Checking includes with IWYU..."
        iwyu_tool -p "$BUILD_DIR_PATH" -- \
            -Xiwyu "--mapping_file=$iwyu_mapping" -Xiwyu --error=1 || failed=1
    } >> "$lint_log" 2>&1 || return 1

    if [[ "$failed" -eq 0 ]]; then
        echo "Lint passed. Log: $lint_log"
    else
        echo "Lint failed. Log: $lint_log" >&2
    fi

    return "$failed"
}

clean_build() {
    check_preset
    echo "Clean $BUILD_DIR_PATH"
    rm -rf "$BUILD_DIR_PATH"
}

# Print the list of C sources, honouring the exclusions
list_sources() {
    local find_cmd="find ."

    for dir in "${EXCLUDE_DIRS[@]}"; do
        find_cmd="$find_cmd \\( -path \"./$dir\" -prune \\) -o"
    done

    for file in "${EXCLUDE_FILES[@]}"; do
        find_cmd="$find_cmd \\( -name \"$file\" -prune \\) -o"
    done

    find_cmd="$find_cmd \\( -type f \\( -name \"*.c\" -o -name \"*.h\" \\) -print \\)"
    eval "$find_cmd" | sort
}

require_clang_format() {
    require_tool clang-format "On Ubuntu/Debian: sudo apt-get install clang-format"
}

# Rewrite sources in place according to .clang-format
format_code() {
    require_clang_format
    echo -e "${YEL}Formatting source code...${NC}"

    local files
    mapfile -t files < <(list_sources)
    if [ "${#files[@]}" -eq 0 ]; then
        echo -e "${YEL}No source files found to format${NC}"
        return 0
    fi

    for file in "${files[@]}"; do
        echo "Formatting: $file"
        clang-format -i "$file" || {
            echo -e "${RED}Failed to format: $file${NC}"
            exit 1
        }
    done
    echo -e "${GREEN}Formatting complete${NC} - processed ${#files[@]} files"
}

# Check formatting without modifying files; non-zero exit if anything differs
check_format() {
    require_clang_format
    echo -e "${YEL}Checking source formatting...${NC}"

    local files
    mapfile -t files < <(list_sources)
    local bad=()

    for file in "${files[@]}"; do
        if ! clang-format --dry-run --Werror "$file" >/dev/null 2>&1; then
            bad+=("$file")
        fi
    done

    if [ "${#bad[@]}" -ne 0 ]; then
        echo -e "${RED}Formatting issues in ${#bad[@]} of ${#files[@]} files:${NC}"
        printf '  %s\n' "${bad[@]}"
        echo "Details: clang-format --dry-run <file>; fix: ./autobuild.sh format"
        exit 1
    fi
    echo -e "${GREEN}Formatting OK${NC} - checked ${#files[@]} files"
}

show_help() {
    echo -e "${YEL}Usage:${NC}"
    echo "  ./autobuild.sh                         - Build debug preset (configure if needed)"
    echo "  ./autobuild.sh configure [debug|release] - Run cmake configure step"
    echo "  ./autobuild.sh build [debug|release]   - Build project (default: debug)"
    echo "  ./autobuild.sh test [debug|release]    - Build and run tests via ctest"
    echo "  ./autobuild.sh valgrind                - Run all tests under valgrind (release preset)"
    echo "  ./autobuild.sh clean [debug|release]   - Remove build directory"
    echo "  ./autobuild.sh format                  - clang-format all sources in place"
    echo "  ./autobuild.sh format-check            - Check formatting without changes (for CI)"
    echo "  ./autobuild.sh lint [debug|release]    - Run Cppcheck and Include What You Use"
    echo "  ./autobuild.sh -h | help               - Show help"
}

case "$ACTION_ARG" in
configure)
    configure
    ;;
-b | build)
    build
    ;;
-t | test)
    run_tests
    ;;
valgrind)
    run_valgrind
    ;;
lint)
    run_lint
    ;;
clean)
    clean_build
    ;;
format)
    format_code
    ;;
format-check | check-format)
    check_format
    ;;
-h | --help | help)
    show_help
    ;;
"")
    build
    ;;
*)
    echo -e "${RED}Unknown option '${ACTION_ARG}'. Use -h for help.${NC}"
    exit 1
    ;;
esac
