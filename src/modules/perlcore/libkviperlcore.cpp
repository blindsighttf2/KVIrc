//=============================================================================
//
//   File : libkviperlcore.cpp
//   Creation date : Tue Jul 13 13:03:31 2004 GMT by Szymon Stefanek
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
//=============================================================================

#include "kvi_module.h"
#include "kvi_settings.h"
#include "kvi_locale.h"
#include "kvi_out.h"
#include "kvi_window.h"
#include "kvi_app.h"

#include "kvi_kvs_script.h"
#include "kvi_kvs_variant.h"
#include "kvi_userinput.h"
#include "kvi_qcstring.h"


#ifdef DEBUG
	#undef DEBUG
#endif

// I MUST say that the perl embedding process is somewhat ugly :(
// First of all the man pages are somewhat unreadable even
// for a non-novice perl user. The writer of each page assumed
// that you have already read each other page...
// Also browsing the pages with "man" is obviously out of mind
// but this can be solved by looking up some html docs on the net.
// Embedding multiple interpreters isn't that hard (after you
// have read perlembed) but to start passing parameters
// around you have to read at least perlembed, perlguts, perlxs,..
// take a look at the perlinternals and have a good trip
// around the web to find some examples for the functions
// that aren't explained enough in the pages.
// It gets even more weird when you attempt to include
// some XS functions... (what the heck is boot_DynaLoader ?).

// ... and I'm still convinced that I'm leaking memory with
// the perl values, but well ...

// anyway, once you struggled for a couple of days with all that
// stuff then you start getting things done... and it rox :)

#ifdef COMPILE_PERL_SUPPORT
	#include <EXTERN.h>
	#include <perl.h>
	#include <XSUB.h>

	#include "ppport.h"
	
	#include "kvi_kvs_runtimecontext.h"

	static KviKvsRunTimeContext * g_pCurrentKvsContext = 0;
	static bool g_bExecuteQuiet = false;
	static KviStr g_szLastReturnValue("");
	static QStringList g_lWarningList;

	#include "xs.inc"
#endif // COMPILE_PERL_SUPPORT

// perl redefines bool :///
#ifdef bool
	#undef bool
#endif

#ifdef COMPILE_PERL_SUPPORT

#include "perlcoreinterface.h"

// people ... are you mad ? ... what the heck is "my_perl" ?
#define my_perl m_pInterpreter

class KviPerlInterpreter
{
public:
	KviPerlInterpreter(const QString &szContextName);
	~KviPerlInterpreter();
protected:
	QString           m_szContextName;
	PerlInterpreter * m_pInterpreter;
public:
	bool init(); // if this fails then well.. :D
	void done();
	bool execute(const QString &szCode,QStringList &args,QString &szRetVal,QString &szError,QStringList &lWarnings);
	const QString & contextName(){ return m_szContextName; };
protected:
	QString svToQString(SV * sv);
};

KviPerlInterpreter::KviPerlInterpreter(const QString &szContextName)
{
	m_szContextName = szContextName;
	m_pInterpreter = 0;
}

KviPerlInterpreter::~KviPerlInterpreter()
{
	done();
}

// this kinda sux :(
// It SHOULD be mentioned somewhere that
// this function is in DynaLoader.a in the perl
// distribution and you MUST link it statically.
extern "C" void boot_DynaLoader(pTHX_ CV* cv);

extern "C" void xs_init(pTHX)
{
	char *file = __FILE__;
	// boot up the DynaLoader
	newXS("DynaLoader::boot_DynaLoader",boot_DynaLoader,file);
	// now bootstrap the KVIrc module
	// This stuff is simply cutted and pasted from xs.inc
	// since I don't really know if it's safe to call
	// something like
	//   CV * dummy;
	//   boot_KVIrc(aTHX,dummy);
	// ...
	newXS("KVIrc::echo", XS_KVIrc_echo, file);
	newXS("KVIrc::say", XS_KVIrc_say, file);
	newXS("KVIrc::warning", XS_KVIrc_warning, file);
	newXS("KVIrc::getLocal", XS_KVIrc_getLocal, file);
	newXS("KVIrc::setLocal", XS_KVIrc_setLocal, file);
	newXS("KVIrc::getGlobal", XS_KVIrc_getGlobal, file);
	newXS("KVIrc::setGlobal", XS_KVIrc_setGlobal, file);
	newXS("KVIrc::eval", XS_KVIrc_eval, file);
	newXS("KVIrc::internalWarning", XS_KVIrc_internalWarning, file);
}

bool KviPerlInterpreter::init()
{
	if(m_pInterpreter)done();
	m_pInterpreter = perl_alloc();
	if(!m_pInterpreter)return false;
	PERL_SET_CONTEXT(m_pInterpreter);
	PL_perl_destruct_level = 1;
	perl_construct(m_pInterpreter);
	char * daArgs[] = { "yo", "-e", "0", "-w" };
	perl_parse(m_pInterpreter,xs_init,4,daArgs,NULL);
	QString szInitCode;

	// this part of the code seems to be unnecessary
	// even if it is created by the perl make process...
	//	"our %EXPORT_TAGS = ('all' => [qw(echo)]);\n"
	//	"our @EXPORT_OK = (qw(echo));\n"
	//	"our @EXPORT = qw();\n"
	// This is probably needed only if perl has to load
	// the XS through XSLoader ?
	// Maybe also the remaining part of the package
	// declaration could be dropped as well...
	// I just haven't tried :D

	KviQString::sprintf(
		szInitCode,
		"{\n" \
			"package KVIrc;\n" \
			"require Exporter;\n" \
			"our @ISA = qw(Exporter);\n" \
			"1;\n" \
		"}\n" \
		"$g_szContext = \"%Q\";\n" \
		"$g_bExecuteQuiet = 0;\n" \
		"$SIG{__WARN__} = sub\n" \
		"{\n" \
		"	my($p,$f,$l,$x);\n" \
		"	($p,$f,$l) = caller;\n" \
		"	KVIrc::internalWarning(\"At line \".$l.\" of perl code: \");\n" \
		"	KVIrc::internalWarning(join(' ',@_));\n" \
		"}\n",
		&m_szContextName);

	eval_pv(szInitCode.utf8().data(),false);
	return true;
}

void KviPerlInterpreter::done()
{
	if(!m_pInterpreter)return;
	PERL_SET_CONTEXT(m_pInterpreter);
	PL_perl_destruct_level = 1;
	perl_destruct(m_pInterpreter);
	perl_free(m_pInterpreter);
	m_pInterpreter = 0;
}

QString KviPerlInterpreter::svToQString(SV * sv)
{
	QString ret = "";
	if(!sv)return ret;
	STRLEN len;
	char * ptr = SvPV(sv,len);
	if(ptr)ret = ptr;
	return ret;
}

bool KviPerlInterpreter::execute(
		const QString &szCode,
		QStringList &args,
		QString &szRetVal,
		QString &szError,
		QStringList &lWarnings)
{
	if(!m_pInterpreter)
	{
		szError = __tr2qs_ctx("Internal error: perl interpreter not initialized","perlcore");
		return false;
	}
	
	g_lWarningList.clear();

	KviQCString szUtf8 = szCode.utf8();
	PERL_SET_CONTEXT(m_pInterpreter);
	
	// clear the _ array
	AV * pArgs = get_av("_",1);
	SV * pArg = av_shift(pArgs);
	while(SvOK(pArg))
	{
		SvREFCNT_dec(pArg);
		pArg = av_shift(pArgs);
	}

	if(args.count() > 0)
	{
		// set the args in the _ arry
		av_unshift(pArgs,(I32)args.count());
		int idx = 0;
		for(QStringList::Iterator it = args.begin();it != args.end();++it)
		{
			QString tmp = *it;
			const char * val = tmp.utf8().data();
			if(val)
			{
				pArg = newSVpv(val,tmp.length());
				if(!av_store(pArgs,idx,pArg))
					SvREFCNT_dec(pArg);
			}
			idx++;
		}
	}

	// call the code
	SV * pRet = eval_pv(szUtf8.data(),false);

	// clear the _ array again
	pArgs = get_av("_",1);
	pArg = av_shift(pArgs);
	while(SvOK(pArg))
	{
		SvREFCNT_dec(pArg);
		pArg = av_shift(pArgs);
	}
	av_undef(pArgs);

	// get the ret value
	if(pRet)
	{
		if(SvOK(pRet))
			szRetVal = svToQString(pRet);
	}

	if(!g_lWarningList.isEmpty())
		lWarnings = g_lWarningList;

	// and the eventual error string
	pRet = get_sv("@",false);
	if(pRet)
	{
		if(SvOK(pRet))
		{
			szError = svToQString(pRet);
			if(!szError.isEmpty())return false;
		}
	}

	return true;
}

static QHash<QString,KviPerlInterpreter*> * g_pInterpreters = 0;

static KviPerlInterpreter * perlcore_get_interpreter(const QString &szContextName)
{
	KviPerlInterpreter * i = g_pInterpreters->value(szContextName);
	if(i)return i;
	i = new KviPerlInterpreter(szContextName);
	if(!i->init())
	{
		delete i;
		return 0;
	}
	g_pInterpreters->insert(szContextName,i);
	return i;
}

static void perlcore_destroy_interpreter(const QString &szContextName)
{
	KviPerlInterpreter * i = g_pInterpreters->value(szContextName);
	if(!i)return;
	g_pInterpreters->remove(szContextName);
	i->done();
	delete i;
}

static void perlcore_destroy_all_interpreters()
{
	foreach(KviPerlInterpreter * i,*g_pInterpreters)
	{
		i->done();
		delete i;
	}
	g_pInterpreters->clear();
}

#endif // COMPILE_PERL_SUPPORT

static bool perlcore_module_ctrl(KviModule * m,const char * cmd,void * param)
{
#ifdef COMPILE_PERL_SUPPORT
	if(kvi_strEqualCS(cmd,KVI_PERLCORECTRLCOMMAND_EXECUTE))
	{
		KviPerlCoreCtrlCommand_execute * ex = (KviPerlCoreCtrlCommand_execute *)param;
		if(ex->uSize != sizeof(KviPerlCoreCtrlCommand_execute))return false;
		g_pCurrentKvsContext = ex->pKvsContext;
		g_bExecuteQuiet = ex->bQuiet;
		if(ex->szContext.isEmpty())
		{
			KviPerlInterpreter * m = new KviPerlInterpreter("temporary");
			if(!m->init())
			{
				delete m;
				return false;
			}
			ex->bExitOk = m->execute(ex->szCode,ex->lArgs,ex->szRetVal,ex->szError,ex->lWarnings);
			m->done();
			delete m;
		} else {
			KviPerlInterpreter * m = perlcore_get_interpreter(ex->szContext);
			ex->bExitOk = m->execute(ex->szCode,ex->lArgs,ex->szRetVal,ex->szError,ex->lWarnings);
		}
		return true;
	}
	if(kvi_strEqualCS(cmd,KVI_PERLCORECTRLCOMMAND_DESTROY))
	{
		KviPerlCoreCtrlCommand_destroy * de = (KviPerlCoreCtrlCommand_destroy *)param;
		if(de->uSize != sizeof(KviPerlCoreCtrlCommand_destroy))return false;
		perlcore_destroy_interpreter(de->szContext);
		return true;
	}
#endif // COMPILE_PERL_SUPPORT
	return false;
}

static bool perlcore_module_init(KviModule * m)
{
#ifdef COMPILE_PERL_SUPPORT
	g_pInterpreters = new QHash<QString,KviPerlInterpreter*>;
	return true;
#else // !COMPILE_PERL_SUPPORT
	return false;
#endif // !COMPILE_PERL_SUPPORT
}

static bool perlcore_module_cleanup(KviModule *m)
{
#ifdef COMPILE_PERL_SUPPORT
	perlcore_destroy_all_interpreters();
	delete g_pInterpreters;
    g_pInterpreters = 0;
#endif // COMPILE_PERL_SUPPORT
	return true;
}

static bool perlcore_module_can_unload(KviModule *m)
{
#ifdef COMPILE_PERL_SUPPORT
	return (g_pInterpreters->count() == 0);
#endif // COMPILE_PERL_SUPPORT
	return true;
}

KVIRC_MODULE(
	"Perl",                                                 // module name
	"4.0.0",                                                // module version
	"Copyright (C) 2004 Szymon Stefanek (pragma at kvirc dot net)", // author & (C)
	"Perl scripting engine core",
	perlcore_module_init,
	perlcore_module_can_unload,
	perlcore_module_ctrl,
	perlcore_module_cleanup
)
