#include "eudaq/RunControl.hh"

class DummyRunControl: public eudaq::RunControl{
public:
  DummyRunControl(const std::string & listenaddress);
  void Configure() override;
  void StartRun() override;
  void StopRun() override;
  void Exec() override;
  static const uint32_t m_id_factory = eudaq::cstr2hash("DummyRunControl");

private:
  bool m_flag_running;
  std::chrono::steady_clock::time_point m_tp_start_run;
};

namespace{
  auto dummy0 = eudaq::Factory<eudaq::RunControl>::
    Register<DummyRunControl, const std::string&>(DummyRunControl::m_id_factory);
}

DummyRunControl::DummyRunControl(const std::string & listenaddress)
  :RunControl(listenaddress){
  m_flag_running = false;
}

void DummyRunControl::StartRun(){
  RunControl::StartRun();
  m_tp_start_run = std::chrono::steady_clock::now();
  m_flag_running = true;
}

void DummyRunControl::StopRun(){
  RunControl::StopRun();
  m_flag_running = false;
}

void DummyRunControl::Configure(){
  auto conf = GetConfiguration();
  RunControl::Configure();
}

void DummyRunControl::Exec(){
  StartRunControl();
  while(IsActiveRunControl()){
    if(m_flag_running){
      auto tp_now = std::chrono::steady_clock::now();
      std::chrono::nanoseconds du_ts(tp_now - m_tp_start_run);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
}
