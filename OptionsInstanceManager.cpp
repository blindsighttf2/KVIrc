

//=============================================================================
//
//   File : OptionsInstanceManager.cpp
//
//   This file is part of the KVIrc IRC client distribution
//   Copyright (C) 2001-2008 Szymon Stefanek (stefanek@tin.it)
//
//   This program is FREE software. You can redistribute it and/or
//   modify it under the terms of the GNU General Public License
//   as published by the Free Software Foundation; either version 2
//   of the License, or (at your option) any later version.
//
//   This program is distributed in the HOPE that it will be USEFUL,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//   See the GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program. If not, write to the Free Software Foundation,
//   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
//=============================================================================

//
// Instance creation routines for the KVIrc options module
// DO NOT EDIT THIS FILE!! ALL CHANGES WILL BE LOST!!
// This file is automatically generated by mkcreateinstanceproc.sh
// so any change should go in that script
//

#include "OptionsWidget_*.h"

#include "KviLocale.h"
#include "OptionsInstanceManager.h"

int g_iOptionWidgetInstances = 0;




OptionsInstanceManager::OptionsInstanceManager()
	: QObject(0)
{

	//qDebug("Instantiating");
	// Create the global widget dict : case sensitive, do not copy keys
	m_pInstanceTree = new KviPointerList<OptionsWidgetInstanceEntry>;
	m_pInstanceTree->setAutoDelete(true);


}

void OptionsInstanceManager::deleteInstanceTree(KviPointerList<OptionsWidgetInstanceEntry> * pList)
{
	if(pList)
	{
		for(OptionsWidgetInstanceEntry * pEntry = pList->first(); pEntry; pEntry = pList->next())
		{
			if(pEntry->pWidget)
			{
				if(pEntry->pWidget->parent()->inherits("OptionsWidgetContainer"))
				{
					disconnect(pEntry->pWidget,SIGNAL(destroyed()),this,SLOT(widgetDestroyed()));
					delete pEntry->pWidget->parent();
					pEntry->pWidget =  0;
				} else {
					qDebug("Ops...i have deleted the options dialog ?");
				}
			} //else qDebug("Class %s has no widget",e->szName);
			if(pEntry->pChildList)
				deleteInstanceTree(pEntry->pChildList);
		}
		delete pList;
	}
}

OptionsInstanceManager::~OptionsInstanceManager()
{
	if(m_pInstanceTree)
		qDebug("Ops...OptionsInstanceManager::cleanup() not called ?");
}

void OptionsInstanceManager::cleanup(KviModule *)
{
	deleteInstanceTree(m_pInstanceTree);
	m_pInstanceTree = nullptr;
}

void OptionsInstanceManager::widgetDestroyed()
{
	OptionsWidgetInstanceEntry * pEntry = findInstanceEntry(sender(),m_pInstanceTree);
	if(pEntry)
		pEntry->pWidget = nullptr;
	if(g_iOptionWidgetInstances > 0)
		g_iOptionWidgetInstances--;

}

KviOptionsWidget * OptionsInstanceManager::getInstance(OptionsWidgetInstanceEntry * pEntry, QWidget * pPar)
{
	if(!pEntry)
		return NULL;

#if 0
	if(pEntry->pWidget)
	{
		if(pEntry->pWidget->parent() != pPar)
		{
			QWidget * pOldPar = (QWidget *)pEntry->pWidget->parent();
			pEntry->pWidget->setParent(pPar);
			pOldPar->deleteLater();
			pEntry->pWidget = nullptr;
		}
	}
#endif

	if(!(pEntry->pWidget))
	{
		pEntry->pWidget = pEntry->createProc(pPar);
		g_iOptionWidgetInstances++;
		connect(pEntry->pWidget,SIGNAL(destroyed()),this,SLOT(widgetDestroyed()));
	}

	if(pEntry->pWidget->parent() != pPar)
	{
		QWidget * pOldPar = (QWidget *)pEntry->pWidget->parent();
		pEntry->pWidget->setParent(pPar); //reparent(pPar,QPoint(0,0));
		if(pOldPar->inherits("OptionsWidgetContainer"))
			delete pOldPar;
		// else it's very likely a QStackedWidget, child of a KviOptionsWidget: don't delete
	}

	if(pEntry->bIsContainer)
	{
		// need to create the container structure!
		pEntry->pWidget->createTabbedPage();
		if(pEntry->pChildList)
		{
			KviPointerList<OptionsWidgetInstanceEntry> tmpList;
			tmpList.setAutoDelete(false);

			for(OptionsWidgetInstanceEntry * pEntry2 = pEntry->pChildList->first(); pEntry2; pEntry2 = pEntry->pChildList->next())
			{
				// add only non containers and widgets not explicitly marked as noncontained
				if((!pEntry2->bIsContainer) && (!pEntry2->bIsNotContained))
				{
					OptionsWidgetInstanceEntry * pEntry3 = tmpList.first();
					int iIdx = 0;
					while(pEntry3)
					{
						if(pEntry3->iPriority >= pEntry2->iPriority)
							break;
						iIdx++;
						pEntry3 = tmpList.next();
					}
					tmpList.insert(iIdx,pEntry2);
				}
			}

			for(OptionsWidgetInstanceEntry * pEntry4 = tmpList.last(); pEntry4; pEntry4 = tmpList.prev())
			{
				KviOptionsWidget * pOpt = getInstance(pEntry4,pEntry->pWidget->tabWidget());
				pEntry->pWidget->addOptionsWidget(pEntry4->szName,*(g_pIconManager->getSmallIcon(pEntry4->eIcon)),pOpt);
			}
		}
	}
	return pEntry->pWidget;
}

OptionsWidgetInstanceEntry * OptionsInstanceManager::findInstanceEntry(const QObject * pObj, KviPointerList<OptionsWidgetInstanceEntry> * pList)
{
	if(pList)
	{
		for(OptionsWidgetInstanceEntry * pEntry = pList->first(); pEntry; pEntry = pList->next())
		{
			if(pObj == pEntry->pWidget)
				return pEntry;
			if(pEntry->pChildList)
			{
				OptionsWidgetInstanceEntry * pEntry2 = findInstanceEntry(pObj,pEntry->pChildList);
				if(pEntry2)
					return pEntry2;
			}
		}
	}
	return 0;
}

OptionsWidgetInstanceEntry * OptionsInstanceManager::findInstanceEntry(const char * pcName, KviPointerList<OptionsWidgetInstanceEntry> * pList)
{
	if(pList)
	{
		for(OptionsWidgetInstanceEntry * pEntry = pList->first(); pEntry; pEntry = pList->next())
		{
			if(kvi_strEqualCI(pEntry->szClassName,pcName))
				return pEntry;
			if(pEntry->pChildList)
			{
				OptionsWidgetInstanceEntry * pEntry2 = findInstanceEntry(pcName,pEntry->pChildList);
				if(pEntry2)
					return pEntry2;
			}
		}
	}
	return 0;
}

OptionsWidgetInstanceEntry * OptionsInstanceManager::findInstanceEntry(const char * pcName)
{
	return findInstanceEntry(pcName,m_pInstanceTree);
}

