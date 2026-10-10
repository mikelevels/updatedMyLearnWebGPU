// main.cpp: where the program starts. It only creates the Application and
// runs its main loop; everything else lives in the Application*.cpp files
// (see the map at the top of Application.h).

#include "Application.h"

#ifdef __EMSCRIPTEN__
    #include <emscripten.h>
#endif

int main(int, char**){
    Application app;

    if(!app.Initialize()){
        return 1;
    }

#ifdef __EMSCRIPTEN__
//Equivalent of the main loop when using emscripten
    auto callback = [](void * arg){
        Application * pApp = reinterpret_cast<Application*>(arg);
        pApp->MainLoop();
    };
    emscripten_set_main_loop_arg(callback, &app,0,true);
#else
    while(app.IsRunning()){
        app.MainLoop();
    }
	// Release every GPU object and close the window once the loop ends.
	// (In the browser the loop above never "ends": the page simply closes,
	// so there is no equivalent call in the __EMSCRIPTEN__ branch.)
	app.Terminate();
#endif

    return 0;
}
