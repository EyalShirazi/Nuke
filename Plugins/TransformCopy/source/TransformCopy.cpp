//*******************************************************************//
// Author   : Eyal Shirazi
// Created  : 01/05/2017
// Modified : 07/09/2023
// 
// TODO (Eyal) ::
// * bring back proxy scale
// * Reference needs to be sampled after retime
// * opengl viewer widget are in the wrong position on new resolution (?)
// * In case a TransfromCopy is connected to transform pipe (input 1), and it is being disabled so does our transformation, but I prefer it to always give out transformation info
//*******************************************************************//

#include "DDImage/Transform.h"
#include "DDImage/CameraOp.h"
#include "DDImage/Knobs.h"
#include "DDImage/Format.h"

#include <QtWidgets/QDialog>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QLabel>
#include <QtWidgets/QComboBox>

#include "ToggleButtonKnob.h"

// #define FASTMB
#define VERSION_MAJOR 2
#define VERSION_MINOR 00

#define HELP    "TransfromCopy will apply a transformation from one node to another image,\n" \
                "It is possible to mix different formats (transformation/input/output resolution).\n" \
                "\n" \
                "inverse knob   : inverse the transformation (i.e. in case of stabilization).\n" \
                "reference knob : setting the transformation to match a frame.\n" \
                "retime knob    : retime the transformation (i.e. to match retime plates).\n" \
                "\n" \
                "Tip: hold ctrl + click on reference/retime button will set frame number to current frame.\n";


void printM33(DD::Image::Matrix4 mat, const char* name);
void printM44(DD::Image::Matrix4 mat, const char* name);

class TransformCopy final : public DD::Image::Transform
{
	ToggleButtonKnob* kReference{nullptr};
	ToggleButtonKnob* kRetime{nullptr};

    DD::Image::Knob* kReferenceFrame{nullptr};
    DD::Image::Knob* kRetimeFrame{nullptr};
    DD::Image::Knob* kFormat{nullptr};

    TransformCopy* transformCopyOp{nullptr};
    DD::Image::Transform* transformOp{nullptr};
    DD::Image::CameraOp* cameraOp{nullptr};

    DD::Image::Matrix4 matReference;
    DD::Image::Matrix4 matFormatOut;
    DD::Image::Matrix4 matFormatIn;
    DD::Image::Matrix4 matTransFormatOut;
    DD::Image::Matrix4 matTransFormatIn;

    DD::Image::FormatPair kOutputFormat;
    DD::Image::Box bboxOut;

	float kReferenceFrameValue{0};
	float kRetimeFrameValue{0};
    int kExpandBBox{0};
    int kBBoxMode{0};
	int kResizeTypeValue{1};


#define BBOX_TYPE_LIST(ENTRY) \
    ENTRY(full) \
    ENTRY(clipped) \
    ENTRY(format) \

    static const char* const kBBoxModeList[];
    enum kBBoxModeEnum {
        #define EXPEND_BBOX_TYPE_ENUM(name)   BBoxType_##name,
        BBOX_TYPE_LIST(EXPEND_BBOX_TYPE_ENUM)
        kBBoxModeEnumSize
    };

#define RESIZE_TYPE_LIST(ENTRY) \
    ENTRY(none) \
    ENTRY(width) \
    ENTRY(height) \
    ENTRY(distort) \
    // ENTRY(fit) \
    // ENTRY(fill) \

    static const char* const kResizeTypeList[];
    enum kResizeTypeEnum {
        #define EXPEND_RESIZE_TYPE_ENUM(name)   ResizeType_##name,
        RESIZE_TYPE_LIST(EXPEND_RESIZE_TYPE_ENUM)
        kResizeTypeEnumSize
    };

    bool kInverseValue{0};
    bool kReferenceValue{0};
    bool kRetimeValue{0};
#ifdef FASTMB
    bool kFastValue{0};
#endif


public:

    TransformCopy(Node* node) 
    : DD::Image::Transform(node)
    {
		kOutputFormat.format(nullptr);
    }
    
    ~TransformCopy() 
    {}

    static const char* const kHelp;
    static const char* const kInfo;
    static const DD::Image::Transform::Description d;
    const char* Class() const override { return d.name; }
    const char* node_help() const override { return kHelp; }
  	int maximum_inputs() const override { return 2; }
	int minimum_inputs() const override { return 2; }

    void append(DD::Image::Hash& hash) override
    {
        hash.append(__DATE__);
        hash.append(__TIME__);

        hash.append(kInverseValue);
        hash.append(kReferenceValue);
        hash.append(kRetimeValue);
        hash.append(kReferenceFrameValue);
        hash.append(kRetimeFrameValue);

        DD::Image::Transform::append(hash);
    }

    void knobs(DD::Image::Knob_Callback f) override   
    {
        CustomKnob1(ToggleButtonKnob, f, &kInverseValue, "inverse");
		SetFlags(f, DD::Image::Knob::ENDLINE);

        kReference = CustomKnob1(ToggleButtonKnob, f, &kReferenceValue, "reference");
        kReferenceFrame = Float_knob(f, &kReferenceFrameValue, "referenceFrame", "");
		ClearFlags(f, DD::Image::Knob::SLIDER);
		SetFlags(f, DD::Image::Knob::ENDLINE);

        kRetime = CustomKnob1(ToggleButtonKnob, f, &kRetimeValue, "retime");
		kRetimeFrame = Float_knob(f, &kRetimeFrameValue, "retimeFrame", "");
		ClearFlags(f, DD::Image::Knob::SLIDER);
		SetFlags(f, DD::Image::Knob::ENDLINE);

        Button(f, "bake"); 
        SetFlags(f, DD::Image::Knob::STARTLINE);

		Divider(f);

    	Transform::knobs(f);

		Divider(f);

		Enumeration_knob(f, &kResizeTypeValue, kResizeTypeList, "resize", "resize type");

        Enumeration_knob(f, &kBBoxMode, kBBoxModeList, "boundingBox", "bounding box");
		Int_knob(f, &kExpandBBox, "bboxexpand", "");
		ClearFlags(f, DD::Image::Knob::SLIDER);

		kFormat = Format_knob(f, &kOutputFormat, "format");
        
        // Button(f, "root"); 
        // Button(f, "input"); 
        // Button(f, "transform"); 
        
        Tab_knob(f, "info");

#ifdef FASTMB
        CustomKnob1(ToggleButtonKnob, f, &kFastValue, "fast mb");
#endif

		Divider(f);
        Text_knob(f, kInfo);
    }

	int knob_changed(DD::Image::Knob* k) override
    {
		if(k == kReference)
		{
            if ((QGuiApplication::keyboardModifiers().testFlag(Qt::ControlModifier)))
            {
                kReferenceFrame->clear_animated();
                kReferenceFrame->set_value(outputContext().frame());
            }

			return 1;
		}

		if(k == kRetime)
		{
            if ((QGuiApplication::keyboardModifiers().testFlag(Qt::ControlModifier)))
            {
                kRetimeFrame->clear_animated();
                kRetimeFrame->set_value(outputContext().frame());
            }
			return 1;
		}

        if (k->name() == "bake") 
        {
            qt_bake_panel();
            return 1;
        }

        // if (k->name() == "root") 
        // {
        //     std::cout<<"root\n";
        //     return 1;
        // }
        // if (k->name() == "input") 
        // {
        //     std::cout<<"input\n";
        //     return 1;
        // }
        // if (k->name() == "transform") 
        // {
        //     std::cout<<"transform\n";
        //     return 1;
        // }
		return 0;
    }

	const char* input_label(int n, char*) const override
	{	
		switch (n) 
		{
			case 0: return "image";
			case 1: return "transform";
			default: return "";
		}
	}

	bool test_input(int input, DD::Image::Op *op) const override
	{
		if (input==1)
        {
            return
            (dynamic_cast<DD::Image::Transform*>(op) != nullptr) ||
            (dynamic_cast<DD::Image::CameraOp*>(op)  != nullptr);
        }
        return DD::Image::Transform::test_input(input, op);
	}

    inline void clean_and_inverse_mat(DD::Image::Matrix4& mat)
    {
        mat.a02 = mat.a12 = mat.a32 = 0;
        mat.a20 = mat.a21 = mat.a23 = 0;
        mat.a22 = 1;
        mat = mat.inverse();
    }

    void _validate(bool for_real) override
    {
        if (input(1)==nullptr)
        {
            matrix_.makeIdentity();
    		DD::Image::Transform::_validate(for_real);
            return;
        }

        set_transformation_data();

        if(kReferenceValue)
        {
            DD::Image::OutputContext ocReferenceTime;
			ocReferenceTime = outputContext();
			ocReferenceTime.setFrame(kReferenceFrameValue);

            matReference.makeIdentity();
            get_homography(ocReferenceTime, matReference);
            clean_and_inverse_mat(matReference);
        }

        matrixAt(outputContext(), matrix_);
		DD::Image::Transform::_validate(for_real);

		info_.full_size_format(*kOutputFormat.fullSizeFormat());
		info_.format(*kOutputFormat.format());
    
        switch(kBBoxMode)
        {
            case (BBoxType_full):
                break;
            case (BBoxType_clipped):
            {
                DD::Image::Box boxExpand{*kOutputFormat.format()};
                boxExpand.expand(kExpandBBox);
                info_.intersect(boxExpand);
                break;
            }
            case (BBoxType_format):
            {
                info_.setBox(*kOutputFormat.format());
                break;
            }
            default:
                break;
        }
    }

    void set_transformation_data()
    {
        bboxOut.clear();
        transformCopyOp = this;
        transformOp = nullptr;
        cameraOp = nullptr;

        if (input(0)) input(0)->validate();
        if (input(1)) input(1)->validate();

        DD::Image::Op* node = input(1);

		while (dynamic_cast<TransformCopy*>(node))
		{
            transformCopyOp = dynamic_cast<TransformCopy*>(node);
			node = node->input(1);
		}

        if (node == nullptr) return;

        transformCopyOp->validate();
        
        // Homography matrices to convert between different resolutions
        matFormatOut.makeIdentity();
        matFormatIn.makeIdentity();
        DD::Image::CameraOp::to_format(matFormatOut, kOutputFormat.fullSizeFormat());
        DD::Image::CameraOp::from_format(matFormatIn, &input0().full_size_format());

        if (dynamic_cast<DD::Image::CameraOp*>(node) != nullptr)
        {
            cameraOp = dynamic_cast<DD::Image::CameraOp*>(node);
            cameraOp->validate();
        }

        if (dynamic_cast<DD::Image::Transform*>(node) != nullptr)
        {
            transformOp = dynamic_cast<DD::Image::Transform*>(node);
            transformOp->validate();

            float scalex = 1;
            float scaley = 1;
            switch (kResizeTypeValue)
            {
                case ResizeType_none:
                {
                    // scale image to original size in relationship to the output resolution
                    DD::Image::Matrix4 matFormatTest;
                    matFormatTest.makeIdentity();

                    DD::Image::CameraOp::to_format(matFormatTest, kOutputFormat.fullSizeFormat());
                    DD::Image::CameraOp::from_format(matFormatTest, &transformOp->full_size_format());
                    // homography
                    DD::Image::CameraOp::to_format(matFormatTest, &transformOp->concat_input()->input_format());
                    DD::Image::CameraOp::from_format(matFormatTest, &input0().full_size_format());

                    scalex = matFormatTest.inverse().a11;
                    scaley = matFormatTest.inverse().a11;
                    break;
                }
                case ResizeType_height:
                case ResizeType_distort:
                {
                    // (trans height / (trans width * trans aspect)) / (input height / (input width * input aspect))
                    float trW = transformOp->concat_input()->input_format().width();
                    float trH = transformOp->concat_input()->input_format().height();
                    float trA = transformOp->concat_input()->input_format().pixel_aspect();
                    float inW = input0().full_size_format().width();
                    float inH = input0().full_size_format().height();
                    float inA = input0().full_size_format().pixel_aspect();

                    // scale image evenly to fit height of transformion
                    scaley = (trH * (inW * inA)) / (inH * (trW * trA));
                    scalex = kResizeTypeValue == ResizeType_distort ? 1 : scaley;
                    break;
                }
                default: break;
            }

            matTransFormatOut.makeIdentity();
            matTransFormatIn.makeIdentity();

            DD::Image::CameraOp::from_format(matTransFormatOut, &transformOp->full_size_format());
            DD::Image::CameraOp::to_format(matTransFormatIn, &transformOp->concat_input()->input_format());
            matTransFormatIn.scale(scalex, scaley, 1);
        }
    }

    void get_homography(const DD::Image::OutputContext& context, DD::Image::Matrix4& homography)
    {
        homography.makeIdentity();
        DD::Image::OutputContext retimeContext = context;

        if (kRetimeValue)
        {
            float retimeSample{0};
            DD::Image::Hash hash;
            kRetimeFrame->store(DD::Image::FloatPtr , &retimeSample, hash, context);
            retimeContext.setFrame(retimeSample);
        } 

        if (cameraOp != nullptr)
        {
            DD::Image::Hash hash;

            DD::Image::Vector2 win_translate;
            DD::Image::Vector2 win_scale;
            double win_roll;

            cameraOp->knob("win_translate")->store(DD::Image::FloatPtr , &win_translate.x, hash, retimeContext);
            cameraOp->knob("win_scale")    ->store(DD::Image::FloatPtr , &win_scale.x    , hash, retimeContext);
            cameraOp->knob("winroll")      ->store(DD::Image::DoublePtr, &win_roll       , hash, retimeContext);            

            homography.rotate(radians(win_roll));
            homography.scale(1.0f/win_scale.x, 1.0f/win_scale.y,1);
            homography.translate(-win_translate.x, -win_translate.y,0);
        }

        if (transformOp != nullptr)
        {
#ifdef FASTMB
            if (kFastValue)
            {
                DD::Image::Transform* parent = transformOp;
                do
                {
                    DD::Image::Matrix4 parMat;
                    parMat.makeIdentity();
                    parent->matrixAt(retimeContext, parMat);
                    homography *= parMat;
                } 
                while (parent = dynamic_cast<DD::Image::Transform*>(parent->input(0)));
                homography = matTransFormatOut * homography * matTransFormatIn;
            }
            else
#endif
            {
                DD::Image::Transform* transform = dynamic_cast<DD::Image::Transform*>(transformCopyOp->node_input(1, Op::EXECUTABLE_SKIP, &retimeContext)); // EXECUTABLE_INPUT
                if (transform == nullptr) return;
                transform->validate();
                homography = matTransFormatOut * transform->concat_matrix() * matTransFormatIn;              
            }
        }
    }

    void matrixAt(const DD::Image::OutputContext& context, DD::Image::Matrix4& out) override
    {
        DD::Image::Matrix4 homography;
        get_homography(context, homography);

        // reference transformation
        if (kReferenceValue) homography = homography * matReference;

        // inverse transformation
        if (kInverseValue) clean_and_inverse_mat(homography);

        // scale transformation to resolution
        out = matFormatOut * homography * matFormatIn;
    }

    void qt_bake_panel()
    {
        if (input(0) == nullptr) return;

        set_transformation_data();
    
        if (cameraOp == nullptr && transformOp == nullptr) return;

        script_command("nuke.root().firstFrame()",true,true);
        int frameStart = atoi(script_result(true));
        script_unlock();

        script_command("nuke.root().lastFrame()",true,true);
        int frameEnd = atoi(script_result(true));
        script_unlock();

        QDialog DialogQT;

        DialogQT.setWindowTitle("bake");
        DialogQT.setWindowFlags(Qt::Dialog);
    
        QPushButton OkButtonQT("Ok");
        OkButtonQT.setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
        OkButtonQT.setCheckable(true);
        QObject::connect(&OkButtonQT, &QPushButton::clicked, &DialogQT, &QDialog::accept);

        QPushButton CancelButtonQT("Cancel");
        CancelButtonQT.setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
        CancelButtonQT.setCheckable(true);
        QObject::connect(&CancelButtonQT, &QPushButton::clicked, &DialogQT, &QDialog::reject);

        QComboBox BakeFormat;
        BakeFormat.addItem("corner pin points");
        BakeFormat.addItem("corner pin matrix");

        QLabel FrameRangeLabelQT(QObject::tr("frame range:"));
        QLineEdit FrameRangeNumbersQT;
        FrameRangeNumbersQT.setText(QString::number(frameStart)+"-"+QString::number(frameEnd));

        QGridLayout GridLayoutQT(&DialogQT);
        GridLayoutQT.addWidget(&FrameRangeLabelQT, 1, 0);
        GridLayoutQT.addWidget(&FrameRangeNumbersQT, 1, 1);
        GridLayoutQT.addWidget(&BakeFormat, 2, 0);
        GridLayoutQT.addWidget(&OkButtonQT, 3, 0);
        GridLayoutQT.addWidget(&CancelButtonQT, 3, 1);

        if (DialogQT.exec() == false) return;

        QStringList RangeListQT = FrameRangeNumbersQT.text().split("-", QString::SkipEmptyParts);

        switch (RangeListQT.size())
        {
            case 0:
                frameStart = frameEnd = 0;
                break;
            case 1:
                frameStart = RangeListQT.at(0).toInt();
                frameEnd = frameStart + 1;
                break;
            default:
                frameStart = std::min(RangeListQT.at(0).toInt(), RangeListQT.at(1).toInt());
                frameEnd  = std::max(RangeListQT.at(0).toInt(), RangeListQT.at(1).toInt());
                break;
        }

        if (BakeFormat.currentIndex() == 0)
        {
            bake_points(frameStart, frameEnd);
        }
        else
        {
            bake_matrix(frameStart, frameEnd);
        }
    }

    void bake_points(int frameStart, int frameEnd)
    {
        enum KNOBKEY
        {
            KNOBKEY_X0, KNOBKEY_Y0, 
            KNOBKEY_X1, KNOBKEY_Y1, 
            KNOBKEY_X2, KNOBKEY_Y2, 
            KNOBKEY_X3, KNOBKEY_Y3, 
            KNOBKEY_SIZE,
        };

        std::vector<std::vector<float>> matrixKnobKeys;
        matrixKnobKeys.resize(KNOBKEY_SIZE);
        for (int k = 0; k < KNOBKEY_SIZE; ++k)
            matrixKnobKeys[k].reserve(frameEnd - frameStart + 1);

        DD::Image::OutputContext context = outputContext();
        for (int f = frameStart; f < frameEnd; ++f)
        {
            context.setFrame((double)f);

            DD::Image::Matrix4 mat;
            matrixAt(context, mat);

            float x = 0;
            float y = 0;
            float r = kOutputFormat.fullSizeFormat()->width();
            float t = kOutputFormat.fullSizeFormat()->height();

            DD::Image::Vector4 v0 = mat.transform({ x, y, 0, 1}).divide_w();
            DD::Image::Vector4 v1 = mat.transform({ r, y, 0, 1}).divide_w();
            DD::Image::Vector4 v2 = mat.transform({ r, t, 0, 1}).divide_w();
            DD::Image::Vector4 v3 = mat.transform({ x, t, 0, 1}).divide_w();

            matrixKnobKeys[KNOBKEY_X0].push_back(v0.x);
            matrixKnobKeys[KNOBKEY_Y0].push_back(v0.y);
            matrixKnobKeys[KNOBKEY_X1].push_back(v1.x);
            matrixKnobKeys[KNOBKEY_Y1].push_back(v1.y);
            matrixKnobKeys[KNOBKEY_X2].push_back(v2.x);
            matrixKnobKeys[KNOBKEY_Y2].push_back(v2.y);
            matrixKnobKeys[KNOBKEY_X3].push_back(v3.x);
            matrixKnobKeys[KNOBKEY_Y3].push_back(v3.y);
        }
        
        const char* const knobsName[KNOBKEY_SIZE + 1] = 
        {
            // knob::
            // to1 {KNOBKEY_X0 KNOBKEY_Y0}
            // to2 {KNOBKEY_X1 KNOBKEY_Y1}
            // to3 {KNOBKEY_X2 KNOBKEY_Y2}
            // to4 {KNOBKEY_X3 KNOBKEY_Y3}

            "'to1 {",
            " ",
            " } to2 {",
            " ",
            " } to3 {",
            " ",
            " } to4 {",
            " ",
            " } ",
        };

        int w = kOutputFormat.fullSizeFormat()->w();
        int h = kOutputFormat.fullSizeFormat()->h();

        std::stringstream Script;
        Script	<< "nukescripts.clear_selection_recursive();"
                << "nuke.autoplace(nuke.createNode('CornerPin2D',";
                for (int k = 0; k < KNOBKEY_SIZE; ++k)
                {
                    Script  << knobsName[k];
                    Script  << "{curve R x" <<frameStart<<" ";
                    for (int f = 0, frameRange = frameEnd-frameStart; f < (frameRange); ++f) 
                    {
                        Script << matrixKnobKeys[k][f] << " ";
                    }; 
                    Script  <<  "}";
                }
        Script  << knobsName[KNOBKEY_SIZE];
        Script  << " label {TransformCopy \\nframes "<<frameStart<<" - "<<frameEnd<<"}";
        Script  << " from1 {0 0} from2 {"<<w<<" 0} from3 {"<<w<<" "<<h<<"} from4 {0 "<<h<<"}";
        Script  << "'));";
        Script  << "nuke.autoplace(nuke.createNode('Reformat', '";
        Script  << "resize none ";
        Script  << "center false ";
        Script  << "format \""<<kOutputFormat.fullSizeFormat()->name()<<"\"'));";

        script_command(Script.str().c_str(),true,false);
        script_unlock();
	}

    void bake_matrix(int frameStart, int frameEnd)
    {
        enum KNOBKEY
        {
            KNOBKEY_A00, KNOBKEY_A01, KNOBKEY_A03,
            KNOBKEY_A10, KNOBKEY_A11, KNOBKEY_A13,
            KNOBKEY_A30, KNOBKEY_A31, KNOBKEY_A33,
            KNOBKEY_SIZE,
        };

        std::vector<std::vector<float>> matrixKnobKeys;
        matrixKnobKeys.resize(KNOBKEY_SIZE);
        for (int k = 0; k < KNOBKEY_SIZE; ++k)
            matrixKnobKeys[k].reserve(frameEnd - frameStart + 1);

        DD::Image::OutputContext context = outputContext();
        for (int f = frameStart; f < frameEnd; ++f)
        {
            context.setFrame((double)f);

            DD::Image::Matrix4 mat;
            matrixAt(context, mat);

            matrixKnobKeys[KNOBKEY_A00].push_back(mat.a00);
            matrixKnobKeys[KNOBKEY_A01].push_back(mat.a01);
            matrixKnobKeys[KNOBKEY_A03].push_back(mat.a03);
            matrixKnobKeys[KNOBKEY_A10].push_back(mat.a10);
            matrixKnobKeys[KNOBKEY_A11].push_back(mat.a11);
            matrixKnobKeys[KNOBKEY_A13].push_back(mat.a13);
            matrixKnobKeys[KNOBKEY_A30].push_back(mat.a30);
            matrixKnobKeys[KNOBKEY_A31].push_back(mat.a31);
            matrixKnobKeys[KNOBKEY_A33].push_back(mat.a33);
        }

        const char* const knobsName[KNOBKEY_SIZE + 1] = 
        {
            //  knob::
            //  "extra matrix" 1
            //  transform_matrix {
            //      {KNOBKEY_A00 KNOBKEY_A01 0 KNOBKEY_A03}
            //      {KNOBKEY_A10 KNOBKEY_A11 0 KNOBKEY_A13}
            //      {0 0 1 0}
            //      {KNOBKEY_A30 KNOBKEY_A31 0 KNOBKEY_A33}
            //    }

            "' \"extra matrix\" 1 transform_matrix {{",
            " ",
            " 0 ",
            "} {",
            " ",
            " 0 ",
            "} {0 0 1 0} {",
            " ",
            " 0 ",
            "}} ",
        };

        int frameRange = frameEnd-frameStart;
        std::stringstream Script;
        Script	<< "nukescripts.clear_selection_recursive();"
                << "nuke.autoplace(nuke.createNode('CornerPin2D',";
                for (int k = 0; k < KNOBKEY_SIZE; ++k)
                {
                    Script  << knobsName[k];
                    Script  << "{curve R x" <<frameStart<<" ";
                    for (int f = 0; f < (frameRange); ++f) 
                    {
                        Script << matrixKnobKeys[k][f] << " ";
                    }; 
                    Script  <<  "}";
                }
        Script  << knobsName[KNOBKEY_SIZE];
        Script  << " label {TransformCopy \\nframes "<<frameStart<<" - "<<frameEnd<<"}";
        Script  << "'));";
        Script  << "nuke.autoplace(nuke.createNode('Reformat', '";
        Script  << "resize none ";
        Script  << "center false ";
        Script  << "format \""<<kOutputFormat.fullSizeFormat()->name()<<"\"'));";

        script_command(Script.str().c_str(),true,false);
        script_unlock();
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

const char* const TransformCopy::kHelp = HELP;
const char* const TransformCopy::kInfo = NUKE_INFO_TAB;
const char* const TransformCopy::kResizeTypeList[] = {
    #define EXPEND_RESIZE_TYPE_CHAR(name) #name,
    RESIZE_TYPE_LIST(EXPEND_RESIZE_TYPE_CHAR)
    NULL };
const char* const TransformCopy::kBBoxModeList[] = {
    #define EXPEND_BBOX_TYPE_CHAR(name) #name,
    BBOX_TYPE_LIST(EXPEND_BBOX_TYPE_CHAR)
    NULL };

static DD::Image::Iop* buildTransformCopyOp(Node* node) { return new TransformCopy(node); }
const DD::Image::Iop::Description TransformCopy::d("TransformCopy", 0, buildTransformCopyOp);
