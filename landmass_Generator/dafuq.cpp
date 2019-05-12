#include <stdio.h>
#include <math.h>
#include <time.h>
#include <Windows.h>

#include "SDL2-2.0.8\include\SDL.h"
#include "SDL2-2.0.8\include\SDL_image.h"
#pragma comment(lib, "SDL2-2.0.8\\lib\\x86\\SDL2.lib")
#pragma comment(lib, "SDL2-2.0.8\\lib\\x86\\SDL2main.lib")
#pragma comment(lib, "SDL2-2.0.8\\lib\\x86\\SDL2_image.lib")

struct Vec2D
{
	float x, y;
};

struct Pixel
{
	char r, g, b, a;
};

struct Rect
{
	int x, y, w, h;
	Pixel color;
};

struct Slider
{
	Rect range;
	Rect marker;
	float position;
};

struct Gaussian_Set
{
	int n_data;
	float** data;
	int width;
	int height;
	Vec2D* std_dev;
	Vec2D* mean;
	float* min;
	float* max;
};

void set_2D_Gaussian(float* data, int width, int height, const Vec2D* std_dev, const Vec2D* mean, float min, float max)
{
	if (max == 0.0 && min == 0.0) return;

	int area = width * height;

	if (max < min) max = min;
	else if (max > 1.0) max = 1.0;

	if (min < 0) min = 0;
	else if (min > max) min = max;

	for (int i = 0; i < area; i++)
	{
		int x = i % width;
		int y = i / width;
		float curve = 0.0;
		curve = 333.0 * (max - min) * exp(-0.5 * (((float)x - mean->x) / std_dev->x) * (((float)x - mean->x) / std_dev->x)) / (std_dev->x * sqrt(2.0 * M_PI)) + min;
		curve *= 333.0 * (max - min) * exp(-0.5 * (((float)y - mean->y) / std_dev->y) * (((float)y - mean->y) / std_dev->y)) / (std_dev->y * sqrt(2.0 * M_PI)) + min;
		data[i] = curve;
	}
}

void merge_2D_Gaussians(const Gaussian_Set* src, float* dest)
{
	for (int i = 0; i < src->width * src->height; i++)
	{
		float max = 0.0;
		for (int j = 0; j < src->n_data; j++)
		{
			if (src->data[j][i] > max) max = src->data[j][i];
		}
		dest[i] = max;
	}
}

void apply_Layer(float* dest, const float* src, float area, float strength)
{
	for (int i = 0; i < area; i++)
	{
		dest[i] *= 1.0 - strength;
		dest[i] += strength * src[i];
	}
}

void wipe_Screen(Pixel* screen, int screen_area, Pixel color)
{
	for (int i = 0; i < screen_area; i++) screen[i] = color;
}

void draw_Rect(Pixel* screen, int screen_width, int screen_height, const Rect* rect)
{
	for (int y = 0; y < rect->h; y++)
	{
		for (int x = 0; x < rect->w; x++)
		{
			int rect_index = y * rect->w + x;
			int screen_index = (y + rect->y) * screen_width + x + rect->x;

			screen[screen_index] = rect->color;
		}
	}
}

void draw_Screen(Pixel* screen, Pixel* window, int screen_x, int screen_y, int screen_width, int screen_height, int window_width, int window_height)
{
	for (int y = 0; y < screen_height; y++)
	{
		for (int x = 0; x < screen_width; x++)
		{
			int screen_index = y * screen_width + x;
			int window_index = (y + screen_y) * window_width + x + screen_x;
			window[window_index] = screen[screen_index];
		}
	}
}

void wmain()
{
	//SDL Init
	SDL_Init(SDL_INIT_VIDEO);

	int window_width = 1000;
	int window_height = 600;
	int window_area = window_width * window_height;

	SDL_Window *window = SDL_CreateWindow("mep", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, window_width, window_height, SDL_WINDOW_SHOWN);

	SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

	SDL_Surface *print_surface = SDL_GetWindowSurface(window);
	SDL_Surface *work_surface = SDL_CreateRGBSurfaceWithFormat(0, window_width, window_height, 32, SDL_PIXELFORMAT_RGBA32);
	Pixel *p = (Pixel*)work_surface->pixels;

	Pixel ui_back;
	ui_back.r = 150;
	ui_back.g = 255;
	ui_back.b = 225;
	ui_back.a = 255;

	Pixel black;
	black.r = 0;
	black.g = 0;
	black.b = 0;
	black.a = 255;

	//init screen 1
	int screen_1_x = 0;
	int screen_1_y = 0;
	int screen_1_width = 800;
	int screen_1_height = window_height;
	int screen_1_area = screen_1_width * screen_1_height;
	Pixel *screen_1 = (Pixel*)calloc(screen_1_area, sizeof(Pixel));

	//init screen 2
	int screen_2_x = screen_1_width;
	int screen_2_y = 0;
	int screen_2_width = 200;
	int screen_2_height = window_height;
	int screen_2_area = screen_2_width * screen_2_height;
	Pixel *screen_2 = (Pixel*)calloc(screen_2_area, sizeof(Pixel));

	//buttons init
	int n_buttons = 5;
	Rect* buttons = (Rect*)calloc(n_buttons, sizeof(Rect));
	for (int i = 0; i < n_buttons; i++)
	{
		buttons[i].w = 50;
		buttons[i].h = 30;
		buttons[i].x = (1.0 / (n_buttons / 2 + 1)) * ((i / ((n_buttons / 2) + 1)) + 1.0) * screen_2_width - 0.5 * buttons[i].w;
		buttons[i].y = (screen_2_height * 0.5 - buttons[i].h * n_buttons) / (n_buttons + 1) * (i + 1) + buttons[i].h * (i % 3);
		buttons[i].color.r = 50;
		buttons[i].color.g = 100;
		buttons[i].color.b = 255;
		buttons[i].color.a = 255;
	}

	Gaussian_Set g;
	g.n_data = 4;
	g.data = (float**)malloc(g.n_data * sizeof(float*));
	g.mean = (Vec2D*)calloc(g.n_data, sizeof(Vec2D));
	g.std_dev = (Vec2D*)calloc(g.n_data, sizeof(Vec2D));
	g.min = (float*)calloc(g.n_data, sizeof(float));
	g.max = (float*)calloc(g.n_data, sizeof(float));
	for (int i = 0; i < g.n_data; i++)
	{
		g.width = screen_1_width;
		g.height = screen_1_height;
		g.data[i] = (float*)calloc(g.width * g.height, sizeof(float));
		g.mean[i].x = 0.0;
		g.mean[i].y = 0.0;
		g.std_dev[i].x = 0.0;
		g.std_dev[i].y = 0.0;
		g.min[i] = 1.0;
		g.max[i] = 0.0;
	}

	Slider g_sliders[6];
	for (int i = 0; i < 6; i++)
	{
		g_sliders[i].marker.w = 10;
		g_sliders[i].marker.h = 30;
		g_sliders[i].marker.x = 0.5 * (screen_2_width - g_sliders[i].marker.w);
		g_sliders[i].marker.y = ((screen_2_height * 0.5 - g_sliders[i].marker.h * 6) / (6 + 1) * (i + 1) + g_sliders[i].marker.h * i) + screen_2_height * 0.5;
		g_sliders[i].marker.color.r = 50;
		g_sliders[i].marker.color.g = 100;
		g_sliders[i].marker.color.b = 255;
		g_sliders[i].marker.color.a = 255;

		g_sliders[i].range.w = 100;
		g_sliders[i].range.h = 2;
		g_sliders[i].range.x = 0.5 * (screen_2_width - g_sliders[i].range.w);
		g_sliders[i].range.y = g_sliders[i].marker.y + 0.5 * g_sliders[i].marker.h - g_sliders[i].range.h * 0.5;
		g_sliders[i].range.color.r = 0;
		g_sliders[i].range.color.g = 0;
		g_sliders[i].range.color.b = 0;
		g_sliders[i].range.color.a = 100;

		g_sliders[i].position = 0.25 + 0.5 * i;
	}

	float* g_demo = (float*)calloc(screen_1_area, sizeof(float));

	int mouse_x = 0;
	int mouse_y = 0;
	int mouse_l = 0;
	int mouse_prev_l = 0;
	int selected_slider = -1;
	char button_pressed = 0;

	for (;;)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event) == 1)
		{
			SDL_GetMouseState(&mouse_x, &mouse_y);
			if (event.type == SDL_QUIT) exit(0);
			else if (event.type == SDL_KEYDOWN)
			{
				char key = event.key.keysym.sym;
				if (key == SDLK_ESCAPE)
				{
					exit(0);
				}
			}
			else if (event.type == SDL_MOUSEBUTTONDOWN)
			{
				mouse_l = 1;
			}
			else if (event.type == SDL_MOUSEBUTTONUP)
			{
				mouse_l = 0;
			}
		}

		if (mouse_l == 0 && mouse_prev_l == 1)
		{
			for (int i = 0; i < n_buttons; i++)
			{
				if (mouse_x >= buttons[i].x + screen_2_x && mouse_x <= buttons[i].x + buttons[i].w + screen_2_x)
				{
					if (mouse_y >= buttons[i].y + screen_2_y && mouse_y <= buttons[i].y + buttons[i].h + screen_2_y)
					{
						button_pressed = i;
						break;
					}
				}
			}
		}
		else if (mouse_l == 1 && mouse_prev_l == 0)
		{
			for (int i = 0; i < 6; i++)
			{
				if (mouse_x >= g_sliders[i].marker.x + screen_2_x && mouse_x <= g_sliders[i].marker.x + g_sliders[i].marker.w + screen_2_x)
				{
					if (mouse_y >= g_sliders[i].marker.y + screen_2_y && mouse_y <= g_sliders[i].marker.y + g_sliders[i].marker.h + screen_2_y)
					{
						selected_slider = i;
						break;
					}
				}
			}
		}

		if (selected_slider >= 0)
		{
			if (mouse_l == 1)
			{
				if (mouse_x > screen_2_x + g_sliders[selected_slider].range.x && mouse_x < screen_2_x + g_sliders[selected_slider].range.x + g_sliders[selected_slider].range.w)
				{
					g_sliders[selected_slider].marker.x = mouse_x - screen_2_x - (g_sliders[selected_slider].marker.w * 0.5);
				}
				g_sliders[selected_slider].position = 0.5 * ((g_sliders[selected_slider].marker.x + 0.5 * g_sliders[selected_slider].marker.w) - g_sliders[selected_slider].range.x) / (g_sliders[selected_slider].range.w - g_sliders[selected_slider].range.x);

			}
			else
			{
				if (selected_slider == 0) g.mean[button_pressed].x = g_sliders[selected_slider].position * (float)g.width;
				else if (selected_slider == 1) g.mean[button_pressed].y = g_sliders[selected_slider].position * (float)g.height;
				else if (selected_slider == 2) g.std_dev[button_pressed].x = g_sliders[selected_slider].position * (float)g.width * 0.5;
				else if (selected_slider == 3) g.std_dev[button_pressed].y = g_sliders[selected_slider].position * (float)g.height * 0.5;
				else if (selected_slider == 4) g.min[button_pressed] = g_sliders[selected_slider].position;
				else if (selected_slider == 5) g.max[button_pressed] = g_sliders[selected_slider].position;
				set_2D_Gaussian(g.data[button_pressed], g.width, g.height, &g.std_dev[button_pressed], &g.mean[button_pressed], g.min[button_pressed], g.max[button_pressed]);
				merge_2D_Gaussians(&g, g_demo);
				printf("%f\n", g_demo[10000]);
				selected_slider = -1;
			}
		}
		else if (button_pressed == n_buttons - 1)
		{
			SDL_Surface *map_surface = SDL_CreateRGBSurfaceWithFormat(0, screen_1_width, screen_1_height, 32, SDL_PIXELFORMAT_RGBA32);
			Pixel *s = (Pixel*)map_surface->pixels;
			for (int i = 0; i < screen_1_area; i++) s[i].a = 255;
			draw_Screen(screen_1, s, 0, 0, screen_1_width, screen_1_height, screen_1_width, screen_1_height);
			IMG_SavePNG(map_surface, "map.png");
			printf("Map saved!\n");
			button_pressed = 0;
		}
		else if (button_pressed < n_buttons - 1)
		{
			g_sliders[0].marker.x = g_sliders[0].range.w * g.mean[button_pressed].x / (float)g.width + g_sliders[0].range.x - (g_sliders[0].marker.w * 0.5);
			g_sliders[1].marker.x = g_sliders[1].range.w * g.mean[button_pressed].y / (float)g.height + g_sliders[1].range.x - (g_sliders[1].marker.w * 0.5);
			g_sliders[2].marker.x = g_sliders[2].range.w * 2.0 * g.std_dev[button_pressed].x / (float)g.width + g_sliders[2].range.x - (g_sliders[2].marker.w * 0.5);
			g_sliders[3].marker.x = g_sliders[3].range.w * 2.0 * g.std_dev[button_pressed].y / (float)g.height + g_sliders[3].range.x - (g_sliders[3].marker.w * 0.5);
			g_sliders[4].marker.x = g_sliders[4].range.w * g.min[button_pressed] + g_sliders[4].range.x - (g_sliders[4].marker.w * 0.5);
			g_sliders[5].marker.x = g_sliders[5].range.w * g.max[button_pressed] + g_sliders[5].range.x - (g_sliders[5].marker.w * 0.5);
			for (int i = 0; i < 6; i++)
			{
				g_sliders[i].position = 0.5 * ((g_sliders[i].marker.x + 0.5 * g_sliders[i].marker.w) - g_sliders[i].range.x) / (g_sliders[i].range.w - g_sliders[i].range.x);
			}
		}

		mouse_prev_l = mouse_l;

		//screen 1
		wipe_Screen(screen_1, screen_1_area, black);

		for (int i = 0; i < screen_1_area; i++)
		{
			float *shade = g_demo;

			screen_1[i].r = shade[i] * 255;
			screen_1[i].g = shade[i] * 255;
			screen_1[i].b = shade[i] * 255;
		}
		draw_Screen(screen_1, p, screen_1_x, screen_1_y, screen_1_width, screen_1_height, window_width, window_height);

		//screen 2
		wipe_Screen(screen_2, screen_2_area, ui_back);
		for (int i = 0; i < n_buttons; i++) draw_Rect(screen_2, screen_2_width, screen_2_height, &buttons[i]);

		for (int i = 0; i < 6; i++)
		{
			draw_Rect(screen_2, screen_2_width, screen_2_height, &g_sliders[i].range);
			draw_Rect(screen_2, screen_2_width, screen_2_height, &g_sliders[i].marker);
		}
		draw_Screen(screen_2, p, screen_2_x, screen_2_y, screen_2_width, screen_2_height, window_width, window_height);

		SDL_BlitScaled(work_surface, NULL, print_surface, NULL);

		SDL_UpdateWindowSurface(window);
	}
}