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

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
TTF_Font* textFont = NULL;
SDL_Color textColorBlack = {32, 32, 32};
SDL_Color textColorWhite = {224, 224, 224};
char cmdBuffer[] = {0,0,0,0,0};
int cmdBufferLen = 4;
int cmdIdx = 0;
int clownPosY = SCREEN_HEIGHT - 240;
int ballPosY = SCREEN_HEIGHT - 180;
b2World* world = NULL;
float ballSize = 30.0f;
float gMod = 6.0f;
const char* ballTxt[] = {
    "A","B","C","D","E","F",
    "G","H","I","J","K","L",
    "M","N","O","P","Q","R",
    "S","T","U","V","W","X",
    "Y","Z"," "
};
const char* ballNum[] {
    "0","1","2","3","4","5",
    "6","7","8","9",",","."
};
int colorPalettes[][3] =
  {{0,1,2},{0,2,1},
   {1,0,2},{1,2,0},
   {2,0,1},{2,1,0}};

typedef struct PhysBall {
  b2Body* body;
  SDL_Texture* texture;
} PhysBall;


std::list<PhysBall*> activeObjects;

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

SDL_Texture* createFilledRectTexture(SDL_Renderer* renderer, int radius) {
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, 2 * radius * METERS_TO_PIXELS, 2 * radius * METERS_TO_PIXELS, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_FillRect(surface, NULL, SDL_MapRGBA(surface->format, 255, 0, 0, 255));

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    return texture;
}

SDL_Texture* createFilledCircleTexture(SDL_Renderer* renderer, int radius, int letter) {
    SDL_Surface* surface = SDL_CreateRGBSurface(0, 2 * radius, 2 * radius, 32, 0, 0, 0, 0);

    SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);
    SDL_SetColorKey(surface, SDL_TRUE, SDL_MapRGBA(surface->format, 0, 0, 0, 0));

    SDL_Renderer* surfaceRenderer = SDL_CreateSoftwareRenderer(surface);
    
    int colorPlace = rand() % 7;
/*
color palettes
1 2 3
1 3 2
2 1 3
2 3 1
3 1 2
3 2 1
*/
    // Draw filled circle
    for (int x = -radius; x <= radius; ++x) {
        for (int y = -radius; y <= radius; ++y) {
            if (x*x + y*y <= radius * radius) {
                Uint32 distMod = (int)(sqrt(x*x + y*y) / radius * 96);
                int tmpColor[] = {255 - (int)distMod, 32, 32+(int)(0.5*distMod)};
                SDL_SetRenderDrawColor(surfaceRenderer,
                    tmpColor[colorPalettes[colorPlace][0]],
                    tmpColor[colorPalettes[colorPlace][1]],
                    tmpColor[colorPalettes[colorPlace][2]], 255);
                //SDL_SetRenderDrawColor(surfaceRenderer, 255 - distMod, 32, 32+(int)(0.5*distMod), 255);
                //SDL_SetRenderDrawColor(surfaceRenderer, randomColorC - distMod, randomColorA, randomColorA+(int)(0.5*distMod), 255);
                SDL_RenderDrawPoint(surfaceRenderer, radius + x, radius + y);
            }
        }
    }
    const char* targetTxt = ballTxt[26]; // Default value used will be a white space
    if (letter >= 'a' && letter <= 'z') {
        targetTxt = ballTxt[letter - 'a'];
    } else if (letter >= '0' && letter <= '9') {
        targetTxt = ballNum[letter - '0'];
    } else if (letter == ',') {
        targetTxt = ballNum[10];
    } else if (letter == '.') {
        targetTxt = ballNum[11];
    } else {
        letter = ' ';
    }
    cmdBuffer[cmdIdx++ % cmdBufferLen] = letter;
//    int ballChr = letter - 'a';
//    ballChr = ballChr > 26 || ballChr < 0 ? 26 : ballChr;
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

void renderFilledCircle(SDL_Renderer* renderer, SDL_Texture* texture, b2Body* body) {
    b2Vec2 position = body->GetPosition();
    float radius = ((b2CircleShape*)body->GetFixtureList()->GetShape())->m_radius * METERS_TO_PIXELS;

    //printf("x: %f, y: %f, size: %f\n", position.x, position.y, radius);

    SDL_Rect destinationRect = {
        static_cast<int>(position.x * METERS_TO_PIXELS - radius),
        static_cast<int>(position.y * METERS_TO_PIXELS - radius),
        static_cast<int>(2 * radius),
        static_cast<int>(2 * radius)
    };

    SDL_RenderCopy(renderer, texture, NULL, &destinationRect);
}

void initSDL() {
    SDL_Init(SDL_INIT_VIDEO);
    //window = SDL_CreateWindow("Kirjainbox", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    window = SDL_CreateWindow("Kirjainbox", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_FULLSCREEN | SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
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

    SDL_Quit();
}

SDL_Texture* loadTexture(const char* path, SDL_Renderer* renderer) {
    SDL_Surface* surface = IMG_Load(path);
    if (!surface) {
        // Handle error, e.g., print an error message
        return NULL;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    return texture;
}

void renderTexture(SDL_Texture* texture, SDL_Renderer* renderer, int x, int y, int w, int h) {
    SDL_Rect destinationRect = { x, y, w, h };
    SDL_RenderCopy(renderer, texture, NULL, &destinationRect);
}

PhysBall* createPhysBall(int letter, float curLocation) {
    PhysBall* newBall = (PhysBall*)malloc(sizeof(struct PhysBall));
//    b2Body* b = createCircle(world, SCREEN_WIDTH / 2, 0.0f, ballSize);
    int ballMod = rand() % 30;
    b2Body* b = createCircle(world, curLocation, ballPosY, ballSize + (float)ballMod);
    SDL_Texture* t = createFilledCircleTexture(renderer, (int)ballSize + ballMod, letter);

    // Set the linear velocity for the body
    float speedMod = 0.6 + (rand() % 5000) / 5000.0;
    float dirMod = ((rand() % 1000) / 1000.0) * 16.0f;
    //printf("%f\n", speedMod);
    b2Vec2 velocity(speedMod * (8.0f - dirMod), - speedMod * 16.0f * sqrt(gMod));
    b->SetLinearVelocity(velocity);

    newBall->body = b;
    newBall->texture = t;
    return newBall;
}


int main() {
    initSDL();

    // Initialize SDL_ttf
    if (TTF_Init() != 0) {
        // Handle TTF initialization failure
        SDL_Quit();
        return 1;
    }

    textFont = TTF_OpenFont("/usr/share/fonts/truetype/freefont/FreeSans.ttf", 24);
    if (!textFont) {
        // Handle font loading failure
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        printf("Fonts could not be loaded!");
        return 1;
    }

    SDL_Texture* clownTexture = loadTexture("clown.png", renderer);

    initBox2D();
    std::vector<PhysBall*> bodiesToDestroy;

    bool quit = false;
    float worldTime = 1.0f / 60.0f;
    Uint32 targetMs = 1000 / TARGET_FPS;
    SDL_Event e;
    Uint32 frameStart, frameTime;

    if (!clownTexture) {
        printf("Could not load images.\n");
        quit = true;
    }

    char lastCommand[] = {'a','a','a','a',0};

    while (!quit) {
        frameStart = SDL_GetTicks(); // Get the current time at the beginning of the frame
        float currentXPos = SCREEN_WIDTH * (0.5f + 0.5f * sin((float)SDL_GetTicks() / 1000.0f));
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
            } else if (e.type == SDL_KEYDOWN) {
                PhysBall* newBall = createPhysBall(e.key.keysym.sym, currentXPos);
                activeObjects.push_back(newBall);
            }
        }

        //    activeObjects.push_back(createPhysBall());

        // Step the Box2D world
        world->Step(worldTime, 6, 2);

        // Clear the renderer
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);

        // Draw clown
        renderTexture(clownTexture, renderer,
              currentXPos - 160, clownPosY,
              240, 240);

        // Draw non-fallen objects
        for (PhysBall* physBall : activeObjects) {
          if (static_cast<int>(physBall->body->GetPosition().y) * METERS_TO_PIXELS > SCREEN_HEIGHT) {
            bodiesToDestroy.push_back(physBall);
          } else {
            // Render the falling circle
            renderFilledCircle(renderer, physBall->texture, physBall->body);
            int posX = static_cast<int>(physBall->body->GetPosition().x) * METERS_TO_PIXELS;
            if (posX < 0) {
                b2Vec2 velocity(6.0f, physBall->body->GetLinearVelocity().y);
                physBall->body->SetLinearVelocity(velocity);
            } else if (posX > SCREEN_WIDTH) {
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
        //printf("%i < %i\n", frameTime, targetMs);
        Uint32 quitCode = 0;
        bool lastCmdMatch = true;
        for (int i = 0; i< cmdBufferLen; i++) {
            quitCode = quitCode + (cmdBuffer[(cmdIdx + i) % cmdBufferLen] << (24 - (8*i)));
            if (lastCommand[i] != cmdBuffer[i]) lastCmdMatch = false;
            lastCommand[i] = cmdBuffer[i];
        }
        if (!lastCmdMatch) {
            //printf("last command: %s\n", lastCommand);
            //printf("%x\n", quitCode);
            if (quitCode == 0x71756974) quit = true;
        }

        // If the frame was too fast, delay to achieve the desired frame rate
        if (frameTime < targetMs) {
            SDL_Delay(targetMs - frameTime);
        }
    }

    cleanup();
    SDL_DestroyTexture(clownTexture);

    return 0;
}

