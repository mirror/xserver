simple_xinit = executable(
    'simple-xinit',
    'simple-xinit.c',
    include_directories: inc,
)

piglit_env = environment()
piglit_env.set('XSERVER_DIR', meson.source_root())
piglit_env.set('XSERVER_BUILDDIR', meson.build_root())

some_ops = ' -o clear,src,dst,over,xor,disjointover'
rendercheck_tests = [
    ['blend/All/a8r8g8b8', '-t blend -f a8r8g8b8'],
    ['blend/All/x8r8g8b8', '-t blend -f a8r8g8b8,x8r8g8b8'],
    ['blend/All/a2r10g10b10', '-t blend -f a8r8g8b8,a2r10g10b10'],
    ['blend/Clear', '-t blend -o clear'],
    ['blend/Src', '-t blend -o src'],
    ['blend/Over', '-t blend -o over'],
    ['composite/Some/a8r8g8b8', '-t composite -f a8r8g8b8' + some_ops],
    ['composite/Some/x8r8g8b8', '-t composite -f a8r8g8b8,x8r8g8b8' + some_ops],
    ['composite/Some/a2r10g10b10', '-t composite -f a8r8g8b8,a2r10g10b10' + some_ops],
    ['ca composite/Some/a8r8g8b8', '-t cacomposite -f a8r8g8b8' + some_ops],
    ['ca composite/Some/a8', '-t cacomposite -f a8r8g8b8,a8' + some_ops],
    ['ca composite/Some/x8r8g8b8', '-t cacomposite -f a8r8g8b8,x8r8g8b8' + some_ops],
    ['ca composite/Some/a2r10g10b10', '-t cacomposite -f a8r8g8b8,a2r10g10b10' + some_ops],
    ['fill', '-t fill'],
    ['bug7366', '-t bug7366'],
    ['destination coordinates', '-t dcoords'],
    ['source coordinates', '-t scoords'],
    ['mask coordinates', '-t mcoords'],
    ['translated source coordinates', '-t tscoords'],
    ['translated mask coordinates', '-t tmcoords'],
    ['triangles', '-t triangles'],
    ['LibreOffice xRGB', '-t libreoffice_xrgb'],
    ['GTK ARGB vs xBGR', '-t gtk_argb_xbgr'],
]

rendercheck = find_program('rendercheck', required:false)

if get_option('xvfb')
    xvfb_args = [
        xvfb_server.full_path(),
        '-screen',
        'scrn',
        '1280x1024x24',
    ]

    test('XTS',
        find_program('scripts/xvfb-piglit.sh'),
        env: piglit_env,
        timeout: 1200,
        suite: 'xvfb'
    )

    if rendercheck.found()
        foreach rctest: rendercheck_tests
            test(rctest[0],
                 simple_xinit,
                 # Use full_path so people can copy and paste the
                 # command line from testlog.txt easily.
                 args: [
                     rendercheck.path(),
                     rctest[1].split(' '),
                     '--',
                     xvfb_args,
                 ],
                 suite: 'xvfb'
                )
        endforeach
    endif
endif

if build_xwayland
    xwayland_args = [
        xwayland_server.full_path(),
    ]

    test('XTS',
        find_program('scripts/xwayland-piglit.sh'),
        env: piglit_env,
        timeout: 1200,
        suite: 'xwayland'
    )
endif

subdir('bigreq')
subdir('damage')
subdir('sync')
