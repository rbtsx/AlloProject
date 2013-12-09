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

struct KOI {
  string name;
  Vec3f pos;
  DynamicSamplePlayer player;
};

struct Set {
  vector<KOI> koi;
  Texture texture;
};

typedef vector<Set> Data;

void load(Data& data);

struct MyApp : App {
  Data data;
  int channel;
  Vec3f mouse;
  int nearest;

  MyApp() {
    channel = 0;
    nearest = 0;
    mouse = Vec3f(0, 0, 0);

    load(data);

    // for (int i = 0; i < 84; i++) {
    //  cout << i << ":" << data[i].size() << endl;
    //  // for (int k = 0; k < data[i].size(); k++) {
    //  //  cout << i << " -> " << "(" << k << ", " << data[i][k].name << ")" <<
    //  // endl;
    //  //}
    //}

    initAudio();
    initWindow(Window::Dim(512, 512));
  }

  virtual void onSound(AudioIOData& io) {
    gam::Sync::master().spu(audioIO().fps());

    while (io()) {
      float f = 0;
      if (data[channel].koi[nearest].player.size() != 0)
        f += data[channel].koi[nearest].player();
      if (isnan(f))
        f = 0;
      io.out(0) = io.out(1) = f;
    }
  }

  virtual void onAnimate(double dt) {
    vector<KOI>& koi(data[channel].koi);

    float smallestDistance = 10000;
    int indexOfClosest = -1;
    for (int i = 0; i < koi.size(); i++) {
      float d = abs((koi[i].pos - mouse).mag());
      if (d < smallestDistance) {
        smallestDistance = d;
        indexOfClosest = i;
      }
    }

    nearest = indexOfClosest;
    cout << data[channel].koi[nearest].name << endl;
  }

  virtual void onDraw(Graphics& g, const Viewpoint& v) {
    data[channel].texture.quadViewport(g);
  }

  virtual void onKeyDown(const ViewpointWindow& w, const Keyboard& k) {
    // cout << k.key() << endl;
    switch (k.key()) {
      case ' ':
        if (k.shift())
          channel = (channel == 0) ? data.size() - 1 : channel - 1;
        else
          channel = (channel == (data.size() - 1)) ? 0 : channel + 1;
        cout << "channel: " << channel << endl;
        break;
    }
  }

  virtual void onMouseMove(const ViewpointWindow& w, const Mouse& m) {
    float x = (float)m.x() / w.dimensions().w * data[0].texture.array().width();
    float y = (float)m.y() / w.dimensions().h * data[0].texture.array().height();
    //y = 1 - y;
    mouse.x = x;
    mouse.y = y;

    //std::cout << "(" << x << ", " << y << ")\n";
  }
};

int main() { MyApp().start(); }

void load(Data& data) {

  data.resize(84);

  ifstream koi_map("txt/map.txt");

  int bytes = 0;

  for (int i = 0; i < data.size(); i++) {
    char buffer[32];
    sprintf(buffer, "bmp/ffi_%d.bmp", i + 1);
    Image image;
    if (!image.load(buffer)) {
      cout << "could not load " << buffer << endl;
      exit(-1);
    }
    data[i].texture.allocate(image.array());
    bytes += image.array().size();
  }

  cout << bytes << " bytes (texture)\n";

  while (!koi_map.eof()) {
    // 000757450|84|73.4470|46.2242|291.1376|36.5774
    char s[64];
    char* p = s;
    int koi_id, channel;
    float x, y, ra, dec;
    koi_map.getline(s, sizeof(s));

    koi_id = atoi(p);
    if (koi_id == 0) break;
    while (*p != '|') p++;
    p++;

    channel = atoi(p);
    while (*p != '|') p++;
    p++;

    x = atof(p);
    while (*p != '|') p++;
    p++;

    y = atof(p);
    while (*p != '|') p++;
    p++;

    ra = atof(p);
    while (*p != '|') p++;
    p++;
    dec = atof(p);

    char koi_id_string[20];
    sprintf(koi_id_string, "%09d", koi_id);

    string fileName = "wav/";
    fileName += koi_id_string;
    fileName += "_fixed.wav";

    data[channel - 1].koi.push_back(KOI());
    KOI& koi(data[channel - 1].koi[data[channel - 1].koi.size() - 1]);

    koi.pos.x = x;
    koi.pos.y = y;
    koi.pos.z = 0;
    koi.name = koi_id_string;

    if (!koi.player.load(fileName.c_str())) {
      cout << "failed to load " << fileName << endl;
    } else {
      bytes += 4 * koi.player.size();
    }
  }

  cout << bytes << " bytes (total)\n";
}
