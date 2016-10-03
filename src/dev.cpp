#include "allocore/io/al_App.hpp"
using namespace al;
using namespace std;

#define BLOCK_SIZE (1024)

struct MyApp : App {

  AudioScene scene;
  SpeakerLayout* speakerLayout;
  Vbap* panner;
  Listener* listener;
  bool onLaptop = false;
  SoundSource source[10];

  MyApp() : scene(BLOCK_SIZE) {
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

    listener = scene.createListener(panner);
    listener->compile();
    for (int i = 0; i < 10; i++) {
      source[i].dopplerType(DOPPLER_NONE); // XXX doppler kills when moving fast!
      source[i].law(ATTEN_LINEAR);
      scene.addSource(source[i]);
    }
    panner->print();

    scene.usePerSampleProcessing(false);

    AudioDevice::printAll();

    audioIO().print();
    fflush(stdout);

    if (onLaptop) {
      cout << "we're on a laptop, so use normal, default audio hardware" << endl;
      initAudio(44100, BLOCK_SIZE);
    }
    else {
      cout << "we're on the mini, so we will try the TASCAM" << endl;
//      audioIO().device(AudioDevice("TASCAM"));
      audioIO().device(AudioDevice(29));
      initAudio(44100, BLOCK_SIZE, 4, 0);
    }
    cout << "GOT HERE" << endl;

    audioIO().print();
    fflush(stdout);
  }

  virtual void onSound(AudioIOData& io) {}
};

int main(int argc, char* argv[]) {
  MyApp app;
  app.start();
}
