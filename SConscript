#    Copyright (c) Open Connectivity Foundation (OCF) and AllJoyn Open
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
#    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
#    WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
#    WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
#    AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
#    DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
#    PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
#    TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
#    PERFORMANCE OF THIS SOFTWARE.

import os

Import('env')

env['_XMPPCONN'] = True
    
if not env.has_key('_ALLJOYN_ABOUT_') and os.path.exists('../../core/alljoyn/services/about/SConscript'):
    env.SConscript('../../core/alljoyn/services/about/SConscript')

if not env.has_key('_ALLJOYN_SERVICES_COMMON_') and os.path.exists('../../services/base/services_common/SConscript'):
    env.SConscript('../../services/base/services_common/SConscript')

if env['BUILD_SERVICES_SAMPLES'] == 'on':
    if not env.has_key('_ALLJOYN_CONTROLPANEL_') and os.path.exists('../../services/base/controlpanel/SConscript'):
        env.SConscript('../../services/base/controlpanel/SConscript')

if not env.has_key('_ALLJOYN_CONFIG_') and os.path.exists('../../services/base/config/SConscript'):
    env.SConscript('../../services/base/config/SConscript')

if not env.has_key('_ALLJOYNCORE_') and os.path.exists('../../core/alljoyn/alljoyn_core/SConscript'):
    env.SConscript('../../core/alljoyn/alljoyn_core/SConscript')

xmppconn_env = env.Clone()

for b in xmppconn_env['bindings']:
    if os.path.exists('%s/SConscript' % b):
        xmppconn_env.VariantDir('$OBJDIR/%s' % b, b, duplicate = 0)

xmppconn_env.VariantDir('$OBJDIR/src', 'src', duplicate = 0)

xmppconn_prog = xmppconn_env.SConscript('$OBJDIR/src/SConscript', exports = ['xmppconn_env'])
xmppconn_env.Alias('xmppconn', xmppconn_prog)

def AllFiles(node='.', pattern='*'):
    result = [AllFiles(dir, pattern)
              for dir in Glob(str(node)+'/*')
              if dir.isdir()]
    files = [source
               for source in Glob(str(node)+'/'+pattern)
               if source.isfile()]
    #for name in files:
    #    print name.rstr()
    result += files
    return result

BUILD_DIR='build/*/*/*'
OBJ_DIR=BUILD_DIR+'/obj'
DIST_DIR=BUILD_DIR+'/dist'

if xmppconn_env['FULLCLEAN'] == 'off':
    xmppconn_env.NoClean(xmppconn_prog, AllFiles(OBJ_DIR+'/about', '*'))
    xmppconn_env.NoClean(xmppconn_prog, AllFiles(OBJ_DIR+'/alljoyn_core', '*'))
    xmppconn_env.NoClean(xmppconn_prog, AllFiles(OBJ_DIR+'/common', '*'))
    xmppconn_env.NoClean(xmppconn_prog, AllFiles(OBJ_DIR+'/services', '*'))
    xmppconn_env.NoClean(xmppconn_prog, AllFiles(DIST_DIR+'/config', '*'))
    xmppconn_env.NoClean(xmppconn_prog, AllFiles(DIST_DIR+'/controlpanel', '*'))
    xmppconn_env.NoClean(xmppconn_prog, AllFiles(DIST_DIR+'/cpp', '*'))
    xmppconn_env.NoClean(xmppconn_prog, AllFiles(DIST_DIR+'/services_common', '*'))
    xmppconn_env.NoClean(xmppconn_prog, AllFiles(DIST_DIR, '*.txt'))
else:
    xmppconn_env.Clean(xmppconn_prog, 'build')

xmppconn_env['XMPPCONN_DISTDIR'] = xmppconn_env['DISTDIR'] + '/xmppconn'
xmppconn_env.Install('$XMPPCONN_DISTDIR/bin', xmppconn_prog)
xmppconn_env.Install('$XMPPCONN_DISTDIR/conf', File('conf/xmppconn_factory.conf'))
