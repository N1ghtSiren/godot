// Single Compilation Unit
#define SCU_IDENT(x) x
#define SCU_XSTR(x) #x
#define SCU_STR(x) SCU_XSTR(x)
#define SCU_PATH(x,y) SCU_STR(SCU_IDENT(x)SCU_IDENT(y))
#define SCU_DIR scene/resources/

#include SCU_PATH(SCU_DIR,animation.cpp)
#include SCU_PATH(SCU_DIR,audio_stream_sample.cpp)
#include SCU_PATH(SCU_DIR,bit_map.cpp)
#include SCU_PATH(SCU_DIR,box_shape.cpp)
#include SCU_PATH(SCU_DIR,canvas.cpp)
#include SCU_PATH(SCU_DIR,capsule_shape.cpp)
#include SCU_PATH(SCU_DIR,capsule_shape_2d.cpp)
#include SCU_PATH(SCU_DIR,circle_shape_2d.cpp)
#include SCU_PATH(SCU_DIR,concave_polygon_shape.cpp)
#include SCU_PATH(SCU_DIR,concave_polygon_shape_2d.cpp)
#include SCU_PATH(SCU_DIR,convex_polygon_shape.cpp)
#include SCU_PATH(SCU_DIR,convex_polygon_shape_2d.cpp)
#include SCU_PATH(SCU_DIR,curve.cpp)
#include SCU_PATH(SCU_DIR,cylinder_shape.cpp)
#include SCU_PATH(SCU_DIR,dynamic_font.cpp)
#include SCU_PATH(SCU_DIR,dynamic_font_stb.cpp)
#include SCU_PATH(SCU_DIR,environment.cpp)
#include SCU_PATH(SCU_DIR,font.cpp)
#include SCU_PATH(SCU_DIR,gradient.cpp)
#include SCU_PATH(SCU_DIR,height_map_shape.cpp)
#include SCU_PATH(SCU_DIR,line_shape_2d.cpp)
#include SCU_PATH(SCU_DIR,material.cpp)
#include SCU_PATH(SCU_DIR,mesh.cpp)
#include SCU_PATH(SCU_DIR,mesh_data_tool.cpp)
#include SCU_PATH(SCU_DIR,mesh_library.cpp)
#include SCU_PATH(SCU_DIR,multimesh.cpp)
#include SCU_PATH(SCU_DIR,packed_scene.cpp)
#include SCU_PATH(SCU_DIR,particles_material.cpp)
#include SCU_PATH(SCU_DIR,physics_material.cpp)
#include SCU_PATH(SCU_DIR,plane_shape.cpp)
#include SCU_PATH(SCU_DIR,polygon_path_finder.cpp)
#include SCU_PATH(SCU_DIR,primitive_meshes.cpp)
#include SCU_PATH(SCU_DIR,ray_shape.cpp)
#include SCU_PATH(SCU_DIR,rectangle_shape_2d.cpp)
#include SCU_PATH(SCU_DIR,resource_format_text.cpp)
#include SCU_PATH(SCU_DIR,room.cpp)
#include SCU_PATH(SCU_DIR,segment_shape_2d.cpp)
#include SCU_PATH(SCU_DIR,shader.cpp)
#include SCU_PATH(SCU_DIR,shape.cpp)
#include SCU_PATH(SCU_DIR,shape_2d.cpp)
#include SCU_PATH(SCU_DIR,sky.cpp)
#include SCU_PATH(SCU_DIR,space_2d.cpp)
#include SCU_PATH(SCU_DIR,sphere_shape.cpp)
#include SCU_PATH(SCU_DIR,style_box.cpp)
#include SCU_PATH(SCU_DIR,surface_tool.cpp)
#include SCU_PATH(SCU_DIR,text_file.cpp)
#include SCU_PATH(SCU_DIR,texture.cpp)
#include SCU_PATH(SCU_DIR,theme.cpp)
#include SCU_PATH(SCU_DIR,tile_set.cpp)
#include SCU_PATH(SCU_DIR,video_stream.cpp)
#include SCU_PATH(SCU_DIR,visual_shader.cpp)
#include SCU_PATH(SCU_DIR,visual_shader_nodes.cpp)
#include SCU_PATH(SCU_DIR,world.cpp)
#include SCU_PATH(SCU_DIR,world_2d.cpp)
