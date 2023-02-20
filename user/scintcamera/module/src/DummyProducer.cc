#include "eudaq/Producer.hh"
#include <pylon/PylonIncludes.h>
//#include <pylon/gige/GigETransportLayer.h>
#include <pylon/BaslerUniversalInstantCamera.h>
#include <pylon/camemu/PylonCamEmuIncludes.h>

#ifdef PYLON_WIN_BUILD
#  include <pylon/PylonGUI.h>
#endif

//using namespace GenApi;
class DummyProducer : public eudaq::Producer {
public:
  DummyProducer(const std::string name, const std::string &runcontrol);
  ~DummyProducer() override;
  void DoInitialise() override;
  void DoConfigure() override;
  void DoStartRun() override;
  void DoStopRun() override;
  void DoReset() override;
  void DoTerminate() override;
  void RunLoop() override;

  static const uint32_t m_id_factory = eudaq::cstr2hash("DummyProducer");

private:
  bool m_running;
  Pylon::CBaslerUniversalInstantCamera* camera;
  constexpr static const float imagegrabtimeout = 10000;  // in ms
  std::string filelocation;
  uint64_t n_images;
  uint64_t images_taken;
  bool initialized;
  std::chrono::steady_clock::time_point time_after_trigger{};
  std::chrono::duration<double> time_between_triggers;
  std::chrono::steady_clock::time_point time_before_trigger{};
};

namespace{
  auto dummy0 = eudaq::Factory<eudaq::Producer>::
    Register<DummyProducer, const std::string&, const std::string&>(DummyProducer::m_id_factory);
}

DummyProducer::DummyProducer(const std::string name, const std::string &runcontrol)
  : eudaq::Producer(name, runcontrol), m_running(false){
  m_running = false;
}

DummyProducer::~DummyProducer(){
  m_running = false;
}

void DummyProducer::RunLoop(){
  try{
    /*Pylon::CTlFactory& TlFactory = Pylon::CTlFactory::GetInstance();
    Pylon::IGigETransportLayer* pTl = dynamic_cast<Pylon::IGigETransportLayer*>(TlFactory.CreateTl( Pylon::BaslerGigEDeviceClass ));
    if (!pTl)
      {
        std::cerr << "Error: No GigE transport layer installed." << std::endl;
        std::cerr << "       Please install GigE support as it is required for this sample." << std::endl;
        DoTerminate();
      }*/
    camera->TriggerSelector.SetValue(Basler_UniversalCameraParams::TriggerSelector_FrameStart);
    // Enable triggered image acquisition for the Frame Start trigger
    camera->TriggerMode.SetValue(Basler_UniversalCameraParams::TriggerMode_On);
    // Set the trigger source for the Frame Start trigger to Software
    camera->TriggerSource.SetValue(Basler_UniversalCameraParams::TriggerSource_Software);

    camera->StartGrabbing(Pylon::EGrabStrategy::GrabStrategy_OneByOne, Pylon::EGrabLoop::GrabLoop_ProvidedByUser);

    Pylon::CBaslerUniversalGrabResultPtr ptrGrabResult;

    while(m_running){

      if(camera->WaitForFrameTriggerReady( 10000, Pylon::TimeoutHandling_ThrowException )){
        std::cout << "camera ready and waiting " << std::endl;
        // Generate a software trigger signal
        time_before_trigger = std::chrono::steady_clock::now();
        camera->TriggerSoftware.Execute();
        time_between_triggers = time_before_trigger - time_after_trigger;
        time_after_trigger = std::chrono::steady_clock::now();
      }
      camera->RetrieveResult( imagegrabtimeout,  ptrGrabResult);   // try to get one of the camera image results
      if (ptrGrabResult->GrabSucceeded()){
        std::cout << "Succesfully grabbed image" << std::endl;
        std::string filename =  filelocation + "image" + (images_taken==0?"_t=0_":("_t=" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(time_between_triggers).count())) + "_") + std::to_string(images_taken);
        Pylon::CImagePersistence::Save( Pylon::ImageFileFormat_Tiff, filename.c_str(), ptrGrabResult );     // save the image taken, in the previsouly defined path, with parameters as above
        images_taken++;
      }else{
        std::cout << "Unable to capture image: Error Code "<< ptrGrabResult->GetErrorCode() << std::endl <<"Error Description: " << ptrGrabResult->GetErrorDescription() << std::endl << std::endl << "The softare will try again in a few seconds" << std::endl;
      }
      ptrGrabResult.Release();
      if(images_taken == n_images){
        DoStopRun();
        m_running=false;
      }
    }
  }catch(const GenICam_3_1_Basler_pylon::GenericException& e){
    std::cerr << "An exception occurred." << std::endl << e.GetDescription() << std::endl;

  }catch(const std::exception e){
    std::cerr << "An exception occurred: " << e.what() << std::endl;
  }


 
}

void DummyProducer::DoInitialise(){
  EUDAQ_DEBUG("Starting PROD INIT");
  auto conf = GetInitConfiguration();
#if defined(PYLON_WIN_BUILD)
    _putenv("PYLON_CAMEMU=1");
#elif defined(PYLON_UNIX_BUILD)
    setenv("PYLON_CAMEMU", "1", true);
#endif
  Pylon::PylonInitialize();

  Pylon::DeviceInfoList_t devices;
  if (Pylon::CTlFactory::GetInstance().EnumerateDevices(devices) == 0){
    EUDAQ_DEBUG("No camera present.");
    Pylon::PylonTerminate();
    EUDAQ_THROW("No camera present.");
  }
  Pylon::CDeviceInfo deviceInfo;
  EUDAQ_DEBUG("Number of Cameras: "+std::to_string(Pylon::CTlFactory::GetInstance().EnumerateDevices(devices)));
  deviceInfo.SetSerialNumber(devices[0].GetSerialNumber());

  // Get the transport layer factory.
  Pylon::CTlFactory& tlFactory = Pylon::CTlFactory::GetInstance();
  initialized = false;

  // This checks, among other things, that the camera is not already in use.
  // Without that check, the following CreateDevice() may crash on duplicate
  // serial number. Unfortunately, this call is slow.
  Pylon::EDeviceAccessiblityInfo isAccessableInfo;
  EUDAQ_DEBUG("trying to open camera with Serial Number " + std::string(deviceInfo.GetSerialNumber()));
  EUDAQ_DEBUG( "The current state of selected camera " + std::to_string(isAccessableInfo));
  if (!tlFactory.IsDeviceAccessible(deviceInfo, Pylon::Control, &isAccessableInfo)){
    //EUDAQ_DEBUG("trying to open camera with SN " + std::string(deviceInfo.GetSerialNumber()));
    //EUDAQ_DEBUG( "The current state of selected camera " + std::to_string(isAccessableInfo));
    EUDAQ_ERROR("Can't Connect to Camera");
  }

  Pylon::IPylonDevice* device = tlFactory.CreateDevice(deviceInfo);
  if (!device){
    EUDAQ_ERROR("Can't Connect to Camera");
  }else{
    if (camera && camera->IsPylonDeviceAttached()){
      camera->DestroyDevice();
    }
    camera = new Pylon::CBaslerUniversalInstantCamera(device);
    initialized = true;
  }
  if(!initialized){
    EUDAQ_THROW("Camera could not be initialized");
  }
  EUDAQ_DEBUG("Finished PROD INIT");

}

void DummyProducer::DoConfigure(){
  EUDAQ_DEBUG("Starting PROD CONF");
  auto conf = GetConfiguration();
  filelocation ="./";
  n_images =10;
  EUDAQ_DEBUG("Finished PROD CONF");

}

void DummyProducer::DoStartRun(){
  EUDAQ_DEBUG("Starting PROD STARTRUN");
  if(camera->IsPylonDeviceAttached()){
    if(!camera->IsOpen()){
    camera->Open();
    EUDAQ_DEBUG("Succesfully Opened Camera");
    }else{
    EUDAQ_THROW("Camera was already open before the run started");
    }
  }else{
    EUDAQ_THROW("No Camera attached");
  }

  EUDAQ_DEBUG("Selecting Test Image");
  camera->TestImageSelector.SetValue(Basler_UniversalCameraParams::TestImageSelectorEnums::TestImageSelector_Testimage2);
  EUDAQ_DEBUG("Test Image Succesfully Selected");
  images_taken=0;
  m_running = true;
  EUDAQ_DEBUG("Finished PROF STARTRUN");
}

void DummyProducer::DoStopRun(){
  m_running = false;
  if(camera->IsPylonDeviceAttached()){
    if(camera->IsOpen()){
      camera->Close();
    }
  }

}

void DummyProducer::DoReset(){
  m_running = false;
  initialized = false;
  images_taken = 0;
  if(camera->IsPylonDeviceAttached()){
    if(camera->IsOpen()){
      camera->Close();
    }
    camera->DestroyDevice();
  }
  Pylon::PylonTerminate();

}

void DummyProducer::DoTerminate() {
  m_running = false;
  initialized = false;
  if(camera->IsPylonDeviceAttached()){
    if(camera->IsOpen()){
      camera->Close();
    }
    camera->DestroyDevice();
  }
  Pylon::PylonTerminate();
}
