#include <ros/ros.h>
#include <geometry_msgs/PoseWithCovarianceStamped.h>
#include <move_base_msgs/MoveBaseAction.h>
#include <geometry_msgs/Twist.h>
#include <actionlib/client/simple_action_client.h>
#include <std_msgs/String.h>
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <vector>
#include <map>
#include <iostream>
#include <sstream>

typedef actionlib::SimpleActionClient<move_base_msgs::MoveBaseAction> MoveBaseClient;

enum TaskState {
  INIT,
  GO_TO_LOCATION,
  QR_VERIFY,
  REPORT,
  NEXT_LOCATION,
  FINISH
};

struct Location {
  std::string name;
  float goal_x, goal_y, goal_yaw;
  std::string qr_expected;
  std::string status;
};

class TaskManager {
private:
  ros::NodeHandle nh_;
  MoveBaseClient* ac_;
  std::vector<Location> locations_;
  std::map<std::string, Location> location_map_;
  int current_location_idx_;
  TaskState current_state_;
  std::string last_qr_data_;
  int qr_retry_count_;
  int move_retry_count_;
  const int QR_MAX_RETRIES = 3; // 1 ilk deneme + 2 YENİDEN deneme
  const int MOVE_MAX_RETRIES = 1;
  
  ros::Subscriber qr_subscriber_;
  ros::Publisher status_publisher_;
  ros::Publisher cmd_vel_pub_;

public:
  TaskManager() : current_location_idx_(0), current_state_(INIT), qr_retry_count_(0), move_retry_count_(0) {
    // Initialize action client
    ac_ = new MoveBaseClient("move_base", true);
    
    // move_base'in baslatilmasini bekle
    while (!ac_->waitForServer(ros::Duration(5.0))) {
      ROS_INFO("move_base sunucusunun baslamasi bekleniyor...");
    }
    
    // QR okuyucusuna abone ol
    qr_subscriber_ = nh_.subscribe("qr_data", 10, &TaskManager::qrCallback, this);
    
    // Durum güncellemeleri için yayıncı
    status_publisher_ = nh_.advertise<std_msgs::String>("task_status", 10);
    
    // Kurtarma manevraları için hız yayıncısı
    cmd_vel_pub_ = nh_.advertise<geometry_msgs::Twist>("cmd_vel", 10);
    
    // Görev yapılandırmasını yükle
    loadMissionConfig();
  }

  ~TaskManager() {
    delete ac_;
  }

  void loadMissionConfig() {
    std::string config_file;
    
    // Once parametre sunucusundan almaya calis
    if (!ros::param::get("/bookstore_robot_nav/config_file", config_file)) {
      // Ayarlanmazsa varsayilani kullan
      config_file = "/home/berk/ders_ws/src/bookstore_robot_nav/config/mission.yaml";
      ROS_WARN("Yapilandirma dosyasi parametresi bulunamadi, varsayilan kullaniliyor: %s", config_file.c_str());
    }

    try {
      YAML::Node config = YAML::LoadFile(config_file);
      
      // Load location names
      if (config["locations"]) {
        for (const auto& loc : config["locations"]) {
          Location location;
          location.name = loc.as<std::string>();
          
          if (config[location.name]) {
            location.goal_x = config[location.name]["goal"]["x"].as<float>();
            location.goal_y = config[location.name]["goal"]["y"].as<float>();
            location.goal_yaw = config[location.name]["goal"]["yaw"].as<float>();
            location.qr_expected = config[location.name]["qr_expected"].as<std::string>();
            location.status = "PENDING";
            
            locations_.push_back(location);
            location_map_[location.name] = location;
          }
        }
      }
      
      ROS_INFO("Gorev yapilandirmasindan %zu konum yuklendi", locations_.size());
    } catch (std::exception& e) {
      ROS_ERROR("Gorev yapilandirmasi yuklenemedi: %s", e.what());
    }
  }

  void qrCallback(const std_msgs::String::ConstPtr& msg) {
    // Sadece QR doğrulama aşamasındaysa veriyi kabul et ve ekrana yazdır
    if (current_state_ == QR_VERIFY) {
      if (last_qr_data_ != msg->data) {
        last_qr_data_ = msg->data;
        ROS_INFO("QR Verisi alindi: %s", last_qr_data_.c_str());
      }
    }
  }

  void run() {
    ros::Rate loop_rate(1); // 1 Hz

    while (ros::ok()) {
      switch (current_state_) {
        case INIT:
          handleInit();
          break;
        case GO_TO_LOCATION:
          handleGoToLocation();
          break;
        case QR_VERIFY:
          handleQRVerify();
          break;
        case REPORT:
          handleReport();
          break;
        case NEXT_LOCATION:
          handleNextLocation();
          break;
        case FINISH:
          handleFinish();
          return;
        default:
          break;
      }

      ros::spinOnce();
      loop_rate.sleep();
    }
  }

private:
  void handleInit() {
    ROS_INFO("TaskManager: BASLATMA durumu");
    publishStatus("BASLATMA: Sistem baslatildi. Navigasyona baslamaya hazir.");
    
    if (locations_.empty()) {
      ROS_ERROR("Hic konum yuklenmedi!");
      current_state_ = FINISH;
      return;
    }

    current_location_idx_ = 0;
    current_state_ = GO_TO_LOCATION;
  }

  void handleGoToLocation() {
    if (current_location_idx_ >= locations_.size()) {
      current_state_ = FINISH;
      return;
    }

    Location& current_loc = locations_[current_location_idx_];
    ROS_INFO("TaskManager: KONUMA_GIT - %s konumuna gidiliyor (%.2f, %.2f, %.2f)",
             current_loc.name.c_str(), current_loc.goal_x, current_loc.goal_y, current_loc.goal_yaw);

    move_base_msgs::MoveBaseGoal goal;
    goal.target_pose.header.frame_id = "map";
    goal.target_pose.header.stamp = ros::Time::now();
    goal.target_pose.pose.position.x = current_loc.goal_x;
    goal.target_pose.pose.position.y = current_loc.goal_y;
    goal.target_pose.pose.orientation.z = sin(current_loc.goal_yaw / 2.0);
    goal.target_pose.pose.orientation.w = cos(current_loc.goal_yaw / 2.0);

    ac_->sendGoal(goal);

    // Sonucu bekle (90 saniye zaman asimi)
    if (ac_->waitForResult(ros::Duration(90.0))) {
      if (ac_->getState() == actionlib::SimpleClientGoalState::SUCCEEDED) {
        ROS_INFO("Konuma ulasildi: %s", current_loc.name.c_str());
        move_retry_count_ = 0;
        current_state_ = QR_VERIFY;
      } else {
        ROS_WARN("Konuma ulasilamadi. Tekrar deneme sayisi: %d", move_retry_count_);
        move_retry_count_++;
        
        if (move_retry_count_ >= MOVE_MAX_RETRIES) {
          current_loc.status = "FAIL";
          ROS_ERROR("%s konumu icin maksimum tekrar deneme sayisina ulasildi", current_loc.name.c_str());
          current_state_ = REPORT;
          move_retry_count_ = 0;
        }
      }
    } else {
      ROS_WARN("Hedef zaman asimi. Tekrar deneme sayisi: %d", move_retry_count_);
      move_retry_count_++;
      
      if (move_retry_count_ >= MOVE_MAX_RETRIES) {
        current_loc.status = "FAIL";
        current_state_ = REPORT;
        move_retry_count_ = 0;
      }
    }
  }

  void handleQRVerify() {
    if (current_location_idx_ >= locations_.size()) {
      current_state_ = FINISH;
      return;
    }

    Location& current_loc = locations_[current_location_idx_];
    ROS_INFO("TaskManager: QR_DOGRULAMA - %s konumunda QR dogrulaniyor", current_loc.name.c_str());

    // QR verisi bekle (2 saniye zaman asimi)
    ros::Time start_time = ros::Time::now();
    ros::Duration timeout(2.0);
    
    last_qr_data_.clear(); // Her denemede eski veriyi temizle

    while (ros::Time::now() - start_time < timeout && ros::ok()) {
      ros::spinOnce();
      
      if (!last_qr_data_.empty()) {
        if (last_qr_data_ == current_loc.qr_expected) {
          ROS_INFO("%s konumunda QR dogrulama BASARILI", current_loc.name.c_str());
          current_loc.status = "SUCCESS";
          qr_retry_count_ = 0;
          last_qr_data_.clear();
          current_state_ = REPORT;
          return;
        } else {
          ROS_WARN("%s konumunda QR uyusmazligi. Beklenen: %s, Alinan: %s",
                   current_loc.name.c_str(), current_loc.qr_expected.c_str(), last_qr_data_.c_str());
          last_qr_data_.clear();
        }
      }
      
      ros::Duration(0.1).sleep();
    }

    qr_retry_count_++;
    ROS_WARN("QR dogrulanmadi. Tekrar deneme sayisi: %d", qr_retry_count_);
    
    // QR bulunamadıysa küçük bir kurtarma manevrası yap (Kamerayı çevir)
    if (qr_retry_count_ < QR_MAX_RETRIES) {
      geometry_msgs::Twist twist_cmd;
      twist_cmd.angular.z = (qr_retry_count_ % 2 == 1) ? 0.4 : -0.8; // Önce hafif sola, sonra daha fazla sağa dön
      ROS_INFO("QR araniyor... Aci degistiriliyor (Arama Manevrasi)");
      cmd_vel_pub_.publish(twist_cmd);
      ros::Duration(1.0).sleep(); // 1 saniye dön
      twist_cmd.angular.z = 0.0;
      cmd_vel_pub_.publish(twist_cmd); // Dur
      ros::Duration(0.5).sleep(); // Kameranin netlesmesi icin yarim saniye bekle
    }

    if (qr_retry_count_ >= QR_MAX_RETRIES) {
      current_loc.status = "SKIPPED";
      ROS_ERROR("Maksimum QR tekrar deneme sayisina ulasildi. SKIPPED olarak isaretleniyor.");
      qr_retry_count_ = 0;
      current_state_ = REPORT;
    }
  }

  void handleReport() {
    Location& current_loc = locations_[current_location_idx_]; // Mevcut konum
    ROS_INFO("TaskManager: RAPOR - Konum: %s, Durum: %s", 
             current_loc.name.c_str(), current_loc.status.c_str()); // Konum, Durum
    
    std::stringstream ss; // String akışı
    ss << "Konum: " << current_loc.name << " | Durum: " << current_loc.status; // Konum: ... | Durum: ...
    publishStatus(ss.str());

    current_state_ = NEXT_LOCATION;
  }

  void handleNextLocation() {
    current_location_idx_++;
    ROS_INFO("TaskManager: SONRAKI_KONUM - %d/%zu konumuna geciliyor", 
             current_location_idx_ + 1, locations_.size());

    if (current_location_idx_ >= locations_.size()) {
      current_state_ = FINISH;
    } else {
      current_state_ = GO_TO_LOCATION;
    }
  }

  void handleFinish() {
    ROS_INFO("TaskManager: FINISH - Mission complete!");
    
    // Print final report
    std::cout << "\n=== MISSION REPORT ===" << std::endl;
    int success_count = 0, skipped_count = 0, fail_count = 0;
    
    // Rapor dosyasini olustur
    std::string report_file = "/home/berk/ders_ws/src/bookstore_robot_nav/mission_report.txt";
    std::ofstream outfile(report_file);
    
    if (outfile.is_open()) {
      outfile << "=== MISSION REPORT ===\n";
    }

    for (const auto& loc : locations_) {
      std::cout << "  " << loc.name << ": " << loc.status << std::endl;
      if (outfile.is_open()) {
        outfile << "  " << loc.name << ": " << loc.status << "\n";
      }
      if (loc.status == "SUCCESS") success_count++;
      else if (loc.status == "SKIPPED") skipped_count++;
      else if (loc.status == "FAIL") fail_count++;
    }
    
    std::cout << "\nSummary:" << std::endl;
    std::cout << "  SUCCESS: " << success_count << std::endl;
    std::cout << "  SKIPPED: " << skipped_count << std::endl;
    std::cout << "  FAIL: " << fail_count << std::endl;
    std::cout << "=====================" << std::endl;

    if (outfile.is_open()) {
      outfile << "\nSummary:\n";
      outfile << "  SUCCESS: " << success_count << "\n";
      outfile << "  SKIPPED: " << skipped_count << "\n";
      outfile << "  FAIL: " << fail_count << "\n";
      outfile << "=====================\n";
      outfile.close();
      ROS_INFO("Gorev raporu TXT olarak kaydedildi: %s", report_file.c_str());
    } else {
      ROS_ERROR("Rapor dosyasi olusturulamadi: %s", report_file.c_str());
    }

    publishStatus("MISSION FINISHED");
  }

  void publishStatus(const std::string& status) {
    std_msgs::String msg;
    msg.data = status;
    status_publisher_.publish(msg);
  }
};

int main(int argc, char** argv) {
  ros::init(argc, argv, "task_manager");
  
  TaskManager task_manager;
  task_manager.run();

  return 0;
}
