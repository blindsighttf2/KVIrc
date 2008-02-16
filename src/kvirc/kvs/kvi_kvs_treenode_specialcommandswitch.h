#ifndef _KVI_KVS_TREENODE_SPECIALCOMMANDSWITCH_H_
#define _KVI_KVS_TREENODE_SPECIALCOMMANDSWITCH_H_
//=============================================================================
//
//   File : kvi_kvs_treenode_specialcommandswitch.h
//   Created on Fri 02 Jan 2004 13:54:00 by Szymon Stefanek
//
//   This file is part of the KVIrc IRC client distribution
//   Copyright (C) 2008 Szymon Stefanek <pragma at kvirc dot net>
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
#include "kvi_qstring.h"
#include "kvi_kvs_treenode.h"
#include "kvi_kvs_treenode_specialcommand.h"

class KviKvsTreeNodeExpression;
class KviKvsTreeNodeInstruction;
class KviKvsRunTimeContext;
class KviKvsVariant;

class KVIRC_API KviKvsTreeNodeSpecialCommandSwitchLabel : public KviKvsTreeNode
{
public:
	KviKvsTreeNodeSpecialCommandSwitchLabel(const QChar * pLocation);
	~KviKvsTreeNodeSpecialCommandSwitchLabel();
protected:
	KviKvsTreeNodeData        * m_pParameter;
	KviKvsTreeNodeInstruction * m_pInstruction;
	bool                        m_bHasTerminatingBreak;
public:
	void setParameter(KviKvsTreeNodeData * pParameter);
	void setInstruction(KviKvsTreeNodeInstruction * pInstruction);
	void setTerminatingBreak(bool b){ m_bHasTerminatingBreak = b; };
	virtual bool execute(KviKvsRunTimeContext * c,KviKvsVariant * pRealParameter) = 0;
};


class KVIRC_API KviKvsTreeNodeSpecialCommandSwitchLabelCase : public KviKvsTreeNodeSpecialCommandSwitchLabel
{
public:
	KviKvsTreeNodeSpecialCommandSwitchLabelCase(const QChar * pLocation);
	~KviKvsTreeNodeSpecialCommandSwitchLabelCase();
public:
	virtual void contextDescription(QString &szBuffer);
	virtual void dump(const char * prefix);
	virtual bool execute(KviKvsRunTimeContext * c,KviKvsVariant * pRealParameter);
};


class KVIRC_API KviKvsTreeNodeSpecialCommandSwitchLabelMatch : public KviKvsTreeNodeSpecialCommandSwitchLabel
{
public:
	KviKvsTreeNodeSpecialCommandSwitchLabelMatch(const QChar * pLocation);
	~KviKvsTreeNodeSpecialCommandSwitchLabelMatch();
public:
	virtual void contextDescription(QString &szBuffer);
	virtual void dump(const char * prefix);
	virtual bool execute(KviKvsRunTimeContext * c,KviKvsVariant * pRealParameter);
};

class KVIRC_API KviKvsTreeNodeSpecialCommandSwitchLabelRegexp : public KviKvsTreeNodeSpecialCommandSwitchLabel
{
public:
	KviKvsTreeNodeSpecialCommandSwitchLabelRegexp(const QChar * pLocation);
	~KviKvsTreeNodeSpecialCommandSwitchLabelRegexp();
public:
	virtual void contextDescription(QString &szBuffer);
	virtual void dump(const char * prefix);
	virtual bool execute(KviKvsRunTimeContext * c,KviKvsVariant * pRealParameter);
};

class KVIRC_API KviKvsTreeNodeSpecialCommandSwitchLabelDefault : public KviKvsTreeNodeSpecialCommandSwitchLabel
{
public:
	KviKvsTreeNodeSpecialCommandSwitchLabelDefault(const QChar * pLocation);
	~KviKvsTreeNodeSpecialCommandSwitchLabelDefault();
public:
	virtual void dump(const char * prefix);
	virtual void contextDescription(QString &szBuffer);
	virtual bool execute(KviKvsRunTimeContext * c,KviKvsVariant * pRealParameter);
};



class KVIRC_API KviKvsTreeNodeSpecialCommandSwitch : public KviKvsTreeNodeSpecialCommand
{
public:
	KviKvsTreeNodeSpecialCommandSwitch(const QChar * pLocation,KviKvsTreeNodeExpression * e);
	~KviKvsTreeNodeSpecialCommandSwitch();
private:
	KviKvsTreeNodeExpression * m_pExpression; // can't be 0
	QList<KviKvsTreeNodeSpecialCommandSwitchLabel*> * m_pLabels; //can't be 0
public:
	void addLabel(KviKvsTreeNodeSpecialCommandSwitchLabel * l);
	bool isEmpty(){ return m_pLabels->isEmpty(); };
	virtual void contextDescription(QString &szBuffer);
	virtual void dump(const char * prefix);
	virtual bool execute(KviKvsRunTimeContext * c);
};

#endif //!_KVI_KVS_TREENODE_SPECIALCOMMANDSWITCH_H_
