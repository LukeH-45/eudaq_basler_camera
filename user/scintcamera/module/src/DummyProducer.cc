#include "eudaq/Producer.hh"
#include <pylon/PylonIncludes.h>
//#include <pylon/gige/GigETransportLayer.h>
#include <pylon/BaslerUniversalInstantCamera.h>
#include <pylon/camemu/PylonCamEmuIncludes.h>

#ifdef PYLON_WIN_BUILD
#  include <pylon/PylonGUI.h>
#endif

//using namespace GenApi;

enum MyEvents
{
    FrameTriggerEvent = 100,
    TriggerOverrunEvent = 200,
    ExposureEndEvent = 300
};

// Example handler for camera events.
class OurCameraEventHandler : public Pylon::CBaslerUniversalCameraEventHandler
{
public:
  virtual void OnCameraEvent( Pylon::CBaslerUniversalInstantCamera& camera, intptr_t userProvidedId, GenApi::INode* /* pNode */ )
  {
    switch (userProvidedId)
      {

      case FrameTriggerEvent:
	std::cout << "Frame Start event. Timestamp: " << camera.EventFrameStartTimestamp.GetValue() << std::endl << "In seconds: " << (float) camera.EventFrameStartTimestamp.GetValue() / camera.GevTimestampTickFrequency.GetValue() << std::endl << "That makes trigger count " << camera.CounterEventSource.GetValue() << " successful. " << std::endl;
	break;

      case TriggerOverrunEvent:
	std::cout << "Frame Trigger Overrun event. This should not Happen! Timestamp: " << camera.FrameStartOvertriggerEventTimestamp.GetValue() << std::endl
	     << "In seconds: " << (float) camera.FrameStartOvertriggerEventTimestamp.GetValue() / camera.GevTimestampTickFrequency.GetValue() << std::endl;
	break;

      case ExposureEndEvent:
	std::cout << "Exposure End event. FrameID: " << camera.ExposureEndEventFrameID.GetValue() << " Timestamp: " << camera.ExposureEndEventTimestamp.GetValue() << std::endl;
	break;

      }
  }
};

std::string EnumToString(Pylon::EDeviceAccessiblityInfo AccessiblityInfo)
{
	switch (AccessiblityInfo)
	{
      case Pylon::Accessibility_Unknown:
		return "Unknown Device Status";
      case Pylon::Accessibility_Ok:
		return "Device  is ready to be open";
      case Pylon::Accessibility_Opened:
		return "Device  is ready to be open";
      case Pylon::Accessibility_OpenedExclusively:
		return  "Device is currently opened exclusively by another application ";
      case Pylon::Accessibility_NotReachable:
		return  "Device is not reachable ";
	}
	return "Unknown";
}

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

  OurCameraEventHandler* pHandler1;
  Pylon::CBaslerUniversalInstantCamera* camera;
  //Pylon::IGigETransportLayer* pTl;
  Pylon::IPylonDevice* device;

  //Configurable vars
  float imagegrabtimeout;// = 10000;  // in ms
  int imagestograb;  // Number of images to be grabbed per camera per event. Should remain one.  // per camera per trigger,#
  float framerate; // an estimate!! Hz
  int max_buffer_frames; // memory buffer
  int max_images;  // images to collect per camera per run.
  std::string filelocation;  // images saved to ..
  bool frameratebool; // using framerate mode, false to enable trigger
  float network_capacity; // throughput of bottleneck in network in MB/s
  float safety_factor; //buffer for timing of sending of data from cameras

  //next are some arrays with values for each camera
  int exposure_time_us; // in microseconds
  int packet_size; // should be largest possible in network
  int inter_packet_delay;
  int pixel_n_x;
  int pixel_n_y;   // size of the image in y dimension
  // static const int brightness_multiplier[4] = {1, 1, 1, 1}; // a brightness multiplier for the final, accumulated image
  int pixel_x_offset; //these are unimportant if the images are centred
  int pixel_y_offset;
  bool centerimage; // centre sensitive sensor area in sensor? certainly

  int gain_raw;

  float trigger_delay;
  int pixel_signal_bkg_vertical_border; // border between central signal rectangle & outer bkg region of the image
  int pixel_signal_bkg_horizontal_border;

  bool binning; // for combining values for pixels into one
  int binning_x;
  int binning_y;

  bool enable_cam_emu;

  Basler_UniversalCameraParams::PixelFormatEnums pixel_format = Basler_UniversalCameraParams::PixelFormat_Mono12Packed;
  Basler_UniversalCameraParams::TriggerSelectorEnums trigger_start_type = Basler_UniversalCameraParams::TriggerSelector_FrameStart;
  Basler_UniversalCameraParams::TriggerModeEnums trigger_onoff = Basler_UniversalCameraParams::TriggerMode_On;
  // Basler_UniversalCameraParams::TriggerSourceEnums trigger_source = TriggerSource_Line1;
  Basler_UniversalCameraParams::TriggerSourceEnums trigger_source = Basler_UniversalCameraParams::TriggerSource_Software; // images will be created when triggered by a line in this software

  Basler_UniversalCameraParams::TriggerActivationEnums trigger_activation = Basler_UniversalCameraParams::TriggerActivation_RisingEdge;  // for hardware trigger
  Basler_UniversalCameraParams::LineSelectorEnums line_selection = Basler_UniversalCameraParams::LineSelector_Line1;
  Basler_UniversalCameraParams::AcquisitionStatusSelectorEnums acquisition_status_selector  = Basler_UniversalCameraParams::AcquisitionStatusSelector_FrameTriggerWait;


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
    camera->TriggerSelector.SetValue(trigger_start_type);
    // Enable triggered image acquisition for the Frame Start trigger
    camera->TriggerMode.SetValue(trigger_onoff);
    // Set the trigger source for the Frame Start trigger to Software
    camera->TriggerSource.SetValue(trigger_source);

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
        EUDAQ_ERROR("Unable to capture image: Error Code "+ std::to_string(ptrGrabResult->GetErrorCode()) + "\nError Description: " + ptrGrabResult->GetErrorDescription().c_str() + "\n\nThe softare will try again in a few seconds\n");
      }
      ptrGrabResult.Release();
      if(images_taken == max_images){
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
  auto ini = GetInitConfiguration();
  enable_cam_emu = ini->Get<bool>("ENABLE_CAM_EMU",true);
  EUDAQ_DEBUG("enable_cam_emu = "+ std::to_string(enable_cam_emu));
  if(enable_cam_emu){
#if defined(PYLON_WIN_BUILD)
    _putenv("PYLON_CAMEMU=1");
#elif defined(PYLON_UNIX_BUILD)
    setenv("PYLON_CAMEMU", "1", true);
#endif
  }
  Pylon::PylonInitialize();
  pHandler1 = new OurCameraEventHandler;

  Pylon::DeviceInfoList_t devices;
  if (Pylon::CTlFactory::GetInstance().EnumerateDevices(devices) == 0){
    EUDAQ_DEBUG("No camera present.");
    Pylon::PylonTerminate();
    EUDAQ_THROW("No camera present.");
  }
   // Get the transport layer factory.
  Pylon::CTlFactory& tlFactory = Pylon::CTlFactory::GetInstance();
  //if(enable_cam_emu){
    //pTl = dynamic_cast<Pylon::IGigETransportLayer*>(tlFactory.CreateTl( Pylon::BaslerGigEDeviceClass ));
    //if (!pTl)
    //{
      //EUDAQ_THROW("Error: No GigE transport layer installed.\n       Please install GigE support as it is required for this sample.");
    //}
 // }
  Pylon::CDeviceInfo deviceInfo;

  EUDAQ_DEBUG("Number of Cameras: "+std::to_string(tlFactory.EnumerateDevices(devices)));
  deviceInfo.SetSerialNumber(devices[0].GetSerialNumber());


  initialized = false;

  // This checks, among other things, that the camera is not already in use.
  // Without that check, the following CreateDevice() may crash on duplicate
  // serial number. Unfortunately, this call is slow.
  Pylon::EDeviceAccessiblityInfo isAccessableInfo;
  EUDAQ_DEBUG("trying to open camera with Serial Number " + std::string(deviceInfo.GetSerialNumber()));
  EUDAQ_DEBUG( "The current state of selected camera " + EnumToString(isAccessableInfo));
  if (!tlFactory.IsDeviceAccessible(deviceInfo, Pylon::Control, &isAccessableInfo)){
    //EUDAQ_DEBUG("trying to open camera with SN " + std::string(deviceInfo.GetSerialNumber()));
    //EUDAQ_DEBUG( "The current state of selected camera " + EnumToString(isAccessableInfo));
    EUDAQ_ERROR("Can't Connect to Camera");
  }
  EUDAQ_DEBUG("Creating Device");
  device = tlFactory.CreateDevice(deviceInfo);

  EUDAQ_DEBUG("Created Device (No errors occurred)");

  if (!device){
    EUDAQ_ERROR("Can't Connect to Camera");
  }else{
    EUDAQ_DEBUG("Creating Camera from device");
    if (camera && camera->IsPylonDeviceAttached()){
      camera->DestroyDevice();
    }
    camera = new Pylon::CBaslerUniversalInstantCamera(device);
    initialized = true;
    EUDAQ_DEBUG("Successfully Created Camera from device");
  }
  if(!initialized){
    EUDAQ_THROW("Camera could not be initialized");
  }
  EUDAQ_DEBUG("Finished PROD INIT");

}

void DummyProducer::DoConfigure(){
  EUDAQ_DEBUG("Starting PROD CONF");
  auto conf = GetConfiguration();

  imagegrabtimeout = conf->Get("imagegrabtimeout", 10000);
  imagestograb = conf->Get("imagestograb",1);
  framerate = conf->Get("framerate",0.25);
  max_buffer_frames = conf->Get("max_buffer_frames",100);
  max_images = conf->Get("max_images",10);
  filelocation = conf->Get("filelocation", "./");
  frameratebool = conf->Get("frameratebool",false);
  network_capacity = conf->Get("network_capacity", 50.);
  safety_factor = conf->Get("safety_factor", 1.1);
  exposure_time_us = conf->Get("exposure_time_us", 300000);
  packet_size = conf->Get("packet_size",1500);
  inter_packet_delay = conf->Get("inter_packet_delay",0);
  pixel_n_x = conf->Get("pixel_n_x",1920);
  pixel_n_y = conf->Get("pixel_n_y",1080);
  pixel_x_offset = conf->Get("pixel_x_offset",468);
  pixel_y_offset = conf->Get("pixel_y_offset",108);
  centerimage = conf->Get("centerimage",true);
  gain_raw = conf->Get("gain_raw",0);
  trigger_delay = conf->Get("trigger_delay",0.);
  pixel_signal_bkg_vertical_border= conf->Get("pixel_signal_bkg_vertical_border",300);
  pixel_signal_bkg_horizontal_border = conf->Get("pixel_signal_bkg_horizontal_border",300);
  binning = conf->Get("binning",false);
  binning_x = conf->Get("binning_x",1);
  binning_y = conf->Get("binning_y",1);

  //ADD CAMERA SETTINGS

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

  if(enable_cam_emu){
    EUDAQ_DEBUG("Selecting Test Image");
    camera->TestImageSelector.SetValue(Basler_UniversalCameraParams::TestImageSelectorEnums::TestImageSelector_Testimage2);
    EUDAQ_DEBUG("Test Image Succesfully Selected");
  }else{
    try{
    if (!camera->EventSelector.IsWritable())
	    {
	      throw RUNTIME_EXCEPTION( "The device doesn't support events." );
	    }

	  if (!camera->ChunkModeActive.TrySetValue( true ))
	    {
	      throw RUNTIME_EXCEPTION( "The camera doesn't support chunk features" );
	    }
	  // use some objects from only the first camera
	  camera->ChunkSelector.SetValue( Basler_UniversalCameraParams::ChunkSelector_Triggerinputcounter );
	  camera->ChunkEnable.SetValue(true);
	  camera->RegisterCameraEventHandler( pHandler1, "FrameStartEventData", FrameTriggerEvent, Pylon::RegistrationMode_ReplaceAll, Pylon::Cleanup_None );
	  camera->RegisterCameraEventHandler( pHandler1, "FrameStartOvertriggerEventData", TriggerOverrunEvent, Pylon::RegistrationMode_Append, Pylon::Cleanup_None );
	  camera->RegisterCameraEventHandler( pHandler1, "ExposureEndEventData", ExposureEndEvent, Pylon::RegistrationMode_Append, Pylon::Cleanup_None );
	  camera->EventSelector.SetValue( Basler_UniversalCameraParams::EventSelector_FrameStart );
	  camera->EventNotification.SetValue( Basler_UniversalCameraParams::EventNotification_On );
	  camera->EventSelector.SetValue( Basler_UniversalCameraParams::EventSelector_FrameStartOvertrigger );
	  camera->EventNotification.SetValue( Basler_UniversalCameraParams::EventNotification_On );
	  //camera.EventSelector.SetValue(EventSelector_ExposureEnd);
	  //camera.EventNotification.SetValue(EventNotification_On);
	  camera->CounterSelector.SetValue( Basler_UniversalCameraParams::CounterSelector_Counter1 );
	  camera->CounterResetSource.SetValue( Basler_UniversalCameraParams::CounterResetSource_Software );

	  camera->MaxNumBuffer = max_buffer_frames; std::cout << std::endl << "MaxNumBuffer " << camera->MaxNumBuffer.GetValue() << " Set for camera " << std::endl;
	  camera->Width.SetValue(pixel_n_x);
	  camera->Height.SetValue(pixel_n_y); std::cout << "width (pixels): " << camera->Width.GetValue() << " and height: " << camera->Height.GetValue() << " for camera " << std::endl;
	  if(!centerimage){
	    camera->OffsetX.SetValue(pixel_x_offset);
	    camera->OffsetY.SetValue(pixel_y_offset);
	    std::cout << "offsetX (pixels): " << camera->OffsetX.GetValue() << " and OffsetY: " << camera->OffsetY.GetValue() << " set for camera " << std::endl;
	  }
	  else{
	    camera->CenterX.SetValue(true);
	    camera->CenterY.SetValue(true);
        std::cout << "image centered in sensor, offsetX (pixels): " << camera->OffsetX.GetValue() << " and OffsetY: " << camera->OffsetY.GetValue() << " set " << std::endl;
      }

	  if(binning){
	    camera->BinningVertical.SetValue(binning_y);
	    camera->BinningHorizontal.SetValue(binning_x);
	  }
	  camera->PixelFormat.SetValue(pixel_format);
	  if (camera->PixelFormat.GetValue()==41){ std::cout << "Pixel Format Set with Mono 12 Packed for camera " << std::endl;}
	  else if (camera->PixelFormat.GetValue()==40){std::cout << "Pixel Format Set with Mono 12 for camera " << std::endl;}
	  else if (camera->PixelFormat.GetValue()==44){std::cout << "Pixel Format Set with Mono 8 for camera " << std::endl;}
	  else {std::cout << "Pixel Format not recognised :/ for camera " << std::endl;}

	  camera->GainRaw.SetValue(gain_raw); std::cout << "Gain Set with " <<  camera->GainRaw.GetValue() << " for camera " << std::endl;
	  camera->ExposureTimeAbs.SetValue(exposure_time_us); std::cout << "Exposure time Set at " << camera->ExposureTimeAbs.GetValue() << "microseconds for camera " << std::endl;
	  camera->AcquisitionFrameRateEnable.SetValue(frameratebool);
	  camera->TriggerMode.SetValue(trigger_onoff);
	  if(trigger_onoff){std::cout << "Trigger On" << std::endl;}
	  camera->TriggerSelector.SetValue( Basler_UniversalCameraParams::TriggerSelector_FrameStart );
	  std::cout << "Trigger Selector: " <<	camera->TriggerSelector.GetValue() << std::endl;
	  camera->TriggerSource.SetValue(trigger_source);
	  std::cout << "Trigger Source: " << camera->TriggerSource.GetValue() << std::endl;


	  int64_t payloadSize = camera->PayloadSize.GetValue();

	  camera->GevSCPSPacketSize.SetValue(packet_size);

	  std::cout << "Optimising GigE data streaming for " << payloadSize/1000000. << "MB images; Packet Size is " << packet_size << " bits and Inter-Packet Delay is " << inter_packet_delay << " camera-ticks for camera " << std::endl;

	  std::cout << "Camera tick frequency: " << camera->GevTimestampTickFrequency.GetValue() << " Hz for camera " << std::endl;

	  camera->GevStreamChannelSelector.SetValue( Basler_UniversalCameraParams::GevStreamChannelSelector_StreamChannel0 );

	  //camera.GevSCFTD.SetValue(0);
	  //camera.GevStreamChannelSelector.SetValue(GevStreamChannelSelector_StreamChannel0);
	  camera->GevSCPD.SetValue(0);
    }catch(const std::exception e){
        EUDAQ_THROW(e.what());
      }catch(const GenICam_3_1_Basler_pylon::GenericException& e){
        EUDAQ_THROW(e.GetDescription());
      }
  }

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
