//=============================================================================
//
//   File : kvi_notifylist.cpp
//   Creation date : Fri Oct 27 2000 23:41:01 CEST by Szymon Stefanek
//
//   This file is part of the KVirc irc client distribution
//   Copyright (C) 2000-2004 Szymon Stefanek (pragma at kvirc dot net)
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




#include "kvi_debug.h"
#include "kvi_notifylist.h"
#include "kvi_console.h"
#include "kvi_ircsocket.h"
#include "kvi_regusersdb.h"
#include "kvi_userlistview.h"
#include "kvi_channel.h"
#include "kvi_options.h"
#include "kvi_window.h"
#include "kvi_locale.h"
#include "kvi_out.h"
#include "kvi_sparser.h"
#include "kvi_ircmask.h"
#include "kvi_numeric.h"
#include "kvi_ircconnection.h"
#include "kvi_app.h"
#include "kvi_qstring.h"
#include "kvi_lagmeter.h"
#include "kvi_kvs_eventtriggers.h"
#include "kvi_qcstring.h"

#include <qstringlist.h>

// FIXME: #warning "Finish this doc!"

/*
	@doc: notify_list
	@title:
		Notify lists
	@short:
		Tracking users on IRC
	@keyterms:
		notify property, watch property, notify lists
	@body:
		The notify list is a means of keeping track of users on IRC.[br]
		Once connected to an IRC server, you can tell KVIrc to check
		periodically if your friends are online.[br]
		This is basically achieved by setting a property in the [doc:registered_users]registered users database[/doc]
		entry.[br]
		The property is called "notify", and you have to set it to the nickname
		that you want to look for.[br]
		So for example, assume to register a frend of yours like Szymon:[br]
		[example]
			[cmd:reguser.add]reguser.add[/cmd] Szymon
			[cmd:reguser.addmask]reguser.addmask[/cmd] Szymon Pragma!*@*.it
		[/example]
		And then want it in the notify list; nothing easier, just set
		hist "notify" property to the nickname that you want him to be "looked for":[br]
		[example]
			[cmd:reguser.setproperty]reguser.setproperty[/cmd] Szymon notify Pragma
		[/example]
		In this way, once in a while, KVIrc will send to the server an ISON message
		with the nickname Pragma. If Szymon is online, you will be notified with a message:[br]
		"Pragma [someuser@somehost.it] is on IRC".[br]
		If Szymon uses often "[Pragma]" as his secondary nickname , you can do the following:[br]
		[example]
			[cmd:reguser.addmask]reguser.addmask[/cmd] Szymon [Pragma]*@*.it
			[cmd:reguser.setproperty]reguser.setproperty[/cmd] Szymon notify "Pragma [Pragma]"
		[/example]
		KVIrc will then look for both nicknames getting online.[br]
		KVIrc supports three notify lists management methods:[br]
		The "stupid ISON method", the "intelligent ISON method" and the "WATCH method".[br]
		The "stupid ISON method" will assume that Szymon is online if any user with nickname
		Pragma (or [Pragma] in the second example) gets online; this means that also Pragma!someuser@somehost.com will be
		assumed to be "Szymon" and will be shown in the notify list.[br]
		This might be a false assumption (since somehod.com does not even match *.it),
		but it is the best result that the "stupid ISON method" can achieve.[br]
		The "intelligent ISON method" will also check the Pragma's username and hostname
		and match it in the registered masks; so in the example above, you will be notified if
		any user that matches Pragma!*@*.it gets online; (but you will NOT be notified if
		(for example) Pragma!someuser@somehost.com gets online).[br]
		So what's the point in including a stupid method? :) Well...the intelligent
		method "eats" some of your IRC bandwidth; it has to send USERHOST messages
		for every group of 5 users in the notify list. If you have a lot of users
		in the notify list, it might become slow and eventually cause a
		client to server flood.[br]
		So finally, the intelligent method is the default. If you have "flood" problems,
		or if you think that the notify list is quite slow , try the "stupid" method:
		it is not that bad after all.[br]
		The third notify list management method is the "WATCH method".[br]
		It uses a totally different (and better) approach to the notify lists management,
		and can be used only on the networks that support the WATCH notify method (DALnet, WebNet, etc.).[br]
		KVIrc will attempt to guess if the server you're currently using supports the WATCH command
		and eventually use this last method.[br]
		The WATCH method uses the "notify" property to get the nicknames that have to be
		sent to the server in the /WATCH commands. 
*/

// Basic NotifyListManager: this does completely nothing

KviNotifyListManager::KviNotifyListManager(KviIrcConnection * pConnection)
: QObject(0,"notify_list_manager")
{
	m_pConnection = pConnection;
	m_pConsole = pConnection->console();
}

KviNotifyListManager::~KviNotifyListManager()
{
}

void KviNotifyListManager::start()
{
}

void KviNotifyListManager::stop()
{
}

bool KviNotifyListManager::handleUserhost(KviIrcMessage *)
{
	return false;
}

bool KviNotifyListManager::handleIsOn(KviIrcMessage *)
{
	return false;
}

bool KviNotifyListManager::handleWatchReply(KviIrcMessage *)
{
	return false;
}

void KviNotifyListManager::notifyOnLine(const QString &nick,const QString &user,const QString &host,const QString &szReason,bool bJoin)
{
	if(bJoin)
		m_pConsole->notifyListView()->join(nick,user,host);

	KviWindow * out = KVI_OPTION_BOOL(KviOption_boolNotifyListChangesToActiveWindow) ? m_pConsole->activeWindow() : m_pConsole;
	if(KVS_TRIGGER_EVENT_1_HALTED(KviEvent_OnNotifyOnLine,out,nick))return;

	QString szWho;
	QString szMsg;

	if(!(user.isEmpty() || host.isEmpty()))
		KviQString::sprintf(szWho,"\r!n\r%Q\r [%Q@\r!h\r%Q\r]",&nick,&user,&host);
	else
		KviQString::sprintf(szWho,"\r!n\r%Q\r",&nick);
	
	QHash<QString,KviRegisteredUser*> * d = g_pRegisteredUserDataBase->userDict();
	QString szNotify;
	
	foreach(KviRegisteredUser * u,*d)
	{
		if(QStringList::split(",",u->getProperty("notify")).findIndex(nick)!=-1)
		{
			QString szComment=u->getProperty("comment");
			if(!szComment.isEmpty())
				KviQString::sprintf(szMsg,"%Q (%Q), Group \"%Q\" is on IRC as (%Q)",&(u->name()),&szComment,&(u->group()),&szWho);
			else
				KviQString::sprintf(szMsg,"%Q, Group \"%Q\" is on IRC as (%Q)",&(u->name()),&(u->group()),&szWho);
			break;
		}
	}
	QString szFmt = __tr2qs("%Q is on IRC");
	
	if(szMsg.isEmpty())
		KviQString::sprintf(szMsg,szFmt,&szWho);
	
	if((!szReason.isEmpty()) && (_OUTPUT_VERBOSE))
	{
		szMsg += "(";
		szMsg += szReason;
		szMsg += ")";
	}

	out->outputNoFmt(KVI_OUT_NOTIFYONLINE,szMsg);

	if(!(out->hasAttention()))
	{
		if(KVI_OPTION_BOOL(KviOption_boolFlashWindowOnNotifyOnLine))
			out->demandAttention();
		if(KVI_OPTION_BOOL(KviOption_boolPopupNotifierOnNotifyOnLine))
		{
			szWho = "<b>";
			szWho += nick;
			szWho += "</b>";
			KviQString::sprintf(szMsg,szFmt,&szWho);
			g_pApp->notifierMessage(0,KVI_OPTION_MSGTYPE(KVI_OUT_NOTIFYONLINE).pixId(),szMsg,15);
		}
	}
}

void KviNotifyListManager::notifyOffLine(const QString &nick,const QString &user,const QString &host,const QString &szReason)
{
	KviWindow * out = KVI_OPTION_BOOL(KviOption_boolNotifyListChangesToActiveWindow) ? m_pConsole->activeWindow() : m_pConsole;
	if(!KVS_TRIGGER_EVENT_1_HALTED(KviEvent_OnNotifyOffLine,out,nick))
	{
		QString szWho;
			
		if(!(user.isEmpty() || host.isEmpty()))
			KviQString::sprintf(szWho,"\r!n\r%Q\r [%Q@\r!h\r%Q\r]",&nick,&user,&host);
		else
			KviQString::sprintf(szWho,"\r!n\r%Q\r",&nick);
	
		QString szMsg;
		
		QHash<QString,KviRegisteredUser*> * d = g_pRegisteredUserDataBase->userDict();
		QString szNotify;
		
		foreach(KviRegisteredUser * u,*d)
		{
			if(QStringList::split(",",u->getProperty("notify")).findIndex(nick)!=-1)
			{
				QString szComment=u->getProperty("comment");
				if(!szComment.isEmpty())
					KviQString::sprintf(szMsg,"%Q (%Q), Group \"%Q\" has left IRC as (%Q)",&(u->name()),&szComment,&(u->group()),&szWho);
				else
					KviQString::sprintf(szMsg,"%Q, Group \"%Q\" has left IRC as (%Q)",&(u->name()),&(u->group()),&szWho);
				break;
			}
		}
		
		if(szMsg.isEmpty())
			KviQString::sprintf(szMsg,__tr2qs("%Q has left IRC"),&szWho);
	
		if((!szReason.isEmpty()) && (_OUTPUT_VERBOSE))
		{
			szMsg += "(";
			szMsg += szReason;
			szMsg += ")";
		}
	
		out->outputNoFmt(KVI_OUT_NOTIFYOFFLINE,szMsg);
	}

	m_pConsole->notifyListView()->part(nick);
}



//
//  INTELLIGENT NOTIFY LIST MANAGER: NOTIFY PROCESS:
//
//            start()                              stop()
//               |                                   ^
//         buildRegUserDict()                        |
//               |                                   |
//     m_pRegUserDict->isEmpty() ? -- YES ---------->+
//                       |                           |
//                      NO                           |
//                       |                           |
//         newNotifySession()<------- TIMER ---------------- delayedNotifySession() --------------------------------+
//                       |    (can be stopped here)  |              ^                                               |
//                       |                           |              ^                                               |
//                  buildNotifyList()                |              |                                              YES
//                       |                           |              |                                               |
//                m_pNotifyList->isEmpty() ? - YES ->+              |                                               |
//                       |                                          |                                               |
//                      NO                                          |                                               |
//                       |                                          |                                               |
//                  newIsOnSession()<------------- TIMER -------------------- delayedIsOnSession() -- NO - m_pNotifyList->isEmpty() ?
//                               |           (can be stopped here)  |                                               |
//                               |                                  |                                               |
//                            buildIsOnList()                       |                                               |
//                               |                                  |                                               |
//                       m_pIsOnList->isEmpty() ? -- YES ---------->+                                               |
//                               |                                                                                  |
//                              NO                                                                                  |
//                               |                                                                                  |
//                            sendIsOn() - - - - - - - - - - - -> handleIsOn()                                      |
//                                                                      |                                           |
//                                                            (build m_pOnlineList)                                 |
//                                                                      |                                           |
//                                                         m_pOnlineList->isEmpty() ? - YES ----------------------->+
//                                                                      |                                           |
//                                                                     NO                                          YES
//                                                                      |                                           |
//                                                            delayedUserhostSession()<--------------- NO - m_pOnlineList->isEmpty() ?
//                                                                               |                                  ^
//                                                                             TIMER (can be stopped here)          |
//                                                                               |                                  |
//                                                                           newUserhostSession()                   |
//                                                                               |                                  |
//                                                                           buildUserhostList()                    |
//                                                                               |                                  |
//                                                                           m_pUserhostList->isEmpty() ? - YES --->+
//                                                                               |                          ^^^     |  
//                                                                               |             (unexpected!)|||     |
//                                                                               NO                                 |
//                                                                               |                                  |
//                                                                           sendUserhost() - - - - - - - - > handleUserhost()
//


KviIsOnNotifyListManager::KviIsOnNotifyListManager(KviIrcConnection * pConnection)
: KviNotifyListManager(pConnection)
{
	m_pRegUserDict = new QHash<QString,QString>; // case insensitive , copy keys
	m_pDelayedNotifyTimer = new QTimer();
	connect(m_pDelayedNotifyTimer,SIGNAL(timeout()),this,SLOT(newNotifySession()));
	m_pDelayedIsOnTimer = new QTimer();
	connect(m_pDelayedIsOnTimer,SIGNAL(timeout()),this,SLOT(newIsOnSession()));
	m_pDelayedUserhostTimer = new QTimer();
	connect(m_pDelayedUserhostTimer,SIGNAL(timeout()),this,SLOT(newUserhostSession()));
	m_bRunning = false;
}


KviIsOnNotifyListManager::~KviIsOnNotifyListManager()
{
	if(m_bRunning)stop();
	delete m_pDelayedNotifyTimer;
	delete m_pDelayedIsOnTimer;
	delete m_pDelayedUserhostTimer;
	delete m_pRegUserDict;
}

void KviIsOnNotifyListManager::start()
{
	if(m_bRunning)stop();
	m_bRunning = true;
	m_pConsole->notifyListView()->partAllButOne(m_pConnection->currentNickName());

	m_bExpectingIsOn = false;
	m_bExpectingUserhost = false;

	buildRegUserDict();
	if(m_pRegUserDict->isEmpty())
	{
		if(_OUTPUT_VERBOSE)
			m_pConsole->output(KVI_OUT_SYSTEMMESSAGE,__tr2qs("Notify list: No users to check for, quitting"));
		stop();
		return;
	}
	newNotifySession();
}

void KviIsOnNotifyListManager::buildRegUserDict()
{
	m_pRegUserDict->clear();

	foreach(KviRegisteredUser * u,*(g_pRegisteredUserDataBase->userDict()))
	{
		QString notify;
		if(u->getProperty("notify",notify))
		{
			notify.stripWhiteSpace();
			while(!notify.isEmpty())
			{
				int idx = notify.find(' ');
				if(idx > 0)
				{
					QString single = notify.left(idx);
					m_pRegUserDict->insert(single,u->name());
					notify.remove(0,idx+1);
				} else {
					m_pRegUserDict->insert(notify,u->name());
					notify = "";
				}
			}
		}
	}	
}

void KviIsOnNotifyListManager::delayedNotifySession()
{
	unsigned int iTimeout = KVI_OPTION_UINT(KviOption_uintNotifyListCheckTimeInSecs);
	if(iTimeout < 15)
	{
		// life first of all.
		// don't allow the user to suicide
		if(_OUTPUT_VERBOSE)
			m_pConsole->output(KVI_OUT_SYSTEMWARNING,
				__tr2qs("Notify list: Timeout (%d sec) is too short, resetting to something more reasonable (15 sec)"),
			iTimeout);
		iTimeout = 15;
		KVI_OPTION_UINT(KviOption_uintNotifyListCheckTimeInSecs) = 15;
	}
	m_pDelayedNotifyTimer->start(iTimeout * 1000,true);
}

void KviIsOnNotifyListManager::newNotifySession()
{
	buildNotifyList();
	if(m_lNotifyList.isEmpty())
	{
		if(_OUTPUT_VERBOSE)
			m_pConsole->output(KVI_OUT_SYSTEMMESSAGE,__tr2qs("Notify list: Notify list empty, quitting"));
		stop();
		return;
	}
	newIsOnSession();
}

void KviIsOnNotifyListManager::buildNotifyList()
{
	m_lNotifyList.clear();
	QHash<QString,QString>::iterator it(m_pRegUserDict->begin());
	while(it != m_pRegUserDict->end())
	{
		m_lNotifyList.append(it.key());
		++it;
	}
}

void KviIsOnNotifyListManager::delayedIsOnSession()
{
	unsigned int iTimeout = KVI_OPTION_UINT(KviOption_uintNotifyListIsOnDelayTimeInSecs);
	if(iTimeout < 5)
	{
		// life first of all.
		// don't allow the user to suicide
		if(_OUTPUT_VERBOSE)
			m_pConsole->output(KVI_OUT_SYSTEMWARNING,
				__tr2qs("Notify list: ISON delay (%d sec) is too short, resetting to something more reasonable (5 sec)"),
			iTimeout);
		iTimeout = 5;
		KVI_OPTION_UINT(KviOption_uintNotifyListIsOnDelayTimeInSecs) = 5;
	}
	m_pDelayedIsOnTimer->start(iTimeout * 1000,true);
}

void KviIsOnNotifyListManager::newIsOnSession()
{
	buildIsOnList();
	if(m_lIsOnList.isEmpty())delayedNotifySession();
	else sendIsOn();
}

void KviIsOnNotifyListManager::buildIsOnList()
{
	m_lIsOnList.clear();
	m_szIsOnString = "";
	foreach(QString s,m_lNotifyList)
	{
		if(((m_szIsOnString.length() + s.length()) + 1) < 504)
		{
			if(!m_szIsOnString.isEmpty())m_szIsOnString.append(' ');
			m_szIsOnString.append(s);
			m_lIsOnList.append(s);
			m_lNotifyList.removeFirst();
		} else break;
	}
}

void KviIsOnNotifyListManager::sendIsOn()
{
	if(_OUTPUT_PARANOIC)
		m_pConsole->output(KVI_OUT_SYSTEMMESSAGE,__tr2qs("Notify list: Checking for: %Q"),&m_szIsOnString);
	KviQCString szDec = m_pConnection->encodeText(m_szIsOnString);
	m_pConnection->sendFmtData("ISON %s",szDec.data());
	if(m_pConnection->lagMeter())
		m_pConnection->lagMeter()->lagCheckRegister("@notify_ison",40); // not that reliable
	m_szIsOnString = "";
	m_bExpectingIsOn = true;
	// FIXME: #warning "And if can't send ?"
}


bool KviIsOnNotifyListManager::handleIsOn(KviIrcMessage *msg)
{
	if(!m_bExpectingIsOn)return false;

	// Check if it is our ISON
	// all the nicks must be on the IsOnList

	QStringList tmplist;

	KviStr nk;
	const char * aux = msg->trailing();

	while(*aux)
	{
		nk = "";
		aux = kvi_extractToken(nk,aux,' ');
		if(nk.hasData())
		{
			bool bGotIt = false;
			QString dnk = m_pConnection->decodeText(nk.ptr());
			foreach(QString s,m_lIsOnList)
			{
				if(KviQString::equalCI(s,dnk))
				{
					tmplist.append(s);
					bGotIt = true;
				}
			}
			if(!bGotIt)
			{
				// ops...not my userhost!
				if(_OUTPUT_VERBOSE)
					m_pConsole->output(KVI_OUT_SYSTEMMESSAGE,__tr2qs("Notify list: Hey! You've used ISON behind my back? (I might be confused now...)"));
				return false;
			}
		}
	}

	// Ok...looks to be my ison (still not sure at 100% , but can't do better)
	if(m_pConnection->lagMeter())
		m_pConnection->lagMeter()->lagCheckComplete("@notify_ison");

	m_bExpectingIsOn = false;

	m_lOnlineList.clear();

	// Ok...we have an IsOn reply here
	// The nicks in the IsOnList that are also in the reply are online , and go to the OnlineList
	// the remaining in the IsOnList are offline

	QString s;
	
	foreach(s,tmplist)
	{
		m_lIsOnList.removeAll(s);
		m_lOnlineList.append(s);
	}

	// Ok...all the users that are online , are on the OnlineList
	// the remaining users are in the m_pIsOnList , and are no longer online

	// first the easy step: remove the users that have just left irc or have never been online
	// we're clearling the m_pIsOnList
	foreach(s,m_lIsOnList)
	{
		if(m_pConsole->notifyListView()->findEntry(s))
		{
			// has just left IRC... make him part
			notifyOffLine(s);
		} // else has never been here

		m_lIsOnList.removeFirst(); // autodelete is true
	}

	// ok... complex step now: the remaining users in the userhost list are online
	// if they have been online before, just remove them from the list
	// otherwise they must be matched for masks
	// and eventually inserted in the notify view later

	KviIrcUserDataBase * db = console()->connection()->userDataBase();

	QStringList l;

	foreach(s,m_lOnlineList)
	{
		if(KviUserListEntry * ent = m_pConsole->notifyListView()->findEntry(s))
		{
			// the user was online from a previous notify session
			// might the mask have been changed ? (heh...this is tricky, maybe too much even)
			if(KVI_OPTION_BOOL(KviOption_boolNotifyListSendUserhostForOnlineUsers))
			{
				// user wants to be sure about online users....
				// check if he is on some channels
				if(ent->globalData()->nRefs() > 1)
				{
					// mmmh...we have more than one ref , so the user is at least in one query or channel
					// look him up on channels , if we find his entry , we can be sure that he is
					// still the right user
					foreach(KviChannel * ch,*(m_pConsole->connection()->channelList()))
					{
						if(KviUserListEntry * le = ch->findEntry(s))
						{
							l.append(s); // ok...found on a channel...we don't need an userhost to match him
							KviIrcMask mk(s,le->globalData()->user(),le->globalData()->host());
							if(!doMatchUser(s,mk))return true; // critical problems = have to restart!!!
							break;
						}
					}
				} // else Only one ref...we need an userhost to be sure (don't remove from the list)
			} else {
				// user wants no userhost for online users...we "hope" that everything will go ok.
				l.append(s);
			}
			//l.append(s); // we will remove him from the list
		} else {
			// the user was not online!
			// check if we have a cached mask
			if(db)
			{
				if(KviIrcUserEntry * ue = db->find(s))
				{
					// already in the db... do we have a mask ?
					if(ue->hasUser() && ue->hasHost())
					{
						// yup! we have a complete mask to match on
						KviIrcMask mk(s,ue->user(),ue->host());
						// lookup the user's name in the m_pRegUserDict
						if(!doMatchUser(s,mk))return true; // critical problems = have to restart!!!
						l.append(s); // remove anyway
					}
				}
			}
		}
	}

	foreach(s,l)
	{
		m_lOnlineList.removeAll(s); // autodelete is true
	}

	if(m_lOnlineList.isEmpty())
	{
		if(m_lNotifyList.isEmpty())delayedNotifySession();
		else delayedIsOnSession();
	} else delayedUserhostSession();

	return true;
}

// FIXME: #warning "Nickname escapes (links) in the notifylist messages!"

bool KviIsOnNotifyListManager::doMatchUser(const QString &notifyString,const KviIrcMask & mask)
{
	
	if(m_pRegUserDict->contains(notifyString))
	{
		QString nam = m_pRegUserDict->value(notifyString);
		// ok...find the user
		if(KviRegisteredUser * u = g_pRegisteredUserDataBase->findUserByName(nam))
		{
			// ok ... match the user
			if(u->matchesFixed(mask))
			{
				// new user online
				if(!(m_pConsole->notifyListView()->findEntry(mask.nick())))
				{
					notifyOnLine(mask.nick(),mask.user(),mask.host());
				} // else already online , and matching...all ok
			} else {
				// not matched.... has he been online before ?
				if(m_pConsole->notifyListView()->findEntry(mask.nick()))
				{
					// has been online just a sec ago , but now the mask does not match
					// either reguserdb has changed , or the user went offline and another one got his nick
					// in the meantime... (ugly situation anyway)
					notifyOffLine(mask.nick(),mask.user(),mask.host(),__tr2qs("registration mask changed, or nickname is being used by someone else"));
				} else {
					// has never been online
					if(_OUTPUT_VERBOSE)
						m_pConsole->output(KVI_OUT_SYSTEMMESSAGE,__tr2qs("Notify list: \r!n\r%Q\r appears to be online, but the mask [%Q@\r!h\r%Q\r] does not match (registration mask does not match, or nickname is being used by someone else)"),&(mask.nick()),&(mask.user()),&(mask.host()));
				}
			}
		} else {
			// ops... unexpected inconsistency .... reguser db modified ?
			m_pConsole->output(KVI_OUT_SYSTEMWARNING,__tr2qs("Notify list: Unexpected inconsistency, registered user DB modified? (restarting)"));
			stop();
			start();
			return false; // critical ... exit from the call stack
		}
	} else {
		// ops...unexpected inconsistency
		m_pConsole->output(KVI_OUT_SYSTEMWARNING,__tr2qs("Notify list: Unexpected inconsistency, expected \r!n\r%Q\r in the registered user DB"),&notifyString);
	}
	return true;
}

void KviIsOnNotifyListManager::delayedUserhostSession()
{
	unsigned int iTimeout = KVI_OPTION_UINT(KviOption_uintNotifyListUserhostDelayTimeInSecs);
	if(iTimeout < 5)
	{
		// life first of all.
		// don't allow the user to suicide
		if(_OUTPUT_VERBOSE)
			m_pConsole->output(KVI_OUT_SYSTEMWARNING,
				__tr2qs("Notify list: USERHOST delay (%d sec) is too short, resetting to something more reasonable (5 sec)"),
			iTimeout);
		iTimeout = 5;
		KVI_OPTION_UINT(KviOption_uintNotifyListUserhostDelayTimeInSecs) = 5;
	}
	m_pDelayedUserhostTimer->start(iTimeout * 1000,true);
}

void KviIsOnNotifyListManager::newUserhostSession()
{
	buildUserhostList();
	if(m_lUserhostList.isEmpty())
	{
		// this is unexpected!
		m_pConsole->output(KVI_OUT_SYSTEMWARNING,__tr2qs("Notify list: Unexpected inconsistency, userhost list is empty!"));
		if(m_lOnlineList.isEmpty())
		{
			if(m_lNotifyList.isEmpty())delayedNotifySession();
			else delayedIsOnSession();
		} else delayedUserhostSession();
		return;
	}
	sendUserhost();
}

#define MAX_USERHOST_ENTRIES 5

void KviIsOnNotifyListManager::buildUserhostList()
{
	m_szUserhostString = "";
	m_lUserhostList.clear();

	int i = 0;
	QString s;
	while(i < MAX_USERHOST_ENTRIES)
	{
		if(m_lOnlineList.isEmpty()) break;
		s = m_lOnlineList.first();
		if(!m_szUserhostString.isEmpty())m_szUserhostString.append(' ');
		m_szUserhostString.append(s);
		m_lUserhostList.append(s);
		m_lOnlineList.removeFirst();
		i++;
	}
}

void KviIsOnNotifyListManager::sendUserhost()
{
	if(_OUTPUT_PARANOIC)
		m_pConsole->output(KVI_OUT_SYSTEMMESSAGE,__tr2qs("Notify list: Checking userhost for: %Q"),&m_szUserhostString);
	KviQCString ccc = m_pConnection->encodeText(m_szUserhostString);
	m_pConnection->sendFmtData("USERHOST %s",ccc.data());
	if(m_pConnection->lagMeter())
		m_pConnection->lagMeter()->lagCheckRegister("@notify_userhost",50);
	m_szUserhostString = "";
	m_bExpectingUserhost = true;
// FIXME: #warning "And if can't send ?"
}

bool KviIsOnNotifyListManager::handleUserhost(KviIrcMessage *msg)
{
	if(!m_bExpectingUserhost)return false;
	// first check for consistency: all the replies must be on the USERHOST list
	QList<KviIrcMask*> tmplist;

	KviStr nk;
	const char * aux = msg->trailing();

	while(*aux)
	{
		nk = "";
		aux = kvi_extractToken(nk,aux,' ');
		if(nk.hasData())
		{
			// split it in a mask
			KviStr nick;
			KviStr user;
			KviStr host;

			int idx = nk.findFirstIdx('=');
			if(idx != -1)
			{
				nick = nk.left(idx);
				if(nick.lastCharIs('*'))nick.cutRight(1);
				nk.cutLeft(idx + 1);
				if(nk.firstCharIs('+') || nk.firstCharIs('-'))nk.cutLeft(1);

				idx = nk.findFirstIdx('@');
				if(idx != -1)
				{
					user = nk.left(idx);
					nk.cutLeft(idx + 1);
					host = nk;
				} else {
					user = "*";
					host = nk;
				}
	
				bool bGotIt = false;
				QString szNick = m_pConnection->decodeText(nick.ptr());
				QString szUser = m_pConnection->decodeText(user.ptr());
				QString szHost = m_pConnection->decodeText(host.ptr());
				
				foreach(QString s,m_lUserhostList)
				{
					if(KviQString::equalCI(s,szNick))
					{
						KviIrcMask * mk = new KviIrcMask(szNick,szUser,szHost);	
						tmplist.append(mk);
						bGotIt = true;
						m_lUserhostList.removeAll(s);
					}
				}
	
				if(!bGotIt)
				{
					// ops...not my userhost!
					if(_OUTPUT_VERBOSE)
						m_pConsole->output(KVI_OUT_SYSTEMWARNING,__tr2qs("Notify list: Hey! You've used USERHOST behind my back? (I might be confused now...)"));
					return false;
				}
			} else {
				if(_OUTPUT_VERBOSE)
					m_pConsole->output(KVI_OUT_SYSTEMWARNING,__tr2qs("Notify list: Broken USERHOST reply from the server? (%s)"),nk.ptr());
			}
		}
	}

	// Ok...looks to be my usershot (still not sure at 100% , but can't do better)

	if(m_pConnection->lagMeter())
		m_pConnection->lagMeter()->lagCheckComplete("@notify_userhost");


	m_bExpectingUserhost = false;

	foreach(KviIrcMask * mk,tmplist)
	{
		if(!doMatchUser(mk->nick(),*mk))
			return true; // have to restart!!!
	}

	if(!m_lUserhostList.isEmpty())
	{
		// ops...someone is no longer online ?
		foreach(QString s,m_lUserhostList)
		{
			if(_OUTPUT_VERBOSE)
				m_pConsole->output(KVI_OUT_SYSTEMMESSAGE,__tr2qs("Notify list: \r!n\r%Q\r appears to have gone offline before USERHOST reply was received, will recheck in the next loop"),&s);
			m_lUserhostList.removeFirst();
		}
	
	}

	if(m_lOnlineList.isEmpty())
	{
		if(m_lNotifyList.isEmpty())delayedNotifySession();
		else delayedIsOnSession();
	} else delayedUserhostSession();

	return true;
}

void KviIsOnNotifyListManager::stop()
{
	if(!m_bRunning)return;

	if(m_pConnection->lagMeter())
		m_pConnection->lagMeter()->lagCheckAbort("@notify_userhost");
	if(m_pConnection->lagMeter())
		m_pConnection->lagMeter()->lagCheckAbort("@notify_ison");

	m_pDelayedNotifyTimer->stop();
	m_pDelayedIsOnTimer->stop();
	m_pDelayedUserhostTimer->stop();
	m_pConsole->notifyListView()->partAllButOne(m_pConnection->currentNickName());
	m_pRegUserDict->clear();
	m_lNotifyList.clear();
	m_lIsOnList.clear();
	m_lOnlineList.clear();
	m_lUserhostList.clear();
	m_szIsOnString = "";
	m_szUserhostString = "";
	m_bRunning = false;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Stupid notify list manager
//
///////////////////////////////////////////////////////////////////////////////////////////////////

KviStupidNotifyListManager::KviStupidNotifyListManager(KviIrcConnection * pConnection)
: KviNotifyListManager(pConnection)
{
	m_iRestartTimer = 0;
}

KviStupidNotifyListManager::~KviStupidNotifyListManager()
{
	if(m_iRestartTimer)
	{
		killTimer(m_iRestartTimer);
		m_iRestartTimer = 0;
	}
}

void KviStupidNotifyListManager::start()
{
	if(m_iRestartTimer)
	{
		killTimer(m_iRestartTimer);
		m_iRestartTimer = 0;
	}
	if(_OUTPUT_VERBOSE)
		m_pConsole->outputNoFmt(KVI_OUT_SYSTEMMESSAGE,__tr2qs("Starting notify list"));
	buildNickList();
	if(m_lNickList.isEmpty())
	{
		if(_OUTPUT_VERBOSE)
			m_pConsole->outputNoFmt(KVI_OUT_SYSTEMMESSAGE,__tr2qs("No users in the notify list"));
		return; // Ok...no nicknames in the list
	}
	m_iNextNickToCheck = 0;
	m_pConsole->notifyListView()->partAllButOne(m_pConnection->currentNickName());
	sendIsOn();
}

void KviStupidNotifyListManager::sendIsOn()
{
	m_szLastIsOnMsg = "";
	QString nick = m_lNickList.at(m_iNextNickToCheck);
	__range_valid(nick);

	m_iNextNickToCheck++;
	while( (nick.length() + 5 + m_szLastIsOnMsg.length()) < 510)
	{
		KviQString::appendFormatted(m_szLastIsOnMsg," %Q",&nick);
		nick = m_lNickList.at(m_iNextNickToCheck);
		if(m_lNickList.isEmpty()) break;
		m_iNextNickToCheck++;
	}
	if(_OUTPUT_PARANOIC)
		m_pConsole->output(KVI_OUT_SYSTEMMESSAGE,__tr2qs("Notify list: Checking for:%Q"),&m_szLastIsOnMsg);
	KviQCString dat = m_pConnection->encodeText(m_szLastIsOnMsg);
	m_pConnection->sendFmtData("ISON%s",dat.data());

	if(m_pConnection->lagMeter())
		m_pConnection->lagMeter()->lagCheckRegister("@notify_naive",20);

}

bool KviStupidNotifyListManager::handleIsOn(KviIrcMessage * msg)
{
	if(m_pConnection->lagMeter())
		m_pConnection->lagMeter()->lagCheckComplete("@notify_naive");

	KviStr nk;
	const char * aux = msg->trailing();
	while(*aux)
	{
		nk = "";
		aux = kvi_extractToken(nk,aux,' ');
		if(nk.hasData())
		{
			QString nkd = m_pConnection->decodeText(nk.ptr());
			QString nksp = " " + nkd;
			m_szLastIsOnMsg.replace(nksp,"",false);
			if(!(m_pConsole->notifyListView()->findEntry(nkd)))
			{
				// not yet notified
				notifyOnLine(nkd);
			}
		}
	}
	// ok...check the users that have left irc now...
	QStringList sl = QStringList::split(' ',m_szLastIsOnMsg);

	for(QStringList::Iterator it = sl.begin();it != sl.end();++it)
	{
		if(m_pConsole->notifyListView()->findEntry(*it))
		{
			// has just left irc
			notifyOffLine(*it);
		} // else has never been here...
	}

	if(((unsigned int)m_iNextNickToCheck) >= m_lNickList.count())
	{
		// have to restart
		unsigned int iTimeout = KVI_OPTION_UINT(KviOption_uintNotifyListCheckTimeInSecs);
		if(iTimeout < 5)
		{
			// life first of all.
			// don't allow the user to suicide
			if(_OUTPUT_VERBOSE)
				m_pConsole->output(KVI_OUT_SYSTEMWARNING,
					__tr2qs("Notify list: Timeout (%d sec) is too short, resetting to something more reasonable (5 sec)"),
				iTimeout);
			iTimeout = 5;
			KVI_OPTION_UINT(KviOption_uintNotifyListCheckTimeInSecs) = 5;
		}
		m_iRestartTimer = startTimer(iTimeout * 1000);
	} else sendIsOn();
	return true;
}

void KviStupidNotifyListManager::timerEvent(QTimerEvent *e)
{
	if(e->timerId() == m_iRestartTimer)
	{
		killTimer(m_iRestartTimer);
		m_iRestartTimer = 0;
		m_iNextNickToCheck = 0;
		sendIsOn();
		return;
	}
	QObject::timerEvent(e);
}

void KviStupidNotifyListManager::stop()
{
	if(m_pConnection->lagMeter())
		m_pConnection->lagMeter()->lagCheckAbort("@notify_naive");

	if(m_iRestartTimer)
	{
		killTimer(m_iRestartTimer);
		m_iRestartTimer = 0;
	}
	m_pConsole->notifyListView()->partAllButOne(m_pConnection->currentNickName());

	// The ISON Method needs no stopping
}

void KviStupidNotifyListManager::buildNickList()
{
	m_lNickList.clear();
	foreach(KviRegisteredUser* u, *(g_pRegisteredUserDataBase->userDict()))
	{
		QString notify;
		if(u->getProperty("notify",notify))
		{
			m_lNickList.append(notify);
		};
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Watch notify list manager
//
///////////////////////////////////////////////////////////////////////////////////////////////////

KviWatchNotifyListManager::KviWatchNotifyListManager(KviIrcConnection * pConnection)
: KviNotifyListManager(pConnection)
{
	m_pRegUserDict = new QHash<QString,QString>;
}

KviWatchNotifyListManager::~KviWatchNotifyListManager()
{
	delete m_pRegUserDict;
}

void KviWatchNotifyListManager::buildRegUserDict()
{
	m_pRegUserDict->clear();


	foreach(KviRegisteredUser * u ,*(g_pRegisteredUserDataBase->userDict()))
	{
		QString notify;
		if(u->getProperty("notify",notify))
		{
			notify.stripWhiteSpace();
			QStringList sl = QStringList::split(' ',notify);
			for(QStringList::Iterator it = sl.begin();it != sl.end();++it)
			{
				m_pRegUserDict->insert(*it,u->name());
			}
		}
	}	
}

void KviWatchNotifyListManager::start()
{
	m_pConsole->notifyListView()->partAllButOne(m_pConnection->currentNickName());

	buildRegUserDict();

	QString watchStr;

	QHash<QString,QString>::iterator it(m_pRegUserDict->begin());
	while(it != m_pRegUserDict->end())
	{
		QString nk = it.key();
		if(nk.find('*') == -1)
		{
			if((watchStr.length() + nk.length() + 2) > 501)
			{
				KviQCString dat = m_pConnection->encodeText(watchStr);
				m_pConnection->sendFmtData("WATCH%s",dat.data());
				if(_OUTPUT_VERBOSE)
					m_pConsole->output(KVI_OUT_SYSTEMMESSAGE,__tr2qs("Notify list: Adding watch entries for%Q"),&watchStr);
				watchStr = "";
			}
			KviQString::appendFormatted(watchStr," +%Q",&nk);
		}
		++it;
	}

	if(!watchStr.isEmpty())
	{
		KviQCString dat = m_pConnection->encodeText(watchStr);
		m_pConnection->sendFmtData("WATCH%s",dat.data());
		if(_OUTPUT_VERBOSE)
			m_pConsole->output(KVI_OUT_SYSTEMMESSAGE,__tr2qs("Notify list: Adding watch entries for%Q"),&watchStr);
	}
}
void KviWatchNotifyListManager::stop()
{
	m_pConsole->notifyListView()->partAllButOne(m_pConnection->currentNickName());
	m_pConnection->sendFmtData("WATCH clear");
	m_pRegUserDict->clear();
}

bool KviWatchNotifyListManager::doMatchUser(KviIrcMessage * msg,const QString &notifyString,const KviIrcMask & mask)
{
	if(m_pRegUserDict->contains(notifyString))
	{
		QString nam = m_pRegUserDict->value(notifyString);
		// ok...find the user
		if(KviRegisteredUser * u = g_pRegisteredUserDataBase->findUserByName(nam))
		{
			// ok ... match the user
			if(u->matchesFixed(mask))
			{
				// new user online
				if(!(m_pConsole->notifyListView()->findEntry(mask.nick())))
				{
					notifyOnLine(mask.nick(),mask.user(),mask.host(),"watch");
				} else {
					// else already online , and matching...all ok
					if(msg->numeric() == RPL_NOWON)
					{
						// This is a reply to a /watch +something (should not happen, unless the user is messing) or to /watch l (user requested)
						notifyOnLine(mask.nick(),mask.user(),mask.host(),
								__tr2qs("watch entry listing requested by user"),false);
					} else {
						// This is a RPL_LOGON....we're desynched ?
						notifyOnLine(mask.nick(),mask.user(),mask.host(),
								__tr2qs("possible watch list desync"),false);
					}
				}
			} else {
				// not matched.... has he been online before ?
				if(m_pConsole->notifyListView()->findEntry(mask.nick()))
				{
					// has been online just a sec ago , but now the mask does not match
					// prolly the reguserdb has been changed
					notifyOffLine(mask.nick(),mask.user(),mask.host(),
						__tr2qs("registration mask changed or desync with the watch service"));
				} else {
					// has never been online
					if(_OUTPUT_VERBOSE)
						m_pConsole->output(KVI_OUT_SYSTEMMESSAGE,
							__tr("Notify list: \r!n\r%Q\r appears to be online, but the mask [%Q@\r!h\r%Q\r] does not match (watch: registration mask does not match, or nickname is being used by someone else)"),
							&(mask.nick()),&(mask.user()),&(mask.host()));
				}
			}
		} else {
			// ops... unexpected inconsistency .... reguser db modified ?
			m_pConsole->output(KVI_OUT_SYSTEMWARNING,
				__tr2qs("Notify list: Unexpected inconsistency, registered user DB modified? (watch: restarting)"));
			stop();
			start();
			return false; // critical ... exit from the call stack
		}
	} else {
		// not in our dictionary
		// prolly someone used /WATCH behind our back... bad boy!
		if(!(m_pConsole->notifyListView()->findEntry(mask.nick())))
		{
			notifyOnLine(mask.nick(),mask.user(),mask.host(),__tr2qs("watch entry added by user"));
		}
	}
	return true;
}

// FIXME: #warning "DEDICATED WATCH LIST VERBOSITY FLAG ? (To allow the user to use /WATCH l and manual /WATCH)"

bool KviWatchNotifyListManager::handleWatchReply(KviIrcMessage *msg)
{
	// 600: RPL_LOGON
	// :prefix 600 <target> <nick> <user> <host> <logintime> :logged online
	// 601: RPL_LOGON
	// :prefix 601 <target> <nick> <user> <host> <logintime> :logged offline
	// 604: PRL_NOWON
	// :prefix 604 <target> <nick> <user> <host> <logintime> :is online
	// 605: PRL_NOWOFF
	// :prefix 605 <target> <nick> <user> <host> 0 :is offline

// FIXME: #warning "Use the logintime in some way ?"

	const char * nk = msg->safeParam(1);
	const char * us = msg->safeParam(2);
	const char * ho = msg->safeParam(3);
	QString dnk = m_pConnection->decodeText(nk); 
	QString dus = m_pConnection->decodeText(us);
	QString dho = m_pConnection->decodeText(ho);

	if((msg->numeric() == RPL_LOGON) || (msg->numeric() == RPL_NOWON))
	{
		KviIrcMask m(dnk,dus,dho);
		doMatchUser(msg,dnk,m);
		return true;

	} else if(msg->numeric() == RPL_WATCHOFF)
	{
		if(m_pConsole->notifyListView()->findEntry(dnk))
		{
			notifyOffLine(dnk,dus,dho,__tr2qs("removed from watch list"));
		} else {
			if(_OUTPUT_VERBOSE)
				m_pConsole->output(KVI_OUT_SYSTEMMESSAGE,__tr2qs("Notify list: Stopped watching for \r!n\r%Q\r"),&dnk);
		}
		if(m_pRegUserDict->contains(dnk))
			m_pRegUserDict->remove(dnk); // kill that

		return true;

	} else if((msg->numeric() == RPL_LOGOFF) || (msg->numeric() == RPL_NOWOFF))
	{
		if(m_pConsole->notifyListView()->findEntry(dnk))
		{
			notifyOffLine(dnk,dus,dho,__tr2qs("watch"));
		} else {
			if(msg->numeric() == RPL_NOWOFF)
			{
				// This is a reply to a /watch +something
				if(_OUTPUT_VERBOSE)
					m_pConsole->output(KVI_OUT_SYSTEMMESSAGE,__tr2qs("Notify list: \r!n\r%Q\r is offline (watch)"),&dnk);
			} else {
				// This is a RPL_LOGOFF for an user that has not matched the reg-mask
				notifyOffLine(dnk,dus,dho,__tr2qs("unmatched watch list entry"));
			}
		}
		return true;
	}

	return false;
}

