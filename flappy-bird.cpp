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

  HANDLE h  = GetStdHandle(STD_INPUT_HANDLE);  // input
  HANDLE h2 = GetStdHandle(STD_OUTPUT_HANDLE); // output
  if (h == INVALID_HANDLE_VALUE)  { std::cerr << "error"  << std::endl; return 1; }
  if (h2 == INVALID_HANDLE_VALUE) { std::cerr << "error" << std::endl; return 1; }

  // use words for console io
  DWORD m  = 0;
  DWORD m2 = 0;
  DWORD m3 = 0;
  if (!GetConsoleMode(h,  &m))  { std::cerr << "error" << std::endl; return 1; }
  m2 = m;
  m2 &= ~ENABLE_LINE_INPUT;
  m2 &= ~ENABLE_ECHO_INPUT;
  if (!SetConsoleMode(h, m2))   { std::cerr << "error" << std::endl; return 1; }
  if (GetConsoleMode(h2, &m3))  SetConsoleMode(h2, m3 | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

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
  std::vector<float> px; // x positions
  std::vector<int>   pg; // gap tops
  std::vector<int>   ps; // scored flag (0 or 1)

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
  std::ifstream fin("best-score.txt");
  if (fin)
  {
    fin >> bsc;
    if (!fin) bsc = 0; // reset if read failed
  }
  fin.close(); // close the file

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
    if (!GetNumberOfConsoleInputEvents(h, &nEvents)) {
      SetConsoleMode(h, m);
      return 1;
    }

    for (DWORD i = 0; i < nEvents; ++i) // loop through events
    {
      if (!ReadConsoleInput(h, &rec, 1, &ne))
      {
        std::cerr << "Failed to read console input." << std::endl;
        SetConsoleMode(h, m);
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
      px.push_back(PipeSpawnX); 
      pg.push_back(d(rng)); 
      ps.push_back(0); 
    } 

    // ----------------------------
    // Déplacement des tuyaux + score
    // ----------------------------
    for (int i = 0; i < (int)px.size(); i++) // loop over all pipes
    {
      px[i] = px[i] - PipeSpeed * dt; // move pipe left

      int pipeRight = (int)std::floor(px[i]) + PipeWidth - 1;
      if (ps[i] == 0 && pipeRight < BirdLeft) {
        ps[i] = 1;         
        sc = sc + 1;        
        if (sc > bsc) bsc = sc;
      }
    }

    // ----------------------------
    // Suppression des tuyaux hors écran
    // ----------------------------
    for (int i = (int)px.size() - 1; i >= 0; i--) {
      if (px[i] + PipeWidth < OffscreenPipeLimit) 
      {
        px.erase(px.begin() + i); 
        pg.erase(pg.begin() + i); 
        ps.erase(ps.begin() + i); 
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
    if (bt < 0 || bb >= ScreenHeight)  { dead = 1;  }

    // ----------------------------
    // Collision : joueur / tuyaux
    // ----------------------------
    if (!dead)
    {
      for (int i = 0; i < (int)px.size(); i++)
      {
        int pl = (int)std::floor(px[i]);
        int pr = pl + PipeWidth - 1;

        if (br >= pl && bl <= pr)
        {
          for (int y = bt; y <= bb; y++)
          {
            if (y < pg[i] || y >= pg[i] + PipeGapHeight)
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
    for (int i = 0; i < (int)px.size(); i++)
    {
      int pl = (int)std::floor(px[i]);
      for (int dx = 0; dx < PipeWidth; dx++)
      {
        int x = pl + dx;
        if (x < 0 || x >= ScreenWidth) continue;
        for (int y = 0; y < ScreenHeight; y++)
        {
          if (!(y >= pg[i] && y < pg[i] + PipeGapHeight))
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
  std::ofstream fout("best-score.txt", std::ios::trunc);
  if (fout) fout << bsc; // write best score
  fout.close(); // close the file

  // restore original console mode
  SetConsoleMode(h, m);
  return 0;
}