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

  // ----------------------------
  // Structure de données : tuyau
  // ----------------------------
  struct Pipe
  {
    float x;        // position horizontale
    int gapTop;     // début vertical du trou
    int scored;     // 0 = pas encore compté, 1 = déjà compté
  };

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

  INPUT_RECORD rec;
  DWORD ne = 0;

  // int debug = 1;
  // if (debug) std::cout << "debug: game starting" << std::endl;

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
    // delta time
    auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - prev).count();
    prev = now;
    if (dt > MaxDeltaTime) dt = MaxDeltaTime; // clamp delta time

    // ----------------------------
    // Lecture des événements clavier
    // ----------------------------
    DWORD nEvents = 0;
    if (!GetNumberOfConsoleInputEvents(h, &nEvents))
    {
      RestoreConsole(h, m);
      return 1;
    }

    for (DWORD i = 0; i < nEvents; ++i) // loop through events
    {
      if (!ReadConsoleInput(h, &rec, 1, &ne))
      {
        std::cerr << "Failed to read console input." << std::endl;
        RestoreConsole(h, m);
        return 1;
      } // end if ReadConsoleInput

      if (rec.EventType == KEY_EVENT)
      {
        KEY_EVENT_RECORD k = rec.Event.KeyEvent;
        if (k.bKeyDown == TRUE)
        {
          if (k.wVirtualKeyCode == VK_RETURN)
          {
            bv = JumpVelocity;
          } // end if enter
        } // end if key down
      } // end if key event
    } // end for each event

    // ----------------------------
    // Physique du joueur
    // ----------------------------
    bv = bv + Gravity * dt;
    by = by + bv * dt;

    // ----------------------------
    // Spawn des tuyaux
    // ----------------------------
    t = t + dt;
    if (t >= PipeSpawnInterval)
    {
      t = t - PipeSpawnInterval;
      pipes.push_back({ PipeSpawnX, d(rng), 0 });
    }

    // ----------------------------
    // Déplacement des tuyaux + score
    // ----------------------------
    for (int i = 0; i < (int)pipes.size(); i++) // loop over all pipes
    {
      pipes[i].x = pipes[i].x - PipeSpeed * dt; // move pipe left

      int pipeRight = (int)std::floor(pipes[i].x) + PipeWidth - 1;
      if (pipes[i].scored == 0 && pipeRight < BirdLeft)
      {
        pipes[i].scored = 1;
        sc = sc + 1;
        if (sc > bsc) bsc = sc;
      }
    }

    // ----------------------------
    // Suppression des tuyaux hors écran
    // ----------------------------
    for (int i = (int)pipes.size() - 1; i >= 0; i--)
    {
      if (pipes[i].x + PipeWidth < OffscreenPipeLimit)
      {
        pipes.erase(pipes.begin() + i);
      }
    }

    // ----------------------------
    // Collision : joueur / murs
    // ----------------------------
    bt = (int)std::floor(by);
    bb = bt + BirdHeight - 1;
    bl = BirdLeft;
    br = BirdLeft + BirdWidth - 1;

    // check wall
    if (bt < 0 || bb >= ScreenHeight)  { dead = 1; }

    // ----------------------------
    // Collision : joueur / tuyaux
    // ----------------------------
    if (!dead)
    {
      for (int i = 0; i < (int)pipes.size(); i++)
      {
        int pl = (int)std::floor(pipes[i].x);
        int pr = pl + PipeWidth - 1;

        if (br >= pl && bl <= pr)
        {
          for (int y = bt; y <= bb; y++)
          {
            if (y < pipes[i].gapTop || y >= pipes[i].gapTop + PipeGapHeight)
            {
              dead = 1;
              break;
            }
          }
        }

        if (dead != 0) break;
      }
    }

    if (dead != 0) break;

    std::vector<std::string> frame(ScreenHeight, std::string(ScreenWidth, ' '));

    // ----------------------------
    // Dessin : tuyaux ("P")
    // ----------------------------
    for (int i = 0; i < (int)pipes.size(); i++)
    {
      int pl = (int)std::floor(pipes[i].x);
      for (int dx = 0; dx < PipeWidth; dx++)
      {
        int x = pl + dx;
        if (x < 0 || x >= ScreenWidth) continue;
        for (int y = 0; y < ScreenHeight; y++)
        {
          if (!(y >= pipes[i].gapTop && y < pipes[i].gapTop + PipeGapHeight))
          {
            frame[y][x] = 'P';
          }
        }
      }
    }

    // ----------------------------
    // Dessin : joueur ("B")
    // ----------------------------
    for (int dy = 0; dy < BirdHeight; dy++)
    {
      int y = bt + dy;
      if (y < 0 || y >= ScreenHeight) continue;
      for (int dx = 0; dx < BirdWidth; dx++)
      {
        int x = BirdLeft + dx;
        if (x >= 0 && x < ScreenWidth) frame[y][x] = 'B';
      }
    }

    // ----------------------------
    // Prépa HUD
    // ----------------------------
    std::string scoreText = "Score: " + std::to_string(sc) + "   Best: " + std::to_string(bsc);
    if (scoreText.size() > ScreenWidth) scoreText = scoreText.substr(0, ScreenWidth);
    lp = (int)((ScreenWidth - (int)scoreText.size()) / 2);
    rp = ScreenWidth - lp - (int)scoreText.size();

    // ----------------------------
    // Affichage console ? Je crois
    // ----------------------------
    std::cout << "\x1b[2J\x1b[H";
    std::cout << "+" << std::string(ScreenWidth, '-') << "+" << "\n";
    for (int y = 0; y < ScreenHeight; y++)
    {
      std::cout << "|";
      for (int x = 0; x < ScreenWidth; x++)
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
    std::cout << "+" << std::string(ScreenWidth, '-') << "+" << "\n";
    std::cout << "+" << std::string(ScreenWidth, '-') << "+" << "\n";
    std::cout << "|" << std::string(lp, ' ') << scoreText << std::string(rp, ' ') << "|\n";
    std::cout << "+" << std::string(ScreenWidth, '-') << "+" << "\n";
    std::cout.flush();

    // ----------------------------
    // Limitation à 30 FPS
    // ----------------------------
    float ft = std::chrono::duration<float>(std::chrono::steady_clock::now() - now).count();
    if (ft < TargetFrameDuration)
    {
      std::this_thread::sleep_for(std::chrono::duration<float>(TargetFrameDuration - ft));
    }
  }

  // ----------------------------
  // Sauvegarde du meilleur score
  // ----------------------------
  SaveBestScore("best-score.txt", bsc);

  // restore original console mode
  RestoreConsole(h, m);
  return 0;
}