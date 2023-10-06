#pragma once

#include <QtCore/QObject>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTabWidget>
#include <QtGui/QGuiApplication>

class ToggleButtonKnob;

class ToggleButtonWidget : public QPushButton
{
	Q_OBJECT

	ToggleButtonKnob* knob;
	bool& buttonState;

public:

	ToggleButtonWidget(const char*, bool&, ToggleButtonKnob*);
	~ToggleButtonWidget();
	static int WidgetCallback(void*, DD::Image::Knob::CallbackReason);

public Q_SLOTS:

	void buttonToggleCallback(bool);
};
