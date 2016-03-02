# Copyright (c) Open Connectivity Foundation (OCF) and AllJoyn Open
#    Source Project (AJOSP) Contributors and others.
#
#    SPDX-License-Identifier: Apache-2.0
#
#    All rights reserved. This program and the accompanying materials are
#    made available under the terms of the Apache License, Version 2.0
#    which accompanies this distribution, and is available at
#    http://www.apache.org/licenses/LICENSE-2.0
#
#    Copyright (c) Open Connectivity Foundation and Contributors to AllSeen
#    Alliance. All rights reserved.
#
#    Permission to use, copy, modify, and/or distribute this software for
#    any purpose with or without fee is hereby granted, provided that the
#    above copyright notice and this permission notice appear in all
#    copies.
#
#     THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
#     WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
#     WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
#     AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
#     DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
#     PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
#     TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
#     PERFORMANCE OF THIS SOFTWARE.

import os

env = SConscript('../../core/alljoyn/build_core/SConscript')

vars = Variables()
vars.Add('BINDINGS', 'Bindings to build (comma separated list): cpp', 'cpp')

vars.Add(PathVariable('ALLJOYN_DISTDIR',
                      'Directory containing a pre-built AllJoyn Core dist directory.',
                      os.environ.get('ALLJOYN_DISTDIR')))
                      
vars.Add(EnumVariable('BUILD_SERVICES_SAMPLES',
                      'Build the services samples.',
                      'off',
                      allowed_values = ['off', 'on']))
 
vars.Add(PathVariable('APP_COMMON_DIR',
                      'Directory containing common sample application sources.',
                      os.environ.get('APP_COMMON_DIR','../../services/base/sample_apps')))

vars.Add(PathVariable('LIBXML2_INCDIR',
                      'Directory containing the libxml2 include files.',
                      os.environ.get('LIBXML2_INCDIR','/usr/include/libxml2')))

vars.Add(PathVariable('LIBXML2_LIBDIR',
                      'Directory containing the libxml2 library files.',
                      os.environ.get('LIBXML2_LIBDIR','/usr/lib')))

vars.Add(PathVariable('RAPIDJSON_INCDIR',
                      'Directory containing the rapidjson include folder.',
                      os.environ.get('RAPIDJSON_INCDIR','/usr/include')))

vars.Add(PathVariable('LIBSTROPHE_INCDIR',
                      'Directory containing the libstrophe include files.',
                      os.environ.get('LIBSTROPHE_INCDIR','/usr/include')))

vars.Add(PathVariable('LIBSTROPHE_LIBDIR',
                      'Directory containing the libstrophe library files.',
                      os.environ.get('LIBSTROPHE_LIBDIR','/usr/lib')))

vars.Add(EnumVariable('USE_GATEWAY_AGENT',
                      'Build with Gateway Agent as a dependency.',
                      'off',
                      allowed_values = ['off', 'on']))

vars.Add(EnumVariable('FULLCLEAN',
                      'Used with the scons -c option to clean everything, including AllJoyn dependencies.',
                      'off',
                      allowed_values = ['off', 'on']))

vars.Update(env)
Help(vars.GenerateHelpText(env))

if env.get('ALLJOYN_DISTDIR'):
    # normalize ALLJOYN_DISTDIR first
    env['ALLJOYN_DISTDIR'] = env.Dir('$ALLJOYN_DISTDIR')
    env.Append(CPPPATH = [ env.Dir('$ALLJOYN_DISTDIR/cpp/inc'),
                           env.Dir('$ALLJOYN_DISTDIR/about/inc'),
                           env.Dir('$ALLJOYN_DISTDIR/config/inc'),
                           env.Dir('$ALLJOYN_DISTDIR/services_common/inc') ])
    env.Append(LIBPATH = [ env.Dir('$ALLJOYN_DISTDIR/cpp/lib'),
                           env.Dir('$ALLJOYN_DISTDIR/about/lib'),
                           env.Dir('$ALLJOYN_DISTDIR/config/lib'),
                           env.Dir('$ALLJOYN_DISTDIR/services_common/lib') ])

if env.get('APP_COMMON_DIR'):
    # normalize APP_COMMON_DIR
    env['APP_COMMON_DIR'] = env.Dir('$APP_COMMON_DIR')

env['bindings'] = set([ b.strip() for b in env['BINDINGS'].split(',') ])

env.SConscript('SConscript')
