#pragma once

#include <QtWidgets/QGroupBox>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTableWidget>

class PointsListKnob;
class Points;

class PointsListWidget : public QGroupBox
{
    Q_OBJECT

public:

    PointsListKnob* knob;

    QBoxLayout mainLayout{QBoxLayout::LeftToRight};
    QBoxLayout buttonsLayout{QBoxLayout::TopToBottom};

    QPushButton add{"add"};
    QPushButton remove{"remove"};
    QPushButton clear{"clear"};
    QPushButton bake{"bake"};
    QPushButton label{"label"};
    QPushButton stabilize{"stabilize"};

    QTableWidget table;

    Points& points;

    PointsListWidget(PointsListKnob*, Points&);
    ~PointsListWidget();
    static int WidgetCallback(void*, DD::Image::Knob::CallbackReason);

public Q_SLOTS:

    void add_widget_cb();
    void remove_widget_cb();
    void clear_widget_cb();
    bool is_show_label();
    bool is_stabilize();

    void set_widget_callbacks();
    void set_table_layout();

    void populate_table();
    void add_table_item(int);
    void upade_name_cell(int, int);
    void update_widget_cell();
    void update_stabilize();

    void select_rows();
    void get_selected_rows();
};