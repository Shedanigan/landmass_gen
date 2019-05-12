#pragma warning (disable:4996)

#include <stdlib.h>
#include <stdio.h>
#include <Windows.h>
#include <time.h>
#include <math.h>

#include "SDL2-2.0.8\include\SDL.h"
#include "SDL2-2.0.8\include\SDL_image.h"
#pragma comment(lib, "SDL2-2.0.8\\lib\\x86\\SDL2.lib")
#pragma comment(lib, "SDL2-2.0.8\\lib\\x86\\SDL2main.lib")
#pragma comment(lib, "SDL2-2.0.8\\lib\\x86\\SDL2_image.lib")

struct Pixel
{
	char r, g, b, a;
};

struct Vec2D
{
	float x, y;
};

struct Lattice
{
	int frequency;
	Vec2D* point;
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

void noise(float* noise, const Lattice* set, int requested_frequency, int width, int height)
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
			float fx = (float)x * scale.x;
			float fy = (float)y * scale.y;

			int x0 = (int)fx;
			int x1 = x0 + 1;
			int y0 = (int)fy;
			int y1 = y0 + 1;

			float sx = fx - (float)x0;
			float sy = fy - (float)y0;

			float n0 = dot_Gradient(0, 0, sx, sy, &set->point[y0 * set->frequency + x0]);
			float n1 = dot_Gradient(1, 0, sx, sy, &set->point[y0 * set->frequency + x1]);
			float ix0 = linear_Interp(n0, n1, sx);
			n0 = dot_Gradient(0, 1, sx, sy, &set->point[y1 * set->frequency + x0]);
			n1 = dot_Gradient(1, 1, sx, sy, &set->point[y1 * set->frequency + x1]);
			float ix1 = linear_Interp(n0, n1, sx);

			noise[screen_pos] = linear_Interp(ix0, ix1, sy);
			if (noise[screen_pos] < lowest) lowest = noise[screen_pos];
			if (noise[screen_pos] > highest) highest = noise[screen_pos];
		}
	}
	for (int i = 0; i < width * height; i++)
	{
		noise[i] = (noise[i] - lowest) / (highest - lowest);
	}
}

void oct_Noise(float* noise, const Lattice* set, int requested_frequency, int oct, int width, int height)
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
				float fx = (float)x * scale.x / pow(2, o);
				float fy = (float)y * scale.y / pow(2, o);

				int x0 = (int)fx;
				int x1 = x0 + pow(2,o);
				int y0 = (int)fy;
				int y1 = y0 + pow(2, o);

				float sx = fx - (float)x0;
				float sy = fy - (float)y0;

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

void eased_Noise(float* noise, const Lattice* set, int requested_frequency, int width, int height)
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
			float fx = (float)x * scale.x;
			float fy = (float)y * scale.y;

			int x0 = (int)fx;
			int x1 = x0 + 1;
			int y0 = (int)fy;
			int y1 = y0 + 1;

			float sx = fx - (float)x0;
			float sy = fy - (float)y0;

			sx = sx * sx * sx * (sx * (sx * 6 - 15) + 10);
			sy = sy * sy * sy * (sy * (sy * 6 - 15) + 10);

			float n0 = dot_Gradient(0, 0, sx, sy, &set->point[y0 * set->frequency + x0]);
			float n1 = dot_Gradient(1, 0, sx, sy, &set->point[y0 * set->frequency + x1]);
			float ix0 = linear_Interp(n0, n1, sx);
			n0 = dot_Gradient(0, 1, sx, sy, &set->point[y1 * set->frequency + x0]);
			n1 = dot_Gradient(1, 1, sx, sy, &set->point[y1 * set->frequency + x1]);
			float ix1 = linear_Interp(n0, n1, sx);

			noise[screen_pos] = linear_Interp(ix0, ix1, sy);
			if (noise[screen_pos] < lowest) lowest = noise[screen_pos];
			if (noise[screen_pos] > highest) highest = noise[screen_pos];
		}
	}
	for (int i = 0; i < width * height; i++)
	{
		noise[i] = (noise[i] - lowest) / (highest - lowest);
	}
}

void eased_Oct_Noise(float* noise, const Lattice* set, int requested_frequency, int oct, int width, int height)
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

void variable_Kernel(float* dest, const float* src,  int src_width, int src_height, int kernel_size)
{
	for (int i = 0; i <= src_height - kernel_size; i++)
	{
		for (int j = 0; j <= src_width - kernel_size; j++)
		{
			int dest_index = (i + kernel_size / 2)*src_width + j + kernel_size / 2;
			dest[dest_index] = 0.0;
			for (int k = 0; k < kernel_size; k++)
			{
				for (int l = 0; l < kernel_size; l++)
				{
					dest[dest_index] += src[(i + k)*src_width + j + l];
				}
			}

			dest[dest_index] /= kernel_size * kernel_size;
		}
	}
}

void apply_2D_Gaussian(float* src, Vec2D std_dev, Vec2D mean, int width, int height)
{
	int area = width * height;
	float lowest = src[0];
	float highest = src[0];
	for (int i = 0; i < area; i++)
	{
		int x = i % width;
		int y = i / width;
		float curve = 0.0;
		curve = 225 * exp(-1 * (x - mean.x)*(x - mean.x) / (2 * std_dev.x * std_dev.x)) / sqrt(2 * M_PI * std_dev.x * std_dev.x) + 0.1;
		curve *= 225 * exp(-1 * (y - mean.y)*(y - mean.y) / (2 * std_dev.y * std_dev.y)) / sqrt(2 * M_PI * std_dev.y * std_dev.y) + 0.1;
		src[i] *= 0.432;
		src[i] += curve;
		if (src[i] > highest) highest = src[i];
		else if (src[i] < lowest) lowest = src[i];
	}
	for (int i = 0; i < area; i++)
	{
		src[i] = (src[i] - lowest) / (highest - lowest);
	}
}

void wmain()
{
	//SDL Init
	SDL_Init(SDL_INIT_VIDEO);

	int screen_width = 800;
	int screen_height = 600;
	int screen_area = screen_width * screen_height;

	SDL_Window *window = SDL_CreateWindow("lOuD nOiSeS", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, screen_width, screen_height, SDL_WINDOW_SHOWN);

	SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

	SDL_Surface *print_surface = SDL_GetWindowSurface(window);
	SDL_Surface *work_surface = SDL_CreateRGBSurfaceWithFormat(0, screen_width, screen_height, 32, SDL_PIXELFORMAT_RGBA32);

	Pixel *p = (Pixel*)work_surface->pixels;
	Pixel *canvas = (Pixel*)calloc(screen_area, sizeof(Pixel));
	for (int i = 0; i < screen_area; i++) canvas[i].a = 255;

	//Perlin init
	Lattice main_perlin;
	main_perlin.frequency = 300;
	int requested_frequency = 256;
	/*
	while (requested_frequency < 2 || requested_frequency > 101)
	{
		printf("Input Frequency(2-100): ");
		scanf("%d", &requested_frequency);
	}
	*/
	main_perlin.point = (Vec2D*)malloc(sizeof(Vec2D) * main_perlin.frequency * main_perlin.frequency);
	int seed = time(0);
	/*
	printf("Input seed(0+): ");
	scanf("%d", &seed);
	if (seed < 0) seed = time(0);
	*/
	srand(seed);
	system("cls");
	printf("Frequency: %d\n", requested_frequency);
	printf("Seed: %d\n", seed);

	init_Lattice(&main_perlin);

	float* e6_noise = (float*)calloc(screen_area, sizeof(float));
	eased_Oct_Noise(e6_noise, &main_perlin, requested_frequency, 6, screen_width, screen_height);

	float* e6g_noise = (float*)calloc(screen_area, sizeof(float));
	for (int i = 0; i < screen_area; i++) e6g_noise[i] = e6_noise[i];

	Vec2D landmass_std_dev;
	landmass_std_dev.x = screen_width / 6.0;
	landmass_std_dev.y = screen_height / 6.0;
	Vec2D landmass_mean;
	landmass_mean.x = screen_width / 2.0;
	landmass_mean.y = screen_height / 2.0;
	apply_2D_Gaussian(e6g_noise, landmass_std_dev, landmass_mean, screen_width, screen_height);

	int choice = 2;
	char color = 'c';
	int pics = 0;

	float water_level = 0.38;
	float alpine_level = 0.8;

	for (;;)
	{
		//SDL event loop
		SDL_Event event;
		while (SDL_PollEvent(&event) == 1)
		{
			if (event.type == SDL_QUIT) exit(0);
			else if (event.type == SDL_KEYDOWN)
			{
				if (event.key.keysym.sym == SDLK_ESCAPE)
				{
					exit(0);
				}
				else if (event.key.keysym.sym == SDLK_1)
				{
					choice = 1;
				}
				else if (event.key.keysym.sym == SDLK_2)
				{
					choice = 2;
				}
				else if (event.key.keysym.sym == SDLK_3)
				{
					//choice = 3;
				}
				else if (event.key.keysym.sym == SDLK_4)
				{
					//choice = 4;
				}
				else if (event.key.keysym.sym == SDLK_5)
				{
					//choice = 5;
				}
				else if (event.key.keysym.sym == SDLK_6)
				{
					//choice = 6;
				}
				else if (event.key.keysym.sym == SDLK_7)
				{
					//choice = 7;
				}
				else if (event.key.keysym.sym == SDLK_8)
				{
					//choice = 8;
				}
				else if (event.key.keysym.sym == SDLK_9)
				{
					//choice = 9;
				}
				else if (event.key.keysym.sym == SDLK_g)
				{
					color = 'g';
				}
				else if (event.key.keysym.sym == SDLK_c)
				{
					color = 'c';
				}
				else if (event.key.keysym.sym == SDLK_s)
				{
					IMG_SavePNG(print_surface, "map.png");
					printf("Map saved!\n");
				}
			}
		}
		//update
		for (int i = 0; i < screen_area; i++)
		{
			float *shade = NULL;
			if (choice == 1) shade = e6_noise;
			else if (choice == 2) shade = e6g_noise;

			if (color == 'g')
			{
				canvas[i].r = shade[i] * 255;
				canvas[i].g = shade[i] * 255;
				canvas[i].b = shade[i] * 255;
			}
			
			else if (color == 'c')
			{
				if (shade[i] < water_level)
				{
					int brightness = shade[i] / (3.5 * water_level) * 255;

					canvas[i].r = brightness;
					canvas[i].g = brightness;
					canvas[i].b = ((shade[i] / (2 * water_level)) + 0.35) * 255;
				}
				else if (shade[i] < alpine_level)
				{
					canvas[i].r = (1 - (shade[i] / alpine_level)) * 255;
					canvas[i].g = (1 - (shade[i] / (2 * alpine_level))) * 255;
					canvas[i].b = (1 - (shade[i] / alpine_level)) * 5 / 8 * 255;
				}
				else
				{
					int brightness = (shade[i] - alpine_level) / (1 - alpine_level) * 255;

					canvas[i].r = brightness;
					canvas[i].g = ((shade[i] - alpine_level) / (2 * (1 - alpine_level)) + 0.5) * 255;
					canvas[i].b = brightness;
				}

				/*if ((shade[i] - shade[i+1]) * (shade[i] - shade[i + 1]) > 0.00001)
				{
					canvas[i].r = 0;
					canvas[i].g = 0;
					canvas[i].b = 0;
				}*/
			}
		}

		memcpy(p, canvas, sizeof(Pixel) * screen_area);
		SDL_BlitScaled(work_surface, NULL, print_surface, NULL);

		SDL_UpdateWindowSurface(window);
	}
}