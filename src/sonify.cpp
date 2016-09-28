#include "Gamma/Filter.h"
#include "Gamma/SamplePlayer.h"
#include "allocore/graphics/al_Image.hpp"
#include "allocore/graphics/al_Texture.hpp"
#include "allocore/io/al_App.hpp"
#include "allocore/io/al_File.hpp"
#include "allocore/sound/al_Ambisonics.hpp"
#include "allocore/spatial/al_HashSpace.hpp"
#include <fstream> // ifstream
#include <algorithm> // sort
using namespace al;
using namespace std;

#define MAXIMUM_NUMBER_OF_SOUND_SOURCES (10)
#define BLOCK_SIZE (1024)
#define CACHE_SIZE (65) // in pixels

SearchPaths searchPaths;

SpeakerLayout speakerLayout = HeadsetSpeakerLayout();
// layout, dimension and order
AmbisonicsSpatializer* panner = new AmbisonicsSpatializer(speakerLayout, 2, 1);
AudioScene scene(BLOCK_SIZE);
Listener* listener;

HashSpace space(5, 220100);

// SampleLooper
//
typedef gam::SamplePlayer<float, gam::ipl::Cubic, gam::tap::Wrap>/*<<<*/
    GammaSamplePlayerFloatCubicWrap;

struct DynamicSamplePlayer : GammaSamplePlayerFloatCubicWrap {
  DynamicSamplePlayer() : GammaSamplePlayerFloatCubicWrap() { zero(); }
  DynamicSamplePlayer(const DynamicSamplePlayer& other) {}

  // need this for some old version of gcc
  DynamicSamplePlayer& operator=(const DynamicSamplePlayer& other) {
    return *this;
  }
};

struct StarSystem {
  DynamicSamplePlayer player;
  unsigned kic;
  float x, y, amplitude;
  //float ascension, delcination;
  void print() {
    printf("%09d (%f,%f) %f\n", kic, x, y, amplitude);
  }
};

bool load(StarSystem& system) {
  char fileName[100];
  sprintf(fileName, "%09d_fixed.wav", system.kic);
  string filePath = searchPaths.find(fileName).filepath();
  if (filePath.empty()) {
    cout << fileName << " was not found in the path"<< endl;
    return false;
  }

  if (system.player.load(filePath.c_str())) {
    cout << "Loaded " << fileName << " into memory!"<< endl;
    return true;
  }

  assert(false);
  return false;
}

void unload(StarSystem& system) {
  char fileName[100];
  sprintf(fileName, "%09d_fixed.wav", system.kic);
  cout << "Unloaded " << fileName << " from memory!"<< endl;
  system.player.clear();
}

void load(vector<StarSystem>& system, string filePath);

struct MyApp : App, al::osc::PacketHandler {

  Texture fffi;
  Mesh ring;
  Mesh field;
  vector<StarSystem> system;
  SoundSource source[MAXIMUM_NUMBER_OF_SOUND_SOURCES];
  float sourceGain[MAXIMUM_NUMBER_OF_SOUND_SOURCES];
  vector<unsigned> cache;

  float x = 0, y = 0, z = 2000, r = 20;

  void findNeighbors(vector<unsigned>& n, float r) {
    HashSpace::Query qmany(100);
    qmany.clear(); // XXX why?
    float unit_x = (x + 6025) / 12050;
    float unit_y = (y + 6025) / 12050;
    float unit_r = r / 12050;
    int results = qmany(space,
      Vec3d(unit_x, unit_y, 0) * space.dim(),
      unit_r * space.dim());
    for (int i = 0; i < results; i++)
      n.push_back(qmany[i]->id);
  }

  MyApp() {
    listener = scene.createListener(panner);
    for (int i = 0; i < MAXIMUM_NUMBER_OF_SOUND_SOURCES; i++) {
      source[i].nearClip(1.0);
      source[i].farClip(50);
      scene.addSource(source[i]);
    }
    scene.usePerSampleProcessing(false);

    // OSC Configuration
    //
    App::oscSend().open(13000, "localhost", 0.1, Socket::UDP | Socket::DGRAM);
    App::oscRecv().open(13004, "", 0.1, Socket::UDP | Socket::DGRAM);
    App::oscRecv().handler(*this);
    App::oscRecv().start();

    // Make a circle
    //
    int N = 20;
    ring.primitive(Graphics::LINE_STRIP);
    for (int i = 0; i < N + 1; ++i) {
      float theta = M_2PI / N * i;
      ring.vertex(cos(theta), sin(theta), 0);
      ring.color(1, 1, 1);
      //ring.color(0, 0, 0);
    }

    FilePath filePath = searchPaths.find("testFFFI.png");
    //FilePath filePath = searchPaths.find("FFFI.tif");
    //FilePath filePath = searchPaths.find("printedFFFI.png");
    Image i;
    i.load(filePath.filepath());
    fffi.allocate(i.array());

    FileList fileList = searchPaths.glob(".*?map.txt");
    assert(fileList.count() == 1);
    load(system, fileList[0].filepath());
    cout << system.size() << " systems loaded from map file" << endl;

    field.primitive(Graphics::POINTS);
    for (unsigned i = 0; i < system.size(); i++) {
      field.vertex(system[i].x, system[i].y, 1);
      field.color(HSV(0.5, 1, 1));
    }

    for (unsigned i = 0; i < system.size(); i++) {
      float x = (system[i].x + 6025) / 12050;
      float y = (system[i].y + 6025) / 12050;
      //cout << x << "," << y << endl;
      space.move(i, x * space.dim(), y * space.dim(), 0);
    }

    nav().pos(x, y, z);
    lens().far(25010);
    lens().near(1);

    findNeighbors(cache, CACHE_SIZE);
    for (int i : cache)
      load(system[i]);

    initWindow(Window::Dim(800, 800));
    initAudio(44100, BLOCK_SIZE);
  }

  virtual void onSound(AudioIOData& io) {
    // for (int i = 0; i < system.size(); ++i)
    //   system[i].player.rate(rate);

    // make a copy of where we are..
    //
    Vec3d position(x, y, 0);

    // put the listener there..
    //
    listener->pose(Pose(position, Quatd()));

    // get a list of neighbors
    //
    HashSpace::Query qmany(MAXIMUM_NUMBER_OF_SOUND_SOURCES);
    qmany.clear(); // XXX why?
    float unit_x = (x + 6025) / 12050;
    float unit_y = (y + 6025) / 12050;
    float unit_r = r / 12050;
    int results = qmany(space,
      Vec3d(unit_x, unit_y, 0) * space.dim(),
      unit_r * space.dim());

    if (results) {
      // set sound source positions
      // calculate distances for attenuation
      //
      for (int i = 0; i < MAXIMUM_NUMBER_OF_SOUND_SOURCES; i++) {
        if (i < results) {
          Vec3f p(system[qmany[i]->id].x, system[qmany[i]->id].y, 0);
          source[i].pose(Pose(p, Quatd()));
          sourceGain[i] = 1.0f / (p - position).mag();
        }
        else {
          sourceGain[i] = 0;
        }
      }

      /*
      cout << ">--(" << results << ")--------------<" << endl;
      for (int i = 0; i < results; i++) {
        cout << sourceGain[i] << " ";
        system[qmany[i]->id].print();
      }
      */

    }

    while (io()) {
      for (int i = 0; i < MAXIMUM_NUMBER_OF_SOUND_SOURCES; i++) {
        if (i < results) {
          float f = 0 ; //system[qmany[i]->id].player();
          double d = isnan(f) ? 0.0 : (double)f;
          source[i].writeSample(d * sourceGain[i]);
        }
        else {
          source[i].writeSample(0.0);
        }
      }
    }

    scene.render(io);
  }

  virtual void onAnimate(double dt) {
    nav().quat(Quatf(1, 0, 0, 0));
    if ((Vec3d(x, y, z) - nav().pos()).mag() > 0.1)
      nav().pos(nav().pos() + (Vec3d(x, y, z) - nav().pos()) * 0.07);
    else
      nav().pos(Vec3d(x, y, z));

    vector<unsigned> latest;
    findNeighbors(latest, CACHE_SIZE);

    sort(cache.begin(), cache.end());
    sort(latest.begin(), latest.end());

    vector<unsigned> shouldLoad;
    set_difference(
      latest.begin(), latest.end(),
      cache.begin(), cache.end(),
      inserter(shouldLoad, shouldLoad.begin())
    );
    for (int i : shouldLoad)
      load(system[i]);

    vector<unsigned> shouldClear;
    set_difference(
      cache.begin(), cache.end(),
      latest.begin(), latest.end(),
      inserter(shouldClear, shouldClear.begin())
    );
    for (int i : shouldClear)
      unload(system[i]);

    cache.clear();
    for (int i : latest)
      cache.push_back(i);
  }

  virtual void onDraw(Graphics& g, const Viewpoint& v) {
    //fffi.quad(g, 12050, 12050, -6025, -6025, -1);
    g.draw(field);
    Mesh c;
    for (int i : cache) {
      c.vertex(system[i].x, system[i].y, 3);
      c.color(HSV(0.0, 1, 1));
    }
    g.draw(c);
    g.translate(x, y, 1);
    g.scale(r);
    g.draw(ring);
    g.scale(CACHE_SIZE/r);
    g.draw(ring);
  }

  virtual void onKeyDown(const ViewpointWindow& w, const Keyboard& k) { }

  virtual void onMessage(osc::Message& m) {
    if (m.addressPattern() == "/xy") {
      m >> x >> y;
      cout << "Look at (" << x << "," << y << ")" << endl;
    } else if (m.addressPattern() == "/z") {
      m >> z;
      cout << "Zoom 'closeness': " << z << endl;
    } else if (m.addressPattern() == "/r") {
      m >> r;
      cout << "Search Radius: " << r << endl;
    } else cout << "Unknown OSC message" << endl;
  }

};

int main() {
  searchPaths.addSearchPath("../");
  MyApp().start();
}

void load(vector<StarSystem>& system, string filePath) {
  ifstream mapFile(filePath);
  string line;
  while (getline(mapFile, line)) {
//    cout << line.length() << endl;
    char *p = new char[line.length() + 1];
    strcpy(p, line.c_str());
    system.push_back(StarSystem());

    //
    // cell 0: kic
    system.back().kic = atoi(p);

    while (*p != '|') p++; p++; // cell 1: ch // IGNORE
    while (*p != '|') p++; p++; // cell 2: x-coord in channel // IGNORE
    while (*p != '|') p++; p++; // cell 3: y-coord in channel // IGNORE

    //
    // cell 4: x-coord in FFFI
    while (*p != '|') p++; p++;
    system.back().x = atof(p);
    //
    // cell 5: y-coord in FFFI
    while (*p != '|') p++; p++;
    system.back().y = atof(p);

    while (*p != '|') p++; p++; // cell 6: ra // IGNORE
    while (*p != '|') p++; p++; // cell 7: dec // IGNORE

    //
    // cell 8: mean amplitude
    while (*p != '|') p++; p++;
    system.back().amplitude = atof(p);

//    system.back().print();
  }
}
