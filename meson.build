project(
    'xterm-plus',
    'c',
    version: '0.1.0',
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

version_configuration = configuration_data()
version_configuration.set_quoted('XTP_VERSION', meson.project_version())
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
x11_xcb_dep = dependency('x11-xcb')
fontconfig_dep = dependency('fontconfig')
xft_dep = dependency('xft')
util_dep = cc.find_library('util')
m_dep = cc.find_library('m')

sources = files(
    'src/config_report.c',
    'src/diagnostics.c',
    'src/main.c',
    'src/menus.c',
    'src/pty.c',
    'src/terminal.c',
    'src/vt_widget.c',
)

xterm_resource_catalog = custom_target(
    'xterm-410-resource-catalog',
    input: [
        'compat/xterm-410-resources.tsv',
        'compat/xterm-410-actions.txt',
        'compat/xterm-410-app-defaults.txt',
    ],
    output: 'xterm_410_resource_catalog.c',
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
    'xterm+',
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
        x11_xcb_dep,
        fontconfig_dep,
        xft_dep,
        util_dep,
        m_dep,
    ],
    c_args: terminal_compile_args,
    install: true,
)

font_key_test_tool = executable(
    'xtp-send-font-keys',
    'tests/xtp-send-font-keys.c',
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

resize_test_tool = executable(
    'xtp-resize-loop',
    'tests/xtp-resize-loop.c',
    dependencies: x11_dep,
    install: false,
)

test(
    'self-test',
    xterm_plus,
    args: ['--self-test'],
)

summary(
    {
        'executable': 'xterm+',
        'X resource class': 'XTerm',
        'X resource instance': 'xterm',
        'libghostty-vt': build_with_ghostty,
        'libghostty source': ghostty_source_dir,
        'profiling frame pointers': get_option('profiling'),
        'font-key test tool': font_key_test_tool.full_path(),
        'selection test tool': selection_test_tool.full_path(),
        'resize test tool': resize_test_tool.full_path(),
        'wheel test tool': wheel_test_tool.full_path(),
    },
    section: 'xterm+ configuration',
)
