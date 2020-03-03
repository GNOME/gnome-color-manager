
icon_sizes = ['16x16', '22x22', '24x24', '32x32', '48x48', '64x64', '256x256']
foreach size: icon_sizes
  install_data([
    size + '/gnome-color-manager.png',
    ],
    install_dir : join_paths(prefixed_datadir, 'icons/hicolor/' + size + '/apps'),
  )
endforeach


install_data([
  'scalable/gnome-color-manager.svg',
  ],
  install_dir : join_paths(prefixed_datadir, 'icons/hicolor/scalable/apps')
)
