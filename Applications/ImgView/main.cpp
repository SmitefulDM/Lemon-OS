#include <gui/window.h>
#include <gui/widgets.h>
#include <gui/messagebox.h>
#include <stdio.h>
#include <stdlib.h>
#include <gui/filedialog.h>

#define IMGVIEW_OPEN 1

Lemon::GUI::Window* window;
Lemon::GUI::ScrollView* sv;
Lemon::GUI::Bitmap* imgWidget;
Lemon::GUI::WindowMenu fileMenu;
surface_t image;

int LoadImage(char* path){
    if(!path){
        Lemon::GUI::DisplayMessageBox("Image Viewer", "Invalid Filepath");
        return 1;
    }

    int ret = Lemon::Graphics::LoadImage(path, &image);

    if(ret){
        char msg[128];
        sprintf(msg, "Failed to open image, Error Code: %d", ret);
        Lemon::GUI::DisplayMessageBox("Image Viewer", msg);
        return ret;
    }

    return 0;
}

void OnWindowCmd(unsigned short cmd, Lemon::GUI::Window* win){
    if(cmd == IMGVIEW_OPEN){
        free(image.buffer);
        if(LoadImage(Lemon::GUI::FileDialog("/"))){
            exit(-1);
        }
        sv->RemoveWidget(imgWidget);
        delete imgWidget;
        imgWidget = new Lemon::GUI::Bitmap({{0, 0}, {0, 0}}, &image);
        sv->AddWidget(imgWidget);
    }
}

int main(int argc, char** argv){
    if(argc > 1){
        if(LoadImage(argv[1])){
            return -1;
        }
    } else if(LoadImage(Lemon::GUI::FileDialog("."))){
        return -1;
    }

    fileMenu.first = "File";
	fileMenu.second.push_back({.id = IMGVIEW_OPEN, .name = std::string("Open...")});

    window = new Lemon::GUI::Window("Image Viewer", {800, 500}, 0, Lemon::GUI::WindowType::GUI);
    window->CreateMenuBar();
    window->menuBar->items.push_back(fileMenu);
	window->OnMenuCmd = OnWindowCmd;

    sv = new Lemon::GUI::ScrollView({{0, 0}, {window->GetSize().x, window->GetSize().y}});
    imgWidget = new Lemon::GUI::Bitmap({{0, 0}, {0, 0}}, &image);

    sv->AddWidget(imgWidget);

    window->AddWidget(sv);
    
	while(!window->closed){
        Lemon::LemonEvent ev;
		while(window->PollEvent(ev)){
            window->GUIHandleEvent(ev);
        }

        window->Paint();
        window->WaitEvent();
	}
}