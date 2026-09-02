project(
    'xterm-plus',
    'c',
    version: '0.5.0-dev',
    meson_version: '>= 1.3',
    default_options: [
        'buildtype=debugoptimized',
        'c_std=c11',
        'warning_level=3',
        'werror=true',
    ],
)

cc = meson.get_compiler('c')
fs = import('fs')
python3 = find_program('python3')
uv = find_program('uv', required: false)

build_version = get_option('release-version')
if build_version == ''
    build_version = meson.project_version()
endif
program_name = 'revenant'

version_configuration = configuration_data()
version_configuration.set_quoted('XTP_VERSION', build_version)
version_configuration.set_quoted('XTP_PROGRAM_NAME', program_name)
configure_file(
    output: 'version.h',
    configuration: version_configuration,
)

add_project_arguments(
    cc.get_supported_arguments(
        '-D_DEFAULT_SOURCE',
        '-Wdeclaration-after-statement',
        '-Wshadow',
        '-Wstrict-prototypes',
        '-Wswitch-enum',
        '-Wundef',
    ),
    language: 'c',
)

if get_option('profiling')
    add_project_arguments(
        cc.get_supported_arguments('-fno-omit-frame-pointer'),
        language: 'c',
    )
endif

x11_dep = dependency('x11')
xt_dep = dependency('xt')
xaw_dep = dependency('xaw7')
xmu_dep = dependency('xmu')
fontconfig_dep = dependency('fontconfig')
xft_dep = dependency('xft')
xrender_dep = dependency('xrender')
cairo_dep = dependency('cairo', version: '>= 1.18')
cairo_ft_dep = dependency('cairo-ft', version: '>= 1.18')
cairo_xlib_dep = dependency('cairo-xlib', version: '>= 1.18')
harfbuzz_dep = dependency('harfbuzz')
util_dep = cc.find_library('util')
m_dep = cc.find_library('m')

sources = files(
    'src/char_class.c',
    'src/command_options.c',
    'src/config_report.c',
    'src/cursor_blink.c',
    'src/diagnostics.c',
    'src/emoji_presentation.c',
    'src/font_chain.c',
    'src/font_metrics.c',
    'src/font_report.c',
    'src/font_role.c',
    'src/font_router.c',
    'src/font_route_cache.c',
    'src/glyph_cairo.c',
    'src/glyph_shape.c',
    'src/main.c',
    'src/menus.c',
    'src/pty.c',
    'src/selftest.c',
    'src/sme_slider.c',
    'src/terminal.c',
    'src/unicode_script.c',
    'src/vt_font.c',
    'src/vt_draw.c',
    'src/vt_input.c',
    'src/vt_interaction.c',
    'src/vt_widget.c',
    'src/x11_opacity.c',
)

xterm_resource_catalog = custom_target(
    'xterm-411-resource-catalog',
    input: [
        'compat/xterm-411-resources.tsv',
        'compat/xterm-411-actions.txt',
        'compat/xterm-411-app-defaults.txt',
    ],
    output: 'xterm_411_resource_catalog.c',
    command: [
        python3,
        files('tools/compile-resource-catalog'),
        '@INPUT0@',
        '@INPUT1@',
        '@INPUT2@',
        '@OUTPUT@',
    ],
)
sources += xterm_resource_catalog

terminal_sources = []
terminal_link_sources = []
terminal_compile_args = []

ghostty_option = get_option('libghostty')
ghostty_source_option = get_option('libghostty-source')
if fs.is_absolute(ghostty_source_option)
    ghostty_source_dir = ghostty_source_option
else
    ghostty_source_dir = meson.current_source_dir() / ghostty_source_option
endif

have_ghostty_source = fs.exists(ghostty_source_dir / 'build.zig')
build_with_ghostty = not ghostty_option.disabled() and have_ghostty_source

if ghostty_option.enabled() and not have_ghostty_source
    error(
        'libghostty was enabled, but no Ghostty checkout exists at ' +
        ghostty_source_dir + '. Run tools/fetch-libghostty first.',
    )
endif

if build_with_ghostty
    zig = find_program('zig', required: true)
    ghostty_builder = find_program('tools/build-libghostty')
    ghostty_archive = custom_target(
        'libghostty-vt',
        output: 'libghostty-vt.a',
        # The checkout is outside Meson's source manifest; tools/fetch-libghostty
        # always detaches, so .git/HEAD holds the commit and changes with the pin.
        depend_files: [
            ghostty_source_dir / '.git' / 'HEAD',
            files('tools/fetch-libghostty', 'tools/build-libghostty'),
        ],
        command: [
            ghostty_builder,
            zig,
            ghostty_source_dir,
            '@OUTPUT@',
            get_option('buildtype'),
        ],
        build_by_default: true,
    )

    terminal_sources += files('src/terminal_ghostty.c')
    terminal_link_sources += ghostty_archive
    terminal_compile_args += [
        '-DGHOSTTY_STATIC',
        '-isystem' + (ghostty_source_dir / 'include'),
    ]
    add_project_arguments('-DHAVE_LIBGHOSTTY=1', language: 'c')
else
    terminal_sources += files('src/terminal_stub.c')
endif

xterm_plus = executable(
    program_name,
    sources,
    terminal_sources,
    terminal_link_sources,
    include_directories: [
        include_directories('src'),
    ],
    dependencies: [
        x11_dep,
        xt_dep,
        xaw_dep,
        xmu_dep,
        fontconfig_dep,
        xft_dep,
        xrender_dep,
        cairo_dep,
        cairo_ft_dep,
        cairo_xlib_dep,
        harfbuzz_dep,
        util_dep,
        m_dep,
    ],
    c_args: terminal_compile_args,
    install: true,
)

install_symlink('xterm+', install_dir: get_option('bindir'), pointing_to: program_name)
install_data('data/revenant.desktop', install_dir: get_option('datadir') / 'applications')
install_data('data/icons/revenant.svg',
    install_dir: get_option('datadir') / 'icons/hicolor/scalable/apps')
install_data('data/icons/revenant-256.png', rename: 'revenant.png',
    install_dir: get_option('datadir') / 'icons/hicolor/256x256/apps')

font_key_test_tool = executable(
    'xtp-send-font-keys',
    'tests/xtp-send-font-keys.c',
    dependencies: x11_dep,
    install: false,
)

font_chain_test_tool = executable(
    'xtp-font-chain',
    'tests/xtp-font-chain.c',
    'src/font_chain.c',
    include_directories: include_directories('src'),
    install: false,
)

font_reload_test_tool = executable(
    'xtp-font-reload',
    [
        'tests/xtp-font-reload.c',
        'src/char_class.c',
        'src/cursor_blink.c',
        'src/diagnostics.c',
        'src/emoji_presentation.c',
        'src/font_chain.c',
        'src/font_metrics.c',
        'src/font_report.c',
        'src/font_role.c',
        'src/font_router.c',
        'src/font_route_cache.c',
        'src/glyph_cairo.c',
        'src/glyph_shape.c',
        'src/menus.c',
        'src/sme_slider.c',
        'src/terminal.c',
        'src/terminal_stub.c',
        'src/unicode_script.c',
        'src/vt_font.c',
        'src/vt_draw.c',
        'src/vt_input.c',
        'src/vt_interaction.c',
        'src/vt_widget.c',
        'src/x11_opacity.c',
    ],
    include_directories: include_directories('src'),
    dependencies: [
        x11_dep,
        xt_dep,
        xaw_dep,
        xmu_dep,
        fontconfig_dep,
        xft_dep,
        xrender_dep,
        cairo_dep,
        cairo_ft_dep,
        cairo_xlib_dep,
        harfbuzz_dep,
        m_dep,
    ],
    install: false,
)

key_test_tool = executable(
    'xtp-send-key',
    'tests/xtp-send-key.c',
    dependencies: x11_dep,
    install: false,
)

wheel_test_tool = executable(
    'xtp-send-wheel',
    'tests/xtp-send-wheel.c',
    dependencies: x11_dep,
    install: false,
)

selection_test_tool = executable(
    'xtp-send-selection',
    'tests/xtp-send-selection.c',
    dependencies: x11_dep,
    install: false,
)

shift_click_test_tool = executable(
    'xtp-send-shift-click',
    'tests/xtp-send-shift-click.c',
    dependencies: x11_dep,
    install: false,
)

selection_reader_tool = executable(
    'xtp-read-selection',
    'tests/xtp-read-selection.c',
    dependencies: x11_dep,
    install: false,
)

resize_test_tool = executable(
    'xtp-resize-loop',
    'tests/xtp-resize-loop.c',
    dependencies: x11_dep,
    install: false,
)

compositor_owner_test_tool = executable(
    'xtp-own-compositor',
    'tests/xtp-own-compositor.c',
    dependencies: x11_dep,
    install: false,
)

window_alpha_test_tool = executable(
    'xtp-window-alpha',
    'tests/xtp-window-alpha.c',
    dependencies: [x11_dep, xrender_dep],
    install: false,
)

window_ink_test_tool = executable(
    'xtp-window-ink',
    'tests/xtp-window-ink.c',
    dependencies: [x11_dep, xrender_dep],
    install: false,
)

hold_colors_test_tool = executable(
    'xtp-hold-colors',
    'tests/xtp-hold-colors.c',
    dependencies: x11_dep,
    install: false,
)

opacity_slider_test_tool = executable(
    'xtp-drag-opacity-slider',
    'tests/xtp-drag-opacity-slider.c',
    dependencies: x11_dep,
    install: false,
)

test(
    'self-test',
    xterm_plus,
    args: ['--self-test'],
)

test(
    'internal-branding',
    python3,
    args: [files('tools/check-internal-branding'), meson.current_source_dir()],
)

if uv.found()
    test(
        'han-table-current',
        uv,
        args: ['run', '--script', files('tools/generate-han-table'), '--check'],
    )
else
    test(
        'han-table-current',
        python3,
        args: [files('tools/generate-han-table'), '--check'],
    )
endif

test(
    't0-font-chain-parser',
    python3,
    args: [
        files('tests/t0-font-chain-parser.py'),
        font_chain_test_tool.full_path(),
        files('compat/xterm-411-face-name.json'),
    ],
)

test(
    'probe-keymodes-parser',
    python3,
    args: [files('tools/probe-keymodes.py'), '--self-test'],
)

test(
    'probe-colors-parser',
    python3,
    args: [files('tools/probe-colors.py'), '--self-test'],
)

xvfb_tests_option = get_option('xvfb-tests')
if xvfb_tests_option.enabled() and not build_with_ghostty
    error('xvfb-tests was enabled, but the libghostty backend is unavailable')
endif

xvfb = find_program('Xvfb', required: xvfb_tests_option)
xprop = find_program('xprop', required: xvfb_tests_option)
xrdb = find_program('xrdb', required: xvfb_tests_option)
timeout_program = find_program('timeout', required: xvfb_tests_option)
if build_with_ghostty and xvfb.found()
    test(
        'xvfb-report-config',
        find_program('sh'),
        args: [
            files('tests/xvfb-report-config.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            meson.current_source_dir() / 'font-fixtures-stage',
        ],
        timeout: 30,
        is_parallel: false,
    )
    test(
        'xvfb-colors',
        find_program('sh'),
        args: [
            files('tests/xvfb-colors.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            xrdb.full_path(),
            window_alpha_test_tool.full_path(),
            timeout_program.full_path(),
            hold_colors_test_tool.full_path(),
        ],
        timeout: 30,
        is_parallel: false,
    )
    test(
        'xvfb-opacity',
        find_program('sh'),
        args: [
            files('tests/xvfb-opacity.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            compositor_owner_test_tool.full_path(),
            window_alpha_test_tool.full_path(),
            opacity_slider_test_tool.full_path(),
        ],
        timeout: 30,
        is_parallel: false,
    )
    test(
        'xvfb-font-baseline',
        find_program('sh'),
        args: [
            files('tests/xvfb-font-baseline.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            window_ink_test_tool.full_path(),
            font_key_test_tool.full_path(),
            meson.current_source_dir() / 'font-fixtures-stage',
        ],
        timeout: 30,
        is_parallel: false,
    )
    test(
        'xvfb-emoji-routing',
        find_program('sh'),
        args: [
            files('tests/xvfb-emoji-routing.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            window_ink_test_tool.full_path(),
            meson.current_source_dir() / 'font-fixtures-stage',
        ],
        timeout: 30,
        is_parallel: false,
    )
    test(
        'xvfb-text-shaping',
        find_program('sh'),
        args: [
            files('tests/xvfb-text-shaping.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            window_ink_test_tool.full_path(),
            meson.current_source_dir() / 'font-fixtures-stage',
        ],
        timeout: 30,
        is_parallel: false,
    )
    test(
        'xvfb-font-user-fallback',
        find_program('sh'),
        args: [
            files('tests/xvfb-font-user-fallback.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            meson.current_source_dir() / 'font-fixtures-stage',
        ],
        timeout: 30,
        is_parallel: false,
    )
    test(
        'xvfb-font-system-fallback',
        find_program('sh'),
        args: [
            files('tests/xvfb-font-system-fallback.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            meson.current_source_dir() / 'font-fixtures-stage',
        ],
        timeout: 30,
        is_parallel: false,
    )
    test(
        'xvfb-font-han',
        find_program('sh'),
        args: [
            files('tests/xvfb-font-han.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            meson.current_source_dir() / 'font-fixtures-stage',
        ],
        timeout: 30,
        is_parallel: false,
    )
    test(
        'xvfb-font-tofu',
        find_program('sh'),
        args: [
            files('tests/xvfb-font-tofu.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            window_ink_test_tool.full_path(),
            meson.current_source_dir() / 'font-fixtures-stage',
        ],
        timeout: 30,
        is_parallel: false,
    )
    test(
        'xvfb-font-wide-boundary',
        find_program('sh'),
        args: [
            files('tests/xvfb-font-wide-boundary.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            meson.current_source_dir() / 'font-fixtures-stage',
        ],
        timeout: 30,
        is_parallel: false,
    )
    test(
        'xvfb-font-style-family',
        find_program('sh'),
        args: [
            files('tests/xvfb-font-style-family.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            meson.current_source_dir() / 'font-fixtures-stage',
        ],
        timeout: 30,
        is_parallel: false,
    )
    test(
        'xvfb-font-route-cache',
        find_program('sh'),
        args: [
            files('tests/xvfb-font-route-cache.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            meson.current_source_dir() / 'font-fixtures-stage',
        ],
        timeout: 30,
        is_parallel: false,
    )
    test(
        'xvfb-font-routing-report',
        find_program('sh'),
        args: [
            files('tests/xvfb-font-routing-report.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            key_test_tool.full_path(),
            meson.current_source_dir() / 'font-fixtures-stage',
            files('tests/check-font-routing-report.py'),
        ],
        timeout: 30,
        is_parallel: false,
    )
    test(
        'xvfb-font-reload',
        find_program('sh'),
        args: [
            files('tests/xvfb-font-reload.sh'),
            xvfb.full_path(),
            font_reload_test_tool.full_path(),
            meson.current_source_dir() / 'font-fixtures-stage',
        ],
        timeout: 30,
        is_parallel: false,
    )
    if xprop.found() and xrdb.found()
        test(
            't0-font-resolution',
            python3,
            args: [
                files('tests/t0-font-resolution.py'),
                '--xvfb', xvfb.full_path(),
                '--xprop', xprop.full_path(),
                '--xrdb', xrdb.full_path(),
                '--xterm-plus', xterm_plus.full_path(),
                '--fixture', files('compat/xterm-411-face-name.json'),
                '--fixture-root', meson.current_source_dir() / 'font-fixtures-stage',
            ],
            timeout: 90,
            is_parallel: false,
        )
    endif
    test(
        'xvfb-selection',
        find_program('sh'),
        args: [
            files('tests/xvfb-selection.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            selection_test_tool.full_path(),
            selection_reader_tool.full_path(),
            shift_click_test_tool.full_path(),
        ],
        timeout: 30,
        is_parallel: false,
    )
    test(
        'xvfb-keyboard',
        find_program('sh'),
        args: [
            files('tests/xvfb-keyboard.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            key_test_tool.full_path(),
        ],
        timeout: 30,
        is_parallel: false,
    )
    test(
        'xvfb-kitty-keyboard',
        find_program('sh'),
        args: [
            files('tests/xvfb-kitty-keyboard.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            key_test_tool.full_path(),
        ],
        timeout: 30,
        is_parallel: false,
    )
    test(
        'xvfb-keyboard-matrix',
        find_program('sh'),
        args: [
            files('tests/xvfb-keyboard-matrix.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            key_test_tool.full_path(),
        ],
        timeout: 30,
        is_parallel: false,
    )
endif

summary(
    {
        'executable': program_name,
        'X resource class': 'XTerm',
        'X resource instance': 'xterm',
        'libghostty-vt': build_with_ghostty,
        'libghostty source': ghostty_source_dir,
        'profiling frame pointers': get_option('profiling'),
        'font-key test tool': font_key_test_tool.full_path(),
        'key test tool': key_test_tool.full_path(),
        'selection test tool': selection_test_tool.full_path(),
        'selection reader tool': selection_reader_tool.full_path(),
        'Shift-click test tool': shift_click_test_tool.full_path(),
        'Xvfb selection test': build_with_ghostty and xvfb.found(),
        'Xvfb opacity test': build_with_ghostty and xvfb.found(),
        'resize test tool': resize_test_tool.full_path(),
        'wheel test tool': wheel_test_tool.full_path(),
    },
    section: 'xterm+ configuration',
)
