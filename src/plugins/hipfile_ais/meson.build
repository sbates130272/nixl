# Copyright (c) Advanced Micro Devices, Inc. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

hipfile_ais_path = get_option('hipfile_ais_path')
if hipfile_ais_path == ''
    # Auto-detect from ROCM_PATH or /opt/rocm
    rocm_path = run_command('bash', '-c', 'echo $ROCM_PATH').stdout().strip()
    if rocm_path == ''
        rocm_path = '/opt/rocm'
    endif
    hipfile_search_dirs = [rocm_path, '/opt/rocs-ais', '/usr/local']
    hipfile_lib_found = false
    foreach search_dir : hipfile_search_dirs
        hipfile_lib_candidate = search_dir + '/lib'
        hipfile_inc_candidate = search_dir + '/include'
        if fs.exists(hipfile_lib_candidate) and fs.exists(hipfile_inc_candidate)
            cc_local = meson.get_compiler('cpp')
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
    hipfile_lib_path = hipfile_ais_path + '/lib'
    hipfile_inc_path = hipfile_ais_path + '/include'
    hipfile_dep = declare_dependency(
        link_args: ['-L' + hipfile_lib_path, '-lhipfile',
                    '-Wl,-rpath,' + hipfile_lib_path],
        include_directories: include_directories(hipfile_inc_path))
endif

if 'HIPFILE_AIS' in static_plugins
    hipfile_ais_backend_lib = static_library('HIPFILE_AIS',
        'hipfile_ais_utils.cpp', 'hipfile_ais_utils.h',
        'hipfile_ais_backend.cpp', 'hipfile_ais_backend.h',
        'hipfile_ais_plugin.cpp',
        dependencies: [nixl_infra, nixl_common_dep, hip_dep, hipfile_dep,
                       file_utils_interface],
        include_directories: [nixl_inc_dirs, utils_inc_dirs],
        install: false,
        cpp_args: compile_flags,
        name_prefix: 'libplugin_')
else
    hipfile_ais_backend_lib = shared_library('HIPFILE_AIS',
        'hipfile_ais_utils.cpp', 'hipfile_ais_utils.h',
        'hipfile_ais_backend.cpp', 'hipfile_ais_backend.h',
        'hipfile_ais_plugin.cpp',
        dependencies: [nixl_infra, nixl_common_dep, hip_dep, hipfile_dep,
                       file_utils_interface],
        include_directories: [nixl_inc_dirs, utils_inc_dirs],
        install: true,
        cpp_args: ['-fPIC'],
        name_prefix: 'libplugin_',
        install_dir: plugin_install_dir,
        install_rpath: '$ORIGIN/..')
    if get_option('buildtype') == 'debug'
        run_command('sh', '-c',
            'echo "HIPFILE_AIS=' + hipfile_ais_backend_lib.full_path() +
            '" >> ' + plugin_build_dir + '/pluginlist',
            check: true
        )
    endif
endif

hipfile_ais_backend_interface = declare_dependency(
    link_with: hipfile_ais_backend_lib)
