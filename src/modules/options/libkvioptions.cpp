//============================================================================================
//
//   File : libkvioptions.cpp
//   Creation date : Fri Aug 18 2000 15:04:09 by Szymon Stefanek
//
//   This file is part of the KVirc irc client distribution
//   Copyright (C) 1999-2006 Szymon Stefanek (pragma at kvirc dot net)
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
//============================================================================================

#include "kvi_module.h"
#include "kvi_options.h"
#include "kvi_app.h"
#include "kvi_frame.h"
#include "kvi_window.h"
#include "kvi_locale.h"
#include "kvi_mirccntrl.h"
#include "kvi_out.h"
#include "kvi_splash.h"

#include "container.h"
#include "instances.h"

#include "dialog.h"

#include <qsplitter.h>



QHash<QString,KviOptionsDialog*> * g_pOptionsDialogDict = 0;

/*
	@doc: options.save
	@type:
		command
	@title:
		options.save
	@short:
		Saves the options to disk
	@syntax:
		options.save
	@description:
		Saves the options to disk; this includes aliases , popups ,
		events and user preferences.
*/

static bool options_kvs_cmd_save(KviKvsModuleCommandCall * c)
{
	g_pApp->saveOptions();
	return true;
}


/*
	@doc: options.dialog
	@type:
		command
	@title:
		options.dialog
	@short:
		Shows the "options dialog"
	@syntax:
		options.dialog [-t] [options_group]
	@description:
		Shows the KVIrc options dialog for the specified options group.
		If the [-t] switch is used , the dialog is opened as toplevel window,
		otherwise it is opened as part of the current frame window.[br]
		Valid values for [options_group] are "general" and "theme".
		If [options_group] is omitted, the option group "general" is assumed.
		This command is exported by the "options" module.
	@seealso:
		[fnc]$options.isdialog[/fnc]
*/


static bool options_kvs_cmd_dialog(KviKvsModuleCommandCall * c)
{
	QString szGroup;
	KVSM_PARAMETERS_BEGIN(c)
		KVSM_PARAMETER("options_group",KVS_PT_STRING,KVS_PF_OPTIONAL,szGroup)
	KVSM_PARAMETERS_END(c)
	if(szGroup.isEmpty())szGroup = "general";
	KviOptionsDialog * d = g_pOptionsDialogDict->value(szGroup);
	if(d)
	{
		if(c->hasSwitch('t',"toplevel"))
		{
			if(d->parent())
			{
				d->reparent(0,QPoint(0,0),true);
			}
		} else {
			if(d->parent() != c->window()->frame()->splitter())
			{
				d->reparent(c->window()->frame()->splitter(),QPoint(0,0),true);
			}
		}
	} else {
		if(c->hasSwitch('t',"toplevel"))
		{
			d = new KviOptionsDialog(0,szGroup);
		} else {
			d = new KviOptionsDialog(c->window()->frame()->splitter(),szGroup);
		}
		g_pOptionsDialogDict->insert(szGroup,d);
	}
	d->raise();
	d->show();
	d->setFocus();
	return true;
}

/*
	@doc: options.pages
	@type:
		command
	@title:
		options.pages
	@short:
		Lists the option pages
	@syntax:
		options.pages
	@description:
		Lists the option pages available for editing by the means of [cmd]options.edit[/cmd].
	@seealso:
*/


static void options_kvs_module_print_pages(KviKvsModuleCommandCall * c,KviOptionsPageDescriptorBase * e,const QString& prefix)
{
	c->window()->output(KVI_OUT_SYSTEMMESSAGE,"%Q%c%Q%c",&prefix,KVI_TEXT_BOLD,&(e->name()),KVI_TEXT_BOLD);
	QString szPre = prefix;
	if(szPre.isEmpty()) szPre.append('|');
	szPre.append('-');
	foreach(KviOptionsPageDescriptorBase * ex,*(e->childs()))
	{
		options_kvs_module_print_pages(c,ex,szPre);
	}
}

static bool options_kvs_cmd_pages(KviKvsModuleCommandCall * c)
{
	foreach(KviOptionsPageDescriptorBase * e,KviOptionsManager::instance()->instanceEntryTree())
	{
		options_kvs_module_print_pages(c,e,"");
	}

	return true;
}

/*
	@doc: options.edit
	@type:
		command
	@title:
		options.edit [-m]
	@short:
		Shows a single options page
	@syntax:
		options.edit <"options page class name">
	@description:
		Shows an options page as toplevel dialog.
		The available option pages can be listed by using [cmd]options.pages[/cmd].
		If the -m switch is used, the dialog will appear as modal, blocking input
		to the main frame until it's closed.
	@seealso:
*/

static bool options_kvs_cmd_edit(KviKvsModuleCommandCall * c)
{
	QString szOption;
	KVSM_PARAMETERS_BEGIN(c)
		KVSM_PARAMETER("option",KVS_PT_STRING,0,szOption)
	KVSM_PARAMETERS_END(c)
	KviOptionsPageDescriptorBase* e = KviOptionsManager::instance()->findPage(szOption);
	if(!e)
	{
		c->warning(__tr2qs_ctx("No such options page class name %Q","options"),&szOption);
		return true;
	}

	if(e->instance())
	{
		//c->warning(__tr("The editor page is already open","options"));
		e->instance()->raise();
		e->instance()->setActiveWindow();
		e->instance()->setFocus();
		return true;
	}

	QWidget * w;

	if(c->hasSwitch('m',"modal"))
	{
		w = g_pApp->activeModalWidget();
		if(!w)w = g_pFrame;
	} else {
		w = g_pFrame;
	}

	KviOptionsWidgetContainer * wc = new KviOptionsWidgetContainer(w,c->hasSwitch('m',"modal"));

	e->createInstance(wc);
	wc->setup(e->instance());

	// a trick for the dialog covering the splash screen before the time (this is prolly a WM or Qt bug)
	if(g_pSplashScreen)
	{
		if(g_pSplashScreen->isVisible()) // another bug: this ALWAYS RETURNS TRUE, even if the splash was hidden by a mouse click...
		{
			QObject::connect(g_pSplashScreen,SIGNAL(destroyed()),wc,SLOT(show()));
			return true;
		}
	}

	wc->show();
	wc->raise();
	return true;
}

/*
	@doc: options.isdialog
	@type:
		function
	@title:
		$options.isdialog
	@short:
		Returns the options dialog state
	@syntax:
		<boolean> $options.isdialog([options_group:string])
	@description:
		Returns '1' if the options dialog for the specified options_group is open, '0' otherwise.[br]
		If [options_group] is omitted then the group "general" is assumed.
		At the moment of writing the valid [options_group] values are "general" and "theme".
		This command is exported by the "options" module.
	@seealso:
		[cmd]options.dialog[/cmd]
*/

static bool options_kvs_fnc_isdialog(KviKvsModuleFunctionCall * c)
{ 
	QString szGroup;
	KVSM_PARAMETERS_BEGIN(c)
		KVSM_PARAMETER("options_group",KVS_PT_STRING,KVS_PF_OPTIONAL,szGroup)
	KVSM_PARAMETERS_END(c)
	if(szGroup.isEmpty())szGroup = "general";
	c->returnValue()->setBoolean(g_pOptionsDialogDict->value(szGroup));
	return true;
}


static bool options_module_init(KviModule * m)
{
	new KviOptionsManager();

	KVSM_REGISTER_SIMPLE_COMMAND(m,"dialog",options_kvs_cmd_dialog);
	KVSM_REGISTER_SIMPLE_COMMAND(m,"save",options_kvs_cmd_save);
	KVSM_REGISTER_SIMPLE_COMMAND(m,"pages",options_kvs_cmd_pages);
	KVSM_REGISTER_SIMPLE_COMMAND(m,"edit",options_kvs_cmd_edit);
	KVSM_REGISTER_FUNCTION(m,"isDialog",options_kvs_fnc_isdialog);

	g_pOptionsDialogDict = new QHash<QString,KviOptionsDialog*>;

	return true;
}

static bool options_module_cleanup(KviModule *m)
{
	foreach(KviOptionsDialog * d,*g_pOptionsDialogDict)
	{
		delete d;
	}
	delete g_pOptionsDialogDict;
	g_pOptionsDialogDict = 0;
    
	delete KviOptionsManager::instance();

	return true;
}

static bool options_module_can_unload(KviModule *m)
{
	return g_pOptionsDialogDict->isEmpty();
}

KVIRC_MODULE(
	"Options",                                              // module name
	"1.0.0",                                                // module version
	"Copyright (C) 2000 Szymon Stefanek (pragma at kvirc dot net)", // author & (C)
	"Options Dialog",
	options_module_init,
	options_module_can_unload,
	0,
	options_module_cleanup
)
