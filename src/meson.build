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

if get_option('exiv')
  executable(
    'gcm-helper-exiv',
    sources : [
      'gcm-helper-exiv.cpp',
    ],
    include_directories : [
      include_directories('..'),
    ],
    dependencies : [
      libgio,
      libexiv,
    ],
    c_args : cargs,
    install : true,
    install_dir : prefixed_libexecdir,
  )
endif

shared_srcs = [
  'gcm-cie-widget.c',
  'gcm-debug.c',
  'gcm-exif.c',
  'gcm-print.c',
  'gcm-trc-widget.c',
  'gcm-utils.c',
]

executable(
  'gcm-calibrate',
  sources : [
    'gcm-brightness.c',
    'gcm-brightness.h',
    'gcm-calibrate-argyll.c',
    'gcm-calibrate-argyll.h',
    'gcm-calibrate.c',
    'gcm-calibrate.h',
    'gcm-cell-renderer-profile-text.c',
    'gcm-cell-renderer-profile-text.h',
    'gcm-gamma-widget.c',
    'gcm-gamma-widget.h',
    'gcm-calibrate-main.c',
    shared_srcs
  ],
  include_directories : [
    include_directories('..'),
  ],
  dependencies : [
    libcanberra,
    libtiff,
    libcolord_gtk,
    libm,
    libexif,
    libgio,
    libgtk,
    liblcms,
    libvte,
  ],
  c_args : cargs,
  install : true,
)

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
    libtiff,
    libcolord_gtk,
    libm,
    libexif,
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
    libtiff,
    libcolord_gtk,
    libm,
    libexif,
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
    libtiff,
    liblcms,
    libcolord_gtk,
    libm,
    libexif,
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
    libcanberra,
    libtiff,
    libcolord_gtk,
    libm,
    liblcms,
    libexif,
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
      'gcm-brightness.c',
      'gcm-calibrate-argyll.c',
      'gcm-calibrate.c',
      'gcm-gamma-widget.c',
      'gcm-self-test.c',
    ],
    include_directories : [
      include_directories('..'),
    ],
    dependencies : [
      libcolord_gtk,
      libexif,
      libgio,
      libgtk,
      libcanberra,
      libexif,
      libm,
      libtiff,
      libvte,
    ],
    c_args : cargs
  )
  test('gcm-self-test', e)
endif
