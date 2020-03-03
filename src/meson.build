cargs = ['-DG_LOG_DOMAIN="GnomeColorManager"']

gcm_picker_resources = gnome.compile_resources(
  'gcm-picker-resources',
  'gcm-picker.gresource.xml',
  source_dir : '.',
  c_name : 'gcm',
)

gcm_viewer_resources = gnome.compile_resources(
  'gcm-viewer-resources',
  'gcm-viewer.gresource.xml',
  source_dir : '.',
  c_name : 'gcm',
)

shared_srcs = [
  'gcm-cie-widget.c',
  'gcm-debug.c',
  'gcm-trc-widget.c',
  'gcm-utils.c',
]

executable(
  'gcm-inspect',
  sources : [
    'gcm-inspect.c',
    shared_srcs
  ],
  include_directories : [
    include_directories('..'),
  ],
  dependencies : [
    libcolord,
    libm,
    libgio,
    libgtk,
  ],
  c_args : cargs,
  install : true,
)

executable(
  'gcm-import',
  sources : [
    'gcm-import.c',
    shared_srcs
  ],
  include_directories : [
    include_directories('..'),
  ],
  dependencies : [
    libcolord,
    libm,
    libgio,
    libgtk,
  ],
  c_args : cargs,
  install : true,
)

executable(
  'gcm-picker',
  gcm_picker_resources,
  sources : [
    'gcm-picker.c',
    shared_srcs
  ],
  include_directories : [
    include_directories('..'),
  ],
  dependencies : [
    liblcms,
    libcolord,
    libm,
    libgio,
    libgtk,
  ],
  c_args : cargs,
  install : true,
)

executable(
  'gcm-viewer',
  gcm_viewer_resources,
  sources : [
    'gcm-cell-renderer-profile-text.c',
    'gcm-cell-renderer-color.c',
    'gcm-viewer.c',
    shared_srcs
  ],
  include_directories : [
    include_directories('..'),
  ],
  dependencies : [
    libcolord,
    libm,
    liblcms,
    libgio,
    libgtk,
  ],
  c_args : cargs,
  install : true,
)

if get_option('tests')
  cargs += ['-DTESTDATADIR="' + join_paths(meson.current_source_dir(), '..', 'data', 'tests') + '"']
  e = executable(
    'gcm-self-test',
    sources : [
      shared_srcs,
      'gcm-gamma-widget.c',
      'gcm-self-test.c',
    ],
    include_directories : [
      include_directories('..'),
    ],
    dependencies : [
      libcolord,
      libgio,
      libgtk,
      libm,
    ],
    c_args : cargs
  )
  test('gcm-self-test', e)
endif
