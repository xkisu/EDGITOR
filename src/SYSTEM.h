#pragma once

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;
struct UIBOX_INFO;

void PRINT(std::string print);
void PRINT(const char* print);
void PRINT(int print);

bool INIT_SDL();
SDL_Window* INIT_WINDOW();
SDL_Renderer* INIT_RENDERER(SDL_Window* WINDOW);
SDL_Texture* INIT_FONT();

void SYSTEM_SHUTDOWN(SDL_Window* WINDOW);

void SYSTEM_INPUT_UPDATE();

void SYSTEM_BRUSH_UPDATE();
void SYSTEM_LAYER_UPDATE();
void SYSTEM_CANVAS_UPDATE();


extern UIBOX_INFO* UIBOX_TOOLS;
extern UIBOX_INFO* UIBOX_COLOR;
extern UIBOX_INFO* UIBOX_BRUSH;
extern UIBOX_INFO* UIBOX_FILES;
extern UIBOX_INFO* UIBOX_CANVAS;
