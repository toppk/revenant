project(
    'xterm-plus',
    'c',
    version: '0.9.0-dev',
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

build_version = get_option('release-version')
if build_version == ''
    build_version = meson.project_version()
endif
program_name = 'revenant'
support_url = 'https://github.com/toppk/revenant/issues'
ghostty_reference = 'none'
if build_with_ghostty
    git = find_program('git', required: false)
    if git.found() and fs.exists(ghostty_source_dir / '.git')
        ghostty_head = run_command(
            git,
            '-C', ghostty_source_dir,
            'rev-parse', 'HEAD',
            check: false,
        )
        if ghostty_head.returncode() == 0 and ghostty_head.stdout().strip() != ''
            ghostty_reference = ghostty_head.stdout().strip()
            ghostty_changes = run_command(
                git,
                '-C', ghostty_source_dir,
                'status', '--porcelain', '--untracked-files=no',
                check: false,
            )
            if ghostty_changes.returncode() == 0 and ghostty_changes.stdout().strip() != ''
                ghostty_reference += '-dirty'
            endif
        endif
    endif
    if ghostty_reference == 'none'
        ghostty_reference = run_command(
            find_program('tools/fetch-libghostty'),
            '--print-reference',
            check: true,
        ).stdout().strip()
    endif
endif

version_configuration = configuration_data()
version_configuration.set_quoted('XTP_VERSION', build_version)
version_configuration.set_quoted('XTP_PROGRAM_NAME', program_name)
version_configuration.set_quoted('XTP_SUPPORT_URL', support_url)
version_configuration.set_quoted('XTP_GHOSTTY_REVISION', ghostty_reference)
configure_file(
    output: 'version.h',
    configuration: version_configuration,
)

feature_configuration = configuration_data()
libnotify_dep = dependency('libnotify', required: get_option('libnotify'),
                           include_type: 'system')
if libnotify_dep.found()
    feature_configuration.set('HAVE_LIBNOTIFY', 1)
endif
configure_file(
    output: 'config.h',
    configuration: feature_configuration,
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
xcursor_dep = dependency('xcursor')
cairo_dep = dependency('cairo', version: '>= 1.18')
cairo_ft_dep = dependency('cairo-ft', version: '>= 1.18')
cairo_xlib_dep = dependency('cairo-xlib', version: '>= 1.18')
harfbuzz_dep = dependency('harfbuzz')
util_dep = cc.find_library('util')
m_dep = cc.find_library('m')

widget_dependencies = [
    x11_dep,
    xt_dep,
    xaw_dep,
    xmu_dep,
    fontconfig_dep,
    xft_dep,
    xrender_dep,
    xcursor_dep,
    cairo_dep,
    cairo_ft_dep,
    cairo_xlib_dep,
    harfbuzz_dep,
]
program_dependencies = widget_dependencies + [util_dep, m_dep]
if libnotify_dep.found()
    program_dependencies += libnotify_dep
endif

widget_sources = files(
    'src/box_glyphs.c',
    'src/procedural_glyphs.c',
    'src/char_class.c',
    'src/terminal_control_observer.c',
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
    'src/notification_policy.c',
    'src/search_match.c',
    'src/sme_slider.c',
    'src/terminal.c',
    'src/title_encoding.c',
    'src/title_stack.c',
    'src/unicode_script.c',
    'src/url_match.c',
    'src/window_ops.c',
    'src/working_directory.c',
    'src/device_attributes.c',
    'src/urgency.c',
    'src/pipe_command.c',
    'src/ops_list.c',
    'src/color_ops.c',
    'src/request_ops.c',
    'src/utf8.c',
    'src/vt_font.c',
    'src/vt_draw.c',
    'src/vt_input.c',
    'src/vt_interaction.c',
    'src/vt_progress.c',
    'src/vt_search.c',
    'src/vt_widget.c',
    'src/x11_opacity.c',
)

sources = files(
    'src/command_options.c',
    'src/config_report.c',
    'src/desktop_notification.c',
    'src/main.c',
    'src/pty.c',
    'src/selftest.c',
    'src/welcome.c',
)
sources += widget_sources

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

    terminal_sources += files(
        'src/terminal_ghostty.c',
        'src/terminal_ghostty_search.c',
        'src/terminal_ghostty_selection.c',
    )
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
    dependencies: program_dependencies,
    c_args: terminal_compile_args,
    install: true,
)

install_symlink('xterm+', install_dir: get_option('bindir'), pointing_to: program_name)

pandoc = find_program('pandoc', required: get_option('man'))
if pandoc.found()
    man_page = custom_target(
        'revenant.1',
        input: 'man/revenant.1.md',
        output: 'revenant.1',
        command: [
            python3,
            files('tools/build-man'),
            '@INPUT@',
            '@OUTPUT@',
            pandoc,
            program_name + ' ' + build_version,
        ],
        depend_files: files(
            'compat/manual-resources.tsv',
            'compat/manual-actions.tsv',
            'data/app-defaults/XTerm',
            'src/command_options.c',
            'src/menus.c',
            'src/vt_widget.c',
            'tools/generate-doc-fragments',
        ),
        install: true,
        install_dir: get_option('mandir') / 'man1',
    )
endif
install_data('data/revenant.desktop', install_dir: get_option('datadir') / 'applications')
install_data('data/icons/revenant.svg',
    install_dir: get_option('datadir') / 'icons/hicolor/scalable/apps')
install_data('data/icons/revenant-256.png', rename: 'revenant.png',
    install_dir: get_option('datadir') / 'icons/hicolor/256x256/apps')

subdir('tests')

summary(
    {
        'executable': program_name,
        'X resource class': 'XTerm',
        'X resource instance': 'xterm',
        'libghostty-vt': build_with_ghostty,
        'libghostty source': ghostty_source_dir,
        'desktop notifications (libnotify)': libnotify_dep.found(),
        'profiling frame pointers': get_option('profiling'),
        'font-key test tool': font_key_test_tool.full_path(),
        'key test tool': key_test_tool.full_path(),
        'selection test tool': selection_test_tool.full_path(),
        'selection reader tool': selection_reader_tool.full_path(),
        'Shift-click test tool': shift_click_test_tool.full_path(),
        'hover test tool': hover_test_tool.full_path(),
        'Xvfb selection test': build_with_ghostty and xvfb.found(),
        'Xvfb opacity test': build_with_ghostty and xvfb.found(),
        'resize test tool': resize_test_tool.full_path(),
        'wheel test tool': wheel_test_tool.full_path(),
    },
    section: 'xterm+ configuration',
)
