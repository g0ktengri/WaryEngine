# Wary Engine - Teknik Dokümantasyon

## 1. Genel Mimari (Architecture)
Wary Engine, C++ ile yazılmış, **SDL2** (pencere ve giriş yönetimi) ve **OpenGL 3.3** (grafik işleme) tabanlı minimal bir 2D oyun motorudur. Kullanıcı arayüzü yönetimi için **ImGui** kütüphanesi entegre edilmiştir.

### Temel Döngü (Main Loop)
Motorun çalışma mantığı şu sırayla işler:
1.  **Giriş İşleme (Input Handling)**: SDL olayları (klavye, fare) yakalanır.
2.  **Güncelleme (Update)**: Varlıkların (Entity) fiziksel durumları ve parçacık sistemleri güncellenir.
3.  **Çizim (Rendering)**:
    *   OpenGL ile oyun sahnesi çizilir.
    *   ImGui ile editör arayüzü çizilir.
    *   `SDL_GL_SwapWindow` ile ekran güncellenir.

---

## 2. Grafik Motoru (Graphics Engine)
*   **API**: OpenGL 3.3 Core Profile.
*   **Shaderlar**:
    *   **Vertex Shader**: Varlıkların pozisyonunu (`transform`), kamera açısını (`view`) ve doku koordinatlarını işler.
    *   **Fragment Shader**: Doku rengini (`texture`) ve renk tonunu (`tint`) birleştirerek piksel rengini belirler.
*   **Kamera Sistemi**: 2D ortografik projeksiyon benzeri bir yapı kullanır. Yakınlaştırma (Zoom) ve Kaydırma (Pan) özellikleri mevcuttur.
*   **Doku Yönetimi (Texture Management)**:
    *   `stb_image` kütüphanesi ile `.png` ve `.jpg` formatlarını yükler.
    *   `RefreshTextures()` fonksiyonu `assets` klasörünü tarar ve dokuları belleğe yükler.
*   **Izgara Sistemi (Grid System)**:
    *   OpenGL `GL_LINES` kullanılarak çizilen sonsuz görünümlü bir zemin ızgarası.
    *   Merkezi (0,0) belirtmek için Kırmızı (X ekseni) ve Yeşil (Y ekseni) renkli çizgiler.
    *   Gri renkli yardımcı çizgiler ile nesne hizalamayı kolaylaştırır.

---

## 3. Varlık Sistemi (Entity System)
Motor, "Entity" yapısı üzerinden nesne tabanlı bir yaklaşım kullanır.
*   **Veri Yapısı**:
    *   `Transform`: x, y, rotation (dönüş), scale (boyut).
    *   `Properties`: name (isim), color (renk), active (aktiflik).
    *   `Physics`: hasGravity (yerçekimi var mı?), vy (dikey hız).
    *   `Rendering`: textureName (kullanılan doku ismi).

---

## 4. Kullanıcı Arayüzü (User Interface - UI)
Editör arayüzü **Dear ImGui** kullanılarak geliştirilmiştir.
*   **Tema**: Özelleştirilmiş modern karanlık tema ("Dark Theme") ve turuncu aksan renkleri.
*   **Font**: `Segoe UI` (Windows sistem fontu) entegrasyonu.
*   **Düzen (Layout)**:
    *   **Sol Panel (Hierarchy)**: Sahnedeki nesnelerin listesi, Kaydet/Yükle butonları.
    *   **Sağ Panel (Inspector)**: Seçili nesnenin özelliklerini (pozisyon, renk, doku, fizik) değiştirme.
    *   **Orta Alan (Scene View)**: Oyun dünyasının görüntülendiği alan. Dinamik `glViewport` ayarı ile pencere boyutuna göre otomatik ölçeklenir.

---

## 5. Fizik ve Etkileşim
*   **Yerçekimi**: Basit bir yerçekimi simülasyonu (`e.vy -= 0.001f`) ve zemin çarpışma kontrolü (`y < -0.8f`) bulunur.
*   **Parçacık Sistemi (Particle System)**: Basit ömürlü (life) parçacıklar. Seçili nesneden `SPACE` tuşu ile oluşturulabilir.
*   **Fare Seçimi (Picking)**:
    *   Ekran koordinatlarını dünya (World) koordinatlarına çeviren bir matematiksel dönüşüm kullanır.
    *   Pencere ve UI düzen değişikliklerine (Aspect Ratio) uyumlu çalışır.

---

## 6. Dosya Sistemi (File I/O)
*   **Sahne Kaydı**: `.wary` uzantılı metin dosyaları kullanır.
*   **Format**: Her satır bir varlığı temsil eder.
    *   Örnek: `Player 0.5 0.5 0.0 1.0 1.0 1.0 1.0 1 PlayerTex.png`

## 7. Derleme ve Bağımlılıklar (Build System)
*   **Build Sistemi**: CMake (v3.20+).
*   **Derleyici**: MinGW-w64 (GCC).
*   **Kütüphaneler**:
    *   SDL2 (Pencere, Giriş)
    *   GLAD (OpenGL Yükleyici)
    *   ImGui (Arayüz)
    *   stb_image (Resim Yükleme)
