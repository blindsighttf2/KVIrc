//=============================================================================
//
//   File : kvi_kvs_timermanager.cpp
//   Created on Fri 19 Dec 2003 01:29:22 by Szymon Stefanek
//
//   This file is part of the KVIrc IRC client distribution
//   Copyright (C) 2003 Szymon Stefanek <pragma at kvirc dot net>
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



#include "kvi_kvs_timermanager.h"

#include "kvi_kvs_script.h"
#include "kvi_kvs_variantlist.h"
#include "kvi_kvs_runtimecontext.h"

#include "kvi_app.h"
#include "kvi_window.h"
#include "kvi_console.h"
#include "kvi_options.h"
#include "kvi_locale.h"
#include "kvi_out.h"


KviKvsTimer::KviKvsTimer(const QString &szName,Lifetime l,KviWindow * pWnd,int iDelay,int iId,KviKvsScript * pCallback,KviKvsVariantList * pParams)
{
	m_szName = szName;
	m_eLifetime = l;
	m_pWnd = pWnd;
	m_iDelay = iDelay;
	m_iId = iId;
	m_pCallback = pCallback;
	//m_pVariables = new KviKvsHash();
	m_pRunTimeData = new KviKvsExtendedRunTimeData(new KviKvsHash(),TRUE);
	m_pParameterList = pParams;
}

KviKvsTimer::~KviKvsTimer()
{
	delete m_pRunTimeData;
	delete m_pParameterList;
	delete m_pCallback;
}







KviKvsTimerManager * KviKvsTimerManager::m_pInstance = 0;


KviKvsTimerManager::KviKvsTimerManager()
: QObject()
{
	m_pTimerDictById = new QHash<int,KviKvsTimer*>; //auto
	m_pTimerDictByName = new QHash<QString,KviKvsTimer*>; //...
	m_pKilledTimerList = 0;
	m_iAssassinTimer = 0;
	m_iCurrentTimer = 0;
}

KviKvsTimerManager::~KviKvsTimerManager()
{
	foreach(KviKvsTimer*i,*m_pTimerDictById) {delete i;}
	delete m_pTimerDictById;
	foreach(KviKvsTimer*i,*m_pTimerDictByName) {delete i;}
	delete m_pTimerDictByName;
	if(m_pKilledTimerList)
	{
		qDeleteAll(*m_pKilledTimerList);
		delete m_pKilledTimerList;
	}
}

void KviKvsTimerManager::init()
{
	if(KviKvsTimerManager::m_pInstance)
	{
		debug("Trying to double init() the timer manager!");
		return;
	}
	KviKvsTimerManager::m_pInstance = new KviKvsTimerManager();
}

void KviKvsTimerManager::done()
{
	if(!KviKvsTimerManager::m_pInstance)
	{
		debug("Trying to call done() on a non existing timer manager!");
		return;
	}
	delete KviKvsTimerManager::m_pInstance;
	KviKvsTimerManager::m_pInstance = 0;
}

bool KviKvsTimerManager::addTimer(const QString &szName,KviKvsTimer::Lifetime l,KviWindow * pWnd,int iDelay,KviKvsScript * pCallback,KviKvsVariantList * pParams)
{
	int iId = startTimer(iDelay);

	if(iId <= 0)
	{
		delete pCallback;
        pCallback = 0;
		delete pParams;
        pParams = 0;
		return false;
	}

	KviKvsTimer * t = new KviKvsTimer(szName,l,pWnd,iDelay,iId,pCallback,pParams);
	KviKvsTimer * old = m_pTimerDictByName->value(szName);
	if(old)deleteTimer(old->id());
	m_pTimerDictByName->insert(szName,t);
	m_pTimerDictById->insert(t->id(),t);
	return true;
}

bool KviKvsTimerManager::deleteTimer(const QString &szName)
{
	KviKvsTimer * t = m_pTimerDictByName->value(szName);
	if(!t)return false;
	killTimer(t->id());
	m_pTimerDictById->remove(t->id());
	m_pTimerDictByName->remove(szName);
	scheduleKill(t);
	return true;
}

bool KviKvsTimerManager::deleteTimer(int iId)
{
	KviKvsTimer * t = m_pTimerDictById->value(iId);
	if(!t)return false;
	killTimer(t->id());
	m_pTimerDictById->remove(t->id());
	m_pTimerDictByName->remove(t->name());
	scheduleKill(t);
	return true;
}

bool KviKvsTimerManager::deleteCurrentTimer()
{
	if(!m_iCurrentTimer)return false;
	deleteTimer(m_iCurrentTimer);
	m_iCurrentTimer = 0;
	return true;
}

void KviKvsTimerManager::deleteAllTimers()
{
	if(m_pTimerDictById->isEmpty())return;
	QList<KviKvsTimer*> tl;
	foreach(KviKvsTimer * t,*m_pTimerDictById)
	{
		tl.append(t);
	}
	foreach(KviKvsTimer * dying,*m_pTimerDictById)
	{
		deleteTimer(dying->id());
	}
}

void KviKvsTimerManager::scheduleKill(KviKvsTimer *t)
{
	if(!m_pKilledTimerList)
	{
		m_pKilledTimerList = new QList<KviKvsTimer*>;
	}
	m_pKilledTimerList->append(t);

	if(!m_iAssassinTimer)m_iAssassinTimer = startTimer(0);
}


void KviKvsTimerManager::timerEvent(QTimerEvent *e)
{
	int iId = e->timerId();

	if(iId == m_iAssassinTimer)
	{
		if(!m_pKilledTimerList)
		{
			debug("ops.. assassing timer with no victims ?");
		} else {
			foreach(KviKvsTimer*i,*m_pKilledTimerList){ delete i;}
			m_pKilledTimerList->clear();
		}
		killTimer(m_iAssassinTimer);
		m_iAssassinTimer = 0;
		return;
	}

	KviKvsTimer * t = m_pTimerDictById->value(iId);
	if(!t)
	{
		debug("Internal error: got an nonexistant timer event");
		return; // HUH ?
	}

	if(!g_pApp->windowExists(t->window()))
	{
		if(t->lifetime() != KviKvsTimer::Persistent)
		{
			deleteTimer(t->id());
			return;
		}

		// rebind to an existing console
		t->setWindow(g_pApp->activeConsole());
	}

	KviKvsScript copy(*(t->callback()));

	m_iCurrentTimer = t->id();
	bool bRet = copy.run(t->window(),
			t->parameterList(),
			0,
			KviKvsScript::PreserveParams,
			t->runTimeData());

	m_iCurrentTimer = 0;

	if(!bRet)
	{
		// the timer may already have been scheduled for killing!
		if(KVI_OPTION_BOOL(KviOption_boolKillBrokenTimers))
		{
			t->window()->output(KVI_OUT_PARSERERROR,__tr2qs("Timer '%Q' has a broken callback handler: killing the timer"),&(t->name()));
			deleteTimer(t->id());
		}
		return;
	}

	// the timer may already have been scheduled for killing!

	if(t->lifetime() == KviKvsTimer::SingleShot)
	{
		deleteTimer(t->id());
	}
}
