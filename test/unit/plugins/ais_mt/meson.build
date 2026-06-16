# Copyright (c) Advanced Micro Devices, Inc. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0

if not hip_dep.found()
    subdir_done()
endif

nixl_ais_mt_app = executable('nixl_ais_mt_test',
    'nixl_ais_mt_test.cpp',
    dependencies: [nixl_dep, nixl_infra, hip_dep, nixl_common_dep],
    include_directories: [nixl_inc_dirs, utils_inc_dirs, include_directories('..')],
    install: true)

test('ais_mt_path_mode_smoke', nixl_ais_mt_app)
