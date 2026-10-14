test('internal-branding-policy', python3, args: files('internal-branding.py'))
uv = find_program('uv', required: false)
widget_test_dependencies = widget_dependencies + [m_dep]
test_root_include_directories = include_directories('..')
test_include_directories = include_directories('..', '../src')

font_key_test_tool = executable(
    'xtp-send-font-keys',
    'xtp-send-font-keys.c',
    dependencies: x11_dep,
    include_directories: test_root_include_directories,
    install: false,
)

font_chain_test_tool = executable(
    'xtp-font-chain',
    'xtp-font-chain.c',
    '../src/font_chain.c',
    include_directories: test_include_directories,
    install: false,
)

font_reload_test_tool = executable(
    'xtp-font-reload',
    [
        'xtp-font-reload.c',
        widget_sources,
        '../src/terminal_stub.c',
    ],
    include_directories: test_include_directories,
    dependencies: widget_test_dependencies,
    install: false,
)

# The same transitions with the real terminal backend, which is what makes painting
# observable: the stub emits no cells and reports column zero.
paint_test_sources = widget_sources + terminal_sources + terminal_link_sources
font_reload_paint_test_tool = executable(
    'xtp-font-reload-paint',
    'xtp-font-reload-paint.c',
    paint_test_sources,
    include_directories: test_include_directories,
    c_args: terminal_compile_args,
    dependencies: widget_test_dependencies,
    install: false,
)

sync_output_paint_test_tool = executable(
    'xtp-sync-output-paint',
    'xtp-sync-output-paint.c',
    paint_test_sources,
    include_directories: test_include_directories,
    c_args: terminal_compile_args,
    dependencies: widget_test_dependencies,
    install: false,
)

xfixes_dep = dependency('xfixes', required: false)
if build_with_ghostty and xfixes_dep.found()
    pointer_shape_test_tool = executable(
        'xtp-pointer-shape',
        'xtp-pointer-shape.c',
        paint_test_sources,
        include_directories: test_include_directories,
        c_args: terminal_compile_args,
        dependencies: widget_test_dependencies + [xfixes_dep],
        install: false,
    )
endif

search_lifecycle_test_tool = executable(
    'xtp-search-lifecycle',
    'xtp-search-lifecycle.c',
    widget_sources,
    terminal_sources,
    terminal_link_sources,
    include_directories: test_include_directories,
    c_args: terminal_compile_args,
    dependencies: widget_test_dependencies,
    install: false,
)

key_test_tool = executable(
    'xtp-send-key',
    'xtp-send-key.c',
    dependencies: x11_dep,
    include_directories: test_root_include_directories,
    install: false,
)

close_window_tool = executable(
    'xtp-close-window',
    'xtp-close-window.c',
    dependencies: x11_dep,
    include_directories: test_root_include_directories,
    install: false,
)

property_watch_tool = executable(
    'xtp-watch-properties',
    'xtp-watch-properties.c',
    dependencies: x11_dep,
    include_directories: test_root_include_directories,
    install: false,
)

xtst_dep = dependency('xtst', required: false)
wheel_test_tool = executable(
    'xtp-send-wheel',
    'xtp-send-wheel.c',
    dependencies: [x11_dep, xtst_dep],
    c_args: xtst_dep.found() ? ['-DXTP_HAVE_XTEST=1'] : [],
    include_directories: test_root_include_directories,
    install: false,
)

selection_test_tool = executable(
    'xtp-send-selection',
    'xtp-send-selection.c',
    dependencies: x11_dep,
    include_directories: test_root_include_directories,
    install: false,
)

shift_click_test_tool = executable(
    'xtp-send-shift-click',
    'xtp-send-shift-click.c',
    dependencies: x11_dep,
    include_directories: test_root_include_directories,
    install: false,
)

hover_test_tool = executable(
    'xtp-send-hover',
    'xtp-send-hover.c',
    dependencies: x11_dep,
    include_directories: test_root_include_directories,
    install: false,
)

selection_owner_tool = executable(
    'xtp-own-selection',
    'xtp-own-selection.c',
    dependencies: x11_dep,
    include_directories: test_root_include_directories,
    install: false,
)

null_fault_preload = shared_module(
    'xtp-null-fault',
    'xtp-null-fault.c',
    dependencies: cc.find_library('dl', required: false),
    include_directories: test_root_include_directories,
    install: false,
)

urgency_test_tool = executable(
    'xtp-wm-urgency',
    'xtp-wm-urgency.c',
    dependencies: x11_dep,
    include_directories: test_root_include_directories,
    install: false,
)

# libnotify builds get a GIO stand-in for org.freedesktop.Notifications to deliver to.
if libnotify_dep.found()
    fake_notifications_tool = executable(
        'xtp-fake-notifications',
        'xtp-fake-notifications.c',
        dependencies: dependency('gio-2.0', include_type: 'system'),
        include_directories: test_root_include_directories,
        install: false,
    )
endif

window_ops_test_tool = executable(
    'xtp-toggle-window-ops',
    'xtp-toggle-window-ops.c',
    dependencies: x11_dep,
    include_directories: test_root_include_directories,
    install: false,
)

selection_reader_tool = executable(
    'xtp-read-selection',
    'xtp-read-selection.c',
    dependencies: x11_dep,
    include_directories: test_root_include_directories,
    install: false,
)

resize_test_tool = executable(
    'xtp-resize-loop',
    'xtp-resize-loop.c',
    dependencies: x11_dep,
    include_directories: test_root_include_directories,
    install: false,
)

compositor_owner_test_tool = executable(
    'xtp-own-compositor',
    'xtp-own-compositor.c',
    dependencies: x11_dep,
    include_directories: test_root_include_directories,
    install: false,
)

window_alpha_test_tool = executable(
    'xtp-window-alpha',
    'xtp-window-alpha.c',
    dependencies: [x11_dep, xrender_dep],
    include_directories: test_root_include_directories,
    install: false,
)

window_ink_test_tool = executable(
    'xtp-window-ink',
    'xtp-window-ink.c',
    dependencies: [x11_dep, xrender_dep],
    include_directories: test_root_include_directories,
    install: false,
)

hold_colors_test_tool = executable(
    'xtp-hold-colors',
    'xtp-hold-colors.c',
    dependencies: x11_dep,
    include_directories: test_root_include_directories,
    install: false,
)

opacity_slider_test_tool = executable(
    'xtp-drag-opacity-slider',
    'xtp-drag-opacity-slider.c',
    dependencies: x11_dep,
    include_directories: test_root_include_directories,
    install: false,
)

url_match_test_tool = executable(
    'xtp-url-match',
    ['xtp-url-match.c', '../src/url_match.c'],
    include_directories: test_include_directories,
    install: false,
)

test('url-match-sentence', url_match_test_tool, args: ['sentence'])
test('url-match-pointer-outside', url_match_test_tool, args: ['pointer-outside'])
test('url-match-balanced-punctuation', url_match_test_tool, args: ['balanced-punctuation'])
test('url-match-unmatched-punctuation', url_match_test_tool, args: ['unmatched-punctuation'])
test('url-match-case-insensitive', url_match_test_tool, args: ['case-insensitive'])
test('url-match-missing-host', url_match_test_tool, args: ['missing-host'])
test('url-match-length-limit', url_match_test_tool, args: ['length-limit'])
test('url-match-null-input', url_match_test_tool, args: ['null-input'])

test(
    'self-test',
    xterm_plus,
    args: ['-self-test'],
)

test(
    'command-line',
    find_program('sh'),
    args: [files('command-line.sh'), xterm_plus.full_path()],
    depends: [xterm_plus],
)

test(
    'internal-branding',
    python3,
    args: [files('../tools/check-internal-branding'), meson.project_source_root()],
)

if uv.found()
    test(
        'han-table-current',
        uv,
        args: ['run', '--script', files('../tools/generate-han-table'), '--check'],
    )
else
    test(
        'han-table-current',
        python3,
        args: [files('../tools/generate-han-table'), '--check'],
    )
endif

test(
    't0-font-chain-parser',
    python3,
    args: [
        files('t0-font-chain-parser.py'),
        font_chain_test_tool.full_path(),
        files('../compat/xterm-411-face-name.json'),
    ],
    depends: [font_chain_test_tool],
)

test(
    'probe-keymodes-parser',
    python3,
    args: [files('../tools/probe-keymodes.py'), '--self-test'],
)

test('probe-features', python3, args: [files('probe-features.py')], timeout: 45)

test(
    'unicode-table-provenance',
    python3,
    args: [files('unicode-table-provenance.py')],
    timeout: 60,
)

test('emoji-coverage-audit', python3, args: [files('emoji-coverage-audit.py')], timeout: 60)

test(
    'probe-colors-parser',
    python3,
    args: [files('../tools/probe-colors.py'), '--self-test'],
)

xvfb_tests_option = get_option('xvfb-tests')
if xvfb_tests_option.enabled() and not build_with_ghostty
    error('xvfb-tests was enabled, but the libghostty backend is unavailable')
endif

xvfb = find_program('Xvfb', required: xvfb_tests_option)
xprop = find_program('xprop', required: xvfb_tests_option)
xwininfo = find_program('xwininfo', required: xvfb_tests_option)
xrdb = find_program('xrdb', required: xvfb_tests_option)
timeout_program = find_program('timeout', required: xvfb_tests_option)
if xvfb.found()
    test(
        'xvfb-search-lifecycle',
        find_program('sh'),
        args: [files('xvfb-search-lifecycle.sh'), xvfb.full_path(), search_lifecycle_test_tool],
        timeout: 60,
        is_parallel: false,
    )
endif
if build_with_ghostty and xvfb.found()
    test(
        'xvfb-pty-burst',
        find_program('sh'),
        args: [
            files('xvfb-pty-burst.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            python3.full_path(),
            files('pty-burst.py'),
        ],
        timeout: 30,
        is_parallel: false,
        depends: [xterm_plus],
    )
    test(
        'xvfb-mouse-scroll',
        find_program('sh'),
        args: [
            files('xvfb-mouse-scroll.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            python3.full_path(),
            files('mouse-scroll.py'),
            wheel_test_tool.full_path(),
            selection_reader_tool.full_path(),
        ],
        depends: [
            wheel_test_tool,
            selection_reader_tool,
            xterm_plus,
        ],
        timeout: 240,
        is_parallel: false,
    )
    test(
        'xvfb-login-shell',
        find_program('sh'),
        args: [
            files('xvfb-login-shell.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            selection_test_tool.full_path(),
            selection_reader_tool.full_path(),
            close_window_tool.full_path(),
        ],
        depends: [
            selection_test_tool,
            selection_reader_tool,
            close_window_tool,
            xterm_plus,
        ],
        timeout: 120,
        is_parallel: false,
    )
    test(
        'xvfb-hold',
        find_program('sh'),
        args: [
            files('xvfb-hold.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            selection_test_tool.full_path(),
            selection_reader_tool.full_path(),
            close_window_tool.full_path(),
            window_ink_test_tool.full_path(),
            resize_test_tool.full_path(),
            key_test_tool.full_path(),
        ],
        depends: [
            selection_test_tool,
            selection_reader_tool,
            close_window_tool,
            window_ink_test_tool,
            resize_test_tool,
            key_test_tool,
            xterm_plus,
        ],
        timeout: 120,
        is_parallel: false,
    )
    test(
        'xvfb-pty-closed-stdio',
        find_program('sh'),
        args: [
            files('xvfb-pty-closed-stdio.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            python3.full_path(),
            files('pty-closed-stdio.py'),
            timeout_program.full_path(),
            get_option('b_sanitize') == 'none' ? null_fault_preload.full_path() : '',
        ],
        depends: [null_fault_preload, xterm_plus],
        timeout: 180,
        is_parallel: false,
    )
    test(
        'xvfb-sync-output',
        find_program('sh'),
        args: [
            files('xvfb-sync-output.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            python3.full_path(),
            files('sync-output.py'),
            resize_test_tool.full_path(),
            hover_test_tool.full_path(),
            # The target itself, not its path, so Meson builds it before this test.
            sync_output_paint_test_tool,
        ],
        timeout: 60,
        is_parallel: false,
        depends: [
            xterm_plus,
            resize_test_tool,
            hover_test_tool,
        ],
    )
    test(
        'xvfb-report-config',
        find_program('sh'),
        args: [
            files('xvfb-report-config.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            meson.project_source_root() / 'font-fixtures-stage',
        ],
        timeout: 30,
        is_parallel: false,
        depends: [xterm_plus],
    )
    test(
        'xvfb-welcome',
        find_program('sh'),
        args: [
            files('xvfb-welcome.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            meson.project_source_root() / 'font-fixtures-stage',
            xrdb.full_path(),
            compositor_owner_test_tool.full_path(),
        ],
        timeout: 30,
        is_parallel: false,
        depends: [xterm_plus, compositor_owner_test_tool],
    )
    if xprop.found()
        test(
            'xvfb-window-identity',
            find_program('sh'),
            args: [
                files('xvfb-window-identity.sh'),
                xvfb.full_path(),
                xterm_plus.full_path(),
                xprop.full_path(),
                python3.full_path(),
            ],
            timeout: 30,
            is_parallel: false,
            depends: [xterm_plus],
        )
    endif
    if xwininfo.found() and xprop.found()
        test(
            'xvfb-geometry',
            find_program('sh'),
            args: [
                files('xvfb-geometry.sh'),
                xvfb.full_path(),
                xterm_plus.full_path(),
                xwininfo.full_path(),
                xprop.full_path(),
                python3.full_path(),
            ],
            timeout: 60,
            is_parallel: false,
            depends: [xterm_plus],
        )
        test(
            'xvfb-command-line',
            find_program('sh'),
            args: [
                files('xvfb-command-line.sh'),
                xvfb.full_path(),
                xterm_plus.full_path(),
                xwininfo.full_path(),
                xprop.full_path(),
            ],
            timeout: 30,
            is_parallel: false,
            depends: [xterm_plus],
        )
    endif
    test(
        'xvfb-colors',
        find_program('sh'),
        args: [
            files('xvfb-colors.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            xrdb.full_path(),
            window_alpha_test_tool.full_path(),
            timeout_program.full_path(),
            hold_colors_test_tool.full_path(),
        ],
        timeout: 30,
        is_parallel: false,
        depends: [
            xterm_plus,
            window_alpha_test_tool,
            hold_colors_test_tool,
        ],
    )
    test(
        'xvfb-user-signals', find_program('sh'),
        args: [files('xvfb-user-signals.sh'), xvfb.full_path(), xterm_plus.full_path(), python3.full_path()],
        depends: [xterm_plus], timeout: 30, is_parallel: false,
    )
    if xprop.found()
        test(
            'xvfb-theme-reload',
            find_program('sh'),
            args: [
                files('xvfb-theme-reload.sh'),
                xvfb.full_path(),
                xterm_plus.full_path(),
                xrdb.full_path(),
                python3.full_path(),
            ],
            timeout: 30,
            is_parallel: false,
            depends: [xterm_plus],
        )
    endif
    test(
        'xvfb-opacity',
        find_program('sh'),
        args: [
            files('xvfb-opacity.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            compositor_owner_test_tool.full_path(),
            window_alpha_test_tool.full_path(),
            opacity_slider_test_tool.full_path(),
        ],
        timeout: 30,
        is_parallel: false,
        depends: [
            xterm_plus,
            compositor_owner_test_tool,
            window_alpha_test_tool,
            opacity_slider_test_tool,
        ],
    )
    test(
        'xvfb-font-baseline',
        find_program('sh'),
        args: [
            files('xvfb-font-baseline.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            window_ink_test_tool.full_path(),
            font_key_test_tool.full_path(),
            meson.project_source_root() / 'font-fixtures-stage',
        ],
        timeout: 30,
        is_parallel: false,
        depends: [
            xterm_plus,
            window_ink_test_tool,
            font_key_test_tool,
        ],
    )
    test(
        'xvfb-emoji-routing',
        find_program('sh'),
        args: [
            files('xvfb-emoji-routing.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            window_ink_test_tool.full_path(),
            meson.project_source_root() / 'font-fixtures-stage',
        ],
        timeout: 60,
        is_parallel: false,
        depends: [xterm_plus, window_ink_test_tool],
    )
    test(
        'xvfb-emoji-artwork',
        find_program('sh'),
        args: [
            files('xvfb-emoji-artwork.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            window_ink_test_tool.full_path(),
            meson.project_source_root() / 'font-fixtures-stage',
        ],
        timeout: 120,
        is_parallel: false,
        depends: [xterm_plus, window_ink_test_tool],
    )
    test(
        'xvfb-font-discovery',
        find_program('sh'),
        args: [
            files('xvfb-font-discovery.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            window_ink_test_tool.full_path(),
            meson.project_source_root() / 'font-fixtures-stage',
        ],
        timeout: 120,
        is_parallel: false,
        depends: [xterm_plus, window_ink_test_tool],
    )
    test(
        'xvfb-emoji-text-role',
        find_program('sh'),
        args: [
            files('xvfb-emoji-text-role.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            window_ink_test_tool.full_path(),
            font_key_test_tool.full_path(),
            meson.project_source_root() / 'font-fixtures-stage',
        ],
        timeout: 120,
        is_parallel: false,
        depends: [
            xterm_plus,
            window_ink_test_tool,
            font_key_test_tool,
        ],
    )
    test(
        'xvfb-text-shaping',
        find_program('sh'),
        args: [
            files('xvfb-text-shaping.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            window_ink_test_tool.full_path(),
            meson.project_source_root() / 'font-fixtures-stage',
        ],
        timeout: 30,
        is_parallel: false,
        depends: [xterm_plus, window_ink_test_tool],
    )
    test(
        'xvfb-font-user-fallback',
        find_program('sh'),
        args: [
            files('xvfb-font-user-fallback.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            meson.project_source_root() / 'font-fixtures-stage',
        ],
        timeout: 30,
        is_parallel: false,
        depends: [xterm_plus],
    )
    test(
        'xvfb-font-system-fallback',
        find_program('sh'),
        args: [
            files('xvfb-font-system-fallback.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            meson.project_source_root() / 'font-fixtures-stage',
        ],
        timeout: 30,
        is_parallel: false,
        depends: [xterm_plus],
    )
    test(
        'xvfb-font-han',
        find_program('sh'),
        args: [
            files('xvfb-font-han.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            meson.project_source_root() / 'font-fixtures-stage',
        ],
        timeout: 30,
        is_parallel: false,
        depends: [xterm_plus],
    )
    test(
        'xvfb-font-tofu',
        find_program('sh'),
        args: [
            files('xvfb-font-tofu.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            window_ink_test_tool.full_path(),
            meson.project_source_root() / 'font-fixtures-stage',
        ],
        timeout: 30,
        is_parallel: false,
        depends: [xterm_plus, window_ink_test_tool],
    )
    test(
        'xvfb-box-glyphs',
        find_program('sh'),
        args: [
            files('xvfb-box-glyphs.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            window_ink_test_tool.full_path(),
            window_ops_test_tool.full_path(),
            selection_test_tool.full_path(),
            key_test_tool.full_path(),
            meson.project_source_root() / 'font-fixtures-stage',
        ],
        timeout: 60,
        is_parallel: false,
        depends: [
            xterm_plus,
            window_ink_test_tool,
            window_ops_test_tool,
            selection_test_tool,
            key_test_tool,
        ],
    )
    test(
        'xvfb-font-wide-boundary',
        find_program('sh'),
        args: [
            files('xvfb-font-wide-boundary.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            meson.project_source_root() / 'font-fixtures-stage',
        ],
        timeout: 30,
        is_parallel: false,
        depends: [xterm_plus],
    )
    test(
        'xvfb-font-style-family',
        find_program('sh'),
        args: [
            files('xvfb-font-style-family.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            meson.project_source_root() / 'font-fixtures-stage',
        ],
        timeout: 30,
        is_parallel: false,
        depends: [xterm_plus],
    )
    test(
        'xvfb-font-route-cache',
        find_program('sh'),
        args: [
            files('xvfb-font-route-cache.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            meson.project_source_root() / 'font-fixtures-stage',
        ],
        timeout: 30,
        is_parallel: false,
        depends: [xterm_plus],
    )
    test(
        'xvfb-font-routing-report',
        find_program('sh'),
        args: [
            files('xvfb-font-routing-report.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            key_test_tool.full_path(),
            meson.project_source_root() / 'font-fixtures-stage',
            files('check-font-routing-report.py'),
        ],
        timeout: 30,
        is_parallel: false,
        depends: [xterm_plus, key_test_tool],
    )
    test(
        'xvfb-font-reload',
        find_program('sh'),
        args: [
            files('xvfb-font-reload.sh'),
            xvfb.full_path(),
            font_reload_test_tool.full_path(),
            font_reload_paint_test_tool.full_path(),
            meson.project_source_root() / 'font-fixtures-stage',
        ],
        timeout: 30,
        is_parallel: false,
        depends: [font_reload_test_tool, font_reload_paint_test_tool],
    )
    if xprop.found() and xrdb.found()
        test(
            't0-font-resolution',
            python3,
            args: [
                files('t0-font-resolution.py'),
                '--xvfb', xvfb.full_path(),
                '--xprop', xprop.full_path(),
                '--xrdb', xrdb.full_path(),
                '--xterm-plus', xterm_plus.full_path(),
                '--fixture', files('../compat/xterm-411-face-name.json'),
                '--fixture-root', meson.project_source_root() / 'font-fixtures-stage',
            ],
            timeout: 90,
            is_parallel: false,
            depends: [xterm_plus],
        )
    endif
    test(
        'xvfb-selection',
        find_program('sh'),
        args: [
            files('xvfb-selection.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            selection_test_tool.full_path(),
            selection_reader_tool.full_path(),
            shift_click_test_tool.full_path(),
        ],
        timeout: 30,
        is_parallel: false,
        depends: [
            xterm_plus,
            selection_test_tool,
            selection_reader_tool,
            shift_click_test_tool,
        ],
    )
    test(
        'xvfb-search',
        find_program('sh'),
        args: [
            files('xvfb-search.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            key_test_tool.full_path(),
            selection_reader_tool.full_path(),
            window_alpha_test_tool.full_path(),
            resize_test_tool.full_path(),
        ],
        timeout: 120,
        is_parallel: false,
        depends: [
            xterm_plus,
            key_test_tool,
            selection_reader_tool,
            window_alpha_test_tool,
            resize_test_tool,
        ],
    )
    test(
        'xvfb-copy-flash',
        find_program('sh'),
        args: [
            files('xvfb-copy-flash.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            window_alpha_test_tool.full_path(),
            selection_test_tool.full_path(),
            selection_reader_tool.full_path(),
            selection_owner_tool.full_path(),
            resize_test_tool.full_path(),
        ],
        timeout: 90,
        is_parallel: false,
        depends: [
            xterm_plus,
            window_alpha_test_tool,
            selection_test_tool,
            selection_reader_tool,
            selection_owner_tool,
            resize_test_tool,
        ],
    )
    test(
        'xvfb-clipboard',
        find_program('sh'),
        args: [
            files('xvfb-clipboard.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            python3.full_path(),
            files('clipboard-osc52.py'),
            selection_reader_tool.full_path(),
            selection_owner_tool.full_path(),
            selection_test_tool.full_path(),
            key_test_tool,
            files('clipboard-encoding.py'),
        ],
        # The owner and reader gained modes this suite depends on.
        depends: [
            selection_reader_tool,
            selection_owner_tool,
            xterm_plus,
            selection_test_tool,
        ],
        timeout: 120,
        is_parallel: false,
    )
    test(
        'xvfb-window-ops',
        find_program('sh'),
        args: [
            files('xvfb-window-ops.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            python3.full_path(),
            files('window-ops.py'),
            window_ops_test_tool.full_path(),
            selection_reader_tool.full_path(),
            xwininfo.full_path(),
            selection_owner_tool.full_path(),
        ],
        depends: [
            selection_reader_tool,
            selection_owner_tool,
            xterm_plus,
            window_ops_test_tool,
        ],
        timeout: 240,
        is_parallel: false,
    )
    test(
        'xvfb-title-stack',
        find_program('sh'),
        args: [
            files('xvfb-title-stack.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            python3.full_path(),
            files('title-stack.py'),
            xprop.full_path(),
            window_ops_test_tool.full_path(),
        ],
        timeout: 90,
        is_parallel: false,
        depends: [xterm_plus, window_ops_test_tool],
    )
    test(
        'xvfb-title-ops',
        find_program('sh'),
        args: [
            files('xvfb-title-ops.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            python3.full_path(),
            files('title-ops.py'),
            xprop.full_path(),
            window_ops_test_tool.full_path(),
            key_test_tool.full_path(),
            property_watch_tool.full_path(),
        ],
        depends: [
            key_test_tool,
            property_watch_tool,
            xterm_plus,
            window_ops_test_tool,
        ],
        timeout: 180,
        is_parallel: false,
    )
    test(
        'xvfb-term-name',
        find_program('sh'),
        args: [
            files('xvfb-term-name.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
        ],
        timeout: 60,
        is_parallel: false,
        depends: [xterm_plus],
    )
    test(
        'xvfb-working-directory',
        find_program('sh'),
        args: [
            files('xvfb-working-directory.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
        ],
        timeout: 60,
        is_parallel: false,
        depends: [xterm_plus],
    )
    test(
        'xvfb-pipe-output',
        find_program('sh'),
        args: [
            files('xvfb-pipe-output.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            key_test_tool.full_path(),
        ],
        timeout: 120,
        is_parallel: false,
        depends: [xterm_plus, key_test_tool],
    )
    test(
        'xvfb-prompt-navigation',
        find_program('sh'),
        args: [
            files('xvfb-prompt-navigation.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            key_test_tool.full_path(),
            window_ink_test_tool.full_path(),
            resize_test_tool.full_path(),
            urgency_test_tool.full_path(),
        ],
        timeout: 150,
        is_parallel: false,
        depends: [
            xterm_plus,
            key_test_tool,
            window_ink_test_tool,
            resize_test_tool,
            urgency_test_tool,
        ],
    )
    test(
        'xvfb-progress',
        find_program('sh'),
        args: [
            files('xvfb-progress.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            window_alpha_test_tool.full_path(),
            resize_test_tool.full_path(),
            xprop.full_path(),
            window_ops_test_tool.full_path(),
        ],
        timeout: 90,
        is_parallel: false,
        depends: [
            xterm_plus,
            window_alpha_test_tool,
            resize_test_tool,
            window_ops_test_tool,
        ],
    )
    if libnotify_dep.found()
        dbus_run_session = find_program('dbus-run-session', required: xvfb_tests_option)
        if dbus_run_session.found()
            test(
                'xvfb-desktop-notification',
                dbus_run_session,
                args: [
                    '--',
                    find_program('sh').full_path(),
                    files('xvfb-desktop-notification.sh'),
                    xvfb.full_path(),
                    xterm_plus.full_path(),
                    urgency_test_tool.full_path(),
                    fake_notifications_tool.full_path(),
                    'compiled',
                ],
                depends: [xterm_plus, urgency_test_tool, fake_notifications_tool],
                timeout: 120,
                is_parallel: false,
            )
        endif
    else
        test(
            'xvfb-desktop-notification',
            find_program('sh'),
            args: [
                files('xvfb-desktop-notification.sh'),
                xvfb.full_path(),
                xterm_plus.full_path(),
                urgency_test_tool.full_path(),
                'none',
                'absent',
            ],
            timeout: 60,
            is_parallel: false,
            depends: [xterm_plus, urgency_test_tool],
        )
    endif
    test(
        'xvfb-notification-urgency',
        find_program('sh'),
        args: [
            files('xvfb-notification-urgency.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            urgency_test_tool.full_path(),
        ],
        timeout: 90,
        is_parallel: false,
        depends: [xterm_plus, urgency_test_tool],
    )
    test(
        'xvfb-unknown-apc',
        find_program('sh'),
        args: [
            files('xvfb-unknown-apc.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
        ],
        timeout: 60,
        is_parallel: false,
        depends: [xterm_plus],
    )
    test(
        'xvfb-device-attributes',
        find_program('sh'),
        args: [
            files('xvfb-device-attributes.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            program_name,
            build_version,
        ],
        timeout: 60,
        is_parallel: false,
        depends: [xterm_plus],
    )
    test(
        'xvfb-answerback',
        find_program('sh'),
        args: [
            files('xvfb-answerback.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
        ],
        timeout: 60,
        is_parallel: false,
        depends: [xterm_plus],
    )
    test(
        'xvfb-cursor-shape',
        find_program('sh'),
        args: [
            files('xvfb-cursor-shape.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            window_alpha_test_tool.full_path(),
        ],
        timeout: 60,
        is_parallel: false,
        depends: [xterm_plus, window_alpha_test_tool],
    )
    test(
        'xvfb-underline-color',
        find_program('sh'),
        args: [
            files('xvfb-underline-color.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            window_alpha_test_tool.full_path(),
            selection_test_tool.full_path(),
            compositor_owner_test_tool.full_path(),
        ],
        timeout: 90,
        is_parallel: false,
        depends: [
            xterm_plus,
            window_alpha_test_tool,
            selection_test_tool,
            compositor_owner_test_tool,
        ],
    )
    test(
        'xvfb-dynamic-colors',
        find_program('sh'),
        args: [
            files('xvfb-dynamic-colors.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            python3.full_path(),
            files('dynamic-colors.py'),
            window_alpha_test_tool.full_path(),
            compositor_owner_test_tool.full_path(),
            opacity_slider_test_tool.full_path(),
        ],
        timeout: 90,
        is_parallel: false,
        depends: [
            xterm_plus,
            window_alpha_test_tool,
            compositor_owner_test_tool,
            opacity_slider_test_tool,
        ],
    )
    test(
        'xvfb-color-ops',
        find_program('sh'),
        args: [
            files('xvfb-color-ops.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            python3.full_path(),
            files('color-ops.py'),
            window_ops_test_tool.full_path(),
            key_test_tool.full_path(),
            window_ink_test_tool.full_path(),
        ],
        depends: [
            key_test_tool,
            window_ink_test_tool,
            xterm_plus,
            window_ops_test_tool,
        ],
        timeout: 240,
        is_parallel: false,
    )
    test(
        'xvfb-request-ops',
        find_program('sh'),
        args: [files('xvfb-request-ops.sh'), xvfb.full_path(),
               xterm_plus.full_path(), python3.full_path(),
               window_ops_test_tool.full_path(), selection_test_tool.full_path()],
        timeout: 90,
        is_parallel: false,
        depends: [
            xterm_plus,
            window_ops_test_tool,
            selection_test_tool,
        ],
    )
    if xfixes_dep.found()
        test(
            'xvfb-cursor-modes',
            find_program('sh'),
            args: [files('xvfb-cursor-modes.sh'), xvfb.full_path(),
                   xterm_plus.full_path(), xrdb.full_path(), python3.full_path()],
            timeout: 90,
            is_parallel: false,
            depends: [xterm_plus],
        )
        test(
            'xvfb-pointer-shape',
            find_program('sh'),
            args: [files('xvfb-pointer-shape.sh'), xvfb.full_path(),
                   pointer_shape_test_tool, files('xvfb-test-lib.sh')],
            timeout: 30,
            is_parallel: false,
        )
    endif
    test(
        'xvfb-hyperlink-hover',
        find_program('sh'),
        args: [
            files('xvfb-hyperlink-hover.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            hover_test_tool.full_path(),
            files('xvfb-test-lib.sh'),
        ],
        timeout: 30,
        is_parallel: false,
        depends: [xterm_plus, hover_test_tool],
    )
    test(
        'xvfb-keyboard',
        find_program('sh'),
        args: [
            files('xvfb-keyboard.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            key_test_tool.full_path(),
        ],
        timeout: 30,
        is_parallel: false,
        depends: [xterm_plus, key_test_tool],
    )
    test(
        'xvfb-kitty-keyboard',
        find_program('sh'),
        args: [
            files('xvfb-kitty-keyboard.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            key_test_tool.full_path(),
        ],
        timeout: 30,
        is_parallel: false,
        depends: [xterm_plus, key_test_tool],
    )
    test(
        'xvfb-keyboard-matrix',
        find_program('sh'),
        args: [
            files('xvfb-keyboard-matrix.sh'),
            xvfb.full_path(),
            xterm_plus.full_path(),
            key_test_tool.full_path(),
        ],
        timeout: 30,
        is_parallel: false,
        depends: [xterm_plus, key_test_tool],
    )
endif
