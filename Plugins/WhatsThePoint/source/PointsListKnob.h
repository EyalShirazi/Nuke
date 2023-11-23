#pragma once
 
#ifdef PYTHON_LINK
    #include <Python.h> // from python-dev, main python/C API
    #include <structmember.h> // defines a python class in C++
    #include <stddef.h>
#endif

#include "Points.h"
#include "PointsListWidget.moc"
#include <QtGui/QGuiApplication>


#define gl_color_selected  glColor4f(1,0,0,1)
#define gl_color_valid     glColor4f(1,1,1,1)
#define gl_color_nonvalid  glColor4f(0,0,1,1)
#define gl_color_shadow    glColor4f(0,0,0,0.75)

#define gl_2d_text(x, y, text) 		DD::Image::gl_text(text, x, y);

#define gl_2d_cross(x, y, scale) 	glBegin(GL_LINE_STRIP);  \
                                    glVertex2f(x - scale, y);\
                                    glVertex2f(x + scale, y);\
                                    glEnd();                 \
                                    glBegin(GL_LINE_STRIP);  \
                                    glVertex2f(x, y - scale);\
                                    glVertex2f(x, y + scale);\
                                    glEnd();

#define gl_3d_cross(x, y, z, scale) glBegin(GL_LINE_STRIP);     \
                                    glVertex3f(x - scale, y, z);\
                                    glVertex3f(x + scale, y, z);\
                                    glEnd();                    \
                                    glBegin(GL_LINE_STRIP);     \
                                    glVertex3f(x, y - scale, z);\
                                    glVertex3f(x, y + scale, z);\
                                    glEnd();                    \
                                    glBegin(GL_LINE_STRIP);     \
                                    glVertex3f(x, y, z - scale);\
                                    glVertex3f(x, y, z + scale);\
                                    glEnd();


struct PointsListKnob : public DD::Image::Knob
{  
    PointsListWidget* qtWidget;
    Points& points;
    std::vector<uint32_t> temporary_point_selection;

    DD::Image::Vector3 screenCenter;
    Point2D user_pen;
    uint32_t previous_mouse_click{false};
    uint32_t hit_deteced{false};
    uint32_t hit_deteced_state{false};
    uint32_t hit_deteced_index{0};

    PointsListKnob(DD::Image::Knob_Closure* kc, Points* _points, const char* _name)
    : Knob{kc, _name, ""}
    , points(*_points)
    {
#ifdef PYTHON_LINK
        PluginPython_KnobI();
        setPythonType(&PointsListKnobPythonType);
#endif
    }

    virtual const char* Class() const { return "PointsListKnob"; } 
    virtual bool not_default () const { return points.size; }
    virtual void to_script(std::ostream&, const DD::Image::OutputContext*, bool) const override;
    virtual bool from_script(const char*) override;
    void store(DD::Image::StoreType type, void* data, DD::Image::Hash &hash, const DD::Image::OutputContext &oc) override 
    {}
    
#ifdef PYTHON_LINK
    DD::Image::PluginPython_KnobI* pluginPythonKnob() override
    {
        
        return this;
    }
#endif

    WidgetPointer make_widget(const DD::Image::WidgetContext& context) override
    {
        
        qtWidget = new PointsListWidget(this, points);
        return qtWidget;
    }

    Point2D screen_mouse_to_image( DD::Image::ViewerContext* ctx)
    {
        
        Point2D user;
      
        DD::Image::Matrix4 viewer_to_format;
        viewer_to_format.makeIdentity();
        viewer_to_format.a00 =  2.0f / (float)ctx->viewport().w();
        viewer_to_format.a11 = -2.0f / (float)ctx->viewport().h();
        viewer_to_format.a02 = -1.0f;
        viewer_to_format.a12 =  1.0f;
        
        viewer_to_format = ctx->cam_matrix().inverse() * ctx->proj_matrix().inverse() * viewer_to_format;
        float mousex = ctx->mouse_x();
        float mousey = ctx->mouse_y();

        user.x = viewer_to_format.a00 * mousex + viewer_to_format.a01 * mousey + viewer_to_format.a02 + viewer_to_format.a03;
        user.y = viewer_to_format.a10 * mousex + viewer_to_format.a11 * mousey + viewer_to_format.a12 + viewer_to_format.a13;
        
        return user;
    }

    bool build_handle( DD::Image::ViewerContext* ctx )
    {
        

        op()->validate();

        // if((qtWidget != nullptr) && !node_disabled()) // is node enable and panel is visible
        if(isVisible() && !node_disabled()) // is node enable and panel is visible
        {
            if (ctx->transform_mode() != DD::Image::ViewerMode::VIEWER_2D)
            {
                if (points.is_any_selected())
                {
                    Point3D min, max;
                    points.get_bbox_3d(min, max);

                    DD::Image::Box3 box;
                    box.set_min(min.x,min.y,min.z);
                    box.set_max(max.x,max.y,max.z);
                    ctx->expand_bbox(true, box);
                }
                return true;
            }
            else
            {
                return true;				
            }

        }

        return false;
    }
        
    void draw_handle( DD::Image::ViewerContext* ctx )
    {
        // 
        if (qtWidget == nullptr) return;

         if (ctx->transform_mode() == DD::Image::VIEWER_2D)
        {
            glPushMatrix();

            if (qtWidget->is_stabilize()) glMultMatrixf(&(((DD::Image::Transform*)op())->matrix()->a00));

            size_t size = points.size;
            float scale  = 10.0f / ctx->zoom();
            float shadow = 0.4f  / ctx->zoom();
            float text   = 5.0f  / ctx->zoom();
            screenCenter = (ctx->proj_matrix().translation()/ctx->proj_matrix().scale());

            begin_handle( DD::Image::Knob::ANYWHERE_KEY_PRESSED, ctx, keypress_detection_wrapper_cb, 0, 0, 0);

            // Handle Mouse 
            uint32_t mouseButton = QGuiApplication::mouseButtons();
            if (mouseButton != previous_mouse_click) // faulty behaviour while using ctx for mouse input
            {	
                if (ctx->event() > DD::Image::ViewerEvent::DRAW_LINES) // secondary check for mouse clicks only in viewer area
                {
                    previous_mouse_click = mouseButton;
                    // PUSH
                    if (previous_mouse_click) 
                    {
                        user_pen = screen_mouse_to_image(ctx);
                        if (ctx->state() == (DD::Image::SHIFT + DD::Image::ALT)) // add point
                        {}
                        else if (ctx->state() == DD::Image::SHIFT) // add selection
                        {
                            temporary_point_selection.resize(size);
                            for (size_t index = 0; index < size; ++index)
                            {
                                temporary_point_selection[index] = points.is_selected(index);
                            }
                        }
                        else // new selection box
                        {}
                    }
                    // REALSE
                    else 
                    {
                        qtWidget->select_rows();
                        temporary_point_selection.clear();
                    }
                }
            }
            else
            {
                // DRAG
                if (!hit_deteced) 
                {
                    if(previous_mouse_click)
                    {
                        if (ctx->state() == (DD::Image::SHIFT + DD::Image::ALT))
                        {}
                        else
                        {
                            Point2D user_temp = screen_mouse_to_image(ctx);
                            float bx = user_pen.x < user_temp.x ? user_pen.x : user_temp.x;
                            float by = user_pen.y < user_temp.y ? user_pen.y : user_temp.y;
                            float br = user_pen.x > user_temp.x ? user_pen.x : user_temp.x;
                            float bt = user_pen.y > user_temp.y ? user_pen.y : user_temp.y;

                            if (ctx->state() == DD::Image::SHIFT) // add selection
                            {
                                for (size_t index = 0; index < size; ++index)
                                {
                                    Point2D& v = points.points_2d[index];
                                    points.set_selected(index, temporary_point_selection[index] | ((v.x > bx) & (v.x < br) & (v.y > by) & (v.y < bt)));
                                }
                            }
                            else // new selection
                            {
                                for (size_t index = 0; index < size; ++index)
                                {
                                    Point2D& v = points.points_2d[index];
                                    points.set_selected(index, ((v.x > bx) & (v.x < br) & (v.y > by) & (v.y < bt)));
                                }
                            }
                        }
                    }
                }
            }

            uint32_t is_draw_shadow = ctx->event() == DD::Image::ViewerEvent::DRAW_SHADOW;
            uint32_t is_draw_lines  = ctx->event() == DD::Image::ViewerEvent::DRAW_LINES;
            uint32_t is_hit_push    = ctx->event() == DD::Image::ViewerEvent::PUSH;

            if (is_draw_shadow | is_draw_lines | is_hit_push )
            {
                // Draw point name, could be slow
                if (qtWidget->is_show_label())
                {
                    if (is_draw_shadow)
                    {
                        gl_color_shadow;
                    }
                    else
                    {
                        gl_color_valid;
                    }

                    QTableWidget& table = qtWidget->table;
                    for (size_t index = 0; index < size; ++index)
                    {
                        float x = points.points_2d[index].x;
                        float y = points.points_2d[index].y;

                        gl_2d_text(x+text, y+text, points.name[index].c_str());
                    }
                }

                // Draw point 2d cross
                begin_handle(ctx, hit_detection_wrapper_cb, 0, 0, 0, 0);
                for (size_t index = 0; index < size; ++index)
                {
                    float x = points.points_2d[index].x;
                    float y = points.points_2d[index].y;

                    if (is_draw_shadow)
                    {
                        gl_color_shadow;
                        gl_2d_cross(x+shadow, y-shadow, scale);
                    }
                    else
                    {
                        DD::Image::glColor(points.is_selected(index) ? ctx->node_color() : 0xeeeeeeee);
                        gl_2d_cross(x, y, scale);
                    }
                }
            }

            glPopMatrix();
        }
        else
        {
            op()->validate();
        
            DD::Image::Vector3 camera = ctx->camera_pos().truncate_w();

            // Draw point 3d cross
            float scale = .03f;
            size_t size = points.size;
            
            for (size_t index = 0; index < size; ++index)
            {
                DD::Image::Vector3 point{points.points_3d[index].arr};
                DD::Image::Vector3 normal{points.normals[index].norm};
                DD::Image::Vector3 tangent{points.normals[index].norm+3};
                DD::Image::Vector3 bi = normal.cross(tangent);

                float d = (camera - point).length() * scale;
                normal  *= d;
                tangent *= d;
                bi      *= d;

                DD::Image::glColor(points.is_selected(index) ? ctx->node_color() : 0xeeeeeeee);
                glBegin(GL_LINE_STRIP);
                glVertex3f(point.x - normal.x, point.y - normal.y, point.z - normal.z);
                glVertex3f(point.x + normal.x, point.y + normal.y, point.z + normal.z);
                glEnd();
                glBegin(GL_LINE_STRIP);
                glVertex3f(point.x - tangent.x, point.y - tangent.y, point.z - tangent.z);
                glVertex3f(point.x + tangent.x, point.y + tangent.y, point.z + tangent.z);
                glEnd();
                glBegin(GL_LINE_STRIP);
                glVertex3f(point.x - bi.x, point.y - bi.y, point.z - bi.z);
                glVertex3f(point.x + bi.x, point.y + bi.y, point.z + bi.z);
                glEnd();
            }
        }
    }


    static bool keypress_detection_wrapper_cb(DD::Image::ViewerContext* ctx, DD::Image::Knob* knob, int) { return ((PointsListKnob*)knob)->keypress_detection_cb(ctx); }
    bool keypress_detection_cb(DD::Image::ViewerContext* ctx);
    
    static bool hit_detection_wrapper_cb(DD::Image::ViewerContext* ctx, DD::Image::Knob* knob, int index) { return ((PointsListKnob*)knob)->hit_detection_cb(ctx); }
    bool hit_detection_cb(DD::Image::ViewerContext* ctx);
};




PointsListWidget::PointsListWidget(PointsListKnob* _knob, Points& _points) : knob(_knob), points(_points)
{
    
    knob->addCallback(WidgetCallback, this);

    set_widget_callbacks();
    set_table_layout();

    label.setCheckable(true);
    stabilize.setCheckable(true);

    buttonsLayout.addWidget(&add,    0, Qt::AlignTop);
    buttonsLayout.addWidget(&remove, 0, Qt::AlignTop);
    buttonsLayout.addWidget(&clear,  0, Qt::AlignTop);
    buttonsLayout.addWidget(&bake,  0, Qt::AlignTop);
    buttonsLayout.addStretch(0);
    buttonsLayout.addWidget(&label,  0, Qt::AlignTop);
    buttonsLayout.addWidget(&stabilize,  0, Qt::AlignTop);

    mainLayout.addWidget(&table);
    mainLayout.addLayout(&buttonsLayout);
    
    setLayout(&mainLayout);
    setStyleSheet("QGroupBox{ border: 0; margin-top: 10; margin-bottom: 10}");

    populate_table();
}

int PointsListWidget::WidgetCallback(void* closure, DD::Image::Knob::CallbackReason reason)
{
    
    PointsListWidget* widget = (PointsListWidget*)closure;
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


inline bool PointsListWidget::is_show_label()
{
    
    return label.isChecked();
}

inline bool PointsListWidget::is_stabilize()
{
    
    return stabilize.isChecked();
}

void PointsListWidget::populate_table()
{
    
    table.blockSignals(true);

    size_t size = points.size;
    for (size_t index = 0; index < size; ++index)
    {
        add_table_item(index);
    }

    table.blockSignals(false);
    select_rows();
    knob->redraw();
    
}

inline void PointsListWidget::add_widget_cb()
{
    
    int index = table.rowCount();
    table.blockSignals(true);

    add_table_item(index);

    table.blockSignals(false);
    select_rows();
    knob->redraw();
    
}

void PointsListWidget::remove_widget_cb()
{
    
    table.blockSignals(true);
    table.setSelectionMode(QAbstractItemView::MultiSelection);

    size_t size = points.size;
    for(size_t index = size-1; index < size ; --index)
    {
        if (points.is_selected(index)) table.removeRow(index);
    }

    table.setSelectionMode(QAbstractItemView::ExtendedSelection);
    table.blockSignals(false);
    knob->redraw();
    
}

void PointsListWidget::clear_widget_cb()
{
    
    table.clearContents();
    table.setRowCount(0);
    knob->redraw();
    
}