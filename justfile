# Maintainer tasks. Run `just` to list.

mkdocs := "uvx --with mkdocs-material mkdocs"
gcc_build := "build-agent-gcc"
clang_build := "build-agent-clang"
stub_build := "build-agent-stub"

default:
    @just --list

# Configure an isolated build directory, creating or reconfiguring as needed
_configure build_dir compiler ghostty:
    @if test -f "{{build_dir}}/meson-private/coredata.dat"; then CC="{{compiler}}" meson setup --reconfigure "{{build_dir}}" -Dlibghostty="{{ghostty}}"; else CC="{{compiler}}" meson setup "{{build_dir}}" -Dlibghostty="{{ghostty}}"; fi

# Build the libghostty backend with GCC
build-gcc:
    @just _configure {{gcc_build}} gcc enabled
    meson compile -C {{gcc_build}}

test-gcc: build-gcc
    meson test -C {{gcc_build}} --print-errorlogs

# Build the libghostty backend with Clang
build-clang:
    @just _configure {{clang_build}} clang enabled
    meson compile -C {{clang_build}}

test-clang: build-clang
    meson test -C {{clang_build}} --print-errorlogs

# Build the UI-only stub backend
build-stub:
    @just _configure {{stub_build}} gcc disabled
    meson compile -C {{stub_build}}

test-stub: build-stub
    meson test -C {{stub_build}} --print-errorlogs

# Build and test all supported compiler/backend combinations
test: test-gcc test-clang test-stub

# Format C sources and test helpers
format:
    clang-format -i src/*.[ch] tests/*.c

# Verify C formatting without changing files
format-check:
    clang-format --dry-run --Werror src/*.[ch] tests/*.c

# Build landing page, xterm+ docs, and TDN into site/
site:
    rm -rf site
    {{mkdocs}} build --strict -d site/docs
    {{mkdocs}} build --strict -f tdn/mkdocs.yml -d ../site/tdn
    cp -r www/. site/

# Build everything and serve at http://localhost:8000/
serve: site
    python3 -m http.server -d site 8000

# Live-reload the xterm+ docs only
serve-docs:
    {{mkdocs}} serve

# Live-reload TDN only
serve-tdn:
    {{mkdocs}} serve -f tdn/mkdocs.yml

# Strict-build both mkdocs sites without writing site/
check:
    {{mkdocs}} build --strict -d /tmp/xterm-plus-check/docs
    {{mkdocs}} build --strict -f tdn/mkdocs.yml -d /tmp/xterm-plus-check/tdn
    rm -rf /tmp/xterm-plus-check

# Run formatting, compiler/backend, and documentation checks
check-all: format-check test check

# Remove built site output
clean-site:
    rm -rf site tdn/site
