#include "SDL.h"
#include "Engine.h"
#include <iostream>

int main(int argc, char* args[]) {

	SDL_Init(SDL_INIT_VIDEO);

	SDL_Window* window = SDL_CreateWindow(
		"Vulkan Initialization",
		SDL_WINDOWPOS_UNDEFINED,	// x
		SDL_WINDOWPOS_UNDEFINED,	// y
		800,	// width
		600,	// height
		SDL_WINDOW_VULKAN
	);

	if (!window)
	{
		std::cerr << "Cannot create SDL window!" << std::endl;
		exit(-1);
	}

	Engine engine;
	engine.init(window);

	SDL_Event sdlEvent;
	bool running = true;

	while (running)
	{
		if (SDL_PollEvent(&sdlEvent))
		{
			if (sdlEvent.type == SDL_WINDOWEVENT)
			{
				switch (sdlEvent.window.event)
				{
				case SDL_WINDOWEVENT_CLOSE:
					running = false;
					break;
				}
			}
		}

		engine.update();
		engine.render();
	}

	engine.cleanUp();
	SDL_DestroyWindow(window);

	SDL_Quit();
	return 0;
}