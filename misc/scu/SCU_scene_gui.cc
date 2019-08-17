// Single Compilation Unit
#define SCU_IDENT(x) x
#define SCU_XSTR(x) #x
#define SCU_STR(x) SCU_XSTR(x)
#define SCU_PATH(x,y) SCU_STR(SCU_IDENT(x)SCU_IDENT(y))
#define SCU_DIR scene/gui/

#include SCU_PATH(SCU_DIR,base_button.cpp)
#include SCU_PATH(SCU_DIR,box_container.cpp)
#include SCU_PATH(SCU_DIR,button.cpp)
#include SCU_PATH(SCU_DIR,center_container.cpp)
#include SCU_PATH(SCU_DIR,check_box.cpp)
#include SCU_PATH(SCU_DIR,check_button.cpp)
#include SCU_PATH(SCU_DIR,color_picker.cpp)
#include SCU_PATH(SCU_DIR,color_rect.cpp)
#include SCU_PATH(SCU_DIR,container.cpp)
#include SCU_PATH(SCU_DIR,control.cpp)
#include SCU_PATH(SCU_DIR,dialogs.cpp)
#include SCU_PATH(SCU_DIR,file_dialog.cpp)
#include SCU_PATH(SCU_DIR,gradient_edit.cpp)
#include SCU_PATH(SCU_DIR,graph_edit.cpp)
#include SCU_PATH(SCU_DIR,graph_node.cpp)
#include SCU_PATH(SCU_DIR,grid_container.cpp)
#include SCU_PATH(SCU_DIR,item_list.cpp)
#include SCU_PATH(SCU_DIR,label.cpp)
#include SCU_PATH(SCU_DIR,link_button.cpp)
#include SCU_PATH(SCU_DIR,margin_container.cpp)
#include SCU_PATH(SCU_DIR,menu_button.cpp)
#include SCU_PATH(SCU_DIR,nine_patch_rect.cpp)
#include SCU_PATH(SCU_DIR,option_button.cpp)
#include SCU_PATH(SCU_DIR,panel.cpp)
#include SCU_PATH(SCU_DIR,panel_container.cpp)
#include SCU_PATH(SCU_DIR,popup.cpp)
#include SCU_PATH(SCU_DIR,popup_menu.cpp)
#include SCU_PATH(SCU_DIR,progress_bar.cpp)
#include SCU_PATH(SCU_DIR,range.cpp)
#include SCU_PATH(SCU_DIR,reference_rect.cpp)
#include SCU_PATH(SCU_DIR,rich_text_label.cpp)
#include SCU_PATH(SCU_DIR,scroll_bar.cpp)
#include SCU_PATH(SCU_DIR,scroll_container.cpp)
#include SCU_PATH(SCU_DIR,separator.cpp)
#include SCU_PATH(SCU_DIR,shortcut.cpp)
#include SCU_PATH(SCU_DIR,slider.cpp)
#include SCU_PATH(SCU_DIR,spin_box.cpp)
#include SCU_PATH(SCU_DIR,split_container.cpp)
#include SCU_PATH(SCU_DIR,tab_container.cpp)
#include SCU_PATH(SCU_DIR,tabs.cpp)
#include SCU_PATH(SCU_DIR,text_edit.cpp)
#include SCU_PATH(SCU_DIR,texture_button.cpp)
#include SCU_PATH(SCU_DIR,texture_progress.cpp)
#include SCU_PATH(SCU_DIR,texture_rect.cpp)
#include SCU_PATH(SCU_DIR,tool_button.cpp)
#include SCU_PATH(SCU_DIR,tree.cpp)
#include SCU_PATH(SCU_DIR,video_player.cpp)
#include SCU_PATH(SCU_DIR,viewport_container.cpp)
