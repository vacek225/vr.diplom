# agents.md

## 1) Суть приложения и логика

### 1.1 Продуктовая идея
`KUB_KiberUniversitetBudushego` - это VR/desktop-аудитория с подключением по LAN, где преподаватель создает комнату, выбирает аудиторию, запускает занятие и управляет медиа/участниками, а студенты заходят по короткому room-коду.

### 1.2 Роли
- `Host (преподаватель)`: создает аудиторию, получает room-код, управляет контентом (экран/браузер/3D-объекты), модерирует участников (mute/kick/ban).
- `Student (ученик)`: входит по room-коду, слушает VOIP, видит общий экран, может локально регулировать громкость других участников.

### 1.3 Основной пользовательский сценарий
1. Пользователь запускает приложение и видит world-space хаб с двумя действиями: `Create Auditorium` и `Join Auditorium`.
2. Преподаватель выбирает `Create Auditorium`.
3. Открывается список аудиторий (сейчас одна, в дальнейшем расширяемый список пресетов).
4. После выбора аудитории создается LAN-сессия, генерируется room-код, хост телепортируется на карту аудитории.
5. В аудитории показывается плашка с кодом в стиле liquid-glass.
6. Ученики выбирают `Join Auditorium`, вводят код и подключаются к хосту.
7. Во время занятия:
- работает VOIP;
- у хоста есть модераторские инструменты;
- у преподавателя есть маленький браузер-экран на столе;
- большой экран дублирует URL преподавателя;
- преподаватель может спавнить/удалять предзагруженные 3D-объекты.

### 1.4 Логика состояний хаба
- `Root`: Create / Join.
- `AuditoriumSelection`: выбор карты аудитории.
- `JoinCodeEntry`: ввод room-кода.
- `RoomHosted`: сессия поднята, код сгенерирован.
- `InRoom`: пользователь уже в аудитории.

### 1.5 Требования к UX/UI
- Стартовые кнопки должны быть **объектами в мире** (world-space), а не overlay на экране.
- Визуальный стиль: полупрозрачные закругленные liquid-glass таблетки.
- Хаб должен исчезать после входа в аудиторию, чтобы не мешать занятию.

---

## 2) Техническая работа плагинов

### 2.1 NodeToCode (порт/форк UE 5.7)
Назначение плагина в проекте:
- перевод графов Blueprint в код;
- режим cloud-перевода через LLM-провайдеры;
- режим `Local Codex handoff` без API-ключей (сохранение handoff-файлов локально для дальнейшей реализации в C++).

Практическая роль для этого проекта:
- быстро снимать текущую Blueprint-логику (UI, flow, сессии);
- переносить/фиксировать ее в C++ классах плагина `KemSUinVR`;
- держать гибридный цикл: прототип в BP -> перенос в код -> проверка в Editor.

### 2.2 KemSUinVR (основная игровая логика)

#### Сетевой слой и комнаты
- `UKemRoomSubsystem`:
  - хранит текущее состояние комнаты (`FKemRoomInfo`);
  - создает LAN-сессии, генерирует room-код;
  - ищет сессии по коду и выполняет join;
  - обрабатывает travel (host listen / client travel);
  - дергает `ToggleSpeaking 1` для VOIP bootstrap.

- `AKemClassroomStateActor`:
  - реплицирует room-код, URL лекции, список участников, бан-лист;
  - дает host-методы: `SetParticipantVolume`, `SetParticipantMuted`, `KickParticipant`, `BanParticipant`;
  - управляет спавном презентационных примитивов/объектов.

- `AKemVoiceControlProxy`:
  - owner-only actor для удаленного переключения voice transmit (`ToggleSpeaking 0/1`).

- `AKemPlayerControlProxy`:
  - переключает локального игрока в desktop fallback при отсутствии активного HMD.

#### World-space UI и акторы
- `AKemHubActor` - world-space хаб входа.
- `AKemRoomCodeDisplayActor` - плашка room-кода (видна хосту).
- `AKemTeacherControlPanelActor` - панель преподавателя.
- `AKemBrowserDeskActor` - настольный браузер преподавателя.
- `AKemSharedLectureScreenActor` - большой общий экран.
- `AKemParticipantModeratorActor` + `AKemHostModeratorManagerActor` - панели модерации участников.
- `AKemClassroomBootstrapActor` - собирает layout вокруг локального pawn и поддерживает режимы управления.

#### UI-виджеты
- `UKemHubMenuWidget` - flow `Create/Join` и состояние хаба.
- `UKemRoomCodeWidget` - отображение кода комнаты.
- `UKemTeacherControlWidget` - спавн/очистка объектов + смена URL лекции.
- `UKemBrowserDeskWidget` - маленький браузер преподавателя.
- `UKemLectureScreenWidget` - общий/локальный экран лекции.
- `UKemParticipantModeratorWidget` - mute/kick/ban + громкость.

### 2.3 Что изменено сейчас под ваш запрос про 3D кнопки
В текущем коде были две разные презентации хаба:
- desktop overlay через `AddToViewport`;
- world-space панель через `UWidgetComponent`.

Из-за этого кнопки на desktop выглядели как наложение поверх экрана.

Сделано:
1. Убран viewport overlay-хаб в `AKemDesktopCharacter` (больше не создается `AddToViewport` меню).
2. Оставлен единый world-space путь для стартового хаба.
3. `AKemHubActor` теперь скрывается после входа в комнату (`bIsInRoom`), чтобы старые кнопки не оставались в аудитории.
4. Для кнопок и корневой панели хаба усилен glass-стиль:
- более прозрачная холодная палитра;
- увеличенные кнопки-таблетки;
- мягкие тени/контраст текста.

### 2.4 Следующий шаг (техдолг/roadmap)
- Перевести кнопки из world UMG-квадов в полноценные 3D-меш-кнопки (Static Mesh + collision + interaction interface) для реального объемного вида.
- Вынести тему liquid-glass в отдельный material/UMG style asset (чтобы переиспользовать для room code, teacher panel, browser desk).
- Пресет `3_Block` уже зарегистрирован; карты `Lobby` и `3_Block` включены в упаковку. Есть точки появления по тегам и Editor-скрипты настройки.
- Добавлен учебный двигатель ВАЗ-2112; следующие модели и точные CAD-ассеты можно подключать отдельно.

### 2.5 Учебный двигатель ВАЗ-2112
- `AKemEngineAssembly`: серверный стенд, определения узлов, зависимости, свободный режим и уроки разборки/сборки.
- `AKemEnginePart`: реплицируемый узел с учебной геометрией из базовых мешей; блокировка захвата, положение, ориентация и фиксация.
- `AKemPlayerControlProxy`: owner-only RPC захвата, перемещения, отпускания и режима. Не отправлять RPC с общего стенда: клиент им не владеет.
- `UKemVRGrabInteractionComponent`: VR-контроллеры; `AKemDesktopCharacter`: F / R / колесо мыши для desktop.
- `UKemEngineTrainingWidget`: старый виджет; автоматическая панель у двигателя удалена. Управление и подсказки упражнения находятся в `UKemEscapeMenuWidget`.
- Материалы: `Content/KUB/Engine2112`, скрипт `Scripts/Editor/SetupEngine2112Materials.py`. Процедурный PBR-граф использует локальную позицию с учётом масштаба; профили металлических/неметаллических поверхностей назначаются в `AKemEnginePart::RebuildVisuals`. Стенд — `MI_EngineStand`. Сохранять параметр Tint и отдельный M_EngineTarget для учебной подсветки. Скрипт обновляет узлы без удаления rooted expressions UE 5.7. Превью и крупный план — `PreviewEngine2112.py`, он также обновляет T_engine в каталоге.
- Геометрия и порядок операций упрощены для обучения; это не точный CAD и не полный регламент ремонта.
- Прозрачность при сборке: `AKemEnginePart::UpdateAssemblyTransparency`, для локально удерживаемой детали того же стенда — установленные соседи, Free/Assemble. Неудачно отпущенная внутри конструкции деталь сохраняет PlacementPawn в атомарном реплицируемом State; все установленные детали остаются прозрачными только у этого участника до исправления/извлечения/сброса. Повторный захват тем же участником сохраняет эффект внутри; передача другому очищает его. Не реплицировать эффект, не менять collision и Ghosts; возвращать PBR-материал и тени. M_EngineAssemblyXRay создаёт SetupEngine2112Materials.py.
- Фиксация: SnapDistance=16 см, SnapAngleDegrees=55°, CanSnapAt — общая проверка для Release и зелёного призрака. Сохранять серверные зависимости/очередность, исправлять финальную позу в точное гнездо. В Free/Assemble не телепортировать неудачную установку на лоток.
- Автотесты: `KemSUinVR.Engine2112`. LAN/HMD требуют отдельной ручной приёмки.


### 2.6 Escape menu
- `UKemEscapeMenuSubsystem`: автоматически создаётся на LocalPlayer, Esc перехватывается Slate до End PIE; VIVE Menu Click — постоянный `/Game/KUB/Input/IMC_KemVRUI`, priority 1000, в DefaultMappingContexts до создания OpenXR session. Не заменять поздним transient context. Ассеты создаёт `SetupVRMenuInput.py`, каталог включён в cooking.
- `UKemEscapeMenuWidget`: нативный UMG с разделами room/tools/objects/screen/people/audio. ПК — AddToPlayerScreen, VR — локальный world WidgetComponent. Это отдельное меню во время игры; стартовый Hub остаётся world-space.
- Grip не переключает инструменты или UI-луч. Курок выбирает UI автоматически при наведении. System не назначать.
- VR UI: обе руки, отдельные virtual pointers (virtual user 1, pointer 1/2), custom hit обновляется перед нажатием. Menu обеих рук переключает панель по фронту нажатия независимо от размещения InputComponent на pawn/controller. Открытие перед активной HMD CameraComponent; внешняя рамка 24px — захват курком и перенос/поворот с сохранением начального смещения. Один drag owner; отпускание другой руки его не завершает. Закрытие/потеря tracking отменяют взаимодействие, инструменты ждут отпускания обоих курков. Автотест `KemSUinVR.EscapeMenu.VRInputAndPanelDrag`, Slate-клики проверяются с RHI; HMD отдельно.
- Не ставить сетевую игру на паузу; после закрытия ждать отпускания trigger/ЛКМ. Балансировать только собственный SetIgnoreMoveInput, не сбрасывать чужие блокировки.
- Действия преподавателя вызываются на listen host; новые действия студента с сетью — через его `AKemPlayerControlProxy`, не через общий unowned actor.
- Рисование: серверная Fast Array на доске (2048 сегментов); локальное меню задаёт цвет/размер. Указатель — отдельный always-relevant `AKemPointerActor`, owner-only proxy не делать общедоступным.
- VOIP mute преподавателя имеет приоритет над сохранённым локальным микрофоном.
- Экран сейчас синхронизирует URL; загрузка локальных файлов и синхронное воспроизведение остаются отдельной задачей. Не выдавать передачу URL за потоковую трансляцию экрана.
- Автотесты меню: `KemSUinVR.EscapeMenu`; весь набор: `KemSUinVR`.

### 2.7 Каталог объектов и переключатель луча
- При закрытии сохранять выбранную вкладку в LocalPlayer subsystem и восстанавливать при открытии. Подтверждения, выбор объекта и прокрутку сбрасывать; настройки звука/инструментов сохранять. ПК — боковая навигация, VR — нижняя, с подсказками VIVE.
- Указательный луч — отдельный bool `bPointerEnabled`, не взаимоисключающий инструмент; UI-луч не зависит от этого переключателя.
- Каталог показывает только название и превью. После выбора показывать действия для выбранного экземпляра в том же меню. Не добавлять LessonPanel обратно на двигатель.
- Standalone Lobby: `UKemEscapeMenuSubsystem` создаёт и отслеживает локальные объекты, обеспечивает grab proxy; LAN-комната не нужна. В аудитории сохраняются права преподавателя и владение объектами через ClassroomState.
- Перемещение стенда реплицируется; его детали сбрасываются к новым местам установки, удерживаемые детали блокируют действие.
- Текстуры превью: Content/KUB/UI/ObjectPreviews, всегда включать в cooking.

### 2.8 Lobby: материалы и профиль рендеринга
- Меш `/Game/KUB/lobby/2` использует `WALL2_MAT`; override `lobby/objects/PBR` без текстур делает стену чёрной. Восстановление: `FixLobbySurfaces.py`, с резервными копиями пакетов в `Saved/Backups/LobbySurfaceRepair`.
- ORM: R=AO, G=Roughness, B=Metallic, sRGB выключен, Masks compression и соответствующий sampler. Исправлены каналы `Roof_mat`.
- `OptimizeLobbyTextures.py`: исходники/цвет 4K сохраняются, normal/ORM пяти материалов ограничены до 2K для стриминга/cooking. Не путать размер исходного uasset с расходом GPU-памяти.
- Forward + Instanced Stereo, Windows DX12. Lumen GI/reflections, VSM и аппаратный ray tracing отключены; baked lighting/reflection captures/distance fields сохранены. Не включать дорогие функции без проверки совместимости и замеров на целевом HMD.
- World UI панели имеют Movable root (включая восстановление старых сериализованных экземпляров в BeginPlay); скрытые не поворачивать, одинаковые повороты повторно не назначать.
- Аудит: `InspectLobbySurfaces.py`; отдельный несохраняющий карту рендер: `PreviewLobby.py`. Исправляющие скрипты не запускать одновременно с редактированием тех же пакетов. После изменения RendererSettings перезапустить Editor; старый packaged build требует переупаковки.

### 2.9 Кисть и захват активным лучом
- Кисть читает непрерывный `IA_KemUIRight` через общий GetTriggerStrength, не полагаться только на raw keys/старый Pressed action. Начало/направление луча — RightAim. Подавлять рисование только UI правой руки/занятой правой рукой, а не наведением левой руки на UI. TraceDrawingSurface даёт пересечение plane без зависимости от complex collision, TraceBoardRay учитывает ближайшее препятствие. Не возвращать старый fallback ActiveVRBoard сквозь препятствие.
- VR-панель: 90 см перед HMD, прежний масштаб 0.085 (85 × 108.8 см). Обе кнопки выхода закреплены сверху вне PageScroll на ПК/VR и всех вкладках; в Lobby выход в Lobby виден, но disabled. Подтверждения сохранены.
- Указатель включён + новый фронт курка: TryRayGrab (обе руки), плавный подлёт 500 см/с, bRayLatched до нажатия Grip. Grip Press передаёт притянутый предмет в обычный захват; Grip Release освобождает. bManualHold продолжает движение props после передачи, включая незавершённый подлёт. Захват рядом тоже управляется фронтами непрерывного Grip (не старые Pressed/Released bindings на PC InputComponent). Потеря tracking освобождает ray/manual hold. UI имеет приоритет перед курком, Grip доступен постоянно. Не обходить серверные зависимости/владение деталей двигателя.
- Детали: owner-only ServerRayGrabEnginePart -> TryAcquireFromRay (проверки позы/расстояния, ray LOS до 400 см) -> общие AcquireValidated зависимости. Обычный TryAcquire по-прежнему требует 30 см. Move/Release идут прежним путём, состояние реплицируется; UI/latched/pull — локальны.
- Props Lobby: whitelist точных StaticMesh asset paths Green_Apple_FBX/AppleLowPoly, SM_Pistol, skull; не разрешать стены/пол по общему признаку StaticMesh. BeginPlay включает query collision только этих props. Подлёт без collision, Release восстанавливает collision/physics. В NM_Client локальное притягивание props запрещено; сетевая реализация сейчас только у деталей двигателя.
- Постоянные не consuming IA_KemGripLeft/Right создаёт SetupVRMenuInput.py в отдельном IMC_KemVRGrip (DefaultMappingContexts priority 0, как XRFramework Hands; subsystem восстанавливает контекст после сброса mappings). Не класть Grip в priority-1000 меню: OpenXR action-set priority подавляет lower-priority bindings независимо от ConsumeInput. Проверки: KemSUinVR.EscapeMenu.BrushAndRayGrab (непрерывный enhanced ввод без raw axis, штрих через proxy, препятствие, реальные props, плавность/передача Grip/обычный ближний захват обеими руками/отпускание, двигатель). Физический HMD/LAN — отдельно.
