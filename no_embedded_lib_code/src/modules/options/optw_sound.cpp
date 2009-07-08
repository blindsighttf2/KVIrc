//=============================================================================
//
//   File : optw_sound.cpp
//   Creation date : Fri Sep  6 02:18:23 2002 GMT by Szymon Stefanek
//
//   This file is part of the KVsound sound client distribution
//   Copyright (C) 2002-2008 Szymon Stefanek (pragma at kvsound dot net)
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
//   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
//=============================================================================

#include "optw_sound.h"

#include "kvi_settings.h"
#include "kvi_locale.h"
#include "kvi_options.h"
#include "kvi_modulemanager.h"
#include "kvi_pointerlist.h"
#include "kvi_string.h"
#include "kvi_tal_hbox.h"
#include "kvi_tal_tooltip.h"

#include <QLabel>
#include <QPushButton>


// FIXME: This module doesn't Cancel properly when auto-detection is performed!



KviSoundOptionsWidget::KviSoundOptionsWidget(QWidget * pParent)
: KviOptionsWidget(pParent)
{
}

KviSoundOptionsWidget::~KviSoundOptionsWidget()
{
}


KviSoundGeneralOptionsWidget::KviSoundGeneralOptionsWidget(QWidget * parent)
: KviOptionsWidget(parent)
{
	setObjectName("sound_system_options_widget");
	createLayout();

	KviTalGroupBox * g = addGroupBox(0,0,0,0,Qt::Horizontal,__tr2qs_ctx("Sound System","options"),true);

	KviTalToolTip::add(g,__tr2qs_ctx("This allows you to select the sound system to be used with KVIrc.","options"));

	KviTalHBox * h = new KviTalHBox(g);

	m_pSoundSystemBox = new QComboBox(h);

	m_pSoundAutoDetectButton = new QPushButton(__tr2qs_ctx("Auto-detect","options"),h);
	connect(m_pSoundAutoDetectButton,SIGNAL(clicked()),this,SLOT(soundAutoDetect()));

	m_pSoundTestButton = new QPushButton(__tr2qs_ctx("Test","options"),h);
	connect(m_pSoundTestButton,SIGNAL(clicked()),this,SLOT(soundTest()));

	g = addGroupBox(0,1,0,1,Qt::Horizontal,__tr2qs_ctx("Media Player","options"),true);

	KviTalToolTip::add(g,__tr2qs_ctx("This allows you to select the preferred media player to be used with " \
		"the mediaplayer.* module commands and functions.","options"));

	h = new KviTalHBox(g);

	m_pMediaPlayerBox = new QComboBox(h);

	m_pMediaAutoDetectButton = new QPushButton(__tr2qs_ctx("Auto-detect","options"),h);
	connect(m_pMediaAutoDetectButton,SIGNAL(clicked()),this,SLOT(mediaAutoDetect()));

	m_pMediaTestButton = new QPushButton(__tr2qs_ctx("Test","options"),h);
	connect(m_pMediaTestButton,SIGNAL(clicked()),this,SLOT(mediaTest()));

	soundFillBox();
	mediaFillBox();
	// FIXME!
	m_pSoundTestButton->setEnabled(false);
	m_pMediaTestButton->setEnabled(false);

	g = addGroupBox(0,2,0,2,Qt::Horizontal,__tr2qs_ctx("ID3 tags' encoding","options"),true);

	KviTalToolTip::add(g,__tr2qs_ctx("This allows you to select encoding of mp3 tags.","options"));

	h = new KviTalHBox(g);

	m_pTagsEncodingCombo = new QComboBox(h);
	m_pTagsEncodingCombo->addItem(__tr2qs_ctx("Use Language Encoding","options"));

	int i = 0;
	int iMatch = 0;
	
	KviLocale::EncodingDescription * d = KviLocale::encodingDescription(i);
	while(d->szName)
	{
		if(KviQString::equalCI(d->szName,KVI_OPTION_STRING(KviOption_stringMp3TagsEncoding)))iMatch = i + 1;
		m_pTagsEncodingCombo->insertItem(m_pTagsEncodingCombo->count(),d->szName);
		i++;
		d = KviLocale::encodingDescription(i);
	}
	m_pTagsEncodingCombo->setCurrentIndex(iMatch);

#if defined(COMPILE_ON_WINDOWS) || defined(COMPILE_ON_MINGW)
	g = addGroupBox(0,3,0,3,Qt::Horizontal,__tr2qs_ctx("Winamp messages ecoding","options"),true);

	KviTalToolTip::add(g,__tr2qs_ctx("This allows you to select encoding of winamp messages.","options"));

	h = new KviTalHBox(g);

	m_pWinampEncodingCombo = new QComboBox(h);
	
	m_pWinampEncodingCombo->addItem(__tr2qs_ctx("Use Language Encoding","options"));
	i = 0;
	iMatch = 0;
	
	d = KviLocale::encodingDescription(i);
	while(d->szName)
	{
		if(KviQString::equalCI(d->szName,KVI_OPTION_STRING(KviOption_stringWinampTextEncoding)))iMatch = i + 1;
		m_pWinampEncodingCombo->insertItem(m_pWinampEncodingCombo->count(),d->szName);
		i++;
		d = KviLocale::encodingDescription(i);
	}
	m_pWinampEncodingCombo->setCurrentIndex(iMatch);

	addRowSpacer(0,4,0,4);
#else
	addRowSpacer(0,3,0,3);
#endif
}


KviSoundGeneralOptionsWidget::~KviSoundGeneralOptionsWidget()
{
}

void KviSoundGeneralOptionsWidget::soundTest()
{
}

void KviSoundGeneralOptionsWidget::mediaTest()
{
}

void KviSoundGeneralOptionsWidget::soundAutoDetect()
{
	KviModule * m = g_pModuleManager->getModule("snd");
	if(!m)return;
	m->ctrl("detectSoundSystem",0);
	soundFillBox();
}

void KviSoundGeneralOptionsWidget::mediaAutoDetect()
{
	KviModule * m = g_pModuleManager->getModule("mediaplayer");
	if(!m)return;
	m->ctrl("detectMediaPlayer",0);
	mediaFillBox();
}

void KviSoundGeneralOptionsWidget::soundFillBox()
{
	QStringList l;
	QStringList::Iterator it;
	int cnt;
	int i;
	KviModule * m = g_pModuleManager->getModule("snd");


	if(!m)goto disable;


	if(!m->ctrl("getAvailableSoundSystems",&l))goto disable;

	m_pSoundSystemBox->clear();

	for ( it = l.begin(); it != l.end(); ++it )
	{
		m_pSoundSystemBox->addItem(*it);
	}

	cnt = m_pSoundSystemBox->count();

	for(i=0;i<cnt;i++)
	{
		QString t = m_pSoundSystemBox->itemText(i);
		if(KviQString::equalCI(t,KVI_OPTION_STRING(KviOption_stringSoundSystem)))
		{
			m_pSoundSystemBox->setCurrentIndex(i);
			break;
		}
	}

	return;
disable:
	m_pSoundSystemBox->clear();
	m_pSoundSystemBox->setEnabled(false);
	m_pSoundTestButton->setEnabled(false);
	m_pSoundAutoDetectButton->setEnabled(false);
}

void KviSoundGeneralOptionsWidget::mediaFillBox()
{
	QStringList l;
	QStringList::Iterator it;
	int cnt;
	int i;
	KviModule * m = g_pModuleManager->getModule("mediaplayer");

	if(!m)goto disable;
	if(!m->ctrl("getAvailableMediaPlayers",&l))goto disable;
	m_pMediaPlayerBox->clear();
	for ( it = l.begin(); it != l.end(); ++it )
	{
		m_pMediaPlayerBox->addItem(*it);
	}
	cnt = m_pMediaPlayerBox->count();

	for(i=0;i<cnt;i++)
	{
		QString t = m_pMediaPlayerBox->itemText(i);
		if(KviQString::equalCI(t,KVI_OPTION_STRING(KviOption_stringPreferredMediaPlayer)))
		{
			m_pMediaPlayerBox->setCurrentIndex(i);
			break;
		}
	}

	return;
disable:
	m_pMediaPlayerBox->clear();
	m_pMediaPlayerBox->setEnabled(false);
	m_pMediaTestButton->setEnabled(false);
	m_pMediaAutoDetectButton->setEnabled(false);
}

void KviSoundGeneralOptionsWidget::commit()
{
	KviOptionsWidget::commit();
	KVI_OPTION_STRING(KviOption_stringSoundSystem) = m_pSoundSystemBox->currentText();
	KVI_OPTION_STRING(KviOption_stringPreferredMediaPlayer) = m_pMediaPlayerBox->currentText();
	
	int idx = m_pTagsEncodingCombo->currentIndex();
	if(idx <= 0)
	{
		// guess from locale
		KVI_OPTION_STRING(KviOption_stringMp3TagsEncoding) = "";
	} else {
		KVI_OPTION_STRING(KviOption_stringMp3TagsEncoding) = m_pTagsEncodingCombo->itemText(idx);
	}


#if defined(COMPILE_ON_WINDOWS) || defined(COMPILE_ON_MINGW)
	idx = m_pWinampEncodingCombo->currentIndex();
	if(idx <= 0)
	{
		// guess from locale
		KVI_OPTION_STRING(KviOption_stringWinampTextEncoding) = "";
	} else {
		KVI_OPTION_STRING(KviOption_stringWinampTextEncoding) = m_pWinampEncodingCombo->itemText(idx);
	}
#endif
}

KviSoundsOptionsWidget::KviSoundsOptionsWidget(QWidget * parent)
: KviOptionsWidget(parent)
{
	setObjectName("sound_options_widget");
	createLayout();
	addLabel(0,0,0,0,__tr2qs_ctx("New Query opened","options"));
	addSoundSelector(1,0,1,0,"",KviOption_stringOnNewQueryOpenedSound);
	
	addLabel(0,1,0,1,__tr2qs_ctx("New message in inactive query","options"));
	addSoundSelector(1,1,1,1,"",KviOption_stringOnQueryMessageSound);
	
	addLabel(0,2,0,2,__tr2qs_ctx("Highlighted message in inactive window","options"));
	addSoundSelector(1,2,1,2,"",KviOption_stringOnHighlightedMessageSound);
	
	addLabel(0,3,0,3,__tr2qs_ctx("Me have been kicked","options"));
	addSoundSelector(1,3,1,3,"",KviOption_stringOnMeKickedSound);
	
	addRowSpacer(0,4,1,4);
	
}

KviSoundsOptionsWidget::~KviSoundsOptionsWidget()
{
}

#ifndef COMPILE_USE_STANDALONE_MOC_SOURCES
#include "m_optw_sound.moc"
#endif //!COMPILE_USE_STANDALONE_MOC_SOURCES
