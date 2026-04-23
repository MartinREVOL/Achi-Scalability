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
  float x;
  int gapTop;
  bool hasScored;
};

struct GameState
{
  float birdY = 9.0f;
  float birdVelocity = 0.0f;

  int birdTop = 0;
  int birdBottom = 0;
  int birdLeft = 0;
  int birdRight = 0;

  bool isDead = false;
  float spawnTimer = 0.0f;

  unsigned long long score = 0;
  unsigned long long bestScore = 0;

  int hudLeftPadding = 0;
  int hudRightPadding = 0;

  std::vector<Pipe> pipes;
};

unsigned long long LoadBestScore(const std::string& filename)
{
  unsigned long long bestScore = 0;
  std::ifstream inputFile(filename);

  if (inputFile)
  {
    inputFile >> bestScore;
    if (!inputFile)
    {
      bestScore = 0;
    }
  }

  inputFile.close();
  return bestScore;
}

void SaveBestScore(const std::string& filename, unsigned long long bestScore)
{
  std::ofstream outputFile(filename, std::ios::trunc);
  if (outputFile)
  {
    outputFile << bestScore;
  }
  outputFile.close();
}

bool SetupConsole(HANDLE& inputHandle, HANDLE& outputHandle, DWORD& originalInputMode)
{
  inputHandle = GetStdHandle(STD_INPUT_HANDLE);
  outputHandle = GetStdHandle(STD_OUTPUT_HANDLE);

  if (inputHandle == INVALID_HANDLE_VALUE)
  {
    std::cerr << "Failed to get console input handle." << std::endl;
    return false;
  }

  if (outputHandle == INVALID_HANDLE_VALUE)
  {
    std::cerr << "Failed to get console output handle." << std::endl;
    return false;
  }

  DWORD modifiedInputMode = 0;
  DWORD outputMode = 0;

  if (!GetConsoleMode(inputHandle, &originalInputMode))
  {
    std::cerr << "Failed to read console input mode." << std::endl;
    return false;
  }

  modifiedInputMode = originalInputMode;
  modifiedInputMode &= ~ENABLE_LINE_INPUT;
  modifiedInputMode &= ~ENABLE_ECHO_INPUT;

  if (!SetConsoleMode(inputHandle, modifiedInputMode))
  {
    std::cerr << "Failed to set console input mode." << std::endl;
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

bool HandleInput(HANDLE inputHandle, GameState& game, float jumpVelocity)
{
  INPUT_RECORD inputRecord;
  DWORD eventsRead = 0;
  DWORD eventCount = 0;

  if (!GetNumberOfConsoleInputEvents(inputHandle, &eventCount))
  {
    return false;
  }

  for (DWORD eventIndex = 0; eventIndex < eventCount; ++eventIndex)
  {
    if (!ReadConsoleInput(inputHandle, &inputRecord, 1, &eventsRead))
    {
      std::cerr << "Failed to read console input." << std::endl;
      return false;
    }

    if (inputRecord.EventType == KEY_EVENT)
    {
      KEY_EVENT_RECORD keyEvent = inputRecord.Event.KeyEvent;
      if (keyEvent.bKeyDown == TRUE)
      {
        if (keyEvent.wVirtualKeyCode == VK_RETURN)
        {
          game.birdVelocity = jumpVelocity;
        }
      }
    }
  }

  return true;
}

void UpdateBirdPhysics(float deltaTime, float gravity, GameState& game)
{
  game.birdVelocity = game.birdVelocity + gravity * deltaTime;
  game.birdY = game.birdY + game.birdVelocity * deltaTime;
}

void SpawnPipeIfNeeded(
  float deltaTime,
  float pipeSpawnInterval,
  float pipeSpawnX,
  GameState& game,
  std::mt19937& randomGenerator,
  std::uniform_int_distribution<int>& gapDistribution)
{
  game.spawnTimer = game.spawnTimer + deltaTime;
  if (game.spawnTimer >= pipeSpawnInterval)
  {
    game.spawnTimer = game.spawnTimer - pipeSpawnInterval;
    game.pipes.push_back({ pipeSpawnX, gapDistribution(randomGenerator), false });
  }
}

void UpdatePipesAndScore(
  float deltaTime,
  float pipeSpeed,
  int pipeWidth,
  int birdLeft,
  GameState& game)
{
  for (int pipeIndex = 0; pipeIndex < static_cast<int>(game.pipes.size()); pipeIndex++)
  {
    game.pipes[pipeIndex].x = game.pipes[pipeIndex].x - pipeSpeed * deltaTime;

    int pipeRight = static_cast<int>(std::floor(game.pipes[pipeIndex].x)) + pipeWidth - 1;
    if (!game.pipes[pipeIndex].hasScored && pipeRight < birdLeft)
    {
      game.pipes[pipeIndex].hasScored = true;
      game.score = game.score + 1;
      if (game.score > game.bestScore)
      {
        game.bestScore = game.score;
      }
    }
  }
}

void RemoveOffscreenPipes(float offscreenPipeLimit, int pipeWidth, GameState& game)
{
  for (int pipeIndex = static_cast<int>(game.pipes.size()) - 1; pipeIndex >= 0; pipeIndex--)
  {
    if (game.pipes[pipeIndex].x + pipeWidth < offscreenPipeLimit)
    {
      game.pipes.erase(game.pipes.begin() + pipeIndex);
    }
  }
}

void UpdateBirdBounds(float birdY, int birdHeight, int birdLeft, int birdWidth, GameState& game)
{
  game.birdTop = static_cast<int>(std::floor(birdY));
  game.birdBottom = game.birdTop + birdHeight - 1;
  game.birdLeft = birdLeft;
  game.birdRight = birdLeft + birdWidth - 1;
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
  for (int pipeIndex = 0; pipeIndex < static_cast<int>(pipes.size()); pipeIndex++)
  {
    int pipeLeft = static_cast<int>(std::floor(pipes[pipeIndex].x));
    int pipeRight = pipeLeft + pipeWidth - 1;

    if (birdRight >= pipeLeft && birdLeft <= pipeRight)
    {
      for (int y = birdTop; y <= birdBottom; y++)
      {
        if (y < pipes[pipeIndex].gapTop || y >= pipes[pipeIndex].gapTop + pipeGapHeight)
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

void DrawPipes(
  std::vector<std::string>& frame,
  const std::vector<Pipe>& pipes,
  int pipeWidth,
  int pipeGapHeight,
  int screenWidth,
  int screenHeight)
{
  for (int pipeIndex = 0; pipeIndex < static_cast<int>(pipes.size()); pipeIndex++)
  {
    int pipeLeft = static_cast<int>(std::floor(pipes[pipeIndex].x));
    for (int dx = 0; dx < pipeWidth; dx++)
    {
      int x = pipeLeft + dx;
      if (x < 0 || x >= screenWidth)
      {
        continue;
      }

      for (int y = 0; y < screenHeight; y++)
      {
        if (!(y >= pipes[pipeIndex].gapTop && y < pipes[pipeIndex].gapTop + pipeGapHeight))
        {
          frame[y][x] = 'P';
        }
      }
    }
  }
}

void DrawBird(
  std::vector<std::string>& frame,
  int birdTop,
  int birdLeft,
  int birdWidth,
  int birdHeight,
  int screenWidth,
  int screenHeight)
{
  for (int dy = 0; dy < birdHeight; dy++)
  {
    int y = birdTop + dy;
    if (y < 0 || y >= screenHeight)
    {
      continue;
    }

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
  if (scoreText.size() > static_cast<size_t>(screenWidth))
  {
    scoreText = scoreText.substr(0, screenWidth);
  }
  return scoreText;
}

void ComputeHudPadding(const std::string& scoreText, int screenWidth, int& leftPadding, int& rightPadding)
{
  leftPadding = static_cast<int>((screenWidth - static_cast<int>(scoreText.size())) / 2);
  rightPadding = screenWidth - leftPadding - static_cast<int>(scoreText.size());
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
      char cell = frame[y][x];
      if (cell == 'P')
      {
        std::cout << "\x1b[32mP\x1b[0m";
      }
      else if (cell == 'B')
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

  HANDLE inputHandle = INVALID_HANDLE_VALUE;
  HANDLE outputHandle = INVALID_HANDLE_VALUE;
  DWORD originalInputMode = 0;

  if (!SetupConsole(inputHandle, outputHandle, originalInputMode))
  {
    return 1;
  }

  GameState game;

  std::mt19937 randomGenerator(std::random_device{}());
  std::uniform_int_distribution<int> gapDistribution(GapMinTop, GapMaxTop);

  game.bestScore = LoadBestScore("best-score.txt");

  auto previousFrameTime = std::chrono::steady_clock::now();

  while (!game.isDead)
  {
    auto frameStartTime = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(frameStartTime - previousFrameTime).count();
    previousFrameTime = frameStartTime;
    if (deltaTime > MaxDeltaTime)
    {
      deltaTime = MaxDeltaTime;
    }

    if (!HandleInput(inputHandle, game, JumpVelocity))
    {
      RestoreConsole(inputHandle, originalInputMode);
      return 1;
    }

    UpdateBirdPhysics(deltaTime, Gravity, game);

    SpawnPipeIfNeeded(deltaTime, PipeSpawnInterval, PipeSpawnX, game, randomGenerator, gapDistribution);
    UpdatePipesAndScore(deltaTime, PipeSpeed, PipeWidth, BirdLeft, game);
    RemoveOffscreenPipes(OffscreenPipeLimit, PipeWidth, game);

    UpdateBirdBounds(game.birdY, BirdHeight, BirdLeft, BirdWidth, game);

    if (CheckWallCollision(game.birdTop, game.birdBottom, ScreenHeight))
    {
      game.isDead = true;
    }

    if (!game.isDead &&
        CheckPipeCollision(
          game.birdTop,
          game.birdBottom,
          game.birdLeft,
          game.birdRight,
          PipeWidth,
          PipeGapHeight,
          game.pipes))
    {
      game.isDead = true;
    }

    if (game.isDead)
    {
      break;
    }

    std::vector<std::string> frame = BuildEmptyFrame(ScreenWidth, ScreenHeight);
    DrawPipes(frame, game.pipes, PipeWidth, PipeGapHeight, ScreenWidth, ScreenHeight);
    DrawBird(frame, game.birdTop, BirdLeft, BirdWidth, BirdHeight, ScreenWidth, ScreenHeight);

    std::string scoreText = BuildScoreText(game.score, game.bestScore, ScreenWidth);
    ComputeHudPadding(scoreText, ScreenWidth, game.hudLeftPadding, game.hudRightPadding);

    RenderFrame(frame, ScreenWidth, ScreenHeight, scoreText, game.hudLeftPadding, game.hudRightPadding);

    float frameTime = std::chrono::duration<float>(std::chrono::steady_clock::now() - frameStartTime).count();
    if (frameTime < TargetFrameDuration)
    {
      std::this_thread::sleep_for(std::chrono::duration<float>(TargetFrameDuration - frameTime));
    }
  }

  SaveBestScore("best-score.txt", game.bestScore);
  RestoreConsole(inputHandle, originalInputMode);
  return 0;
}