subdir('appdata')
subdir('figures')
subdir('icons')

desktop_files = [
  'org.gnome.ColorProfileViewer.desktop',
  'gcm-picker.desktop',
  'gcm-import.desktop',
]
foreach desktop_file: desktop_files
  i18n.merge_file(
    input: desktop_file + '.in',
    output: desktop_file,
    type: 'desktop',
    po_dir: join_paths(meson.source_root(), 'po'),
    install: true,
    install_dir: join_paths(prefixed_datadir, 'applications')
  )
endforeach

dbusconf = configuration_data()
dbusconf.set('bindir', prefixed_bindir)
configure_file(
  input: 'org.gnome.ColorProfileViewer.service.in',
  output: 'org.gnome.ColorProfileViewer.service',
  configuration: dbusconf,
  install: true,
  install_dir: join_paths(prefixed_datadir, 'dbus-1', 'services'),
)
