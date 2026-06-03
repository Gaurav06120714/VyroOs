project('hello-vyro', 'c',
  version : '0.1.0',
  license : 'MIT',
  default_options : ['warning_level=2', 'c_std=c11'])

gtk_dep = dependency('gtk4', version : '>=4.10')

executable('hello-vyro',
  'src/main.c',
  dependencies : [gtk_dep],
  install : true)

install_data('data/os.vyro.HelloVyro.desktop',
  install_dir : get_option('datadir') / 'applications')
