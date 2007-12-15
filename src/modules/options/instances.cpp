

//
//   File : instances.cpp
//
//   This file is part of the KVirc irc client distribution
//   Copyright (C) 2007 Alexey Y Uzhva
//   Copyright (C) 2001 Szymon Stefanek (stefanek@tin.it)
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

//
// Instance creation routines for the KVIrc options module
// DO NOT EDIT THIS FILE!! ALL CHANGES WILL BE LOST!!
// This file is automatically generated by mkcreateinstanceproc.sh
// so any change should go in that script
//

#include "instances.h"

KviOptionsPageDescriptorBase::KviOptionsPageDescriptorBase(
				const QString& szName,
				const QString& szKeywords,
				const QString& szKeywordsNoLocale,
				const QString& szGroup,
				const QString& szParent,
				int iIcon,
				int iPriority
				)
:m_szName(szName),m_szKeywords(szKeywords),m_szParent(szParent),
m_szKeywordsNoLocale(szKeywords),m_szGroup(szGroup),
m_iIcon(iIcon),m_iPriority(iPriority),m_pInstance(0)
{
	m_pChilds = new QMap<int,KviOptionsPageDescriptorBase*>;
	KviOptionsManager::registerWidget(this);
}

KviOptionsPageDescriptorBase::~KviOptionsPageDescriptorBase()
{
	//childs must be destroyed by KviOptionsManager
	delete m_pChilds;
}

// private
static KviOptionsManager  *g_pInstance = 0;
				
KviOptionsManager::KviOptionsManager()
{
	m_pRegistrationMutex = new QMutex;
	g_pInstance = this;
}

KviOptionsManager::~KviOptionsManager()
{
	qDeleteAll(m_pages);
	g_pInstance = 0;
}

static KviOptionsManager::KviOptionsManager* instance()
{
	return g_pInstance; 
}

const QMap<int,KviOptionsPageDescriptorBase*> & KviOptionsManager::instanceEntryTree()
{
	m_pRegistrationMutex->lock();
	m_pRegistrationMutex->unlock();
	return m_pages;
}

void KviOptionsManager::registrationStarted(int approxNubberOfPages)
{
	g_pInstance->m_pRegistrationMutex->lock();
	if(approxNubberOfPages){
		g_pInstance->m_pagesByName.reserve(approxNubberOfPages);
		g_pInstance->m_groupAndWidgetCache.reserve(approxNubberOfPages);
	}
}

void KviOptionsManager::registerWidget(KviOptionsPageDescriptorBase* pDescriptor)
{
	g_pInstance->m_pagesByName.insert(pDescriptor->m_szName,pDescriptor);
	g_pInstance->m_groupAndWidgetCache.insert(pDescriptor,pDescriptor->m_szGroup);
}

void KviOptionsManager::registrationFinished()
{
	KviOptionsPageDescriptorBase* parent;
	foreach(KviOptionsPageDescriptorBase* page,m_pInstance->m_pagesByName)
	{
		if(!page->m_szParent.isEmpty())
		{
			parent=g_pInstance->m_pagesByName.value(page->m_szParent);
			if(parent)
			{
				parent->m_pChilds->insert(page->m_iPriority,page);
			} else {
				g_pInstance->m_pages.insert(page->m_iPriority,page);
			}
		} else {
			g_pInstance->m_pages.insert(page->m_iPriority,page);
		}
	}
	g_pInstance->m_pagesByName.squeeze();
	g_pInstance->m_groupAndWidgetCache.squeeze();
	g_pInstance->m_pRegistrationMutex->unlock();
}

