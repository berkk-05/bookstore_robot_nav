#include <ros/ros.h>
#include <std_msgs/String.h>
#include <sensor_msgs/Image.h>
#include <cv_bridge/cv_bridge.h>
#include <opencv2/opencv.hpp>
#include <zbar.h>
#include <string>

class QRReader {
private:
  ros::NodeHandle nh_;
  ros::Publisher qr_publisher_;
  ros::Subscriber image_subscriber_;
  bool camera_ready_;

public:
  QRReader() : camera_ready_(false) {
    // Publisher for QR data
    qr_publisher_ = nh_.advertise<std_msgs::String>("qr_data", 10);
    
    // Subscriber for camera images
    image_subscriber_ = nh_.subscribe("camera/rgb/image_raw", 10, &QRReader::imageCallback, this);
    
    ROS_INFO("QRReader dugumu baslatildi. Kamera akisi bekleniyor...");
  }

  void imageCallback(const sensor_msgs::ImageConstPtr& msg) {
    try {
      // Convert ROS image message to OpenCV Mat
      cv_bridge::CvImagePtr cv_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::BGR8);
      
      // Try to detect and decode QR codes
      std::string qr_data = detectQRCode(cv_ptr->image);
      
      if (!qr_data.empty()) {
        publishQRData(qr_data);
      }
    } catch (cv_bridge::Exception& e) {
      ROS_ERROR("cv_bridge istisnasi: %s", e.what());
    }
  }

  std::string detectQRCode(const cv::Mat& image) {
    if (image.empty()) {
      return "";
    }
    
    try {
      // Goruntuyu gri tonlamaya cevir (ZBar bu formati bekler)
      cv::Mat gray;
      cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);

      // ZBar tarayicisini olustur
      zbar::ImageScanner scanner;
      scanner.set_config(zbar::ZBAR_NONE, zbar::ZBAR_CFG_ENABLE, 0); // Diger formatlari (Barkod vs) kapat
      scanner.set_config(zbar::ZBAR_QRCODE, zbar::ZBAR_CFG_ENABLE, 1); // Sadece QR kod algilamayi ac

      // OpenCV Mat verisini ZBar Image formatina donustur
      zbar::Image zbar_img(image.cols, image.rows, "Y800", (uchar *)gray.data, image.cols * image.rows);
      
      // Goruntuyu tara
      scanner.scan(zbar_img);
      
      // Bulunan QR kodlari icinde don (ilk buldugunu dondurur)
      for (zbar::Image::SymbolIterator symbol = zbar_img.symbol_begin(); symbol != zbar_img.symbol_end(); ++symbol) {
        std::string decoded_text = symbol->get_data();
        return decoded_text;
      }
    } catch (const std::exception& e) {
      ROS_DEBUG("QR algilama hatasi: %s", e.what());
    }
    
    return "";
  }

  void publishQRData(const std::string& data) {
    std_msgs::String msg;
    msg.data = data;
    qr_publisher_.publish(msg);
  }

  void spin() {
    ros::spin();
  }
};

int main(int argc, char** argv) {
  ros::init(argc, argv, "qr_reader");
  
  QRReader qr_reader;
  qr_reader.spin();

  return 0;
}
