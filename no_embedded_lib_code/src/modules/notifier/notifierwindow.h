#ifndef _NOTIFIERWINDOW_H_
#define _NOTIFIERWINDOW_H_
//=============================================================================
//
//   File : notifierwindow.h
//   Creation date : Tue Jul 6 2004 20:25:12 CEST by Szymon Stefanek
//
//   This file is part of the KVirc irc client distribution
//   Copyright (C) 2004 Szymon Stefanek (pragma at kvirc dot net)
//   Copyright (C) 2005-2008 Iacopo Palazzi < iakko(at)siena(dot)linux(dot)it >
//
//   This program is FREE software. You can redistribute it and/or
//   modify it under the linkss of the GNU General Public License
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

#include "notifiersettings.h"

#include "kvi_settings.h"
#include "kvi_qstring.h"
#include "kvi_pointerlist.h"
#include "kvi_tal_popupmenu.h"
#include "kvi_time.h"

#include <QBitmap>
#include <QColor>
#include <QCursor>
#include <QDateTime>
#include <QFont>
#include <QImage>
#include <QLineEdit>
#include <QPixmap>
#include <QRect>
#include <QTimer>
#include <QWidget>
#include <QTabWidget>
#include <QProgressBar>

class QPainter;
class KviWindow;
class KviNotifierMessage;
class KviNotifierWindowBorder;
class KviNotifierWindowTabs;


extern kvi_time_t g_tNotifierDisabledUntil;

class KviNotifierWindow : public QWidget
{
	Q_OBJECT
public:
	KviNotifierWindow();
	~KviNotifierWindow();
protected:
	QTimer * m_pShowHideTimer;
	QTimer * m_pBlinkTimer;
	QTimer * m_pAutoHideTimer;
	State   m_eState;
	bool    m_bBlinkOn;
	double  m_dOpacity;
// 	QImage  m_imgDesktop;            // the desktop screenshot
// 	QPixmap m_pixBackground;         // our background image
// 	QPixmap m_pixBackgroundHighlighted;
// 	QPixmap m_pixForeground;         // we paint the stuff HERE
// 
// 	// Notifier graphic layout
// 	QPixmap m_pixBckgrnd;

	bool    m_bCloseDown;
	bool    m_bPrevDown;
	bool    m_bNextDown;
	bool    m_bWriteDown;
	bool m_bCrashShowWorkAround;

	QFont * m_pDefaultFont;
	QFont * m_pTitleFont;

	QRect	m_wndRect;

	QColor  m_clrCurText;
	QColor  m_clrOldText[NUM_OLD_COLORS];
	QColor  m_clrHistoricText;
	QColor  m_clrTitle;

	KviNotifierMessage * m_pCurrentMessage;
	QLineEdit * m_pLineEdit;

	bool	m_bDragging;
	bool	m_bLeftButtonIsPressed;
	bool	m_bDiagonalResizing;
	bool	m_bResizing;

	int	m_whereResizing;

	QPoint      m_pntDrag;
	QPoint      m_pntPos;
	QPoint      m_pntClick;
	int         m_iInputHeight;
	int         m_iBlinkTimeout;
	int         m_iBlinkCount;
	KviTalPopupMenu     * m_pContextPopup;
	KviTalPopupMenu     * m_pDisablePopup;
	KviWindow * m_pWindowToRaise;
	kvi_time_t  m_tAutoHideAt;
	kvi_time_t  m_tStartedAt;
	QTime	    m_qtStartedAt;
	bool	    m_bDisableHideOnMainWindowGotAttention;
	//bool	    m_bForceShowing;

	QCursor m_cursor;

	QTabWidget 			* m_pWndTabs;
	QProgressBar			* m_pProgressBar;
	KviNotifierWindowBorder 	* m_pWndBorder;
public:
	void doShow(bool bDoAnimate);
	void doHide(bool bDoAnimate);
	const QFont & defaultFont(){ return *m_pDefaultFont; };
	int textWidth();
	void addMessage(KviWindow * pWnd,const QString &szImageId,const QString &szText,unsigned int uMessageTime);
	void setDisableHideOnMainWindowGotAttention(bool b){ m_bDisableHideOnMainWindowGotAttention = b; };
	void showLineEdit(bool bShow);
	inline int countTabs() const { if(m_pWndTabs)return m_pWndTabs->count(); return 0; };
	inline State state() const { return m_eState; };
protected:
	virtual void showEvent(QShowEvent *e);
	virtual void hideEvent(QHideEvent * e);
	virtual void paintEvent(QPaintEvent * e);
	virtual void mousePressEvent(QMouseEvent * e);
	virtual void mouseReleaseEvent(QMouseEvent * e);
	virtual void mouseMoveEvent(QMouseEvent * e);
	virtual void leaveEvent(QEvent * e);
	virtual void enterEvent(QEvent * e);
	virtual bool eventFilter(QObject * pEdit,QEvent * e);
	virtual void keyPressEvent ( QKeyEvent * e );
public slots:
	void hideNow();
	void toggleLineEdit();
protected slots:
	void blink();
	void heartbeat();
	void returnPressed();
	void reloadImages();
	void fillContextPopup();
	void disableFor1Minute();
	void disableFor5Minutes();
	void disableFor15Minutes();
	void disableFor30Minutes();
	void disableFor60Minutes();
	void disableUntilKVIrcRestarted();
	void disablePermanently();
	void progressUpdate();
	void slotTabCloseRequested(int index);
private:
	void contextPopup(const QPoint &pos);
	void startBlinking();
// 	void computeRect();
	void stopShowHideTimer();
	void stopBlinkTimer();
	void stopAutoHideTimer();
	void startAutoHideTimer();
	bool shouldHideIfMainWindowGotAttention();
	void setCursor(int);
	void resize(QPoint p, bool = true);
	void redrawText();
	bool checkResizing(QPoint);
};

#endif //_NOTIFIERWINDOW_H_
