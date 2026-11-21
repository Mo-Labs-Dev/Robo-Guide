#include <chrono>
#include <cmath>
#include <memory>
#include <regex>
#include <string>
#include <stdexcept>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
#include "sensor_msgs/msg/joint_state.hpp"
using namespace std::chrono_literals;
using json=nlohmann::json;
class Driver : public rclcpp::Node {
  enum class Mode { Off, Sim, Hardware };
  Mode mode_=Mode::Off;
  std::string ip_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr status_;
  rclcpp::Publisher<sensor_msgs::msg::JointState>::SharedPtr joints_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr commands_;
  rclcpp::TimerBase::SharedPtr timer_;
  static size_t writer(char *p,size_t s,size_t n,void *u) {
    static_cast<std::string*>(u)->append(p,s*n);return s*n;
  }
  static bool validIp(const std::string &ip) {
    static const std::regex r(R"(^([0-9]{1,3}\.){3}[0-9]{1,3}$)");
    if(!std::regex_match(ip,r))return false;
    size_t start=0;
    for(int i=0;i<4;++i) {
      auto end=ip.find('.',start);
      auto part=ip.substr(start,end==std::string::npos?end:end-start);
      int x=std::stoi(part);
      if(x>255 || (i==0&&(x==0||x>=224)) || (i==3&&(x==0||x==255)))return false;
      start=end+1;
    }
    return true;
  }
  void publishStatus(const std::string &v) {
    std_msgs::msg::String m;m.data=v;status_->publish(m);
  }
  void publishJoints(double b,double s,double e,double t) {
    sensor_msgs::msg::JointState m;m.header.stamp=now();
    m.name={"base","shoulder","elbow","hand"};m.position={b,s,e,t};joints_->publish(m);
  }
  bool feedback() {
    CURL *c=curl_easy_init();if(!c)return false;
    std::string data;
    std::string url="http://"+ip_+"/js?json=%7B%22T%22%3A105%7D";
    curl_easy_setopt(c,CURLOPT_URL,url.c_str());
    curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,writer);
    curl_easy_setopt(c,CURLOPT_WRITEDATA,&data);
    curl_easy_setopt(c,CURLOPT_CONNECTTIMEOUT_MS,1000L);
    curl_easy_setopt(c,CURLOPT_TIMEOUT_MS,1800L);
    curl_easy_setopt(c,CURLOPT_NOSIGNAL,1L);
    curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,0L);
    curl_easy_setopt(c,CURLOPT_NOPROXY,"*");
    auto result=curl_easy_perform(c);
    long http=0;curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&http);
    curl_easy_cleanup(c);
    if(result!=CURLE_OK||http!=200)return false;
    try {
      auto j=json::parse(data);
      if(j.at("T").get<int>()!=1051)return false;
      double b=j.at("b").get<double>(),s=j.at("s").get<double>();
      double e=j.at("e").get<double>(),t=j.at("t").get<double>();
      if(!std::isfinite(b)||!std::isfinite(s)||!std::isfinite(e)||!std::isfinite(t))return false;
      publishJoints(b,s,e,t);return true;
    }catch(const std::exception&){return false;}
  }
  void command(const std::string &raw) {
    try {
      auto j=json::parse(raw);auto action=j.at("action").get<std::string>();
      if(action=="disconnect"){mode_=Mode::Off;ip_.clear();publishStatus("DISCONNECTED | Monitoring stopped");}
      else if(action=="simulation"){mode_=Mode::Sim;ip_.clear();publishStatus("SIMULATION | READY");}
      else if(action=="hardware"){
        auto ip=j.at("ip").get<std::string>();
        if(!validIp(ip)){publishStatus("ERROR | Invalid IPv4 address");return;}
        ip_=ip;mode_=Mode::Hardware;publishStatus("HARDWARE | Connecting");
      }else publishStatus("ERROR | Unsupported request");
    }catch(const std::exception&){publishStatus("ERROR | Invalid request");}
  }
  void tick(){
    if(mode_==Mode::Off){publishStatus("DISCONNECTED | Monitoring stopped");return;}
    if(mode_==Mode::Sim){publishJoints(0,0,1.57,1.57);publishStatus("SIMULATION | READY");return;}
    publishStatus(feedback()?"HARDWARE | CONNECTED":"HARDWARE | FEEDBACK UNAVAILABLE");
  }
public:
  Driver():Node("waveshare_driver_node"){
    if(curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK)throw std::runtime_error("curl init failed");
    status_=create_publisher<std_msgs::msg::String>("/robotest/robot/status",10);
    joints_=create_publisher<sensor_msgs::msg::JointState>("/robotest/robot/joint_states",10);
    commands_=create_subscription<std_msgs::msg::String>("/robotest/robot/connection_command",10,
      [this](std_msgs::msg::String::SharedPtr m){command(m->data);});
    timer_=create_wall_timer(1000ms,[this](){tick();});
    RCLCPP_INFO(get_logger(),"Read-only RoArm driver ready. No motion endpoints.");
  }
  ~Driver()override{timer_.reset();curl_global_cleanup();}
};
int main(int argc,char**argv){rclcpp::init(argc,argv);rclcpp::spin(std::make_shared<Driver>());rclcpp::shutdown();}
