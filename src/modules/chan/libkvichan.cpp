//=============================================================================
//
//   File : libkvichan.cpp
//   Creation date : Sun Feb 02 2002 05:27:11 GMT by Szymon Stefanek
//
//   This chan is part of the KVirc irc client distribution
//   Copyright (C) 2002-2005 Szymon Stefanek (pragma@kvirc.net)
//   Copyright (C) 2002-2004 Juanjo Alvarez  (juanjux@yahoo.es)
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

#include "kvi_module.h"
#include "kvi_string.h"
#include "qapplication.h" //broken mingw compiler?
#include "kvi_app.h"
#include "kvi_channel.h"
#include "kvi_locale.h"
#include "kvi_topicw.h"
#include "kvi_ircmask.h"
#include "kvi_maskeditor.h"
#include "kvi_ircurl.h"
#include "kvi_ircconnectiontarget.h"
#include "kvi_ircconnection.h"

static KviChannel * chan_kvs_find_channel(KviKvsModuleFunctionCall * c,QString &szChan,bool noWarnings=false)
{
	if(szChan.isEmpty())
	{
		if(c->window()->type() == KVI_WINDOW_TYPE_CHANNEL)return (KviChannel *)(c->window());
		if(c->window()->type() == KVI_WINDOW_TYPE_DEADCHANNEL)return (KviChannel *)(c->window());
		if(!noWarnings) c->warning(__tr2qs("The current window is not a channel"));
	} else {
		KviWindow * w = g_pApp->findWindow(szChan);
		if(!w)
		{
			if(!noWarnings) c->warning(__tr2qs("Can't find the window with id '%Q'"),&szChan);
			return 0;
		}
		if(w->type() == KVI_WINDOW_TYPE_CHANNEL)return (KviChannel *)w;
		if(!noWarnings) c->warning(__tr2qs("The specified window (%Q) is not a channel"),&szChan);
	}
	return 0;
}

/*
	@doc: chan.name
	@type:
		function
	@title:
		$chan.name
	@short:
		Returns the name of a channel
	@syntax:
		<string> $chan.name
		<string> $chan.name(<window_id:string>)
	@description:
		The first form returns the name of the current channel (assuming that the current window
		is a channel at all). If the current window is not a channel, a warning is printed
		and an empty string is returned.[br]
		The second form returns the name of the channel specified by <window_id>.[br]
		This function works also on dead channels.[br]
*/

static bool chan_kvs_fnc_name(KviKvsModuleFunctionCall * c)
{ 
	QString szId;
	KVSM_PARAMETERS_BEGIN(c)
		KVSM_PARAMETER("window id",KVS_PT_STRING,KVS_PF_OPTIONAL,szId)
	KVSM_PARAMETERS_END(c)
	KviChannel * ch = chan_kvs_find_channel(c,szId);
	if (ch) {
		c->returnValue()->setString(ch->windowName());
	}
	return true;
}

/*
	@doc: chan.getUrl
	@type:
		function
	@title:
		$chan.getUrl
	@short:
		Returns the URL of a channel
	@syntax:
		<string> $chan.getUrl
		<string> $chan.getUrl(<window_id:string>)
		<string> $chan.getUrl(<channel_name:string>)
	@description:
		Returns IRC URL for this channel
*/

static bool chan_kvs_fnc_getUrl(KviKvsModuleFunctionCall * c)
{ 
	QString szId,url;
	KVSM_PARAMETERS_BEGIN(c)
		KVSM_PARAMETER("channel id",KVS_PT_STRING,KVS_PF_OPTIONAL,szId)
	KVSM_PARAMETERS_END(c)
	KviChannel * ch = chan_kvs_find_channel(c,szId,true);
	if (ch) {
		KviIrcUrl::join(url,ch->connection()->target()->server());
		url.append(ch->windowName());
		if(ch->hasChannelKey())
		{
			url.append('?');
			url.append(ch->channelKey());
		}
	} else {
		if(c->window()->connection()) {
			KviIrcUrl::join(url,c->window()->connection()->target()->server());
			url.append(szId);
		} else {
			c->error("There is no active IRC connection for current context");
		}
	}
	c->returnValue()->setString(url);
	return true;
}

/*
	@doc: chan.isDead
	@type:
		function
	@title:
		$chan.isDead
	@short:
		Checks if a channel is dead
	@syntax:
		<boolean> $chan.isDead
		<boolean> $chan.isDead(<window_id:string>)
	@description:
		Returns 1 if the channel specified by <window_id> is a dead channel and 0 otherwise.[br]
		The form without parameters works on the current window.[br]
*/

static bool chan_kvs_fnc_isdead(KviKvsModuleFunctionCall * c)
{ 
	QString szId;
	KVSM_PARAMETERS_BEGIN(c)
		KVSM_PARAMETER("window id",KVS_PT_STRING,KVS_PF_OPTIONAL,szId)
	KVSM_PARAMETERS_END(c)
	KviChannel * ch = chan_kvs_find_channel(c,szId);
	if (ch) c->returnValue()->setBoolean((ch->type() == KVI_WINDOW_TYPE_DEADCHANNEL));
	return true;
}

/*
	@doc: chan.topic
	@type:
		function
	@title:
		$chan.topic
	@short:
		Returns the topic of a channel
	@syntax:
		<string> $chan.topic
		<string> $chan.topic(<window_id:string>)
	@description:
		The first form returns the topic of the current channel (assuming that the current window
		is a channel at all). If the current window is not a channel, a warning is printed
		and an empty string is returned.[br]
		The second form returns the topic of the channel specified by <window_id>.[br]
		The topic is returned as it is known form at the call time: this means that 
		if the channel is not synchronized with the server (as just after the join, for example)
		you might get an empty string (the topic is not yet known).[br]
		This function works also on dead channels altough the topic returned is the last
		topic seen while the channel wasn't dead.[br]
*/

static bool chan_kvs_fnc_topic(KviKvsModuleFunctionCall * c)
{ 
	QString szId;
	KVSM_PARAMETERS_BEGIN(c)
		KVSM_PARAMETER("window id",KVS_PT_STRING,KVS_PF_OPTIONAL,szId)
	KVSM_PARAMETERS_END(c)
	KviChannel * ch = chan_kvs_find_channel(c,szId);
	if (ch) c->returnValue()->setString(ch->topicWidget()->topic());
	return true;
}

/*
	@doc: chan.topicsetby
	@type:
		function
	@title:
		$chan.topicsetby
	@short:
		Returns the author of the topic of a channel
	@syntax:
		<string> $chan.topicsetby
		<string> $chan.topicsetby(<window_id:string>)
	@description:
		The first form returns the author of the topic of the current channel (assuming that the current window
		is a channel at all). If the current window is not a channel, a warning is printed
		and an empty string is returned.[br]
		The second form returns the author of the topic of the channel specified by <window_id>.[br]
		The topic author nickname is returned if it is known form at the call time: this means that 
		if the channel is not synchronized with the server (as just after the join, for example)
		you might get an empty string (the topic is not yet known).[br]
		This function works also on dead channels altough the information returned is the last
		information seen while the channel wasn't dead.[br]
*/

static bool chan_kvs_fnc_topicsetby(KviKvsModuleFunctionCall * c)
{ 
	QString szId;
	KVSM_PARAMETERS_BEGIN(c)
		KVSM_PARAMETER("window id",KVS_PT_STRING,KVS_PF_OPTIONAL,szId)
	KVSM_PARAMETERS_END(c)
	KviChannel * ch = chan_kvs_find_channel(c,szId);
	if (ch) c->returnValue()->setString(ch->topicWidget()->topicSetBy());
	return true;
}

/*
	@doc: chan.topicsetat
	@type:
		function
	@title:
		$chan.topicsetat
	@short:
		Returns the set time of the topic of a channel
	@syntax:
		<string> $chan.topicsetat
		<string> $chan.topicsetat(<window_id:string>)
	@description:
		The first form returns the set time of the topic of the current channel (assuming that the current window
		is a channel at all). If the current window is not a channel, a warning is printed
		and an empty string is returned.[br]
		The second form returns the set time of the topic of the channel specified by <window_id>.[br]
		The topic set time is returned if it is known form at the call time: this means that 
		if the channel is not synchronized with the server (as just after the join, for example)
		you might get an empty string (the topic is not yet known).[br]
		This function works also on dead channels altough the information returned is the last
		information seen while the channel wasn't dead.[br]
*/

static bool chan_kvs_fnc_topicsetat(KviKvsModuleFunctionCall * c)
{ 
	QString szId;
	KVSM_PARAMETERS_BEGIN(c)
		KVSM_PARAMETER("window id",KVS_PT_STRING,KVS_PF_OPTIONAL,szId)
	KVSM_PARAMETERS_END(c)
	KviChannel * ch = chan_kvs_find_channel(c,szId);
	if (ch) c->returnValue()->setString(ch->topicWidget()->topicSetAt());
	return true;
}
/*
	@doc: chan.usercount
	@type:
		function
	@title:
		$chan.usercount
	@short:
		Returns the number of users on a channel
	@syntax:
		<intger> $chan.usercount
		<integer> $chan.usercount(<window_id:string>)
	@description:
		The first form returns the number of users on the current channel (assuming that the current window
		is a channel at all). If the current window is not a channel, a warning is printed
		and an empty string is returned.[br]
		The second form returns the number of users on the channel specified by <window_id>.[br]
		The number of users is returned if it is known form at the call time: this means that 
		if the channel is not synchronized with the server (as just after the join, for example)
		you might get a number that is actually smaller.[br]
*/

static bool chan_kvs_fnc_usercount(KviKvsModuleFunctionCall * c)
{ 
	QString szId;
	KVSM_PARAMETERS_BEGIN(c)
		KVSM_PARAMETER("window id",KVS_PT_STRING,KVS_PF_OPTIONAL,szId)
	KVSM_PARAMETERS_END(c)
	KviChannel * ch = chan_kvs_find_channel(c,szId);
	if (ch) c->returnValue()->setInteger(ch->count());
	return true;
}

/*
	@doc: chan.ownercount
	@type:
		function
	@title:
		$chan.ownercount
	@short:
		Returns the number of channel owner users on a channel
	@syntax:
		<integer> $chan.ownercount
		<integer> $chan.ownercount(<window_id:integer>)
	@description:
		The first form returns the number of owners users on the current channel (assuming that the current window
		is a channel at all). If the current window is not a channel, a warning is printed
		and an empty string is returned.[br]
		The second form returns the number of owners users on the channel specified by <window_id>.[br]
		The number of owners is returned if it is known form at the call time: this means that 
		if the channel is not synchronized with the server (as just after the join, for example)
		you might get a number that is actually smaller.[br]
*/

static bool chan_kvs_fnc_ownercount(KviKvsModuleFunctionCall * c)
{ 
	QString szId;
	KVSM_PARAMETERS_BEGIN(c)
		KVSM_PARAMETER("window id",KVS_PT_STRING,KVS_PF_OPTIONAL,szId)
	KVSM_PARAMETERS_END(c)
	KviChannel * ch = chan_kvs_find_channel(c,szId);
	if (ch) c->returnValue()->setInteger(ch->chanOwnerCount());
	return true;
}

/*
	@doc: chan.admincount
	@type:
		function
	@title:
		$chan.admincount
	@short:
		Returns the number of channel admin users on a channel
	@syntax:
		<integer> $chan.admincount
		<integer> $chan.admincount(<window_id:string>)
	@description:
		The first form returns the number of administrator users on the current channel (assuming that the current window
		is a channel at all). If the current window is not a channel, a warning is printed
		and an empty string is returned.[br]
		The second form returns the number of administrator users on the channel specified by <window_id>.[br]
		The number of administrators is returned if it is known form at the call time: this means that 
		if the channel is not synchronized with the server (as just after the join, for example)
		you might get a number that is actually smaller.[br]
*/

static bool chan_kvs_fnc_admincount(KviKvsModuleFunctionCall * c)
{ 
	QString szId;
	KVSM_PARAMETERS_BEGIN(c)
		KVSM_PARAMETER("window id",KVS_PT_STRING,KVS_PF_OPTIONAL,szId)
	KVSM_PARAMETERS_END(c)
	KviChannel * ch = chan_kvs_find_channel(c,szId);
	if (ch) c->returnValue()->setInteger(ch->chanAdminCount());
	return true;
}


/*
	@doc: chan.opcount
	@type:
		function
	@title:
		$chan.opcount
	@short:
		Returns the number of op users on a channel
	@syntax:
		<integer> $chan.opcount
		<integer> $chan.opcount(<window_id:string>)
	@description:
		The first form returns the number of op users on the current channel (assuming that the current window
		is a channel at all). If the current window is not a channel, a warning is printed
		and an empty string is returned.[br]
		The second form returns the number of op users on the channel specified by <window_id>.[br]
		The number of ops is returned if it is known form at the call time: this means that 
		if the channel is not synchronized with the server (as just after the join, for example)
		you might get a number that is actually smaller.[br]
*/

static bool chan_kvs_fnc_opcount(KviKvsModuleFunctionCall * c)
{ 
	QString szId;
	KVSM_PARAMETERS_BEGIN(c)
		KVSM_PARAMETER("window id",KVS_PT_STRING,KVS_PF_OPTIONAL,szId)
	KVSM_PARAMETERS_END(c)
	KviChannel * ch = chan_kvs_find_channel(c,szId);
	if (ch) c->returnValue()->setInteger(ch->opCount());
	return true;
}

/*
	@doc: chan.voicecount
	@type:
		function
	@title:
		$chan.voicecount
	@short:
		Returns the number of voiced users on a channel
	@syntax:
		<integer> $chan.voicecount
		<integer> $chan.voicecount(<window_id:string>)
	@description:
		The first form returns the number of voiced users on the current channel (assuming that the current window
		is a channel at all). If the current window is not a channel, a warning is printed
		and an empty string is returned.[br]
		The second form returns the number of voiced users on the channel specified by <window_id>.[br]
		The number of voiced users is returned if it is known form at the call time: this means that 
		if the channel is not synchronized with the server (as just after the join, for example)
		you might get a number that is actually smaller.[br]
*/

static bool chan_kvs_fnc_voicecount(KviKvsModuleFunctionCall * c)
{ 
	QString szId;
	KVSM_PARAMETERS_BEGIN(c)
		KVSM_PARAMETER("window id",KVS_PT_STRING,KVS_PF_OPTIONAL,szId)
	KVSM_PARAMETERS_END(c)
	KviChannel * ch = chan_kvs_find_channel(c,szId);
	if (ch) c->returnValue()->setInteger(ch->voiceCount());
	return true;
}

/*
	@doc: chan.halfopcount
	@type:
		function
	@title:
		$chan.halfopcount
	@short:
		Returns the number of halfop users on a channel
	@syntax:
		<integer> $chan.halfOpCount
		<integer> $chan.halfOpCount(<window_id:string>)
	@description:
		The first form returns the number of half-operator users on the current channel (assuming that the current window
		is a channel at all). If the current window is not a channel, a warning is printed
		and an empty string is returned.[br]
		The second form returns the number of half-operator users on the channel specified by <window_id>.[br]
		The number of half-operator users is returned if it is known form at the call time: this means that 
		if the channel is not synchronized with the server (as just after the join, for example)
		you might get a number that is actually smaller.[br]
*/

static bool chan_kvs_fnc_halfopcount(KviKvsModuleFunctionCall * c)
{ 
	QString szId;
	KVSM_PARAMETERS_BEGIN(c)
		KVSM_PARAMETER("window id",KVS_PT_STRING,KVS_PF_OPTIONAL,szId)
	KVSM_PARAMETERS_END(c)
	KviChannel * ch = chan_kvs_find_channel(c,szId);
	if (ch) c->returnValue()->setInteger(ch->halfOpCount());
	return true;
}

/*
	@doc: chan.useropcount
	@type:
		function
	@title:
		$chan.useropcount
	@short:
		Returns the number of userop users on a channel
	@syntax:
		<integer> $chan.userOpCount
		<integer> $chan.userOpCount(<window_id:string>)
	@description:
		The first form returns the number of user-operator users on the current channel (assuming that the current window
		is a channel at all). If the current window is not a channel, a warning is printed
		and an empty string is returned.[br]
		The second form returns the number of user-operator users on the channel specified by <window_id>.[br]
		The number of user-operator users is returned if it is known form at the call time: this means that 
		if the channel is not synchronized with the server (as just after the join, for example)
		you might get a number that is actually smaller.[br]
*/

static bool chan_kvs_fnc_useropcount(KviKvsModuleFunctionCall * c)
{ 
	QString szId;
	KVSM_PARAMETERS_BEGIN(c)
		KVSM_PARAMETER("window id",KVS_PT_STRING,KVS_PF_OPTIONAL,szId)
	KVSM_PARAMETERS_END(c)
	KviChannel * ch = chan_kvs_find_channel(c,szId);
	if (ch) c->returnValue()->setInteger(ch->userOpCount());
	return true;
}

/*
	@doc: chan.bancount
	@type:
		function
	@title:
		$chan.bancount
	@short:
		Returns the number of entries in the channel ban list
	@syntax:
		<integer> $chan.bancount
		<integer> $chan.bancount(<window_id:string>)
	@description:
		The first form returns the number of entries in the ban list of the current channel (assuming that the current window
		is a channel at all). If the current window is not a channel, a warning is printed
		and 0 is returned.[br]
		The second form returns the number entries in the ban list of the channel specified by <window_id>.[br]
		The number of ban list entries is returned if it is known form at the call time: this means that 
		if the channel is not synchronized with the server (as just after the join, for example)
		you might get a number that is actually smaller.[br]
*/

static bool chan_kvs_fnc_bancount(KviKvsModuleFunctionCall * c)
{ 
	QString szId;
	KVSM_PARAMETERS_BEGIN(c)
		KVSM_PARAMETER("window id",KVS_PT_STRING,KVS_PF_OPTIONAL,szId)
	KVSM_PARAMETERS_END(c)
	KviChannel * ch = chan_kvs_find_channel(c,szId);
	if (ch) c->returnValue()->setInteger(ch->banCount());
	return true;
}

/*
	@doc: chan.banexceptioncount
	@type:
		function
	@title:
		$chan.banexceptioncount
	@short:
		Returns the number of entries in the channel ban exception list
	@syntax:
		<integer> $chan.banexceptioncount
		<integer> $chan.banexceptioncount(<window_id:string>)
	@description:
		The first form returns the number of entries in the ban exception list of the current channel (assuming that the current window
		is a channel at all). If the current window is not a channel, a warning is printed
		and 0 is returned.[br]
		The second form returns the number entries in the ban exception list of the channel specified by <window_id>.[br]
		The number of ban exception list entries is returned if it is known form at the call time: this means that 
		if the channel is not synchronized with the server (as just after the join, for example)
		you might get a number that is actually smaller.[br]
*/

static bool chan_kvs_fnc_banexceptioncount(KviKvsModuleFunctionCall * c)
{ 
	QString szId;
	KVSM_PARAMETERS_BEGIN(c)
		KVSM_PARAMETER("window id",KVS_PT_STRING,KVS_PF_OPTIONAL,szId)
	KVSM_PARAMETERS_END(c)
	KviChannel * ch = chan_kvs_find_channel(c,szId);
	if (ch) c->returnValue()->setInteger(ch->banExceptionCount());
	return true;
}

/*
	@doc: chan.invitecount
	@type:
		function
	@title:
		$chan.invitecount
	@short:
		Returns the number of entries in the channel invite list
	@syntax:
		<integer> $chan.invitecount
		<integer> $chan.invitecount(<window_id:string>)
	@description:
		The first form returns the number of entries in the invite list of the current channel (assuming that the current window
		is a channel at all). If the current window is not a channel, a warning is printed
		and 0 is returned.[br]
		The second form returns the number entries in the invite list of the channel specified by <window_id>.[br]
		The number of invite list entries is returned if it is known form at the call time: this means that 
		if the channel is not synchronized with the server (as just after the join, for example)
		you might get a number that is actually smaller.[br]
*/

static bool chan_kvs_fnc_invitecount(KviKvsModuleFunctionCall * c)
{ 
	QString szId;
	KVSM_PARAMETERS_BEGIN(c)
		KVSM_PARAMETER("window id",KVS_PT_STRING,KVS_PF_OPTIONAL,szId)
	KVSM_PARAMETERS_END(c)
	KviChannel * ch = chan_kvs_find_channel(c,szId);
	if (ch) c->returnValue()->setInteger(ch->inviteCount());
	return true;
}

/*
	@doc: chan.ison
	@type:
		function
	@title:
		$chan.ison
	@short:
		Checks if an user is on a channel
	@syntax:
		<boolean> $chan.ison(<nickname:string>[,<window_id:string>])
	@description:
		Returns 1 if <nickname> is on the channel identified by <window_id>, 0 otherwise.[br]
		If <window_id> is not specified the current window is used (assuming that it is a channel at all).[br]
		If the window is not a channel, a warning is printed and an empty string is returned.[br]
*/

static bool chan_kvs_fnc_ison(KviKvsModuleFunctionCall * c)
{ 
	QString szId,szNick;
	KVSM_PARAMETERS_BEGIN(c)
		KVSM_PARAMETER("nickname",KVS_PT_NONEMPTYSTRING,0,szNick)
		KVSM_PARAMETER("window id",KVS_PT_STRING,KVS_PF_OPTIONAL,szId)
	KVSM_PARAMETERS_END(c)
	KviChannel * ch = chan_kvs_find_channel(c,szId);
	if (ch) c->returnValue()->setBoolean(ch->isOn(szNick));
	return true;
}

/*
	@doc: chan.getflag
	@type:
		function
	@title:
		$chan.getflag
	@short:
		Returns the channel-user mode flag of an user
	@syntax:
		<char> $chan.getflag(<nickname:string>[,<window_id:string>])
	@description:
		Returns the channel user mode flag of an user on the channel specified by <window_id>.[br]
		If <window_id> is not passed, the current window is used.[br]
		If the specified window is not a channel, a warning is printed and an empty string is returned.[br]
		If the specified user is not on the channel identified by <window_id>, and empty string is returned.[br]
		The possible user flags are:[br]
			'!' for channel administrators[br]
			'@' for ops[br]
			'%' for halfops[br]
			'+' for voiced users[br]
			'-' for userops[br]
		If the user has more than one flag then the highest one is returned.[br]
*/

static bool chan_kvs_fnc_getflag(KviKvsModuleFunctionCall * c)
{ 
	QString szId,szNick;
	KVSM_PARAMETERS_BEGIN(c)
		KVSM_PARAMETER("nickname",KVS_PT_NONEMPTYSTRING,0,szNick)
		KVSM_PARAMETER("window id",KVS_PT_STRING,KVS_PF_OPTIONAL,szId)
	KVSM_PARAMETERS_END(c)
	KviChannel * ch = chan_kvs_find_channel(c,szId);
	if(ch)
	{
		QString szFlag = QChar(ch->userListView()->getUserFlag(szNick));
		c->returnValue()->setString(szFlag);
	}
	return true;
}

#define IS_KVS_FUNC(__clbkname,__chanfunc) \
static bool __clbkname(KviKvsModuleFunctionCall * c) \
{ \
	QString szId,szNick;\
	KVSM_PARAMETERS_BEGIN(c)\
		KVSM_PARAMETER("nickname",KVS_PT_NONEMPTYSTRING,0,szNick)\
		KVSM_PARAMETER("window id",KVS_PT_STRING,KVS_PF_OPTIONAL,szId)\
	KVSM_PARAMETERS_END(c)\
	KviChannel * ch = chan_kvs_find_channel(c,szId); \
	if(ch) c->returnValue()->setBoolean(ch->__chanfunc(szNick,true)); \
	return true; \
}

/*
	@doc: chan.isowner
	@type:
		function
	@title:
		$chan.isowner
	@short:
		Checks if an user is at least a channel owner
	@syntax:
		<boolean> $chan.isowner(<nickname:string>[,<window_id:string>])
	@description:
		Returns 1 if <nickname> is at least an owner on the channel identified by <window_id>, 0 otherwise.[br]
		If <window_id> is not specified the current window is used (assuming that it is a channel at all).[br]
		If the window is not a channel, a warning is printed and an empty string is returned.[br]
		Note that if the user is not on the channel at all, you will get 0 as return value.[br]
*/

IS_KVS_FUNC(chan_kvs_fnc_isowner,isChanOwner)

/*
	@doc: chan.isadmin
	@type:
		function
	@title:
		$chan.isadmin
	@short:
		Checks if an user is at least channel administrator
	@syntax:
		<boolean> $chan.isadmin(<nickname:string>[,<window_id:string>])
	@description:
		Returns 1 if <nickname> is at least an administrator on the channel identified by <window_id>, 0 otherwise.[br]
		If <window_id> is not specified the current window is used (assuming that it is a channel at all).[br]
		If the window is not a channel, a warning is printed and an empty string is returned.[br]
		Note that if the user is not on the channel at all, you will get 0 as return value.[br]
*/

IS_KVS_FUNC(chan_kvs_fnc_isadmin,isChanAdmin)

/*
	@doc: chan.isop
	@type:
		function
	@title:
		$chan.isop
	@short:
		Checks if an user is at least an op on a channel
	@syntax:
		<boolean> $chan.isop(<nickname:string>[,<window_id:stringn>])
	@description:
		Returns 1 if <nickname> is at least an operator on the channel identified by <window_id>, 0 otherwise.[br]
		If <window_id> is not specified the current window is used (assuming that it is a channel at all).[br]
		If the window is not a channel, a warning is printed and an empty string is returned.[br]
		Note that if the user is not on the channel at all, you will get 0 as return value.[br]
*/

IS_KVS_FUNC(chan_kvs_fnc_isop,isOp)

/*
	@doc: chan.isvoice
	@type:
		function
	@title:
		$chan.isvoice
	@short:
		Checks if an user is at least voiced on a channel
	@syntax:
		<boolean> $chan.isvoice(<nickname:string>[,<window_id:string>])
	@description:
		Returns 1 if <nickname> is at least voiced on the channel identified by <window_id>, 0 otherwise.[br]
		If <window_id> is not specified the current window is used (assuming that it is a channel at all).[br]
		If the window is not a channel, a warning is printed and an empty string is returned.[br]
		Note that if the user is not on the channel at all, you will get 0 as return value.[br]
*/

IS_KVS_FUNC(chan_kvs_fnc_isvoice,isVoice)

/*
	@doc: chan.ishalfop
	@type:
		function
	@title:
		$chan.ishalfop
	@short:
		Checks if an user is at least halfop on a channel
	@syntax:
		<boolean> $chan.ishalfop(<nickname:string>[,<window_id:string>])
	@description:
		Returns 1 if <nickname> is at least a half-operator on the channel identified by <window_id>, 0 otherwise.[br]
		If <window_id> is not specified the current window is used (assuming that it is a channel at all).[br]
		If the window is not a channel, a warning is printed and an empty string is returned.[br]
		Note that if the user is not on the channel at all, you will get 0 as return value.[br]
*/

IS_KVS_FUNC(chan_kvs_fnc_ishalfop,isHalfOp)

/*
	@doc: chan.isuserop
	@type:
		function
	@title:
		$chan.isuserop
	@short:
		Checks if an user is at least an userop on a channel
	@syntax:
		<boolean> $chan.isuserop(<nickname:string>[,<window_id:string>])
	@description:
		Returns 1 if <nickname> is at least an user-operator on the channel identified by <window_id>, 0 otherwise.[br]
		If <window_id> is not specified the current window is used (assuming that it is a channel at all).[br]
		If the window is not a channel, a warning is printed and an empty string is returned.[br]
		Note that if the user is not on the channel at all, you will get 0 as return value.[br]
*/

IS_KVS_FUNC(chan_kvs_fnc_isuserop,isUserOp)


/*
	@doc: chan.ismeowner
	@type:
		function
	@title:
		$chan.isMeOwner
	@short:
		Checks if the current user is at least an owner on a channel
	@syntax:
		<boolean> $chan.isMeOwner
		<boolean> $chan.isMeOwner(<window_id:string>)
	@description:
		Returns 1 if the current user is at least an owner on the channel specified by <window_id>, 0 otherwise.[br]
		If <window_id> is not passed, the current window is used (assuming it is a channel at all).[br]
		If the window is not a channel, a warning is printed and an empty string is returned.[br]
		This function is a "shortcut" for [fnc]$chan.isowner[/fnc]([fnc]$me[/fnc]).[br]
*/

#define IS_ME_KVS_FUNC(__clbkname,__chanfunc) \
static bool __clbkname(KviKvsModuleFunctionCall * c) \
{ \
	QString szId;\
	KVSM_PARAMETERS_BEGIN(c)\
		KVSM_PARAMETER("window id",KVS_PT_STRING,KVS_PF_OPTIONAL,szId)\
	KVSM_PARAMETERS_END(c)\
	KviChannel * ch = chan_kvs_find_channel(c,szId); \
	if(ch) c->returnValue()->setBoolean(ch->__chanfunc(true)); \
	return true; \
}

IS_ME_KVS_FUNC(chan_kvs_fnc_ismeowner,isMeChanOwner)

/*
	@doc: chan.ismeadmin
	@type:
		function
	@title:
		$chan.isMeAdmin
	@short:
		Checks if the current user is at least an administrator on a channel
	@syntax:
		<boolean> $chan.isMeAdmin
		<boolean> $chan.isMeAdmin(<window_id:string>)
	@description:
		Returns 1 if the current user is at least an administrator on the channel specified by <window_id>, 0 otherwise.[br]
		If <window_id> is not passed, the current window is used (assuming it is a channel at all).[br]
		If the window is not a channel, a warning is printed and an empty string is returned.[br]
		This function is a "shortcut" for [fnc]$chan.isadmin[/fnc]([fnc]$me[/fnc]).[br]
*/

IS_ME_KVS_FUNC(chan_kvs_fnc_ismeadmin,isMeChanAdmin)

/*
	@doc: chan.ismeop
	@type:
		function
	@title:
		$chan.isMeOp
	@short:
		Checks if the current user is at least op on a channel
	@syntax:
		<boolean> $chan.isMeOp
		<boolean> $chan.isMeOp(<window_id:string>)
	@description:
		Returns 1 if the current user is at least op on the channel specified by <window_id>, 0 otherwise.[br]
		If <window_id> is not passed, the current window is used (assuming it is a channel at all).[br]
		If the window is not a channel, a warning is printed and an empty string is returned.[br]
		This function is a "shortcut" for [fnc]$chan.isop[/fnc]([fnc]$me[/fnc]).[br]
*/

IS_ME_KVS_FUNC(chan_kvs_fnc_ismeop,isMeOp)

/*
	@doc: chan.ismehalfop
	@type:
		function
	@title:
		$chan.isMeHalfOp
	@short:
		Checks if the current user is at least an half operator on a channel
	@syntax:
		<boolean> $chan.isMeHalfOp
		<boolean> $chan.isMeHalfOp(<window_id:string>)
	@description:
		Returns 1 if the current user is at least an half operator on the channel specified by <window_id>, 0 otherwise.[br]
		If <window_id> is not passed, the current window is used (assuming it is a channel at all).[br]
		If the window is not a channel, a warning is printed and an empty string is returned.[br]
		This function is a "shortcut" for [fnc]$chan.ishalfop[/fnc]([fnc]$me[/fnc]).[br]
*/

IS_ME_KVS_FUNC(chan_kvs_fnc_ismehalfop,isMeHalfOp)

/*
	@doc: chan.ismevoice
	@type:
		function
	@title:
		$chan.isMeVoice
	@short:
		Checks if the current user is at least voice on a channel
	@syntax:
		<boolean> $chan.isMeVoice
		<boolean> $chan.isMeVoice(<window_id:string>)
	@description:
		Returns 1 if the current user is at least voice on the channel specified by <window_id>, 0 otherwise.[br]
		If <window_id> is not passed, the current window is used (assuming it is a channel at all).[br]
		If the window is not a channel, a warning is printed and an empty string is returned.[br]
		This function is a "shortcut" for [fnc]$chan.isvoice[/fnc]([fnc]$me[/fnc]).[br]
*/

IS_ME_KVS_FUNC(chan_kvs_fnc_ismevoice,isMeVoice)

/*
	@doc: chan.ismeuserop
	@type:
		function
	@title:
		$chan.isMeUserOp
	@short:
		Checks if the current user is at least an user operator on a channel
	@syntax:
		<boolean> $chan.isMeUserOp
		<boolean> $chan.isMeUserOp(<window_id:String>)
	@description:
		Returns 1 if the current user is at least an user operator on the channel specified by <window_id>, 0 otherwise.[br]
		If <window_id> is not passed, the current window is used (assuming it is a channel at all).[br]
		If the window is not a channel, a warning is printed and an empty string is returned.[br]
		This function is a "shortcut" for [fnc]$chan.isuserop[/fnc]([fnc]$me[/fnc]).[br]
*/

IS_ME_KVS_FUNC(chan_kvs_fnc_ismeuserop,isMeUserOp)

/*
	@doc: chan.mode
	@type:
		function
	@title:
		$chan.mode
	@short:
		Returns the mode string of a channel
	@syntax:
		<string> $chan.mode
		<string> $chan.mode(<window_id:string>)
	@description:
		Returns the mode string of the channel identified by <window_id>.[br]
		If no <window_id> is passed, the current channel mode string is returned (assuming that
		the current window is a chnannel at all).[br]
		If the window is not a channel, a warning is printed and an empty string is returned.[br]
*/

static bool chan_kvs_fnc_mode(KviKvsModuleFunctionCall * c)
{ 
	QString szId;
	KVSM_PARAMETERS_BEGIN(c)
		KVSM_PARAMETER("window id",KVS_PT_STRING,KVS_PF_OPTIONAL,szId)
	KVSM_PARAMETERS_END(c)
	KviChannel * ch = chan_kvs_find_channel(c,szId);
	if(ch)
	{
		QString chMode;
		ch->getChannelModeString(chMode);
		c->returnValue()->setString(chMode);
	}
	return true;
}


/*
	@doc: chan.key
	@type:
		function
	@title:
		$chan.key
	@short:
		Returns the key of a channel
	@syntax:
		<string> $chan.key
		<string> $chan.key(<window_id:string>)
	@description:
		Returns the key of the channel identified by <window_id>.[br]
		If no <window_id> is passed, the current channel key is returned (assuming that
		the current window is a chnannel at all).[br]
		If the window is not a channel, a warning is printed and an empty string is returned.[br]
		If the channel has no key set, an empty string is returned.[br]
*/

static bool chan_kvs_fnc_key(KviKvsModuleFunctionCall * c)
{ 
	QString szId;
	KVSM_PARAMETERS_BEGIN(c)
		KVSM_PARAMETER("window id",KVS_PT_STRING,KVS_PF_OPTIONAL,szId)
	KVSM_PARAMETERS_END(c)
	KviChannel * ch = chan_kvs_find_channel(c,szId);
	if (ch) c->returnValue()->setString(ch->channelKey());
	return true;
}

/*
	@doc: chan.limit
	@type:
		function
	@title:
		$chan.limit
	@short:
		Returns the limit of a channel
	@syntax:
		<integer> $chan.limit
		<integer> $chan.limit(<window_id:string>)
	@description:
		Returns the limit of the channel identified by <window_id>.[br]
		If no <window_id> is passed, the current channel limit is returned (assuming that
		the current window is a chnannel at all).[br]
		If the window is not a channel, a warning is printed and an empty string is returned.[br]
		If the channel has no limit set, "0" is returned.[br]
*/

static bool chan_kvs_fnc_limit(KviKvsModuleFunctionCall * c)
{ 
	QString szId;
	KVSM_PARAMETERS_BEGIN(c)
		KVSM_PARAMETER("window id",KVS_PT_STRING,KVS_PF_OPTIONAL,szId)
	KVSM_PARAMETERS_END(c)
	KviChannel * ch = chan_kvs_find_channel(c,szId);
	kvs_int_t limit=0;
	QString lim;
	if (ch) 
	{
		if(ch->hasChannelLimit())
		{
			lim=ch->channelLimit().ptr();
			limit=lim.toInt();
		}
		c->returnValue()->setInteger(limit);
	}
	return true;
}


/*
	@doc: chan.users
	@type:
		function
	@title:
		$chan.users
	@short:
		Returns an array of channel user nicknames
	@syntax:
		<array> $chan.users([window_id:string],[mask:string],[flags:string])
	@description:
		Returns an array of nicknames on the channel specified by [window_id].[br]
		If [window_id] is empty, the current window is used.[br]
		If the window designated by [window_id] is not a channel a warning is printed and an empty array is returned.[br]
		If [mask] is given, each user is added to the array only
		if it matches the [mask].[br]
		[flags] may contain a subset of the letters "aovhnmi":[br]
		"ovhn" are mode flags: the users are added to the array only if they are channel administrators ('a'), operators ('o'),
		voiced users ('v'), half-operators ('h'), user-operators ('u') or unflagged ('n') users. (Unflagged means not operators, not
		voiced and not half-operators). If none of the "ovhun" flags are used, KVIrc behaves like all five were passed.[br]
		The flag 'm' causes the entire user masks to be added to the 
		array entries, as known by KVIrc at the moment of this function call.[br]
		The flag 'i' causes KVIrc to invert the match and add only the users that do NOT match [mask].[br]
		Please note that on really large channels this function may be time consuming (especially if [mask] is used):
		use with care.[br]
	@example:
		[example]
			[comment]# Get the nickname list[/comment]
			%test[] = $chan.users
			[comment]# And loop thru the items[/comment]
			%i = 0
			[comment]# %test[]# returns the number of elements in the array[/comment]
			%count = %test[]#
			while(%i < %count)
			{
				echo "User %i : %test[%i]"
				%i++
			}
			[comment]# Another way of looping[/comment]
			foreach(%tmp,%test[])echo "User %tmp"
			[comment]# Find all the users that come from the *.org domain[/comment]
			%test[]=$chan.users(,*!*@*.org)
			echo %test[]
			[comment]# This will work too![/comment]
			echo $chan.users(,*!*@*.org)
			[comment]# Find all the channel operators[/comment]
			%test[] = $chan.users(,,o)
			echo %test[]
			[comment]# Find all the voiced users that do NOT come from *.edu[/comment]
			[comment]# See also their whole mask[/comment]
			%test[] = $chan.users(,*!*@*.edu,vim)
			echo %test[]
		[/example]
		
*/

static bool chan_kvs_fnc_users(KviKvsModuleFunctionCall * c)
{ 
	QString szWinId,szMask,szFlags;
	KVSM_PARAMETERS_BEGIN(c)
		KVSM_PARAMETER("window id",KVS_PT_STRING,KVS_PF_OPTIONAL,szWinId)
		KVSM_PARAMETER("mask",KVS_PT_STRING,KVS_PF_OPTIONAL,szMask)
		KVSM_PARAMETER("flags",KVS_PT_STRING,KVS_PF_OPTIONAL,szFlags)
	KVSM_PARAMETERS_END(c)

	KviKvsArray * pArray = new KviKvsArray();
	c->returnValue()->setArray(pArray);

	KviChannel * ch = chan_kvs_find_channel(c,szWinId);
	if(!ch)return true;

	KviUserListEntry * e = ch->userListView()->firstItem(); // Thnx Julien :)

	bool bCheckMask = !szMask.isEmpty();
	bool bOp = szFlags.find('o',false) != -1;
	bool bVoice = szFlags.find('v',false) != -1;
	bool bHalfOp = szFlags.find('h',false) != -1;
	bool bChanAdmins = szFlags.find('a',false) != -1;
	bool bUserOp = szFlags.find('u',false) != -1;
	bool bNone = szFlags.find('n',false) != -1;
	bool bCheckFlags = bOp || bVoice || bHalfOp || bNone || bChanAdmins || bUserOp;
	bool bAddMask = szFlags.find('m',false) != -1;

	int idx = 0;

	if(bAddMask || bCheckFlags || bCheckMask)
	{
		bool bMaskMustMatch = szFlags.find('i',false) == -1;
		KviIrcMask mask(szMask);

		while(e)
		{
			if(bCheckFlags)
			{
				if(bChanAdmins)
				{
					if(e->flags() & KVI_USERFLAG_CHANADMIN)goto check_mask;
				}
				if(bOp)
				{
					if(e->flags() & KVI_USERFLAG_OP)goto check_mask;
				}
				if(bVoice)
				{
					if(e->flags() & KVI_USERFLAG_VOICE)goto check_mask;
				}
				if(bHalfOp)
				{
					if(e->flags() & KVI_USERFLAG_HALFOP)goto check_mask;
				}
				if(bUserOp)
				{
					if(e->flags() & KVI_USERFLAG_USEROP)goto check_mask;
				}
				if(bNone)
				{
					if(!(e->flags() & KVI_USERFLAG_MASK))goto check_mask;
				}
				goto next_item;
			}
check_mask:
			if(bCheckMask)
			{
				if(mask.matchesFixed(e->nick(),e->globalData()->user(),e->globalData()->host()) == bMaskMustMatch)goto add_item;
				goto next_item;
			}
add_item:
			if(bAddMask)
			{
				QString x(e->nick());
				x.append('!');
				x.append(e->globalData()->user());
				x.append('@');
				x.append(e->globalData()->host());
				pArray->set(idx,new KviKvsVariant(x));
			} else {
				pArray->set(idx,new KviKvsVariant(e->nick()));
			}
			idx++;
next_item:
			e = e->next();
		}
	} else {
		while(e)
		{
			pArray->set(idx,new KviKvsVariant(e->nick()));
			idx++;
			e = e->next();
		}
	}


	return true;
}

/*
	@doc: chan.banlist
	@type:
		function
	@title:
		$chan.banlist
	@short:
		Returns an array of channel ban masks
	@syntax:
		$chan.banlist([window_id])
	@description:
		Returns an array of ban masks set ont the channel identified by [window_id].[br]
		If [window_id] is empty, the current window is used.[br]
		If the window designated by [window_id] is not a channel a warning is printed and an empty array is returned.[br]
*/

static bool chan_kvs_fnc_banlist(KviKvsModuleFunctionCall * c)
{ 
	QString szWinId,szMask,szFlags;
	KVSM_PARAMETERS_BEGIN(c)
		KVSM_PARAMETER("window id",KVS_PT_STRING,KVS_PF_OPTIONAL,szWinId)
	KVSM_PARAMETERS_END(c)

	KviKvsArray * pArray = new KviKvsArray();
	c->returnValue()->setArray(pArray);

	KviChannel * ch = chan_kvs_find_channel(c,szWinId);

	if(!ch)return true;

	int idx = 0;

	KviPtrList<KviMaskEntry> * l = ((KviChannel *)(c->window()))->banList();

	for(KviMaskEntry * e = l->first();e;e = l->next())
	{
		pArray->set(idx,new KviKvsVariant(e->szMask));
		idx++;
	}

	return true;
}

/*
	@doc: chan.banexceptionlist
	@type:
		function
	@title:
		$chan.banexceptionlist
	@short:
		Returns an array of channel ban exception masks
	@syntax:
		<array> $chan.banexceptionlist([window_id])
	@description:
		Returns an array of ban exception masks set ont the channel identified by [window_id].[br]
		If [window_id] is empty, the current window is used.[br]
		If the window designated by [window_id] is not a channel a warning is printed and an empty array is returned.[br]
*/

static bool chan_kvs_fnc_banexceptionlist(KviKvsModuleFunctionCall * c)
{ 
	QString szWinId,szMask,szFlags;
	KVSM_PARAMETERS_BEGIN(c)
		KVSM_PARAMETER("window id",KVS_PT_STRING,KVS_PF_OPTIONAL,szWinId)
	KVSM_PARAMETERS_END(c)

	KviKvsArray * pArray = new KviKvsArray();
	c->returnValue()->setArray(pArray);

	KviChannel * ch = chan_kvs_find_channel(c,szWinId);

	if(!ch)return true;

	int idx = 0;

	KviPtrList<KviMaskEntry> * l = ((KviChannel *)(c->window()))->banExceptionList();

	for(KviMaskEntry * e = l->first();e;e = l->next())
	{
		pArray->set(idx,new KviKvsVariant(e->szMask));
		idx++;
	}

	return true;
}

/*
	@doc: chan.invitelist
	@type:
		function
	@title:
		$chan.invitelist
	@short:
		Returns an array of channel invite masks
	@syntax:
		<array> $chan.banexceptionlist([window_id])
	@description:
		Returns an array of invite masks set ont the channel identified by [window_id].[br]
		If [window_id] is empty, the current window is used.[br]
		If the window designated by [window_id] is not a channel a warning is printed and an empty array is returned.[br]
*/

static bool chan_kvs_fnc_invitelist(KviKvsModuleFunctionCall * c)
{ 
	QString szWinId,szMask,szFlags;
	KVSM_PARAMETERS_BEGIN(c)
		KVSM_PARAMETER("window id",KVS_PT_STRING,KVS_PF_OPTIONAL,szWinId)
	KVSM_PARAMETERS_END(c)

	KviKvsArray * pArray = new KviKvsArray();
	c->returnValue()->setArray(pArray);

	KviChannel * ch = chan_kvs_find_channel(c,szWinId);

	if(!ch)return true;

	int idx = 0;

	KviPtrList<KviMaskEntry> * l = ((KviChannel *)(c->window()))->inviteList();

	for(KviMaskEntry * e = l->first();e;e = l->next())
	{
		pArray->set(idx,new KviKvsVariant(e->szMask));
		idx++;
	}

	return true;
}

/*
	@doc: chan.matchban
	@type:
		function
	@title:
		$chan.matchban
	@short:
		Matches a mask agains the channel ban list
	@syntax:
		<string> $chan.matchban([window_id],<complete_mask>)
	@description:
		Returns the ban mask that matches <complete_mask> on channel identified by [window_id].[br]
		If no ban mask matches <complete_mask> an empty string is returned.[br]
		If [window_id] is empty, the current window is used.[br]
		If the window designated by [window_id] is not a channel a warning is printed and an empty string is returned.[br]
		This function is useful to determine if a ban set on the channel matches an user.[br]
*/

static bool chan_kvs_fnc_matchban(KviKvsModuleFunctionCall * c)
{ 
	QString szWinId,szMask;
	KVSM_PARAMETERS_BEGIN(c)
		KVSM_PARAMETER("window id",KVS_PT_STRING,0,szWinId)
		KVSM_PARAMETER("mask",KVS_PT_STRING,0,szMask)
	KVSM_PARAMETERS_END(c)

	KviChannel * ch = chan_kvs_find_channel(c,szWinId);

	if(!ch)
	{
		c->returnValue()->setNothing();
		return true;
	}

	KviPtrList<KviMaskEntry> * l = ((KviChannel *)(c->window()))->banList();

	for(KviMaskEntry * e = l->first();e;e = l->next())
	{
		if(KviQString::matchStringCI(e->szMask,szMask))
		{
			c->returnValue()->setString(e->szMask);
			return true;
		}
	}

	c->returnValue()->setNothing();
	return true;
}

/*
	@doc: chan.matchbanexception
	@type:
		function
	@title:
		$chan.matchbanexception
	@short:
		Matches a mask agains the channel ban exception list
	@syntax:
		<string> $chan.matchbanexception([window_id],<complete_mask>)
	@description:
		Returns the ban exception mask that matches <complete_mask> on channel identified by [window_id].[br]
		If no ban exception mask matches <complete_mask> an empty string is returned.[br]
		If [window_id] is empty, the current window is used.[br]
		If the window designated by [window_id] is not a channel a warning is printed and an empty string is returned.[br]
		This function is useful to determine if a ban exception set on the channel matches an user.[br]
*/

static bool chan_kvs_fnc_matchbanexception(KviKvsModuleFunctionCall * c)
{ 
	QString szWinId,szMask;
	KVSM_PARAMETERS_BEGIN(c)
		KVSM_PARAMETER("window id",KVS_PT_STRING,0,szWinId)
		KVSM_PARAMETER("mask",KVS_PT_STRING,0,szMask)
	KVSM_PARAMETERS_END(c)

	KviChannel * ch = chan_kvs_find_channel(c,szWinId);

	if(!ch)
	{
		c->returnValue()->setNothing();
		return true;
	}

	KviPtrList<KviMaskEntry> * l = ((KviChannel *)(c->window()))->banExceptionList();

	for(KviMaskEntry * e = l->first();e;e = l->next())
	{
		if(KviQString::matchStringCI(e->szMask,szMask))
		{
			c->returnValue()->setString(e->szMask);
			return true;
		}
	}

	c->returnValue()->setNothing();
	return true;
}

/*
	@doc: chan.matchinvite
	@type:
		function
	@title:
		$chan.matchinvite
	@short:
		Matches a mask agains the channel invite list
	@syntax:
		<string> $chan.matchinvite([window_id:string],<complete_mask>)
	@description:
		Returns the invite mask that matches <complete_mask> on channel identified by [window_id].[br]
		If no invite mask matches <complete_mask> an empty string is returned.[br]
		If [window_id] is empty, the current window is used.[br]
		If the window designated by [window_id] is not a channel a warning is printed and an empty string is returned.[br]
		This function is useful to determine if a invite set on the channel matches an user.[br]
*/

static bool chan_kvs_fnc_matchinvite(KviKvsModuleFunctionCall * c)
{ 
	QString szWinId,szMask;
	KVSM_PARAMETERS_BEGIN(c)
		KVSM_PARAMETER("window id",KVS_PT_STRING,0,szWinId)
		KVSM_PARAMETER("mask",KVS_PT_STRING,0,szMask)
	KVSM_PARAMETERS_END(c)

	KviChannel * ch = chan_kvs_find_channel(c,szWinId);

	if(!ch)
	{
		c->returnValue()->setNothing();
		return true;
	}

	KviPtrList<KviMaskEntry> * l = ((KviChannel *)(c->window()))->inviteList();

	for(KviMaskEntry * e = l->first();e;e = l->next())
	{
		if(KviQString::matchStringCI(e->szMask,szMask))
		{
			c->returnValue()->setString(e->szMask);
			return true;
		}
	}

	c->returnValue()->setNothing();
	return true;
}

/*
	@doc: chan.usermodelevel
	@type:
		function
	@title:
		$chan.usermodelevel
	@short:
		Returns the channel user-mode level
	@syntax:
		<integer> $chan.userModeLevel(<nickname:string>[,<window_id:string>])
	@description:
		Returns an integer identifying the specified user's channel mode on the channel specified by <window_id>.[br]
		If <window_id> is not passed, the current window is used.[br]
		If the specified window is not a channel, a warning is printed and '0' is returned.[br]
		This number can be useful to implement comparison functions between
		users in order to determine the actions they can issue between each other.[br]
		For example it is granted that an op will have userModeLevel greater than
		a voiced user or that a simple "modeless" user will have
		an userModeLevel lower than a halfop.[br]
		IRC allows multiple modes to be applied to a single user on a channel,
		in that case this function will return the level of the highest mode
		applied to the user.[br]
		Note: Don't rely on this number being any particular exact value except
		for the completely modeless users (in which case this function will return always '0').
*/

static bool chan_kvs_fnc_usermodelevel(KviKvsModuleFunctionCall * c)
{ 
	QString szNick,szWinId;
	KVSM_PARAMETERS_BEGIN(c)
		KVSM_PARAMETER("nickname",KVS_PT_STRING,0,szNick)
		KVSM_PARAMETER("window id",KVS_PT_STRING,KVS_PF_OPTIONAL,szWinId)
	KVSM_PARAMETERS_END(c)
	KviChannel * ch = chan_kvs_find_channel(c,szWinId);

	kvs_int_t mode=0;
	if(ch) mode=ch->userListView()->getUserModeLevel(szNick);
	c->returnValue()->setInteger(mode);
	return true;
}

/*
	@doc: chan.userjointime
	@type:
		function
	@title:
		$chan.userJoinTime
	@short:
		Returns the time that an user has joined the channel
	@syntax:
		<integer> $chan.userJoinTime(<nickname:string>[,<window_id:string>])
	@description:
		Returns the unix time at which the user specified by <nickname> has joined the channel specified by <window_id>.[br]
		If <window_id> is not passed, the current window is used.[br]
		If the specified window is not a channel, a warning is printed and '0' is returned.[br]
		'0' is also returned when the user's join time is unknown: this is true
		for all the users that were on the channel before the local user has joined it (e.g.
		you know the join time only for users that YOU see joining).[br]
	@seealso:
		[fnc]$unixtime[/fnc], [fnc]$chan.userLastActionTime[/fnc]()
*/

static bool chan_kvs_fnc_userjointime(KviKvsModuleFunctionCall * c)
{ 
	QString szNick,szWinId;
	KVSM_PARAMETERS_BEGIN(c)
		KVSM_PARAMETER("nickname",KVS_PT_STRING,0,szNick)
		KVSM_PARAMETER("window id",KVS_PT_STRING,KVS_PF_OPTIONAL,szWinId)
	KVSM_PARAMETERS_END(c)
	KviChannel * ch = chan_kvs_find_channel(c,szWinId);

	kvs_int_t time=0;
	if(ch) time=ch->userListView()->getUserJoinTime(szNick);
	c->returnValue()->setInteger(time);
	return true;
}

/*
	@doc: chan.userlastactiontime
	@type:
		function
	@title:
		$chan.userLastActionTime
	@short:
		Returns the time that an user has last performed some kind of action on the channel
	@syntax:
		<integer> $chan.userLastActionTime(<nickname:string>[,<window_id:string>])
	@description:
		Returns the unix time at which the user specified by <nickname> has performed
		some kind of action on the channel specified by <window_id>.[br]
		If <window_id> is not passed, the current window is used.[br]
		If the specified window is not a channel, a warning is printed and '0' is returned.[br]
		'0' is also returned when the user's last action time is unknown: this is true
		for all the users that were on the channel before the local user has joined it
		and have performed no actions since that moment.
	@seealso:
		[fnc]$unixtime[/fnc](), [fnc]$chan.userJoinTime[/fnc]()
*/

static bool chan_kvs_fnc_userlastactiontime(KviKvsModuleFunctionCall * c)
{ 
	QString szNick,szWinId;
	KVSM_PARAMETERS_BEGIN(c)
		KVSM_PARAMETER("nickname",KVS_PT_STRING,0,szNick)
		KVSM_PARAMETER("window id",KVS_PT_STRING,KVS_PF_OPTIONAL,szWinId)
	KVSM_PARAMETERS_END(c)
	KviChannel * ch = chan_kvs_find_channel(c,szWinId);

	kvs_int_t time=0;
	if(ch) time=ch->userListView()->getUserLastActionTime(szNick);
	c->returnValue()->setInteger(time);
	return true;
}

static bool chan_module_init(KviModule * m)
{

	KVSM_REGISTER_FUNCTION(m,"name",chan_kvs_fnc_name);
	KVSM_REGISTER_FUNCTION(m,"topic",chan_kvs_fnc_topic);
	KVSM_REGISTER_FUNCTION(m,"topicsetby",chan_kvs_fnc_topicsetby);
	KVSM_REGISTER_FUNCTION(m,"topicsetat",chan_kvs_fnc_topicsetat);
	KVSM_REGISTER_FUNCTION(m,"usercount",chan_kvs_fnc_usercount);
	KVSM_REGISTER_FUNCTION(m,"ownercount",chan_kvs_fnc_ownercount);
	KVSM_REGISTER_FUNCTION(m,"admincount",chan_kvs_fnc_admincount);
	KVSM_REGISTER_FUNCTION(m,"opcount",chan_kvs_fnc_opcount);
	KVSM_REGISTER_FUNCTION(m,"voicecount",chan_kvs_fnc_voicecount);
	KVSM_REGISTER_FUNCTION(m,"halfopcount",chan_kvs_fnc_halfopcount);
	KVSM_REGISTER_FUNCTION(m,"useropcount",chan_kvs_fnc_useropcount);
	KVSM_REGISTER_FUNCTION(m,"isowner",chan_kvs_fnc_isowner);
	KVSM_REGISTER_FUNCTION(m,"ison",chan_kvs_fnc_ison);
	KVSM_REGISTER_FUNCTION(m,"isadmin",chan_kvs_fnc_isadmin);
	KVSM_REGISTER_FUNCTION(m,"isop",chan_kvs_fnc_isop);
	KVSM_REGISTER_FUNCTION(m,"isvoice",chan_kvs_fnc_isvoice);
	KVSM_REGISTER_FUNCTION(m,"ishalfop",chan_kvs_fnc_ishalfop);
	KVSM_REGISTER_FUNCTION(m,"isuserop",chan_kvs_fnc_isuserop);
	KVSM_REGISTER_FUNCTION(m,"ismeuserop",chan_kvs_fnc_ismeuserop);
	KVSM_REGISTER_FUNCTION(m,"ismevoice",chan_kvs_fnc_ismevoice);
	KVSM_REGISTER_FUNCTION(m,"ismehalfop",chan_kvs_fnc_ismehalfop);
	KVSM_REGISTER_FUNCTION(m,"ismeop",chan_kvs_fnc_ismeop);
	KVSM_REGISTER_FUNCTION(m,"ismeadmin",chan_kvs_fnc_ismeadmin);
	KVSM_REGISTER_FUNCTION(m,"ismeowner",chan_kvs_fnc_ismeowner);
	KVSM_REGISTER_FUNCTION(m,"isdead",chan_kvs_fnc_isdead);
	KVSM_REGISTER_FUNCTION(m,"getflag",chan_kvs_fnc_getflag);

	KVSM_REGISTER_FUNCTION(m,"usermodelevel",chan_kvs_fnc_usermodelevel);
	KVSM_REGISTER_FUNCTION(m,"userjointime",chan_kvs_fnc_userjointime);//
	KVSM_REGISTER_FUNCTION(m,"userlastactiontime",chan_kvs_fnc_userlastactiontime);
	KVSM_REGISTER_FUNCTION(m,"mode",chan_kvs_fnc_mode);

	KVSM_REGISTER_FUNCTION(m,"key",chan_kvs_fnc_key);
	KVSM_REGISTER_FUNCTION(m,"limit",chan_kvs_fnc_limit);
	KVSM_REGISTER_FUNCTION(m,"users",chan_kvs_fnc_users);
	KVSM_REGISTER_FUNCTION(m,"bancount",chan_kvs_fnc_bancount);
	KVSM_REGISTER_FUNCTION(m,"banexceptioncount",chan_kvs_fnc_banexceptioncount);
	KVSM_REGISTER_FUNCTION(m,"invitecount",chan_kvs_fnc_invitecount);
	KVSM_REGISTER_FUNCTION(m,"banlist",chan_kvs_fnc_banlist);
	KVSM_REGISTER_FUNCTION(m,"banexceptionlist",chan_kvs_fnc_banexceptionlist);
	KVSM_REGISTER_FUNCTION(m,"invitelist",chan_kvs_fnc_invitelist);
	KVSM_REGISTER_FUNCTION(m,"matchban",chan_kvs_fnc_matchban);
	KVSM_REGISTER_FUNCTION(m,"matchbanexception",chan_kvs_fnc_matchbanexception);
	KVSM_REGISTER_FUNCTION(m,"matchinvite",chan_kvs_fnc_matchinvite);
	KVSM_REGISTER_FUNCTION(m,"getUrl",chan_kvs_fnc_getUrl);

	return true;
}

static bool chan_module_cleanup(KviModule *m)
{
	return true;
}


KVIRC_MODULE(
	"Chan",                                                  // module name
	"1.0.0",                                                // module version
	"Copyright (C) 2002 Szymon Stefanek (pragma at kvirc dot net)"\
	"          (C) 2002 Juanjo Alvarez (juanjux at yahoo dot es)",
	"Scripting interface for the channel management",
	chan_module_init,
	0,
	0,
	chan_module_cleanup
)
