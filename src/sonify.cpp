#include "Gamma/Filter.h"
#include "Gamma/SamplePlayer.h"
#include "allocore/graphics/al_Image.hpp"
#include "allocore/graphics/al_Texture.hpp"
#include "allocore/io/al_App.hpp"
#include "allocore/io/al_File.hpp"
#include "allocore/sound/al_Vbap.hpp"
#include "allocore/spatial/al_HashSpace.hpp"
#include <fstream> // ifstream
#include <algorithm> // sort
using namespace al;
using namespace std;

//#define DATASET "wav_sonify/"
#define DATASET "wav_182864/"
//#define FFFI_FILE "testFFFI.png"
//#define FFFI_FILE "FFFI.tif"
#define FFFI_FILE "printedFFFI.png"

#define MAXIMUM_NUMBER_OF_SOUND_SOURCES (10)
#define BLOCK_SIZE (1024)

// TODO:
// - do audio analysis:
//   + take the FFT of each system
//   + find onsets/transients of each system
//   + get the noise-floor
//   + characterize as choppy or smooth
// - do audio effects/synthesis
//   + add dynamic range compression
//   + add reverb
//   + resynthesize systems
// - try increasing the number of sound sources
// - try higher HashSpace dimension
// - add text HUD with KOI information
// - make doppler toggle
// - render star systems as perspective point cloud
// - implement "jumping" the cursor on click
// - use message-passing queues between threads
// - star system eraser tool
//

vector<string> path;
HashSpace space(5, 190000); // 5++?

// SampleLooper
//
typedef gam::SamplePlayer<float, gam::ipl::Cubic, gam::tap::Wrap>
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
  //int channel;
  char name[10];
};

string findPath(string fileName, bool critical = true) {
  for (string d : path) {
    d += "/";
    //cout << "Trying: " << d + fileName << endl;
    if (File::exists(d + fileName))
      return d + fileName;
  }
  if (critical) {
    cout << "Failed to find critical file: " << fileName << endl;
    exit(10);
  }
  return string(fileName + " does not exist!");
}

bool load(StarSystem& system) {
  char fileName[200];
  sprintf(fileName, DATASET "%09d.g.bin.wav", system.kic);
  string filePath = findPath(fileName, false);
  if (system.player.load(filePath.c_str())) {
    //cout << "Loaded " << fileName << " into memory!"<< endl;
    return true;
  }

  assert(false);
  return false;
}

void unload(StarSystem& system) {
  char fileName[200];
  sprintf(fileName, DATASET "%09d.g.bin.wav", system.kic);
  //cout << "Unloaded " << fileName << " from memory!"<< endl;
  system.player.clear();
}

void load(vector<StarSystem>& system, string filePath);

struct MyApp : App, al::osc::PacketHandler {

  bool macOS = false;
  bool autonomous = false;
  bool onLaptop = false;
  bool imageFound = false;
  bool shouldDrawImage = false;

  float zd = 0, rd = 0;
  float x = 0, y = 0, z = 2000, r = 20, cacheSize = 80, near = 1, far = 40;
  Vec3f go;

  vector<StarSystem> system;

  vector<unsigned> cache;

  // Audio Spatialization
  //
  AudioScene scene;
  SpeakerLayout* speakerLayout;
  Vbap* panner;
  Listener* listener;
  SoundSource source[MAXIMUM_NUMBER_OF_SOUND_SOURCES];

  // Graphics
  //
  Texture fffi;
  Mesh circle, field, square;

  void findNeighbors(vector<unsigned>& n, float x, float y, float r, bool shouldSort = false) {
    HashSpace::Query qmany(512);
    qmany.clear(); // XXX why?
    float unit_x = (x + 6025) / 12050;
    float unit_y = (y + 6025) / 12050;
    float unit_r = r / 12050;
    int results = qmany(space,
      Vec3d(unit_x, unit_y, 0) * space.dim(),
      unit_r * space.dim());
    for (int i = 0; i < results; i++)
      n.push_back(qmany[i]->id);
    if (shouldSort) {
      sort(n.begin(), n.end(), [&](unsigned a, unsigned b) {
        float dist_a = (Vec2f(x, y) - Vec2f(system[a].x, system[a].y)).mag();
        float dist_b = (Vec2f(x, y) - Vec2f(system[b].x, system[b].y)).mag();
        if (dist_a > dist_b) return -1;
        if (dist_b > dist_a) return 1;
        return 0;
      });
    }
  }

  MyApp() : scene(BLOCK_SIZE) {

    // Make a circle
    //
    int N = 360;
    circle.primitive(Graphics::LINE_STRIP);
    for (int i = 0; i < N + 1; ++i) {
      float theta = M_2PI / N * i;
      circle.vertex(cos(theta), sin(theta), 0);
      circle.color(Color(0.5));
      //circle.color(0, 0, 0);
    }

    // Make a square
    //
    square.primitive(Graphics::LINE_LOOP);
    square.vertex(-6025, -6025);
    square.vertex(6024, -6025);
    square.vertex(6024, 6024);
    square.vertex(-6025, 6024);
    square.color(Color(0.5));
    square.color(Color(0.5));
    square.color(Color(0.5));
    square.color(Color(0.5));

    string filePath = findPath(DATASET "map.txt");
    cout << filePath << endl;
    load(system, filePath);
    cout << system.size() << " systems loaded from map file" << endl;

    // build a mesh so we can draw all the systems
    //
    field.primitive(Graphics::POINTS);
    for (unsigned i = 0; i < system.size(); i++) {
      field.vertex(system[i].x, system[i].y, 1);
      field.color(HSV(0.6, 1, 1));
    }

    // position all the systems in the hashspace structure
    //
    for (unsigned i = 0; i < system.size(); i++) {
      float x = (system[i].x + 6025) / 12050;
      float y = (system[i].y + 6025) / 12050;
      //cout << x << "," << y << endl;
      space.move(i, x * space.dim(), y * space.dim(), 0);
    }


    // preload the cache with systems
    //
    findNeighbors(cache, x, y, cacheSize);
    for (int i : cache) {
      load(system[i]);
      field.color(HSV(0.1, 1, 1));
    }

    // add a slight randomization to the playback
    // rate of each system so they don't all line
    // up phase-wise
    //
    for (int i = 0; i < system.size(); ++i)
      system[i].player.rate(1.0 + rnd::uniformS() * 0.03);

    // load an image of the starfield
    //
    filePath = findPath(FFFI_FILE, false);
    Image i;
    imageFound = i.load(filePath);
    if (imageFound)
      fffi.allocate(i.array());


    // Make a window and setup UI
    //
    initWindow(Window::Dim(200, 200));
    nav().pos(x, y, z);
    lens().far(25010);
    lens().near(1);
    navControl().useMouse(false);

    // Audio configuration and initialization
    //

    //AudioDevice::printAll();
    //audioIO().print();
    //fflush(stdout);

    speakerLayout = new SpeakerLayout();
    if (onLaptop) {
      cout << "Using 2 speaker layout" << endl;
      speakerLayout->addSpeaker(Speaker(0, 45, 0, 1.0, 1.0));
      speakerLayout->addSpeaker(Speaker(1, -45, 0, 1.0, 1.0));
    }
    else {
      cout << "Using 3 speaker layout" << endl;
      speakerLayout->addSpeaker(Speaker(0,   0, 0, 100.0, 1.0));
      speakerLayout->addSpeaker(Speaker(1, 120, 0, 100.0, 1.0));
      speakerLayout->addSpeaker(Speaker(2,-120, 0, 100.0, 1.0));
      //speakerLayout->addSpeaker(Speaker(3,   0, 0,   0.0, 0.5));
    }
    panner = new Vbap(*speakerLayout);
    panner->setIs3D(false); // no 3d!
    //panner->print();
    listener = scene.createListener(panner);
    listener->compile(); // XXX need this?
    for (int i = 0; i < MAXIMUM_NUMBER_OF_SOUND_SOURCES; i++) {
      source[i].nearClip(0.1);
      source[i].farClip(cacheSize);
      source[i].dopplerType(DOPPLER_NONE); // XXX doppler kills when moving fast!
      //source[i].law(ATTEN_NONE);
      source[i].law(ATTEN_LINEAR);
      scene.addSource(source[i]);
    }
    scene.usePerSampleProcessing(false);
    //scene.usePerSampleProcessing(true);

    if (onLaptop) {
      cout << "we're on a laptop, so use default audio hardware" << endl;
      initAudio(44100, BLOCK_SIZE);
    }
    else {
      if (macOS) {
        cout << "we're on macOS (probably Karl's machine)" << endl;
        audioIO().device(AudioDevice("TASCAM"));
      }
      else {
        cout << "we're on the mini, so we will try the TASCAM" << endl;
        audioIO().device(AudioDevice("US-4x4 Wave"));
      }
      initAudio(44100, BLOCK_SIZE, 4, 0);
    }
    cout << "Using audio device: " << endl;
    audioIO().print();
    fflush(stdout);

    // Configure and start OSC
    //
    App::oscSend().open(13000, "127.0.0.1", 0.1, Socket::UDP | Socket::DGRAM);
    App::oscRecv().open(13004, "127.0.0.1", 0.1, Socket::UDP | Socket::DGRAM);
    App::oscRecv().handler(*this);
    App::oscRecv().start();
  }

  virtual void onSound(AudioIOData& io) {
    gam::Sync::master().spu(audioIO().fps());

    // make a copy of where we are..
    //
    float x = nav().pos().x;
    float y = nav().pos().y;

    // find all the neighbors in sorted order
    //
    vector<unsigned> n;
    findNeighbors(n, x, y, r, true);

    // send neighbors
    //
    oscSend().beginMessage("/knn");
    for (int i = 0; i < n.size(); i++)
      oscSend() << system[n[i]].name;
    oscSend().endMessage();
    oscSend().send();

    // set sound source positions
    //
    for (int i = 0; i < MAXIMUM_NUMBER_OF_SOUND_SOURCES; i++)
      if (i < n.size())
        source[i].pos(system[n[i]].x, system[n[i]].y, 0);

    // position the listener
    //
    //listener->pose(Pose(position, Quatd())); // XXX rotate the listener!
    listener->pos(x, y, 0);

    int numFrames = io.framesPerBuffer();
    for (int k = 0; k < numFrames; k++) {
      for (int i = 0; i < MAXIMUM_NUMBER_OF_SOUND_SOURCES; i++) {
        if (i < n.size()) {
          float f = 0;
          if (system[n[i]].player.size() > 1)
            f = system[n[i]].player();
          double d = isnan(f) ? 0.0 : (double)f;
          source[i].writeSample(d);
        }
        else {
          source[i].writeSample(0.0);
        }
      }
    }

    scene.render(io);
  }

  virtual void onAnimate(double dt) {
    z += zd;

    if (autonomous) {
      r += rd;
      if (go.mag() < 0.03) {
      } else if (go.mag() > 0.1) {
        x -= go.x * 4;
        y -= go.y * 4;
      } else if (go.mag() > 0.21) {
        x -= go.x * 16;
        y -= go.y * 16;
      }
      else {
        x -= go.x;
        y -= go.y;
      }
    }

    nav().quat(Quatf(1, 0, 0, 0));
    if ((Vec3d(x, y, z) - nav().pos()).mag() > 0.1)
      nav().pos(nav().pos() + (Vec3d(x, y, z) - nav().pos()) * 0.11);
    else
      nav().pos(Vec3d(x, y, z));

    vector<unsigned> latest;
    findNeighbors(latest, nav().pos().x, nav().pos().y, cacheSize);

    sort(cache.begin(), cache.end());
    sort(latest.begin(), latest.end());

    vector<unsigned> shouldLoad;
    set_difference(
      latest.begin(), latest.end(),
      cache.begin(), cache.end(),
      inserter(shouldLoad, shouldLoad.begin())
    );
    for (int i : shouldLoad) {
      load(system[i]);
      field.colors()[i].set(HSV(0.1, 1, 1), 1);
    }

    vector<unsigned> shouldClear;
    set_difference(
      cache.begin(), cache.end(),
      latest.begin(), latest.end(),
      inserter(shouldClear, shouldClear.begin())
    );
    for (int i : shouldClear) {
      unload(system[i]);
      field.colors()[i].set(HSV(0.6, 1, 1), 1);
    }

    cache.clear();
    for (int i : latest)
      cache.push_back(i);
  }

  virtual void onDraw(Graphics& g, const Viewpoint& v) {
    g.nicest();
    if (imageFound)
      if (shouldDrawImage)
        fffi.quad(g, 12050, 12050, -6024.5, -6024.5, -1);

    g.draw(field);
    g.draw(square);

    g.translate(nav().pos().x, nav().pos().y, 0);
    g.scale(r);
    g.draw(circle);
    g.scale(cacheSize/r);
    g.draw(circle);
  }

  virtual void onKeyDown(const ViewpointWindow& w, const Keyboard& k) {

    if (k.key() == ' ') {
      stringstream s;
      s << "echo " << x << "," << y << " >>coolshit";
      ::system(s.str().c_str());
    }

    if (k.key() == 'f') shouldDrawImage = !shouldDrawImage;

    if (k.key() == ']') zd = -100;
    if (k.key() == '[') zd = 100;

    if (autonomous) {
      if (k.key() == ',') rd = -0.33333333;
      if (k.key() == '.') rd = 0.33333333;
    }
  }
  virtual void onKeyUp(const ViewpointWindow& w, const Keyboard& k) {
    if (k.key() == '[' || k.key() == ']') zd = 0;

    if (autonomous) {
      if (k.key() == ',' || k.key() == '.') rd = 0;
    }
  }

  virtual void onMouseMove(const ViewpointWindow& w, const Mouse& m) {
    if (autonomous) {
      if (m.x() < 0 || m.y() < 0 || m.x() > w.dimensions().w || m.y() > w.dimensions().h) {
        go.normalize(0);
        return;
      }

      float x = (float)m.x() / w.dimensions().w;
      float y = (float)m.y() / w.dimensions().h;
      y = 1 - y;

      if (x < 0) x = 0;
      if (y < 0) y = 0;
      if (x > 1) x = 1;
      if (y > 1) y = 1;
      go = Vec3f(0.5, 0.5, 0) - Vec3f(x, y, 0);
    }
  }

  virtual void onMessage(osc::Message& m) {
    if (autonomous) {
      // XXX don't listen to them
      cout << "ignoring OSC message" << endl;
      m.print();
      return;
    }

    if (m.addressPattern() == "/xy") {
      m >> x >> y;
      //cout << "Look at (" << x << "," << y << ")" << endl;
    } else if (m.addressPattern() == "/z") {
      m >> z;
      //cout << "Zoom 'closeness': " << z << endl;
    } else if (m.addressPattern() == "/r") {
      m >> r;
      //cout << "Search Radius: " << r << endl;
    } else cout << "Unknown OSC message" << endl;
  }
};

int main(int argc, char* argv[]) {
  // look for stuff in this folder
  // and the folder above this one
  // and in each folder given as
  // an argument
  //
  path.push_back(".");
  path.push_back("..");
  for (int i = 1; i < argc; i++)
    path.push_back(argv[i]);
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
    sprintf(system.back().name, "%09u", system.back().kic);

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
    system.back().y = -atof(p); // flip the y axis to match image coordinates

    while (*p != '|') p++; p++; // cell 6: ra // IGNORE
    while (*p != '|') p++; p++; // cell 7: dec // IGNORE

    //
    // cell 8: mean amplitude
    while (*p != '|') p++; p++;
    system.back().amplitude = atof(p);

//    system.back().print();
  }
}
