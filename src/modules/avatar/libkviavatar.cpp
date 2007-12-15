//=============================================================================
//
//   File : libkviavatar.cpp
//   Creation date : Thu Nov 12 02:32:59 2004 GMT by Szymon Stefanek
//
//   This file is part of the KVirc irc client distribution
//   Copyright (C) 2004 Szymon Stefanek (pragma at kvirc dot net)
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

#include "libkviavatar.h"

#include "kvi_module.h"
#include "kvi_fileutils.h"
#include "kvi_locale.h"
#include "kvi_malloc.h"
#include "kvi_app.h"
#include "kvi_options.h"
#include "kvi_http.h"
#include "kvi_iconmanager.h"
#include "kvi_avatar.h"
#include "kvi_ircuserdb.h"

#include "kvi_ircconnection.h"
#include "kvi_ircconnectionuserinfo.h"
#include "kvi_console.h"
#include "kvi_filedialog.h"

#include "kvi_frame.h"
#include "kvi_sharedfiles.h"
#include "kvi_out.h"
#include "kvi_ircmask.h"
#include "kvi_qcstring.h"

#include <QLineEdit>
#include <QPushButton>
#include <QMessageBox>
#include <QTimer>
#include <QLabel>
#include <QLayout>

static QList<KviAsyncAvatarSelectionDialog*> * g_pAvatarSelectionDialogList = 0;
extern KVIRC_API KviSharedFilesManager * g_pSharedFilesManager;

KviAsyncAvatarSelectionDialog::KviAsyncAvatarSelectionDialog(QWidget * par,const QString &szInitialPath,KviIrcConnection * c)
: QDialog(par,0,false,Qt::WType_Dialog | Qt::WStyle_DialogBorder | Qt::WStyle_StaysOnTop)
{
	g_pAvatarSelectionDialogList->append(this);

	m_pConnection = c;

	setCaption(__tr2qs("Choose Avatar - KVIrc"));

	QGridLayout * g = new QGridLayout(this,3,3,4,8);
	
	QString msg = "<center>";
	msg += __tr2qs("Please select an avatar image. " \
				"The full path to a local file or an image on the Web can be used.<br>" \
				"If you wish to use a local image file, click the \"<b>Browse</b>\"" \
				"button to browse local folders.<br>" \
				"The full URL for an image (including <b>http://</b>) can be entered manually.");
	msg += "</center><br>";

	QLabel * l = new QLabel(msg,this);
	l->setMinimumWidth(250);

	g->addMultiCellWidget(l,0,0,0,2);

	m_pLineEdit = new QLineEdit(this);
	m_pLineEdit->setText(szInitialPath);
	m_pLineEdit->setMinimumWidth(180);

	g->addMultiCellWidget(m_pLineEdit,1,1,0,1);

	QPushButton * b = new QPushButton(__tr2qs("&Browse..."),this);
	connect(b,SIGNAL(clicked()),this,SLOT(chooseFileClicked()));
	g->addWidget(b,1,2);
	
	QWidget * h = new QWidget;

	QHBoxLayout * layout = new QHBoxLayout;
	layout->setSpacing(8);
	h->setLayout(layout);
	
	g->addMultiCellWidget(h,2,2,1,2);
	b = new QPushButton(__tr2qs("&OK"),h);
	layout->addWidget(b);
	
	b->setMinimumWidth(80);
	b->setDefault(true);
	connect(b,SIGNAL(clicked()),this,SLOT(okClicked()));

	b = new QPushButton(__tr2qs("Cancel"),h);
	b->setMinimumWidth(80);
	layout->addWidget(b);
	connect(b,SIGNAL(clicked()),this,SLOT(cancelClicked()));
	
	h->setLayout(layout);
	
	g->setRowStretch(0,1);
	g->setColStretch(0,1);
}

KviAsyncAvatarSelectionDialog::~KviAsyncAvatarSelectionDialog()
{
	g_pAvatarSelectionDialogList->removeAll(this);
}

void KviAsyncAvatarSelectionDialog::okClicked()
{
	m_szAvatarName = m_pLineEdit->text();

	if(!g_pApp->connectionExists(m_pConnection))return; // the connection no longer exists :/
	
	if(!m_szAvatarName.isEmpty())
	{
		QString tmp = m_szAvatarName;
		tmp.replace("\\","\\\\");
		QString szBuffer=QString("avatar.set \"%1\"").arg(tmp);
		KviKvsScript::run(szBuffer,m_pConnection->console());
	}
	
	accept();
	deleteLater();
}

void KviAsyncAvatarSelectionDialog::cancelClicked()
{
	reject();
	deleteLater();
}

void KviAsyncAvatarSelectionDialog::chooseFileClicked()
{
	QString tmp;
 	if(KviFileDialog::askForOpenFileName(tmp,__tr2qs("Choose an Image File - KVIrc")))
	{
		m_pLineEdit->setText(tmp);
	}
}

void KviAsyncAvatarSelectionDialog::closeEvent(QCloseEvent * e)
{
	e->ignore();
	reject();
	deleteLater();
}

/*
	@doc: avatar.set
	@type:
		command
	@title:
		avatar.set
	@keyterms:
		setting your avatar, avatar
	@short:
		Sets the local user's avatar
	@syntax:
		avatar.set [avatar:string]
	@description:
		Sets your avatar in the current connection to <avatar>.
		<avatar> may be a local filename or a http url.[br]
		If avatar is an empty string then an asynchronous dialog
		will be opened that will allow choosing an avatar.[br]
		Note that this command does NOT notify the avatar to
		any target: use [cmd]avatar.notify[/cmd] for that purpose.
		Note also that this will NOT set your default avatar
		option: you must use the options dialog for that.[br]
	@examples:
		[example]
			avatar.set /home/myavatar.png
			avatar.set http://www.kvirc.net/img/pragma.png
		[/example]
	@seealso:
		[cmd]avatar.unset[/cmd]
*/

static bool avatar_kvs_cmd_set(KviKvsModuleCommandCall * c)
{
	QString szAvatar;
	KVSM_PARAMETERS_BEGIN(c)
		KVSM_PARAMETER("avatar",KVS_PT_NONEMPTYSTRING,KVS_PF_OPTIONAL,szAvatar)
	KVSM_PARAMETERS_END(c)

	KVSM_REQUIRE_CONNECTION(c)

	QString absPath;

	if(szAvatar.isEmpty())
	{
		KviAsyncAvatarSelectionDialog * d = new KviAsyncAvatarSelectionDialog(g_pFrame,QString::null,c->window()->connection());
		d->show();
		return true;
	}
	
	// new avatar specified...try to load it

	KviIrcUserEntry * e = c->window()->connection()->userDataBase()->find(c->window()->connection()->currentNickName());
	if(!e)
	{
		c->warning(__tr2qs("Internal error: I'm not in the user database ?"));
		return true;
	}

	KviAvatar * av = g_pIconManager->getAvatar(QString::null,szAvatar);
	if(av)
	{
		// Ok...got it...
		e->setAvatar(av);
		c->window()->console()->avatarChanged(av,
			c->window()->connection()->userInfo()->nickName(),
			c->window()->connection()->userInfo()->userName(),
			c->window()->connection()->userInfo()->hostName(),
			QString::null);
	} else {
		bool bIsUrl = KviQString::equalCIN(szAvatar,"http://",7) && (szAvatar.length() > 7);

		if(bIsUrl)
		{
			// This is an url, and we don't have a cached copy for now
			QString szLocalFilePath;
			QString szLocalFile = szAvatar;
			g_pIconManager->urlToCachedFileName(szLocalFile);
			g_pApp->getLocalKvircDirectory(szLocalFilePath,KviApp::Avatars,szLocalFile);

			szLocalFilePath.replace("\\","\\\\");

			QString szCommand = "http.get -w=nm ";
				szCommand += szAvatar;
				szCommand += " ";
				szCommand += szLocalFilePath;

			if(KviKvsScript::run(szCommand,c->window()->console()))
			{
				g_pApp->setAvatarOnFileReceived(c->window()->console(),
						szAvatar,
						c->window()->connection()->userInfo()->nickName(),
						c->window()->connection()->userInfo()->userName(),
						c->window()->connection()->userInfo()->hostName());
			} else {
				c->warning(__tr2qs("Can't set the current avatar to '%Q': failed to start the http transfer"),&szAvatar);
				return true;
			}
		} else {
			c->warning(__tr2qs("Can't set the current avatar to '%Q': can't load the image"),&szAvatar);
			return true;
		}
	}

	return true;
}


/*
	@doc: avatar.unset
	@type:
		command
	@title:
		avatar.unset
	@short:
		Unsets the local user's avatar
	@syntax:
		avatar.unset
	@description:
		Unsets the local user's avatar.
		Note also that this will NOT unset your default avatar
		option: you must use the options dialog for that.[br]
	@seealso:
		[cmd]avatar.set[/cmd]
*/

static bool avatar_kvs_cmd_unset(KviKvsModuleCommandCall * c)
{
	KVSM_REQUIRE_CONNECTION(c)

	KviIrcUserEntry * e = c->window()->connection()->userDataBase()->find(c->window()->connection()->currentNickName());
	if(!e)
	{
		c->warning(__tr2qs("Internal error: I'm not in the user database ?"));
		return true;
	}

	e->setAvatar(0);
	c->window()->console()->avatarChanged(0,
			c->window()->connection()->userInfo()->nickName(),
			c->window()->connection()->userInfo()->userName(),
			c->window()->connection()->userInfo()->hostName(),
			QString::null);

	return true;
}

/*
	@doc: avatar.notify
	@type:
		command
	@title:
		avatar.notify
	@short:
		Notifies the current avatar to a remote target
	@syntax:
		avatar.notify [-q] [-t=<timeout:integer>] <target:string>
	@switches:
		!sw: -q | --quiet
		Do not print warnings
		!sw: -t=<timeout> | --timeout=<integer>
	@description:
		Notifies the current avatar to the remote <target> via CTCP AVATAR.
		See the [doc:ctcp_avatar]avatar protocol documentation[/doc] for the
		description of the protocol.[br]
		This has the effect to notify your avatar image to the <target>.[br]
		The CTCP is sent thru a NOTICE and the current avatar image
		is added to the public offer list for <timeout> seconds (or a default timeout if the -t switch is not used).[br]
		If the -q switch is specified, the command executes in quet mode and
		prints nothing in the current window.[br]
		If you don't have an avatar set, the ctcp will unset the previous avatar
		on the target side.[br]
		[b]Warning:[/b] The implementation of the avatar protocol is actually
		restricted to KVIrc clients only. In the future other clients may implement it.[br]
		This command is [doc:connection_dependant_commands]connection dependant[/doc].[br]
	@examples:
			[example]
			[comment]# Notify your current avatar to Pragma[/comment]
			avatar.notify Pragma
			[comment]# Notify your avatar to the channel #kvirc and to Pragma[/comment]
			avatar #kvirc,Pragma
			[/example]
	@seealso:
		[cmd]avatar.set[/cmd], [cmd]avatar.unset[/cmd]
*/

static bool avatar_kvs_cmd_notify(KviKvsModuleCommandCall * c)
{
	QString szTarget;
	KVSM_PARAMETERS_BEGIN(c)
		KVSM_PARAMETER("target",KVS_PT_NONEMPTYSTRING,0,szTarget)
	KVSM_PARAMETERS_END(c)

	KVSM_REQUIRE_CONNECTION(c)

	kvs_int_t iTimeout = (kvs_int_t)KVI_OPTION_UINT(KviOption_uintAvatarOfferTimeoutInSecs);
	if(KviKvsVariant * pTimeout = c->switches()->find('t',"timeout"))
	{
		if(!pTimeout->asInteger(iTimeout))
		{
			if(!c->switches()->find('q',"quiet"))
				c->warning(__tr2qs("Invalid timeout specified, using default"));
		}
	}

	KviIrcUserEntry * e = c->window()->connection()->userDataBase()->find(c->window()->connection()->currentNickName());
	if(!e)
	{
		c->warning(__tr2qs("Internal error: I'm not in the user database ?"));
		return true;
	}
	
	QString absPath,avatar;
	
	if(e->avatar())
	{
		absPath = e->avatar()->localPath();
		avatar = e->avatar()->name();
	}

	KviSharedFile * o = 0;
	if((!absPath.isEmpty()) && (!avatar.isEmpty()))
	{
		bool bTargetIsChan = (szTarget.contains('#') || szTarget.contains('&') || szTarget.contains('!'));
		if(bTargetIsChan)o = g_pSharedFilesManager->lookupSharedFile(avatar,0);
		else {
			KviIrcMask u(szTarget);
			o = g_pSharedFilesManager->lookupSharedFile(avatar,&u);
		}
		if(!o)
		{
			// FIXME: #warning "OPTION FOR PERMANENT OR TIMEDOUT OFFER...TIMEDOUT WOULD ALSO NEED TO EXTEND EXISTING OFFERS LIFETIME"
			QString szUserMask = bTargetIsChan ? QString("*") : szTarget;
			szUserMask += "!*@*";
			o = g_pSharedFilesManager->addSharedFile(avatar,absPath,szUserMask,iTimeout);
			if(!o)
			{
				// Don't delete o...it has been already deleted by g_pFileTrader
				if(!c->switches()->find('q',"quiet"))
					c->warning(__tr2qs("Can't add a file offer for file %Q (huh ? file not readable ?)"),&absPath);
				return true;
			}

			if(_OUTPUT_VERBOSE)
			{
				if(!c->switches()->find('q',"quiet"))
					c->window()->output(KVI_OUT_SYSTEMMESSAGE,__tr2qs("Added %d secs file offer for file %Q (%Q) and receiver %Q"),
						iTimeout,&(o->absFilePath()),&avatar,&(o->userMask()));
			}
		}
	}

	if(!c->switches()->find('q',"quiet"))
		c->window()->output(KVI_OUT_AVATAR,__tr2qs("Notifying avatar '%Q' to %Q"),&avatar,&szTarget);

	KviQCString encodedTarget = c->window()->connection()->encodeText(szTarget);

	if(!avatar.isEmpty())
	{
		KviQCString encodedAvatar = c->window()->connection()->encodeText(avatar);
		
		if(o)
		{
			c->window()->connection()->sendFmtData("NOTICE %s :%cAVATAR %s %u%c",encodedTarget.data(),0x01,
					encodedAvatar.data(),o->fileSize(),0x01);
		} else {
			c->window()->connection()->sendFmtData("NOTICE %s :%cAVATAR %s%c",encodedTarget.data(),0x01,
					encodedAvatar.data(),0x01);
		}
	} else {
		c->window()->connection()->sendFmtData("NOTICE %s :%cAVATAR%c",encodedTarget.data(),0x01,0x01);
	}

	return true;
}

/*
	@doc: avatar.name
	@type:
		function
	@title:
		$avatar.name
	@short:
		Returns the avatar name for the specified user
	@syntax:
		<string> $avatar.name
		<string> $avatar.name(<nick:string>)
	@description:
		Returns the name of the avatar belonging to <nick> in the current IRC context.
		If <nick> is omitted then the name of the avatar of the current local user
		is returned. The returned string is empty if the specified user has no
		avatar set or there is no such user at all.
	@seealso:
		[fnc]$avatar.path[/fnc]()
*/

static bool avatar_kvs_fnc_name(KviKvsModuleFunctionCall * c)
{ 
	QString szNick;
	KVSM_PARAMETERS_BEGIN(c)
		KVSM_PARAMETER("nick",KVS_PT_STRING,KVS_PF_OPTIONAL,szNick)
	KVSM_PARAMETERS_END(c)
	
	KVSM_REQUIRE_CONNECTION(c)

	if(szNick.isEmpty())szNick = c->window()->connection()->currentNickName();

	KviIrcUserEntry * e = c->window()->connection()->userDataBase()->find(szNick);
	if(e)
	{
		if(e->avatar())
		{
			c->returnValue()->setString(e->avatar()->name());
			return true;
		}
	}
	c->returnValue()->setNothing();
	return true;
}

/*
	@doc: avatar.path
	@type:
		function
	@title:
		$avatar.path
	@short:
		Returns the avatar path for the specified user
	@syntax:
		<string> $avatar.path
		<string> $avatar.path(<nick:string>)
	@description:
		Returns the local path of the avatar belonging to <nick> in the current IRC context.
		If <nick> is omitted then the path of the avatar of the current local user
		is returned. The returned string is empty if the specified user has no
		avatar set or there is no such user at all.
	@seealso:
		[fnc]$avatar.name[/fnc]()
*/

static bool avatar_kvs_fnc_path(KviKvsModuleFunctionCall * c)
{ 
	QString szNick;
	KVSM_PARAMETERS_BEGIN(c)
		KVSM_PARAMETER("path",KVS_PT_STRING,KVS_PF_OPTIONAL,szNick)
	KVSM_PARAMETERS_END(c)
	
	KVSM_REQUIRE_CONNECTION(c)

	if(szNick.isEmpty())szNick = c->window()->connection()->currentNickName();

	KviIrcUserEntry * e = c->window()->connection()->userDataBase()->find(szNick);
	if(e)
	{
		if(e->avatar())
		{
			c->returnValue()->setString(e->avatar()->localPath());
			return true;
		}
	}
	c->returnValue()->setNothing();
	return true;
}

/*
	@doc: avatar.query
	@type:
		command
	@title:
		avatar.query
	@short:
		Queries the avatar of a remote target
	@syntax:
		avatar.query <target:string>
	@description:
		Queries the avatar of a remote target via CTCP AVATAR.
		The <target> can be a channel or a nickname.
		This command is equivalent to "[cmd]ctcp[/cmd] <target> AVATAR".
	@seealso:
		[cmd]avatar.set[/cmd]
*/

static bool avatar_kvs_cmd_query(KviKvsModuleCommandCall * c)
{
	QString szName;
	KVSM_PARAMETERS_BEGIN(c)
		KVSM_PARAMETER("target",KVS_PT_NONEMPTYSTRING,0,szName)
	KVSM_PARAMETERS_END(c)

	KVSM_REQUIRE_CONNECTION(c)

	KviQCString target = c->window()->connection()->encodeText(szName);
	c->window()->connection()->sendFmtData("PRIVMSG %s :%cAVATAR%c",target.data(),0x01,0x01);

	return true;
}

static bool avatar_module_init(KviModule * m)
{
	g_pAvatarSelectionDialogList = new QList<KviAsyncAvatarSelectionDialog*>;

	KVSM_REGISTER_SIMPLE_COMMAND(m,"query",avatar_kvs_cmd_query);
	KVSM_REGISTER_SIMPLE_COMMAND(m,"set",avatar_kvs_cmd_set);
	KVSM_REGISTER_SIMPLE_COMMAND(m,"unset",avatar_kvs_cmd_set);
	KVSM_REGISTER_SIMPLE_COMMAND(m,"notify",avatar_kvs_cmd_notify);
	
	KVSM_REGISTER_FUNCTION(m,"name",avatar_kvs_fnc_name);
	KVSM_REGISTER_FUNCTION(m,"path",avatar_kvs_fnc_path);

	return true;
}

static bool avatar_module_can_unload(KviModule *m)
{
	return g_pAvatarSelectionDialogList->isEmpty();
}

static bool avatar_module_cleanup(KviModule *m)
{
	qDeleteAll(*g_pAvatarSelectionDialogList);
	delete g_pAvatarSelectionDialogList;
	return true;
}

KVIRC_MODULE(
	"Avatar",
	"4.0.0",
	"Copyright (C) 2004 Szymon Stefanek (pragma at kvirc dot net)",
	"Avatar manipulation routines",
	avatar_module_init,
	avatar_module_can_unload,
	0,
	avatar_module_cleanup
)
