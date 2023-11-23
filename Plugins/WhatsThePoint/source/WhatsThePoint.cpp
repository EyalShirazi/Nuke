//*******************************************************************//
// Author   : Eyal Shirazi
// Created  : 01/01/2010
// Modified : 11/23/2023
// 
// TODO (Eyal) ::
// fix 2d rotation                     (bring back from previous code)
// baking transformation in stereo     (bring back from previous code)
// viewer transform superimpose option (bring back from previous code)
// plane builder and viewer controller (bring back from previous code)
// add selection from 3d point view    (bring back from previous code)
// add tool tips
// add undo ?
// add python linking/erpressions ?
// add gl color indication for valid/non-valid points ?
// 
// BUG (Eyal) ::
// seems that disabled transform is still working on update points
//*******************************************************************//

#include "DDImage/Transform.h"
#include "DDImage/Knobs.h"
#include "DDImage/Format.h"
#include "DDImage/gl.h"

#include <QtWidgets/QDialog>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QFrame>
#include <QtWidgets/QLabel>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QProgressDialog>


#define VERSION_MAJOR 1
#define VERSION_MINOR 00
#define HELP    "" 

// #define OP_DROPDOWN

#include "Points.h"

#include "PointsListKnob.h"
#include "ModGeo.h"

#define OP_TYPE_LIST(ENTRY) \
    ENTRY(geo) \
    ENTRY(plate) \
    ENTRY(cloud) \
    ENTRY(vector) \

enum kOpTypeEnum {
    #define EXPEND_OP_TYPE_ENUM(name) kOpType_##name,
    OP_TYPE_LIST(EXPEND_OP_TYPE_ENUM)
    kOpTypeEnumSize
};

struct WhatsThePoint final : public DD::Image::Transform
{
    static const char* const kOpTypeList[];
    int OpTypeValue{0};

    PointsListKnob* kPointsList{nullptr};
    ModGeo modGeo{this};

    Points points;

    uint32_t update_counter{0};

    WhatsThePoint(Node* node) 
    : DD::Image::Transform(node)
    {}
    
    ~WhatsThePoint()
    {}

    static const char* const kHelp;
    static const char* const kInfo;
    static const DD::Image::Transform::Description d;
    const char* Class() const override { return d.name; }
    const char* node_help() const override { return kHelp; }
    int maximum_inputs() const override { return 3; }
    int minimum_inputs() const override { return 3; }

    void append(DD::Image::Hash& hash) override
    {
        

        hash.append(__DATE__);
        hash.append(__TIME__);
        hash.append(OpTypeValue);
        hash.append(update_counter);
        // points

        DD::Image::Transform::append(hash);
        
    }

    void knobs(DD::Image::Knob_Callback f) override   
    {
        
        #ifdef OP_DROPDOWN
        Enumeration_knob(f, &OpTypeValue, kOpTypeList, "op", "   operation");
        SetFlags(f, DD::Image::Knob::ENDLINE);
        #endif

        kPointsList = CustomKnob1(PointsListKnob, f, &points, "points");

        Tab_knob(f, "info");

        Divider(f);
        Text_knob(f, kInfo);
        
    }

    int knob_changed(DD::Image::Knob* k) override
    {
        
        if (k->name() == "op")
        {
            if (PointsListWidget* table = kPointsList->qtWidget)
            {
                table->set_table_layout();
                
                return 1;
            }
        }
        
        return 0;
    }

    const char* input_label(int n, char*) const override
    {	
        switch (n)
        {
            case 0: return "image";
            case 1: return "camera";
            case 2: return "geo";
            default: return "";
        }
    }

    bool test_input(int input, DD::Image::Op *op) const override
    {
        switch (input) 
        {
            case 0:  return dynamic_cast<DD::Image::Iop*>(op) != nullptr;
            case 1:  return dynamic_cast<DD::Image::CameraOp*>(op) != nullptr;
            case 2:  return dynamic_cast<DD::Image::GeoOp*>(op) != nullptr;
            default: return DD::Image::Transform::test_input(input, op);
        }
    }

    void _validate(bool for_real) override
    {
        
        update_points(points, uiContext().frame(), uiContext().view());

        matrix_.makeIdentity();

        if (kPointsList && kPointsList->isVisible())
        {
            if (kPointsList->qtWidget && kPointsList->qtWidget->is_stabilize())
            {
                matrix_average(matrix_);
                matrix_ = matrix_.inverse();
            }
        }

        DD::Image::Transform::_validate(for_real);
        
    }


    void matrix_average(DD::Image::Matrix4& out_matrix)
    {
        

        if (!points.is_any_selected()) return;
        if (DD::Image::Op::input(1) == nullptr | DD::Image::Op::input(2) == nullptr) return;

        float x = 0;
        float y = 0;
        uint32_t points_counter = 0;
        for(size_t index = 0, point_size = points.size ; index < point_size ; ++index) 
        {
            if (!points.is_selected(index)) continue;

            x += points.points_2d[index].x;
            y += points.points_2d[index].y;
            ++points_counter;
        }

        float avg_recp = 1.0f / (float)points_counter;
        x *= avg_recp;
        y *= avg_recp;

        int format_width  = input_format().width()  / 2;
        int format_height = input_format().height() / 2;

        out_matrix.translation(x-format_width, y-format_height);
        
    }

    void matrixAt(const DD::Image::OutputContext& context, DD::Image::Matrix4& out) override
    {
        out.makeIdentity();
    }

    void build_handles(DD::Image::ViewerContext* ctx)
    {
        DD::Image::Op::build_handles(ctx);
    }

    void to_script(std::ostream &os)
    {
        
        modGeo.to_script(os, points);
        
    }

    void from_script(const char *v)
    {
        
        modGeo.from_script(v, points);
        update_points(points, uiContext().frame(), uiContext().view());
        
    }

    void add_clicked()
    {
        
        float cx = input_format().w() / 2.0f;
        float cy = input_format().h() / 2.0f;
        float as = input_format().pixel_aspect();

        DD::Image::Vector3 v = DD::Image::Vector3(1.0f, cy/cx, 0.0f) - kPointsList->screenCenter;
        add_clicked_position(v.x * cx, v.y * cx * as);
        
    }

    void add_clicked_position(float x, float y)
    {
        
        points.deselect_all_points();

        size_t index = points.add_emtpy();

        std::stringstream ss;
        ss << "Point_" << index;
        
        points.set_2d(index, x, y);
        points.set_name(index, ss.str().c_str());
        points.set_selected(index, 1);

        modGeo.add(x, y);
        modGeo.solve_points(points);

        update_points(points, uiContext().frame(), uiContext().view());

        if (kPointsList && kPointsList->isVisible())
        {
            kPointsList->qtWidget->add_widget_cb();
            kPointsList->changed();
        }
        
    }

    void update_points(Points& points_list, float frame ,int view)
    {
        
        modGeo.update_points(points_list, frame, view);
        
    }

    void update_table_item(QTableWidget& table, int index)
    {
        
        table.item(index, 0)->setText(QString(points.name[index].c_str()).append("            "));
        table.item(index, 1)->setText(QString(modGeo.items[index].geo.c_str()));
        
    }

    void remove_clicked()
    {
        
        points.erase(modGeo);

        if (kPointsList)
        {
            kPointsList->changed();
        }
        
    }

    void clear_clicked()
    {
        
        points.clear(modGeo);

        if (kPointsList)
        {
            kPointsList->changed();
        }     
        
    }

    void rebuild_point()
    {
        
        modGeo.solve_points(points);
        update_points(points, uiContext().frame(), uiContext().view());

        if (kPointsList && kPointsList->isVisible()) 
        {
            kPointsList->qtWidget->update_widget_cell();
            kPointsList->changed();
        }
        
    }

    void bake_points()
    {
        // bail if we have no points, no selected points, no camera or geometry
        if (!points.is_any_selected()) return;
        if (DD::Image::Op::input(1) == nullptr | DD::Image::Op::input(2) == nullptr) return;

        ////////////////////////////////////////////////////////////
        // popup panel and get user request
        ////////////////////////////////////////////////////////////

        int format_width  = input_format().width();
        int format_height = input_format().height();

        script_command("nuke.root().firstFrame()",true,true);
        int frame_start = atoi(script_result(true));
        script_unlock();

        script_command("nuke.root().lastFrame()",true,true);
        int frame_end = atoi(script_result(true));
        script_unlock();

        QDialog qt_dialog_box;
        qt_dialog_box.setWindowTitle("bake");
        qt_dialog_box.setWindowFlags(Qt::Dialog);

        QLabel qt_frame_range_label(QObject::tr("frame range:"));
        QLineEdit qt_frame_range_value;
        qt_frame_range_value.setText(QString::number(frame_start)+"-"+QString::number(frame_end));

        QPushButton qt_transform_button{"Transform"};
        qt_transform_button.setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
        qt_transform_button.setCheckable(true);

        QComboBox qt_transform_option;
        qt_transform_option.addItem("tracking");
        qt_transform_option.addItem("tracking format center");
        qt_transform_option.setCurrentIndex(0);
        qt_transform_option.setEnabled(false);
        QObject::connect(&qt_transform_button, SIGNAL(clicked(bool)), &qt_transform_option, SLOT(setEnabled(bool)));

        QPushButton qt_axis_button{"Axis"};
        qt_axis_button.setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
        qt_axis_button.setCheckable(true);

        QComboBox qt_rotation_order;
        qt_rotation_order.addItem("XYZ");
        qt_rotation_order.addItem("XZY");
        qt_rotation_order.addItem("YXZ");
        qt_rotation_order.addItem("YZX");
        qt_rotation_order.addItem("ZXY");
        qt_rotation_order.addItem("ZYX");
        qt_rotation_order.setCurrentIndex(4);
        qt_rotation_order.setEnabled(false);
        QObject::connect(&qt_axis_button, SIGNAL(clicked(bool)), &qt_rotation_order, SLOT(setEnabled(bool)));

        QFrame qt_divider_1;
        qt_divider_1.setFrameShape(QFrame::HLine);
        qt_divider_1.setFrameShadow(QFrame::Sunken);
        
        QLabel qt_new_group_label(QObject::tr("average points"));

        QPushButton qt_new_transform_button{"Transfrom"};
        qt_new_transform_button.setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
        qt_new_transform_button.setCheckable(true);

        QPushButton qt_new_axis_button{"Axis"};
        qt_new_axis_button.setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
        qt_new_axis_button.setCheckable(true);

        QFrame qt_divider_2;
        qt_divider_2.setFrameShape(QFrame::HLine);
        qt_divider_2.setFrameShadow(QFrame::Sunken);

        QPushButton qt_okay_button("ok");
        qt_okay_button.setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
        qt_okay_button.setCheckable(true);
        QObject::connect(&qt_okay_button, &QPushButton::clicked, &qt_dialog_box, &QDialog::accept);

        QPushButton qt_cancel_button("cancel");
        qt_cancel_button.setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
        qt_cancel_button.setCheckable(true);
        QObject::connect(&qt_cancel_button, &QPushButton::clicked, &qt_dialog_box, &QDialog::reject);

        QGridLayout qt_layout(&qt_dialog_box);

        int pos_y = 1;
        qt_layout.addWidget(&qt_frame_range_label, pos_y, 0);	
        qt_layout.addWidget(&qt_frame_range_value, pos_y, 1); 
        ++pos_y;
        qt_layout.addWidget(&qt_transform_button, pos_y, 0);	
        qt_layout.addWidget(&qt_transform_option, pos_y, 1); 
        ++pos_y;
        qt_layout.addWidget(&qt_axis_button, pos_y, 0);	
        qt_layout.addWidget(&qt_rotation_order, pos_y, 1);
        ++pos_y;
        qt_layout.addWidget(&qt_divider_1, pos_y, 0, 1, 2);
        ++pos_y;
        qt_layout.addWidget(&qt_new_group_label, pos_y, 0, 1, 2);
        ++pos_y;
        qt_layout.addWidget(&qt_new_transform_button, pos_y, 0, Qt::AlignCenter);	
        qt_layout.addWidget(&qt_new_axis_button, pos_y, 1, Qt::AlignCenter);	
        ++pos_y;
        qt_layout.addWidget(&qt_divider_2, pos_y, 0, 1, 2);
        ++pos_y;
        qt_layout.addWidget(&qt_okay_button, pos_y, 0, Qt::AlignCenter);
        qt_layout.addWidget(&qt_cancel_button, pos_y, 1, Qt::AlignCenter);

        if (qt_dialog_box.exec() == false || qt_okay_button.isChecked() == false) return;
        if (qt_transform_button.isChecked() == false && 
            qt_axis_button.isChecked() == false && 
            qt_new_transform_button.isChecked() == false && 
            qt_new_axis_button.isChecked() == false ) return;

        QStringList qt_frame_range_string = qt_frame_range_value.text().split("-", QString::SkipEmptyParts);

        switch (qt_frame_range_string.size())
        {
            case 0:
            {
                frame_start = frame_end = 0;
                break;
            }
            case 1:
            {
                frame_start = frame_end = qt_frame_range_string.at(0).toInt();
                break;
            }

            default:
            {
                frame_start = std::min(qt_frame_range_string.at(0).toInt(), qt_frame_range_string.at(1).toInt());
                frame_end   = std::max(qt_frame_range_string.at(0).toInt(), qt_frame_range_string.at(1).toInt());
                break;
            }
        }
        
        uint8_t do_transformer        = qt_transform_button.isChecked();
        uint8_t do_axis               = qt_axis_button.isChecked();
        uint8_t do_new_transformer    = qt_new_transform_button.isChecked();
        uint8_t do_new_axis           = qt_new_axis_button.isChecked();

        uint8_t transform_option      = qt_transform_option.currentIndex();
        uint8_t rotation_order_option = qt_rotation_order.currentIndex();

        ////////////////////////////////////////////////////////////
        // bake request points
        ////////////////////////////////////////////////////////////

        QProgressDialog qt_prograss_bar;
        qt_prograss_bar.setLabelText("baking points...");
        qt_prograss_bar.setMinimum(frame_start);
        qt_prograss_bar.setMaximum(frame_end + 1);
        qt_prograss_bar.setWindowModality(Qt::WindowModal);
        qt_prograss_bar.setMinimumDuration(0);

        std::vector<uint32_t> selected;
        selected.reserve(points.size);
        for(size_t index = 0, point_size = points.size; index < point_size ; ++index) {
            if (points.is_selected(index)) selected.push_back(index);
        }
        
        size_t baked_size = selected.size();
        int frame_span = frame_end - frame_start + 1;
        
        std::vector<float> transform_avg_knob_keyframes;
        std::vector<float> axis_avg_knob_keyframes;
        // std::vector<float> axis_avg_mat_keyframes;
        transform_avg_knob_keyframes.resize(frame_span * 4);
        axis_avg_knob_keyframes.resize(frame_span * 6);
        // axis_avg_mat_keyframes.resize(frame_span * 2);
        
        std::vector<std::vector<float>> transform_knob_keyframes;
        std::vector<std::vector<float>> axis_knob_keyframes;
        transform_knob_keyframes.resize(baked_size);
        axis_knob_keyframes.resize(baked_size);

        for (size_t p = 0; p < baked_size; ++p) {
            transform_knob_keyframes[p].resize(frame_span * 2); // allocate for two knobs => 2d translate 
            axis_knob_keyframes[p].resize(frame_span * 6); //  allocate for two knobs => 3d translate, 3d rotations
        }

        int knob_transfrom_x = 0;
        int knob_transfrom_y = knob_transfrom_x + frame_span;
        int knob_transfrom_z = knob_transfrom_y + frame_span;
        int knob_rotation_x  = knob_transfrom_z + frame_span;
        int knob_rotation_y  = knob_rotation_x  + frame_span;
        int knob_rotation_z  = knob_rotation_y  + frame_span;

        Points baked_points = points;

        for (int frame = frame_start; frame <= frame_end; ++frame)
        {
            qt_prograss_bar.setValue(frame);
            if (qt_prograss_bar.wasCanceled()) return;

            update_points(baked_points, (float)frame ,uiContext().view());

            float x_2d = 0;
            float y_2d = 0;

            float x_3d = 0;
            float y_3d = 0;
            float z_3d = 0;

            for(size_t p = 0; p < baked_size ; ++p)
            {
                size_t index = selected[p];

                if (do_transformer)
                {
                    transform_knob_keyframes[p][knob_transfrom_x] = baked_points.points_2d[index].x;
                    transform_knob_keyframes[p][knob_transfrom_y] = baked_points.points_2d[index].y;
                }

                if (do_axis)
                {
                    axis_knob_keyframes[p][knob_transfrom_x] = baked_points.points_3d[index].x;
                    axis_knob_keyframes[p][knob_transfrom_y] = baked_points.points_3d[index].y;
                    axis_knob_keyframes[p][knob_transfrom_z] = baked_points.points_3d[index].z;

                    float rx, ry, rz;
                    DD::Image::Matrix4 Rotation;
                    Rotation.setZAxis(baked_points.normals[index].norm);
                    Rotation.setXAxis(baked_points.normals[index].norm+3);
                    Rotation.setYAxis(Rotation.z_axis().cross(Rotation.x_axis()));
                    Rotation.setXAxis(Rotation.y_axis().cross(Rotation.z_axis()));
                    Rotation.getRotations((DD::Image::Matrix4::RotationOrder)rotation_order_option, rx, ry, rz);

                    axis_knob_keyframes[p][knob_rotation_x] = degrees(rx);
                    axis_knob_keyframes[p][knob_rotation_y] = degrees(ry);
                    axis_knob_keyframes[p][knob_rotation_z] = degrees(rz);
                }

                if (do_new_transformer | do_new_axis)
                {
                    x_2d += baked_points.points_2d[index].x;
                    y_2d += baked_points.points_2d[index].y;

                    x_3d += baked_points.points_3d[index].x;
                    y_3d += baked_points.points_3d[index].y;
                    z_3d += baked_points.points_3d[index].z;
                }
            }

            if (do_new_transformer | do_new_axis)
            {
                x_2d = x_2d / (float)baked_size;
                y_2d = y_2d / (float)baked_size;

                x_3d = x_3d / (float)baked_size;
                y_3d = y_3d / (float)baked_size;
                z_3d = z_3d / (float)baked_size;

                transform_avg_knob_keyframes[knob_transfrom_x] = x_2d;
                transform_avg_knob_keyframes[knob_transfrom_y] = y_2d;

                axis_avg_knob_keyframes[knob_transfrom_x] = x_3d ;
                axis_avg_knob_keyframes[knob_transfrom_y] = y_3d ;
                axis_avg_knob_keyframes[knob_transfrom_z] = z_3d ;

                // for 2d scale and rotation 
                DD::Image::OutputContext ocframe;
                ocframe.setFrame(frame + frame_start);
                DD::Image::CameraOp* camera = dynamic_cast<DD::Image::CameraOp*>(node_input(1,DD::Image::Op::EXECUTABLE,&ocframe));
                if (camera == nullptr) return;
                camera->validate();
                DD::Image::Matrix4 world = camera->imatrix();
                float scale = camera->projection().a11;
                float depth = world.a20 * x_3d + world.a21 * y_3d + world.a22 * z_3d + world.a23; 
                transform_avg_knob_keyframes[knob_transfrom_z] = - scale / depth;
            }

            ++knob_transfrom_x;
            ++knob_transfrom_y;
            ++knob_transfrom_z;
            ++knob_rotation_x;
            ++knob_rotation_y;
            ++knob_rotation_z;
        }
        qt_prograss_bar.done(0);

        ////////////////////////////////////////////////////////////
        // make nuke nodes
        ////////////////////////////////////////////////////////////

        if (do_transformer)
        {
            int center[2];
            center[0] = transform_option == 0 ? 0 : format_width;
            center[1] = transform_option == 0 ? 0 : format_height;

            int frame_span = frame_end - frame_start + 1;
            for(size_t point = 0; point < baked_size ; ++point)
            {
                std::stringstream Script;
                Script	<< "nukescripts.clear_selection_recursive();"
                        << "nuke.autoplace(nuke.createNode('Transform',"
                        << "'translate {";
                        for (size_t k = 0; k < 2; ++k)
                        {
                            size_t frame     = frame_span * k;
                            size_t frame_end = frame_span + frame;

                            Script  << "{curve R x" <<frame_start<<" ";
                            for (; frame < frame_end; ++frame) 
                            {
                                Script << transform_knob_keyframes[point][frame] - center[k] << " ";
                            }
                            Script  << "} ";
                        }
                Script  << "}";
                Script  << " scale  {1}";
                Script  << " rotate {0}";
                Script  << " center {"<<center[0]<<" "<<center[1]<<"}";
                Script  << " label {WhatsThePoint \\nframes "<<frame_start<<" - "<<frame_end<<"}";
                Script  << "'));";
                script_command(Script.str().c_str(),true,false);
                script_unlock();
            }
        }

        if (do_axis)
        {
            const char* const knobs_name[] = 
            {
                "translate {",
                "rotate {",
            };

            const char* const rotations_name[] = 
            {
                "XYZ",
                "XZY",
                "YXZ",
                "YZX",
                "ZXY",
                "ZYX",
            };

            int frame_span = frame_end - frame_start + 1;
            for(size_t point = 0, points_size = baked_size ; point < points_size ; ++point)
            {
                std::stringstream Script;
                Script	<< "nukescripts.clear_selection_recursive();"
                        << "nuke.autoplace(nuke.createNode('Axis','";
                        for (size_t knob = 0; knob < 2; ++knob)
                        {
                            Script << knobs_name[knob];
                            size_t indices     = knob * 3;
                            size_t indices_end = indices + 3;
                            for (; indices < indices_end; ++indices)
                            {
                                size_t frame     = frame_span * indices;
                                size_t frame_end = frame_span + frame;

                                Script  << "{curve R x" <<frame_start<<" ";
                                for (; frame < frame_end; ++frame) 
                                {
                                    Script << axis_knob_keyframes[point][frame]<< " ";
                                }
                                Script  << "} ";
                            }
                            Script  << "} ";
                        }
                Script  << " rot_order " << rotations_name[rotation_order_option];
                Script  << " label {WhatsThePoint \\nframes "<<frame_start<<" - "<<frame_end<<"}";
                Script  << "'));";
                script_command(Script.str().c_str(),true,false);
                script_unlock();
            }            
        }

        if (do_new_transformer)
        {
            if (baked_size <= 1) return;

            float scale_frame_ref = 1.0f / transform_avg_knob_keyframes[0];

            std::stringstream Script;
            Script	<< "nukescripts.clear_selection_recursive();"
                    << "nuke.autoplace(nuke.createNode('Transform',"
                    << "'translate {{curve R x" <<frame_start<<" ";
                    for (size_t frame = 0; frame < frame_span; ++frame) 
            Script  << transform_avg_knob_keyframes[frame]<< " ";            
            Script  << "} {curve R x" <<frame_start<<" ";
                    for (size_t frame = 0; frame < frame_span; ++frame) 
            Script  << transform_avg_knob_keyframes[frame + frame_span]<< " ";            
            Script  << "}}";

            Script  << " scale {{curve R x" <<frame_start<<" ";
                    for (size_t frame = 0; frame < frame_span; ++frame) 
            Script  << transform_avg_knob_keyframes[frame + frame_span * 2]<< " ";
            Script  << "}}";

            // Script  << " rotate {{curve R x" <<frame_start<<" ";
            //         for (size_t frame = 0; frame < frame_span; ++frame) 
            // Script  << average_rotat_knob_keyframes[frame]<< " ";
            // Script  << "}}";
            
            Script  << " center {0 0}";
            Script  << " label {WhatsThePoint \\nframes "<<frame_start<<" - "<<frame_end<<"}";
            Script  << "'));";
            script_command(Script.str().c_str(),true,false);
            script_unlock();
        }   

        if (do_new_axis)
        {
            if (baked_size <= 1) return;

            const char* const rotations_name[] = 
            {
                "XYZ",
                "XZY",
                "YXZ",
                "YZX",
                "ZXY",
                "ZYX",
            };

            std::stringstream Script;
            Script	<< "nukescripts.clear_selection_recursive();"
                    << "nuke.autoplace(nuke.createNode('Axis',"
                    << "'translate {{curve R x" <<frame_start<<" ";
                    for (size_t frame = 0, frameEnd = frame_span; frame < frameEnd; ++frame) 
            Script  << axis_avg_knob_keyframes[frame]<< " ";
            Script  << "} {curve R x" <<frame_start<<" ";
                    for (size_t frame = frame_span, frameEnd = frame + frame_span; frame < frameEnd; ++frame) 
            Script  << axis_avg_knob_keyframes[frame]<< " ";
            Script  << "} {curve R x" <<frame_start<<" ";
                    for (size_t frame = frame_span*2, frameEnd = frame + frame_span; frame < frameEnd; ++frame) 
            Script  << axis_avg_knob_keyframes[frame]<< " ";
            Script  << "}}";

            Script  << " rotate {{curve R x" <<frame_start<<" ";
                    for (size_t frame = frame_span*3, frameEnd = frame + frame_span; frame < frameEnd; ++frame) 
            Script  << axis_avg_knob_keyframes[frame]<< " ";
            Script  << "} {curve R x" <<frame_start<<" ";
                    for (size_t frame = frame_span*4, frameEnd = frame + frame_span; frame < frameEnd; ++frame) 
            Script  << axis_avg_knob_keyframes[frame]<< " ";
            Script  << "} {curve R x" <<frame_start<<" ";
                    for (size_t frame = frame_span*5, frameEnd = frame + frame_span; frame < frameEnd; ++frame) 
            Script  << axis_avg_knob_keyframes[frame]<< " ";
            Script  << "}}";

            Script  << " rot_order " << rotations_name[rotation_order_option];
            Script  << " label {WhatsThePoint \\nframes "<<frame_start<<" - "<<frame_end<<"}";
            Script  << "'));";
            script_command(Script.str().c_str(),true,false);
            script_unlock();                
        }
    }
};



#define STRINGIFY(v) #v
#define TOSTRING(v) STRINGIFY(v)
#define NUKE_INFO_TAB	"author :\tEyal Shirazi\n" \
                        "version:\t" \
                        TOSTRING(VERSION_MAJOR) "." TOSTRING(VERSION_MINOR) \
                        "\n" \
                        "update :\t" \
                        __DATE__ \
                        "\n\n\n" \
                        HELP;

const char* const WhatsThePoint::kHelp = HELP;
const char* const WhatsThePoint::kInfo = NUKE_INFO_TAB;
const char* const WhatsThePoint::kOpTypeList[] = {
    #define EXPEND_OP_TYPE_CHAR(name) #name,
    OP_TYPE_LIST(EXPEND_OP_TYPE_CHAR)
    NULL };

static DD::Image::Iop* buildWhatsThePointOp(Node* node) { return new WhatsThePoint(node); }
const DD::Image::Iop::Description WhatsThePoint::d("WhatsThePoint", 0, buildWhatsThePointOp);

void PointsListKnob::to_script(std::ostream &os, const DD::Image::OutputContext* c, bool quote) const
{
    ((WhatsThePoint*)op())->to_script(os);
}

bool PointsListKnob::from_script(const char *v)
{
    ((WhatsThePoint*)op())->from_script(v);
    return true;
}

bool PointsListKnob::keypress_detection_cb(DD::Image::ViewerContext* ctx)
{
    
    if ((ctx->state() == (DD::Image::SHIFT + DD::Image::ALT)) & (ctx->event() == DD::Image::ViewerEvent::PUSH))
    {
        Point2D pos = screen_mouse_to_image(ctx);
        ((WhatsThePoint*)op())->add_clicked_position(pos.x, pos.y);
        return true;
    }
    return false;
}

bool PointsListKnob::hit_detection_cb(DD::Image::ViewerContext* ctx)
{
    
    switch (ctx->event()) 
    {
        case DD::Image::ViewerEvent::PUSH:
        {
            user_pen = screen_mouse_to_image(ctx);

            if ((ctx->state() == (DD::Image::SHIFT + DD::Image::ALT)))
            {
                ((WhatsThePoint*)op())->add_clicked_position(user_pen.x, user_pen.y);
                return true;
            }

            // get hit index
            float distMin = 10000000;
            for (size_t i=0; i<points.size; ++i)
            {
                float distx = points.points_2d[i].x - user_pen.x;
                float disty = points.points_2d[i].y - user_pen.y;
                float dist = distx * distx + disty * disty;

                if (dist < distMin)
                {
                    distMin = dist;
                    hit_deteced_index = i;
                }
            }

            hit_deteced_state = points.is_selected(hit_deteced_index);
            if (hit_deteced_state == false) points.deselect_all_points();
            points.set_selected(hit_deteced_index, 1);

            hit_deteced = true;
            return true;
        }
        case DD::Image::ViewerEvent::DRAG:
        {
            if ((ctx->state() == (DD::Image::SHIFT + DD::Image::ALT))) return false;

            Point2D delta = screen_mouse_to_image(ctx);
            delta.x -= user_pen.x;
            delta.y -= user_pen.y;

            for (size_t index = 0; index<points.size; ++index)
            {
                if (points.is_selected(index))
                {
                    points.points_2d[index].x += delta.x;
                    points.points_2d[index].y += delta.y;
                }
            } 
            
            user_pen = screen_mouse_to_image(ctx);
            return true;
        }
        case DD::Image::ViewerEvent::RELEASE:
        {
            if ((ctx->state() == (DD::Image::SHIFT + DD::Image::ALT))) return false;

            ((WhatsThePoint*)op())->rebuild_point();
            points.set_selected(hit_deteced_index, hit_deteced_state);
            hit_deteced = false;
            return true;
        }			
    }

    return false;
}

PointsListWidget::~PointsListWidget()
{
    
    if(knob) 
    {
        if (stabilize.isChecked())
        {
            WhatsThePoint* parent = (WhatsThePoint*)knob->op();
            ++parent->update_counter;
            parent->asapUpdate();
            knob->changed();
        }

        knob->removeCallback( WidgetCallback, this );
        knob = nullptr;
    }
    
}

inline void PointsListWidget::update_stabilize()
{
    
    WhatsThePoint* parent = (WhatsThePoint*)knob->op();
    ++parent->update_counter;
    parent->asapUpdate();

    knob->redraw();
    
}


inline void PointsListWidget::get_selected_rows()
{
    
    QItemSelectionModel *tableSelect = table.selectionModel();
    size_t size = points.size;
    for (size_t index = 0; index < size; ++index)
    {
        points.set_selected(index, tableSelect->isRowSelected(index, QModelIndex()));
    }

    if (stabilize.isChecked())
    {
        WhatsThePoint* parent = (WhatsThePoint*)knob->op();
        ++parent->update_counter;
        parent->asapUpdate();
        knob->changed();
    }
    
    knob->redraw();
    
}

inline void PointsListWidget::select_rows()
{
    
    table.blockSignals(true);
    table.setSelectionMode(QAbstractItemView::MultiSelection);

    table.clearSelection();

    size_t size = points.size;
    for (size_t index = 0; index < size; ++index)
    {
        if (points.is_selected(index)) table.selectRow(index);
    } 

    table.setSelectionMode(QAbstractItemView::ExtendedSelection);
    table.blockSignals(false);

    if (stabilize.isChecked())
    {
        WhatsThePoint* parent = (WhatsThePoint*)knob->op();
        ++parent->update_counter;
        parent->asapUpdate();
        knob->changed();
    }

    knob->redraw();
    
}


inline void PointsListWidget::update_widget_cell()
{
    table.blockSignals(true);
    WhatsThePoint* parent = (WhatsThePoint*)knob->op();

    size_t size = points.size;
    for (size_t index = 0; index < size; ++index)
    {
        if (points.is_selected(index)) parent->update_table_item(table, index);
    }

    table.blockSignals(false);
    knob->redraw();
}

void PointsListWidget::upade_name_cell(int y, int x)
{
    if (x == 0)
    {
        
        table.blockSignals(true);

        QTableWidgetItem* cell = table.item(y,0);
        std::string name = cell->text().toStdString();

        size_t spaces = name.find_last_not_of(' ') + 1;
        if (spaces < name.length()) name.resize(spaces);

        std::replace(name.begin(), name.end(), ' ', '_');	

        points.name[y] = name;
        ((WhatsThePoint*)knob->op())->update_table_item(table, y);

        table.blockSignals(false);
        knob->redraw();
        
    }
}

inline void PointsListWidget::add_table_item(int index)
{
    
    WhatsThePoint* parent = (WhatsThePoint*)knob->op();

    table.insertRow(index);

    size_t columns_size = table.columnCount();
    for (size_t i=0 ; i<columns_size ; ++i)
    {
        table.setItem(index, i, new QTableWidgetItem());
        table.item(index,i)->setFlags(table.item(index,i)->flags() & ~Qt::ItemIsEditable);
    }
    table.item(index,0)->setFlags(table.item(index,0)->flags() | Qt::ItemIsEditable);

    parent->update_table_item(table, index);
    
}

void PointsListWidget::set_widget_callbacks()
{
    
    WhatsThePoint* parent = (WhatsThePoint*)knob->op();

    connect(&remove, SIGNAL(clicked()), this, SLOT(remove_widget_cb()));
    connect(&clear,  SIGNAL(clicked()), this, SLOT(clear_widget_cb()));
    connect(&label,  &QPushButton::clicked, [=](){knob->redraw();});

    connect(&add,    &QPushButton::clicked, [=](){parent->add_clicked();});
    connect(&remove, &QPushButton::clicked, [=](){parent->remove_clicked();});
    connect(&clear,  &QPushButton::clicked, [=](){parent->clear_clicked();});
    connect(&bake,   &QPushButton::clicked, [=](){parent->bake_points();});

    connect(&stabilize, SIGNAL(clicked()), this, SLOT(update_stabilize()));

    connect(&table, SIGNAL(itemSelectionChanged()), this, SLOT(get_selected_rows()));
    connect(&table, SIGNAL(cellChanged(int,int)), this, SLOT(upade_name_cell(int,int)));
    
}

void PointsListWidget::set_table_layout()
{
    
    WhatsThePoint* parent = (WhatsThePoint*)knob->op();

    uint32_t columns = 0;
    QString labels;

    switch (parent->OpTypeValue)
    {
        case kOpTypeEnum::kOpType_geo:    { columns = 2; labels = QString("name;geometry");        break; }
        case kOpTypeEnum::kOpType_plate:  { columns = 5; labels = QString("name;x;y;depth;frame"); break; }
        case kOpTypeEnum::kOpType_cloud:  { columns = 4; labels = QString("name;x;y;z");           break; }
        case kOpTypeEnum::kOpType_vector: { columns = 4; labels = QString("name;x;y;frame");       break; }
    }
 
    table.setSelectionMode(QAbstractItemView::ExtendedSelection);
    table.setSelectionBehavior(QAbstractItemView::SelectRows);
    table.setSortingEnabled(false);

    table.verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table.verticalHeader()->setDefaultSectionSize(20);
    table.setWordWrap(false);
    table.setTextElideMode(Qt::ElideLeft);
    
    table.horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table.horizontalHeader()->setStretchLastSection(true);
    
    table.setColumnCount(columns);
    table.setHorizontalHeaderLabels(labels.split(";"));
    
}
