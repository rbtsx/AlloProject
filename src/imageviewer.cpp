#include "allocore/io/al_App.hpp"
#include "allocore/io/al_File.hpp"
#include "allocore/graphics/al_Image.hpp"
#include "allocore/graphics/al_Texture.hpp"
using namespace al;
using namespace std;

SearchPaths searchPaths;

struct MyApp : App, al::osc::PacketHandler {

  Texture fffi;
  Mesh ring;

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
      float x = cos(theta);
      float y = sin(theta);
      ring.vertex(x, y, 0);
      ring.color(0, 0, 0);
    }

    //FilePath fffi = searchPaths.find("testFFFI.png");
    //FilePath fffi = searchPaths.find("FFFI.tif");
    FilePath filePath = searchPaths.find("printedFFFI.png");
    Image i;
    i.load(filePath.filepath());
    fffi.allocate(i.array());

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
  }

  virtual void onDraw(Graphics& g, const Viewpoint& v) {
    fffi.quad(g, 12050, 12050, -6025, -6025, -1);
    g.translate(x, y, 1);
    g.scale(r);
    g.draw(ring);
  }

  virtual void onKeyDown(const ViewpointWindow& w, const Keyboard& k) {}

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
