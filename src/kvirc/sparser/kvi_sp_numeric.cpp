//=============================================================================
//
//   File : kvi_sp_numeric.cpp
//   Creation date : Thu Aug 3 2000 01:30:45 by Szymon Stefanek
//
//   This file is part of the KVirc irc client distribution
//   Copyright (C) 1999-2007 Szymon Stefanek (pragma at kvirc dot net)
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
//=============================================================================



#include "kvi_sparser.h"
#include "kvi_window.h"
#include "kvi_query.h"
#include "kvi_out.h"
#include "kvi_locale.h"
#include "kvi_ircsocket.h"
#include "kvi_options.h"
#include "kvi_channel.h"
#include "kvi_topicw.h"
#include "kvi_ircuserdb.h"
#include "kvi_defaults.h"
#include "kvi_mirccntrl.h"
#include "kvi_frame.h"
#include "kvi_app.h"
#include "kvi_notifylist.h"
#include "kvi_numeric.h"
#include "kvi_ircconnection.h"
#include "kvi_ircconnectionstatedata.h"
#include "kvi_ircconnectionuserinfo.h"
#include "kvi_ircconnectionserverinfo.h"
#include "kvi_ircconnectionasyncwhoisdata.h"
#include "kvi_ircconnectiontarget.h"
#include "kvi_time.h"
#include "kvi_lagmeter.h"
#include "kvi_qcstring.h"

#include <qpixmap.h>
#include <qdatetime.h>
#include <qtextcodec.h>
#include <qregexp.h> 

#include "kvi_kvs_eventtriggers.h"
#include "kvi_kvs_script.h"
#include "kvi_kvs_variantlist.h"

// #define IS_CHANNEL_TYPE_FLAG(_str) ((*(_str) == '#') || (*(_str) == '&') || (*(_str) == '!'))
#define IS_CHANNEL_TYPE_FLAG(_qchar) (msg->connection()->serverInfo()->supportedChannelTypes().find(_qchar) != -1)
#define IS_USER_MODE_PREFIX(_qchar) (msg->connection()->serverInfo()->supportedModePrefixes().find(_qchar) != -1)

// Numeric message handlers

// FIXME: #warning "IN ALL OUTPUT ADD ESCAPE SEQUENCES!!!!"
// FIXME: #warning "parseErrorUnknownModeChar() for modes e and I , parseErrorUnknownCommand for WATCH"

void KviServerParser::parseNumeric001(KviIrcMessage *msg)
{
	// 001: RPL_WELCOME
	// :prefix 001 target :Welcome to the Internet Relay Network <usermask>
	// FIXME: #warning "SET THE USERMASK FROM SERVER"
	QString szText = msg->connection()->decodeText(msg->safeTrailing());
	QRegExp rx( " ([^ ]+)!([^ ]+)@([^ ]+)$" );
	if( rx.search(szText) != -1)
	{
		msg->connection()->userInfo()->setUnmaskedHostName(rx.cap(3));
		msg->connection()->userInfo()->setNickName(rx.cap(1));
		msg->connection()->userInfoReceived(rx.cap(2),rx.cap(3));
	}
	if(msg->connection()->context()->state() != KviIrcContext::Connected)
		msg->connection()->loginComplete(msg->connection()->decodeText(msg->param(0)));
	if(!msg->haltOutput())
		msg->console()->outputNoFmt(KVI_OUT_SERVERINFO,szText);
}

void KviServerParser::parseNumeric002(KviIrcMessage *msg)
{
	// 002: RPL_YOURHOST [I,E,U,D]
	// :prefix 002 target :Your host is <server name>, running version <server version>
	if(msg->connection()->context()->state() != KviIrcContext::Connected)
		msg->connection()->loginComplete(msg->connection()->decodeText(msg->param(0)));
	if(!msg->haltOutput())msg->console()->outputNoFmt(KVI_OUT_SERVERINFO,msg->connection()->decodeText(msg->safeTrailing()));
}

void KviServerParser::parseNumeric003(KviIrcMessage *msg)
{
	// 003: RPL_CREATED [I,E,U,D]
	// :prefix 003 target :This server was created <date>
	if(msg->connection()->context()->state() != KviIrcContext::Connected)
		msg->connection()->loginComplete(msg->connection()->decodeText(msg->param(0)));
	if(!msg->haltOutput())msg->console()->outputNoFmt(KVI_OUT_SERVERINFO,msg->connection()->decodeText(msg->safeTrailing()));
}

void KviServerParser::parseNumeric004(KviIrcMessage *msg)
{
	// 004: RPL_MYINFO [I,E,U,D]
	// :prefix 004 target <server_name> <srv_version> <u_modes> <ch_modes>
	if(msg->connection()->context()->state() != KviIrcContext::Connected)
		msg->connection()->loginComplete(msg->connection()->decodeText(msg->param(0)));

	int uParams = msg->paramCount();
	int uModeParam = 3;
	
	if(uParams < 2)uParams = 2;

	KviStr version   = msg->safeParam(2);
	msg->connection()->serverInfo()->setServerVersion(msg->safeParam(2));
	
	KviStr umodes;
	// skip version number (great, thanks WEBMASTER INCORPORATED -_-)
	do
	{
		umodes      = msg->safeParam(uModeParam);
	} while (((umodes.contains('.')) || (umodes.contains('-'))) && uModeParam++ < uParams);

	KviStr chanmodes = msg->safeParam(uModeParam+1);

	if(uModeParam > 3)
	{
		version.append(' ');
		version.append(msg->safeParam(3));
	}

	if((umodes.occurences('o') != 1) || (chanmodes.occurences('o') != 1) ||
		(chanmodes.occurences('b') != 1) || (chanmodes.occurences('v') != 1) ||
		(chanmodes.occurences('t') != 1) || (chanmodes.occurences('n') != 1) ||
		(chanmodes.contains('.')) || (chanmodes.contains('-')) || (chanmodes.contains('(')))
	{
		if(!_OUTPUT_QUIET)
		{
			msg->console()->output(KVI_OUT_SYSTEMWARNING,__tr2qs(
				"One or more standard mode flags are missing in the server available modes.\n" \
				"This is caused either by a non RFC1459-compliant IRC daemon or a broken server reply.\n" \
				"Server umodes seem to be '%s' and channel modes seem to be '%s'.\n" \
				"Ignoring this reply and assuming that the basic set of modes is available.\n" \
				"If you have strange problems, try changing the server."),umodes.ptr(),chanmodes.ptr());
		}
		umodes = "oiws";    // standard support
		chanmodes = "obtkmlvsn"; // standard support
	}

	if(KVI_OPTION_BOOL(KviOption_boolShowExtendedServerInfo) && (!msg->haltOutput()))
	{
		if(umodes.hasData())msg->console()->outputNoFmt(KVI_OUT_SERVERINFO,__tr2qs("Available user modes:"));

		const char * aux = umodes.ptr();
		QString tmp;

		while(*aux)
		{
			tmp = msg->connection()->serverInfo()->getUserModeDescription(*aux);
			if(tmp.isNull()) tmp = __tr2qs(": Unknown user mode");
			
			msg->console()->outputNoFmt(KVI_OUT_SERVERINFO,tmp);
			aux++;
		}

		if(chanmodes.hasData())msg->console()->outputNoFmt(KVI_OUT_SERVERINFO,__tr2qs("Available channel modes:"));

		aux = chanmodes.ptr();

		if(!_OUTPUT_MUTE)
		{
			while(*aux)
			{
				KviQString::sprintf(tmp,"%c: %Q",*aux,&(msg->connection()->serverInfo()->getChannelModeDescription(*aux)));
				msg->console()->outputNoFmt(KVI_OUT_SERVERINFO,tmp);
				aux++;
			}
		}
	}

	QString szServer = msg->connection()->decodeText(msg->safeParam(1));

	msg->connection()->serverInfoReceived(szServer,umodes.ptr(),chanmodes.ptr());

	// FIXME: #warning "TO ACTIVE ? OR TO CONSOLE ?"
	if(!_OUTPUT_MUTE)
	{
		if(!msg->haltOutput())msg->console()->output(KVI_OUT_SERVERINFO,
			__tr2qs("Server %Q version %S supporting user modes '%S' and channel modes '%S'"),
			&szServer,&version,&umodes,&chanmodes);
	}
}

void KviServerParser::parseNumeric005(KviIrcMessage *msg)
{
	// 005: RPL_PROTOCTL [D]
	// :prefix 005 target <proto> <proto> .... :are available/supported on this server
	// 005: RPL_BOUNCE [?]
	// :prefix 005 target :Try server <server>, port <port>
	// 005: RPL_ISUPPORT
	if(msg->connection()->context()->state() != KviIrcContext::Connected)
		msg->connection()->loginComplete(msg->connection()->decodeText(msg->param(0)));

	bool bUhNames = false;
	bool bNamesx = false;

	unsigned int count = msg->paramCount();
	if(count > 2)
	{
		count--;
		for(unsigned int i = 1;i < count;i++)
		{
			const char * p = msg->param(i);
			if(kvi_strEqualCIN("PREFIX=(",p,8))
			{
				p+=8;
				const char * pModes = p;
				while(*p && (*p != ')'))p++;
				KviStr szModeFlags(pModes,p-pModes);
				if(*p)p++;
				KviStr szModePrefixes = p;
				if(szModePrefixes.hasData() && (szModePrefixes.len() == szModeFlags.len()))
				{
					msg->connection()->serverInfo()->setSupportedModePrefixes(szModePrefixes.ptr(),szModeFlags.ptr());
				}
			} else if(kvi_strEqualCIN("CHANTYPES=",p,10))
			{
				p+=10;
				KviStr tmp = p;
				if(tmp.hasData())msg->connection()->serverInfo()->setSupportedChannelTypes(tmp.ptr());
			} else if(kvi_strEqualCI("WATCH",p) || kvi_strEqualCIN("WATCH=",p,6))
			{
				msg->connection()->serverInfo()->setSupportsWatchList(true);
				if((!_OUTPUT_MUTE) && (!msg->haltOutput()) && KVI_OPTION_BOOL(KviOption_boolShowExtendedServerInfo))
				{
					msg->console()->outputNoFmt(KVI_OUT_SERVERINFO,__tr2qs("This server supports the WATCH notify list method, it will be used"));
				}
			} else if(kvi_strEqualCIN("TOPICLEN=",p,9))
			{
				p += 9;
				QString tmp = p;
				if(!tmp.isEmpty()) {
					bool ok;
        				int len = tmp.toInt( &ok );
        				if(ok) msg->connection()->serverInfo()->setMaxTopicLen(len);
				}
			} else if(kvi_strEqualCIN("NETWORK=",p,8))
			{
				p += 8;
				QString tmp = p;
				if(!tmp.isEmpty())msg->console()->connection()->target()->setNetworkName(tmp);
				if((!_OUTPUT_MUTE) && (!msg->haltOutput()) && KVI_OPTION_BOOL(KviOption_boolShowExtendedServerInfo))
				{
					msg->console()->output(KVI_OUT_SERVERINFO,__tr2qs("The current network is %Q"),&tmp);
				}
			} else if(kvi_strEqualCI("CODEPAGES",p))
			{
				msg->connection()->serverInfo()->setSupportsCodePages(true);
				if((!_OUTPUT_MUTE) && (!msg->haltOutput()) && KVI_OPTION_BOOL(KviOption_boolShowExtendedServerInfo))
				{
					msg->console()->outputNoFmt(KVI_OUT_SERVERINFO,__tr2qs("This server supports the CODEPAGE command, it will be used"));
				}

				//msg->connection()->sendFmtData("CODEPAGE %s",msg->console()->textCodec()->name());

			} else if(kvi_strEqualCIN("CHANMODES=",p,10))
			{
				p+=10;
				QString tmp = p;
				msg->connection()->serverInfo()->setSupportedChannelModes(tmp);
			}else if(kvi_strEqualCIN("MODES=",p,6))
			{
				p+=6;
				QString tmp = p;
				bool bok;
				int num=tmp.toUInt(&bok);
				if(bok)
					msg->connection()->serverInfo()->setMaxModeChanges(num);
			} else if(kvi_strEqualCIN("NAMESX",p,6))
			{
				p+=6;
				bNamesx=true;
			} else if(kvi_strEqualCIN("UHNAMES",p,7))
			{
				p+=7;
				bUhNames=true;
			}else if(kvi_strEqualCIN("CHARSET=",p,8))
			{
				p+=8;
				QString tmp = p;
				msg->connection()->serverInfo()->setSupportsCodePages(true);

				if((!_OUTPUT_MUTE) && (!msg->haltOutput()) && KVI_OPTION_BOOL(KviOption_boolShowExtendedServerInfo))
				{
					msg->console()->outputNoFmt(KVI_OUT_SERVERINFO,__tr2qs("This server supports the CODEPAGE command, it will be used"));
				}

				/*if( tmp.contains(msg->console()->textCodec()->name(),false) || tmp.contains("*",false) )
				{
					msg->connection()->sendFmtData("CODEPAGE %s",msg->console()->textCodec()->name());
				}*/
			}
		}
		if((!_OUTPUT_MUTE) && (!msg->haltOutput()))
		{
			const char * aux = msg->allParams();
			while(*aux == ' ')aux++;
			while(*aux && (*aux != ' '))aux++;
			while(*aux == ' ')aux++;
			if(*aux == ':')aux++;
			if(!msg->haltOutput())msg->console()->output(KVI_OUT_SERVERINFO,__tr2qs("This server supports: %s"),msg->connection()->decodeText(aux).utf8().data());
			if(bNamesx || bUhNames) {
				msg->connection()->sendFmtData("PROTOCTL %s %s",bNamesx ? "NAMESX" : "", bUhNames ? "UHNAMES" : "");
			}
		}
	} else {
		QString inf = msg->connection()->decodeText(msg->safeTrailing());
		if(!msg->haltOutput())msg->console()->outputNoFmt(KVI_OUT_SERVERINFO,inf);
	}
	// } else {
	// 	// RPL_BOUNCE prolly
	// 	if(!msg->haltOutput())msg->console()->outputNoFmt(KVI_OUT_SERVERINFO,msg->safeTrailing());
	// }
}

void KviServerParser::parseNumericMotd(KviIrcMessage *msg)
{
	// 372: RPL_MOTD [I,E,U,D]
	// :prefix 372 target : - <motd>
	// 377: RPL_MOTD2 [?]
	// :prefix 377 target : - <motd>
	// 378: RPL_MOTD3 [Austnet]
	// :prefix 377 target : - <motd>
	// 375: RPL_MOTDSTART [I,E,U,D]
	// :prefix 375 target : - <server> Message of the Day -
	// 372: RPL_ENDOFMOTD [I,E,U,D]
	// :prefix 376 target :End of /MOTD command.
	// FIXME: #warning "SKIP MOTD , MOTD IN A SEPARATE WINDOW , SILENT ENDOFMOTD , MOTD IN ACTIVE WINDOW"
	if(!msg->haltOutput())msg->console()->outputNoFmt(KVI_OUT_MOTD,msg->connection()->decodeText(msg->safeTrailing()));

	if(msg->numeric() == RPL_ENDOFMOTD)
	{
		msg->connection()->endOfMotdReceived();
	}
}

void KviServerParser::parseNumericEndOfNames(KviIrcMessage *msg)
{
	// 366: RPL_ENDOFNAMES [I,E,U,D]
	// :prefix 366 target <channel> :End of /NAMES list.
	QString szChan = msg->connection()->decodeText(msg->safeParam(1));
	KviChannel * chan = msg->connection()->findChannel(szChan);
	if(chan)
	{
		if(!chan->hasAllNames())
		{
			chan->setHasAllNames();
			return;
		}
	}

	if(!msg->haltOutput() && !_OUTPUT_MUTE)
	{
// FIXME: #warning "KVI_OUT_NAMES missing"
		KviWindow * pOut = chan ? chan : KVI_OPTION_BOOL(KviOption_boolServerRepliesToActiveWindow) ?
			msg->console()->activeWindow() : (KviWindow *)(msg->console());
		pOut->output(KVI_OUT_UNHANDLED,__tr2qs("End of NAMES for \r!c\r%Q\r"),&szChan);
	}
}

void KviServerParser::parseNumeric020(KviIrcMessage *msg)
{
	// 020: RPL_CONNECTING
	//:irc.dotsrc.org 020 * :Please wait while we process your connection.
	QString szServer;
	if(!msg->haltOutput())
	{
		QString szWText = msg->console()->decodeText(msg->safeTrailing());
		msg->console()->output(
			KVI_OUT_CONNECTION,__tr2qs("%c\r!s\r%s\r%c: %Q"),KVI_TEXT_BOLD,
			msg->safePrefix(),KVI_TEXT_BOLD,&szWText);
	}
}

void KviServerParser::parseNumericNames(KviIrcMessage *msg)
{
	// 353: RPL_NAMREPLY [I,E,U,D]
	// :prefix 353 target [=|*|@] <channel> :<space_separated_list_of_nicks>

	// [=|*|@] is the type of the channel:
	// = --> public  * --> private   @ --> secret
	// ...but we ignore it
	//QString szChan = msg->connection()->decodeText(msg->cSafeParam(2)->data()); // <-- KviQCString::data() is implicitly unsafe: it CAN return 0
	QString szChan = msg->connection()->decodeText(msg->safeParam(2));
	KviChannel * chan = msg->connection()->findChannel(szChan);
	// and run to the first nickname
	char * aux = msg->safeTrailingString().ptr();
	while((*aux) && (*aux == ' '))aux++;
	// now check if we have that channel

	char * trailing = aux;

	bool bHalt = msg->haltOutput();

	if(chan)
	{
		bHalt = bHalt || (!chan->hasAllNames());

		// K...time to parse a lot of data
		chan->enableUserListUpdates(false);

		int iPrevFlags = chan->myFlags();

		KviIrcConnectionServerInfo * pServerInfo = msg->connection()->serverInfo();
		
		while(*aux)
		{
			int iFlags = 0;
			// @ = op (+o), + = voiced (+v), % = halfop (+h), - = userop (+u), ^ = protected (+a?), * = chan owner (+q), !, & = channel admin (+a?)
			// ^  +a is a weird mode: it also breaks nicknames on some networks!
			// not a valid first char(s) of nickname, must be a mode prefix
			
			bool bContinue = true;

			while(pServerInfo->isSupportedModePrefix((unsigned char)(*aux)))
			{
				// leading umode flag(s)
				iFlags |= pServerInfo->modeFlagFromPrefixChar(*aux);
				aux++;
			}
			
			char * begin = aux;
			while(*aux && (*aux != ' '))aux++;
			char save = *aux;
			*aux = 0;
			// NAMESX + UHNAMES support
			KviIrcMask mask(msg->connection()->decodeText(begin));
			// and make it join
			if(!mask.nick().isEmpty())chan->join(mask.nick(),
				mask.hasUser() ? mask.user() : QString::null,
				mask.hasHost() ? mask.host() : QString::null,
					iFlags);
			*aux = ' ';
			*aux = save;
			// run to the next nick (or the end)
			while((*aux) && (*aux == ' '))aux++;
		}

		if(iPrevFlags != chan->myFlags())chan->updateCaption();

		chan->enableUserListUpdates(true);
		// finished a block
	}

	// So it is a result of a /NAMES command or a local desync
	// We handle it in a cool way.

	if(!bHalt)
	{
		// FIXME: #warning "KVI_OUT_NAMES missing"
		KviWindow * pOut = chan ? chan : KVI_OPTION_BOOL(KviOption_boolServerRepliesToActiveWindow) ?
			msg->console()->activeWindow() : (KviWindow *)(msg->console());
		QString szTrailing = trailing ? msg->connection()->decodeText(trailing) : QString("");
		pOut->output(KVI_OUT_UNHANDLED,__tr2qs("Names for \r!c\r%Q\r: %Q"),&szChan,&szTrailing);
	}
}

void KviServerParser::parseNumericTopic(KviIrcMessage *msg)
{
	// 332: RPL_TOPIC [I,E,U,D]
	// :prefix 332 target <channel> :<topic>
	QString szChan = msg->connection()->decodeText(msg->safeParam(1));
	KviChannel * chan = msg->connection()->findChannel(szChan);
	if(chan)
	{
		QString szTopic = chan->decodeText(msg->safeTrailing());

		chan->topicWidget()->setTopic(szTopic);
		chan->topicWidget()->setTopicSetBy(__tr2qs("(unknown)"));
		if(KVI_OPTION_BOOL(KviOption_boolEchoNumericTopic))
		{
			if(!msg->haltOutput())
				chan->output(KVI_OUT_TOPIC,__tr2qs("Channel topic is: %Q"),&szTopic);
		}
	} else {
		KviWindow * pOut = KVI_OPTION_BOOL(KviOption_boolServerRepliesToActiveWindow) ?
			msg->console()->activeWindow() : (KviWindow *)(msg->console());

		QString szTopic = msg->connection()->decodeText(msg->safeTrailing());
		pOut->output(KVI_OUT_TOPIC,__tr2qs("Topic for \r!c\r%Q\r is: %Q"),
			&szChan,&szTopic);
	}

}

void KviServerParser::parseNumericNoTopic(KviIrcMessage *msg)
{
	// 331: RPL_NOTOPIC [I,E,U,D]
	// :prefix 331 target <channel> :No topic is set
	QString szChan = msg->connection()->decodeText(msg->safeParam(1));
	KviChannel * chan = msg->connection()->findChannel(szChan);
	if(chan)
	{
		chan->topicWidget()->setTopic("");
		if(KVI_OPTION_BOOL(KviOption_boolEchoNumericTopic))
		{
			if(!msg->haltOutput())
				chan->outputNoFmt(KVI_OUT_TOPIC,__tr2qs("No channel topic is set"));
		}
	} else {
		KviWindow * pOut = KVI_OPTION_BOOL(KviOption_boolServerRepliesToActiveWindow) ?
			msg->console()->activeWindow() : (KviWindow *)(msg->console());
		pOut->output(KVI_OUT_TOPIC,__tr2qs("No topic is set for channel \r!c\r%Q\r"),
			&szChan);
	}
}

void KviServerParser::parseNumericTopicWhoTime(KviIrcMessage *msg)
{
	// 333: RPL_TOPICWHOTIME [e,U,D]
	// :prefix 333 target <channel> <whoset> <time>

	QString szChan = msg->connection()->decodeText(msg->safeParam(1));
	KviChannel * chan = msg->connection()->findChannel(szChan);

	KviStr tmp = msg->safeParam(3);
	bool bOk = false;
	unsigned long t = 0;
	if(tmp.hasData())t = tmp.toUInt(&bOk);

	QDateTime dt;
	dt.setTime_t(t);

	QString szDate = dt.toString();
	QString szWho = msg->connection()->decodeText(msg->safeParam(2));
	KviIrcMask who(szWho);
	QString szDisplayableWho;
	if( !(who.hasUser() && who.hasHost()) )
	{
		szDisplayableWho="\r!n\r"+szWho+"\r";
	} else {
		KviQString::sprintf(szDisplayableWho,"\r!n\r%Q\r!%Q@\r!h\r%Q\r",&(who.nick()),&(who.user()),&(who.host()));
	}
	if(chan)
	{
		chan->topicWidget()->setTopicSetBy(szWho);
		if(bOk)chan->topicWidget()->setTopicSetAt(szDate);
		if(KVI_OPTION_BOOL(KviOption_boolEchoNumericTopic))
		{
			if(!msg->haltOutput())
			{
				if(bOk)chan->output(KVI_OUT_TOPIC,__tr2qs("Topic was set by %Q on %Q"),&szDisplayableWho,&szDate);
				else chan->output(KVI_OUT_TOPIC,__tr2qs("Topic was set by %Q"),&szDisplayableWho);
			}
		}
	} else {
		KviWindow * pOut = KVI_OPTION_BOOL(KviOption_boolServerRepliesToActiveWindow) ?
			msg->console()->activeWindow() : (KviWindow *)(msg->console());
		if(bOk)
		{
			pOut->output(KVI_OUT_TOPIC,__tr2qs("Topic for \r!c\r%Q\r was set by %Q on %Q"),
					&szChan,&szDisplayableWho,&szDate);
		} else {
			pOut->output(KVI_OUT_TOPIC,__tr2qs("Topic for \r!c\r%Q\r was set by %Q"),
					&szChan,&szDisplayableWho);
		}
	}
}

void KviServerParser::parseNumericChannelModeIs(KviIrcMessage *msg)
{
	// 324: RPL_CHANNELMODEIS [I,E,U,D]
	// :prefix 324 target <channel> +<chanmode>
	QString szSource = msg->connection()->decodeText(msg->safePrefix());
	QString szChan = msg->connection()->decodeText(msg->safeParam(1));
	KviChannel * chan = msg->connection()->findChannel(szChan);
	KviStr modefl = msg->safeParam(2);
	if(chan)parseChannelMode(szSource,"*","*",chan,modefl,msg,3);
	else {
		KviWindow * pOut = KVI_OPTION_BOOL(KviOption_boolServerRepliesToActiveWindow) ?
			msg->console()->activeWindow() : (KviWindow *)(msg->console());
		if((!szChan.isEmpty()) && IS_CHANNEL_TYPE_FLAG(szChan[0]))
		{
			pOut->output(KVI_OUT_CHANMODE,__tr2qs("Channel mode for \r!c\r%Q\r is %s"),
				&szChan,msg->safeParam(2));
		} else {
			pOut->output(KVI_OUT_MODE,__tr2qs("User mode for \r!n\r%Q\r is %s"),
				&szChan,msg->safeParam(2));
		}
	}
}

void getDateTimeStringFromCharTimeT(QString &buffer,const char *time_t_string)
{
	KviStr tmp=time_t_string;
	bool bOk=false;
	unsigned int uTime = tmp.toUInt(&bOk);
	if(bOk){
		QDateTime dt;
		dt.setTime_t(uTime);
		buffer = dt.toString();
	} else buffer = __tr2qs("(Unknown)");
}

#define PARSE_NUMERIC_ENDOFLIST(__funcname,__setGotIt,__didSendRequest,__setDone,__daicon,__szWhatQString) \
	void KviServerParser::__funcname(KviIrcMessage *msg) \
	{ \
		QString szChan = msg->connection()->decodeText(msg->safeParam(1)); \
		KviChannel * chan = msg->connection()->findChannel(szChan); \
		if(chan) \
		{ \
			chan->__setGotIt(); \
			if(chan->__didSendRequest()) \
			{ \
				chan->__setDone(); \
				return; \
			} \
		} \
		if(!msg->haltOutput()) \
		{ \
			KviWindow * pOut = chan ? chan : KVI_OPTION_BOOL(KviOption_boolServerRepliesToActiveWindow) ? \
				msg->console()->activeWindow() : (KviWindow *)(msg->console()); \
			pOut->output(__daicon,__tr2qs("End of channel %Q for \r!c\r%Q\r"),&(__szWhatQString),&szChan); \
		} \
	}

PARSE_NUMERIC_ENDOFLIST(parseNumericEndOfBanList,setHasBanList,sentBanListRequest,setBanListDone,KVI_OUT_BAN,__tr2qs("ban list"))
PARSE_NUMERIC_ENDOFLIST(parseNumericEndOfInviteList,setHasInviteList,sentInviteListRequest,setInviteListDone,KVI_OUT_INVITEEXCEPT,__tr2qs("invite list"))
PARSE_NUMERIC_ENDOFLIST(parseNumericEndOfExceptList,setHasBanExceptionList,sentBanExceptionListRequest,setBanExceptionListDone,KVI_OUT_BANEXCEPT,__tr2qs("ban exception list"))

#define PARSE_NUMERIC_LIST(__funcname,__modechar,__sentRequest,__ico,__szWhatQString) \
	void KviServerParser::__funcname(KviIrcMessage *msg) \
	{ \
		QString szChan   = msg->connection()->decodeText(msg->safeParam(1)); \
		QString banmask  = msg->connection()->decodeText(msg->safeParam(2)); \
		QString bansetby = msg->connection()->decodeText(msg->safeParam(3)); \
		QString bansetat; \
		getDateTimeStringFromCharTimeT(bansetat,msg->safeParam(4)); \
		if(bansetby.isEmpty())bansetby = __tr2qs("(Unknown)"); \
		KviChannel * chan = msg->connection()->findChannel(szChan); \
		if(chan) \
		{ \
			chan->setMask(__modechar,banmask,true,bansetby,QString(msg->safeParam(4)).toUInt()); \
			if(chan->__sentRequest())return; \
		} \
		if(!msg->haltOutput()) \
		{ \
			KviWindow * pOut = chan ? chan : KVI_OPTION_BOOL(KviOption_boolServerRepliesToActiveWindow) ? \
				msg->console()->activeWindow() : (KviWindow *)(msg->console()); \
			pOut->output(__ico,__tr2qs("%Q for \r!c\r%Q\r: \r!m-%c\r%Q\r (set by %Q on %Q)"), \
				&(__szWhatQString),&szChan,__modechar,&banmask,&bansetby,&bansetat); \
		} \
	}

// 367: RPL_BANLIST [I,E,U,D]
// :prefix 367 target <channel> <banmask> [bansetby] [bansetat]
PARSE_NUMERIC_LIST(parseNumericBanList,'b',sentBanListRequest,KVI_OUT_BAN,__tr2qs("Ban listing"))
// 346: RPL_INVITELIST [I,E,U,D]
// :prefix 346 target <channel> <invitemask> [invitesetby] [invitesetat]
PARSE_NUMERIC_LIST(parseNumericInviteList,'I',sentInviteListRequest,KVI_OUT_INVITEEXCEPT,__tr2qs("Invite listing"))
// 346: RPL_EXCEPTLIST [I,E,U,D]
// :prefix 346 target <channel> <banmask> [bansetby] [bansetat]
PARSE_NUMERIC_LIST(parseNumericExceptList,'e',sentBanExceptionListRequest,KVI_OUT_BANEXCEPT,__tr2qs("Ban exception listing"));


void KviServerParser::parseNumericWhoReply(KviIrcMessage *msg)
{
	// 352: RPL_WHOREPLY [I,E,U,D]
	// :prefix 352 target <chan> <usr> <hst> <srv> <nck> <stat> :<hops> <real>

	QString szChan = msg->connection()->decodeText(msg->safeParam(1));
	QString szUser = msg->connection()->decodeText(msg->safeParam(2));
	QString szHost = msg->connection()->decodeText(msg->safeParam(3));
	QString szServ = msg->connection()->decodeText(msg->safeParam(4));
	QString szNick = msg->connection()->decodeText(msg->safeParam(5));
	QString szFlag = msg->connection()->decodeText(msg->safeParam(6));
	bool bAway = szFlag.find('G') != -1;

	KviStr trailing = msg->safeTrailing();
	KviStr hops = trailing.getToken(' ');
	bool bHopsOk = false;
	int iHops = hops.toInt(&bHopsOk);

	QString szReal = msg->connection()->decodeText(trailing.ptr());

	// Update the user entry
	KviIrcUserDataBase * db = msg->connection()->userDataBase();
	KviIrcUserEntry * e = db->find(szNick);
	if(e)
	{
		if(bHopsOk)e->setHops(iHops);
		e->setUser(szUser);
		e->setHost(szHost);
		e->setServer(szServ);
		e->setRealName(szReal);
		e->setAway(bAway);
		KviQuery * q = msg->connection()->findQuery(szNick);
		if(q) q->updateLabelText();
		if(!e->avatar())
		{
			// FIXME: #warning "THE AVATAR SHOULD BE RESIZED TO MATCH THE MAX WIDTH/HEIGHT"
			// maybe now we can match this user ?
			msg->console()->checkDefaultAvatar(e,szNick,szUser,szHost);
		}
	}

	KviChannel * chan = msg->connection()->findChannel(szChan);
	if(chan)
	{
		if(!chan->hasWhoList())
		{
			// FIXME: #warning "IF VERBOSE && SHOW INTERNAL WHO REPLIES...."
			return;
		}
		if(chan->sentSyncWhoRequest())
		{
			// FIXME: #warning "IF VERBOSE && SHOW INTERNAL WHO REPLIES...."
			return;
		}
	}

	// FIXME: #warning "SYNC OP/VOICE on channel!!!"

	if(!msg->haltOutput())
	{
		KviWindow * pOut = chan ? chan : KVI_OPTION_BOOL(KviOption_boolServerRepliesToActiveWindow) ?
			msg->console()->activeWindow() : (KviWindow *)(msg->console());

		QString szAway = bAway ? __tr2qs("Yes") : __tr2qs("No");

		pOut->output(KVI_OUT_WHO,
			__tr2qs("WHO entry for %c\r!n\r%Q\r%c [%Q@\r!h\r%Q\r]: %cChannel%c: \r!c\r%Q\r, %cServer%c: \r!s\r%Q\r, %cHops%c: %d, %cFlags%c: %Q, %cAway%c: %Q, %cReal name%c: %Q"),
			KVI_TEXT_BOLD,&szNick, KVI_TEXT_BOLD, 
			&szUser,&szHost,KVI_TEXT_UNDERLINE,
			KVI_TEXT_UNDERLINE,&szChan,KVI_TEXT_UNDERLINE,
			KVI_TEXT_UNDERLINE,&szServ,KVI_TEXT_UNDERLINE,
			KVI_TEXT_UNDERLINE,iHops, KVI_TEXT_UNDERLINE, KVI_TEXT_UNDERLINE,
			&szFlag, KVI_TEXT_UNDERLINE, KVI_TEXT_UNDERLINE,
			&szAway, KVI_TEXT_UNDERLINE,
			KVI_TEXT_UNDERLINE, &szReal);
	}

	
}

void KviServerParser::parseNumericEndOfWho(KviIrcMessage *msg)
{
	// 315: RPL_ENDOFWHO [I,E,U,D]
	// :prefix 315 target <channel/nick> :End of /WHO List.
	QString szChan = msg->connection()->decodeText(msg->safeParam(1));
	KviChannel * chan = msg->connection()->findChannel(szChan);
	if(chan)
	{
		chan->userListView()->updateArea();
		kvi_time_t tNow = kvi_unixTime();
		msg->connection()->stateData()->setLastReceivedChannelWhoReply(tNow);
		chan->setLastReceivedWhoReply(tNow);
		if(msg->connection()->lagMeter())
		{
			KviStr tmp(KviStr::Format,"WHO %s",msg->safeParam(1));
			msg->connection()->lagMeter()->lagCheckComplete(tmp.ptr());
		}

		if(!chan->hasWhoList())
		{
			// FIXME: #warning "IF VERBOSE && SHOW INTERNAL WHO REPLIES...."
			chan->setHasWhoList();
			return;
		}
		
		if(chan->sentSyncWhoRequest())
		{
			// FIXME: #warning "IF VERBOSE && SHOW INTERNAL WHO REPLIES...."
			chan->clearSentSyncWhoRequest();
			return;
		}
	}

	if(!msg->haltOutput())
	{
		KviWindow * pOut = chan ? chan : KVI_OPTION_BOOL(KviOption_boolServerRepliesToActiveWindow) ?
			msg->console()->activeWindow() : (KviWindow *)(msg->console());
		QString whoTarget = msg->connection()->decodeText(msg->safeParam(1));
		if(IS_CHANNEL_TYPE_FLAG(whoTarget[0]))
			whoTarget.prepend("\r!c\r");
		else
			whoTarget.prepend("\r!n\r");
		whoTarget.append("\r");
		pOut->output(KVI_OUT_WHO,__tr2qs("End of WHO list for %Q"),&whoTarget);
	}
}

void KviServerParser::parseLoginNicknameProblem(KviIrcMessage *msg)
{
	// ops...not logged in yet...
	QString nextNick;
	unsigned int uNickCnt;
	switch(msg->connection()->stateData()->loginNickIndex())
	{
		case 0:
			// used a server specific nickname
			KVI_OPTION_STRING(KviOption_stringNickname1).stripWhiteSpace();
			if(KVI_OPTION_STRING(KviOption_stringNickname1).isEmpty())
				KVI_OPTION_STRING(KviOption_stringNickname1) = KVI_DEFAULT_NICKNAME1;
			nextNick = KVI_OPTION_STRING(KviOption_stringNickname1);
			uNickCnt = 1;
		case 1:
			// used the first nickname of the identity
			KVI_OPTION_STRING(KviOption_stringNickname2).stripWhiteSpace();
			if(KVI_OPTION_STRING(KviOption_stringNickname2).isEmpty())
				KVI_OPTION_STRING(KviOption_stringNickname2) = KVI_DEFAULT_NICKNAME2;
			nextNick = KVI_OPTION_STRING(KviOption_stringNickname2);
			uNickCnt = 2;
		break;
		case 2:
			// used the second nickname of the identity
			KVI_OPTION_STRING(KviOption_stringNickname3).stripWhiteSpace();
			if(KVI_OPTION_STRING(KviOption_stringNickname3).isEmpty())
				KVI_OPTION_STRING(KviOption_stringNickname3) = KVI_DEFAULT_NICKNAME3;
			nextNick = KVI_OPTION_STRING(KviOption_stringNickname3);
			uNickCnt = 3;
		break;
		default:
		{
			// used all the nicknames of the identity
			// fall back to a random string...
			nextNick = msg->safeParam(1);
			nextNick.stripWhiteSpace();
			if(nextNick.isEmpty())nextNick = KVI_DEFAULT_NICKNAME1;
			nextNick = nextNick.left(7);
			QString num;
			num.setNum(msg->connection()->stateData()->loginNickIndex());
			nextNick.append(num);
			uNickCnt = msg->connection()->stateData()->loginNickIndex() + 1;
		}
		break;
	}

	QString szOldNick = msg->connection()->userInfo()->nickName();
	msg->console()->notifyListView()->nickChange(szOldNick,nextNick);

	msg->connection()->userInfo()->setNickName(nextNick);
	msg->connection()->stateData()->setLoginNickIndex(uNickCnt);

	if(uNickCnt > 7)
	{
		msg->console()->output(KVI_OUT_NICKNAMEPROBLEM,
			__tr2qs("Something really weird is happening: the server is refusing all the login nicknames..."));
			
		if(msg->connection()->stateData()->loginNickIndex() > 10)
		{
			msg->console()->output(KVI_OUT_NICKNAMEPROBLEM,
				__tr2qs("The server is refusing all the login nicknames: giving up, you must send the nickname manually"));
			return;
		}
	}

	if(!msg->haltOutput())
	{
		QString szActual = msg->connection()->decodeText(msg->safeParam(1));
		QString szWText = msg->connection()->decodeText(msg->safeTrailing());
		msg->console()->output(KVI_OUT_NICKNAMEPROBLEM,
			__tr2qs("No way to login as '\r!n\r%Q\r' (%d: %Q), trying '%Q'..."),
				&szActual,msg->numeric(),&szWText,&nextNick);
	}

	KviQCString d = msg->connection()->encodeText(nextNick);
	msg->connection()->sendFmtData("NICK %s",d.data());
}

void KviServerParser::parseNumericUnavailResource(KviIrcMessage *msg)
{
	// 437: ERR_UNAVAILRESOURCE [I]
	// :prefix 437 <target> <nick/channel> :Nick/Channel is temporairly unavailable
	if(!(msg->console()->isConnected()))
	{
		parseLoginNicknameProblem(msg);
	} else {
		// already connected... just say that we have problems
		if(!msg->haltOutput())
		{
			QString szNk = msg->connection()->decodeText(msg->safeParam(1));
			QString szWText = msg->connection()->decodeText(msg->safeTrailing());
			msg->console()->output(KVI_OUT_NICKNAMEPROBLEM,
				"\r!n\r%Q\r: %Q",&szNk,&szWText);
		}
	}
}

void KviServerParser::parseNumericCantJoinChannel(KviIrcMessage *msg)
{
	// 471: ERR_CHANNELISFULL [I,E,U,D]
	// 473: ERR_INVITEONLYCHAN [I,E,U,D]
	// 474: ERR_BANNEDFROMCHAN [I,E,U,D]
	// 475: ERR_BADCHANNELKEY [I,E,U,D]
	// :prefix 47* <target> <channel> :Can't join channel (+l/i/b/k)
	if(!msg->haltOutput())
	{
		KviWindow * pOut = (KviWindow *)(msg->connection()->findChannel(msg->safeParam(1)));
		if(!pOut)pOut = (KviWindow *)(msg->console());
		QString szChannel = msg->connection()->decodeText(msg->safeParam(1));
		QString szWText = msg->connection()->decodeText(msg->safeTrailing());
		pOut->output(KVI_OUT_JOINERROR,
			"\r!c\r%Q\r: %Q",&szChannel,&szWText);
	}
}

// Keep the source ordered: this should be named "parseOtherChannelError"

void KviServerParser::otherChannelError(KviIrcMessage *msg)
{
	// 482: ERR_CHANOPRIVSNEEDED
	// 467: ERR_KEYSET
	// 472: ERR_UNKNOWNMODE
	// :prefix 4?? <target> <channel> :error text
	if(!msg->haltOutput())
	{
		KviWindow * pOut = (KviWindow *)(msg->connection()->findChannel(msg->safeParam(1)));
		if(!pOut)pOut = (KviWindow *)(msg->console());
		QString szChannel = msg->connection()->decodeText(msg->safeParam(1));
		QString szWText = msg->connection()->decodeText(msg->safeTrailing());
		pOut->output(KVI_OUT_GENERICERROR,
			"\r!c\r%Q\r: %Q",&szChannel,&szWText);
	}
}

void KviServerParser::parseCommandSyntaxHelp(KviIrcMessage *msg)
{
	// 704 RPL_COMMANDSYNTAX
	// :prefix 704 <target> <command> :text
	if(!msg->haltOutput())
	{
		KviWindow * pOut = (KviWindow *)(msg->console());
		QString szCommand = msg->connection()->decodeText(msg->safeParam(1));
		QString szWText = msg->connection()->decodeText(msg->safeTrailing());
		pOut->output(KVI_OUT_HELP,
			__tr2qs("Command syntax %Q: %Q"),&szCommand,&szWText); // Pragma: wheee..... that should be in english :D
	}
}

void KviServerParser::parseCommandHelp(KviIrcMessage *msg)
{
	// 705 RPL_COMMANDHELP
	// :prefix 705 <target> <command> :text
	if(!msg->haltOutput())
	{
		KviWindow * pOut = (KviWindow *)(msg->console());
		QString szCommand = msg->connection()->decodeText(msg->safeParam(1));
		QString szWText = msg->connection()->decodeText(msg->safeTrailing());
		pOut->outputNoFmt(KVI_OUT_HELP,szWText);
	}
}

void KviServerParser::parseChannelHelp(KviIrcMessage *msg)
{
	// 477 RPL_CHANNELHELP (freenode)
	// :prefix 477 <target> <channel> :text
	if(!msg->haltOutput())
	{
		QString szChan  = msg->connection()->decodeText(msg->safeParam(1));
		QString szText = msg->connection()->decodeText(msg->safeTrailing());
		KviWindow * pOut = msg->connection()->findChannel(szChan);
		if(pOut)
		{
			pOut->output(KVI_OUT_HELP,__tr2qs("Tip: %Q"),&szText);
		} else {
			pOut = (KviWindow *)(msg->console());
			pOut->output(KVI_OUT_HELP,__tr2qs("Tip for %Q: %Q"),&szChan,&szText);
		}
		
	}
}


void KviServerParser::parseCommandEndOfHelp(KviIrcMessage *msg)
{
	// 704 RPL_COMMANDSYNTAX
	// 705 RPL_COMMANDHELP
	// :prefix 706 <target> <command> :End of /HELP.
	if(!msg->haltOutput())
	{
		KviWindow * pOut = (KviWindow *)(msg->console());
		QString szCommand = msg->connection()->decodeText(msg->safeParam(1));
		pOut->output(KVI_OUT_HELP,
			__tr2qs("End of help about %Q"),&szCommand);
	}
}

void KviServerParser::parseNumericNicknameProblem(KviIrcMessage *msg)
{
	// 433: ERR_NICKNAMEINUSE [I,E,U,D]
	// :prefix 433 <target> <nick> :Nickname is already in use.
	// 432: ERR_ERRONEUSNICKNAME [I,E,U,D]
	// :prefix 433 <target> <nick> :Erroneous nickname

	if(!(msg->console()->isConnected()))
	{
		parseLoginNicknameProblem(msg);
	} else {
		// already connected... just say that we have problems
		if(!msg->haltOutput())
		{
			QString szNk = msg->connection()->decodeText(msg->safeParam(1));
			QString szWText = msg->connection()->decodeText(msg->safeTrailing());
			msg->console()->output(KVI_OUT_NICKNAMEPROBLEM,
				"\r!n\r%Q\r: %Q",&szNk,&szWText);
		}
	}
}

void KviServerParser::parseNumericWhoisAway(KviIrcMessage * msg)
{
// FIXME: #warning "Need an icon here too: sth like KVI_OUT_WHOISSERVER, but with 'A' letter"
	msg->connection()->stateData()->setLastReceivedWhoisReply(kvi_unixTime());

	QString szNk = msg->connection()->decodeText(msg->safeParam(1));
	KviIrcUserDataBase * db = msg->connection()->userDataBase();
	KviIrcUserEntry * e = db->find(szNk);
	if(e)e->setAway(true);
	KviQuery * q = msg->connection()->findQuery(szNk);
	if(q) q->updateLabelText();
	if(!msg->haltOutput())
	{
		KviWindow * pOut = (KviWindow *)(msg->connection()->findQuery(szNk));
		QString szWText = pOut ? pOut->decodeText(msg->safeTrailing()) : msg->connection()->decodeText(msg->safeTrailing());

		if(!pOut)pOut = KVI_OPTION_BOOL(KviOption_boolWhoisRepliesToActiveWindow) ?
			msg->console()->activeWindow() : (KviWindow *)(msg->console());
		pOut->output(KVI_OUT_WHOISUSER,__tr2qs("%c\r!n\r%Q\r%c is away: %Q"),
			KVI_TEXT_BOLD,&szNk,KVI_TEXT_BOLD,&szWText);
	}
}

void KviServerParser::parseNumericWhoisUser(KviIrcMessage *msg)
{
	// 311: RPL_WHOISUSER [I,E,U,D]
	// :prefix 311 <target> <nick> <user> <host> * :<real_name>
	msg->connection()->stateData()->setLastReceivedWhoisReply(kvi_unixTime());

	QString szNick = msg->connection()->decodeText(msg->safeParam(1));
	QString szUser = msg->connection()->decodeText(msg->safeParam(2));
	QString szHost = msg->connection()->decodeText(msg->safeParam(3));
	QString szReal = msg->connection()->decodeText(msg->safeTrailing());
	KviIrcUserDataBase * db = msg->connection()->userDataBase();
	KviIrcUserEntry * e = db->find(szNick);
	if(e)
	{
		e->setUser(szUser);
		e->setHost(szHost);
		e->setRealName(szReal);
		if(e->gender()!=KviIrcUserEntry::Unknown) {
			if(KviQString::equalCS(g_pActiveWindow->className(),QString("KviChannel")))
			{
				((KviChannel*)g_pActiveWindow)->userListView()->updateArea();
			}
		}
		KviQuery * q = msg->connection()->findQuery(szNick);
		if(q) q->updateLabelText();
		if(!e->avatar())
		{
			// FIXME: #warning "THE AVATAR SHOULD BE RESIZED TO MATCH THE MAX WIDTH/HEIGHT"
			// maybe now we can match this user ?
			msg->console()->checkDefaultAvatar(e,szNick,szUser,szHost);
		}
	}

	KviAsyncWhoisInfo * i = msg->connection()->asyncWhoisData()->lookup(szNick);
	if(i)
	{
		i->szNick = szNick;
		i->szUser = szUser;
		i->szHost = szHost;
		i->szReal = szReal;
		return;
	}

	if(!msg->haltOutput())
	{
		KviWindow * pOut = KVI_OPTION_BOOL(KviOption_boolWhoisRepliesToActiveWindow) ?
			msg->console()->activeWindow() : (KviWindow *)(msg->console());
		pOut->output(
			KVI_OUT_WHOISUSER,__tr2qs("%c\r!n\r%Q\r%c is %c\r!n\r%Q\r!%Q@\r!h\r%Q\r%c"),KVI_TEXT_BOLD,
				&szNick,KVI_TEXT_BOLD,KVI_TEXT_UNDERLINE,&szNick,
				&szUser,&szHost,KVI_TEXT_UNDERLINE);

		pOut->output(
			KVI_OUT_WHOISUSER,__tr2qs("%c\r!n\r%Q\r%c's real name: %Q"),KVI_TEXT_BOLD,
				&szNick,KVI_TEXT_BOLD,&szReal);
	}
}

void KviServerParser::parseNumericWhowasUser(KviIrcMessage * msg)
{
	// 314: RPL_WHOWASUSER [I,E,U,D]
	// :prefix 314 <target> <nick> <user> <host> * :<real_name>

	if(!msg->haltOutput())
	{
		QString szNick = msg->connection()->decodeText(msg->safeParam(1));
		QString szUser = msg->connection()->decodeText(msg->safeParam(2));
		QString szHost = msg->connection()->decodeText(msg->safeParam(3));
		QString szReal = msg->connection()->decodeText(msg->safeTrailing());

		KviWindow * pOut = KVI_OPTION_BOOL(KviOption_boolWhoisRepliesToActiveWindow) ?
			msg->console()->activeWindow() : (KviWindow *)(msg->console());
		pOut->output(
			KVI_OUT_WHOISUSER,__tr2qs("%c\r!n\r%Q\r%c was %c\r!n\r%Q\r!%Q@\r!h\r%Q\r%c"),KVI_TEXT_BOLD,
			&szNick,KVI_TEXT_BOLD,KVI_TEXT_UNDERLINE,&szNick,
			&szUser,&szHost,KVI_TEXT_UNDERLINE);
		pOut->output(
			KVI_OUT_WHOISUSER,__tr2qs("%c\r!n\r%Q\r%c's real name was: %Q"),KVI_TEXT_BOLD,
			&szNick,KVI_TEXT_BOLD,&szReal);
	}
}


void KviServerParser::parseNumericWhoisChannels(KviIrcMessage *msg)
{
	// 319: RPL_WHOISCHANNELS [I,E,U,D]
	// :prefix 319 <target> <nick> :<channel list>

	msg->connection()->stateData()->setLastReceivedWhoisReply(kvi_unixTime());

	QString szNick = msg->connection()->decodeText(msg->safeParam(1));
	QString szChans = msg->connection()->decodeText(msg->safeTrailing());
	
	KviAsyncWhoisInfo * i = msg->connection()->asyncWhoisData()->lookup(szNick);
	if(i)
	{
		i->szChannels = szChans;
		return;
	}

	if(!msg->haltOutput())
	{
		KviWindow * pOut = KVI_OPTION_BOOL(KviOption_boolWhoisRepliesToActiveWindow) ?
			msg->console()->activeWindow() : (KviWindow *)(msg->console());
			
		QStringList sl = QStringList::split(" ",szChans);
		QString szChanList;

		for(QStringList::Iterator it = sl.begin();it != sl.end();++it)
		{
			QString szCur = *it;
			// deals with <flag>[#channel] and [##channel]
			int len = szCur.length();
			int i =0;
			while(i < len)
			{
				
				if(IS_CHANNEL_TYPE_FLAG(szCur[i]) && (!IS_USER_MODE_PREFIX(szCur[i])))break;
				i++;
			}
			if(i < len)
			{
				if(i > 0)
				{
					len = szCur.length() - i;
					if(szChanList.length() > 0)szChanList.append(", ");
					szChanList += szCur.left(i);
					QString szR = szCur.right(len);
					KviQString::appendFormatted(szChanList,"\r!c\r%Q\r",&szR); 
				} else {
					if(szChanList.length() > 0)szChanList.append(", ");
					KviQString::appendFormatted(szChanList,"\r!c\r%Q\r",&szCur);
				}
			} else {
				// we dunno what is this.. just append
				if(szChanList.length() > 0)szChanList.append(", ");
				szChanList.append(szCur);
			}
		}

		pOut->output(
			KVI_OUT_WHOISCHANNELS,__tr2qs("%c\r!n\r%Q\r%c's channels: %Q"),KVI_TEXT_BOLD,
				&szNick,KVI_TEXT_BOLD,&szChanList);
	}
}

void KviServerParser::parseNumericWhoisIdle(KviIrcMessage *msg)
{
	// 317: RPL_WHOISIDLE [I,E,U,D]
	// :prefix 317 <target> <nick> <number> <number> :seconds idle, signon time

	// FIXME: #warning "and NICK LINKS"
	msg->connection()->stateData()->setLastReceivedWhoisReply(kvi_unixTime());

	QString szNick = msg->connection()->decodeText(msg->safeParam(1));

	KviAsyncWhoisInfo * i = msg->connection()->asyncWhoisData()->lookup(szNick);
	if(i)
	{
		i->szIdle = msg->safeParam(2);
		i->szSignon = msg->safeParam(3);
		bool bOk = false;
		i->szSignon.toUInt(&bOk);
		if(!bOk)i->szSignon = "";
		return;
	}

	if(!msg->haltOutput())
	{
		KviWindow * pOut = KVI_OPTION_BOOL(KviOption_boolWhoisRepliesToActiveWindow) ?
			msg->console()->activeWindow() : (KviWindow *)(msg->console());
		KviStr idle = msg->safeParam(2); // shouldn't be encoded
		KviStr sign = msg->safeParam(3); // shouldn't be encoded

		bool bOk;
		unsigned int uTime = idle.toUInt(&bOk);
		if(!bOk)
		{
			UNRECOGNIZED_MESSAGE(msg,__tr2qs("Received a broken RPL_WHOISIDLE, can't evaluate the idle time"));
			return;
		}
		unsigned int uDays = uTime / 86400;
		uTime = uTime % 86400;
		unsigned int uHours = uTime / 3600;
		uTime = uTime % 3600;
		unsigned int uMins = uTime / 60;
		uTime = uTime % 60;
		pOut->output(
			KVI_OUT_WHOISIDLE,__tr2qs("%c\r!n\r%Q\r%c's idle time: %ud %uh %um %us"),KVI_TEXT_BOLD,
			&szNick,KVI_TEXT_BOLD,uDays,uHours,uMins,uTime);

		uTime = sign.toUInt(&bOk);
		if(bOk)
		{
			QDateTime dt;
			dt.setTime_t((time_t)uTime);
			QString tmp = dt.toString();
			pOut->output(
				KVI_OUT_WHOISIDLE,__tr2qs("%c\r!n\r%Q\r%c's signon time: %Q"),KVI_TEXT_BOLD,
				&szNick,KVI_TEXT_BOLD,&tmp);
		}
	}
}

void KviServerParser::parseNumericWhoisServer(KviIrcMessage *msg)
{
	// 312: RPL_WHOISSERVER [I,E,U,D] (sent also in response to WHOWAS)
	// :prefix 312 <target> <nick> <server> :<server description / last whowas date>
	msg->connection()->stateData()->setLastReceivedWhoisReply(kvi_unixTime());

	QString szNick = msg->connection()->decodeText(msg->safeParam(1));
	QString szServ = msg->connection()->decodeText(msg->safeParam(2));

	KviIrcUserDataBase * db = msg->connection()->userDataBase();
	KviIrcUserEntry * e = db->find(szNick);
	if(e)e->setServer(szServ);
	KviQuery * q = msg->connection()->findQuery(szNick);
	if(q) q->updateLabelText();
	KviAsyncWhoisInfo * i = msg->connection()->asyncWhoisData()->lookup(msg->safeParam(1));
	if(i)
	{
		i->szServer = szServ;
		return;
	}

	// FIXME: #warning "AWHOIS HERE.... and NICK LINKS"
	if(!msg->haltOutput())
	{
	    	KviWindow * pOut = KVI_OPTION_BOOL(KviOption_boolWhoisRepliesToActiveWindow) ?
		msg->console()->activeWindow() : (KviWindow *)(msg->console());
		QString szWText = pOut->decodeText(msg->safeTrailing());
		pOut->output(
			KVI_OUT_WHOISSERVER,__tr2qs("%c\r!n\r%Q\r%c's server: \r!s\r%Q\r - %Q"),KVI_TEXT_BOLD,
			&szNick,KVI_TEXT_BOLD,&szServ,&szWText);
	}
}

void KviServerParser::parseNumericWhoisAuth(KviIrcMessage *msg)
{
	// :prefix RPL_WHOISAUTH <target> <nick> :is authed as
	// actually seen only on Quakenet
	msg->connection()->stateData()->setLastReceivedWhoisReply(kvi_unixTime());

	QString szNick = msg->connection()->decodeText(msg->safeParam(1));
	QString szAuth = msg->connection()->decodeText(msg->safeParam(2));

	if(!msg->haltOutput())
	{
		KviWindow * pOut = KVI_OPTION_BOOL(KviOption_boolWhoisRepliesToActiveWindow) ?
			msg->console()->activeWindow() : (KviWindow *)(msg->console());
		pOut->output(
			KVI_OUT_WHOISOTHER,__tr2qs("%c\r!n\r%Q\r%c is authenticated as %Q"),KVI_TEXT_BOLD,
			&szNick,KVI_TEXT_BOLD,&szAuth);
	}
}

void KviServerParser::parseNumericWhoisOther(KviIrcMessage *msg)
{
	// *: RPL_WHOIS* [?]
	// :prefix * <target> <nick> :<description>
	// used for RPL_WHOISCHANOP,RPL_WHOISADMIN,
	// RPL_WHOISSADMIN,RPL_WHOISOPERATOR,RPL_WHOISREGNICK,RPL_WHOISSSL
	// and all the other unrecognized codes that look really like a RPL_WHOIS*

	msg->connection()->stateData()->setLastReceivedWhoisReply(kvi_unixTime());

	QString szNick = msg->connection()->decodeText(msg->safeParam(1));
	QString szOth = msg->connection()->decodeText(msg->safeTrailing());

	KviAsyncWhoisInfo * i = msg->connection()->asyncWhoisData()->lookup(szNick);
	if(i)
	{
		if(!(i->szServer.isEmpty()))i->szServer.append(',');
		i->szServer.append(szOth);
		return;
	}

	// FIXME: #warning "NICK LINKS"
	if(!msg->haltOutput())
	{
		KviWindow * pOut = KVI_OPTION_BOOL(KviOption_boolWhoisRepliesToActiveWindow) ?
			msg->console()->activeWindow() : (KviWindow *)(msg->console());
		pOut->output(
			KVI_OUT_WHOISOTHER,__tr2qs("%c\r!n\r%Q\r%c's info: %Q"),KVI_TEXT_BOLD,
			&szNick,KVI_TEXT_BOLD,&szOth);
	}
}

// FIXME: #warning "WHOWAS MISSING"

void KviServerParser::parseNumericEndOfWhois(KviIrcMessage *msg)
{
	// 318: RPL_ENDOFWHOIS [I,E,U,D]
	// :prefix 318 <target> <nick> :End of /WHOIS list

	msg->connection()->stateData()->setLastReceivedWhoisReply(0);

	QString szNick = msg->connection()->decodeText(msg->safeParam(1));

	KviAsyncWhoisInfo * i = msg->connection()->asyncWhoisData()->lookup(szNick);
	if(i)
	{
		if(!g_pApp->windowExists(i->pWindow))i->pWindow = msg->console();
		
		// that's the new KVS engine!
		KviKvsVariantList vl;
		vl.setAutoDelete(true);
		vl.append(new KviKvsVariant(i->szNick));
		vl.append(new KviKvsVariant(i->szUser));
		vl.append(new KviKvsVariant(i->szHost));
		vl.append(new KviKvsVariant(i->szReal));
		vl.append(new KviKvsVariant(i->szServer));
		vl.append(new KviKvsVariant(i->szIdle));
		vl.append(new KviKvsVariant(i->szSignon));
		vl.append(new KviKvsVariant(i->szChannels));
		vl.append(new KviKvsVariant(QString(msg->safePrefix())));
		vl.append(new KviKvsVariant(i->szSpecial));
		vl.append(new KviKvsVariant(*(i->pMagic)));
		i->pCallback->run(i->pWindow,&vl,0,KviKvsScript::PreserveParams);
		msg->connection()->asyncWhoisData()->remove(i);
		return;
	}

	// FIXME: #warning "NICK LINKS"
	if(!msg->haltOutput())
	{
		KviWindow * pOut = KVI_OPTION_BOOL(KviOption_boolWhoisRepliesToActiveWindow) ?
			msg->console()->activeWindow() : (KviWindow *)(msg->console());
		QString pref = msg->connection()->decodeText(msg->safePrefix());
		pOut->output(
			KVI_OUT_WHOISOTHER,__tr2qs("%c\r!n\r%Q\r%c WHOIS info from \r!s\r%Q\r"),KVI_TEXT_BOLD,
			&szNick,KVI_TEXT_BOLD,&pref);
	}
}


void KviServerParser::parseNumericEndOfWhowas(KviIrcMessage *msg)
{
	// 369: RPL_ENDOFWHOWAS [I,E,U,D]
	// :prefix 369 <target> <nick> :End of /WHOWAS list
	if(!msg->haltOutput())
	{
		QString szNick = msg->connection()->decodeText(msg->safeParam(1));
	   	KviWindow * pOut = KVI_OPTION_BOOL(KviOption_boolWhoisRepliesToActiveWindow) ?
			msg->console()->activeWindow() : (KviWindow *)(msg->console());
		QString pref = msg->connection()->decodeText(msg->safePrefix());
		pOut->output(
			KVI_OUT_WHOISOTHER,__tr2qs("%c\r!n\r%Q\r%c WHOWAS info from \r!s\r%Q\r"),KVI_TEXT_BOLD,
			&szNick,KVI_TEXT_BOLD,&pref);
	}
}

void KviServerParser::parseNumericNoSuchNick(KviIrcMessage *msg)
{
	// 401: ERR_NOSUCHNICK [I,E,U,D]
	// 406: ERR_WASNOSUCHNICK [I,E,U,D]
	// :prefix 401 <target> <nick> :No such nick/channel
	// :prefix 406 <target> <nick> :There was no such nickname
	QString szNick = msg->connection()->decodeText(msg->safeParam(1));
	
	if(msg->numeric() == ERR_NOSUCHNICK)
	{
		KviAsyncWhoisInfo * i = msg->connection()->asyncWhoisData()->lookup(szNick);
		if(i)
		{
			if(!g_pApp->windowExists(i->pWindow))i->pWindow = msg->console();
			// that's the new KVS engine!
			KviKvsVariantList vl;
			vl.setAutoDelete(true);
			vl.append(new KviKvsVariant(i->szNick));
			vl.append(new KviKvsVariant());
			vl.append(new KviKvsVariant());
			vl.append(new KviKvsVariant());
			vl.append(new KviKvsVariant());
			vl.append(new KviKvsVariant());
			vl.append(new KviKvsVariant());
			vl.append(new KviKvsVariant());
			vl.append(new KviKvsVariant());
			vl.append(new KviKvsVariant());
			vl.append(new KviKvsVariant(*(i->pMagic)));
			i->pCallback->run(i->pWindow,&vl,0,KviKvsScript::PreserveParams);
			msg->connection()->asyncWhoisData()->remove(i);
			return;
		}
	}
	// FIXME: #warning "KVI_OUT_NOSUCHNICKCHANNEL ?"
	// FIXME: #warning "QUERIES SHOULD REPORT NO TARGET HERE! (?)"
	if(!msg->haltOutput())
	{
		KviWindow * pOut = (KviWindow *)(msg->connection()->findQuery(szNick));
		if(!pOut)
		{
			pOut = (KviWindow *)(msg->console()->activeWindow());
		}
		//} else {
		//	((KviQuery *)pOut)->removeTarget(msg->safeParam(1));
		//}
		QString szWText = pOut->decodeText(msg->safeTrailing());
		pOut->output(KVI_OUT_NICKNAMEPROBLEM,"\r!n\r%Q\r: %Q",
			&szNick,&szWText);
	}
}

void KviServerParser::parseNumericCreationTime(KviIrcMessage *msg)
{
	// 329: RPL_CREATIONTIME
	// :prefix 329 <target> <channel> <creation_time>
	QString szChan = msg->connection()->decodeText(msg->safeParam(1));
	KviChannel * chan = msg->connection()->findChannel(szChan);
	KviStr tmstr = msg->safeParam(2);
	QDateTime dt;
	dt.setTime_t((time_t)tmstr.toUInt());

	if(!tmstr.isUnsignedNum())
	{
		UNRECOGNIZED_MESSAGE(msg,__tr2qs("Can't evaluate creation time"));
		return;
	}
	QString szDate = dt.toString();
	if(chan)
	{
		if(!msg->haltOutput())
		{
			chan->output(KVI_OUT_CREATIONTIME,__tr2qs("Channel was created at %Q"),&szDate);
		}
	} else {
		KviWindow * pOut = KVI_OPTION_BOOL(KviOption_boolServerRepliesToActiveWindow) ?
			msg->console()->activeWindow() : (KviWindow *)(msg->console());
		pOut->output(KVI_OUT_CREATIONTIME,__tr2qs("Channel \r!c\r%Q\r was created at %Q"),
			&szChan,&szDate);
	}
}

void KviServerParser::parseNumericIsOn(KviIrcMessage *msg)
{
	// 303: RPL_ISON
	// :prefix 303 <target> :<ison replies>
	if(msg->connection()->notifyListManager())
	{
		if(msg->connection()->notifyListManager()->handleIsOn(msg))return;
	}
	// not handled...output it

	// FIXME: #warning "OUTPUT IT! (In a suitable way)"
	if(!msg->haltOutput())
	{
		KviWindow * pOut = KVI_OPTION_BOOL(KviOption_boolServerRepliesToActiveWindow) ?
			msg->console()->activeWindow() : (KviWindow *)(msg->console());
		QString szPrefix = msg->connection()->decodeText(msg->safePrefix());
		QString szAllParms = msg->connection()->decodeText(msg->allParams());
		pOut->output(KVI_OUT_UNHANDLED,
			"[%Q][%s] %Q",&szPrefix,msg->command(),&szAllParms);
	}
}

void KviServerParser::parseNumericUserhost(KviIrcMessage *msg)
{
	// 302: RPL_USERHOST
	// :prefix 302 <target> :<userhost replies>
	if(msg->connection()->notifyListManager())
	{
		if(msg->connection()->notifyListManager()->handleUserhost(msg))return;
	}
	// not handled...output it
	if(!msg->haltOutput())
	{
		KviWindow * pOut = KVI_OPTION_BOOL(KviOption_boolServerRepliesToActiveWindow) ?
			msg->console()->activeWindow() : (KviWindow *)(msg->console());
		QString szUser = msg->connection()->decodeText(msg->safeTrailing());
		pOut->output(KVI_OUT_WHOISUSER,__tr2qs("USERHOST info: %Q"),&szUser);
	}
}

void KviServerParser::parseNumericListStart(KviIrcMessage *msg)
{
	// 321: RPL_LISTSTART [I,E,U,D]
	// :prefix 321 <target> :Channel users name
	if(msg->haltOutput())return;  // stopped by raw
	
	if(!(msg->console()->ircContext()->listWindow()))
	{
		// attempt to load the module...
		msg->console()->ircContext()->createListWindow();
	}

	if(msg->console()->ircContext()->listWindow())
	{
		// module loaded
		msg->console()->ircContext()->listWindow()->control(EXTERNAL_SERVER_DATA_PARSER_CONTROL_STARTOFDATA);
	} else {
		msg->console()->output(KVI_OUT_LIST,__tr2qs("Channel list begin: channel, users, topic"));
	}
}

void KviServerParser::parseNumericList(KviIrcMessage *msg)
{
	// 322: RPL_LIST [I,E,U,D]
	// :prefix 364 <target> <channel> <users> :<topic>
	if(msg->haltOutput())return;  // stopped by raw

	if(!(msg->console()->ircContext()->listWindow()))
	{
		// attempt to load the module...
		msg->console()->ircContext()->createListWindow();
		if(msg->console()->ircContext()->listWindow())
		{
			msg->console()->ircContext()->listWindow()->control(EXTERNAL_SERVER_DATA_PARSER_CONTROL_STARTOFDATA);
		}
	}

	if(msg->console()->ircContext()->listWindow())
	{
		// module loaded
		msg->console()->ircContext()->listWindow()->processData(msg);
	} else {
		// ops...can't load the module...
		QString szList = msg->connection()->decodeText(msg->allParams());
		msg->console()->output(KVI_OUT_LIST,__tr2qs("List: %Q"),&szList);
	}
}

void KviServerParser::parseNumericListEnd(KviIrcMessage *msg)
{
	// 323: RPL_LISTEND [I,E,U,D]
	// :prefix 323 <target> :End of /LIST
	if(msg->haltOutput())return;  // stopped by raw

	if(msg->console()->ircContext()->listWindow())
	{
		msg->console()->ircContext()->listWindow()->control(EXTERNAL_SERVER_DATA_PARSER_CONTROL_ENDOFDATA);
	} else {
		msg->console()->output(KVI_OUT_LIST,__tr2qs("End of LIST"));
	}

}

void KviServerParser::parseNumericLinks(KviIrcMessage *msg)
{
	// 364: RPL_LINKS [I,E,U,D]
	// :prefix 364 <target> <host> <parent> :<hops> <description>
	if(!(msg->console()->ircContext()->linksWindow()))
	{
		// attempt to load the module...
		msg->console()->ircContext()->createLinksWindow();
	}

	if(msg->console()->ircContext()->linksWindow())
	{
		// module loaded
		msg->console()->ircContext()->linksWindow()->processData(msg);
	} else {
		// ops...can't load the module... or the event halted the window creation
		if(!msg->haltOutput())
		{
			QString szList = msg->connection()->decodeText(msg->allParams());
			msg->console()->output(KVI_OUT_LINKS,__tr2qs("Link: %Q"),&szList);
		}
	}
}

void KviServerParser::parseNumericEndOfLinks(KviIrcMessage *msg)
{
	// 365: RPL_ENDOFLINKS [I,E,U,D]
	// :prefix 365 <target> :End of /LINKS
	if(msg->console()->ircContext()->linksWindow())
	{
		msg->console()->ircContext()->linksWindow()->control(EXTERNAL_SERVER_DATA_PARSER_CONTROL_ENDOFDATA);
	} else {
		if(!msg->haltOutput())
		{
			msg->console()->output(KVI_OUT_LINKS,__tr2qs("End of LINKS"));
		}
	}
}

void KviServerParser::parseNumericBackFromAway(KviIrcMessage * msg)
{
	// 305: RPL_UNAWAY [I,E,U,D]
	// :prefix 305 <target> :You are no longer away
	bool bWasAway = msg->connection()->userInfo()->isAway();
	QString szNickBeforeAway;
	QString szWText = msg->connection()->decodeText(msg->safeTrailing());

	if(bWasAway)szNickBeforeAway = msg->connection()->userInfo()->nickNameBeforeAway();
	msg->connection()->changeAwayState(false);

	// trigger the event
	QString tmp;
	KviQString::sprintf(tmp,"%u",bWasAway ? (unsigned int)(msg->connection()->userInfo()->awayTime()) : 0);
	if(KVS_TRIGGER_EVENT_2_HALTED(KviEvent_OnMeBack,msg->console(),tmp,szWText))
		msg->setHaltOutput();
	if(!msg->haltOutput())
	{
		KviWindow * pOut = KVI_OPTION_BOOL(KviOption_boolServerRepliesToActiveWindow) ?
			msg->console()->activeWindow() : (KviWindow *)(msg->console());

		if(bWasAway)
		{
			int uTimeDiff = bWasAway ? (kvi_unixTime() - msg->connection()->userInfo()->awayTime()) : 0;
			pOut->output(KVI_OUT_AWAY,__tr2qs("[Leaving away status after %ud %uh %um %us]: %Q"),
					uTimeDiff / 86400,(uTimeDiff % 86400) / 3600,(uTimeDiff % 3600) / 60,uTimeDiff % 60,
					&szWText);
		} else {
			pOut->output(KVI_OUT_AWAY,__tr2qs("[Leaving away status]: %Q"),&szWText);
		}
	}

	if(KVI_OPTION_BOOL(KviOption_boolChangeNickAway) && bWasAway && (!(szNickBeforeAway.isEmpty())))
	{
		if(_OUTPUT_PARANOIC)
			msg->console()->output(KVI_OUT_AWAY,__tr2qs("Restoring pre-away nickname (%Q)"),&szNickBeforeAway);
		KviQCString szDat = msg->connection()->encodeText(szNickBeforeAway);
		msg->connection()->sendFmtData("NICK %s",szDat.data());
	}

}

void KviServerParser::parseNumericAway(KviIrcMessage * msg)
{
	// 306: RPL_NOWAWAY [I,E,U,D]
	// :prefix 305 <target> :You're away man
	msg->connection()->changeAwayState(true);
	QString szWText = msg->connection()->decodeText(msg->safeTrailing());
	
	if(KVS_TRIGGER_EVENT_1_HALTED(KviEvent_OnMeAway,msg->console(),szWText))msg->setHaltOutput();

	if(!msg->haltOutput())
	{
		KviWindow * pOut = KVI_OPTION_BOOL(KviOption_boolServerRepliesToActiveWindow) ?
			msg->console()->activeWindow() : (KviWindow *)(msg->console());
		pOut->output(KVI_OUT_AWAY,__tr2qs("[Entering away status]: %Q"),&szWText);
	}

	if(KVI_OPTION_BOOL(KviOption_boolChangeNickAway))
	{
		QString nick = msg->connection()->decodeText(msg->safeParam(0));
		QString szNewNick;
		if(KVI_OPTION_BOOL(KviOption_boolAutoGeneratedAwayNick))
		{
			if(nick.length() > 5)szNewNick = nick.left(5);
			else szNewNick = nick;
			szNewNick.append("AWAY");
		} else {
			szNewNick = KVI_OPTION_STRING(KviOption_stringCustomAwayNick);
			szNewNick.replace("%nick%",nick);
		}

		if(_OUTPUT_PARANOIC)
			msg->console()->output(KVI_OUT_AWAY,__tr2qs("Setting away nickname (%Q)"),&szNewNick);
		KviQCString dat = msg->connection()->encodeText(szNewNick);
		msg->connection()->sendFmtData("NICK %s",dat.data());
	}
}

void KviServerParser::parseNumericWatch(KviIrcMessage *msg)
{
	// 600: RPL_LOGON
	// :prefix 600 <target> <nick> <user> <host> <logintime> :logged online
	// 601: RPL_LOGON
	// :prefix 601 <target> <nick> <user> <host> <logintime> :logged offline
	// 602: RPL_WATCHOFF
	// :prefix 602 <target> <nick> <user> <host> <logintime> :stopped watching
	// 604: PRL_NOWON
	// :prefix 604 <target> <nick> <user> <host> <logintime> :is online
	// 605: PRL_NOWOFF
	// :prefix 605 <target> <nick> <user> <host> 0 :is offline

	if(msg->connection()->notifyListManager())
	{
		if(msg->connection()->notifyListManager()->handleWatchReply(msg))return;
	}
	// not handled...output it

// FIXME: #warning "OUTPUT IT! (In a suitable way) (And handle 602 , 603 , 606 and 607 gracefully)"
	if(!msg->haltOutput())
	{
		KviWindow * pOut = KVI_OPTION_BOOL(KviOption_boolServerRepliesToActiveWindow) ?
			msg->console()->activeWindow() : (KviWindow *)(msg->console());
		pOut->output(KVI_OUT_UNHANDLED,
			"[%s][%s] %s",msg->prefix(),msg->command(),msg->allParams());
	}
}

void KviServerParser::parseNumericStats(KviIrcMessage * msg)
{
	if(!msg->haltOutput())
	{
		KviWindow * pOut = (KviWindow *)(msg->console());
		if(msg->paramCount() > 2)
		{
			KviStr szParms;
			KviStr *p = msg->firstParam();
			foreach(p,*(msg->params()))
			{
				if(szParms.hasData())szParms.append(' ');
				szParms.append(*p);
			}
			pOut->outputNoFmt(KVI_OUT_STATS,msg->connection()->decodeText(szParms).utf8().data());
		} else {
			pOut->outputNoFmt(KVI_OUT_STATS,msg->connection()->decodeText(msg->safeTrailing()).utf8().data());
		}
	}
}

void KviServerParser::parseNumericServerAdminInfoTitle(KviIrcMessage * msg)
{
	//RPL_ADMINME          256
	if(!msg->haltOutput())
	{
		KviWindow * pOut = (KviWindow *)(msg->console());
			pOut->outputNoFmt(KVI_OUT_SERVERINFO,msg->connection()->decodeText(msg->safeTrailing()).utf8().data());
	}
}
void KviServerParser::parseNumericServerAdminInfoServerName(KviIrcMessage * msg)
{
	//RPL_ADMINLOC1        257
	if(!msg->haltOutput())
	{
		KviWindow * pOut = (KviWindow *)(msg->console());
			QString szInfo = msg->connection()->decodeText(msg->safeTrailing());
			pOut->output(KVI_OUT_SERVERINFO,__tr2qs("%c\r!s\r%s\r%c's server info: %s"),KVI_TEXT_BOLD,msg->prefix(),KVI_TEXT_BOLD,szInfo.utf8().data());
	}
}

void KviServerParser::parseNumericServerAdminInfoAdminName(KviIrcMessage * msg)
{
	//RPL_ADMINLOC2        258
	if(!msg->haltOutput())
	{
		KviWindow * pOut = (KviWindow *)(msg->console());
		QString szInfo = msg->connection()->decodeText(msg->safeTrailing());
		pOut->output(KVI_OUT_SERVERINFO,__tr2qs("%c\r!s\r%s\r%c's administrator is %s"),KVI_TEXT_BOLD,msg->prefix(),KVI_TEXT_BOLD,szInfo.utf8().data());
	}
}

void KviServerParser::parseNumericServerAdminInfoAdminContact(KviIrcMessage * msg)
{
	//RPL_ADMINEMAIL       259
	if(!msg->haltOutput())
	{
		KviWindow * pOut = (KviWindow *)(msg->console());
			QString szInfo = msg->connection()->decodeText(msg->safeTrailing());
			pOut->output(KVI_OUT_SERVERINFO,__tr2qs("%c\r!s\r%s\r%c's contact adress is %s"),KVI_TEXT_BOLD,msg->prefix(),KVI_TEXT_BOLD,szInfo.utf8().data());
	}
}

void KviServerParser::parseNumericCommandSyntax(KviIrcMessage * msg)
{
	//RPL_COMMANDSYNTAX    334
	if(!msg->haltOutput())
	{
		KviWindow * pOut = (KviWindow *)(msg->console());
		pOut->outputNoFmt(KVI_OUT_STATS,msg->connection()->decodeText(msg->safeTrailing()));
	}
}

void KviServerParser::parseNumericInviting(KviIrcMessage * msg)
{
	//RPL_INVITING         341
	if(!msg->haltOutput())
	{
		QString szWho = msg->connection()->decodeText(msg->safeParam(0));
		QString szTarget = msg->connection()->decodeText(msg->safeParam(1));
		QString szChan = msg->connection()->decodeText(msg->safeParam(2));
		KviChannel * chan = msg->connection()->findChannel(szChan);
		if(chan)
		{
			chan->output(KVI_OUT_INVITE,__tr2qs("\r!n\r%Q\r invited %Q into channel %Q"),&szWho,&szTarget,&szChan);
		} else {
			KviWindow * pOut = (KviWindow *)(msg->console());
			pOut->output(KVI_OUT_INVITE,__tr2qs("\r!n\r%Q\r invited %Q into channel %Q"),&szWho,&szTarget,&szChan);
		}
	}
}

void KviServerParser::parseNumericInfo(KviIrcMessage * msg)
{
	//RPL_INFO             371
	if(!msg->haltOutput())
	{
		KviWindow * pOut = (KviWindow *)(msg->console());
		QString szInfo = msg->connection()->decodeText(msg->safeTrailing());
		pOut->outputNoFmt(KVI_OUT_SERVERINFO,szInfo);
	}
}

void KviServerParser::parseNumericInfoStart(KviIrcMessage * msg)
{
	//RPL_INFOSTART        373
	if(!msg->haltOutput())
	{
		KviWindow * pOut = (KviWindow *)(msg->console());
		pOut->output(KVI_OUT_SERVERINFO,__tr2qs("%c\r!s\r%s\r%c's information:"),KVI_TEXT_BOLD,msg->prefix(),KVI_TEXT_BOLD);
	}
}

void KviServerParser::parseNumericInfoEnd(KviIrcMessage * msg)
{
	//RPL_ENDOFINFO        374
	if(!msg->haltOutput())
	{
		KviWindow * pOut = (KviWindow *)(msg->console());
		pOut->output(KVI_OUT_SERVERINFO,__tr2qs("End of %c\r!s\r%s\r%c's information"),KVI_TEXT_BOLD,msg->prefix(),KVI_TEXT_BOLD);
	}
}

void KviServerParser::parseNumericTime(KviIrcMessage * msg)
{
	//RPL_TIME             391
	if(!msg->haltOutput())
	{
		KviWindow * pOut = (KviWindow *)(msg->console());
		QString szInfo = msg->connection()->decodeText(msg->safeTrailing());
		pOut->output(KVI_OUT_SERVERINFO,__tr2qs("%c\r!s\r%s\r%c's time is %Q"),KVI_TEXT_BOLD,msg->prefix(),KVI_TEXT_BOLD,&szInfo);
	}
}

void KviServerParser::parseNumericNoSuchServer(KviIrcMessage * msg)
{
	//ERR_NOSUCHSERVER     402
	if(!msg->haltOutput())
	{
		KviWindow * pOut = (KviWindow *)(msg->console());
		QString szWhat = msg->connection()->decodeText(msg->safeParam(1));
		pOut->output(KVI_OUT_GENERICERROR,__tr2qs("%Q: no such server"),&szWhat);
	}
}

void KviServerParser::parseNumericNoSuchChannel(KviIrcMessage * msg)
{
	// ERR_NOSUCHCHANNEL    403
	if(!msg->haltOutput())
	{
		KviWindow * pOut = (KviWindow *)(msg->console());
		QString szWhat = msg->connection()->decodeText(msg->safeParam(1));
		pOut->output(KVI_OUT_GENERICERROR,__tr2qs("%Q: no such channel"),&szWhat);
	}
}

void KviServerParser::parseNumericCannotSendColor(KviIrcMessage * msg)
{
	// ERR_NOCOLORSONCHAN   408
	if(!msg->haltOutput())
	{
		QString szChan = msg->connection()->decodeText(msg->safeParam(1));
		QString szInfo = msg->connection()->decodeText(msg->safeTrailing());
		KviChannel * chan = msg->connection()->findChannel(szChan);
		if(chan)
		{
			chan->output(KVI_OUT_GENERICERROR,__tr2qs("Cannot sent to channel: %Q"),&szInfo);
		} else {
			KviWindow * pOut = (KviWindow *)(msg->console());
			pOut->output(KVI_OUT_GENERICERROR,__tr2qs("Cannot sent text to channel %Q: %Q"),&szChan,&szInfo);
		}
	}
}

void KviServerParser::parseNumericCannotSend(KviIrcMessage * msg)
{
	// ERR_CANNOTSENDTOCHAN 404
	if(!msg->haltOutput())
	{
		QString szChan = msg->connection()->decodeText(msg->safeParam(1));
		QString szInfo = msg->connection()->decodeText(msg->safeTrailing());
		KviChannel * chan = msg->connection()->findChannel(szChan);
		if(chan)
		{
			chan->output(KVI_OUT_GENERICERROR,__tr2qs("Cannot sent to channel"));
		} else {
			KviWindow * pOut = (KviWindow *)(msg->console());
			pOut->output(KVI_OUT_GENERICERROR,__tr2qs("Cannot sent text to channel %Q"),&szChan);
		}
	}
}


void KviServerParser::parseNumericCodePageSet(KviIrcMessage *msg)
{
	// a nice extension for irc.wenet.ru
	// 700: RPL_CODEPAGESET
	// :prefix 700 target <encoding> :is now your translation scheme
	
	QString encoding = msg->connection()->decodeText(msg->safeParam(1));
	if(msg->connection()->serverInfo()->supportsCodePages())
	{
		if(encoding=="NONE") encoding="KOI8-R"; //RusNet default codepage
		msg->console()->output(KVI_OUT_TEXTENCODING,__tr2qs("Your encoding is now %Q"),&encoding);
		msg->console()->setTextEncoding(encoding);
		msg->connection()->setEncoding(encoding);
	} else {
		QString szMe = msg->connection()->decodeText(msg->safeParam(0));
		if( (szMe==msg->connection()->currentNickName() || szMe == "*" ) //fix for pre-login codepage message
			&& KviLocale::codecForName(encoding.utf8().data()))
		{
			msg->console()->output(KVI_OUT_TEXTENCODING,__tr2qs("Your encoding is now %Q"),&encoding);
			msg->console()->setTextEncoding(encoding);
			msg->connection()->setEncoding(encoding);
		} else if(!msg->haltOutput()) // simply unhandled
		{
			QString szWText = msg->connection()->decodeText(msg->allParams());
			msg->connection()->console()->output(KVI_OUT_UNHANDLED,
				"[%s][%s] %Q",msg->prefix(),msg->command(),&szWText);
		}
	}
}

void KviServerParser::parseNumericCodePageScheme(KviIrcMessage *msg)
{
	// a nice extension for irc.wenet.ru
	// 703: RPL_WHOISSCHEME
	// :prefix 703 <mynick> <nick> <encoding> :translation scheme

	msg->connection()->stateData()->setLastReceivedWhoisReply(kvi_unixTime());

	if(msg->connection()->serverInfo()->supportsCodePages())
	{
		QString szNick = msg->connection()->decodeText(msg->safeParam(1));
		QString szCodepage = msg->connection()->decodeText(msg->safeParam(2));
	
		if(!msg->haltOutput())
		{
			KviWindow * pOut = KVI_OPTION_BOOL(KviOption_boolWhoisRepliesToActiveWindow) ?
			msg->console()->activeWindow() : (KviWindow *)(msg->console());
			QString szWText = pOut->decodeText(msg->safeTrailing());
			pOut->output(
				KVI_OUT_WHOISOTHER,__tr2qs("%c\r!n\r%Q\r%c's codepage is %Q: %Q"),KVI_TEXT_BOLD,
				&szNick,KVI_TEXT_BOLD,&szCodepage,&szWText);
		}
	} else {
		// simply unhandled
		if(!msg->haltOutput())
		{
			QString szWText = msg->connection()->decodeText(msg->allParams());
			msg->connection()->console()->output(KVI_OUT_UNHANDLED,
				"[%s][%s] %Q",msg->prefix(),msg->command(),&szWText);
		}
	}
}

void KviServerParser::parseNumericUserMode(KviIrcMessage *msg)
{
	// 321: RPL_UMODEIS [I,E,U,D]
	// :prefix 221 <target> <modeflags>
	parseUserMode(msg,msg->safeParam(1));

	if(!msg->haltOutput())
	{
		KviWindow * pOut = KVI_OPTION_BOOL(KviOption_boolServerRepliesToActiveWindow) ?
			msg->console()->activeWindow() : (KviWindow *)(msg->console());
		pOut->output(KVI_OUT_MODE,__tr2qs("Your user mode is %s"),msg->safeParam(1));
	}
}

void KviServerParser::parseNumericEndOfStats(KviIrcMessage *msg)
{
	if(!msg->haltOutput())
	{
		KviWindow * pOut = (KviWindow *)(msg->console());
		QString szText = msg->connection()->decodeText(msg->safeTrailing());
		pOut->outputNoFmt(KVI_OUT_STATS, szText);
	}
}
