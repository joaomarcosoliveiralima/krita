/* This file is part of the KDE project
   Copyright (c) 2004 Michael Thaler <michael.thaler@physik.tu-muenchen.de>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include <kgenericfactory.h>
#include "kis_emboss_filter_plugin.h"
#include "kis_emboss_filter.h"

typedef KGenericFactory<KisEmbossFilterPlugin> KisEmbossFilterPluginFactory;
K_EXPORT_COMPONENT_FACTORY( kritaembossfilter, KisEmbossFilterPluginFactory( "krita" ) )

KisEmbossFilterPlugin::KisEmbossFilterPlugin(QObject *parent, const char *name, const QStringList &) : KParts::Plugin(parent, name)
{
        setInstance(KisEmbossFilterPluginFactory::instance());

        kdDebug() << "EmbossFilter plugin. Class: " 
                << className() 
                << ", Parent: " 
                << parent -> className()
                << "\n";
        KisView * view;

        if ( !parent->inherits("KisView") )
        {
                return;
        } else {
                view = (KisView*) parent;
        }

        KisFilterSP kef = createFilter<KisEmbossFilter>(view);
	(void) new KAction("&Emboss...", 0, 0, kef, SLOT(slotActivated()), actionCollection(), "emboss_filter");
}

KisEmbossFilterPlugin::~KisEmbossFilterPlugin()
{
}
