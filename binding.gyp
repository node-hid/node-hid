{
  'targets': [
    {
      'target_name': 'hidapi',
      'type': 'static_library',
      'target_arch': 'ia32',
      'conditions': [
        [ 'OS=="mac"', {
          'sources': [ 'hidapi/mac/hid.c' ],
          'include_dirs+': [
            '/usr/include/libusb-1.0/'
          ],
        }],
        [ 'OS=="linux"', {
          'sources': [ 'hidapi/linux/hid-libusb.c' ],
          'include_dirs+': [
            '/usr/include/libusb-1.0/'
          ],
        }],
        [ 'OS=="win"', {
          'sources': [ 'hidapi/windows/hid.c' ],
          'msvs_settings': {
            'VCLinkerTool': {
              'AdditionalDependencies': [
                'setupapi.lib',
              ]
            }
          }
        }]
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'hidapi/hidapi'
        ]
      },
      'include_dirs': [
        'hidapi/hidapi'
      ],
      'defines': [
        '_LARGEFILE_SOURCE',
        '_FILE_OFFSET_BITS=64',
      ],
      'cflags': ['-g', '-Wall', '-fPIC']
    },
    {
      'target_name': 'HID',
      'target_arch': 'ia32',
      'sources': [ 'src/HID.cc' ],
      'dependencies': ['hidapi'],
      'defines': [
        '_LARGEFILE_SOURCE',
        '_FILE_OFFSET_BITS=64',
      ],
      'conditions': [
        [ 'OS=="win"', {
          'msvs_settings': {
            'VCLinkerTool': {
              'AdditionalDependencies': [
                'setupapi.lib',
              ]
            }
          }
        }]
      ],
      'cflags': ['-g', '-Wall', '-fPIC']
    }
  ]
}