// TODO: clean this up later

#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <thread>
#include <algorithm>
#include <cmath>
#include <fstream>

struct Pipe
{
  float x;        // position horizontale
  int gapTop;     // début vertical du trou
  int scored;     // 0 = pas encore compté, 1 = déjà compté
};

unsigned long long LoadBestScore(const std::string& filename)
{
  unsigned long long bestScore = 0;
  std::ifstream fin(filename);

  if (fin)
  {
    fin >> bestScore;
    if (!fin)
    {
      bestScore = 0;
    }
  }

  fin.close();
  return bestScore;
}

void SaveBestScore(const std::string& filename, unsigned long long bestScore)
{
  std::ofstream fout(filename, std::ios::trunc);
  if (fout)
  {
    fout << bestScore;
  }
  fout.close();
}

bool SetupConsole(HANDLE& inputHandle, HANDLE& outputHandle, DWORD& originalInputMode)
{
  inputHandle = GetStdHandle(STD_INPUT_HANDLE);
  outputHandle = GetStdHandle(STD_OUTPUT_HANDLE);

  if (inputHandle == INVALID_HANDLE_VALUE)
  {
    std::cerr << "error" << std::endl;
    return false;
  }

  if (outputHandle == INVALID_HANDLE_VALUE)
  {
    std::cerr << "error" << std::endl;
    return false;
  }

  DWORD modifiedInputMode = 0;
  DWORD outputMode = 0;

  if (!GetConsoleMode(inputHandle, &originalInputMode))
  {
    std::cerr << "error" << std::endl;
    return false;
  }

  modifiedInputMode = originalInputMode;
  modifiedInputMode &= ~ENABLE_LINE_INPUT;
  modifiedInputMode &= ~ENABLE_ECHO_INPUT;

  if (!SetConsoleMode(inputHandle, modifiedInputMode))
  {
    std::cerr << "error" << std::endl;
    return false;
  }

  if (GetConsoleMode(outputHandle, &outputMode))
  {
    SetConsoleMode(outputHandle, outputMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
  }

  return true;
}

void RestoreConsole(HANDLE inputHandle, DWORD originalInputMode)
{
  SetConsoleMode(inputHandle, originalInputMode);
}

bool HandleInput(HANDLE inputHandle, float& birdVelocity, float jumpVelocity)
{
  INPUT_RECORD rec;
  DWORD ne = 0;
  DWORD nEvents = 0;

  if (!GetNumberOfConsoleInputEvents(inputHandle, &nEvents))
  {
    return false;
  }

  for (DWORD i = 0; i < nEvents; ++i)
  {
    if (!ReadConsoleInput(inputHandle, &rec, 1, &ne))
    {
      std::cerr << "Failed to read console input." << std::endl;
      return false;
    }

    if (rec.EventType == KEY_EVENT)
    {
      KEY_EVENT_RECORD k = rec.Event.KeyEvent;
      if (k.bKeyDown == TRUE)
      {
        if (k.wVirtualKeyCode == VK_RETURN)
        {
          birdVelocity = jumpVelocity;
        }
      }
    }
  }

  return true;
}

void UpdateBirdPhysics(float dt, float gravity, float& birdVelocity, float& birdY)
{
  birdVelocity = birdVelocity + gravity * dt;
  birdY = birdY + birdVelocity * dt;
}

void SpawnPipeIfNeeded(
  float dt,
  float pipeSpawnInterval,
  float pipeSpawnX,
  float& spawnTimer,
  std::vector<Pipe>& pipes,
  std::mt19937& rng,
  std::uniform_int_distribution<int>& gapDistribution)
{
  spawnTimer = spawnTimer + dt;
  if (spawnTimer >= pipeSpawnInterval)
  {
    spawnTimer = spawnTimer - pipeSpawnInterval;
    pipes.push_back({ pipeSpawnX, gapDistribution(rng), 0 });
  }
}

void UpdatePipesAndScore(
  float dt,
  float pipeSpeed,
  int pipeWidth,
  int birdLeft,
  std::vector<Pipe>& pipes,
  unsigned long long& score,
  unsigned long long& bestScore)
{
  for (int i = 0; i < (int)pipes.size(); i++)
  {
    pipes[i].x = pipes[i].x - pipeSpeed * dt;

    int pipeRight = (int)std::floor(pipes[i].x) + pipeWidth - 1;
    if (pipes[i].scored == 0 && pipeRight < birdLeft)
    {
      pipes[i].scored = 1;
      score = score + 1;
      if (score > bestScore)
      {
        bestScore = score;
      }
    }
  }
}

void RemoveOffscreenPipes(float offscreenPipeLimit, int pipeWidth, std::vector<Pipe>& pipes)
{
  for (int i = (int)pipes.size() - 1; i >= 0; i--)
  {
    if (pipes[i].x + pipeWidth < offscreenPipeLimit)
    {
      pipes.erase(pipes.begin() + i);
    }
  }
}

void GetBirdBounds(float birdY, int birdHeight, int birdLeft, int birdWidth, int& birdTop, int& birdBottom, int& birdLeftOut, int& birdRight)
{
  birdTop = (int)std::floor(birdY);
  birdBottom = birdTop + birdHeight - 1;
  birdLeftOut = birdLeft;
  birdRight = birdLeft + birdWidth - 1;
}

bool CheckWallCollision(int birdTop, int birdBottom, int screenHeight)
{
  return birdTop < 0 || birdBottom >= screenHeight;
}

bool CheckPipeCollision(
  int birdTop,
  int birdBottom,
  int birdLeft,
  int birdRight,
  int pipeWidth,
  int pipeGapHeight,
  const std::vector<Pipe>& pipes)
{
  for (int i = 0; i < (int)pipes.size(); i++)
  {
    int pipeLeft = (int)std::floor(pipes[i].x);
    int pipeRight = pipeLeft + pipeWidth - 1;

    if (birdRight >= pipeLeft && birdLeft <= pipeRight)
    {
      for (int y = birdTop; y <= birdBottom; y++)
      {
        if (y < pipes[i].gapTop || y >= pipes[i].gapTop + pipeGapHeight)
        {
          return true;
        }
      }
    }
  }

  return false;
}

std::vector<std::string> BuildEmptyFrame(int screenWidth, int screenHeight)
{
  return std::vector<std::string>(screenHeight, std::string(screenWidth, ' '));
}

void DrawPipes(std::vector<std::string>& frame, const std::vector<Pipe>& pipes, int pipeWidth, int pipeGapHeight, int screenWidth, int screenHeight)
{
  for (int i = 0; i < (int)pipes.size(); i++)
  {
    int pipeLeft = (int)std::floor(pipes[i].x);
    for (int dx = 0; dx < pipeWidth; dx++)
    {
      int x = pipeLeft + dx;
      if (x < 0 || x >= screenWidth) continue;

      for (int y = 0; y < screenHeight; y++)
      {
        if (!(y >= pipes[i].gapTop && y < pipes[i].gapTop + pipeGapHeight))
        {
          frame[y][x] = 'P';
        }
      }
    }
  }
}

void DrawBird(std::vector<std::string>& frame, int birdTop, int birdLeft, int birdWidth, int birdHeight, int screenWidth, int screenHeight)
{
  for (int dy = 0; dy < birdHeight; dy++)
  {
    int y = birdTop + dy;
    if (y < 0 || y >= screenHeight) continue;

    for (int dx = 0; dx < birdWidth; dx++)
    {
      int x = birdLeft + dx;
      if (x >= 0 && x < screenWidth)
      {
        frame[y][x] = 'B';
      }
    }
  }
}

std::string BuildScoreText(unsigned long long score, unsigned long long bestScore, int screenWidth)
{
  std::string scoreText = "Score: " + std::to_string(score) + "   Best: " + std::to_string(bestScore);
  if (scoreText.size() > (size_t)screenWidth)
  {
    scoreText = scoreText.substr(0, screenWidth);
  }
  return scoreText;
}

void ComputeHudPadding(const std::string& scoreText, int screenWidth, int& leftPadding, int& rightPadding)
{
  leftPadding = (int)((screenWidth - (int)scoreText.size()) / 2);
  rightPadding = screenWidth - leftPadding - (int)scoreText.size();
}

void RenderFrame(
  const std::vector<std::string>& frame,
  int screenWidth,
  int screenHeight,
  const std::string& scoreText,
  int leftPadding,
  int rightPadding)
{
  std::cout << "\x1b[2J\x1b[H";
  std::cout << "+" << std::string(screenWidth, '-') << "+" << "\n";

  for (int y = 0; y < screenHeight; y++)
  {
    std::cout << "|";
    for (int x = 0; x < screenWidth; x++)
    {
      char c = frame[y][x];
      if (c == 'P')
      {
        std::cout << "\x1b[32mP\x1b[0m";
      }
      else if (c == 'B')
      {
        std::cout << "\x1b[33mB\x1b[0m";
      }
      else
      {
        std::cout << ' ';
      }
    }
    std::cout << "|\n";
  }

  std::cout << "+" << std::string(screenWidth, '-') << "+" << "\n";
  std::cout << "+" << std::string(screenWidth, '-') << "+" << "\n";
  std::cout << "|" << std::string(leftPadding, ' ') << scoreText << std::string(rightPadding, ' ') << "|\n";
  std::cout << "+" << std::string(screenWidth, '-') << "+" << "\n";
  std::cout.flush();
}

int main()
{
  // ----------------------------
  // Constantes de jeu / rendu
  // ----------------------------
  const int ScreenWidth = 50;
  const int ScreenHeight = 20;

  const int BirdWidth = 2;
  const int BirdHeight = 2;
  const int BirdLeft = 10;

  const int PipeWidth = 6;
  const int PipeGapHeight = 6;
  const float PipeSpawnX = 50.0f;

  const float MaxDeltaTime = 0.1f;
  const float JumpVelocity = -14.0f;
  const float Gravity = 42.0f;
  const float PipeSpawnInterval = 1.4f;
  const float PipeSpeed = 18.0f;

  const float OffscreenPipeLimit = 0.0f;
  const float TargetFrameDuration = 1.0f / 30.0f;

  const int GapMinTop = 2;
  const int GapMaxTop = ScreenHeight - PipeGapHeight - 2;

  HANDLE h = INVALID_HANDLE_VALUE;
  HANDLE h2 = INVALID_HANDLE_VALUE;
  DWORD m = 0;

  if (!SetupConsole(h, h2, m))
  {
    return 1;
  }

  // ----------------------------
  // Données du joueur ("bird")
  // ----------------------------
  float by = 9.0f;                      // y position (float)
  float bv = 0.0f;                      // velocity
  int   bt = 0;                         // top (int)
  int   bb = 0;                         // bottom (int)
  int   bl = BirdLeft;                  // left
  int   br = BirdLeft + BirdWidth - 1;  // right
  int dead = 0;                         // 0 = alive, 1 = dead
  float t  = 0.0f;                      // spawn timer
  unsigned long long sc  = 0;           // current score
  unsigned long long bsc = 0;           // best score

  // hud padding
  int lp = 0; // left padding
  int rp = 0; // right padding
  
  // ----------------------------
  // Données des tuyaux
  // ----------------------------
  std::vector<Pipe> pipes;

  // ----------------------------
  // Générateur aléatoire
  // ----------------------------
  std::mt19937 rng(std::random_device{}());
  std::uniform_int_distribution<int> d(GapMinTop, GapMaxTop); // gap position

  // ----------------------------
  // Chargement du meilleur score
  // ----------------------------
  bsc = LoadBestScore("best-score.txt");

  auto prev = std::chrono::steady_clock::now();

  // ----------------------------
  // Boucle principale du jeu
  // ----------------------------
  while (dead == 0)
  {
    auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - prev).count();
    prev = now;
    if (dt > MaxDeltaTime) dt = MaxDeltaTime;

    if (!HandleInput(h, bv, JumpVelocity))
    {
      RestoreConsole(h, m);
      return 1;
    }

    UpdateBirdPhysics(dt, Gravity, bv, by);

    SpawnPipeIfNeeded(dt, PipeSpawnInterval, PipeSpawnX, t, pipes, rng, d);
    UpdatePipesAndScore(dt, PipeSpeed, PipeWidth, BirdLeft, pipes, sc, bsc);
    RemoveOffscreenPipes(OffscreenPipeLimit, PipeWidth, pipes);

    GetBirdBounds(by, BirdHeight, BirdLeft, BirdWidth, bt, bb, bl, br);

    if (CheckWallCollision(bt, bb, ScreenHeight))
    {
      dead = 1;
    }

    if (!dead && CheckPipeCollision(bt, bb, bl, br, PipeWidth, PipeGapHeight, pipes))
    {
      dead = 1;
    }

    if (dead != 0) break;

    std::vector<std::string> frame = BuildEmptyFrame(ScreenWidth, ScreenHeight);
    DrawPipes(frame, pipes, PipeWidth, PipeGapHeight, ScreenWidth, ScreenHeight);
    DrawBird(frame, bt, BirdLeft, BirdWidth, BirdHeight, ScreenWidth, ScreenHeight);

    std::string scoreText = BuildScoreText(sc, bsc, ScreenWidth);
    ComputeHudPadding(scoreText, ScreenWidth, lp, rp);

    RenderFrame(frame, ScreenWidth, ScreenHeight, scoreText, lp, rp);

    float ft = std::chrono::duration<float>(std::chrono::steady_clock::now() - now).count();
    if (ft < TargetFrameDuration)
    {
      std::this_thread::sleep_for(std::chrono::duration<float>(TargetFrameDuration - ft));
    }
  }

  SaveBestScore("best-score.txt", bsc);
  RestoreConsole(h, m);
  return 0;
}