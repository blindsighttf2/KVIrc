#ifndef _KVI_ACTIONMANAGER_H_
#define _KVI_ACTIONMANAGER_H_
//=============================================================================
//
//   File : kvi_actionmanager.h
//   Created on Sun 21 Nov 2004 03:37:57 by Szymon Stefanek
//
//   This file is part of the KVIrc IRC Client distribution
//   Copyright (C) 2004 Szymon Stefanek <pragma at kvirc dot net>
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

#include "kvi_settings.h"
#include "kvi_action.h"

#include <qobject.h>
#include <QHash>

class KviActionDrawer;
class KviCustomToolBar;

class KVIRC_API KviActionManager : public QObject
{
	friend class KviActionDrawer;
	friend class KviCustomizeToolBarsDialog;
	friend class KviCustomToolBar;
	friend class KviFrame;
	Q_OBJECT
public:
	KviActionManager();
	~KviActionManager();
protected:
	static KviActionManager * m_pInstance;
	QHash<QString,KviAction*> * m_pActions;
	QHash<QString,KviActionCategory*> * m_pCategories;
	static bool m_bCustomizingToolBars;
	
	// action categories
	static KviActionCategory * m_pCategoryIrc;
	static KviActionCategory * m_pCategoryGeneric;
	static KviActionCategory * m_pCategorySettings;
	static KviActionCategory * m_pCategoryScripting;
	static KviActionCategory * m_pCategoryGUI;
	static KviActionCategory * m_pCategoryChannel;
	static KviActionCategory * m_pCategoryTools;
	// internal , current toolbar to be edited (only when customizing)
	static KviCustomToolBar * m_pCurrentToolBar;
	bool m_bCoreActionsRegistered;
public:
	static void init();
	static void done();
	static KviActionManager * instance(){ return m_pInstance; };
	static void loadAllAvailableActions();
	static bool customizingToolBars(){ return m_bCustomizingToolBars; };
	static KviActionCategory * categoryIrc(){ return m_pCategoryIrc; };
	static KviActionCategory * categoryGeneric(){ return m_pCategoryGeneric; };
	static KviActionCategory * categorySettings(){ return m_pCategorySettings; };
	static KviActionCategory * categoryScripting(){ return m_pCategoryScripting; };
	static KviActionCategory * categoryGUI(){ return m_pCategoryGUI; };
	static KviActionCategory * categoryChannel(){ return m_pCategoryChannel; };
	static KviActionCategory * categoryTools(){ return m_pCategoryTools; };
	
	QHash<QString,KviAction*> * actions(){ return m_pActions; };
	KviActionCategory * category(const QString &szName);
	QHash<QString,KviActionCategory*> * categories(){ return m_pCategories; };
	
	void killAllKvsUserActions();

	static KviCustomToolBar * currentToolBar(){ return m_pCurrentToolBar; };
	KviAction * getAction(const QString &szName);
	void listActionsByCategory(const QString &szCatName,QList<KviAction*> * pBuffer);
	QString nameForAutomaticAction(const QString &szTemplate);
	bool coreActionExists(const QString &szName);

	void load(const QString &szFileName);
	void save(const QString &szFileName);

	bool registerAction(KviAction * a);
	bool unregisterAction(const QString &szName);

	void emitRemoveActionsHintRequest();
protected:
	void setCurrentToolBar(KviCustomToolBar * t);
	KviAction * findAction(const QString &szName){ return m_pActions->value(szName); };
	void customizeToolBarsDialogCreated();
	void customizeToolBarsDialogDestroyed();
	void tryFindCurrentToolBar();
	void delayedRegisterAccelerators(); // this is called ONCE by KviFrame, at startup
protected slots:
	void actionDestroyed();
signals:
	void beginCustomizeToolBars();
	void endCustomizeToolBars();
	void currentToolBarChanged(); // emitted only when customizing!
	void removeActionsHintRequest(); // connected by the KviCustomToolBarDialog to flash the trashcan
	                                 // when the user tries to remove an action from the toolbar
	                                 // and it fails to drag it on the trashcan
};

#define ACTION_POPUP_ITEM(__name,__popup) \
	{ KviAction * a = KviActionManager::instance()->getAction(__name); if(a)a->addToPopupMenu(__popup); }


#endif //!_KVI_ACTIONMANAGER_H_
