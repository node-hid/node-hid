{
    'variables': {
        'driver%': 'libusb',
        'node_hid_no_pkg_config%': '0'
    },
    'targets': [
        {
            'target_name': 'HID',
            'sources': [ 'src/HID.cc' ],
            'dependencies': ['hidapi'],
            'defines': [
                '_LARGEFILE_SOURCE',
                '_FILE_OFFSET_BITS=64',
            ],
            'conditions': [
                [ 'OS=="mac"', {
                    'LDFLAGS': [
                        '-framework IOKit',
                        '-framework CoreFoundation',
                        '-framework AppKit'
                    ],
                    'xcode_settings': {
                        'CLANG_CXX_LIBRARY': 'libc++',
                        'MACOSX_DEPLOYMENT_TARGET': '10.9',
                        'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
                        'OTHER_LDFLAGS': [
                            '-framework IOKit',
                            '-framework CoreFoundation',
                            '-framework AppKit'
                        ],
                    }
                }], # OS==mac
                [ 'OS=="linux"', {
                    'conditions': [
                        [ 'driver=="libusb"', {
                            'libraries': ['-lusb-1.0']
                        }],
                        [ 'driver=="hidraw"', {
                            'libraries': ['-ludev','-lusb-1.0']
                        }]
                    ],
                }], # OS==linux
                [ 'OS=="freebsd"', {
                  'libraries': ['-lusb']
                }], # OS==freebsd
                [ 'OS=="win"', {
                    'msvs_settings': {
                        'VCCLCompilerTool': {
                            'ExceptionHandling': '2', # /EHsc
                            'DisableSpecificWarnings': [ '4290', '4530', '4267' ],
                        },
                        'VCLinkerTool': {
                            'AdditionalDependencies': ['setupapi.lib']
                        }
                    }
                }] # OS==win
            ],
            'cflags!': ['-ansi', '-fno-exceptions' ],
            'cflags_cc!': [ '-fno-exceptions' ],
            'cflags': ['-g', '-exceptions'],
            'cflags_cc': ['-g', '-exceptions']
        }, # target HID

        {
            'target_name': 'hidapi',
            'type': 'static_library',
            'conditions': [
                [ 'OS=="mac"', {
                    'sources': [ 'hidapi/mac/hid.c' ],
                    'include_dirs+': ['/usr/include/libusb-1.0/'],
                    'xcode_settings': {
                        'OTHER_CFLAGS': ['-Wno-sign-compare'],
                        'MACOSX_DEPLOYMENT_TARGET': '10.9',
                    }
                }],
                [ 'OS=="freebsd"', {
                    'sources': [ 'hidapi/libusb/hid.c' ],
                }],
                [ 'OS=="linux"', {
                    'conditions': [
                        [ 'driver=="libusb"', {
                            'sources': [ 'hidapi/libusb/hid.c' ],
                            'conditions': [
                                ['node_hid_no_pkg_config != 1', {
                                    'include_dirs+': ['<!@(pkg-config libusb-1.0 --cflags-only-I | sed s/-I//g)']
                                }]
                            ]
                        }],
                        [ 'driver=="hidraw"', {
                            'sources': [ 'hidapi/linux/hid.c' ]
                        }]
                    ]
                }],
                [ 'OS=="win"', {
                    'sources': [ 'hidapi/windows/hid.c' ],
                    'msvs_settings': {
                        'VCCLCompilerTool': {
                            'ExceptionHandling': '2', # /EHsc
                            'DisableSpecificWarnings': [ '4290', '4530', '4267' ],
                        },
                        'VCLinkerTool': {
                            'AdditionalDependencies': ['setupapi.lib']
                        }
                    }
                }]
            ],
            'direct_dependent_settings': {
                'include_dirs': [
                    'hidapi/hidapi',
                    "<!@(node -p \"require('node-addon-api').include\")"
                ]
            },
            'include_dirs': ['hidapi/hidapi'],
            'defines': [
                '_LARGEFILE_SOURCE',
                '_FILE_OFFSET_BITS=64',
            ],
            'cflags': ['-g'],
            'cflags!': ['-ansi']
        }, # target hidapi

    ],
    'conditions': [
        [ 'OS=="linux"', {
            'targets': [
                {
                    'target_name': 'HID_hidraw',
                    'sources': [ 'src/HID.cc' ],
                    'dependencies': ['hidapi-linux-hidraw'],
                    'defines': [
                        '_LARGEFILE_SOURCE',
                        '_FILE_OFFSET_BITS=64',
                    ],
                    'libraries': [
                        '-ludev',
                        '-lusb-1.0'
                    ],
                    'cflags!': ['-ansi', '-fno-exceptions' ],
                    'cflags_cc!': [ '-fno-exceptions' ],
                    'cflags': ['-g', '-exceptions'],
                    'cflags_cc': ['-g', '-exceptions']
                }, # target 'HID-hidraw'

                {
                    'target_name': 'hidapi-linux-hidraw',
                    'type': 'static_library',
                    'sources': [ 'hidapi/linux/hid.c' ],
                    'direct_dependent_settings': {
                        'include_dirs': [
                            'hidapi/hidapi',
                            "<!@(node -p \"require('node-addon-api').include\")"
                        ]
                    },
                    'include_dirs': ['hidapi/hidapi' ],
                    'defines': [
                        '_LARGEFILE_SOURCE',
                        '_FILE_OFFSET_BITS=64',
                    ],
                    'cflags': ['-g'],
                    'cflags!': ['-ansi']
                }, # target 'hidapi-linux-hidraw'

            ] # targets linux

        }], # OS==linux

    ] # conditions
}
