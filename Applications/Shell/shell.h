#include <core/shell.h>
#include <gui/window.h>

#include <map>

class ShellWindow{
public:
	int id;
	std::string title;
	int state;
	int lastState;
};

class ShellInstance {
    Lemon::MessageServer shellSrv;

    Lemon::GUI::Window* taskbar;
    Lemon::GUI::Window* menu;

    void PollCommands();
public:
    std::map<int, ShellWindow*> windows;
    ShellWindow* active = nullptr;
    bool showMenu = true;

    ShellInstance(sockaddr_un& address);

    void SetMenu(Lemon::GUI::Window* menu);
    void SetTaskbar(Lemon::GUI::Window* taskbar);

    void Update();
    Lemon::MessageHandler& GetServer() { return shellSrv; }
    void Open(char* path);

    void SetWindowState(ShellWindow* win);

    void(*AddWindow)(ShellWindow*) = nullptr;
    void(*RemoveWindow)(ShellWindow*) = nullptr;
    void(*RefreshWindows)(void) = nullptr;
};