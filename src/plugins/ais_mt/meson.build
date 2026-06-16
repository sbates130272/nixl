# Copyright (c) Advanced Micro Devices, Inc. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0

rocm_ais_path = get_option('rocm_ais_path')
if rocm_ais_path == ''
    rocm_path = run_command('bash', '-c', 'echo ${ROCM_PATH:-$ROCM_HOME}').stdout().strip()
    if rocm_path == ''
        rocm_path = '/opt/rocm'
    endif
    hipfile_search_dirs = [rocm_path, '/opt/rocs-ais', '/usr/local']
    hipfile_lib_found = false
    cc_local = meson.get_compiler('cpp')
    foreach search_dir : hipfile_search_dirs
        hipfile_lib_candidate = search_dir + '/lib'
        hipfile_inc_candidate = search_dir + '/include'
        if fs.exists(hipfile_lib_candidate) and fs.exists(hipfile_inc_candidate)
            hipfile_lib_check = cc_local.find_library('hipfile',
                dirs: [hipfile_lib_candidate], required: false)
            if hipfile_lib_check.found()
                hipfile_dep = declare_dependency(
                    link_args: ['-L' + hipfile_lib_candidate, '-lhipfile',
                                '-Wl,-rpath,' + hipfile_lib_candidate],
                    include_directories: include_directories(hipfile_inc_candidate))
                hipfile_lib_found = true
                message('Found hipFile at: ' + search_dir)
                break
            endif
        endif
    endforeach
    if not hipfile_lib_found
        hipfile_dep = declare_dependency()
        subdir_done()
    endif
else
    hipfile_lib_path = rocm_ais_path + '/lib'
    hipfile_inc_path = rocm_ais_path + '/include'
    hipfile_dep = declare_dependency(
        link_args: ['-L' + hipfile_lib_path, '-lhipfile',
                    '-Wl,-rpath,' + hipfile_lib_path],
        include_directories: include_directories(hipfile_inc_path))
endif

if not hip_dep.found()
    subdir_done()
endif

if 'AIS_MT' in static_plugins
    ais_mt_backend_lib = static_library('AIS_MT',
        'ais_mt_utils.cpp', 'ais_mt_utils.h',
        'ais_mt_backend.cpp', 'ais_mt_backend.h',
        'ais_mt_plugin.cpp',
        dependencies: [nixl_infra, nixl_common_dep, hip_dep, hipfile_dep, taskflow_proj,
                         file_utils_interface],
        include_directories: [nixl_inc_dirs, utils_inc_dirs],
        install: false,
        cpp_args: compile_flags,
        name_prefix: 'libplugin_')
else
    ais_mt_backend_lib = shared_library('AIS_MT',
        'ais_mt_utils.cpp', 'ais_mt_utils.h',
        'ais_mt_backend.cpp', 'ais_mt_backend.h',
        'ais_mt_plugin.cpp',
        dependencies: [nixl_infra, nixl_common_dep, hip_dep, hipfile_dep, taskflow_proj,
                         file_utils_interface],
        include_directories: [nixl_inc_dirs, utils_inc_dirs],
        install: true,
        cpp_args: ['-fPIC'],
        name_prefix: 'libplugin_',
        install_dir: plugin_install_dir,
        install_rpath: '$ORIGIN/..')
    if get_option('buildtype') == 'debug'
        run_command('sh', '-c',
            'echo "AIS_MT=' + ais_mt_backend_lib.full_path() +
            '" >> ' + plugin_build_dir + '/pluginlist',
            check: true
        )
    endif
endif

ais_mt_backend_interface = declare_dependency(
    link_with: ais_mt_backend_lib)
