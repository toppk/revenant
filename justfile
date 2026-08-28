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

# Repeatedly resize a Revenant window to exercise reflow and expose handling
resize-loop window cycles="4" delay_ms="100": build-gcc
    ./{{gcc_build}}/xtp-resize-loop "{{window}}" "{{cycles}}" "{{delay_ms}}"

# Open the fixed 45-column Bash prompt used by the wrapped-prompt reproducer
reflow-prompt: build-gcc
    ./{{gcc_build}}/revenant -debug -geometry 80x24 -e bash --noprofile --rcfile "{{justfile_directory()}}/tests/reflow-prompt.bash" -i

# Cross the fixture prompt's wrap boundary once, using terminal grid sizes
reflow-resize window: build-gcc
    ./{{gcc_build}}/xtp-resize-loop "{{window}}" --grid 38 80 24 250

# Interactively inspect legacy, raw, and Kitty keyboard encoding
probe-keymodes *args:
    python3 tools/probe-keymodes.py {{args}}

# Display the SGR attribute and color sampler
probe-color:
    tools/probe-color.sh

# Step through SGR 7, DECSCNM, and widget reverse-video rendering
probe-reverse-video *args:
    tools/probe-reverse-video.sh {{args}}

# Display OSC 8 links for hover and activation checks
probe-osc8:
    tools/probe-osc8.sh

# Interactively inspect emoji, grapheme width, and alignment
probe-emoji *args:
    python3 tools/probe-emoji.py {{args}}

# Build and test all supported compiler/backend combinations
test: test-gcc test-clang test-stub

# Format C sources and test helpers
format:
    clang-format -i src/*.[ch] tests/*.c

# Verify C formatting without changing files
format-check:
    clang-format --dry-run --Werror src/*.[ch] tests/*.c

# Build landing page, Revenant docs, and TDN into site/
site:
    rm -rf site
    {{mkdocs}} build --strict -d site/docs
    {{mkdocs}} build --strict -f tdn/mkdocs.yml -d ../site/tdn
    cp -r www/. site/

# Build everything and serve at http://localhost:8000/
serve: site
    python3 -m http.server -d site 8000

# Live-reload the Revenant docs only
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
