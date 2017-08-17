subdir('appdata')
subdir('figures')
subdir('icons')
subdir('targets')
subdir('ti1')

desktop_files = [
  'org.gnome.ColorProfileViewer.desktop',
  'gcm-picker.desktop',
  'gcm-calibrate.desktop',
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
