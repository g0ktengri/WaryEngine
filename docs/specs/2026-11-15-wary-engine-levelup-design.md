# Wary Engine — Level Up Design

## Niyet ve Neden

Wary Engine, berke'nin portföyünde kullanacağı bir 2D oyun motoru. Şu an
çalışan ama mimari olarak yüzeysel bir prototip (~750 satır): flat
`Entity` struct, Renderer sınıfının içine gömülü editör UI'ı, draw-call
başına entity, basit gravity+AABB fiziği. README "ECS" ve "GLM"
kullanıldığını söylüyor ama kod bunların hiçbirini yapmıyor — bu bir
güvenilirlik riski.

Hedef: motoru hem teknik derinlik (gerçek bir mimari, "ben bunu
tasarladım" diyebilecek kararlar) hem de somut sonuç (oynanabilir, kayda
değer bir demo) gösterecek hale getirmek. Önceki çalışma kutsal değil —
berke mevcut kodun değişmesine/yeniden yazılmasına açıkça izin verdi.

## Hedefler

- README ve kod arasındaki tutarsızlıkları gider, bilinen bug'ları
  (Save butonu çalışmıyor) düzelt.
- Flat `Entity` struct'ı gerçek, okunabilir bir ECS'e (ID + component
  dizileri) dönüştür.
- Renderer'ı saf çizim sorumluluğuna indir; editör UI'ını (Hierarchy,
  Inspector, Tips) ayrı bir modüle çıkar.
- Entity-başına-draw-call yerine texture bazlı batching getir.
- Motoru kullanarak küçük, bitmiş, oynanabilir bir demo sahnesi inşa et
  (mevcut gravity/collision/particle sistemlerini gerçek bir oyun
  döngüsünde kullanan).
- Projeyi git'e al, aşamalı commit geçmişiyle "gelişim hikayesi" oluştur;
  README'yi mimariyi doğru yansıtacak şekilde yeniden yaz, bir demo
  GIF/video ile destekle.

## Non-Goals

- Çoklu platform desteği (Windows/MinGW hedefi aynen kalıyor).
- 3D render, networking/multiplayer.
- Genel amaçlı, tam fizik motoru (sürekli çarpışma tespiti, eklemler,
  vb.) — mevcut AABB + gravity yaklaşımı yeterli, sadece demo'nun
  ihtiyacı kadar genişletilecek.
- Şablon-ağır, "framework" tadında generic bir ECS kütüphanesi.

## Anti-Goals

- Motoru yarım bırakan, derlenemeyen bir ara durumda kalmak.
- Okunabilirliği feda eden bir ECS (bir incelemeci kodu açtığında
  5 dakikada "bu nasıl çalışıyor" diyebilmeli).
- Teknik olarak çalışan ama oynanması/izlenmesi sıkıcı, yarım kalmış
  hissi veren bir demo.
- Polish'e bu kadar zaman harcayıp demo'yu asla kayıt/paylaşıma
  hazır hale getirmemek.

## Kısıtlar

- Mevcut CMake + MinGW + vendored SDL2/glad/imgui/stb_image kurulumu
  üzerinde kalınacak; yeni ağır bağımlılık eklenmeyecek.
- C++20 standardı korunacak.
- Proje zaten bir git deposu ve `https://github.com/g0ktengri/WaryEngine`
  adında **public** bir GitHub remote'u var (3 commit geçmişi mevcut).
  Yani "git init" gerekmiyor — her aşama bu mevcut repoya commit+push
  edilecek, gelişim geçmişi doğrudan bu public repoda görünür olacak.

## Değerlendirilen Yaklaşımlar

1. **Mimari-önce**: Sadece ECS/batching/render-UI ayrımı. Teknik
   incelemeciye güçlü sinyal verir ama görünür/demo edilebilir bir
   sonuç çıkarmaz.
2. **Demo-önce**: Sadece bug fix + oynanabilir sahne. Hızlı görsel etki
   ama mimari derinlik sinyali zayıf kalır.
3. **Aşamalı (seçildi)**: Önce güvenilirlik düzeltmeleri, sonra mimari
   yükseltme, sonra o mimari üzerine inşa edilmiş bir demo, son olarak
   sunum/paylaşım. Her aşama kendi başına bir git milestone'u olur; iş
   büyürse aşama sınırında durup haber verilir.

berke "bütün sebepler" dedi (hem teknik derinlik hem gösterilebilir
sonuç istiyor) ve önceki kodun değişmesine izin verdi — bu, 3. yaklaşımı
doğruluyor.

## Kararlaştırılan Yapı (mühendislik kararları)

Bunlar benim çağrılarım; berke onaylarsa plana geçilecek:

- **ECS tasarımı**: Generic/templated bir ECS kütüphanesi yazılmayacak.
  Her entity bir `EntityID` (int) olacak; component'lar paralel
  `std::vector`lerde ID ile indexlenecek (sparse array yaklaşımı):
  `TransformComponent` (x, y, rotation, sx, sy), `RenderComponent`
  (color, textureName), `PhysicsComponent` (vy, hasGravity, isStatic),
  `NameComponent` (düzenleme/debug için). Silinen entity'ler bir
  free-list üzerinden ID'lerini geri verir. Bu, "ben data-oriented
  tasarımı anlıyorum" sinyalini basit ve okunabilir biçimde verir.
- **Renderer/UI ayrımı**: `Renderer` sadece OpenGL çağrılarını ve
  batching'i yapacak; `EditorUI` (yeni sınıf) Hierarchy/Inspector/Tips
  panellerini ve seçim/undo tetikleme mantığını üstlenecek. İkisi
  Engine üzerinden bağlanacak.
- **Batching**: Instancing değil, texture bazlı basit batching —
  her frame'de texture'a göre gruplanmış dinamik bir vertex buffer
  doldurulup texture-başına tek draw call yapılacak. Mevcut
  entity-başına-draw-call'dan gerçek bir iyileştirme, ama instancing
  kadar karmaşık değil.
- **GLM**: Şimdilik entegre edilmeyecek — Faz 1'de README'den GLM
  iddiası silinecek, elle yazılmış matris matematiği korunacak. GLM
  entegrasyonu rafa kaldırıldı, ileride ayrı bir iş olarak ele
  alınabilir (bu planın kapsamı dışında).
- **Scene formatı**: ECS geçişiyle birlikte mevcut whitespace-ayrılmış
  `scene.wary` formatı (isimde boşluk olursa bozulan) yeniden
  tasarlanacak — satır başına component verisi, boşluk güvenli.
- **Demo sahnesi**: Mevcut gravity/AABB/particle sistemlerini kullanan
  küçük bir platform sahnesi (zıplama, toplanabilir objeler, particle
  efektleri, basit kamera takibi). Tema/detaylar builder'a bırakılıyor.
- **Git**: Repo ve `.gitignore` zaten mevcut. Her aşama bitince ilgili
  değişiklikler commit edilip `origin/main`'e push edilecek — berke
  "sergiliyoruz, görünsün" dedi, yani push etmek varsayılan davranış,
  her aşamada ayrıca sorulmayacak.

## Aşamalar

1. **Güvenilirlik düzeltmeleri**: Save butonu bug'ı; README'deki
   ECS/GLM gibi doğru olmayan iddiaları mevcut koda uydur (bu aşamadan
   sonra zaten değişecek, o yüzden bu README güncellemesi aşama 4'te
   tekrar yapılacak — burada sadece "yalan söylemeyen" bir ara durum);
   commit + push.
2. **Mimari yükseltme**: ECS geçişi, Renderer/UI ayrımı, texture
   batching, scene formatı yenileme.
3. **Demo sahnesi**: Yukarıdaki mimari üzerine oynanabilir, bitmiş bir
   mini sahne.
4. **Sunum**: README'yi gerçek mimariyi anlatacak şekilde (ve bir
   mimari diyagramıyla) yeniden yaz; demo GIF/video kaydı; portföy
   sayfasına/GitHub'a bağlama.

Her aşama ayrı bir onay/inceleme noktası olacak — bir aşama bitince
sonraki aşamaya geçmeden önce ne yapıldığı gösterilecek.

## Builder'a Bırakılanlar

- Component struct'larının kesin alan isimleri/düzeni.
- Batching'in dinamik buffer yönetiminin ayrıntıları (buffer boyutu,
  ne zaman flush edilir).
- Demo sahnesinin tam teması/içeriği (hangi görseller, kaç platform,
  vb.) — gravity/collision/particle sistemlerini kullandığı ve
  "bitmiş" hissettiği sürece serbest.
- Commit granülaritesi (aşama başına tek commit mi, birkaç mantıklı
  commit mi).
