#include "allocore/graphics/al_Image.hpp"
#include "allocore/graphics/al_Texture.hpp"
#include "allocore/io/al_App.hpp"
#include "allocore/io/al_File.hpp"
#include "allocore/spatial/al_HashSpace.hpp"
#include <fstream> // ifstream
using namespace al;
using namespace std;

SearchPaths searchPaths;

HashSpace space(5, 220100);

struct StarSystem {
  //DynamicSamplePlayer player;
  unsigned kic;
  float x, y, amplitude;
  //float ascension, delcination;
  void print() {
    printf("%09d (%f,%f) %f\n", kic, x, y, amplitude);
  }
};

void load(vector<StarSystem>& system, string filePath);

struct MyApp : App, al::osc::PacketHandler {

  Texture fffi;
  Mesh ring;
  Mesh field;
  vector<StarSystem> system;

  float x = 0, y = 0, z = 2000, r = 20;

  MyApp() {
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
    cout << system.size() << endl;

    field.primitive(Graphics::POINTS);
    for (unsigned i = 0; i < system.size(); i++) {
      field.vertex(system[i].x, system[i].y, 1);
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

    initWindow(Window::Dim(800, 800));
  }

  virtual void onAnimate(double dt) {
    //nav().quat(Quatf(0, 0, 0, 1));
    //nav().quat().identity();
    nav().quat(Quatf(1, 0, 0, 0));
    if ((Vec3d(x, y, z) - nav().pos()).mag() > 0.1)
      nav().pos(nav().pos() + (Vec3d(x, y, z) - nav().pos()) * 0.07);
    else
      nav().pos(Vec3d(x, y, z));

    HashSpace::Query qmany;
    qmany.clear();
    float unit_x = (x + 6025) / 12050;
    float unit_y = (y + 6025) / 12050;
    float unit_r = r / 12050;
    int results = qmany(space,
      Vec3d(unit_x, unit_y, 0) * space.dim(),
      unit_r * space.dim());
    for (int i = 0; i < results; i++)
      system[qmany[i]->id].print();
    if (results)
      cout << ">--(" << results << ")--------------<" << endl;

  }

  virtual void onDraw(Graphics& g, const Viewpoint& v) {
    //fffi.quad(g, 12050, 12050, -6025, -6025, -1);
    g.draw(field);
    g.translate(x, y, 1);
    g.scale(r);
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
