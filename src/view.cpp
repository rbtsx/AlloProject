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
  bool hasNan;
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
  Mesh mesh;

  MyApp() {

    mesh.primitive(Graphics::LINE_LOOP);
    for (int i = 0; i < 21; i++) {
      float f = i / 20.0f * M_PI * 2.0f;
      mesh.vertex(15 * sin(f), 15 * cos(f), 0);
    }

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
    initWindow(Window::Dim(800, 800));
  }

  virtual void onSound(AudioIOData& io) {
    gam::Sync::master().spu(audioIO().fps());

    int localChannel = channel;
    int localNearest = nearest;
    vector<KOI>& koi_list(data[localChannel].koi);
    KOI& koi(koi_list[nearest]);

    while (io()) {
      float f = 0;
      if (koi.player.size() > 1)
        f += koi.player();
      if (isnan(f)) f = 0;
      io.out(0) = io.out(1) = f;
    }
  }

  virtual void onAnimate(double dt) {
    int localChannel = channel;
    vector<KOI>& koi(data[localChannel].koi);

    float smallestDistance = 10000;
    int indexOfClosest = -1;
    for (int i = 0; i < koi.size(); i++) {
      float d = abs((koi[i].pos - mouse).mag());
      if (d < smallestDistance) {
        smallestDistance = d;
        indexOfClosest = i;
      }
    }

    if (nearest != indexOfClosest) {
      nearest = indexOfClosest;
      cout << koi[nearest].name << ": " << koi[nearest].player.size() << (koi[nearest].hasNan ? " hasNan" : "") << endl;
    }
  }

  virtual void onDraw(Graphics& g, const Viewpoint& v) {
    data[channel].texture.quadViewport(g);

    g.pushMatrix(Graphics::PROJECTION);
    g.loadMatrix(Matrix4f::ortho2D(0, 1132, 0, 1070));
    g.pushMatrix(Graphics::MODELVIEW);
    g.loadIdentity();

    int localChannel = channel;
    vector<KOI>& koi(data[localChannel].koi);
    for (int i = 0; i < koi.size(); i++) {
      g.pushMatrix();
      g.translate(koi[i].pos);
      g.draw(mesh);
      g.popMatrix();
    }

    g.popMatrix();
    g.popMatrix(Graphics::PROJECTION);
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
    float x = (float)m.x() / w.dimensions().w;
    float y = (float)m.y() / w.dimensions().h;

    y = 1 - y;

    x *= 1132;
    y *= 1070;

    // cout << x << ", " << y << endl;

    mouse.x = x;
    mouse.y = y;
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

    KOI blahblah;
    data[channel - 1].koi.push_back(blahblah);
    KOI& koi(data[channel - 1].koi[data[channel - 1].koi.size() - 1]);

    koi.pos.x = x;
    koi.pos.y = y;
    koi.pos.z = 0;
    koi.name = koi_id_string;

    if (!koi.player.load(fileName.c_str())) {
      cout << "failed to load " << fileName << endl;
    } else {

      while (koi.player.size() < 2)
        koi.player.load(fileName.c_str());

      bytes += 4 * koi.player.size();

      bool hasNan = false;
      for (int i = 0; i < koi.player.size(); i++)
        if (isnan(koi.player[i])) {
          hasNan = true;
          break;
        }
      koi.hasNan = hasNan;
      //cout << koi.name << ": " << channel << " " << koi.player.size() << (hasNan ? " hasNan" : "") << endl;
    }
  }

  cout << bytes << " bytes (total)\n";
}
