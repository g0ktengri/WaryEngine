# Wary Engine — Faz 2a: Renderer / Editor UI Ayrımı

## Niyet ve Neden

Ana spec'in Faz 2'si ("Mimari yükseltme") üç parçadan oluşuyor: Renderer/UI
ayrımı, texture batching, ECS geçişi. berke bunların küçükten büyüğe,
sırayla yapılmasını onayladı — en düşük riskli, en izole değişiklik
(davranış değiştirmeyen saf bir refactor) önce. Bu doküman sadece o ilk
parçayı (2a) kapsıyor.

Şu an `Renderer::Render()` (`src/Renderer.cpp:248-430`) tek bir fonksiyonda
hem OpenGL çizimini (grid, entity, particle) hem de tüm editör UI'ını
(Hierarchy, Inspector, Tips panelleri), mouse picking'i ve undo-capture
mantığını yapıyor. `Renderer::Init()` de hem GL kurulumunu hem ImGui
kurulumunu (context, style, font, backend init) yapıyor. Bu, "Renderer
sadece çizim yapar" iddiasını kodun kendisi yalanlıyor demek — tam da ana
spec'in "güvenilirlik" hedefinin bir sonraki katmanı.

## Hedefler

- Yeni bir `EditorUI` sınıfı (`include/EditorUI.h` / `src/EditorUI.cpp`)
  oluştur: ImGui context/style/font kurulumu, Hierarchy/Inspector/Tips
  panelleri, mouse picking, undo-capture tetikleme mantığı buraya taşınır.
- `Renderer`'ı sadece OpenGL state + çizime indir: shader/buffer/grid/
  texture kurulumu ve sahne çizimi (clear, grid, entity, particle draw).
- `Engine`, ikisini de üye olarak tutup frame'i orkestre eder (ana spec'in
  "İkisi Engine üzerinden bağlanacak" kararı).
- **Davranışta hiçbir değişiklik olmayacak** — bu saf bir refactor;
  Faz 1'de düzelttiğimiz her şey (Save butonu, render fix, serialization)
  birebir aynı şekilde çalışmaya devam etmeli.

## Non-Goals

- ECS geçişi yok (Faz 2c'de).
- Batching yok (Faz 2b'de).
- Yeni özellik yok, davranış değişikliği yok.

## Anti-Goals

- Bu refactor'ın bir yan etki olarak herhangi bir mevcut davranışı
  (picking, undo, kamera pan/zoom, WASD, particle spawn, Save) bozması.
- Gereksiz soyutlama (örn. generic bir "Panel" interface'i, plugin sistemi
  vb.) — sadece iki sınıfa net bir sorumluluk ayrımı yeterli.

## Kısıtlar

- Mevcut CMake yapısı (yeni .cpp/.h dosyaları `file(GLOB_RECURSE ...)` ile
  otomatik toplanıyor, CMakeLists.txt değişikliği gerekmez).
- Faz 1'de eklenen `bool& saveRequested`, `std::vector<std::vector<Entity>>&
  undoStack` gibi parametreler korunacak (isimleri/anlamları değişmeyecek).

## Kararlaştırılan Yapı

Ana spec'in kararı zaten bağlayıcı; burada sadece onu somutlaştırıyorum:

- **Pencere/GL context sahipliği**: `Engine`, `SDL_GL_CreateContext`'i
  kendisi çağırıp hem `Renderer::Init(window, ctx)` hem
  `EditorUI::Init(window, ctx)`'e geçirir (ikisi de aynı context'i
  kullanır). Şu an context oluşturma `Renderer::Init` içinde — Engine'e
  taşınacak.
- **`EditorUI::Init(SDL_Window*, SDL_GLContext)`**: ImGui::CreateContext,
  font yükleme, `SetupUIStyle()`, `ImGui_ImplSDL2_InitForOpenGL`,
  `ImGui_ImplOpenGL3_Init` — hepsi `Renderer::Init`'ten buraya taşınır.
- **`EditorUI::Draw(entities, particles, cam, selected&, undoStack&,
  saveRequested&, textureList) -> ViewportRect`**: `ImGui_ImplOpenGL3_NewFrame`
  /`ImGui_ImplSDL2_NewFrame`/`ImGui::NewFrame` ile başlar, undo-capture
  bloğunu, Hierarchy/Inspector/Tips panellerini ve mouse picking'i içerir
  (`Renderer.cpp`'nin 248-377 ve 279-299 arası kodu). Dönüş değeri, sahne
  viewport'unun (x, genişlik, yükseklik) — şu an `sceneX`/`sceneW`/`sceneH`
  olarak hesaplanan değerler. `ViewportRect` basit bir struct
  (`include/EditorUI.h` içinde: `float x, width, height;`).
- **`Renderer::Render(SDL_Window*, entities, particles, cam, const
  ViewportRect&)`**: Sadece "--- Scene Render ---" bloğu (şu an
  `Renderer.cpp:381-425`) — clear, grid çizimi, entity çizimi, particle
  çizimi. İmza değişir: `selected`, `undoStack`, `saveRequested` parametreleri
  kalkar (artık EditorUI'ye ait).
- **Frame sonu**: `ImGui::Render()` + `ImGui_ImplOpenGL3_RenderDrawData` +
  `SDL_GL_SwapWindow` — `Engine::Run()`'a taşınır (Engine artık frame
  yaşam döngüsünü tam olarak orkestre ediyor).
- **`Renderer::~Renderer()`**: Şu an ImGui shutdown çağırıyor
  (`ImGui_ImplOpenGL3_Shutdown`, `ImGui_ImplSDL2_Shutdown`,
  `ImGui::DestroyContext`) — bunlar `EditorUI::~EditorUI()`'ye taşınır.

## Builder'a Bırakılanlar

- `ViewportRect` struct'ının tam alan isimleri (x/width/height yeterli,
  ama builder farklı isimlendirebilir, anlamı değişmez).
- `EditorUI` içindeki yardımcı private metodların (ör. picking mantığı
  için) tam bölünmesi.
- Undo-capture statik değişkenlerinin (`wasActive`, `preEditState`) yeni
  sınıfa nasıl taşınacağı (üye değişken olacakları açık, implementasyon
  detayı builder'a ait).
