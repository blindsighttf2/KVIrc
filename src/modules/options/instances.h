

#ifndef __OPTIONS_INSTANCES_H__
#define __OPTIONS_INSTANCES_H__

//
//   File : instances.h
//
//   This file is part of the KVirc irc client distribution
//   Copyright (C) 2001-2006 Szymon Stefanek (pragma at kvirc dot net)
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

#include "kvi_optionswidget.h"
#include <QMap>
#include <QMutex>

class KviOptionsManager;

class KviOptionsPageDescriptorBase
{
	friend class KviOptionsManager;
private:
	KviOptionsWidget                         *m_pInstance;
	QMap<int,KviOptionsPageDescriptorBase*>  *m_pChilds;
	
	QString                                   m_szName;
	QString                                   m_szKeywords;
	QString                                   m_szKeywordsNoLocale;
	QString                                   m_szGroup;
	QString                                   m_szParent;
	int                                       m_iIcon;
	int                                       m_iPriority;
public:
	KviOptionsPageDescriptorBase(
				const QString& szName,
				const QString& szKeywords,
				const QString& szKeywordsNoLocale,
				const QString& szGroup,
				const QString& szParent,
				int iIcon,
				int iPriority
				);
	virtual ~KviOptionsPageDescriptorBase();
	virtual KviOptionsWidget* createInstance(QWidget* pParent) = 0;
	virtual KviOptionsWidget* instance() { return m_pInstance; };
	
	const QString& name() { return m_szName; };
	const QString& group() { return m_szGroup; };
	QMap<int,KviOptionsPageDescriptorBase*>* childs() { return m_pChilds; };
	int icon() { return m_iIcon; };
};

template <typename T> class KviOptionsPageDescriptor : public KviOptionsPageDescriptorBase 
{
public:
	virtual KviOptionsWidget* createInstance(QWidget* pParent)
	{
		return new T(pParent);
	}
};


class KviOptionsManager
{
public:
	KviOptionsManager();
	~KviOptionsManager();
protected:
	QMap<int,KviOptionsPageDescriptorBase*>        m_pages;
	QHash<QString,KviOptionsPageDescriptorBase*>   m_pagesByName;
	
	// used only in setup phase
	QHash<KviOptionsPageDescriptorBase*,QString>   m_groupAndWidgetCache;
	
	QMutex                                        *m_pRegistrationMutex;
public:
	const QMap<int,KviOptionsPageDescriptorBase*> & instanceEntryTree();
	
	KviOptionsPageDescriptorBase* findPage(const QString& name) { return m_pagesByName.value(name); };
	
	static KviOptionsManager* instance();
	
	static void registrationStarted(int approxNubberOfPages=0);
	static void registerWidget(KviOptionsPageDescriptorBase* pDescriptor);
	static void registrationFinished();
};

#endif //__OPTIONS_INSTANCES_H__

