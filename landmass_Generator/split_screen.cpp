#include <stdio.h>
#include <math.h>
#include <time.h>
#include <Windows.h>
#include <assert.h>
using namespace std;

#include "SDL2-2.0.8\include\SDL.h"
#include "SDL2-2.0.8\include\SDL_image.h"
#pragma comment(lib, "SDL2-2.0.8\\lib\\x86\\SDL2.lib")
#pragma comment(lib, "SDL2-2.0.8\\lib\\x86\\SDL2main.lib")
#pragma comment(lib, "SDL2-2.0.8\\lib\\x86\\SDL2_image.lib")

//#define SPAWN_CONSOLE
#ifdef SPAWN_CONSOLE
#pragma comment(linker, "/subsystem:console")
#else
#pragma comment(linker, "/subsystem:windows")
#endif

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

struct Lattice
{
	int frequency;
	Vec2D* point;
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
	float* strength;
};

void init_Lattice(Lattice *set)
{
	for (int y = 0; y < set->frequency; y++)
	{
		for (int x = 0; x < set->frequency; x++)
		{
			int i = y * set->frequency + x;
			set->point[i].x = 1.0 - 2.0 * (float)rand() / RAND_MAX;
			set->point[i].y = 1.0 - 2.0 * (float)rand() / RAND_MAX;

			float magnitude = sqrt(set->point[i].x * set->point[i].x + set->point[i].y * set->point[i].y);
			set->point[i].x /= magnitude;
			set->point[i].y /= magnitude;
		}
	}
}

float dot_Gradient(int ix, int iy, float x, float y, Vec2D *point)
{
	float dx = x - (float)ix;
	float dy = y - (float)iy;

	return (dx * point->x + dy * point->y);
}

float linear_Interp(float a, float b, float w)
{
	return a + w * (b - a);
}

void generate_Noise(float* noise, const Lattice* set, int requested_frequency, int oct, int width, int height)
{
	Vec2D scale;
	scale.x = (float)requested_frequency / width;
	scale.y = (float)requested_frequency / height;


	float lowest = 1.0;
	float highest = -1.0;
	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			int screen_pos = y * width + x;
			noise[screen_pos] = 0;
			for (int o = 0; o < oct; o++)
			{
				float fx = (float)x * scale.x;
				float fy = (float)y * scale.y;

				int x0 = (int)(fx - fmod(fx, pow(2, o)));
				int x1 = x0 + pow(2, o);
				int y0 = (int)(fy - fmod(fy, pow(2, o)));
				int y1 = y0 + pow(2, o);

				float sx = fx - (float)x0;
				float sy = fy - (float)y0;

				sx /= pow(2, o);
				sx = sx * sx * sx * (sx * (sx * 6 - 15) + 10);

				sy /= pow(2, o);
				sy = sy * sy * sy * (sy * (sy * 6 - 15) + 10);

				float n0 = dot_Gradient(0, 0, sx, sy, &set->point[y0 * set->frequency + x0]);
				float n1 = dot_Gradient(1, 0, sx, sy, &set->point[y0 * set->frequency + x1]);
				float ix0 = linear_Interp(n0, n1, sx);
				n0 = dot_Gradient(0, 1, sx, sy, &set->point[y1 * set->frequency + x0]);
				n1 = dot_Gradient(1, 1, sx, sy, &set->point[y1 * set->frequency + x1]);
				float ix1 = linear_Interp(n0, n1, sx);

				noise[screen_pos] /= 2;
				noise[screen_pos] += linear_Interp(ix0, ix1, sy);

			}
			if (noise[screen_pos] < lowest) lowest = noise[screen_pos];
			if (noise[screen_pos] > highest) highest = noise[screen_pos];
		}
	}
	for (int i = 0; i < width * height; i++)
	{
		noise[i] = (noise[i] - lowest) / (highest - lowest);
	}
}

void set_2D_Gaussian(float* data, int width, int height, const Vec2D* std_dev, const Vec2D* mean, float min, float max)
{
	if (max == 0.0 && min == 0.0) return;

	int area = width * height;

	if (max < min) max = min;
	else if (max > 1.0) max = 1.0;

	if (min < 0.0) min = 0.0;
	else if (min > max) min = max;

	float lowest = 1.0;
	float highest = 0.0;
	for (int i = 0; i < area; i++)
	{
		int x = i % width;
		int y = i / width;
		float curve = 0.0;
		curve = 1000.0 * exp(-0.5 * (((float)x - mean->x) / std_dev->x) * (((float)x - mean->x) / std_dev->x)) / (std_dev->x * sqrt(2.0 * M_PI));
		curve *= 1000.0 * exp(-0.5 * (((float)y - mean->y) / std_dev->y) * (((float)y - mean->y) / std_dev->y)) / (std_dev->y * sqrt(2.0 * M_PI));
		data[i] = curve;
		if (curve < lowest) lowest = curve;
		if (curve > highest) highest = curve;
	}
	for (int i = 0; i < area; i++)
	{
		data[i] *= (max - min) / highest;
		data[i] += min;
	}
}

void merge_2D_Gaussians(const Gaussian_Set* src, float* dest)
{
	float str_sum = 0.0;
	for (int i = 0; i < src->n_data; i++) str_sum += src->strength[i];
	for (int i = 0; i < src->width * src->height; i++)
	{
		dest[i] = 0.0;
		for (int j = 0; j < src->n_data; j++) dest[i] += (src->strength[j] / str_sum) * src->data[j][i];
	}
}

void apply_Layer(float* dest, const float* src, float area, float strength)
{
	float lowest = 1.0;
	float highest = 0.0;
	for (int i = 0; i < area; i++)
	{
		dest[i] += strength * (src[i]);
		if (dest[i] > highest) highest = dest[i];
		if (dest[i] < lowest) lowest = dest[i];
	}
	//for (int i = 0; i < area; i++) dest[i] = (dest[i] - lowest) / (highest - lowest);
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

void draw_String(SDL_Surface* dest_s, SDL_Surface* src_s, int dx, int dy, int dw, int sw, char* data)
{
	SDL_Rect dest_r;
	dest_r.x = dx;
	dest_r.y = dy;
	dest_r.w = dw;
	dest_r.h = dw * 1.5;

	SDL_Rect src_r;
	src_r.w = sw;
	src_r.h = sw * 1.5;

	int n_data = strlen(data);

	for (int i = 0; i < n_data; i++)
	{
		src_r.x = src_r.w * (data[i] % 16);//col
		src_r.y = src_r.h * (data[i] / 16);//row

		SDL_BlitScaled(src_s, &src_r, dest_s, &dest_r);
		dest_r.x += dest_r.w;
	}
}

void scale_Array(float* dest, float* src, int dest_w, int dest_h, int src_w, int src_h)
{
	Vec2D scale;
	scale.x = (float)src_w / (float)dest_w;
	scale.y = (float)src_h / (float)dest_h;

	for (int y = 0; y < dest_h; y++)
	{
		for (int x = 0; x < dest_w; x++)
		{
			int screen_pos = y * dest_w + x;
			dest[screen_pos] = 0.0;
			float fx = (float)x * scale.x;
			float fy = (float)y * scale.y;

			int x0 = (int)fx;
			int x1 = x0 + 1;
			int y0 = (int)fy;
			int y1 = y0 + 1;

			float sx = fx - (float)x0;
			float sy = fy - (float)y0;

			float ix0 = linear_Interp(src[y0 * src_w + x0], src[y0 * src_w + x1], sx);
			float ix1 = linear_Interp(src[y1 * src_w + x0], src[y1 * src_w + x1], sx);
			dest[screen_pos] = linear_Interp(ix0, ix1, sy);
		}
	}
}

int main(int argc, char** argv)
{
	SDL_Init(SDL_INIT_VIDEO);

	int window_width = 920;
	int window_height = 720;
	int window_area = window_width * window_height;

	SDL_Window *window = SDL_CreateWindow("mep", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, window_width, window_height, SDL_WINDOW_SHOWN);

	SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

	SDL_Surface *print_surface = SDL_GetWindowSurface(window);
	SDL_Surface *work_surface = SDL_CreateRGBSurfaceWithFormat(0, window_width, window_height, 32, SDL_PIXELFORMAT_RGBA32);
	Pixel *p = (Pixel*)work_surface->pixels;

	SDL_Surface *text_surface = IMG_Load("new_font_sheet.png");
	assert(text_surface);
	SDL_SetSurfaceAlphaMod(text_surface, 255);

	char test_text[5] = "1a2Z";
	test_text[4] = 0;

	int font_size = 15;
	const int title1_len = 8;
	char title1_label[title1_len + 1] = "Landmass";
	title1_label[title1_len] = 0;
	const int title2_len = 9;
	char title2_label[title2_len + 1] = "Generator";
	title2_label[title2_len] = 0;
	const int s_save_len = 5;
	char s_save_label[s_save_len + 1] = "sSave";
	s_save_label[s_save_len] = 0;
	const int b_save_len = 5;
	char b_save_label[b_save_len + 1] = "bSave";
	b_save_label[b_save_len] = 0;
	const int color_len = 5;
	char color_label[color_len + 1] = "Color";
	color_label[color_len] = 0;
	const int bw_len = 3;
	char bw_label[bw_len + 1] = "B&W";
	bw_label[bw_len] = 0;
	const int w_lev_len = 11;
	char w_lev_label[w_lev_len + 1] = "Water Level";
	w_lev_label[w_lev_len] = 0;
	const int a_lev_len = 12;
	char a_lev_label[a_lev_len + 1] = "Alpine Level";
	a_lev_label[a_lev_len] = 0;
	const int g_ln1_len = 13;
	char g_label_ln1[g_ln1_len + 1] = "Click to Edit";
	g_label_ln1[g_ln1_len] = 0;
	const int g_ln2_len = 9;
	char g_label_ln2[g_ln2_len + 1] = "Gaussians";
	g_label_ln2[g_ln2_len] = 0;
	const int x_pos_len = 10;
	char x_pos_label[x_pos_len + 1] = "X Position";
	x_pos_label[x_pos_len] = 0;
	const int y_pos_len = 10;
	char y_pos_label[y_pos_len + 1] = "Y Position";
	y_pos_label[y_pos_len] = 0;
	const int x_dist_len = 14;
	char x_dist_label[x_dist_len + 1] = "X Distribution";
	x_dist_label[x_dist_len] = 0;
	const int y_dist_len = 14;
	char y_dist_label[y_dist_len + 1] = "Y Distribution";
	y_dist_label[y_dist_len] = 0;
	const int min_len = 7;
	char min_label[min_len + 1] = "Minimum";
	min_label[min_len] = 0;
	const int max_len = 7;
	char max_label[max_len + 1] = "Maximum";
	max_label[max_len] = 0;
	const int str_len = 8;
	char str_label[str_len + 1] = "Strength";
	str_label[str_len] = 0;

	Pixel ui_back;
	ui_back.r = 50;
	ui_back.g = 200;
	ui_back.b = 200;
	ui_back.a = 255;

	Pixel black;
	black.r = 0;
	black.g = 0;
	black.b = 0;
	black.a = 255;

	//init screen 1
	int screen_1_x = 0;
	int screen_1_y = 0;
	int screen_1_width = window_width - 200;
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
	char gaussians_text[4][3];
	for (int i = 0; i < 4; i++)
	{
		gaussians_text[i][0] = 'G';
		gaussians_text[i][1] = i + 49;
		gaussians_text[i][2] = 0;
	}
	const int n_sys_buttons = 4;
	const int cols = 2;
	const int sys_rows = n_sys_buttons / cols;
	Rect* sys_buttons = (Rect*)calloc(n_sys_buttons, sizeof(Rect));
	for (int i = 0; i < n_sys_buttons; i++)
	{
		sys_buttons[i].w = 80;
		sys_buttons[i].h = 30;
		sys_buttons[i].x = (i % 2) * (((screen_2_width - cols * sys_buttons[i].w) / (cols + 1)) + sys_buttons[i].w) + ((screen_2_width - cols * sys_buttons[i].w) / (cols + 1));
		sys_buttons[i].y = 40 * (i / cols) + 150;
		sys_buttons[i].color.r = 50;
		sys_buttons[i].color.g = 50;
		sys_buttons[i].color.b = 255;
		sys_buttons[i].color.a = 255;
	}

	const int n_buttons = 4;
	const int rows = n_buttons / cols;
	Rect* buttons = (Rect*)calloc(n_buttons, sizeof(Rect));
	for (int i = 0; i < n_buttons; i++)
	{
		buttons[i].w = 80;
		buttons[i].h = 30;
		buttons[i].x = (i % 2) * (((screen_2_width - cols * buttons[i].w) / (cols + 1)) + buttons[i].w) + ((screen_2_width - cols * buttons[i].w) / (cols + 1));
		buttons[i].y = 290 + 40 * (i / cols);
		buttons[i].color.r = 50;
		buttons[i].color.g = 100;
		buttons[i].color.b = 255;
		buttons[i].color.a = 255;
	}

	//sliders init

	static Slider elevs[2];
	for (int i = 0; i < 2; i++)
	{
		elevs[i].marker.w = 10;
		elevs[i].marker.h = 30;
		elevs[i].marker.x = 0.5 * (screen_2_width - elevs[i].marker.w);
		elevs[i].marker.y = elevs[i].marker.h * i + 10 * (i + 7);
		elevs[i].marker.color.r = 50;
		elevs[i].marker.color.g = 100;
		elevs[i].marker.color.b = 255;
		elevs[i].marker.color.a = 255;

		elevs[i].range.w = 100;
		elevs[i].range.h = 2;
		elevs[i].range.x = 0.5 * (screen_2_width - elevs[i].range.w);
		elevs[i].range.y = elevs[i].marker.y + 0.5 * elevs[i].marker.h - elevs[i].range.h * 0.5;
		elevs[i].range.color.r = 0;
		elevs[i].range.color.g = 0;
		elevs[i].range.color.b = 0;
		elevs[i].range.color.a = 100;

		elevs[i].position = 0.25 + 0.5 * i;
	}

	float water_level = elevs[0].position;
	float alpine_level = elevs[1].position;


	//Perlin init
	Lattice main_perlin;
	main_perlin.frequency = 300;
	int requested_frequency = 256;
	main_perlin.point = (Vec2D*)malloc(sizeof(Vec2D) * main_perlin.frequency * main_perlin.frequency);
	int seed = time(0);

	srand(seed);

	init_Lattice(&main_perlin);

	float* e6_noise = (float*)calloc(screen_1_area, sizeof(float));
	generate_Noise(e6_noise, &main_perlin, requested_frequency, 6, screen_1_width, screen_1_height);

	float* e6g_noise = (float*)calloc(screen_1_area, sizeof(float));

	Gaussian_Set g;
	g.n_data = 4;
	g.data = (float**)malloc(g.n_data * sizeof(float*));
	g.mean = (Vec2D*)calloc(g.n_data, sizeof(Vec2D));
	g.std_dev = (Vec2D*)calloc(g.n_data, sizeof(Vec2D));
	g.min = (float*)calloc(g.n_data, sizeof(float));
	g.max = (float*)calloc(g.n_data, sizeof(float));
	g.strength = (float*)calloc(g.n_data, sizeof(float));
	for (int i = 0; i < g.n_data; i++)
	{
		g.width = screen_1_width;
		g.height = screen_1_height;
		g.data[i] = (float*)calloc(g.width * g.height, sizeof(float));
		g.mean[i].x = g.width / 2.0;
		g.mean[i].y = g.height / 2.0;
		g.std_dev[i].x = 0.1;
		g.std_dev[i].y = 0.1;
		g.min[i] = 0.0;
		g.max[i] = 1.0;
		g.strength[i] = 0.1;
	}
	g.width = screen_1_width;
	g.height = screen_1_height;
	g.data[0] = (float*)calloc(g.width * g.height, sizeof(float));
	g.std_dev[0].x = g.width / 4.0;
	g.std_dev[0].y = g.height / 4.0;
	g.strength[0] = 1.0;

	int n_sliders = 7;
	Slider* g_sliders = (Slider*)malloc(n_sliders * sizeof(Slider));
	for (int i = 0; i < n_sliders; i++)
	{
		g_sliders[i].marker.w = 10;
		g_sliders[i].marker.h = 30;
		g_sliders[i].marker.x = 0.5 * (screen_2_width - g_sliders[i].marker.w);
		g_sliders[i].marker.y = ((screen_2_height * 0.5 - g_sliders[i].marker.h * n_sliders) / (n_sliders + 1) * (i + 1) + g_sliders[i].marker.h * i) + screen_2_height * 0.5;
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

	int mouse_x = 0;
	int mouse_y = 0;
	int mouse_l = 0;
	int mouse_prev_l = 0;
	int selected_slider = -3;
	char button_pressed = 0;
	char save_button_pressed = -1;
	char color_toggle = 0;

	merge_2D_Gaussians(&g, e6g_noise);
	apply_Layer(e6g_noise, e6_noise, screen_1_area, 0.25);

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
			for (int i = 0; i < n_sys_buttons / 2; i++)
			{
				if (mouse_x >= sys_buttons[i].x + screen_2_x && mouse_x <= sys_buttons[i].x + sys_buttons[i].w + screen_2_x)
				{
					if (mouse_y >= sys_buttons[i].y + screen_2_y && mouse_y <= sys_buttons[i].y + sys_buttons[i].h + screen_2_y)
					{
						save_button_pressed = i;
						break;
					}
				}
			}
			for (int i = n_sys_buttons / 2; i < n_sys_buttons; i++)
			{
				if (mouse_x >= sys_buttons[i].x + screen_2_x && mouse_x <= sys_buttons[i].x + sys_buttons[i].w + screen_2_x)
				{
					if (mouse_y >= sys_buttons[i].y + screen_2_y && mouse_y <= sys_buttons[i].y + sys_buttons[i].h + screen_2_y)
					{
						sys_buttons[color_toggle + 2].color.g = 50;
						sys_buttons[color_toggle + 2].color.b = 255;
						color_toggle = i - 2;
						break;
					}
				}
			}
		}
		else if (mouse_l == 1)
		{
			for (int i = 0; i < n_sys_buttons / 2; i++)
			{
				if (mouse_x >= sys_buttons[i].x + screen_2_x && mouse_x <= sys_buttons[i].x + sys_buttons[i].w + screen_2_x)
				{
					if (mouse_y >= sys_buttons[i].y + screen_2_y && mouse_y <= sys_buttons[i].y + sys_buttons[i].h + screen_2_y)
					{
						sys_buttons[i].color.g = 225;
						sys_buttons[i].color.b = 0;
						break;
					}
				}
			}
			if (mouse_prev_l == 0);
			{
				{
					for (int i = 0; i < 2; i++)
					{
						if (mouse_x >= elevs[i].marker.x + screen_2_x && mouse_x <= elevs[i].marker.x + elevs[i].marker.w + screen_2_x)
						{
							if (mouse_y >= elevs[i].marker.y + screen_2_y && mouse_y <= elevs[i].marker.y + elevs[i].marker.h + screen_2_y)
							{
								selected_slider = i - 2;
								break;
							}
						}
					}

					for (int i = 0; i < n_sliders; i++)
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
			}
		}
		else
		{
			for (int i = 0; i < 2; i++)
			{
				sys_buttons[i].color.g = 50;
				sys_buttons[i].color.b = 255;
			}
		}
		if (selected_slider >= -2)
		{
			if (mouse_l == 1)
			{
				if (selected_slider < 0)
				{
					selected_slider += 2;
					if (mouse_x > screen_2_x + elevs[selected_slider].range.x && mouse_x < screen_2_x + elevs[selected_slider].range.x + elevs[selected_slider].range.w)
					{
						elevs[selected_slider].marker.x = mouse_x - screen_2_x - (elevs[selected_slider].marker.w * 0.5);
					}
					elevs[selected_slider].position = 0.25 * ((elevs[selected_slider].marker.x + 0.5 * elevs[selected_slider].marker.w) - elevs[selected_slider].range.x) / (elevs[selected_slider].range.w - elevs[selected_slider].range.x) + 0.5 * selected_slider;
					selected_slider -= 2;
				}
				else
				{
					if (mouse_x > screen_2_x + g_sliders[selected_slider].range.x && mouse_x < screen_2_x + g_sliders[selected_slider].range.x + g_sliders[selected_slider].range.w)
					{
						g_sliders[selected_slider].marker.x = mouse_x - screen_2_x - (g_sliders[selected_slider].marker.w * 0.5);
					}
					g_sliders[selected_slider].position = 0.5 * ((g_sliders[selected_slider].marker.x + 0.5 * g_sliders[selected_slider].marker.w) - g_sliders[selected_slider].range.x) / (g_sliders[selected_slider].range.w - g_sliders[selected_slider].range.x);
				}

				if (selected_slider == 0) g.mean[button_pressed].x = g_sliders[selected_slider].position * (float)g.width;
				else if (selected_slider == 1) g.mean[button_pressed].y = g_sliders[selected_slider].position * (float)g.height;
				else if (selected_slider == 2) g.std_dev[button_pressed].x = g_sliders[selected_slider].position * (float)g.width * 0.5;
				else if (selected_slider == 3) g.std_dev[button_pressed].y = g_sliders[selected_slider].position * (float)g.height * 0.5;
				else if (selected_slider == 4) g.min[button_pressed] = g_sliders[selected_slider].position;
				else if (selected_slider == 5) g.max[button_pressed] = g_sliders[selected_slider].position;
				else if (selected_slider == 6) g.strength[button_pressed] = g_sliders[selected_slider].position;
				set_2D_Gaussian(g.data[button_pressed], g.width, g.height, &g.std_dev[button_pressed], &g.mean[button_pressed], g.min[button_pressed], g.max[button_pressed]);
				merge_2D_Gaussians(&g, e6g_noise);
				apply_Layer(e6g_noise, e6_noise, screen_1_area, 0.25);
			}
			else
			{
				selected_slider = -3;
			}
		}
		if (save_button_pressed == 0)
		{
			SDL_Surface *map_surface = SDL_CreateRGBSurfaceWithFormat(0, screen_1_width, screen_1_height, 32, SDL_PIXELFORMAT_RGBA32);
			Pixel *s = (Pixel*)map_surface->pixels;
			for (int i = 0; i < screen_1_area; i++) s[i].a = 255;
			draw_Screen(screen_1, s, 0, 0, screen_1_width, screen_1_height, screen_1_width, screen_1_height);
			IMG_SavePNG(map_surface, "s_map.png");
			save_button_pressed = -1;
			SDL_FreeSurface(map_surface);
		}
		else if (save_button_pressed == 1)
		{
			int big_size = 2048;
			int big_area = big_size * big_size;
			SDL_Surface *map_surface = SDL_CreateRGBSurfaceWithFormat(0, big_size, big_size, 32, SDL_PIXELFORMAT_RGBA32);
			Pixel *s = (Pixel*)map_surface->pixels;
			for (int i = 0; i < big_area; i++) s[i].a = 255;
			float* dense_noise = (float*)calloc(big_area, sizeof(float));
			generate_Noise(dense_noise, &main_perlin, requested_frequency, 6, big_size, big_size);
			float* loose_gauss = (float*)calloc(screen_1_area, sizeof(float));
			merge_2D_Gaussians(&g, loose_gauss);
			float* dense_gauss = (float*)calloc(big_area, sizeof(float));
			scale_Array(dense_gauss, loose_gauss, big_size, big_size, screen_1_width, screen_1_height);
			apply_Layer(dense_gauss, dense_noise, big_area, 0.25);
			Pixel *b_map = (Pixel*)calloc(big_area, sizeof(Pixel));
			for (int i = 0; i < big_area; i++)
			{
				b_map[i].a = 255;
				if (color_toggle == 0)
				{
					b_map[i].r = dense_gauss[i] * 255;
					b_map[i].g = dense_gauss[i] * 255;
					b_map[i].b = dense_gauss[i] * 255;
				}
				else
				{
					if (dense_gauss[i] < water_level)
					{
						int brightness = dense_gauss[i] / (3.5 * water_level) * 255;

						b_map[i].r = brightness;
						b_map[i].g = brightness;
						b_map[i].b = ((dense_gauss[i] / (2 * water_level)) + 0.35) * 255;
					}
					else if (dense_gauss[i] < alpine_level)
					{
						b_map[i].r = (1 - (dense_gauss[i] / alpine_level)) * 255;
						b_map[i].g = (1 - (dense_gauss[i] / (2 * alpine_level))) * 255;
						b_map[i].b = (1 - (dense_gauss[i] / alpine_level)) * 5 / 8 * 255;
					}
					else
					{
						int brightness = (dense_gauss[i] - alpine_level) / (1 - alpine_level) * 255;

						b_map[i].r = brightness;
						b_map[i].g = ((dense_gauss[i] - alpine_level) / (2 * (1 - alpine_level)) + 0.5) * 255;
						b_map[i].b = brightness;
					}
				}
			}
			draw_Screen(b_map, s, 0, 0, big_size, big_size, big_size, big_size);
			IMG_SavePNG(map_surface, "b_map.png");
			save_button_pressed = -1;
			SDL_FreeSurface(map_surface);
		}

		if (button_pressed < n_buttons)
		{
			g_sliders[0].marker.x = g_sliders[0].range.w * g.mean[button_pressed].x / (float)g.width + g_sliders[0].range.x - (g_sliders[0].marker.w * 0.5);
			g_sliders[1].marker.x = g_sliders[1].range.w * g.mean[button_pressed].y / (float)g.height + g_sliders[1].range.x - (g_sliders[1].marker.w * 0.5);
			g_sliders[2].marker.x = g_sliders[2].range.w * 2.0 * g.std_dev[button_pressed].x / (float)g.width + g_sliders[2].range.x - (g_sliders[2].marker.w * 0.5);
			g_sliders[3].marker.x = g_sliders[3].range.w * 2.0 * g.std_dev[button_pressed].y / (float)g.height + g_sliders[3].range.x - (g_sliders[3].marker.w * 0.5);
			g_sliders[4].marker.x = g_sliders[4].range.w * g.min[button_pressed] + g_sliders[4].range.x - (g_sliders[4].marker.w * 0.5);
			g_sliders[5].marker.x = g_sliders[5].range.w * g.max[button_pressed] + g_sliders[5].range.x - (g_sliders[5].marker.w * 0.5);
			g_sliders[6].marker.x = g_sliders[6].range.w * g.strength[button_pressed] + g_sliders[6].range.x - (g_sliders[6].marker.w * 0.5);
			for (int i = 0; i < 6; i++)
			{
				g_sliders[i].position = 0.5 * ((g_sliders[i].marker.x + 0.5 * g_sliders[i].marker.w) - g_sliders[i].range.x) / (g_sliders[i].range.w - g_sliders[i].range.x);
			}

			for (int i = 0; i < n_buttons; i++)
			{
				buttons[i].color.g = 50;
				buttons[i].color.b = 255;
			}

			buttons[button_pressed].color.g = 225;
			buttons[button_pressed].color.b = 0;
			sys_buttons[color_toggle + 2].color.g = 225;
			sys_buttons[color_toggle + 2].color.b = 0;
		}

		water_level = elevs[0].position;
		alpine_level = elevs[1].position;

		mouse_prev_l = mouse_l;

		//draw
		//screen 1
		wipe_Screen(screen_1, screen_1_area, black);

		for (int i = 0; i < screen_1_area; i++)
		{
			float *shade = e6g_noise;

			if (color_toggle == 0)
			{
				screen_1[i].r = shade[i] * 255;
				screen_1[i].g = shade[i] * 255;
				screen_1[i].b = shade[i] * 255;
			}

			else
			{
				if (shade[i] < water_level)
				{
					int brightness = shade[i] / (3.5 * water_level) * 255;

					screen_1[i].r = brightness;
					screen_1[i].g = brightness;
					screen_1[i].b = ((shade[i] / (2 * water_level)) + 0.35) * 255;
				}
				else if (shade[i] < alpine_level)
				{
					screen_1[i].r = (1 - (shade[i] / alpine_level)) * 255;
					screen_1[i].g = (1 - (shade[i] / (2 * alpine_level))) * 255;
					screen_1[i].b = (1 - (shade[i] / alpine_level)) * 5 / 8 * 255;
				}
				else
				{
					int brightness = (shade[i] - alpine_level) / (1 - alpine_level) * 255;

					screen_1[i].r = brightness;
					screen_1[i].g = ((shade[i] - alpine_level) / (2 * (1 - alpine_level)) + 0.5) * 255;
					screen_1[i].b = brightness;
				}
			}
		}
		draw_Screen(screen_1, p, screen_1_x, screen_1_y, screen_1_width, screen_1_height, window_width, window_height);

		//screen 2
		wipe_Screen(screen_2, screen_2_area, ui_back);

		SDL_Rect src;
		src.w = 64;
		src.h = 96;

		SDL_Rect dest;
		dest.w = 20;
		dest.h = dest.w * 1.5;

		for (int i = 0; i < n_buttons; i++) draw_Rect(screen_2, screen_2_width, screen_2_height, &buttons[i]);
		for (int i = 0; i < n_sys_buttons; i++) draw_Rect(screen_2, screen_2_width, screen_2_height, &sys_buttons[i]);
		for (int i = 0; i < 2; i++)
		{
			draw_Rect(screen_2, screen_2_width, screen_2_height, &elevs[i].range);
			draw_Rect(screen_2, screen_2_width, screen_2_height, &elevs[i].marker);
		}
		for (int i = 0; i < n_sliders; i++)
		{
			draw_Rect(screen_2, screen_2_width, screen_2_height, &g_sliders[i].range);
			draw_Rect(screen_2, screen_2_width, screen_2_height, &g_sliders[i].marker);
		}
		draw_Screen(screen_2, p, screen_2_x, screen_2_y, screen_2_width, screen_2_height, window_width, window_height);

		//button labels
		font_size = 15;
		for (int i = 0; i < g.n_data; i++)
		{
			dest.x = buttons[i].x + (buttons[i].w / 2) - font_size + screen_2_x;
			dest.y = buttons[i].y + (buttons[i].h - font_size) * 0.5;

			draw_String(work_surface, text_surface, dest.x, dest.y, font_size, 64, gaussians_text[i]);
		}

		font_size = 22;
		dest.x = screen_2_x + screen_2_width * 0.5 - (font_size * strlen(title1_label) * 0.5);
		dest.y = 8;
		draw_String(work_surface, text_surface, dest.x, dest.y, font_size, 64, title1_label);
		dest.x = screen_2_x + screen_2_width * 0.5 - (font_size * strlen(title2_label) * 0.5);
		dest.y += font_size;
		draw_String(work_surface, text_surface, dest.x, dest.y, font_size, 64, title2_label);

		font_size = 15;
		dest.x = sys_buttons[0].x + (sys_buttons[0].w - font_size * s_save_len) * 0.5 + screen_2_x;
		dest.y = sys_buttons[0].y + (sys_buttons[0].h - font_size) * 0.5;
		draw_String(work_surface, text_surface, dest.x, dest.y, font_size, 64, s_save_label);
		dest.x = sys_buttons[1].x + (sys_buttons[1].w - font_size * b_save_len) * 0.5 + screen_2_x;
		dest.y = sys_buttons[1].y + (sys_buttons[1].h - font_size) * 0.5;
		draw_String(work_surface, text_surface, dest.x, dest.y, font_size, 64, b_save_label);
		dest.x = sys_buttons[2].x + (sys_buttons[2].w - font_size * bw_len) * 0.5 + screen_2_x;
		dest.y = sys_buttons[2].y + (sys_buttons[2].h - font_size) * 0.5;
		draw_String(work_surface, text_surface, dest.x, dest.y, font_size, 64, bw_label);
		dest.x = sys_buttons[3].x + (sys_buttons[3].w - font_size * color_len) * 0.5 + screen_2_x;
		dest.y = sys_buttons[3].y + (sys_buttons[3].h - font_size) * 0.5;
		draw_String(work_surface, text_surface, dest.x, dest.y, font_size, 64, color_label);
		dest.x = screen_2_x + elevs[0].range.x + 0.5 * (elevs[0].range.w - font_size * w_lev_len);
		dest.y = elevs[0].marker.y + (elevs[0].marker.h - font_size) * 0.5;
		draw_String(work_surface, text_surface, dest.x, dest.y, font_size, 64, w_lev_label);
		dest.x = screen_2_x + elevs[1].range.x + 0.5 * (elevs[1].range.w - font_size * a_lev_len);
		dest.y = elevs[1].marker.y + (elevs[1].marker.h - font_size) * 0.5;
		draw_String(work_surface, text_surface, dest.x, dest.y, font_size, 64, a_lev_label);

		dest.x = screen_2_x + screen_2_width * 0.5 - (font_size * strlen(g_label_ln1) * 0.5);
		dest.y = 250;
		draw_String(work_surface, text_surface, dest.x, dest.y, font_size, 64, g_label_ln1);
		dest.x = screen_2_x + screen_2_width * 0.5 - (font_size * strlen(g_label_ln2) * 0.5);
		dest.y += font_size;
		draw_String(work_surface, text_surface, dest.x, dest.y, font_size, 64, g_label_ln2);

		dest.x = screen_2_x + g_sliders[0].range.x + 0.5 * (g_sliders[0].range.w - font_size * x_pos_len);
		dest.y = g_sliders[0].marker.y + (g_sliders[0].marker.h - font_size) * 0.5;
		draw_String(work_surface, text_surface, dest.x, dest.y, font_size, 64, x_pos_label);
		dest.x = screen_2_x + g_sliders[1].range.x + 0.5 * (g_sliders[1].range.w - font_size * y_pos_len);
		dest.y = g_sliders[1].marker.y + (g_sliders[1].marker.h - font_size) * 0.5;
		draw_String(work_surface, text_surface, dest.x, dest.y, font_size, 64, y_pos_label);
		font_size = 12;
		dest.x = screen_2_x + g_sliders[2].range.x + 0.5 * (g_sliders[2].range.w - font_size * x_dist_len);
		dest.y = g_sliders[2].marker.y + (g_sliders[2].marker.h - font_size) * 0.5;
		draw_String(work_surface, text_surface, dest.x, dest.y, font_size, 64, x_dist_label);
		dest.x = screen_2_x + g_sliders[3].range.x + 0.5 * (g_sliders[3].range.w - font_size * y_dist_len);
		dest.y = g_sliders[3].marker.y + (g_sliders[3].marker.h - font_size) * 0.5;
		draw_String(work_surface, text_surface, dest.x, dest.y, font_size, 64, y_dist_label);
		font_size = 15;
		dest.x = screen_2_x + g_sliders[4].range.x + 0.5 * (g_sliders[4].range.w - font_size * min_len);
		dest.y = g_sliders[4].marker.y + (g_sliders[4].marker.h - font_size) * 0.5;
		draw_String(work_surface, text_surface, dest.x, dest.y, font_size, 64, min_label);
		dest.x = screen_2_x + g_sliders[5].range.x + 0.5 * (g_sliders[5].range.w - font_size * max_len);
		dest.y = g_sliders[5].marker.y + (g_sliders[5].marker.h - font_size) * 0.5;
		draw_String(work_surface, text_surface, dest.x, dest.y, font_size, 64, max_label);
		dest.x = screen_2_x + g_sliders[6].range.x + 0.5 * (g_sliders[6].range.w - font_size * str_len);
		dest.y = g_sliders[6].marker.y + (g_sliders[6].marker.h - font_size) * 0.5;
		draw_String(work_surface, text_surface, dest.x, dest.y, font_size, 64, str_label);

		SDL_BlitScaled(work_surface, NULL, print_surface, NULL);

		SDL_UpdateWindowSurface(window);
	}
	return 0;
}