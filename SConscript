# Copyright (c) 2015, AllSeen Alliance. All rights reserved.
#
#    Permission to use, copy, modify, and/or distribute this software for any
#    purpose with or without fee is hereby granted, provided that the above
#    copyright notice and this permission notice appear in all copies.
#
#    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
#    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
#    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
#    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
#    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
#    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
#    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

import os

Import('env')

env['_XMPPCONN'] = True

if not env.has_key('_ALLJOYN_ABOUT_') and os.path.exists('../../core/alljoyn/services/about/SConscript'):
    env.SConscript('../../core/alljoyn/services/about/SConscript')

if not env.has_key('_ALLJOYN_SERVICES_COMMON_') and os.path.exists('../../services/base/services_common/SConscript'):
    env.SConscript('../../services/base/services_common/SConscript')

if not env.has_key('_ALLJOYN_CONFIG_') and os.path.exists('../../services/base/config/SConscript'):
    env.SConscript('../../services/base/config/SConscript')
    
if 'cpp' in env['bindings'] and not env.has_key('_ALLJOYNCORE_') and os.path.exists('../../core/alljoyn/alljoyn_core/SConscript'):
    env.SConscript('../../core/alljoyn/alljoyn_core/SConscript')

xmppconn_env = env.Clone()

for b in xmppconn_env['bindings']:
    if os.path.exists('%s/SConscript' % b):
        xmppconn_env.VariantDir('$OBJDIR/%s' % b, b, duplicate = 0)

xmppconn_env.VariantDir('$OBJDIR/src', 'src', duplicate = 0)

xmppconn_prog = xmppconn_env.SConscript('$OBJDIR/src/SConscript', exports = ['xmppconn_env'])

xmppconn_env['XMPPCONN_DISTDIR'] = xmppconn_env['DISTDIR'] + '/xmppconn'
xmppconn_env.Install('$XMPPCONN_DISTDIR/bin', xmppconn_prog)
xmppconn_env.Install('$XMPPCONN_DISTDIR/conf', File('conf/xmppconn_factory.conf'))

