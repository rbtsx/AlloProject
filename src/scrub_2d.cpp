#include "allocore/io/al_App.hpp"
#include "allocore/spatial/al_HashSpace.hpp"
using namespace al;
#include "Gamma/SamplePlayer.h"
#include "Gamma/Filter.h"
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

struct Box {
  float top, bottom, left, right;
  Box() : top(-999), bottom(999), left(999), right(-999) {}
  void verticle(float f) {
    if (f < bottom) bottom = f;
    if (f > top) top = f;
  }
  void horizontal(float f) {
    if (f < left) left = f;
    if (f > right) right = f;
  }
};

ostream& operator<<(ostream& out, const Box& b) {
  cout << "verticle:" << b.bottom << "," << b.top << " horizontal:" << b.left
       << "," << b.right;
  return out;
}

struct MyApp : App {
  Reverb<float> reverb;
  gam::Biquad<> filter;
  vector<StarSystem> system;
  HashSpace space;
  Mesh point;
  Vec3f mouse;
  rnd::Random<> rng;
  double radius;
  float rate;

  Box box;

  MyApp() : space(8, 4000), filter(9000) {

    rate = 0.75;

    reverb.bandwidth(0.5);  // Low-pass amount on input, in [0,1]
    reverb.damping(0.7);    // High-frequency damping, in [0,1]
    reverb.decay(0.4);      // Tail decay factor, in [0,1]

    // Diffusion amounts
    // Values near 0.7 are recommended. Moving further away from 0.7 will lead
    // to more distinct echoes.
    reverb.diffusion(0.76, 0.666, 0.707, 0.271);

    point.vertex(0, 0, 0);

    map<string, Vec3f> where;
    char s[100];
    ifstream foo("txt/map.txt");
    while (!foo.eof()) {
      // 000757450|84|73.4470|46.2242|291.1376|36.5774
      foo.getline(s, sizeof(s));
      char* p = s;
      int koi = atoi(p);
      if (koi == 0) break;
      while (*p != '|') p++;
      p++;
      while (*p != '|') p++;
      p++;
      while (*p != '|') p++;
      p++;
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
        box.horizontal(ra);
        box.verticle(dec);
        // printf("where[%s] = Vec3f(%f, %f, 0);\n", buffer, ra, dec);
      } else {
        cout << "koi " << buffer << " already in map" << endl;
      }
    }

    cout << "bounding box is " << box << endl;

    // use a glob to find all the .wav files for loading
    //
    glob_t result;
    glob("wav/*_fixed.wav", 0, 0, &result);

    // allocate space for each lightcurve (sample player + name)
    //
    system.resize(result.gl_pathc);
    cout << "loading " << system.size() << " star systems\n";

    int bytes = 0;
    for (int i = 0; i < result.gl_pathc; ++i) {

      // load the lightcurve .wav into a sample player
      //
      if (!system[i].player.load(result.gl_pathv[i])) {
        cout << "FAIL!\n";
        exit(-1);
      }
      bytes += 4 * system[i].player.size();

      system[i].player.rate(rate);

      // take the koi name out of the file/glob string
      //
      char buffer[10];
      buffer[9] = '\0';
      strncpy(buffer, result.gl_pathv[i] + 4, 9);
      // cout << buffer << endl;
      system[i].name = buffer;

      map<string, Vec3f>::iterator it = where.find(buffer);
      if (it == where.end()) {
        cout << "lookup failed on " << buffer << ". assigning random position"
             << endl;
        // ra min/max: 280.258/301.721
        // dec min/max: 36.5774/52.1491

        system[i].position = Vec3f(rnd::uniform(280.258, 301.721),
                                   rnd::uniform(36.5774, 52.1491), 0);
      } else {
        // cout << "found " << buffer << " ... inserting " << it->second <<
        // endl;
        system[i].position = it->second;
      }
    }
    cout << "used " << bytes << " bytes\n";

    for (int i = 0; i < system.size(); ++i) {
      system[i].position.x -= box.left;
      system[i].position.x *= 1 / (box.right - box.left);
      system[i].position.y -= box.bottom;
      system[i].position.y *= 1 / (box.top - box.bottom);
    }

    for (unsigned i = 0; i < system.size(); i++)
      space.move(i, system[i].position.x * space.dim(),
                 system[i].position.y * space.dim(), 0);

    radius = space.maxRadius() * 0.02;
    initAudio();
    initWindow(Window::Dim(800, 800));
  }

  virtual void onSound(AudioIOData& io) {
    gam::Sync::master().spu(audioIO().fps());

    for (int i = 0; i < system.size(); ++i) system[i].player.rate(rate);

    Vec3f local = mouse;
    bool mouseOffScreen = false;
    if (local.x < 0) {
      local.x = 0;
      mouseOffScreen = true;
    }
    if (local.x > 1) {
      local.x = 1;
      mouseOffScreen = true;
    }
    if (local.y < 0) {
      local.y = 0;
      mouseOffScreen = true;
    }
    if (local.y > 1) {
      local.y = 1;
      mouseOffScreen = true;
    }

    local *= space.dim();

    HashSpace::Query qmany(50);
    qmany.clear();
    int results = qmany(space, local, radius);
    cout << results << endl;
    while (io()) {
      float f = 0;
      int n = 0;
      for (int i = 0; i < results; i++) {
        float d = abs((local - qmany[i]->pos).mag());
        if (d > radius) continue;
        n++;
        if (qmany[i]->id >= system.size()) {
          cout << "id " << qmany[i]->id << " is out of bounds\n";
          continue;
        }
        float x = system[qmany[i]->id].player() * (1 - d / radius);
        f += x;
      }
      f /= 2;
      // f /= n;
      // f *= mouseOffScreen ? 0.0 : 1.0;
      if (isnan(f)) f = 0;
      f = filter(f);

      float wet1, wet2;
      reverb(f, wet1, wet2);
      io.out(0) = wet1 * 0.7 + f * 0.3;
      io.out(1) = wet2 * 0.7 + f * 0.3;
      // io.out(0) = io.out(1) = f;
    }
  }

  virtual void onAnimate(double dt) {
    nav().pos(0, 0, 3.75);
    nav().quat(Quatf(1, 0, 0, 0));
  }

  virtual void onDraw(Graphics& g, const Viewpoint& v) {
    g.pushMatrix(Graphics::PROJECTION);
    g.loadMatrix(Matrix4f::ortho2D(0, 1, 0, 1));

    for (int i = 0; i < system.size(); ++i) {
      g.pushMatrix(Graphics::MODELVIEW);
      g.loadIdentity();
      g.translate(system[i].position);
      g.draw(point);
      g.popMatrix();
    }

    g.popMatrix(Graphics::PROJECTION);
  }

  virtual void onKeyDown(const ViewpointWindow& w, const Keyboard& k) {
    switch (k.key()) {
      case ',':
        radius -= 0.5;
        break;
      case '.':
        radius += 0.5;
        break;
      case ';':
        rate -= 0.1;
        break;
      case '\'':
        rate += 0.1;
        break;
    }
  }

    virtual void onMouseMove(const ViewpointWindow & w, const Mouse & m) {
      float x = (float)m.x() / w.dimensions().w;
      float y = (float)m.y() / w.dimensions().h;
      y = 1 - y;
      mouse = Vec3f(x, y, 0);
      // std::cout << "(" << x << ", " << y << ")\n";
    }
  };

  int main() { MyApp().start(); }
