#include "allocore/io/al_App.hpp"
#include "allocore/spatial/al_HashSpace.hpp"
using namespace al;
#include "Gamma/SamplePlayer.h"
#include <vector>
#include <glob.h>
#include <iostream>
#include <fstream>
#include <map>
using namespace std;

struct DynamicSamplePlayer
    : gam::SamplePlayer<float, gam::ipl::Cubic, gam::tap::Wrap> {
  DynamicSamplePlayer() : SamplePlayer("") {}
  DynamicSamplePlayer(const DynamicSamplePlayer& other) {}
};

struct StarSystem {
  DynamicSamplePlayer player;
  string name;
  Vec3f position;
};

struct MyApp : App {
  vector<StarSystem> starSystem;

  HashSpace space;
  Mesh ball;

  Vec3f mouse;

  rnd::Random<> rng;

  HashSpace::Query qmany;

  MyApp() : space(7, 10000), qmany(20) {

    map<string, Vec3f> where;
    char s[100];
    ifstream foo("../../koi.txt");
    foo.getline(s, sizeof(s));
    while (!foo.eof()) {
      foo.getline(s, sizeof(s));
      char* p = s;
      int koi = atoi(p);
      if (koi == 0) break;
      while (*p != '|') p++;
      p++;
      float ra = atof(p);
      while (*p != '|') p++;
      p++;
      float dec = atof(p);

      char buffer[20];
      sprintf(buffer, "%09d", koi);

      if (where.find(buffer) == where.end()) {
        where[buffer] = Vec3f(ra, dec, 0);
        // printf("where[%s] = Vec3f(%f, %f, 0);\n", buffer, ra, dec);
      }
      else {
        cout << "koi " << buffer << " already in map" << endl;
      }
    }

    cout << where.size() << endl;

    //for (std::map<string, Vec3f>::iterator it = where.begin();
    //     it != where.end(); ++it)
    //  std::cout << it->first << " => " << it->second << '\n';

    // use a glob to find all the .wav files for loading
    //
    glob_t result;
    glob("wav/*_fixed.wav", 0, 0, &result);

    // allocate space for each lightcurve (sample player + name)
    //
    starSystem.resize(result.gl_pathc);
    cout << "loading " << starSystem.size() << " star systems\n";

    for (int i = 0; i < result.gl_pathc; ++i) {

      // load the lightcurve .wav into a sample player
      //
      if (!starSystem[i].player.load(result.gl_pathv[i])) {
        cout << "FAIL!\n";
        exit(-1);
      }

      // take the koi name out of the file/glob string
      //
      char buffer[10];
      buffer[9] = '\0';
      strncpy(buffer, result.gl_pathv[i] + 4, 9);
      // cout << buffer << endl;
      starSystem[i].name = buffer;

      map<string, Vec3f>::iterator it = where.find(buffer);
      if (it == where.end()) {
        cout << "lookup failed on " << buffer << endl;
        starSystem[i].position = Vec3f(rnd::uniformS(288.0), rnd::uniformS(52.0), 0);
      }
      else {
        cout << "found " << buffer << " ... inserting " << it->second << endl;
        starSystem[i].position = it->second;
      }
    }

    for (unsigned i = 0; i < space.numObjects(); i++) {
      space.move(i, space.dim() * rng.uniform() * rng.uniform(),
                 space.dim() * rng.uniform() * rng.uniform(), 0);
    }

    addSphere(ball);
    ball.primitive(Graphics::TRIANGLES);
    ball.scale(0.008);
    ball.generateNormals();

    initAudio(44100, 1024);
    initWindow(Window::Dim(500, 500));
  }

  virtual void onSound(AudioIOData& io) {

    while (io()) {
      float f = 0;
      // for (int i = 0; i < nearby[nearest].size(); ++i) {
      //  System& s(player[nearby[nearest][i]]);
      //  f += s.player() * (1 - (abs((s.position - mouse).mag()) / 0.1));
      //}
      // f /= nearby[nearest].size();
      io.out(0) = f;
      io.out(1) = f;
    }
  }

  virtual void onAnimate(double dt) {
    nav().pos(0, 0, 3.75);
    nav().quat(Quatf(1, 0, 0, 0));
  }

  virtual void onDraw(Graphics& g, const Viewpoint& v) {
    for (int i = 0; i < starSystem.size(); ++i) {
      g.pushMatrix();
      g.translate(starSystem[i].position * 0.01);
      g.draw(ball);
      g.popMatrix();
    }
  }

  virtual void onMouseMove(const ViewpointWindow& w, const Mouse& m) {
    float x = 2.0f * m.x() / w.dimensions().w - 1.0f;
    float y = 2.0f * m.y() / w.dimensions().h - 1.0f;
    if (x < -1) x = -1;
    if (y < -1) y = -1;
    if (x > 1) x = 1;
    if (y > 1) y = 1;
    y *= -1;
    mouse = Vec3f(x, y, 0);
    std::cout << "(" << x << ", " << y << ")\n";
  }
};

int main() { MyApp().start(); }
