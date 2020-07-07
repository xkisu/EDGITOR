#include <iostream>
#include <vector>
#include <thread>
#include <SDL.h>
#include <SDL_ttf.h>
#include "SDL_FontCache.h"
#include "VARIABLES.h"
#include "FUNCTIONS.h"

  //
 //   MAIN LOOP   ///////////////////////////////////////////////// ///////  //////   /////    ///     //      /
//

int main(int argc, char* argv[])
{
	std::cout << (UINT16_MAX) << std::endl << (INT16_MAX) << std::endl;

	// MAIN INIT
	INIT_SDL();
	INIT_WINDOW();
	INIT_RENDERER();

	while (!QUIT) // MAIN LOOP
	{
		const Uint64 fps_start = SDL_GetPerformanceCounter(); // fps counter

		// SET WINDOW X, Y, W, H
		// CLEAR RENDER TARGET
		SDL_GetWindowSize(WINDOW, &WINDOW_W, &WINDOW_H);
		SDL_GetWindowPosition(WINDOW, &WINDOW_X, &WINDOW_Y);
		SDL_SetRenderTarget(RENDERER, NULL);
		SDL_SetRenderDrawColor(RENDERER, 0, 0, 0, 255);
		SDL_RenderClear(RENDERER);

	///////////////////////////////////////////////// ///////  //////   /////    ///     //      /
		EVENT_LOOP();
	///////////////////////////////////////////////// ///////  //////   /////    ///     //      /

		// UPDATE THE BRUSH TEXTURE PER-CHANGE
		// this is because a complex shape might be drawn in one tick; like floodfill
		if (BRUSH_UPDATE) {
			// make sure the brush_update isn't beyond the canvas
			BRUSH_UPDATE_X1 = (clamp(BRUSH_UPDATE_X1, 0, CANVAS_W));
			BRUSH_UPDATE_Y1 = (clamp(BRUSH_UPDATE_Y1, 0, CANVAS_H));
			BRUSH_UPDATE_X2 = (clamp(BRUSH_UPDATE_X2, 0, CANVAS_W));
			BRUSH_UPDATE_Y2 = (clamp(BRUSH_UPDATE_Y2, 0, CANVAS_H));

			// set the temporary rectangle
			I_RECT.x = BRUSH_UPDATE_X1;
			I_RECT.y = BRUSH_UPDATE_Y1;
			I_RECT.w = (BRUSH_UPDATE_X2 - BRUSH_UPDATE_X1);
			I_RECT.h = (BRUSH_UPDATE_Y2 - BRUSH_UPDATE_Y1);

			// update the brush texture
			SDL_SetTextureBlendMode(BRUSH_TEXTURE, SDL_BLENDMODE_NONE);
			SDL_UpdateTexture(BRUSH_TEXTURE, &I_RECT, &BRUSH_PIXELS[BRUSH_UPDATE_Y1 * CANVAS_W + BRUSH_UPDATE_X1], CANVAS_PITCH);

			// the layer updates only when we stop drawing - for performance.
			// so we constantly update the min and max bounds
			LAYER_UPDATE_X1 = min(LAYER_UPDATE_X1, BRUSH_UPDATE_X1);
			LAYER_UPDATE_Y1 = min(LAYER_UPDATE_Y1, BRUSH_UPDATE_Y1);
			LAYER_UPDATE_X2 = max(LAYER_UPDATE_X2, BRUSH_UPDATE_X2);
			LAYER_UPDATE_Y2 = max(LAYER_UPDATE_Y2, BRUSH_UPDATE_Y2);

			// reset the brush bounds with every tick
			BRUSH_UPDATE_X1 = INT16_MAX;
			BRUSH_UPDATE_Y1 = INT16_MAX;
			BRUSH_UPDATE_X2 = INT16_MIN;
			BRUSH_UPDATE_Y2 = INT16_MIN;

			BRUSH_UPDATE = 0;
		}

		// LAYER UPDATE
		int t_layer_update_w = max(LAYER_UPDATE_X2 - LAYER_UPDATE_X1, 0), t_layer_update_h = max(LAYER_UPDATE_Y2 - LAYER_UPDATE_Y1, 0); // probably don't need these max()

		if ((LAYER_UPDATE == 1) && (t_layer_update_w > 0) && (t_layer_update_h > 0))
		{
			// temporary rect the size of the layer update
			I_RECT = { LAYER_UPDATE_X1, LAYER_UPDATE_Y1, t_layer_update_w, t_layer_update_h };

			// create new undo pointer
			std::shared_ptr<UNDO_DATA> _u(new UNDO_DATA((uint16_t)t_layer_update_w, (uint16_t)t_layer_update_h));
			_u->x = (uint16_t)LAYER_UPDATE_X1;
			_u->y = (uint16_t)LAYER_UPDATE_Y1;
			_u->type = CURRENT_TOOL;
			_u->layer = CURRENT_LAYER;

			uint32_t cols, cold, NEW_COL;
			int _pos;
			float __d = (1.0f / 255.0f), tdest_cola, tsrc_cola; // '__d' is used to save a bit of calculations
			uint32_t* PD = (LAYERS[CURRENT_LAYER].pixels);
			for (int16_t _Y = LAYER_UPDATE_Y1; _Y < LAYER_UPDATE_Y2; ++_Y) {
				for (int16_t _X = LAYER_UPDATE_X1; _X < LAYER_UPDATE_X2; ++_X) {
					_pos = (_Y * CANVAS_W + _X);
					cols = BRUSH_PIXELS[_pos];
					cold = PD[_pos];

					if (cols == 0x00000000) // if there's an empty pixel in the brush texture
					{
						// make it save the destination pixel as the undo and redo
						_u->_set_pixel(((_Y - LAYER_UPDATE_Y1) * t_layer_update_w + (_X - LAYER_UPDATE_X1)), cold, cold);
						continue;
					}
					else
					if (CURRENT_TOOL == 1) // if it's the erase tool
					{
						BRUSH_PIXELS[_pos] = 0x00000000; // clear the brush pixel
						PD[_pos] = 0x00000000; // erase the destination pixel
						_u->_set_pixel(((_Y - LAYER_UPDATE_Y1) * t_layer_update_w + (_X - LAYER_UPDATE_X1)), cold, 0xffffff00); // set the redo to the erase colour
						// 0xffffff00 is invisible-white, and a colour that can be connected to "is empty" for erasure
						continue;
					}

					if (cold == 0x00000000) // if destination pixel is empty
					{
						BRUSH_PIXELS[_pos] = 0x00000000; // clear the brush pixel
						PD[_pos] = cols; // make destination the saved brush pixel
						_u->_set_pixel(((_Y - LAYER_UPDATE_Y1) * t_layer_update_w + (_X - LAYER_UPDATE_X1)), cold, cols); // save redo and undo
						continue;
					}

					// if it isn't any of those edge cases, we properly mix the colours
					//
					//  THIS COULD BE MORE EFFICIENT !! (it uses floats where it could use pre-calculated int divisions)
					//
					SRC_COLA = (cols & 0x000000ff) * __d;
					DEST_COLA = (cold & 0x000000ff) * __d * (1. - SRC_COLA);
					NEW_COLA = (SRC_COLA + DEST_COLA);
					tdest_cola = (__d * DEST_COLA);
					tsrc_cola = (__d * SRC_COLA);
					NEW_COL = (uint32_t)(
						((uint8_t)((((((cols & 0xff000000) >> 24) * tsrc_cola) + (((cold & 0xff000000) >> 24) * tdest_cola)) / NEW_COLA) * 255) << 24) |
						((uint8_t)((((((cols & 0x00ff0000) >> 16) * tsrc_cola) + (((cold & 0x00ff0000) >> 16) * tdest_cola)) / NEW_COLA) * 255) << 16) |
						((uint8_t)((((((cols & 0x0000ff00) >> 8) * tsrc_cola) + (((cold & 0x0000ff00) >> 8) * tdest_cola)) / NEW_COLA) * 255) << 8) |
						(uint8_t)(NEW_COLA * 255));
					PD[_pos] = NEW_COL;
					BRUSH_PIXELS[_pos] = 0x00000000;
					_u->_set_pixel(((_Y - LAYER_UPDATE_Y1) * t_layer_update_w + (_X - LAYER_UPDATE_X1)), cold, NEW_COL);
				}
			}

			// clear the brush texture (since we made all pixels 0x00000000
			SDL_SetTextureBlendMode(BRUSH_TEXTURE, SDL_BLENDMODE_NONE);
			SDL_UpdateTexture(BRUSH_TEXTURE, &I_RECT, &BRUSH_PIXELS[LAYER_UPDATE_Y1 * CANVAS_W + LAYER_UPDATE_X1], CANVAS_PITCH);

			// if we're back a few steps in the undo reel, we clear all the above undo steps.
			while (UNDO_POS > 0) {
				(UNDO_LIST[UNDO_LIST.size() - 1]->redo_pixels).clear();
				(UNDO_LIST[UNDO_LIST.size() - 1]->undo_pixels).clear();
				UNDO_LIST.pop_back();
				UNDO_POS--;
			};

			// add the new undo
			UNDO_LIST.push_back(_u);
			
			// update the layer we drew to
			SDL_UpdateTexture(LAYERS[CURRENT_LAYER].texture, &I_RECT, &PD[LAYER_UPDATE_Y1 * CANVAS_W + LAYER_UPDATE_X1], CANVAS_PITCH);

			LAYER_UPDATE = 0;
		}
		else
		{
			if (LAYER_UPDATE > 0) LAYER_UPDATE--;

			if (LAYER_UPDATE == 0)
			{
				LAYER_UPDATE_X1 = INT16_MAX;
				LAYER_UPDATE_Y1 = INT16_MAX;
				LAYER_UPDATE_X2 = INT16_MIN;
				LAYER_UPDATE_Y2 = INT16_MIN;

				LAYER_UPDATE = -1;
			}
		}


		SDL_SetRenderDrawColor(RENDERER, 255, 255, 255, 255);

		// RENDER
		// smooth lerping animation to make things SLIGHTLY smooth when panning and zooming
		// the '4.0' can be any integer, and will be a changeable option in Settings
		CANVAS_X_ANIM = (float)(reach_tween(CANVAS_X_ANIM, CANVAS_X, 4.0));
		CANVAS_Y_ANIM = (float)(reach_tween(CANVAS_Y_ANIM, CANVAS_Y, 4.0));
		CANVAS_W_ANIM = (float)(reach_tween(CANVAS_W_ANIM, (float)CANVAS_W * CANVAS_ZOOM, 4.0));
		CANVAS_H_ANIM = (float)(reach_tween(CANVAS_H_ANIM, (float)CANVAS_H * CANVAS_ZOOM, 4.0));
		
		// transparent background grid
		float bg_w = ((CANVAS_W_ANIM / (float)CELL_W) * (float)CELL_W);
		float bg_h = ((CANVAS_H_ANIM / (float)CELL_H) * (float)CELL_H);
		F_RECT = {CANVAS_X_ANIM, CANVAS_Y_ANIM, bg_w, bg_h};
		SDL_SetTextureBlendMode(BG_GRID_TEXTURE, SDL_BLENDMODE_BLEND);
		SDL_RenderCopyF(RENDERER, BG_GRID_TEXTURE, NULL, &F_RECT);
		SDL_SetRenderDrawColor(RENDERER, 0, 0, 0, 255);
		F_RECT = { maxf(0,CANVAS_X_ANIM), maxf(0,CANVAS_Y_ANIM + (CANVAS_H_ANIM)), minf(WINDOW_W,bg_w), CELL_H * CANVAS_ZOOM };
		SDL_RenderFillRectF(RENDERER, &F_RECT);
		F_RECT = { maxf(0,CANVAS_X_ANIM + (CANVAS_W_ANIM)), maxf(0,CANVAS_Y_ANIM), CELL_W * CANVAS_ZOOM, minf(WINDOW_H,bg_h) };
		SDL_RenderFillRectF(RENDERER, &F_RECT);
		
		F_RECT = {CANVAS_X_ANIM, CANVAS_Y_ANIM, CANVAS_W_ANIM, CANVAS_H_ANIM};

		// RENDER THE LAYERS
		for (int16_t i = 0; i < LAYERS.size(); i++)
		{
			SDL_SetTextureBlendMode(LAYERS[i].texture, LAYERS[i].blendmode);
			SDL_RenderCopyF(RENDERER, LAYERS[i].texture, NULL, &F_RECT);
		}

		// the grey box around the canvas
		SDL_SetRenderDrawColor(RENDERER, 51, 51, 51, 255);
		F_RECT = { CANVAS_X_ANIM - 2, CANVAS_Y_ANIM - 2, bg_w + 4, bg_h + 4 };
		SDL_RenderDrawRectF(RENDERER, &F_RECT);
		SDL_SetRenderDrawColor(RENDERER, 0, 0, 0, 255);

		// RENDER BRUSH TEXTURE
		// could probably add a 'if (BRUSH_UPDATE)' so it doesn't always render an empty texture when not drawing
		SDL_SetTextureBlendMode(BRUSH_TEXTURE, SDL_BLENDMODE_BLEND);
		SDL_RenderCopyF(RENDERER, BRUSH_TEXTURE, NULL, &F_RECT);

		// TEST HUE BAR
		//I_RECT = { 10, 10, 32, 360 };
		//SDL_RenderCopy(RENDERER, UI_TEXTURE_HUEBAR, NULL, &I_RECT);

		//FC_Draw(font, RENDERER, 36, 10, "%i\n%i\n%i\n%i", BRUSH_UPDATE, LAYER_UPDATE, CANVAS_MOUSE_X, CANVAS_MOUSE_Y);

		SDL_SetRenderDrawColor(RENDERER, 0, 0, 0, 0);
		SDL_RenderPresent(RENDERER);

		//SDL_Delay(1);

		// SET FPS
		const Uint64 fps_end = SDL_GetPerformanceCounter();
		const static Uint64 fps_freq = SDL_GetPerformanceFrequency();
		const double fps_seconds = (fps_end - fps_start) / static_cast<double>(fps_freq);
		FPS = reach_tween(FPS, 1 / (float)fps_seconds, 100.0);
		std::cout << " FPS: " << (int)floor(FPS) << "          " << '\r';
		std::cout << SDL_GetError();
	}

	SDL_Delay(10);

	FC_FreeFont(font);
	FC_FreeFont(font_bold);
	delete[] BRUSH_PIXELS;
	SDL_DestroyRenderer(RENDERER);
	SDL_DestroyWindow(WINDOW);
	SDL_Quit();

	return 0;
}

  //
 //   END   ///////////////////////////////////////////////// ///////  //////   /////    ///     //      /
//