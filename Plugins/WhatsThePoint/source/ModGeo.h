#pragma once

#include "DDImage/Op.h"
#include "DDImage/CameraOp.h"
#include "DDImage/AxisOp.h"
#include "DDImage/Scene.h"

struct ModGeo
{
    struct Item
    {
        std::string geo;
        uint32_t obj{(uint32_t)-1};
        uint32_t p0{0};
        uint32_t p1{0};
        uint32_t p2{0};
        float u;
        float v;
        float depth{std::numeric_limits<float>::max()};
    };

    DD::Image::Op* parent;
    std::vector<Item> items;

    ModGeo(DD::Image::Op* _parent) : parent{_parent} {}

    void to_script(std::ostream &os, Points& points)
    {
        
        os.precision(10);
        
        // if (points.size != items.size()) return;
        
        os<<"{ "<<items.size();
        for (size_t i=0; i<items.size(); ++i)
        {	
            os	<< "\n\t";
            os	<< points.name[i] <<" ";
            os	<< items[i].geo   <<" ";
            os	<< items[i].obj   <<" ";
            os	<< items[i].p0    <<" ";
            os	<< items[i].p1    <<" ";
            os	<< items[i].p2    <<" ";
            os	<< items[i].u     <<" ";
            os	<< items[i].v     <<" ";
        }
        os	<< "\n\t}";
        
    }

    void from_script(const char *v, Points& points)
    {
        
        std::stringstream buffer(v);

        size_t objectsCount;
        buffer>>objectsCount;

        items.clear();
        items.reserve(objectsCount);
        points.resize(objectsCount);

        for (size_t i=0; i<objectsCount; ++i)
        {
            Item t;
            buffer.ignore(2); // skip "/n/t"
            buffer>>points.name[i];
            buffer>>t.geo;
            buffer>>t.obj;
            buffer>>t.p0;
            buffer>>t.p1;
            buffer>>t.p2;
            buffer>>t.u;
            buffer>>t.v;

            items.push_back(t);
        }
        
    }

    void add(float x, float y)
    {
        
        Item item;
        item.geo = "none";
        item.u = x;
        item.v = y;
        items.push_back(item);
    }
    
    void erase(size_t index)
    {
        
        items.erase(items.begin() + index);
        
    }

    void clear()
    {
        
        items.clear();
        
    }

    void solve_points(Points& points)
    {
        

        std::vector<uint32_t> selected;
        size_t size = points.size;
        for (size_t index = 0; index < size; ++index)
        {
            if (points.is_selected(index))
            {
                selected.push_back(index);
                Item& item = items[index];
                item.geo   = "none";
                item.obj   = (uint32_t)-1;
                item.u     = points.points_2d[index].x;
                item.v     = points.points_2d[index].y;
                item.depth = std::numeric_limits<float>::max();
            }
        }
        size = selected.size();

        DD::Image::CameraOp* camera = dynamic_cast<DD::Image::CameraOp*>(parent->input(1));
        DD::Image::GeoOp* geo = dynamic_cast<DD::Image::GeoOp*>(parent->input(2));

        if (geo==nullptr || camera==nullptr) return;

        camera->validate(true);
        
        DD::Image::Matrix4 mat_format;       mat_format.makeIdentity();
        DD::Image::Matrix4 mat_lens        = camera->projection().inverse();
        DD::Image::Matrix4 mat_orientation = camera->matrix();
        DD::Image::CameraOp::from_format(mat_format,&(parent->input_format()));

        DD::Image::Scene scene;
        geo->validate(true);
        geo->build_scene(scene);

        DD::Image::GeometryList* get_list{nullptr};
        get_list = scene.object_list();

        std::vector<unsigned> scene_items;
        std::vector<std::string> scene_items_names; 
        const DD::Image::GeoOp* source_geo{nullptr};
        DD::Image::SceneView_KnobI* kScene{nullptr};
        size_t geo_obj = 0;

        for (size_t obj = 0, object_size = get_list->objects() ; obj < object_size ; ++obj, ++geo_obj) 
        {
            DD::Image::GeoInfo geo_info = get_list->object(obj);
            std::string geo_name = geo_info.select_geo->node_name(); // source_geo ?

            if (source_geo != geo_info.select_geo)
            {
                geo_obj = 0;
                source_geo = geo_info.select_geo;
                DD::Image::SceneView_KnobI* kScene{nullptr};

                int knob_index = 0;

                while (DD::Image::Knob* k = source_geo->knob(knob_index))
                {
                    ++knob_index;
                    if (kScene = k->sceneViewKnob())
                    {
                        break;
                    }
                }
    
                if (kScene)
                {
                    std::vector<std::string> scene_items_names_tmp = kScene->getItemNames(); 
                    kScene->getImportedItems(scene_items);

                    scene_items_names.clear();
                    scene_items_names.resize(scene_items_names_tmp.size());
                    for (uint32_t i = 0; i<scene_items_names.size(); ++i) {
                        uint32_t offset = scene_items[i];
                        scene_items_names[i] = scene_items_names_tmp[offset];
                    }

                    kScene->getSelectedItems(scene_items);
                }
                else
                {
                    scene_items.clear();
                    scene_items_names.clear();
                }
            }

            if (scene_items.size())
            {
                size_t geo_offset = scene_items[geo_obj];
                geo_name = scene_items_names[geo_offset];
            }

            DD::Image::Matrix4 ray_to_camera_matrix = mat_orientation.inverse() * geo_info.matrix;
            DD::Image::Matrix4 ray_to_object_matrix = geo_info.matrix.inverse() * mat_orientation;
            
            DD::Image::Vector3 ray_origin = ray_to_object_matrix.translation();
            ray_to_object_matrix.a03 = 0; 
            ray_to_object_matrix.a13 = 0; 
            ray_to_object_matrix.a23 = 0;
            ray_to_object_matrix *= mat_lens * mat_format;

            DD::Image::Vector3 ray_to_camera_vector;
            ray_to_camera_vector.x = ray_to_camera_matrix.a20; 
            ray_to_camera_vector.y = ray_to_camera_matrix.a21; 
            ray_to_camera_vector.z = ray_to_camera_matrix.a22; 

            std::vector<DD::Image::Vector3> rays;
            rays.reserve(size);
            
            for (size_t index = 0; index < size; ++index)
            {
                size_t index_offset = selected[index];
                DD::Image::Vector3 ray_direction; 

                float x = points.points_2d[index_offset].x;
                float y = points.points_2d[index_offset].y;

                ray_direction.x = ray_to_object_matrix.a00 * x + ray_to_object_matrix.a01 * y + ray_to_object_matrix.a02 + ray_to_object_matrix.a03;
                ray_direction.y = ray_to_object_matrix.a10 * x + ray_to_object_matrix.a11 * y + ray_to_object_matrix.a12 + ray_to_object_matrix.a13;
                ray_direction.z = ray_to_object_matrix.a20 * x + ray_to_object_matrix.a21 * y + ray_to_object_matrix.a22 + ray_to_object_matrix.a23;

                // ray box intersection
                DD::Image::Vector3 bbox_min = (geo_info.bbox().min() - ray_origin) / ray_direction; // ( bbox min - ray_origin ) / ray_direction
                DD::Image::Vector3 bbox_max = (geo_info.bbox().max() - ray_origin) / ray_direction; // ( bbox max - ray_origin ) / ray_direction

                float tmin = std::min(bbox_min.x, bbox_max.x);
                float tmax = std::max(bbox_min.x, bbox_max.x);

                tmin = std::max(tmin, std::min(bbox_min.y, bbox_max.y));
                tmax = std::min(tmax, std::max(bbox_min.y, bbox_max.y));

                tmin = std::max(tmin, std::min(bbox_min.z, bbox_max.z));
                tmax = std::min(tmax, std::max(bbox_min.z, bbox_max.z));

                if (tmax >= tmin)
                {
                    rays.push_back(ray_direction);
                }
            }

            if (rays.size() == 0) continue;

              const DD::Image::Vector3* obj_points   = geo_info.point_array();

            std::vector<uint32_t> varray;
            for(size_t prim = 0, primitives_size = geo_info.primitives(); prim < primitives_size ; ++prim)
            {
                const DD::Image::Primitive* primitive = geo_info.primitive(prim);
                for(size_t face = 0, face_size = primitive->faces() ; face < face_size ; ++face)
                {
                    size_t vertices_size = primitive->face_vertices(face);
                    if (vertices_size > 2)
                    {
                        varray.resize(vertices_size);

                        if (face_size == 1)
                        {
                            for (size_t ver = 0; ver < vertices_size; ++ver) 
                                varray[ver] = primitive->vertex(ver); 
                        }
                        else
                        {
                            primitive->get_face_vertices(face, &varray[0]);
                            if (primitive->getPrimitiveType() == DD::Image::PrimitiveType::ePolyMesh)
                                for (size_t ver = 0; ver < vertices_size; ++ver) 
                                    varray[ver] = primitive->vertex(varray[ver]); 
                        }

                        vertices_size -= 1;

                        uint32_t v0 = varray[0];
                        DD::Image::Vector3 p0 = obj_points[v0];
                        DD::Image::Vector3 triangle_origin = ray_origin - p0;

                        for (size_t ver = 1; ver < vertices_size ; ++ver)
                        {
                            uint32_t v1 = varray[ver];
                            uint32_t v2 = varray[ver + 1];

                            DD::Image::Vector3 edge_1 = obj_points[v1] - p0;
                            DD::Image::Vector3 edge_2 = obj_points[v2] - p0;

                            DD::Image::Vector3 n1 = triangle_origin.cross(edge_1);
                            DD::Image::Vector3 n2 = edge_2.cross(triangle_origin);

                            DD::Image::Vector3 triangle_normal = edge_2.cross(edge_1);
                            float D = -triangle_origin.dot(triangle_normal);

                            // ray triangle intersection
                            for (size_t index = 0; index < size; ++index)
                            {
                                DD::Image::Vector3 ray_direction = rays[index];
                                float denom = ray_direction.dot(triangle_normal);

                                // is triangle normal and ray parallel or is triangle behind the camera ?
                                // denom can't be zero and D and denom need to have the same sign
                                if (((denom > -0.000001f) & (denom < 0.000001f)) | ((D < 0) ^ (denom < 0))) continue;

                                float f = 1.0f/denom;
                                float u = ray_direction.dot(n2) * f;
                                float v = ray_direction.dot(n1) * f;

                                // is ray intersection inside triangle ?
                                // check if barycentric negative or u+v are above denom 
                                if ((u < 0) | (v < 0) | ((u + v) > 1.0f)) continue;

                                float t = D * f;
            
                                // intersection is done in object space, to get the correct depth we transpose the point to camera space
                                // ray_to_camera_matrix.vtransform(ray_direction * t).z
                                float depth = ray_direction.dot(ray_to_camera_vector) * -t;

                                // is depth nearest ? 
                                size_t index_offset = selected[index];
                                if ( items[index_offset].depth < depth ) continue;

                                Item& item = items[index_offset];
                                item.geo   = geo_name;
                                item.obj   = obj;
                                item.p0    = v0;
                                item.p1    = v1;
                                item.p2    = v2;
                                item.u     = u;
                                item.v     = v;
                                item.depth = depth;
                            }
                        }
                    }
                }
            }
        }
    }

    
    void update_points(Points& points, const float frame ,const int view)
    {
        
        DD::Image::OutputContext ocframe;
        ocframe.setFrame(frame);
        ocframe.setView(view);

        DD::Image::CameraOp* camera = dynamic_cast<DD::Image::CameraOp*>(parent->node_input(1,DD::Image::Op::EXECUTABLE,&ocframe));
        DD::Image::GeoOp* geo = dynamic_cast<DD::Image::GeoOp*>(parent->node_input(2,DD::Image::Op::EXECUTABLE,&ocframe));

        if (geo==nullptr || camera==nullptr) return;

        camera->validate(true);
        
        // local to screen space
        DD::Image::Matrix4 xform;
        xform.makeIdentity();
        DD::Image::CameraOp::to_format(xform, &(parent->input_format()));
        xform *= camera->projection() * camera->imatrix();

        DD::Image::GeometryList geometryList;
        DD::Image::Scene scene;

        geo->validate(true);
        geo->get_geometry(scene, geometryList);

        uint32_t geometryList_size = geometryList.size();
        if (geometryList_size == 0) return;

        size_t size = items.size();
        points.resize(size);

        for (size_t index = 0; index < size; ++index)
        {
            Item& item = items[index];
            
            // skip not valid item
            if (item.obj == uint32_t(-1))
            {
                points.points_2d[index].x = item.u;
                points.points_2d[index].y = item.v;
                continue;
            }

            if (item.obj >= geometryList_size) continue;

            DD::Image::GeoInfo& geo_info = geometryList[item.obj];
            size_t geo_points_size = geo_info.points();
            const DD::Image::Vector3* obj_points = geo_info.point_array();
            const DD::Image::Matrix4 obj_matrix = geo_info.matrix;

            uint32_t max_point_offset = item.p0 > item.p1 ? item.p0 : item.p1;
            max_point_offset = max_point_offset > item.p2 ? max_point_offset : item.p2;

            if (max_point_offset >= geo_points_size) continue;

            DD::Image::Vector3 a = *(obj_points + item.p0);
            DD::Image::Vector3 b = *(obj_points + item.p1) - a;
            DD::Image::Vector3 c = *(obj_points + item.p2) - a;
            DD::Image::Vector3 p = a + b * item.u + c * item.v;

            p = obj_matrix.transform(p);
            points.points_3d[index].x = p.x;
            points.points_3d[index].y = p.y;
            points.points_3d[index].z = p.z;

            p = xform.transform(DD::Image::Vector4(p,1)).divide_w();
            points.points_2d[index].x = p.x;
            points.points_2d[index].y = p.y;

            // if do_normal ? 
            // {
            b = obj_matrix.vtransform(b);
            c = obj_matrix.vtransform(c);
            DD::Image::Vector3 normal = b.cross(c);
            normal.normalize();

            b = normal.cross(b);
            b.normalize();
            
            points.normals[index].nx = normal.x;
            points.normals[index].ny = normal.y;
            points.normals[index].nz = normal.z;
            points.normals[index].tx = b.x;
            points.normals[index].ty = b.y;
            points.normals[index].tz = b.z;
            // }
        }
    }
};