#pragma once

#include <QProxyStyle>

class OBSProxyStyle : public QProxyStyle {
	Q_OBJECT

public:
	OBSProxyStyle() : QProxyStyle() {}

	OBSProxyStyle(const QString &key) : QProxyStyle(key) {}

	int styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget,
		      QStyleHintReturn *returnData) const override;
	int pixelMetric(PixelMetric pm, const QStyleOption *option, const QWidget *widget) const override;
};

class OBSContextBarProxyStyle : public OBSProxyStyle {
public:
	QPixmap generatedIconPixmap(QIcon::Mode iconMode, const QPixmap &pixmap,
				    const QStyleOption *option) const override;
};
