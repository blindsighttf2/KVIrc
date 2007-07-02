//
//   File : optw_uparser.cpp
//   Creation date : Sat Oct 27 16:32:26 2001 GMT by Szymon Stefanek
//
//   This file is part of the KVirc irc client distribution
//   Copyright (C) 2001 Szymon Stefanek (pragma at kvirc dot net)
//
//   This program is FREE software. You can redistribute it and/or
//   modify it under the terms of the GNU General Public License
//   as published by the Free Software Foundation; either version 2
//   of the License, or (at your opinion) any later version.
//
//   This program is distributed in the HOPE that it will be USEFUL,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//   See the GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program. If not, write to the Free Software Foundation,
//   Inc. ,59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//


#include "optw_uparser.h"

#include <qlayout.h>

#include "kvi_options.h"
#include "kvi_locale.h"

#include <qlabel.h>


KviUParserOptionsWidget::KviUParserOptionsWidget(QWidget * parent)
: KviOptionsWidget(parent,"uparser_options_widget")
{
	createLayout(11,1);

	addBoolSelector(0,0,0,0,__tr2qs_ctx("Disable parser warnings","options"),KviOption_boolAvoidParserWarnings);
	addBoolSelector(0,1,0,1,__tr2qs_ctx("Disable broken event handlers","options"),KviOption_boolDisableBrokenEventHandlers);
	addBoolSelector(0,2,0,2,__tr2qs_ctx("Kill broken timers","options"),KviOption_boolKillBrokenTimers);
	addBoolSelector(0,3,0,3,__tr2qs_ctx("Send unknown commands as /RAW","options"),KviOption_boolSendUnknownCommandsAsRaw);

	addSeparator(0,4,0,4);

	addBoolSelector(0,5,0,5,__tr2qs_ctx("Automatically unload unused modules","options"),KviOption_boolCleanupUnusedModules);
	addBoolSelector(0,6,0,6,__tr2qs_ctx("Ignore module versions (dangerous)","options"),KviOption_boolIgnoreModuleVersions);

	addSeparator(0,7,0,7);

	addBoolSelector(0,8,0,8,__tr2qs_ctx("Relay errors and warnings to debug window","options"),KviOption_boolScriptErrorsToDebugWindow);
	addBoolSelector(0,9,0,9,__tr2qs_ctx("Create minimized debug window","options"),KviOption_boolShowMinimizedDebugWindow);

	addRowSpacer(0,10,0,10);
}

KviUParserOptionsWidget::~KviUParserOptionsWidget()
{
}


