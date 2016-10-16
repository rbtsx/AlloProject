#include "Gamma/Filter.h"
#include "Gamma/SamplePlayer.h"
#include "allocore/graphics/al_Image.hpp"
#include "allocore/graphics/al_Texture.hpp"
#include "allocore/io/al_App.hpp"
#include "allocore/io/al_File.hpp"
#include "allocore/sound/al_Vbap.hpp"
#include <fstream> // ifstream
#include <algorithm> // sort
#include <set>
using namespace al;
using namespace std;

//#define DATASET "wav_sonify/"
#define DATASET "wav_182864/"
//#define FFFI_FILE "testFFFI.png"
//#define FFFI_FILE "FFFI.tif"
#define FFFI_FILE "printedFFFI.png"

#define MAXIMUM_NUMBER_OF_SOUND_SOURCES (50)
#define BLOCK_SIZE (2048)

// TODO:
// - Figure out why some starsystems within the load
//     radius do not change color; are they
//     loaded? or just not colored?
// - do audio analysis:
//   + take the FFT of each starsystem
//   + find onsets/transients of each starsystem
//   + get the noise-floor
//   + characterize as choppy or smooth
// - do audio effects/synthesis
//   + add dynamic range compression
//   + add reverb
//   + resynthesize starsystems
// - tune the exponential easing constant to match
// - try increasing the number of sound sources
// - add text HUD with KOI information
// - invert fffi image colors (toggle)
// - make doppler toggle
// - make "autonomous mode" toggle
// - render star starsystems as perspective point cloud
// - implement "jumping" the cursor on click
// - use message-passing queues between threads
// - star starsystem eraser tool
//

vector<string> path;

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

vector<StarSystem> starsystem;

struct Node {
  unsigned index;
  Node *left, *right;
};

struct Node* newNode(unsigned i) {
  struct Node* temp = new Node;
  temp->index = i;
  temp->left = temp->right = NULL;
  return temp;
}

// Inserts a new node and returns root of modified tree
// The parameter depth is used to decide axis of comparison
Node *insertRec(Node *root, unsigned i, unsigned depth) {
  // tree is empty?
  if (root == NULL)
    return newNode(i);

  // current dimension
  unsigned d = depth % 2;

  Vec2f r(starsystem[root->index].x, starsystem[root->index].y);
  Vec2f v(starsystem[i].x, starsystem[i].y);

  if (v.elems()[d] < r.elems()[d])
    root->left  = insertRec(root->left, i, depth + 1);
  else
    root->right = insertRec(root->right, i, depth + 1);

  return root;
}

Node* insert(Node *root, unsigned i) {
  return insertRec(root, i, 0);
}

bool isWithin(Vec2f a, Vec2f b, float r) {
  if ((a - b).mag() < r)
    return true;
  return false;
}

void findNearestNeighborsRec(Node* root, Vec2f v, float searchRadius, vector<unsigned>& within, unsigned depth, unsigned& best, float& bestDistance) {

  // base case
  //
  if (root == NULL) return;

  Vec2f candidate(starsystem[root->index].x, starsystem[root->index].y);

  if (isWithin(candidate, v, searchRadius))
    within.push_back(root->index);

  // leave node
  if (root->left == NULL && root->right == NULL) {
    best = root->index;
    bestDistance = (v - candidate).mag();
    return;
  }

  //findNearestNeighborsRec(root->right, v, searchRadius, within, depth + 1, best, bestDistance);

  // current dimension
  //
  unsigned d = depth % 2;

  bool goLeft = v.elems()[d] < candidate.elems()[d];
  findNearestNeighborsRec(goLeft ? root->left : root->right, v, searchRadius, within, depth + 1, best, bestDistance);

  // we've recursed all the way to the leaves...

  float b = (candidate - Vec2f(starsystem[best].x, starsystem[best].y)).mag();
  if (b < bestDistance) {
    bestDistance = b;
    best = root->index;
  }

  if (abs(v.elems()[d] - candidate.elems()[d]) < searchRadius)
    findNearestNeighborsRec(goLeft ? root->right : root->left, v, searchRadius, within, depth + 1, best, bestDistance);
}

void findNearestNeighbors(Node* root, Vec2f v, float searchRadius, vector<unsigned>& within) {
  unsigned int best = 0;
  float bestDistance = 99999999.0f;
  findNearestNeighborsRec(root, v, searchRadius, within, 0, best, bestDistance);
}

void sortListRec(vector<unsigned>& given, vector<unsigned>& sorted, unsigned depth) {
  if (given.size() == 0)
    return;
  else if (given.size() == 1) {
    sorted.push_back(given[0]);
    return;
  }
  else if (given.size() == 2) {
    sorted.push_back(given[0]);
    sorted.push_back(given[1]);
    return;
  }

  // find the median of the dimension
  //
  unsigned d = depth % 2;
  sort(given.begin(), given.end(),
    [=](unsigned a, unsigned b) {
      Vec2f A(starsystem[a].x, starsystem[a].y);
      Vec2f B(starsystem[b].x, starsystem[b].y);
      return A.elems()[d] < B.elems()[d];
    }
  );
  unsigned middle = given.size() / 2 + 1;
  sorted.push_back(given[middle]);

  vector<unsigned> left, right;
  for (unsigned i = 0; i < given.size(); i++)
    if (i < middle)
      left.push_back(given[i]);
    else if (i > middle)
      right.push_back(given[i]);

  sortListRec(left, sorted, 1 + depth);
  sortListRec(right, sorted, 1 + depth);
}

void sortList(vector<unsigned>& given, vector<unsigned>& sorted) {
  sortListRec(given, sorted, 0);
}
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

bool load(StarSystem& starsystem) {
  char fileName[200];
  sprintf(fileName, DATASET "%09d.g.bin.wav", starsystem.kic);
  string filePath = findPath(fileName, false);
  if (starsystem.player.load(filePath.c_str())) {
    //cout << "Loaded " << fileName << " into memory!"<< endl;
    return true;
  }

  assert(false);
  return false;
}

void unload(StarSystem& starsystem) {
  char fileName[200];
  sprintf(fileName, DATASET "%09d.g.bin.wav", starsystem.kic);
  //cout << "Unloaded " << fileName << " from memory!"<< endl;
  starsystem.player.clear();
}

void load(vector<StarSystem>& starsystem, string filePath);

struct MyApp : App, al::osc::PacketHandler {

  Node* kd = NULL; // XXX very important that this is initially NULL!!!

  bool macOS = false;
  bool autonomous = false;
  bool imageFound = false;
  bool shouldDrawImage = false;

  float zd = 0, rd = 0;
  float x = 0, y = 0, z = 666, listenRadius = 75, loadRadius = 100, unloadRadius = 150, near = 0.2;
  Vec3f go;

  //vector<unsigned> loaded;
  set<unsigned> loaded;

  // Audio Spatialization
  //
  AudioScene scene;
  SpeakerLayout* speakerLayout;
  Spatializer* panner;
  Listener* listener;
  SoundSource source[MAXIMUM_NUMBER_OF_SOUND_SOURCES];

  // Graphics
  //
  Texture fffi;
  Mesh circle, field, square;

  void findNeighbors(vector<unsigned>& n, float x, float y, float r, bool shouldSort = false) {
    findNearestNeighbors(kd, Vec2f(x, y), r, n);
    if (shouldSort) {
      sort(n.begin(), n.end(),
        [&](unsigned a, unsigned b) {
        float xa = x - starsystem[a].x;
        float ya = y - starsystem[a].y;
        float xb = x - starsystem[b].x;
        float yb = y - starsystem[b].y;
        return (xa * xa + ya * ya)
          < (xb * xb + yb * yb);
      });
/*
      // XXX is this correct???
      sort(n.begin(), n.end(), [&](unsigned a, unsigned b) {
        float dist_a = (Vec2f(x, y) - Vec2f(starsystem[a].x, starsystem[a].y)).mag();
        float dist_b = (Vec2f(x, y) - Vec2f(starsystem[b].x, starsystem[b].y)).mag();
        if (dist_a > dist_b) return -1;
        if (dist_b > dist_a) return 1;
        return 0;
      });
*/
    }
  }

  MyApp() : scene(BLOCK_SIZE) {
    macOS = system("ls /Applications >> /dev/null") == 0;
    autonomous = macOS;

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
    load(starsystem, filePath);
    cout << starsystem.size() << " starsystems loaded from map file" << endl;

    vector<unsigned> initial, sorted;
    for (unsigned i = 0; i < starsystem.size(); i++)
      initial.push_back(i);
    sortList(initial, sorted);
    assert(initial.size() == sorted.size());
    for (auto e : sorted)
      kd = insert(kd, e);
/*
    for (int i = 0; i < 10; i++) {
      Vec2f spot = Vec2f(rnd::uniform(-6025.0f, 6024.0f), rnd::uniform(-6025.0f, 6024.0f));
      cout << "----- within 60 of " << spot << " -----------------" << endl;
      vector<unsigned> neighbor;
      findNearestNeighbors(kd, spot, 60, neighbor);
      for (unsigned e : neighbor)
        cout << starsystem[e].x << "," << starsystem[e].y << endl;
    }
*/
    // build a mesh so we can draw all the starsystems
    //
    field.primitive(Graphics::POINTS);
    for (unsigned i = 0; i < starsystem.size(); i++) {
      field.vertex(starsystem[i].x, starsystem[i].y, 1);
      field.color(HSV(0.6, 1, 1));
    }

    // preload the cache with starsystems
    //
    vector<unsigned> foo;
    findNeighbors(foo, x, y, loadRadius);
    for (int i : foo) {
      load(starsystem[i]);
      //loaded.push_back(i);
      loaded.insert(i);
      // XXX this was a bad indicator bug!!
      // field.color(HSV(0.1, 1, 1));
      field.colors()[i].set(HSV(0.1, 1, 1), 1);
    }


    // add a slight randomization to the playback
    // rate of each starsystem so they don't all line
    // up phase-wise
    //
    for (int i = 0; i < starsystem.size(); ++i)
      starsystem[i].player.rate(1.0 + rnd::uniformS() * 0.03);

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

    if (macOS) {
      speakerLayout = new HeadsetSpeakerLayout();
      panner = new StereoPanner(*speakerLayout);
    }
    else {
      cout << "Using 3 speaker layout" << endl;
      speakerLayout = new SpeakerLayout();
      speakerLayout->addSpeaker(Speaker(0,   0, 0, 100.0, 1.0));
      speakerLayout->addSpeaker(Speaker(1, 120, 0, 100.0, 1.0));
      speakerLayout->addSpeaker(Speaker(2,-120, 0, 100.0, 1.0));
      //speakerLayout->addSpeaker(Speaker(3,   0, 0,   0.0, 0.5));
      panner = new Vbap(*speakerLayout);
      dynamic_cast<Vbap*>(panner)->setIs3D(false); // no 3d!
    }
    panner->print();
    listener = scene.createListener(panner);
    listener->compile(); // XXX need this?
    for (int i = 0; i < MAXIMUM_NUMBER_OF_SOUND_SOURCES; i++) {
      source[i].nearClip(near);
      source[i].farClip(listenRadius);
      if (macOS) {
        source[i].law(ATTEN_INVERSE_SQUARE);
        source[i].dopplerType(DOPPLER_NONE); // XXX doppler kills when moving fast!
        //source[i].law(ATTEN_INVERSE);
      }
      else {
        source[i].dopplerType(DOPPLER_NONE); // XXX doppler kills when moving fast!
        source[i].law(ATTEN_NONE);
      }
      //source[i].law(ATTEN_LINEAR);
      scene.addSource(source[i]);
    }
    scene.usePerSampleProcessing(false);
    //scene.usePerSampleProcessing(true);

    if (macOS) {
      audioIO().device(AudioDevice("TASCAM"));
      //initAudio(44100, BLOCK_SIZE);
      initAudio(44100, BLOCK_SIZE, 4, 0);
    }
    else {
      audioIO().device(AudioDevice("US-4x4 Wave"));
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
    findNeighbors(n, x, y, listenRadius, true);

//    for (int i = 0; i < n.size(); i++)
//      cout << n[i] << " ";
    //cout << n.size() << endl;


    // send neighbors
    //
    oscSend().beginMessage("/knn");
    for (int i = 0; i < n.size(); i++)
      if (i < MAXIMUM_NUMBER_OF_SOUND_SOURCES)
        oscSend() << starsystem[n[i]].name;
    oscSend().endMessage();
    oscSend().send();

    // set sound source positions
    //
    for (int i = 0; i < MAXIMUM_NUMBER_OF_SOUND_SOURCES; i++)
      if (i < n.size())
        source[i].pos(starsystem[n[i]].x, starsystem[n[i]].y, 0);

    // position the listener
    //
    //listener->pose(Pose(position, Quatd())); // XXX rotate the listener!
    listener->pos(x, y, 0);

    int numFrames = io.framesPerBuffer();
    for (int k = 0; k < numFrames; k++) {
      for (int i = 0; i < MAXIMUM_NUMBER_OF_SOUND_SOURCES; i++) {
        if (i < n.size()) {
          float f = 0;
          if (starsystem[n[i]].player.size() > 1)
            f = starsystem[n[i]].player();
          double d = isnan(f) ? 0.0 : (double)f; // XXX need this nan check?
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
    if (z > 25010) z = 25010;
    if (z < 1) z = 1;

    if (autonomous) {
      listenRadius += rd;
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

    // force nav to face forward
    //
    nav().quat(Quatf(1, 0, 0, 0));

    // ease toward the target
    //
    if ((Vec3d(x, y, z) - nav().pos()).mag() > 0.01)
      nav().pos(nav().pos() + (Vec3d(x, y, z) - nav().pos()) * 0.9);
    else
      nav().pos(Vec3d(x, y, z));

    vector<unsigned> neighbor;
    findNearestNeighbors(kd, Vec2f(x, y), loadRadius, neighbor);
    for (int i : neighbor)
      if (loaded.find(i) == loaded.end()) {
        field.colors()[i].set(HSV(0.1, 1, 1), 1);
        load(starsystem[i]);
      }

    vector<unsigned> keep;
    for (int i : loaded)
      if ((Vec2f(starsystem[i].x, starsystem[i].y)
            - Vec2f(x, y)).mag()
          > unloadRadius) {
        unload(starsystem[i]);
        field.colors()[i].set(HSV(0.6, 1, 1), 1);
      }
      else {
        keep.push_back(i);
      }

      loaded.clear();
      for (auto i : keep)
        loaded.insert(i);
      for (auto i : neighbor)
        loaded.insert(i);

/*

    sort(loaded.begin(), loaded.end());

    vector<unsigned> neighbor, shouldLoad;
    findNearestNeighbors(kd, Vec2f(nav().pos().x, nav().pos().y), loadRadius, neighbor);
    sort(neighbor.begin(), neighbor.end());
    set_difference(
      neighbor.begin(), neighbor.end(),
      loaded.begin(), loaded.end(),
      inserter(shouldLoad, shouldLoad.begin())
    );

    vector<unsigned> keep, shouldUnload;
    findNearestNeighbors(kd, Vec2f(nav().pos().x, nav().pos().y), unloadRadius, keep);
    sort(keep.begin(), keep.end());
    set_difference(
      loaded.begin(), loaded.end(),
      keep.begin(), keep.end(),
      inserter(shouldUnload, shouldUnload.begin())
    );

    loaded.clear();
    for (int i : shouldLoad)
      loaded.push_back(i);
    for (auto i : keep)
      loaded.push_back(i);

    for (auto i : shouldLoad) {
      load(starsystem[i]);
      field.colors()[i].set(HSV(0.1, 1, 1), 1);
    }

    for (int i : shouldUnload) {
      unload(starsystem[i]);
      field.colors()[i].set(HSV(0.6, 1, 1), 1);
    }
*/
  }

  virtual void onDraw(Graphics& g, const Viewpoint& v) {
    g.nicest();
    if (imageFound)
      if (shouldDrawImage)
        fffi.quad(g, 12050, 12050, -6024.5, -6024.5, -1);

    g.draw(field);
    g.draw(square);

    g.translate(nav().pos().x, nav().pos().y, 0);
    g.scale(listenRadius);
    g.draw(circle);
    g.scale(loadRadius/listenRadius);
    g.draw(circle);
    g.scale(near/loadRadius);
    g.draw(circle);
    g.scale(unloadRadius/near);
    g.draw(circle);
  }

  virtual void onKeyDown(const ViewpointWindow& w, const Keyboard& k) {

    if (k.key() == ' ') {
      stringstream s;
      s << "echo " << x << "," << y << " >>coolshit";
      system(s.str().c_str());
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
      m >> listenRadius;
      //cout << "Search Radius: " << listenRadius << endl;
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

void load(vector<StarSystem>& starsystem, string filePath) {
  ifstream mapFile(filePath);
  string line;
  while (getline(mapFile, line)) {
//    cout << line.length() << endl;
    char *p = new char[line.length() + 1];
    strcpy(p, line.c_str());
    starsystem.push_back(StarSystem());

    //
    // cell 0: kic
    starsystem.back().kic = atoi(p);
    sprintf(starsystem.back().name, "%09u", starsystem.back().kic);

    while (*p != '|') p++; p++; // cell 1: ch // IGNORE
    while (*p != '|') p++; p++; // cell 2: x-coord in channel // IGNORE
    while (*p != '|') p++; p++; // cell 3: y-coord in channel // IGNORE

    //
    // cell 4: x-coord in FFFI
    while (*p != '|') p++; p++;
    starsystem.back().x = atof(p);
    //
    // cell 5: y-coord in FFFI
    while (*p != '|') p++; p++;
    starsystem.back().y = -atof(p); // flip the y axis to match image coordinates

    while (*p != '|') p++; p++; // cell 6: ra // IGNORE
    while (*p != '|') p++; p++; // cell 7: dec // IGNORE

    //
    // cell 8: mean amplitude
    while (*p != '|') p++; p++;
    starsystem.back().amplitude = atof(p);

//    starsystem.back().print();
  }
}
