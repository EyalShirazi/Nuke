#pragma once


struct Point2D
{
    float x{0};
    float y{0};
};

struct Point3D
{
    union
    {
        float arr[3];
        struct { float x, y, z; };
    };
};

struct Point6D
{
    union
    {
        float norm[6];
        struct { float nx, ny, nz, tx, ty, tz; };
    };
};

struct Points
{
    size_t size{0};
    std::vector<std::string> name;
    std::vector<Point2D> points_2d;
    std::vector<Point3D> points_3d;
    std::vector<Point6D> normals;
    std::vector<uint64_t> selected;


    size_t add_emtpy()
    {
        
        ++size;
        resize(size);
        return size - 1;
    }


    template <typename T>
    void clear(T& modulePoints)
    {
        
        size = 0;

        name.clear();
        points_2d.clear();
        points_3d.clear();
        normals.clear();
        selected.clear();

        modulePoints.clear();
    }


    template <typename T>
    void erase(T& modulePoints)
    {
        
        for(size_t index = size-1; index < size ; --index)
        {
            if (is_selected(index) != 0)
            {
                --size; 
                name.erase(name.begin() + index);
                points_2d.erase(points_2d.begin() + index);
                points_3d.erase(points_3d.begin() + index);
                normals.erase(normals.begin() + index);

                modulePoints.erase(index);
            } 
        }
        deselect_all_points();
    }


    void resize(size_t _size)
    {
        
        size = _size;
        
        if (size < name.size()) return;

        size_t resize = size * 1.5;

        name.resize(resize);
        points_2d.resize(resize);
        points_3d.resize(resize);
        normals.resize(resize);
        selected.resize(index_64(resize)+1);
    }


    inline size_t index_64(size_t x)
    {
        return x >> 6;
    }


    inline size_t fmod_64(size_t x)
    {
        return x & 63;
    }


    inline void set_name(size_t index, std::string _name)
    {
        
        name[index] = _name;
        
    }

    inline void set_2d(size_t index, float x, float y)
    {
        
        points_2d[index].x = x;
        points_2d[index].y = y;
        
    }

    inline void set_selected(size_t index, size_t state)
    {
        
        size_t selected_major = index_64(index);
        size_t selected_minor = fmod_64(index);
        selected[selected_major] = selected[selected_major] & ~(1ULL<<selected_minor) | (state<<selected_minor);
        
    }

    inline bool is_selected(size_t index)
    {
        // 
        size_t selected_major = index_64(index);
        size_t selected_minor = fmod_64(index);
        return (selected[selected_major] & (1ULL<<selected_minor));
    }

    inline bool is_any_selected()
    {
        
        for (size_t index = 0, selected_size = selected.size(); index < selected_size; ++index)
        {
            if (selected[index] != 0) return true;
        } 
        return false;
    }

    inline void deselect_all_points()
    {
        
        for (size_t index = 0, selected_size = selected.size(); index < selected_size; ++index)
        {
            selected[index] = 0;
        } 
        
    }

    void get_bbox_3d(Point3D& min, Point3D& max)
    {
        
        min.x = min.y = min.z =  std::numeric_limits<float>::max();
        max.x = max.y = max.z = -std::numeric_limits<float>::max();   
        
        for (size_t index = 0; index < size; ++index)
        {
            if (is_selected(index) != 0)
            {
                float x = points_3d[index].x;
                float y = points_3d[index].y;
                float z = points_3d[index].z;
                min.x = min.x < x ? min.x : x;
                min.y = min.y < y ? min.y : y;
                min.z = min.z < z ? min.z : z;
                max.x = max.x > x ? max.x : x;
                max.y = max.y > y ? max.y : y;
                max.z = max.z > z ? max.z : z;
            }
        }
        
    }
};