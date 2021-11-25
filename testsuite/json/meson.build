testexecdir = join_paths(installed_test_bindir, 'json')
testdatadir = join_paths(installed_test_datadir, 'json')

test_parser = executable('test-json-parser',
                         'test-json-parser.c',
                         '../testutils.c',
                         c_args: common_cflags,
                         include_directories: [confinc],
                         install: get_option('install-tests'),
                         install_dir: testexecdir,
                         link_with: [ libgtk_json ],
                         dependencies: libgtk_json_dep)

test_data = [
]

foreach testname : test_data
  if testname.endswith('.json') and not testname.endswith('.ref.json')
    test('parser ' + testname, test_parser,
         args: [ '--tap',
                 '-k',
                 join_paths(meson.current_source_dir(), testname),
               ],
         protocol: 'tap',
         env: [
                'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
                'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir())
              ],
         suite: 'json')
  endif
endforeach

if get_option('install-tests')
  conf = configuration_data()
  conf.set('libexecdir', gtk_libexecdir)
  configure_file(input: 'parser.test.in',
                 output: 'parser.test',
                 configuration: conf,
                 install_dir: testdatadir)

  install_data(test_data, install_dir: testexecdir)

endif
