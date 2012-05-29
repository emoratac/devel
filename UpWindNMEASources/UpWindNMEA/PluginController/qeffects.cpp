/****************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial Usage
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Commercial License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Nokia.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights.  These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qapplication.h"
#ifndef QT_NO_EFFECTS
#include "qdesktopwidget.h"
#include "qeffects_p.h"
#include "qevent.h"
#include "qimage.h"
#include "qpainter.h"
#include "qpixmap.h"
#include "qpointer.h"
#include "qtimer.h"
#include "qelapsedtimer.h"
#include "qdebug.h"

/*
  Internal class to get access to protected QWidget-members
*/

class QAccessWidget : public QWidget
{
    friend class QAlphaWidget;
    friend class QRollEffect;
public:
    QAccessWidget(QWidget* parent=0, Qt::WindowFlags f = 0)
        : QWidget(parent, f) {}
};

/*
  Internal class QAlphaWidget.

  The QAlphaWidget is shown while the animation lasts
  and displays the pixmap resulting from the alpha blending.
*/

class QAlphaWidget: public QWidget, private QEffects
{
    Q_OBJECT
public:
    QAlphaWidget(QWidget* w, Qt::WindowFlags f = 0);
    ~QAlphaWidget();

    void run(int time);
    bool fadeOut;

protected:
    void paintEvent(QPaintEvent* e);
    void closeEvent(QCloseEvent*);
    void alphaBlend();
    bool eventFilter(QObject *, QEvent *);

protected slots:
    void render();

private:
    QPixmap pm;
    double alpha;
    QImage backImage;
    QImage frontImage;
    QImage mixedImage;
    QPointer<QAccessWidget> widget;
    int duration;
    int elapsed;
    bool showWidget;
    QTimer anim;
    QElapsedTimer checkTime;
    double windowOpacity;
};

static QAlphaWidget* q_blend = 0;

/*
  Constructs a QAlphaWidget.
*/
QAlphaWidget::QAlphaWidget(QWidget* w, Qt::WindowFlags f)
    : QWidget(QApplication::desktop()->screen(QApplication::desktop()->screenNumber(w)), f)
{
#ifndef Q_WS_WIN
    setEnabled(false);
#endif
    setAttribute(Qt::WA_NoSystemBackground, true);
    widget = (QAccessWidget*)w;
    windowOpacity = w->windowOpacity();
    alpha = 0;
}

QAlphaWidget::~QAlphaWidget()
{
#if defined(Q_WS_WIN) && !defined(Q_WS_WINCE)
    // Restore user-defined opacity value
    if (widget)
        widget->setWindowOpacity(windowOpacity);
#endif
}

/*
  \reimp
*/
void QAlphaWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.drawPixmap(0, 0, pm);
}

/*
  Starts the alphablending animation.
  The animation will take about \a time ms
*/
void QAlphaWidget::run(int time)
{
    duration = time;

    if (duration < 0)
        duration = 150;

    if (!widget)
        return;

    elapsed = 0;
    checkTime.start();

    showWidget = true;
#if defined(Q_OS_WIN) && !defined(Q_OS_WINCE)
    qApp->installEventFilter(this);
    if(fadeOut)
        widget->setWindowOpacity(1.0);
    else
        widget->setWindowOpacity(0.0);
    widget->show();
    connect(&anim, SIGNAL(timeout()), this, SLOT(render()));
    anim.start(1);
#else
    //This is roughly equivalent to calling setVisible(true) without actually showing the widget
    widget->setAttribute(Qt::WA_WState_ExplicitShowHide, true);
    widget->setAttribute(Qt::WA_WState_Hidden, false);

    qApp->installEventFilter(this);

    move(widget->geometry().x(),widget->geometry().y());
    resize(widget->size().width(), widget->size().height());

    frontImage = QPixmap::grabWidget(widget).toImage();
    backImage = QPixmap::grabWindow(QApplication::desktop()->winId(),
                                widget->geometry().x(), widget->geometry().y(),
                                widget->geometry().width(), widget->geometry().height()).toImage();

    if (!backImage.isNull() && checkTime.elapsed() < duration / 2) {
        mixedImage = backImage.copy();
        pm = QPixmap::fromImage(mixedImage);
        show();
        setEnabled(false);

        connect(&anim, SIGNAL(timeout()), this, SLOT(render()));
        anim.start(1);
    } else {
       duration = 0;
       render();
    }
#endif
}

/*
  \reimp
*/
bool QAlphaWidget::eventFilter(QObject *o, QEvent *e)
{
    switch (e->type()) {
    case QEvent::Move:
	    if (o != widget)
	        break;
	    move(widget->geometry().x(),widget->geometry().y());
	    update();
	    break;
    case QEvent::Hide:
    case QEvent::Close:
	    if (o != widget)
	        break;
    case QEvent::MouseButtonPress:
	case QEvent::MouseButtonDblClick:
	    showWidget = false;
	    render();
	    break;
    case QEvent::KeyPress: {
	        QKeyEvent *ke = (QKeyEvent*)e;
            if (ke->key() == Qt::Key_Escape) {
		        showWidget = false;
            } else {
		        duration = 0;
            }
	        render();
	        break;
	}
    default:
	    break;
    }
    return QWidget::eventFilter(o, e);
}

/*
  \reimp
*/
void QAlphaWidget::closeEvent(QCloseEvent *e)
{
    e->accept();
    if (!q_blend)
        return;

    showWidget = false;
    render();

    QWidget::closeEvent(e);
}

/*
  Render alphablending for the time elapsed.

  Show the blended widget and free all allocated source
  if the blending is finished.
*/
void QAlphaWidget::render()
{
    int tempel = checkTime.elapsed();
    if (elapsed >= tempel)
        elapsed++;
    else
        elapsed = tempel;

    if (duration != 0)
        alpha = tempel / double(duration);
    else
        alpha = 1;

#if defined(Q_OS_WIN) && !defined(Q_OS_WINCE)
    if (alpha >= windowOpacity || !showWidget) {
        anim.stop();
        qApp->removeEventFilter(this);
        if(fadeOut)
            widget->hide();
        widget->setWindowOpacity(windowOpacity);
        q_blend = 0;
        deleteLater();
    } else {
        if(fadeOut)
            widget->setWindowOpacity(1.0-alpha);
        else
            widget->setWindowOpacity(alpha);
    }
#else
    if (alpha >= 1 || !showWidget) {
        anim.stop();
        qApp->removeEventFilter(this);

        if (widget) {
            if (!showWidget) {
#ifdef Q_WS_WIN
                setEnabled(true);
                setFocus();
#endif // Q_WS_WIN
                widget->hide();
            } else {
                //Since we are faking the visibility of the widget 
                //we need to unset the hidden state on it before calling show
                widget->setAttribute(Qt::WA_WState_Hidden, true);
                widget->show();
                lower();
            }
        }
        q_blend = 0;
        deleteLater();
    } else {
        alphaBlend();
        pm = QPixmap::fromImage(mixedImage);
        repaint();
    }
#endif // defined(Q_OS_WIN) && !defined(Q_OS_WINCE)
}

/*
  Calculate an alphablended image.
*/
void QAlphaWidget::alphaBlend()
{
    int a;
    if(fadeOut)
        a = qRound((1.0-alpha)*256);
    else
        a = qRound(alpha*256);
    const int ia = 256 - a;

    const int sw = frontImage.width();
    const int sh = frontImage.height();
    const int bpl = frontImage.bytesPerLine();
    switch(frontImage.depth()) {
    case 32:
        {
            uchar *mixed_data = mixedImage.bits();
            const uchar *back_data = backImage.bits();
            const uchar *front_data = frontImage.bits();

            for (int sy = 0; sy < sh; sy++) {
                quint32* mixed = (quint32*)mixed_data;
                const quint32* back = (const quint32*)back_data;
                const quint32* front = (const quint32*)front_data;
                for (int sx = 0; sx < sw; sx++) {
                    quint32 bp = back[sx];
                    quint32 fp = front[sx];

                    mixed[sx] =  qRgb((qRed(bp)*ia + qRed(fp)*a)>>8,
                                      (qGreen(bp)*ia + qGreen(fp)*a)>>8,
                                      (qBlue(bp)*ia + qBlue(fp)*a)>>8);
                }
                mixed_data += bpl;
                back_data += bpl;
                front_data += bpl;
            }
        }
    default:
        break;
    }
}

/*!
    Fade in widget \a w in \a time ms.
*/
void fadePlugin(QWidget* w, int time, bool out)
{
    if (q_blend) {
        q_blend->deleteLater();
        q_blend = 0;
    }

    if (!w)
        return;

    QApplication::sendPostedEvents(w, QEvent::Move);
    QApplication::sendPostedEvents(w, QEvent::Resize);

    Qt::WindowFlags flags = Qt::ToolTip;

    // those can be popups - they would steal the focus, but are disabled
    q_blend = new QAlphaWidget(w, flags);

    q_blend->fadeOut = out;
    q_blend->run(time);
}

void fadePlugin(QWidget *w, bool out) {

    fadePlugin(w, 450, out);

}

/*
  Delete this after timeout
*/

#include "qeffects.moc"

#endif //QT_NO_EFFECTS
