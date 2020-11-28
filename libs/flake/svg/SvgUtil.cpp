/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2009 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "SvgUtil.h"
#include "SvgGraphicContext.h"

#include <KoUnit.h>
#include <KoXmlReader.h>

#include <QString>
#include <QRectF>
#include <QStringList>
#include <QFontMetrics>
#include <QRegExp>

#include <math.h>
#include "kis_debug.h"
#include "kis_global.h"

#include <KoXmlWriter.h>
#include "kis_dom_utils.h"

#define DPI 72.0

#define DEG2RAD(degree) degree/180.0*M_PI

double SvgUtil::fromUserSpace(double value)
{
    return value;
}

double SvgUtil::toUserSpace(double value)
{
    return value;
}

double SvgUtil::ptToPx(SvgGraphicsContext *gc, double value)
{
    return value * gc->pixelsPerInch / DPI;
}

QPointF SvgUtil::toUserSpace(const QPointF &point)
{
    return QPointF(toUserSpace(point.x()), toUserSpace(point.y()));
}

QRectF SvgUtil::toUserSpace(const QRectF &rect)
{
    return QRectF(toUserSpace(rect.topLeft()), toUserSpace(rect.size()));
}

QSizeF SvgUtil::toUserSpace(const QSizeF &size)
{
    return QSizeF(toUserSpace(size.width()), toUserSpace(size.height()));
}

QString SvgUtil::toPercentage(qreal value)
{
    return KisDomUtils::toString(value * 100.0) + "%";
}

double SvgUtil::fromPercentage(QString s, bool *ok)
{
    if (s.endsWith('%'))
        return KisDomUtils::toDouble(s.remove('%'), ok) / 100.0;
    else
        return KisDomUtils::toDouble(s, ok);
}

QPointF SvgUtil::objectToUserSpace(const QPointF &position, const QRectF &objectBound)
{
    qreal x = objectBound.left() + position.x() * objectBound.width();
    qreal y = objectBound.top() + position.y() * objectBound.height();
    return QPointF(x, y);
}

QSizeF SvgUtil::objectToUserSpace(const QSizeF &size, const QRectF &objectBound)
{
    qreal w = size.width() * objectBound.width();
    qreal h = size.height() * objectBound.height();
    return QSizeF(w, h);
}

QPointF SvgUtil::userSpaceToObject(const QPointF &position, const QRectF &objectBound)
{
    qreal x = 0.0;
    if (objectBound.width() != 0)
        x = (position.x() - objectBound.x()) / objectBound.width();
    qreal y = 0.0;
    if (objectBound.height() != 0)
        y = (position.y() - objectBound.y()) / objectBound.height();
    return QPointF(x, y);
}

QSizeF SvgUtil::userSpaceToObject(const QSizeF &size, const QRectF &objectBound)
{
    qreal w = objectBound.width() != 0 ? size.width() / objectBound.width() : 0.0;
    qreal h = objectBound.height() != 0 ? size.height() / objectBound.height() : 0.0;
    return QSizeF(w, h);
}

QString SvgUtil::transformToString(const QTransform &transform)
{
    if (transform.isIdentity())
        return QString();

    if (transform.type() == QTransform::TxTranslate) {
        return QString("translate(%1, %2)")
                     .arg(KisDomUtils::toString(toUserSpace(transform.dx())))
                     .arg(KisDomUtils::toString(toUserSpace(transform.dy())));
    } else {
        return QString("matrix(%1 %2 %3 %4 %5 %6)")
                     .arg(KisDomUtils::toString(transform.m11()))
                     .arg(KisDomUtils::toString(transform.m12()))
                     .arg(KisDomUtils::toString(transform.m21()))
                     .arg(KisDomUtils::toString(transform.m22()))
                     .arg(KisDomUtils::toString(toUserSpace(transform.dx())))
                     .arg(KisDomUtils::toString(toUserSpace(transform.dy())));
    }
}

void SvgUtil::writeTransformAttributeLazy(const QString &name, const QTransform &transform, KoXmlWriter &shapeWriter)
{
    const QString value = transformToString(transform);

    if (!value.isEmpty()) {
        shapeWriter.addAttribute(name.toLatin1().data(), value);
    }
}

bool SvgUtil::parseViewBox(const KoXmlElement &e,
                           const QRectF &elementBounds,
                           QRectF *_viewRect, QTransform *_viewTransform)
{
    KIS_ASSERT(_viewRect);
    KIS_ASSERT(_viewTransform);

    QString viewBoxStr = e.attribute("viewBox");
    if (viewBoxStr.isEmpty()) return false;

    bool result = false;

    QRectF viewBoxRect;
    // this is a workaround for bug 260429 for a file generated by blender
    // who has px in the viewbox which is wrong.
    // reported as bug https://developer.blender.org/T30971
    viewBoxStr.remove("px");

    QStringList points = viewBoxStr.replace(',', ' ').simplified().split(' ');
    if (points.count() == 4) {
        viewBoxRect.setX(SvgUtil::fromUserSpace(points[0].toFloat()));
        viewBoxRect.setY(SvgUtil::fromUserSpace(points[1].toFloat()));
        viewBoxRect.setWidth(SvgUtil::fromUserSpace(points[2].toFloat()));
        viewBoxRect.setHeight(SvgUtil::fromUserSpace(points[3].toFloat()));

        result = true;
    } else {
        // TODO: WARNING!
    }

    if (!result) return false;

    QTransform viewBoxTransform =
        QTransform::fromTranslate(-viewBoxRect.x(), -viewBoxRect.y()) *
        QTransform::fromScale(elementBounds.width() / viewBoxRect.width(),
                                  elementBounds.height() / viewBoxRect.height()) *
            QTransform::fromTranslate(elementBounds.x(), elementBounds.y());

    const QString aspectString = e.attribute("preserveAspectRatio");
    // give initial value if value not defined
    PreserveAspectRatioParser p( (!aspectString.isEmpty())? aspectString : QString("xMidYMid meet"));
    parseAspectRatio(p, elementBounds, viewBoxRect, &viewBoxTransform);

    *_viewRect = viewBoxRect;
    *_viewTransform = viewBoxTransform;

    return result;
}

void SvgUtil::parseAspectRatio(const PreserveAspectRatioParser &p, const QRectF &elementBounds, const QRectF &viewBoxRect, QTransform *_viewTransform)
{
    if (p.mode != Qt::IgnoreAspectRatio) {
        QTransform viewBoxTransform = *_viewTransform;

        const qreal tan1 = viewBoxRect.height() / viewBoxRect.width();
        const qreal tan2 = elementBounds.height() / elementBounds.width();

        const qreal uniformScale =
            (p.mode == Qt::KeepAspectRatioByExpanding) ^ (tan1 > tan2) ?
                elementBounds.height() / viewBoxRect.height() :
                elementBounds.width() / viewBoxRect.width();

        viewBoxTransform =
            QTransform::fromTranslate(-viewBoxRect.x(), -viewBoxRect.y()) *
            QTransform::fromScale(uniformScale, uniformScale) *
            QTransform::fromTranslate(elementBounds.x(), elementBounds.y());

        const QPointF viewBoxAnchor = viewBoxTransform.map(p.rectAnchorPoint(viewBoxRect));
        const QPointF elementAnchor = p.rectAnchorPoint(elementBounds);
        const QPointF offset = elementAnchor - viewBoxAnchor;

        viewBoxTransform = viewBoxTransform * QTransform::fromTranslate(offset.x(), offset.y());

        *_viewTransform = viewBoxTransform;
    }
}

qreal SvgUtil::parseUnit(SvgGraphicsContext *gc, const QString &unit, bool horiz, bool vert, const QRectF &bbox)
{
    if (unit.isEmpty())
        return 0.0;
    QByteArray unitLatin1 = unit.toLatin1();
    // TODO : percentage?
    const char *start = unitLatin1.data();
    if (!start) {
        return 0.0;
    }
    qreal value = 0.0;
    const char *end = parseNumber(start, value);

    if (int(end - start) < unit.length()) {
        if (unit.right(2) == "px")
            value = SvgUtil::fromUserSpace(value);
        else if (unit.right(2) == "pt")
            value = ptToPx(gc, value);
        else if (unit.right(2) == "cm")
            value = ptToPx(gc, CM_TO_POINT(value));
        else if (unit.right(2) == "pc")
            value = ptToPx(gc, PI_TO_POINT(value));
        else if (unit.right(2) == "mm")
            value = ptToPx(gc, MM_TO_POINT(value));
        else if (unit.right(2) == "in")
            value = ptToPx(gc, INCH_TO_POINT(value));
        else if (unit.right(2) == "em")
            // NOTE: all the fonts should be created with 'pt' size, not px!
            value = ptToPx(gc, value * gc->font.pointSize());
        else if (unit.right(2) == "ex") {
            QFontMetrics metrics(gc->font);
            value = ptToPx(gc, value * metrics.xHeight());
        } else if (unit.right(1) == "%") {
            if (horiz && vert)
                value = (value / 100.0) * (sqrt(pow(bbox.width(), 2) + pow(bbox.height(), 2)) / sqrt(2.0));
            else if (horiz)
                value = (value / 100.0) * bbox.width();
            else if (vert)
                value = (value / 100.0) * bbox.height();
        }
    } else {
        value = SvgUtil::fromUserSpace(value);
    }
    /*else
    {
        if( m_gc.top() )
        {
            if( horiz && vert )
                value *= sqrt( pow( m_gc.top()->matrix.m11(), 2 ) + pow( m_gc.top()->matrix.m22(), 2 ) ) / sqrt( 2.0 );
            else if( horiz )
                value /= m_gc.top()->matrix.m11();
            else if( vert )
                value /= m_gc.top()->matrix.m22();
        }
    }*/
    //value *= 90.0 / DPI;

    return value;
}

qreal SvgUtil::parseUnitX(SvgGraphicsContext *gc, const QString &unit)
{
    if (gc->forcePercentage) {
        return SvgUtil::fromPercentage(unit) * gc->currentBoundingBox.width();
    } else {
        return SvgUtil::parseUnit(gc, unit, true, false, gc->currentBoundingBox);
    }
}

qreal SvgUtil::parseUnitY(SvgGraphicsContext *gc, const QString &unit)
{
    if (gc->forcePercentage) {
        return SvgUtil::fromPercentage(unit) * gc->currentBoundingBox.height();
    } else {
        return SvgUtil::parseUnit(gc, unit, false, true, gc->currentBoundingBox);
    }
}

qreal SvgUtil::parseUnitXY(SvgGraphicsContext *gc, const QString &unit)
{
    if (gc->forcePercentage) {
        const qreal value = SvgUtil::fromPercentage(unit);
        return value * sqrt(pow(gc->currentBoundingBox.width(), 2) + pow(gc->currentBoundingBox.height(), 2)) / sqrt(2.0);
    } else {
        return SvgUtil::parseUnit(gc, unit, true, true, gc->currentBoundingBox);
    }
}

qreal SvgUtil::parseUnitAngular(SvgGraphicsContext *gc, const QString &unit)
{
    Q_UNUSED(gc);

    qreal value = 0.0;

    if (unit.isEmpty()) return value;
    QByteArray unitLatin1 = unit.toLower().toLatin1();

    const char *start = unitLatin1.data();
    if (!start) return value;

    const char *end = parseNumber(start, value);

    if (int(end - start) < unit.length()) {
        if (unit.right(3) == "deg") {
            value = kisDegreesToRadians(value);
        } else if (unit.right(4) == "grad") {
            value *= M_PI / 200;
        } else if (unit.right(3) == "rad") {
            // noop!
        } else {
            value = kisDegreesToRadians(value);
        }
    } else {
        value = kisDegreesToRadians(value);
    }

    return value;
}

qreal SvgUtil::parseNumber(const QString &string)
{
    qreal value = 0.0;

    if (string.isEmpty()) return value;
    QByteArray unitLatin1 = string.toLatin1();

    const char *start = unitLatin1.data();
    if (!start) return value;

    const char *end = parseNumber(start, value);
    KIS_SAFE_ASSERT_RECOVER_NOOP(int(end - start) == string.length());
    return value;
}

const char * SvgUtil::parseNumber(const char *ptr, qreal &number)
{
    int integer, exponent;
    qreal decimal, frac;
    int sign, expsign;

    exponent = 0;
    integer = 0;
    frac = 1.0;
    decimal = 0;
    sign = 1;
    expsign = 1;

    // read the sign
    if (*ptr == '+') {
        ptr++;
    } else if (*ptr == '-') {
        ptr++;
        sign = -1;
    }

    // read the integer part
    while (*ptr != '\0' && *ptr >= '0' && *ptr <= '9')
        integer = (integer * 10) + *(ptr++) - '0';
    if (*ptr == '.') { // read the decimals
        ptr++;
        while (*ptr != '\0' && *ptr >= '0' && *ptr <= '9')
            decimal += (*(ptr++) - '0') * (frac *= 0.1);
    }

    if (*ptr == 'e' || *ptr == 'E') { // read the exponent part
        ptr++;

        // read the sign of the exponent
        if (*ptr == '+') {
            ptr++;
        } else if (*ptr == '-') {
            ptr++;
            expsign = -1;
        }

        exponent = 0;
        while (*ptr != '\0' && *ptr >= '0' && *ptr <= '9') {
            exponent *= 10;
            exponent += *ptr - '0';
            ptr++;
        }
    }
    number = integer + decimal;
    number *= sign * pow((double)10, double(expsign * exponent));

    return ptr;
}

QString SvgUtil::mapExtendedShapeTag(const QString &tagName, const KoXmlElement &element)
{
    QString result = tagName;

    if (tagName == "path") {
        QString kritaType = element.attribute("krita:type", "");
        QString sodipodiType = element.attribute("sodipodi:type", "");

        if (kritaType == "arc") {
            result = "krita:arc";
        } else if (sodipodiType == "arc") {
            result = "sodipodi:arc";
        }
    }

    return result;
}

QStringList SvgUtil::simplifyList(const QString &str)
{
    QString attribute = str;
    attribute.replace(',', ' ');
    attribute.remove('\r');
    attribute.remove('\n');
    return attribute.simplified().split(' ', QString::SkipEmptyParts);
}

SvgUtil::PreserveAspectRatioParser::PreserveAspectRatioParser(const QString &str)
{
    QRegExp rexp("(defer)?\\s*(none|(x(Min|Max|Mid)Y(Min|Max|Mid)))\\s*(meet|slice)?", Qt::CaseInsensitive);
    int index = rexp.indexIn(str.toLower());

    if (index >= 0) {
        if (rexp.cap(1) == "defer") {
            defer = true;
        }

        if (rexp.cap(2) != "none") {
            xAlignment = alignmentFromString(rexp.cap(4));
            yAlignment = alignmentFromString(rexp.cap(5));
            mode = rexp.cap(6) == "slice" ?
                Qt::KeepAspectRatioByExpanding : Qt::KeepAspectRatio;
        }
    }
}

QPointF SvgUtil::PreserveAspectRatioParser::rectAnchorPoint(const QRectF &rc) const
{
    return QPointF(alignedValue(rc.x(), rc.x() + rc.width(), xAlignment),
                   alignedValue(rc.y(), rc.y() + rc.height(), yAlignment));
}

QString SvgUtil::PreserveAspectRatioParser::toString() const
{
    QString result;

    if (!defer &&
        xAlignment == Middle &&
        yAlignment == Middle &&
        mode == Qt::KeepAspectRatio) {

        return result;
    }

    if (defer) {
        result += "defer ";
    }

    if (mode == Qt::IgnoreAspectRatio) {
        result += "none";
    } else {
        result += QString("x%1Y%2")
            .arg(alignmentToString(xAlignment))
            .arg(alignmentToString(yAlignment));

        if (mode == Qt::KeepAspectRatioByExpanding) {
            result += " slice";
        }
    }

    return result;
}

SvgUtil::PreserveAspectRatioParser::Alignment SvgUtil::PreserveAspectRatioParser::alignmentFromString(const QString &str) const {
    return
        str == "max" ? Max :
        str == "mid" ? Middle : Min;
}

QString SvgUtil::PreserveAspectRatioParser::alignmentToString(SvgUtil::PreserveAspectRatioParser::Alignment alignment) const
{
    return
        alignment == Max ? "Max" :
        alignment == Min ? "Min" :
        "Mid";

}

qreal SvgUtil::PreserveAspectRatioParser::alignedValue(qreal min, qreal max, SvgUtil::PreserveAspectRatioParser::Alignment alignment)
{
    qreal result = min;

    switch (alignment) {
    case Min:
        result = min;
        break;
    case Middle:
        result = 0.5 * (min + max);
        break;
    case Max:
        result = max;
        break;
    }

    return result;
}
