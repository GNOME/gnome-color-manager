docbook2man = find_program('docbook2man', required : false)
if docbook2man.found()
  custom_target('gcm-import-man',
    output : 'gcm-import.1',
    input : 'gcm-import.sgml',
    command : [docbook2man, '@INPUT@', '--output', 'man'],
    install : true,
    install_dir : join_paths(prefixed_mandir, 'man1'),
  )
  custom_target('gcm-inspect-man',
    output : 'gcm-inspect.1',
    input : 'gcm-inspect.sgml',
    command : [docbook2man, '@INPUT@', '--output', 'man'],
    install : true,
    install_dir : join_paths(prefixed_mandir, 'man1'),
  )
  custom_target('gcm-picker-man',
    output : 'gcm-picker.1',
    input : 'gcm-picker.sgml',
    command : [docbook2man, '@INPUT@', '--output', 'man'],
    install : true,
    install_dir : join_paths(prefixed_mandir, 'man1'),
  )
  custom_target('gcm-viewer-man',
    output : 'gcm-viewer.1',
    input : 'gcm-viewer.sgml',
    command : [docbook2man, '@INPUT@', '--output', 'man'],
    install : true,
    install_dir : join_paths(prefixed_mandir, 'man1'),
  )
endif
