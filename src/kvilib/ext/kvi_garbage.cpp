//
//   File : kvi_garbage.cpp
//   Creation date : Mon Dec  3 16:49:15 2001 GMT by Szymon Stefanek
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



#include "kvi_garbage.h"

#include <qvariant.h>

KviGarbageCollector::KviGarbageCollector()
: QObject(0)
{
	m_pGarbageList = 0;
	m_pCleanupTimer = 0;
	m_bForceCleanupNow = false;
}

KviGarbageCollector::~KviGarbageCollector()
{
	m_bForceCleanupNow = true;
	cleanup();
}

void KviGarbageCollector::collect(QObject * g)
{
	if(!m_pGarbageList)
	{
		m_pGarbageList = new QSet<QObject*>;
	}
	//debug("COLLECTING GARBAGE %s",g->className());
	m_pGarbageList->insert(g);
//	debug("Registering garbage object %d (%s:%s)",g,g->className(),g->name());
	connect(g,SIGNAL(destroyed()),this,SLOT(garbageSuicide()));
	triggerCleanup(0);
}

void KviGarbageCollector::garbageSuicide()
{
	if(!m_pGarbageList)
	{
		debug("Ops... garbage suicide while no garbage list");
		return;
	}
	if(!m_pGarbageList->contains(sender()))
	{
		debug("Ops... unregistered garbage suicide");
		return;
	}
	m_pGarbageList->remove(sender());
	if(m_pGarbageList->isEmpty())
	{
		cleanup();
	}
}

void KviGarbageCollector::triggerCleanup(int iTimeout)
{
	//debug("TRIGGERING CLEANUP AFTER %d msecs",iTimeout);
	if(m_pCleanupTimer)
	{
		m_pCleanupTimer->stop();
	} else {
		m_pCleanupTimer = new QTimer(this);
		connect(m_pCleanupTimer,SIGNAL(timeout()),this,SLOT(cleanup()));
	}
	m_pCleanupTimer->start(iTimeout);
}

void KviGarbageCollector::cleanup()
{
	//debug("CLEANUP CALLED !");
	if(m_pGarbageList)
	{
		//debug("SOME GARBAGE TO DELETE");
		QSet<QObject*> dying;
		foreach(QObject * o,*m_pGarbageList)
		{
			//debug("CHECKING GARBAGE CLASS %s",o->className());
			bool bDeleteIt = m_bForceCleanupNow;
			if(!bDeleteIt)
			{
				//debug("CLEANUP NOT FORCED");
				QVariant v = o->property("blockingDelete");
				if(v.isValid())
				{
					//debug("HAS A VALID VARIANT!");
//					debug("[Garbage collector]: garbage has a blockingDelete property");
					bDeleteIt = !(v.toBool());
//					if(!bDeleteIt)debug("And doesn't want to be delete now!");
				} else bDeleteIt = true; // must be deleted
			}
			if(bDeleteIt)dying.insert(o);
		}

		foreach(QObject * o2,dying)
		{
			//debug("KILLING GARBAGE CLASS %s",o2->className());
			disconnect(o2,SIGNAL(destroyed()),this,SLOT(garbageSuicide()));
			m_pGarbageList->remove(o2);
			delete o2;
		}

		if(m_pGarbageList->isEmpty())
		{
			delete m_pGarbageList;
			m_pGarbageList = 0;
		}
	}

	if(m_pGarbageList)
	{
//		debug("[Garbage collector cleanup]: Some stuff left to be deleted, will retry in a while");
		// something left to be destroyed
		if(m_bForceCleanupNow)debug("[Garbage collector]: Ops...I've left some undeleted stuff!");
		triggerCleanup(5000); // retry in 5 sec
	} else {
//		debug("[Garbage collector cleanup]: Completed");
		// nothing left to delete
		if(m_pCleanupTimer)
		{
			delete m_pCleanupTimer;
			m_pCleanupTimer = 0;
		}
	}
}

