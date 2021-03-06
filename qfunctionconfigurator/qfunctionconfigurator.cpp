/********************************************************************************
* FaceTrackNoIR		This program is a private project of some enthusiastic		*
*					gamers from Holland, who don't like to pay much for			*
*					head-tracking.												*
*																				*
* Copyright (C) 2012	Wim Vriend (Developing)									*
*						Ron Hendriks (Researching and Testing)					*
*																				*
* Homepage	http://facetracknoir.sourceforge.net/home/default.htm				*
*																				*
* This program is free software; you can redistribute it and/or modify it		*
* under the terms of the GNU General Public License as published by the			*
* Free Software Foundation; either version 3 of the License, or (at your		*
* option) any later version.													*
*																				*
* This program is distributed in the hope that it will be useful, but			*
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY	*
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for	*
* more details.																	*
*																				*
* You should have received a copy of the GNU General Public License along		*
* with this program; if not, see <http://www.gnu.org/licenses/>.				*
*																				*
* The FunctionConfigurator was made by Stanislaw Halik, and adapted to          *
* FaceTrackNoIR.																*
*																				*
* All credits for this nice piece of code should go to Stanislaw.				*
*																				*
* Copyright (c) 2011-2012, Stanislaw Halik <sthalik@misaki.pl>					*
* Permission to use, copy, modify, and/or distribute this						*
* software for any purpose with or without fee is hereby granted,				*
* provided that the above copyright notice and this permission					*
* notice appear in all copies.													*
********************************************************************************/
#include "qfunctionconfigurator/qfunctionconfigurator.h"
#include <QPainter>
#include <QPaintEvent>
#include <QPainterPathStroker>
#include <QPainterPath>
#include <QBrush>
#include <QFileDialog>
#include <QPen>
#include <QMessageBox>
#include <QImage>
#include <QPixmap>
#include <QTimer>

#include <QtDebug>

#include <math.h>

static const int pointSize = 5;

QFunctionConfigurator::QFunctionConfigurator(QWidget *parent)
    : QWidget(parent)
{

    //
    // Defaults, for when the widget has no values different from the domXML()
    //
    MaxInput = 50;					// Maximum input limit
    MaxOutput = 180;				// Maximum output limit
    pPerEGU_Output = 1;				// Number of pixels, per EGU
    pPerEGU_Input = 4;				// Number of pixels, per EGU
    gDistEGU_Input = 5;				// Distance of gridlines
    gDistEGU_Output = 10;			// Distance of gridlines


    // Change compared to BezierConfigurator: X = horizontal (input), Y = vertical (output)
    // This will require the Curve-Dialog to be higher (which was the reason it was reversed in the first place..)
    range = QRectF(40, 20, MaxInput * pPerEGU_Input, MaxOutput * pPerEGU_Output);

    setMouseTracking(true);
    movingPoint = -1;				// Index of that same point

    //
    // Variables for FunctionConfig
    //
    _config = 0;
    _draw_background = true;
    _draw_function = true;

//	qDebug() << "QFunctionConfigurator::QFunctionConfigurator object created.";

}

//
// Attach an existing FunctionConfig to the Widget.
//
void QFunctionConfigurator::setConfig(FunctionConfig* config, QString settingsFile) {
    QSettings settings("opentrack");	// Registry settings (in HK_USER)
    QString currentFile = settings.value ( "SettingsFile", QCoreApplication::applicationDirPath() + "/settings/default.ini" ).toString();
    QSettings iniFile( currentFile, QSettings::IniFormat );		// Application settings (in INI-file)
    config->loadSettings(iniFile);

    _config = config;

    strSettingsFile = settingsFile;													// Remember for Reset()

    qDebug() << "QFunctionConfigurator::setConfig" << config->getTitle();
    setCaption(config->getTitle());

    _draw_function = _draw_background = true;
    this->update();
}

//
// Load the FunctionConfig (points) from the INI-file.
//
void QFunctionConfigurator::loadSettings(QString settingsFile) {

    QSettings iniFile( settingsFile, QSettings::IniFormat );						// Application settings (in INI-file)
    strSettingsFile = settingsFile;													// Remember for Reset()
    qDebug() << "QFunctionConfigurator::loadSettings = " << settingsFile;
    if (_config) {
        _config->loadSettings(iniFile);
    }
}

//
// Save the FunctionConfig (points) to the INI-file.
//
void QFunctionConfigurator::saveSettings(QString settingsFile) {
    QSettings iniFile( settingsFile, QSettings::IniFormat );						// Application settings (in INI-file)
    strSettingsFile = settingsFile;													// Remember for Reset()
    qDebug() << "QFunctionConfigurator::saveSettings = " << settingsFile;

    if (_config) {
        _config->saveSettings(iniFile);
    }
}

//
// Draw the Background for the graph, the gridlines and the gridpoints.
// The static objects are drawn on a Pixmap, so it does not have to be repeated every paintEvent. Hope this speeds things up...
//
void QFunctionConfigurator::drawBackground(const QRectF &fullRect)
{
    int i;
    QRect scale;

    _background = QPixmap(fullRect.width(), fullRect.height());
    QPainter painter(&_background);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(fullRect, colBackground);
    QColor bg_color(112, 154, 209);
    painter.fillRect(range, bg_color);

    QFont font("ComicSans", 4);
    font.setPointSize(8);
    painter.setFont(font);

    QPen pen(QColor(55, 104, 170, 127), 1, Qt::SolidLine);

    //
    // Draw the Caption
    //
    if (_config) {
        strCaption = _config->getTitle();
    }

    scale.setCoords(range.left(), 0, range.right(), 20);
    painter.drawText(scale, Qt::AlignCenter, strCaption);

    //
    // Draw the horizontal grid
    //
    for (i = range.bottom() - gDistEGU_Output * pPerEGU_Output; i >= range.top(); i -= gDistEGU_Output * pPerEGU_Output) {
        drawLine(&painter, QPointF(40, i), QPointF(range.right(), i), pen);
        scale.setCoords(0, i - 5, range.left() - 5, i + 5);
        painter.drawText(scale, Qt::AlignRight, tr("%1").arg(((range.bottom() - i))/pPerEGU_Output));
    }

    //
    // Draw the vertical guidelines
    //
    for (i = range.left(); i <= range.right(); i += gDistEGU_Input * pPerEGU_Input) {
        drawLine(&painter, QPointF(i, range.top()), QPointF(i, range.bottom()), pen);
        scale.setCoords(i - 10, range.bottom() + 2, i + 10, range.bottom() + 15);
        painter.drawText(scale, Qt::AlignCenter, tr("%1").arg(abs(((range.left() - i))/pPerEGU_Input)));
    }

    scale.setCoords(range.left(), range.bottom() + 20, range.right(), range.bottom() + 35);
    painter.drawText(scale, Qt::AlignRight, strInputEGU);

    //
    // Draw the EGU of the vertical axis (vertically!)
    //
    font.setPointSize(10);
    painter.translate(range.topLeft().x() - 35, range.topLeft().y());
    painter.rotate(90);
    painter.drawText(0,0,strOutputEGU );

    //
    // Draw the two axis
    //
    pen.setWidth(2);
    pen.setColor( Qt::black );
    drawLine(&painter, range.topLeft() - QPointF(2,0), range.bottomLeft() - QPointF(2,0), pen);
    drawLine(&painter, range.bottomLeft(), range.bottomRight(), pen);

    painter.restore();
}


//
// Draw the Function for the graph, on a Pixmap.
//
void QFunctionConfigurator::drawFunction()
{
    if (!_config)
        return;
int i;
QPointF prevPoint;
QPointF currentPoint;

    //
    // Use the background picture to draw on.
    // ToDo: find out how to add Pixmaps, without getting it all green...
    //
    _function = QPixmap(_background);
    QPainter painter(&_function);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    //
    // Draw the handles for the Points
    //

    QList<QPointF> points = _config->getPoints();

    for (i = 0; i < points.size(); i++) {
        currentPoint = graphicalizePoint( points[i] );		// Get the next point and convert it to Widget measures
        drawPoint(&painter, currentPoint, QColor(200, 200, 210, 120));
        lastPoint = currentPoint;											// Remember which point is the rightmost in the graph
    }


    QPen pen(colBezier, 1.2, Qt::SolidLine);

    prevPoint = graphicalizePoint( QPointF(0,0) );		// Start at the Axis
    double max = maxInputEGU();
    QPointF prev = graphicalizePoint(QPointF(0, 0));
    const double step = 1.01;
    for (double i = 0; i < max; i += step) {
        double val = _config->getValue(i);
        QPointF cur = graphicalizePoint(QPointF(i, val));
        drawLine(&painter, prev, cur, pen);
        prev = cur;
    }
    painter.restore();
}

//
// The Widget paints the surface every x msecs.
//
void QFunctionConfigurator::paintEvent(QPaintEvent *e)
{
QPointF prevPoint;
QPointF currentPoint;
QPointF actualPos;
int i;

//	qDebug() << "QFunctionConfigurator::paintEvent.";

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setClipRect(e->rect());

    if (_draw_background) {
        drawBackground(e->rect());						// Draw the static parts on a Pixmap
        p.drawPixmap(0, 0, _background);				// Paint the background
        _draw_background = false;
    }

    if (_draw_function) {
        drawFunction();						// Draw the Function on a Pixmap
        _draw_function = false;
    }
    p.drawPixmap(0, 0, _function);						// Always draw the background and the function

    QPen pen(Qt::white, 1, Qt::SolidLine);

    //
    // Draw the Points, that make up the Curve
    //
    if (_config) {
        QList<QPointF> points = _config->getPoints();
        //
        // When moving, also draw a sketched version of the Function.
        //
        if (movingPoint >= 0 && movingPoint < points.size()) {
            prevPoint = graphicalizePoint( QPointF(0,0) );				// Start at the Axis
            for (i = 0; i < points.size(); i++) {
                currentPoint = graphicalizePoint( points[i] );		// Get the next point and convert it to Widget measures
                drawLine(&p, prevPoint, currentPoint, pen);
                prevPoint = currentPoint;
//	qDebug() << "QFunctionConfigurator::paintEvent, drawing while moving " << currentPoint;
            }

            //
            // When moving, also draw a few help-lines, so positioning the point gets easier.
            //
            pen.setWidth(1);
            pen.setColor( Qt::white );
            pen.setStyle( Qt::DashLine );
            actualPos = graphicalizePoint(points[movingPoint]);
            drawLine(&p, QPoint(range.left(), actualPos.y()), QPoint(actualPos.x(), actualPos.y()), pen);
            drawLine(&p, QPoint(actualPos.x(), actualPos.y()), QPoint(actualPos.x(), range.bottom()), pen);
        }

        //
        // If the Tracker is active, the 'Last Point' it requested is recorded.
        // Show that point on the graph, with some lines to assist.
        // This new feature is very handy for tweaking the curves!
        //
        if (_config->getLastPoint( currentPoint )) {

//	qDebug() << "QFunctionConfigurator::paintEvent, drawing tracked Point " << currentPoint;

            actualPos = graphicalizePoint( currentPoint );
            drawPoint(&p, actualPos, QColor(255, 0, 0, 120));

            pen.setWidth(1);
            pen.setColor( Qt::black );
            pen.setStyle( Qt::SolidLine );
            drawLine(&p, QPoint(range.left(), actualPos.y()), QPoint(actualPos.x(), actualPos.y()), pen);
            drawLine(&p, QPoint(actualPos.x(), actualPos.y()), QPoint(actualPos.x(), range.bottom()), pen);
        }

    }

    //
    // Draw the delimiters
    //
    pen.setWidth(1);
    pen.setColor( Qt::white );
    pen.setStyle( Qt::SolidLine );
    drawLine(&p, QPoint(lastPoint.x(), range.top()), QPoint(lastPoint.x(), range.bottom()), pen);
    drawLine(&p, QPoint(range.left(), lastPoint.y()), QPoint(range.right(), lastPoint.y()), pen);

    //QTimer::singleShot(50, this, SLOT(update()));
}

//
// Draw the handle, to move the Bezier-curve.
//
void QFunctionConfigurator::drawPoint(QPainter *painter, const QPointF &pos, QColor colBG )
{
    painter->save();
    painter->setPen(QColor(50, 100, 120, 200));
    painter->setBrush( colBG );
    painter->drawEllipse(QRectF(pos.x() - pointSize,
                                pos.y() - pointSize,
                                pointSize*2, pointSize*2));
    painter->restore();
}

void QFunctionConfigurator::drawLine(QPainter *painter, const QPointF &start, const QPointF &end, QPen pen)
{
    painter->save();
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawLine(start, end);
    painter->restore();
}

//
// If the mousebutton is pressed, check if it is inside one of the Points.
// If so: start moving that Point, until mouse release.
//
void QFunctionConfigurator::mousePressEvent(QMouseEvent *e)
{
    QList<QPointF> points = _config->getPoints();

    //
    // First: check the left mouse-button
    //
    if (e->button() == Qt::LeftButton) {

        //
        // Check to see if the cursor is touching one of the points.
        //
        bool bTouchingPoint = false;
        movingPoint = -1;
        if (_config) {
            for (int i = 0; i < points.size(); i++) {
                if ( markContains( graphicalizePoint( points[i] ), e->pos() ) ) {
                    bTouchingPoint = true;
                    movingPoint = i;
                    timer.restart();
                    break;
                }
            }

            //
            // If the Left Mouse-button was clicked without touching a Point, add a new Point
            //
            if (!bTouchingPoint) {
                if (withinRect(e->pos(), range)) {
                    _config->addPoint(normalizePoint(e->pos()));
                    emit CurveChanged( true );
                }
            }
        }
    }

    // Then: check the right mouse-button
    //
    if (e->button() == Qt::RightButton) {

        //
        // Check to see if the cursor is touching one of the points.
        //
        if (_config) {
            int found_pt = -1;
            for (int i = 0; i < points.size(); i++) {
                if ( markContains( graphicalizePoint( points[i] ), e->pos() ) ) {
                    found_pt = i;
                    break;
                }
            }

            //
            // If the Right Mouse-button was clicked while touching a Point, remove the Point
            //
            if (found_pt != -1) {
                _config->removePoint(found_pt);
                emit CurveChanged( true );
            }
            movingPoint = -1;
        }
    }

    _draw_function = _draw_background = true;
    update();
}

//
// If the mouse if moving, make sure the Bezier moves along.
// Of course, only when a Point is selected...
//
void QFunctionConfigurator::mouseMoveEvent(QMouseEvent *e)
{
    QList<QPointF> points = _config->getPoints();
    const int refresh_delay = 50;

    if (movingPoint >= 0 && movingPoint < points.size()) {
        setCursor(Qt::ClosedHandCursor);

        if (timer.isValid() && timer.elapsed() > refresh_delay)
        {
            timer.restart();
            QPointF new_pt = normalizePoint(e->pos());
            points[movingPoint] = new_pt;
            _config->movePoint(movingPoint, new_pt);
            _draw_function = _draw_background = true;
            update();
        }
    }
    else {
        if (withinRect(e->pos(), rect()))
        {
            //
            // Check to see if the cursor is touching one of the points.
            //
            bool bTouchingPoint = false;
            if (_config) {
                for (int i = 0; i < points.size(); i++) {
                    if ( markContains( graphicalizePoint( points[i] ), e->pos() ) ) {
                        bTouchingPoint = true;
                    }
                }
            }

            if ( bTouchingPoint ) {
                setCursor(Qt::OpenHandCursor);
            }
            else {
                setCursor(Qt::ArrowCursor);
            }
        }
    }
}

void QFunctionConfigurator::mouseReleaseEvent(QMouseEvent *e)
{
    QList<QPointF> points = _config->getPoints();

    if (e->button() == Qt::LeftButton) {
        timer.invalidate();
        //qDebug()<<"releasing";
        if (movingPoint >= 0 && movingPoint < points.size()) {
            emit CurveChanged( true );

            //
            // Update the Point in the _config
            //
            if (_config) {
                _config->movePoint(movingPoint, normalizePoint(e->pos()));
            }
        }
        setCursor(Qt::ArrowCursor);
        movingPoint = -1;
    }

    _draw_function = _draw_background = true;
    update();
}

//
// Determine if the mousebutton was pressed within the range of the Point.
//
bool QFunctionConfigurator::markContains(const QPointF &pos, const QPointF &coord) const
{
    QRectF rect(pos.x() - pointSize,
                pos.y() - pointSize,
                pointSize*2, pointSize*2);
    QPainterPath path;
    path.addEllipse(rect);
    return path.contains(coord);
}

bool QFunctionConfigurator::withinRect( const QPointF &coord, const QRectF &rect ) const
{
    QPainterPath path;
    path.addRect(rect);
    return path.contains(coord);
}

//
// Convert the Point in the graph, to the real-life Point.
//
QPointF QFunctionConfigurator::normalizePoint(QPointF point) const
{
    QPointF norm;

    norm.setX( (point.x() - range.left()) / pPerEGU_Input );
    norm.setY( (range.bottom() - point.y()) / pPerEGU_Output );

    if (norm.x() > maxInputEGU())
        norm.setX(maxInputEGU());
    else if (norm.x() < 0)
        norm.setX(0);
    if (norm.y() > maxOutputEGU())
        norm.setY(maxOutputEGU());
    else if (norm.y() < 0)
        norm.setY(0);

    return norm;
}

//
// Convert the real-life Point into the graphical Point.
//
QPointF QFunctionConfigurator::graphicalizePoint(QPointF point) const
{
QPointF graph;

    graph.setX( range.left() + (fabs(point.x()) * pPerEGU_Input) );
    graph.setY( range.bottom() - (fabs(point.y()) * pPerEGU_Output) );

    return graph;
}

void QFunctionConfigurator::setmaxInputEGU(int value)
{
    MaxInput = value;
    setMinimumWidth(MaxInput * pPerEGU_Input + 55);
//	resetCurve();
    resize( MaxInput * pPerEGU_Input + 55, MaxOutput * pPerEGU_Output + 60 );
}
void QFunctionConfigurator::setmaxOutputEGU(int value)
{
    MaxOutput = value;
    setMinimumHeight(MaxOutput * pPerEGU_Output + 60);
//	resetCurve();
    resize( MaxInput * pPerEGU_Input + 55, MaxOutput * pPerEGU_Output + 60 );
}

//
// To make configuration more visibly attractive, the number of pixels 'per EGU' can be defined.
//
void QFunctionConfigurator::setpixPerEGU_Input(int value)
{
    pPerEGU_Input = value;
    setMinimumWidth(MaxInput * pPerEGU_Input + 55);
    resize( MaxInput * pPerEGU_Input + 55, MaxOutput * pPerEGU_Output + 60 );
}

//
// To make configuration more visibly attractive, the number of pixels 'per EGU' can be defined.
//
void QFunctionConfigurator::setpixPerEGU_Output(int value)
{
    pPerEGU_Output = value;
    setMinimumHeight(MaxOutput * pPerEGU_Output + 60);
    resize( MaxInput * pPerEGU_Input + 55, MaxOutput * pPerEGU_Output + 60 );
}

//
// Define the distance of the grid 'in EGU' points.
//
void QFunctionConfigurator::setgridDistEGU_Input(int value)
{
    gDistEGU_Input = value;
    _draw_background = true;
    _draw_function = true;
    repaint();
}

//
// Define the distance of the grid 'in EGU' points.
//
void QFunctionConfigurator::setgridDistEGU_Output(int value)
{
    gDistEGU_Output = value;
    _draw_background = true;
    _draw_function = true;
    repaint();
}

void QFunctionConfigurator::setColorBezier(QColor color)
{
    colBezier = color;
    update();
}
void QFunctionConfigurator::setColorBackground(QColor color)
{
    colBackground = color;
    update();
}

void QFunctionConfigurator::setInputEGU(QString egu)
{
    strInputEGU = egu;
    update();
}
void QFunctionConfigurator::setOutputEGU(QString egu)
{
    strOutputEGU = egu;
    update();
}
void QFunctionConfigurator::setCaption(QString cap)
{
    strCaption = cap;
    update();
}

void QFunctionConfigurator::resizeEvent(QResizeEvent *)
{
    range = QRectF(40, 20, MaxInput * pPerEGU_Input, MaxOutput * pPerEGU_Output);

    qDebug() << "QFunctionConfigurator::resizeEvent, name = " << strCaption << ",range = " << range;

    _draw_background = true;
    _draw_function = true;
    repaint();
}
