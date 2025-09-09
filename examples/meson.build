top_incdir = include_directories('..')

executable('run-offline',
  ['run-offline.cpp', 'wav_io.cpp'],
  install: false,
  include_directories: top_incdir,
  dependencies: [audio_processing_dep, absl_dep]
)
