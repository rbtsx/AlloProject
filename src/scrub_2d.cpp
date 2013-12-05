#include "allocore/io/al_App.hpp"
using namespace al;
#include "Gamma/SamplePlayer.h"
#include <vector>
#include <glob.h>

struct System {
  gam::SamplePlayer<float, gam::ipl::Cubic, gam::tap::Wrap>* player;
  float operator()() { return (*player)(); }
  Vec3f position;
  System(const char* fileName)
      : player(new gam::SamplePlayer<float, gam::ipl::Cubic, gam::tap::Wrap>(
            fileName)) {
    position = Vec3f(rnd::uniformS(), rnd::uniformS(), 0);
  }
  System(const System& other) {
    player = other.player;
    position = other.position;
  }
};

struct MyApp : App {
  std::vector<System> system;
  std::vector<std::vector<int> > nearby;
  Mesh ball;

  Vec3f mouse;
  int nearest;

  MyApp() {
    glob_t result;
    glob("wav/*_fixed.wav", 0, 0, &result);
    for (int i = 0; i < result.gl_pathc; ++i) {
      printf("%s\n", result.gl_pathv[i]);
      system.push_back(System(result.gl_pathv[i]));
    }
    std::cout << result.gl_pathc << std::endl;

    nearby.resize(system.size());
    for (int i = 0; i < system.size(); ++i)
      for (int k = 0; k < system.size(); ++k)
        if (fabs((system[i].position - system[k].position).mag()) < 0.1)
          nearby[i].push_back(k);

    addSphere(ball);
    ball.primitive(Graphics::TRIANGLES);
    ball.scale(0.008);
    ball.generateNormals();

    initAudio(44100, 1024);
    initWindow(Window::Dim(500,500));
  }

  virtual void onSound(AudioIOData& io) {

    while (io()) {
      float s = 0;
      for (int i = 0; i < nearby[nearest].size(); ++i) {
        System& sys(system[nearby[nearest][i]]);
        s += sys() * (1 - (abs((sys.position - mouse).mag()) / 0.1));
      }
      s /= nearby[nearest].size();
      io.out(0) = s;
      io.out(1) = s;
    }
  }

  virtual void onAnimate(double dt) {
    int localNearest = -1;
    float bestDistance = 10000.0f;
    for (int i = 0; i < system.size(); ++i) {
      float f = abs((system[i].position - mouse).mag());
      if (f < bestDistance) {
        bestDistance = f;
        localNearest = i;
      }
    }
    nearest = localNearest;

    nav().pos(0, 0, 3.75);
    nav().quat(Quatf(1, 0, 0, 0));
  }

  virtual void onDraw(Graphics& g, const Viewpoint& v) {
    for (int i = 0; i < system.size(); ++i) {
      g.pushMatrix();
      g.translate(system[i].position);
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
