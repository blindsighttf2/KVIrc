//=============================================================================
//
//   File : optw_nickserv.cpp
//   Creation date : Fri Aug 10 2001 03:38:10 CEST by Szymon Stefanek
//
//   This file is part of the KVirc irc client distribution
//   Copyright (C) 2001-2005 Szymon Stefanek (pragma at kvirc dot net)
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

#include "optw_nickserv.h"

#include <qlayout.h>
#include "kvi_tal_tooltip.h"
#include "kvi_tal_listview.h"
#include <qlineedit.h>
#include <qpushbutton.h>
#include <qcheckbox.h>
#include <qmessagebox.h>

#include "kvi_qstring.h"
#include "kvi_options.h"
#include "kvi_locale.h"
#include "kvi_nickserv.h"
#include "kvi_ircmask.h"

// kvi_app.cpp
extern KVIRC_API KviNickServRuleSet * g_pNickServRuleSet;


KviNickServRuleEditor::KviNickServRuleEditor(QWidget * par,bool bUseServerMaskField)
: QDialog(par)
{
	setCaption(__tr2qs_ctx("NickServ Authentication Rule","options"));

	QString html_center_begin = "<center>";
	QString html_center_end = "</center>";

	QGridLayout * gl = new QGridLayout(this,bUseServerMaskField ? 7 : 6,4,10,5);
	
	QLabel * l = new QLabel(__tr2qs_ctx("Registered NickName","options"),this);
	gl->addWidget(l,0,0);
	
	m_pRegisteredNickEdit = new QLineEdit(this);
#ifdef COMPILE_INFO_TIPS
	KviTalToolTip::add(m_pRegisteredNickEdit,html_center_begin + __tr2qs_ctx("Put here the nickname that you have registered with NickServ","options") + html_center_end);
#endif
	gl->addMultiCellWidget(m_pRegisteredNickEdit,0,0,1,3);
	
	l = new QLabel(__tr2qs_ctx("NickServ Mask","options"),this);
	gl->addWidget(l,1,0);
	
	m_pNickServMaskEdit = new QLineEdit(this);
#ifdef COMPILE_INFO_TIPS
	KviTalToolTip::add(m_pNickServMaskEdit,
		html_center_begin + __tr2qs_ctx("This is the mask that NickServ must match to be correctly identified as the NickServ service. "  \
			"This usually will be something like <b>NickServ!service@services.dalnet</b>.<br>" \
			"You can use wildcards for this field, but generally it is a security flaw. " \
			"If you're 100%% sure that NO user on the network can use the nickname \"NickServ\", " \
			"the mask <b>NickServ!*@*</b> may be safe to use in this field.","options") + html_center_end);
#endif
	gl->addMultiCellWidget(m_pNickServMaskEdit,1,1,1,3);
	
	l = new QLabel(__tr2qs_ctx("Message Regexp","options"),this);
	gl->addWidget(l,2,0);

	m_pMessageRegexpEdit = new QLineEdit(this);
	gl->addMultiCellWidget(m_pMessageRegexpEdit,2,2,1,3);

#ifdef COMPILE_INFO_TIPS
	KviTalToolTip::add(m_pMessageRegexpEdit,
		html_center_begin + __tr2qs_ctx("This is the simple regular expression that the identification request message "  \
			"from NickServ must match in order to be correctly recognized.<br>" \
			"The message is usually something like \"To identify yourself please use /ns IDENTIFY password\" " \
			"and it is sent when the NickServ wants you to authenticate yourself. " \
			"You can use the * and ? wildcards.","options") + html_center_end);
#endif
	l = new QLabel(__tr2qs_ctx("Identify Command","options"),this);
	gl->addWidget(l,3,0);

	m_pIdentifyCommandEdit = new QLineEdit(this);
#ifdef COMPILE_INFO_TIPS
	KviTalToolTip::add(m_pIdentifyCommandEdit,
		html_center_begin + __tr2qs_ctx("This is the command that will be executed when NickServ requests authentication " \
			"for the nickname described in this rule (if the both server and NickServ mask are matched). " \
			"This usually will be something like <b>msg NickServ identify &lt;yourpassword&gt;</b>.<br>" \
			"You can use <b>msg -q</b> if you don't want the password echoed on the screen. " \
			"Please note that there is no leading slash in this command.","options") + html_center_end);
#endif
	gl->addMultiCellWidget(m_pIdentifyCommandEdit,3,3,1,3);


	int iNextLine = 4;

	if(bUseServerMaskField)
	{
		l = new QLabel(__tr2qs_ctx("Server mask","options"),this);
		gl->addWidget(l,4,0);
		
		m_pServerMaskEdit = new QLineEdit(this);
#ifdef COMPILE_INFO_TIPS
		KviTalToolTip::add(m_pServerMaskEdit,
			html_center_begin + __tr2qs_ctx("This is the mask that the current server must match in order " \
			"for this rule to apply. It can contain * and ? wildcards.<br>Do NOT use simply \"*\" here...","options") + html_center_end);
#endif
		gl->addMultiCellWidget(m_pServerMaskEdit,4,4,1,3);
		iNextLine++;
	} else {
		m_pServerMaskEdit = 0;
	}


#ifdef COMPILE_INFO_TIPS
	l = new QLabel(html_center_begin + __tr2qs_ctx("Hint: Move the mouse cursor over the fields to get help","options") + html_center_end,this);
#else
	l = new QLabel("",this);
#endif
	l->setMargin(10);
	gl->addMultiCellWidget(l,iNextLine,iNextLine,0,3);

	iNextLine++;

	QPushButton * p = new QPushButton(__tr2qs_ctx("Cancel","options"),this);
	p->setMinimumWidth(100);
	connect(p,SIGNAL(clicked()),this,SLOT(reject()));
	gl->addWidget(p,iNextLine,2);

	m_pOkButton = new QPushButton(__tr2qs_ctx("OK","options"),this);
	m_pOkButton->setMinimumWidth(100);
	m_pOkButton->setDefault(true);
	connect(m_pOkButton,SIGNAL(clicked()),this,SLOT(okPressed()));
	gl->addWidget(m_pOkButton,iNextLine,3);

	gl->setColStretch(1,1);
	gl->setRowStretch(bUseServerMaskField ? 5 : 4,1);
	
	setMinimumWidth(250);
}

KviNickServRuleEditor::~KviNickServRuleEditor()
{
}



bool KviNickServRuleEditor::validate()
{
	QString s = m_pRegisteredNickEdit->text();
	
	QString m = __tr2qs_ctx("Invalid NickServ Rule","options");
	QString o = __tr2qs_ctx("OK","options");
	
	if(s.isEmpty())
	{
		QMessageBox::warning(this,m,__tr2qs_ctx("The Nickname field can't be empty!","options"),o);
		return false;
	}
	
	if(s.find(QChar(' ')) != -1)
	{
		QMessageBox::warning(this,m,__tr2qs_ctx("The Nickname field can't contain spaces!","options"),o);
		return false;
	}
	
	s = m_pNickServMaskEdit->text();
	
	if(s.isEmpty())
	{
		QMessageBox::warning(this,m,__tr2qs_ctx("The NickServ mask can't be empty!<br>You must put at least * there.","options"),o);
		return false;
	}

	s = m_pMessageRegexpEdit->text();
	
	if(s.isEmpty())
	{
		QMessageBox::warning(this,m,__tr2qs_ctx("The Message Regexp can't be empty!<br>You must put at least * there.","options"),o);
		return false;
	}

	s = m_pIdentifyCommandEdit->text();
	
	if(s.isEmpty())
	{
		QMessageBox::warning(this,m,__tr2qs_ctx("The Identify Command can't be empty!","options"),o);
		return false;
	}
	
	return true;
}

void KviNickServRuleEditor::okPressed()
{
	if(!validate())return;
	accept();
}


bool KviNickServRuleEditor::editRule(KviNickServRule * r)
{
	m_pRegisteredNickEdit->setText(r->registeredNick().isEmpty() ? QString("MyNick") : r->registeredNick());
	m_pNickServMaskEdit->setText(r->nickServMask().isEmpty() ? QString("NickServ!*@*") : r->nickServMask());
	m_pMessageRegexpEdit->setText(r->messageRegexp().isEmpty() ? QString("*IDENTIFY*") : r->messageRegexp());
	m_pIdentifyCommandEdit->setText(r->identifyCommand().isEmpty() ? QString("msg -q NickServ IDENTIFY <password>") : r->identifyCommand());
	if(m_pServerMaskEdit)
		m_pServerMaskEdit->setText(r->serverMask().isEmpty() ? QString("irc.yourserver.org") : r->serverMask());
	m_pRegisteredNickEdit->selectAll();
	if(exec() != QDialog::Accepted)return false;
	r->setRegisteredNick(m_pRegisteredNickEdit->text());
	r->setNickServMask(m_pNickServMaskEdit->text());
	r->setMessageRegexp(m_pMessageRegexpEdit->text());
	r->setIdentifyCommand(m_pIdentifyCommandEdit->text());
	if(m_pServerMaskEdit)
		r->setServerMask(m_pServerMaskEdit->text());
	return true;
}



KviNickServOptionsWidget::KviNickServOptionsWidget(QWidget * parent)
: KviOptionsWidget(parent,"nickserv_options_widget")
{
	createLayout(3,3);
	
	QGridLayout * gl = layout();

	KviNickServRuleSet * rs = g_pNickServRuleSet;
	bool bNickServEnabled = rs ? (rs->isEnabled() && !rs->isEmpty()) : false;

	m_pNickServCheck = new KviStyledCheckBox(__tr2qs_ctx("Enable NickServ Identification","options"),this);
	gl->addMultiCellWidget(m_pNickServCheck,0,0,0,2);
#ifdef COMPILE_INFO_TIPS
	KviTalToolTip::add(m_pNickServCheck,
			__tr2qs_ctx("This check enables the automatic identification with NickServ","options"));
#endif
	m_pNickServCheck->setChecked(bNickServEnabled);

	m_pNickServListView = new KviTalListView(this);
	m_pNickServListView->setSelectionMode(KviTalListView::Single);
	m_pNickServListView->setAllColumnsShowFocus(true);
	m_pNickServListView->addColumn(__tr2qs_ctx("Nickname","options"));
	m_pNickServListView->addColumn(__tr2qs_ctx("Server mask","options"));
	m_pNickServListView->addColumn(__tr2qs_ctx("NickServ Mask","options"));
	m_pNickServListView->addColumn(__tr2qs_ctx("NickServ Request Mask","options"));
	m_pNickServListView->addColumn(__tr2qs_ctx("Identify Command","options"));
	connect(m_pNickServListView,SIGNAL(selectionChanged()),this,SLOT(enableDisableNickServControls()));

	gl->addMultiCellWidget(m_pNickServListView,1,1,0,2);
#ifdef COMPILE_INFO_TIPS
	KviTalToolTip::add(m_pNickServListView,
		__tr2qs_ctx("<center>This is a list of NickServ identification rules. " \
			"KVIrc will use them to model its automatic interaction with NickServ on all the networks.<br>" \
			"Please be aware that this feature can cause your NickServ passwords to be stolen " \
			"if used improperly. Make sure that you fully understand the NickServ authentication protocol.<br>" \
			"In other words, be sure to know what you're doing.<br>" \
			"Also note that the password that you provide is stored as <b>PLAIN TEXT</b>.<br>" \
			"KVIrc supports also per-network NickServ authentication rules that can be " \
			"created in the \"Advanced...\" network options (accessible from the servers dialog)."
			"</center>","options"));
#endif

	m_pAddRuleButton = new QPushButton(__tr2qs_ctx("Add Rule","options"),this);
	connect(m_pAddRuleButton,SIGNAL(clicked()),this,SLOT(addNickServRule()));
	gl->addWidget(m_pAddRuleButton,2,0);

	m_pEditRuleButton = new QPushButton(__tr2qs_ctx("Edit Rule","options"),this);
	connect(m_pEditRuleButton,SIGNAL(clicked()),this,SLOT(editNickServRule()));
	gl->addWidget(m_pEditRuleButton,2,1);

	m_pDelRuleButton = new QPushButton(__tr2qs_ctx("Delete Rule","options"),this);
	connect(m_pDelRuleButton,SIGNAL(clicked()),this,SLOT(delNickServRule()));
	gl->addWidget(m_pDelRuleButton,2,2);

	connect(m_pNickServCheck,SIGNAL(toggled(bool)),this,SLOT(enableDisableNickServControls()));

	if(rs && rs->rules())
	{
		foreach(KviNickServRule * rule,*(rs->rules()))
		{
			(void)new KviTalListViewItem(m_pNickServListView,rule->registeredNick(),rule->serverMask(),rule->nickServMask(),rule->messageRegexp(),rule->identifyCommand());
		}
	}

	enableDisableNickServControls();

	gl->setRowStretch(1,1);


}

KviNickServOptionsWidget::~KviNickServOptionsWidget()
{
}

void KviNickServOptionsWidget::editNickServRule()
{
	KviTalListViewItem * it = m_pNickServListView->currentItem();
	if(!it)return;
	KviNickServRule r(it->text(0),it->text(2),it->text(3),it->text(4),it->text(1));
	KviNickServRuleEditor ed(this,true);
	if(ed.editRule(&r))
	{
		it->setText(0,r.registeredNick());
		it->setText(1,r.serverMask());
		it->setText(2,r.nickServMask());
		it->setText(3,r.messageRegexp());
		it->setText(4,r.identifyCommand());
	}
}

void KviNickServOptionsWidget::addNickServRule()
{
	KviNickServRule r;
	KviNickServRuleEditor ed(this,true);
	if(ed.editRule(&r))
		(void)new KviTalListViewItem(m_pNickServListView,r.registeredNick(),r.serverMask(),r.nickServMask(),r.messageRegexp(),r.identifyCommand());
}

void KviNickServOptionsWidget::delNickServRule()
{
	KviTalListViewItem * it = m_pNickServListView->currentItem();
	if(!it)return;
	delete it;
	enableDisableNickServControls();
}

void KviNickServOptionsWidget::enableDisableNickServControls()
{
	bool bEnabled = m_pNickServCheck->isChecked();
	m_pNickServListView->setEnabled(bEnabled);
	m_pAddRuleButton->setEnabled(bEnabled);
	bEnabled = bEnabled && (m_pNickServListView->childCount() > 0) && m_pNickServListView->currentItem();
	m_pDelRuleButton->setEnabled(bEnabled);
	m_pEditRuleButton->setEnabled(bEnabled);
}

void KviNickServOptionsWidget::commit()
{
	g_pNickServRuleSet->clear();
	if(m_pNickServListView->childCount() > 0)
	{
		g_pNickServRuleSet->setEnabled(m_pNickServCheck->isChecked());
		KviTalListViewItem * it = m_pNickServListView->firstChild();
		while(it)
		{
			g_pNickServRuleSet->addRule(KviNickServRule::createInstance(it->text(0),it->text(2),it->text(3),it->text(4),it->text(1)));
			it = it->nextSibling();
		}
	}
	KviOptionsWidget::commit();
}

