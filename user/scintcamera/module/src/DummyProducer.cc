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

  //Ini vars
  bool enable_cam_emu;
  std::string cam_serial_number;

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

  //
  OurCameraEventHandler* pHandler1;
  Pylon::CBaslerUniversalInstantCamera* camera;
  //Pylon::IGigETransportLayer* pTl;
  Pylon::IPylonDevice* device;

  //Other
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

    camera->TriggerSelector.SetValue(trigger_start_type);
    // Enable triggered image acquisition for the Frame Start trigger
    camera->TriggerMode.SetValue(trigger_onoff);
    // Set the trigger source for the Frame Start trigger to Software
    camera->TriggerSource.SetValue(trigger_source);

    camera->StartGrabbing(Pylon::EGrabStrategy::GrabStrategy_OneByOne, Pylon::EGrabLoop::GrabLoop_ProvidedByUser);

    Pylon::CBaslerUniversalGrabResultPtr ptrGrabResult;

    while(m_running){
      if(camera->WaitForFrameTriggerReady(10000, Pylon::TimeoutHandling_ThrowException )){
        EUDAQ_INFO("Camera ready and waiting");
        // Generate a software trigger signal
        camera->TriggerSoftware.Execute();
      }
      camera->RetrieveResult( imagegrabtimeout,  ptrGrabResult);   // try to get one of the camera image results
      if (ptrGrabResult->GrabSucceeded()){
        EUDAQ_INFO("Succesfully grabbed image");
        std::string filename =  filelocation + "image_" + std::to_string(images_taken) + ".tiff";
        Pylon::CImagePersistence::Save( Pylon::ImageFileFormat_Tiff, filename.c_str(), ptrGrabResult );     // save the image taken, in the previsouly defined path, with parameters as above
        images_taken++;
      }else{
        EUDAQ_ERROR("Unable to capture image: Error Code "+ std::to_string(ptrGrabResult->GetErrorCode()) + "\nError Description: " + ptrGrabResult->GetErrorDescription().c_str() + "\n\nThe softare will try again in a few seconds\n");
      }
      ptrGrabResult.Release();
      if(images_taken == max_images){
        EUDAQ_INFO("Maximum number of images taken ("+std::to_string(max_images)+"), the producer won't do anything anymore until you restart.");
        m_running=false;
      }
    }
  }catch(const GenICam_3_1_Basler_pylon::GenericException& e){
    EUDAQ_THROW("An exception occurred.\n" + (std::string)e.GetDescription());

  }catch(const std::exception e){
    EUDAQ_THROW("An exception occurred: " + (std::string)e.what());
  }

}

void DummyProducer::DoInitialise(){
  EUDAQ_DEBUG("Starting PROD INIT");

  // Get values from ini file
  auto ini = GetInitConfiguration();
  enable_cam_emu = ini->Get<bool>("ENABLE_CAM_EMU",true);
  cam_serial_number = ini->Get("CAM_SERIAL_NUMBER", "24301947");

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

  // Get the transport layer factory.
  Pylon::CTlFactory& tlFactory = Pylon::CTlFactory::GetInstance();

  uint64_t n_cameras = tlFactory.EnumerateDevices(devices);

  if (n_cameras == 0){
    EUDAQ_DEBUG("No camera present.");
    Pylon::PylonTerminate();
    EUDAQ_THROW("No camera present.");
  }

  EUDAQ_DEBUG("Number of Cameras: "+std::to_string(n_cameras));

  Pylon::CDeviceInfo deviceInfo;
  if(!enable_cam_emu){
    bool device_found = false;
    for(uint64_t i=0; i < n_cameras; i++){

      if(devices[i].GetSerialNumber().c_str() == cam_serial_number){
        deviceInfo.SetSerialNumber(devices[i].GetSerialNumber());
        device_found = true;
        EUDAQ_DEBUG("Found camera with serial number: "+cam_serial_number);
      }
    }
    if(!device_found){
      EUDAQ_THROW("Could not find camera with serial number: "+cam_serial_number);
    }
  }else{
    deviceInfo.SetSerialNumber(devices[0].GetSerialNumber());
  }

  initialized = false;

  // This checks, among other things, that the camera is not already in use.
  // Without that check, the following CreateDevice() may crash on duplicate
  // serial number. Unfortunately, this call is slow.
  Pylon::EDeviceAccessiblityInfo isAccessableInfo;
  auto IsDeviceAccessible = tlFactory.IsDeviceAccessible(deviceInfo, Pylon::Control, &isAccessableInfo);
  EUDAQ_DEBUG("Trying to open camera with Serial Number " + std::string(deviceInfo.GetSerialNumber()));
  EUDAQ_INFO( "Accessiblilty Status: " + EnumToString(isAccessableInfo));
  if (!IsDeviceAccessible){
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

  // A lot of settings copied from John's code
  // Can be found in ../../misc/dummy.conf
  imagegrabtimeout = conf->Get("imagegrabtimeout", 10000);
  imagestograb = conf->Get("imagestograb",1);
  framerate = conf->Get("framerate",0.25);
  max_buffer_frames = conf->Get("max_buffer_frames",100);
  max_images = conf->Get("max_images",10);
  filelocation = conf->Get("filelocation", "./");
  frameratebool = conf->Get<bool>("frameratebool",false);
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
    // Setting up the camera (pasted from John's code, edited to integrate it with EUDAQ)
    if (!camera->EventSelector.IsWritable())
	    {
	      throw RUNTIME_EXCEPTION( "The device doesn't support events." );
	    }

	  if (!camera->ChunkModeActive.TrySetValue( true ))
	    {
	      throw RUNTIME_EXCEPTION( "The camera doesn't support chunk features" );
	    }
      std::string current_cam_sn = camera->GetDeviceInfo().GetSerialNumber().c_str();
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

	  camera->MaxNumBuffer = max_buffer_frames; EUDAQ_INFO("MaxNumBuffer " + std::to_string(camera->MaxNumBuffer.GetValue()) +" Set for camera "+ current_cam_sn); //std::cout << std::endl << "MaxNumBuffer " << camera->MaxNumBuffer.GetValue() << " Set for camera " << std::endl;
	  camera->Width.SetValue(pixel_n_x);
	  camera->Height.SetValue(pixel_n_y); EUDAQ_INFO("width (pixels): " + std::to_string(camera->Width.GetValue()) + " and height: " + std::to_string(camera->Height.GetValue()) +" for camera "+ current_cam_sn); //std::cout << "width (pixels): " << camera->Width.GetValue() << " and height: " << camera->Height.GetValue() << " for camera " << std::endl;
	  if(!centerimage){
	    camera->OffsetX.SetValue(pixel_x_offset);
	    camera->OffsetY.SetValue(pixel_y_offset);
	    EUDAQ_INFO("offsetX (pixels): " + std::to_string(camera->OffsetX.GetValue()) + " and OffsetY: " + std::to_string(camera->OffsetY.GetValue()) + " set for camera " + current_cam_sn); //std::cout << "offsetX (pixels): " << camera->OffsetX.GetValue() << " and OffsetY: " << camera->OffsetY.GetValue() << " set for camera " << std::endl;
	  }
	  else{
	    camera->CenterX.SetValue(true);
	    camera->CenterY.SetValue(true);
        EUDAQ_INFO("image centered in sensor, offsetX (pixels): " + std::to_string(camera->OffsetX.GetValue()) + " and OffsetY: " + std::to_string(camera->OffsetY.GetValue()) + " set for camera " + current_cam_sn); //std::cout << "image centered in sensor, offsetX (pixels): " << camera->OffsetX.GetValue() << " and OffsetY: " << camera->OffsetY.GetValue() << " set " << std::endl;
      }

	  if(binning){
	    camera->BinningVertical.SetValue(binning_y);
	    camera->BinningHorizontal.SetValue(binning_x);
	  }
	  camera->PixelFormat.SetValue(pixel_format);
	  if (camera->PixelFormat.GetValue()==41){ EUDAQ_INFO("Pixel Format Set with Mono 12 Packed for camera "+ current_cam_sn);}
	  else if (camera->PixelFormat.GetValue()==40){EUDAQ_INFO("Pixel Format Set with Mono 12 for camera "+ current_cam_sn);}
	  else if (camera->PixelFormat.GetValue()==44){EUDAQ_INFO("Pixel Format Set with Mono 8 for camera " + current_cam_sn);}
	  else {EUDAQ_INFO("Pixel Format not recognised :/ for camera "+current_cam_sn);}

	  camera->GainRaw.SetValue(gain_raw); EUDAQ_INFO("Gain Set with " +  std::to_string(camera->GainRaw.GetValue()) + " for camera "+ current_cam_sn);//std::cout << "Gain Set with " <<  camera->GainRaw.GetValue() << " for camera " << std::endl;
	  camera->ExposureTimeAbs.SetValue(exposure_time_us);EUDAQ_INFO("Exposure time Set at " + std::to_string(camera->ExposureTimeAbs.GetValue()) + "microseconds for camera " + current_cam_sn); //std::cout << "Exposure time Set at " << camera->ExposureTimeAbs.GetValue() << "microseconds for camera " << std::endl;
	  camera->AcquisitionFrameRateEnable.SetValue(frameratebool);
	  camera->TriggerMode.SetValue(trigger_onoff);
	  if(trigger_onoff){EUDAQ_INFO("Trigger On for camera "+current_cam_sn);}
	  camera->TriggerSelector.SetValue( Basler_UniversalCameraParams::TriggerSelector_FrameStart );
	  EUDAQ_INFO("Trigger Selector: " +	std::to_string(camera->TriggerSelector.GetValue()) + "for camera "+current_cam_sn);//std::cout << "Trigger Selector: " <<	camera->TriggerSelector.GetValue() << std::endl;
	  camera->TriggerSource.SetValue(trigger_source);
	  EUDAQ_INFO("Trigger Source: " + std::to_string(camera->TriggerSource.GetValue())+ "for camera "+ current_cam_sn);//std::cout << "Trigger Source: " << camera->TriggerSource.GetValue() << std::endl;

	  //int64_t payloadSize = camera->PayloadSize.GetValue();

	  camera->GevSCPSPacketSize.SetValue(packet_size);

	  //std::cout << "Optimising GigE data streaming for " << payloadSize/1000000. << "MB images; Packet Size is " << packet_size << " bits and Inter-Packet Delay is " << inter_packet_delay << " camera-ticks for camera " << std::endl;

	  EUDAQ_INFO("Camera tick frequency: " + std::to_string(camera->GevTimestampTickFrequency.GetValue()) + " Hz for camera "+current_cam_sn);//std::cout << "Camera tick frequency: " << camera->GevTimestampTickFrequency.GetValue() << " Hz for camera " << std::endl;

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
