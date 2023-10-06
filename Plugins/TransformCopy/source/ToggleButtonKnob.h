#pragma once

#include "ToggleButtonWidget.moc"

struct ToggleButtonKnob : public DD::Image::Knob 
{  
	ToggleButtonWidget* qtWidget;
	const char* label;
	bool buttonState{0};

	ToggleButtonKnob(DD::Image::Knob_Closure* kc, bool* _buttonState, const char* _label)
	: Knob(kc, _label)
	, buttonState{*_buttonState}
	, label{_label}
	{}

	virtual const char* Class() const { return "ToggleButtonKnob"; }
	virtual bool not_default () const { return buttonState != 0; }

	WidgetPointer make_widget(const DD::Image::WidgetContext& context) 
	{
		qtWidget = new ToggleButtonWidget(label, buttonState, this);
		return qtWidget;
	}

	void store(DD::Image::StoreType type, void* _data, DD::Image::Hash &hash, const DD::Image::OutputContext &oc) 
	{
		bool* destData = (bool*)_data;
		*destData = buttonState;
		hash.append(buttonState);
	}

	virtual void to_script (std::ostream &os, const DD::Image::OutputContext *, bool quote) const 
	{
		os << buttonState;
	}

	virtual bool from_script(const char *	v)
	{
		std::stringstream ss(v);
		ss >> buttonState;
		return true;
	}

	void buttonStateUpdate()
	{
		changed();
	}
};



ToggleButtonWidget::ToggleButtonWidget( const char* _label, bool& _buttonState, ToggleButtonKnob* _knob) 
: knob(_knob)
, buttonState(_buttonState)
{
	knob->addCallback(WidgetCallback, this);

	setText(_label);
	setCheckable(true);
	setFixedHeight(20);
	setFixedWidth(80);

	setDown(buttonState);
	setChecked(buttonState);

	connect(this, SIGNAL(toggled(bool)), this, SLOT(buttonToggleCallback(bool)));
}

ToggleButtonWidget::~ToggleButtonWidget()
{
	if(knob) 
	{
		knob->removeCallback( WidgetCallback, this );
		knob = nullptr;
	}
}

int ToggleButtonWidget::WidgetCallback(void* closure, DD::Image::Knob::CallbackReason reason)
{
	ToggleButtonWidget* widget = (ToggleButtonWidget*)closure;
	assert(widget);

	if (reason == DD::Image::Knob::kIsVisible)
	{
		for (QWidget* w = widget->parentWidget(); w; w = w->parentWidget())
		{
			if (qobject_cast<QTabWidget*>(w))
			{
				return widget->isVisibleTo(w);
			}	
		}
		return widget->isVisible();
	}
		
	return 0;
}

void ToggleButtonWidget::buttonToggleCallback(bool state)
{
	if ((QGuiApplication::keyboardModifiers().testFlag(Qt::ControlModifier)))
	{
		state = true;
		setDown(state);
		setChecked(state);
	}
	buttonState = state;
	knob->buttonStateUpdate();
}