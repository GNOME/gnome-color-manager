project('gnome-color-manager', 'c', 'cpp',
  version : '3.28.0',
  license : 'GPL-2.0+',
  default_options : ['warning_level=1'],
  meson_version : '>=0.37.0'
)

conf = configuration_data()
conf.set_quoted('PACKAGE_NAME', meson.project_name())
conf.set_quoted('PACKAGE_VERSION', meson.project_version())

# get suported warning flags
test_args = [
  '-fstack-protector-strong',
  '-Waggregate-return',
  '-Warray-bounds',
  '-Wcast-align',
  '-Wclobbered',
  '-Wdeclaration-after-statement',
  '-Wempty-body',
  '-Wextra',
  '-Wformat=2',
  '-Wformat-nonliteral',
  '-Wformat-security',
  '-Wformat-signedness',
  '-Wignored-qualifiers',
  '-Wimplicit-function-declaration',
  '-Winit-self',
  '-Winline',
  '-Wmissing-declarations',
  '-Wmissing-format-attribute',
  '-Wmissing-include-dirs',
  '-Wmissing-noreturn',
  '-Wmissing-parameter-type',
  '-Wmissing-prototypes',
  '-Wnested-externs',
  '-Wno-discarded-qualifiers',
  '-Wno-missing-field-initializers',
  '-Wno-strict-aliasing',
  '-Wno-suggest-attribute=format',
  '-Wno-unused-parameter',
  '-Wold-style-definition',
  '-Woverride-init',
  '-Wpacked',
  '-Wpointer-arith',
  '-Wredundant-decls',
  '-Wreturn-type',
  '-Wshadow',
  '-Wsign-compare',
  '-Wstrict-aliasing',
  '-Wstrict-prototypes',
  '-Wswitch-default',
  '-Wtype-limits',
  '-Wundef',
  '-Wuninitialized',
  '-Wunused-but-set-variable',
  '-Wwrite-strings'
]
cc = meson.get_compiler('c')
foreach arg: test_args
  if cc.has_argument(arg)
    add_project_arguments(arg, language : 'c')
  endif
endforeach

# enable full RELRO where possible
# FIXME: until https://github.com/mesonbuild/meson/issues/1140 is fixed
global_link_args = []
test_link_args = [
  '-Wl,-z,relro',
  '-Wl,-z,now',
]
foreach arg: test_link_args
  if cc.has_argument(arg)
    global_link_args += arg
  endif
endforeach
add_global_link_arguments(
  global_link_args,
  language: 'c'
)

prefixed_bindir = join_paths(get_option('prefix'), get_option('bindir'))
prefixed_localedir = join_paths(get_option('prefix'), get_option('localedir'))
prefixed_libexecdir = join_paths(get_option('prefix'), get_option('libexecdir'))
prefixed_mandir = join_paths(get_option('prefix'), get_option('mandir'))
prefixed_datadir = join_paths(get_option('prefix'), get_option('datadir'))
prefixed_pkgdatadir = join_paths(get_option('prefix'), get_option('datadir'), 'gnome-color-manager')

libgio = dependency('gio-2.0', version : '>= 2.25.9')
libgtk = dependency('gtk+-3.0', version : '>= 2.91.0')
libexif = dependency('libexif')
libtiff = dependency('libtiff-4')
colord = dependency('colord', version : '>= 1.3.1')
libcolord_gtk = dependency('colord-gtk', version : '>= 0.1.20')
libcanberra = dependency('libcanberra-gtk3', version : '>= 0.10')
libm = cc.find_library('libm', required: false)
liblcms = dependency('lcms2', version : '>= 2.2')

libvte = dependency('vte-2.91', version : '>= 0.25.1', required : false)
if libvte.found()
  conf.set('HAVE_VTE', '1')
endif

if get_option('exiv')
  libexiv = dependency('exiv2')
  conf.set('HAVE_EXIV', '1')
endif

if get_option('packagekit')
  libexiv = dependency('exiv2')
  conf.set('HAVE_PACKAGEKIT', '1')
endif

gnome = import('gnome')
i18n = import('i18n')

conf.set_quoted('GETTEXT_PACKAGE', meson.project_name())
conf.set_quoted('LOCALEDIR', prefixed_localedir)
conf.set_quoted('DATADIR', prefixed_datadir)
conf.set_quoted('PKGDATADIR', prefixed_pkgdatadir)
conf.set_quoted('BINDIR', prefixed_bindir)
conf.set_quoted('LIBEXECDIR', prefixed_libexecdir)
configure_file(
  output : 'config.h',
  configuration : conf
)

subdir('data')
subdir('help')
subdir('man')
subdir('po')
subdir('src')

# FIXME: remove when https://github.com/mesonbuild/meson/issues/837 fixed
meson.add_install_script('meson_post_install.sh')
