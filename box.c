#include <SDL2/SDL.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <Box2D/Box2D.h>
#include <math.h>
#include <list>
#include <vector>
#include <stdlib.h>
#include <time.h>

#define SCREEN_WIDTH 1920
#define SCREEN_HEIGHT 1080
#define METERS_TO_PIXELS 30.0f
#define TARGET_FPS 60

int screenTrueWidth = SCREEN_WIDTH;
int screenTrueHeigh = SCREEN_HEIGHT;

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
TTF_Font* textFont = NULL;
TTF_Font* writingFont = NULL;
SDL_Color textColorBlack = {32, 32, 32};
SDL_Color textColorWhite = {224, 224, 224};
char cmdBuffer[] = {0,0,0,0,0};
const char CHR_SPACE = 32; 
int gameMode = 1;
int cmdBufferLen = 4;
int cmdIdx = 0;
int clownMarginY = 240;
int ballMarginClownY = 180;
int ballMarginLumiY = 60;
int mouthMarginX = 160;
int clownPosY = screenTrueHeigh - clownMarginY;
int ballPosY = 0;
b2World* world = NULL;
float ballSize = 30.0f;
float gMod = 6.0f;
const int printChars = 255;
char printables[printChars];
const char* ballTxt[printChars];
const int textRows = 5;
const int textRowChars = 40;
int textAreaTextureWidth = 840;
int textAreaTextureHeight = 200;
char textArea[textRows][textRowChars+1];
int currentTextRow = 0;
int currentTextCol = 0;
int textRowIdle = 5000;
int lastTextTime = 0;

int colorPalettes[][3] =
	{{0,0,1},{0,1,0},{1,0,0},
	 {0,1,1},{1,0,1},{1,1,0}};

typedef struct PhysBall {
	b2Body* body;
	SDL_Texture* texture;
} PhysBall;
std::list<PhysBall*> activeObjects;

int max(int a, int b) {
	return a > b ? a : b;
}

int getBallMarginY() {
	if (gameMode == 1) {
		return ballMarginClownY;
	} else {
		return ballMarginLumiY;
	}
}

int getBallPosY() {
	return screenTrueHeigh - getBallMarginY();
}

void finishCurrentTextRow() {
	currentTextCol = 0;
	currentTextRow = (currentTextRow + 1) % textRows;
}

void backtrackCurrentText() {
	if (currentTextCol > 0) {
		currentTextCol--;
	} else {
		if (currentTextRow == 0) {
			currentTextRow = textRows;
		}
		currentTextRow--;
		currentTextCol = max(0, (int)(strchr(textArea[currentTextRow], 0) - textArea[currentTextRow]) - 1);
	}
	textArea[currentTextRow][currentTextCol] = 0;
}

void textInput(int chr) {
	lastTextTime = 0;
	printf("current chr: %x, %i\n", chr, chr);

	if (chr == 8) { // Backspace
		backtrackCurrentText();
		return;
	}

	textArea[currentTextRow][currentTextCol++] = printables[(0xFF&chr)];
	if (chr == 0x0D || currentTextCol >= textRowChars) {
		textArea[currentTextRow][currentTextCol] = 0;
		finishCurrentTextRow();
	}
	textArea[currentTextRow][currentTextCol] = 0;
}

b2Body* createCircle(b2World* world, float x, float y, float radius) {
	b2BodyDef bodyDef;
	bodyDef.type = b2_dynamicBody;
	bodyDef.position.Set(x / METERS_TO_PIXELS, y / METERS_TO_PIXELS);

	b2CircleShape circleShape;
	circleShape.m_radius = radius / METERS_TO_PIXELS;

	b2FixtureDef fixtureDef;
	fixtureDef.shape = &circleShape;
	fixtureDef.density = 1.0f;
	fixtureDef.friction = 0.3f;

	b2Body* body = world->CreateBody(&bodyDef);
	body->CreateFixture(&fixtureDef);

	return body;
}

SDL_Texture* createFilledCircleTexture(SDL_Renderer* renderer, int radius, int letter) {
	SDL_Surface* surface = SDL_CreateRGBSurface(0, 2 * radius, 2 * radius, 32, 0, 0, 0, 0);

	SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);
	SDL_SetColorKey(surface, SDL_TRUE, SDL_MapRGBA(surface->format, 0, 0, 0, 0));

	SDL_Renderer* surfaceRenderer = SDL_CreateSoftwareRenderer(surface);
	
	int colorPlace = rand() % 6;
	int r2 = radius * radius;
	int paletteA = colorPalettes[colorPlace][0];
	int paletteB = colorPalettes[colorPlace][1];
	int paletteC = colorPalettes[colorPlace][2];
	// Draw filled circle
	for (int x = -radius; x <= radius; ++x) {
		for (int y = -radius; y <= radius; ++y) {
			int x2 = x*x;
			int y2 = y*y;
			int sumxy2 = x2 + y2;
			if (sumxy2 <= r2) {
				float distMod = (sqrt(sumxy2)) / radius;
				int fadeColor = 255 - (int)(127.0f * distMod);
				int tmpColor[] = {fadeColor, 0};
				SDL_SetRenderDrawColor(surfaceRenderer,
					tmpColor[paletteA],
					tmpColor[paletteB],
					tmpColor[paletteC], 255);
				SDL_RenderDrawPoint(surfaceRenderer, radius + x, radius + y);
			}
		}
	}

	const char* targetTxt = ballTxt[letter % printChars]; // Default value used will be a white space
	SDL_Surface* textSurfaceW = TTF_RenderText_Solid(textFont, targetTxt, textColorWhite);
	SDL_Surface* textSurfaceB = TTF_RenderText_Solid(textFont, targetTxt, textColorBlack);

	SDL_Rect comboRectW = {(int)(radius*0.7),(int)(radius*0.7),radius,radius};
	SDL_Rect comboRectB = {(int)(radius*0.65),(int)(radius*0.65),radius,radius};
	SDL_BlitSurface(textSurfaceB, NULL, surface, &comboRectB);
	SDL_BlitSurface(textSurfaceW, NULL, surface, &comboRectW);
	SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);

	SDL_FreeSurface(surface);
	SDL_FreeSurface(textSurfaceW);
	SDL_FreeSurface(textSurfaceB);
	SDL_DestroyRenderer(surfaceRenderer);

	return texture;
}

SDL_Texture* createWritingTexture(SDL_Renderer* renderer) {
//	int textAreaWidth = (int)(0.6*screenTrueWidth);
//	int textAreaHeight = (int)(0.5*textAreaWidth);
//	SDL_Surface* surface = SDL_CreateRGBSurface(0, textAreaWidth, textAreaHeight, 32, 0, 0, 0, 0);
	SDL_Surface* surface = SDL_CreateRGBSurface(0, textAreaTextureWidth, textAreaTextureHeight, 32, 0, 0, 0, 0);

	SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);
	SDL_SetColorKey(surface, SDL_TRUE, SDL_MapRGBA(surface->format, 0, 0, 0, 0));

	for (int i = 0; i < textRows; i++) {
		int tIdx = (1 + currentTextRow + i + textRows) % textRows;
		SDL_Surface* textSurfaceW = TTF_RenderText_Solid(writingFont, textArea[tIdx], textColorWhite);
		SDL_Surface* textSurfaceB = TTF_RenderText_Solid(writingFont, textArea[tIdx], textColorBlack);

		SDL_Rect comboRectW = {0,0+30*i,textAreaTextureWidth,30};
		SDL_Rect comboRectB = {2,2+30*i,textAreaTextureWidth,30};

		SDL_BlitSurface(textSurfaceW, NULL, surface, &comboRectW);
		SDL_BlitSurface(textSurfaceB, NULL, surface, &comboRectB);

		SDL_FreeSurface(textSurfaceW);
		SDL_FreeSurface(textSurfaceB);
	}

	SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);

	SDL_FreeSurface(surface);

	return texture;
}

void renderFilledCircle(SDL_Renderer* renderer, SDL_Texture* texture, b2Body* body) {
	b2Vec2 position = body->GetPosition();
	float radius = ((b2CircleShape*)body->GetFixtureList()->GetShape())->m_radius * METERS_TO_PIXELS;

	SDL_Rect destinationRect = {
		(int)(position.x * METERS_TO_PIXELS - radius),
		(int)(position.y * METERS_TO_PIXELS - radius),
		(int)(2 * radius),
		(int)(2 * radius)
	};

	SDL_RenderCopy(renderer, texture, NULL, &destinationRect);
}

void syncScreenSize() {
	SDL_GetWindowSize(window, &screenTrueWidth, &screenTrueHeigh);
	screenTrueHeigh = max(0, screenTrueHeigh);
	screenTrueWidth = max(0, screenTrueWidth);
}

void initSDL() {
	SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO);
	window = SDL_CreateWindow(
		"Ballblower",
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		SCREEN_WIDTH, SCREEN_HEIGHT,
		SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN | SDL_WINDOW_SHOWN);
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
	SDL_ShowCursor(0);
}

void initBox2D() {
	b2Vec2 gravity(0.0f, gMod * 9.81f);
	world = new b2World(gravity);
	srand(time(NULL));
}

void cleanup() {
	for (PhysBall* physBall : activeObjects) {
		world->DestroyBody(physBall->body);
		SDL_DestroyTexture(physBall->texture);
	}

	if (world) {
		delete world;
		world = NULL;
	}

	if (renderer) {
		SDL_DestroyRenderer(renderer);
		renderer = NULL;
	}

	if (window) {
		SDL_DestroyWindow(window);
		window = NULL;
	}

	textFont = NULL;
	writingFont = NULL;

	SDL_Quit();
}

SDL_Texture* loadTexture(const char* path, SDL_Renderer* renderer) {
	SDL_Surface* surface = IMG_Load(path);
	if (!surface) {
		printf("image load failed from path: %s\n", path);
		// Handle error, e.g., print an error message
		return NULL;
	}

	SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
	if (!texture) {
		printf("creating texture from image failed. Image path: %s\n", path);
	}
	SDL_FreeSurface(surface);

	return texture;
}

void renderTexture(SDL_Texture* texture, SDL_Renderer* renderer, int x, int y, int w, int h) {
	SDL_Rect destinationRect = { x, y, w, h };
	SDL_RenderCopy(renderer, texture, NULL, &destinationRect);
}

//PhysBall* createPhysBall(int letter, float curLocation) {
PhysBall* createPhysBall(int letter, float x, float y) {
	PhysBall* newBall = (PhysBall*)malloc(sizeof(struct PhysBall));
	int ballMod = rand() % (int)ballSize;
	b2Body* b = createCircle(world, x, y, ballSize + (float)ballMod);
	SDL_Texture* t = createFilledCircleTexture(renderer, (int)ballSize + ballMod, letter);

	// Set the linear velocity for the body
	float speedMod = 0.6 + (rand() % 5000) / 5000.0;
	float dirMod = ((rand() % 1000) / 1000.0) * 16.0f;
	b2Vec2 velocity(speedMod * (8.0f - dirMod), - speedMod * 16.0f * sqrt(gMod));
	b->SetLinearVelocity(velocity);

	newBall->body = b;
	newBall->texture = t;
	return newBall;
}

int main() {
	initSDL();

  // Open audio device
  SDL_AudioSpec desired_spec;
  SDL_memset(&desired_spec, 0, sizeof(desired_spec));
  desired_spec.freq = 44100; // Set desired frequency (adjust as needed)
  desired_spec.format = AUDIO_S16LSB; // Set desired audio format
  desired_spec.channels = 2; // Set desired number of channels
  desired_spec.samples = 1024; // Set desired sample buffer size
  SDL_AudioSpec obtained_spec;
  SDL_AudioDeviceID device = SDL_OpenAudioDevice(NULL, 0, &desired_spec, &obtained_spec, 0);
  if (device == 0) {
    fprintf(stderr, "Failed to open audio device! SDL Error: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  // Load audio sample
  SDL_RWops* audio_file = SDL_RWFromFile("./quack.wav", "rb");
  if (audio_file == NULL) {
    fprintf(stderr, "Failed to open audio file: %s\n", "quack.wav");
    SDL_CloseAudioDevice(device);
    SDL_Quit();
    return 1;
  }
  SDL_AudioSpec wave_spec;

  Uint32 wave_length = SDL_RWseek(audio_file, 0, SEEK_END);
  SDL_RWseek(audio_file, 0, SEEK_SET);
  wave_spec.freq = obtained_spec.freq;
  wave_spec.format = obtained_spec.format;
  wave_spec.channels = obtained_spec.channels;
  wave_spec.samples = (Uint32)(wave_length / obtained_spec.size);
  wave_spec.callback = NULL; // We don't need a callback for this simple example
  wave_spec.userdata = NULL;
  void* wave_data = malloc(wave_length);
  if (wave_data == NULL) {
    fprintf(stderr, "Failed to allocate memory for audio data\n");
    SDL_RWclose(audio_file);
    SDL_CloseAudioDevice(device);
    SDL_Quit();
    return 1;
  }
  SDL_RWread(audio_file, wave_data, 1, wave_length);
	
	// Init text for keys
	for (int i = 0; i < printChars; i++) {
		char* tmpStr = (char*)malloc(2*sizeof(char));
		if ((i >= 'a' && i <= 'z') || i == 0xf6 || i == 0xe4 || i == 0xe5) {
			tmpStr[0] = i - 'a' + 'A';
		} else {
			tmpStr[0] = max(i, CHR_SPACE);
		}
		tmpStr[1] = 0;
		ballTxt[i] = tmpStr;
		printables[i] = tmpStr[0];
	}

	// Init text areas
	for (int i = 0; i < textRows; i++) {
		for (int j = 0; j < textRowChars; j++) {
			textArea[i][j] = CHR_SPACE;
		}
	}

	clownPosY = screenTrueHeigh - clownMarginY;
	ballPosY = getBallPosY();

	// Initialize SDL_ttf
	if (TTF_Init() != 0) {
		// Handle TTF initialization failure
		SDL_Quit();
		return 1;
	}

	textFont = TTF_OpenFont("./AndikaCompact-Regular.ttf", 32);
	writingFont = TTF_OpenFont("./AndikaCompact-Regular.ttf", 24);
	if (!textFont || !writingFont) {
		// Handle font loading failure
		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(window);
		TTF_Quit();
		SDL_Quit();
		printf("Fonts could not be loaded!\n");
		return 1;
	}

	SDL_Texture* clownTexture = loadTexture("./clown.png", renderer);
	SDL_Texture* lumiTexture = loadTexture("./lumi.png", renderer);
	SDL_Texture* textAreaTexture = NULL;

	initBox2D();
	std::vector<PhysBall*> bodiesToDestroy;

	bool quit = false;
	float worldTime = 1.0f / 60.0f;
	Uint32 targetMs = 1000 / TARGET_FPS;
	SDL_Event e;
	Uint32 frameStart, frameTime;
	int mouseX = 0;
	int mouseY = 0;

	if (!clownTexture || !lumiTexture) {
		printf("Could not load images.\n");
		quit = true;
	}

	char lastCommand[] = {'a','a','a','a',0};
	
	while (!quit) {
		frameStart = SDL_GetTicks(); // Get the current time at the beginning of the frame
		SDL_GetMouseState(&mouseX, &mouseY);
		float currentXPos = (float)mouseX;
		float currentYPos = (float)(mouseY + getBallMarginY());
		int letter = 0;
		if (gameMode == 1) {
			currentXPos = (float)(screenTrueWidth * (0.5f + 0.5f * sin((float)SDL_GetTicks() / 1000.0f)));
			currentYPos = (float)getBallPosY();
		}
		while (SDL_PollEvent(&e) != 0) {
			if (e.type == SDL_QUIT) {
				quit = true;
			}
			else if (e.type == SDL_KEYDOWN) {
				letter = e.key.keysym.sym;
				letter = letter > printChars ? CHR_SPACE : letter;
				cmdBuffer[cmdIdx++ % cmdBufferLen] = letter;
				textInput(letter);
				if (textAreaTexture != NULL)
					SDL_DestroyTexture(textAreaTexture);
				textAreaTexture = createWritingTexture(renderer);
			}
			else if (e.type == SDL_MOUSEBUTTONDOWN) {
				if (e.button.button == SDL_BUTTON_RIGHT) {
					gameMode = gameMode ^ 1;
				} else {
					letter = 65 + (rand() % 26);
				}
			}
			else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
				syncScreenSize();
				clownPosY = screenTrueHeigh - clownMarginY;
				ballPosY = getBallPosY();
			}
		}
		if (letter > 0) {
			PhysBall* newBall;
			newBall = createPhysBall(letter, currentXPos, currentYPos);
			activeObjects.push_back(newBall);
		}

		// Step the Box2D world
		world->Step(worldTime, 6, 2);

		// Clear the renderer
		SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
		SDL_RenderClear(renderer);

		// Draw blower
		if (gameMode == 1) {
			renderTexture(clownTexture, renderer,
				currentXPos - mouthMarginX, clownPosY,
				240, 240);
		} else {
			renderTexture(lumiTexture, renderer,
				mouseX - mouthMarginX, mouseY,
				240, 240);
		}

		if (textAreaTexture != NULL) {
			renderTexture(textAreaTexture, renderer,
				(screenTrueWidth - textAreaTextureWidth) / 2.0f, 20,
				textAreaTextureWidth, textAreaTextureHeight);
		}

		// Draw non-fallen objects, mark lost ones
		for (PhysBall* physBall : activeObjects) {
			if ((int)(physBall->body->GetPosition().y) * METERS_TO_PIXELS > screenTrueHeigh) {
				bodiesToDestroy.push_back(physBall);
			} else {
				// Render the falling circle
				renderFilledCircle(renderer, physBall->texture, physBall->body);
				int posX = (int)(physBall->body->GetPosition().x) * METERS_TO_PIXELS;
				if (posX < 0) {
					b2Vec2 velocity(6.0f, physBall->body->GetLinearVelocity().y);
					physBall->body->SetLinearVelocity(velocity);
				} else if (posX > screenTrueWidth) {
					b2Vec2 velocity(-6.0f, physBall->body->GetLinearVelocity().y);
					physBall->body->SetLinearVelocity(velocity);
				}
			}
		}

		// Update the screen
		SDL_RenderPresent(renderer);

		// Remove lost objects
		for (PhysBall* physBall : bodiesToDestroy) {
			activeObjects.remove(physBall);
			world->DestroyBody(physBall->body);
			SDL_DestroyTexture(physBall->texture);
			physBall = NULL;
		}
		bodiesToDestroy.clear();

		// Calculate the time taken for this frame
		frameTime = SDL_GetTicks() - frameStart;

		Uint32 quitCode = 0;
		bool lastCmdMatch = true;
		for (int i = 0; i< cmdBufferLen; i++) {
			quitCode = quitCode + (cmdBuffer[(cmdIdx + i) % cmdBufferLen] << (24 - (8*i)));
			if (lastCommand[i] != cmdBuffer[i]) lastCmdMatch = false;
			lastCommand[i] = cmdBuffer[i];
		}
		if (!lastCmdMatch) {
			if (quitCode == 0x71756974) quit = true;
			if (quitCode == 0x73616d6d) system("telinit 0");
			if (quitCode == 0x6475636b) {
				// Play audio sample
				SDL_QueueAudio(device, wave_data, wave_length);
				SDL_PauseAudioDevice(device, 0); // Start playing
			}
		}

		lastTextTime += targetMs;
		if (currentTextCol > 0 && lastTextTime > textRowIdle) {
			finishCurrentTextRow();
		}

		// If the frame was too fast, delay to achieve the desired frame rate
		if (frameTime < targetMs) {
			SDL_Delay(targetMs - frameTime);
		}
	}

	cleanup();
	SDL_DestroyTexture(clownTexture);
	SDL_DestroyTexture(lumiTexture);
  SDL_CloseAudioDevice(device);
  SDL_RWclose(audio_file);
  free(wave_data);

	return 0;
}

