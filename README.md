Kurulum aşamasının en başına, projeyi inceleyecek kişilerin (hoca veya asistanın) hemen dikkatini çekecek şekilde o önemli dosya yolu uyarısını ekledim.

Aşağıdaki güncel metni kopyalayıp doğrudan `README.md` dosyanın içine yapıştırabilirsin:

```markdown
# Otonom Kütüphane Robotu

Bu proje, ROS tabanlı bir servis robotunun simüle edilmiş bir kütüphane ortamında (AWS RoboMaker Bookstore World) SLAM haritalaması, otonom navigasyon ve görev noktalarında QR kod doğrulaması yapmasını sağlayan entegre bir sistemdir.

## 🤖 Proje Özeti

TurtleBot3 kullanılarak geliştirilen bu çoklu görev (multi-task) servis robotu şunları yapabilmektedir:
1. SLAM (gmapping) kullanarak ortamın haritasını çıkarma
2. Kaydedilen harita üzerinde konumunu belirleme (AMCL)
3. Belirlenmiş görev noktalarına (waypoints) otonom olarak ulaşma (move_base)
4. Her bir noktaya ulaştığında ZBar ve OpenCV kullanarak QR kod okuma ve konum doğrulama
5. Başarısız QR okumalarında kamerayı farklı açılara çevirerek (kurtarma manevrası) otonom arama
6. Görevlerin tamamlanmasının ardından durum raporu oluşturma

## ⚙️ Sistem Mimarisi

- **Görev Yöneticisi Düğümü (Task Manager)**: Robotun hareketlerini kontrol eden, hedeflere gönderen ve görev akışını yöneten bir Durum Makinesi (State Machine).
- **QR Okuyucu Düğümü (QR Reader)**: Kamera verisini (`camera/rgb/image_raw`) dinleyen, OpenCV ve ZBar ile karelerdeki QR kodları çözüp sisteme bildiren düğüm.
- **Navigasyon Yığını (Navigation Stack)**: Otonom sürüş için AMCL (lokalizasyon) ve move_base (planlama) bileşenleri.
- **SLAM Yığını**: Ortam haritası çıkarmak için gmapping algoritması.

## 🛠️ Gereksinimler

### Sistem Gereksinimleri
- Ubuntu 20.04 LTS
- ROS Noetic
- Gazebo 11+

### Gerekli ROS ve Sistem Paketleri
Çalışma ortamını hazırlamak için aşağıdaki bağımlılıkları yükleyin:
```bash
sudo apt-get update
sudo apt-get install ros-noetic-turtlebot3
sudo apt-get install ros-noetic-turtlebot3-gazebo
sudo apt-get install ros-noetic-turtlebot3-navigation
sudo apt-get install ros-noetic-gmapping
sudo apt-get install ros-noetic-amcl
sudo apt-get install ros-noetic-move-base
sudo apt-get install ros-noetic-map-server
sudo apt-get install ros-noetic-teleop-twist-keyboard
sudo apt-get install ros-noetic-cv-bridge
sudo apt-get install libyaml-cpp-dev
sudo apt-get install libopencv-dev
sudo apt-get install libzbar-dev

```

*(Not: `libzbar-dev` paketi QR Reader düğümünün derlenebilmesi için zorunludur.)*

## 🚀 Kurulum

> ⚠️ **DEĞERLENDİRİCİLER İÇİN ÖNEMLİ NOT (Dosya Yolları):** > Bu projede harita ve yapılandırma dosyalarının yolları, geliştirmeyi yapan orijinal bilgisayarın yerel çalışma alanına (`/home/berk/ders_ws/...`) göre ayarlanmıştır.
> * Projeyi indirip **kendi sisteminizde çalıştırarak test edecekseniz**, `map.yaml`, `task_manager.py` ve ilgili `.launch` dosyalarındaki dizinleri kendi çalışma alanınıza (örneğin `~/catkin_ws/...`) göre düzenlemeniz gerekmektedir. Aksi takdirde sistem dosyaları bulamayacağı için hata verecektir.
> * Eğer projeyi yerelde çalıştırmadan, yalnızca **kodları ve demo videosunu inceleyerek değerlendirme yapacaksanız** dosya yollarında hiçbir değişiklik yapmanıza gerek yoktur.
> 
> 

1. Depoyu catkin çalışma alanınıza klonlayın:

```bash
cd ~/catkin_ws/src
git clone [https://github.com/berkk-05/bookstore_robot_nav.git](https://github.com/berkk-05/bookstore_robot_nav.git) bookstore_robot_nav

```

2. AWS Bookstore modelleri için Gazebo model yolunu dışa aktarın:

```bash
export GAZEBO_MODEL_PATH=$GAZEBO_MODEL_PATH:~/catkin_ws/src/bookstore_robot_nav/models

```

3. Çalışma alanını derleyin:

```bash
cd ~/catkin_ws
catkin_make
source devel/setup.bash

```

4. TurtleBot3 modelini belirtin:

```bash
export TURTLEBOT3_MODEL=waffle_pi

```

## 🎮 Kullanım

### Adım 1: Gazebo Simülasyonunu Başlatma

```bash
roslaunch bookstore_robot_nav simulation.launch

```

### Adım 2: Harita Oluşturma (İlk Kurulum İçin)

Yeni bir terminalde SLAM uygulamasını başlatın:

```bash
roslaunch bookstore_robot_nav slam.launch

```

Başka bir terminalde teleop aracını başlatın ve robotu ortamda gezdirerek haritayı çıkarın:

```bash
rosrun turtlebot3_teleop turtlebot3_teleop_key

```

Haritalama bittiğinde haritayı kaydedin:

```bash
rosrun map_server map_saver -f ~/catkin_ws/src/bookstore_robot_nav/maps/map

```

### Adım 3: Navigasyonu Başlatma

```bash
roslaunch bookstore_robot_nav navigation.launch

```

RViz açıldığında, üst menüden **"2D Pose Estimate"** aracını seçin ve robotun haritadaki ilk konumunu işaretleyerek lokalizasyonunu sağlayın.

### Adım 4: Görev Yöneticisi ve QR Okuyucuyu Çalıştırma

```bash
roslaunch bookstore_robot_nav task_manager.launch

```

Bu adımdan sonra robot, `config/mission.yaml` dosyasındaki koordinatları sırasıyla gezecek ve belirlenen noktalarda QR doğrulamasını yapacaktır.

## 📂 Görev Yapılandırması (`mission.yaml`)

Robotun gideceği hedefler ve bu hedeflerde beklenen QR kod verileri `config/mission.yaml` dosyası üzerinden yapılandırılır:

```yaml
locations:
  - DANISMA
  - TEKNOLOJI
  - ROMAN
  - OKUMA
  - YENI_CIKANLAR

DANISMA:
  goal: {x: -0.70679, y: 5.0264, yaw: 0.160657}
  qr_expected: "LOC=DANISMA_MASASI"

TEKNOLOJI:
  goal: {x: -0.74578, y: -1.5736, yaw: -1.260829}
  qr_expected: "LOC=TEKNOLOJI_STANDI"

ROMAN:
  goal: {x: 6.6371, y: 0.53398, yaw: 0.012323}
  qr_expected: "LOC=ROMAN_BOLUMU"

OKUMA:
  goal: {x: -5.2959, y: -3.7372, yaw: -2.985271}
  qr_expected: "LOC=OKUMA_MASASI"

YENI_CIKANLAR:
  goal: {x: 0.51707, y: -5.8475, yaw: -1.633661}
  qr_expected: "LOC=YENI_CIKANLAR"

```

## 🧠 Görev Durum Makinesi ve Kurtarma Senaryoları

Görev Yöneticisi (`task_manager.cpp`) şu sırayla çalışır:

1. **INIT**: Sistem başlatılır ve `mission.yaml` okunur.
2. **GO_TO_LOCATION**: Sıradaki hedefe gidilir. Navigasyon için 1 tekrar deneme hakkı vardır, başarısız olursa hedefe **FAIL** durumu atanır ve zaman aşımı süresi 90 saniyedir.
3. **QR_VERIFY**: Hedefe ulaşıldığında, kameranın QR kodunu algılaması beklenir. İlk denemede okuyamazsa **2 kez kurtarma manevrası** (sağa/sola dönme) yapar ve bekler. Maksimum denemeden sonra okuma yapılamazsa konum **SKIPPED** olarak işaretlenir.
4. **REPORT**: Noktanın durumu `task_status` topiği üzerinden yayınlanır.
5. **NEXT_LOCATION**: Bir sonraki noktaya geçilir.
6. **FINISH**: Görev sonlandırılır ve sonuç ekranı ile `mission_report.txt` çıktıları oluşturulur.

## 📄 Çıktı Raporu

Görev tamamlandığında robot bir görev özeti çıkarır ve bu dosya `~/catkin_ws/src/bookstore_robot_nav/mission_report.txt` adresine kaydedilir:

```text
=== MISSION REPORT ===
  DANISMA: SUCCESS
  TEKNOLOJI: SKIPPED
  ROMAN: SUCCESS
  OKUMA: SUCCESS
  YENI_CIKANLAR: SUCCESS

Summary:
  SUCCESS: 4
  SKIPPED: 1
  FAIL: 0
=====================

```

```

```
